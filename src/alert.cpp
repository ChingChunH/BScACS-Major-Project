#include "alert.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <iostream>

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/Aws.h>
#include <aws/sns/model/PublishRequest.h>
#include <aws/sesv2/model/SendEmailRequest.h>
#include <aws/sesv2/model/Content.h>
#include <aws/sesv2/model/Body.h>
#include <aws/sesv2/model/Message.h>

#include "Database.h"
#include "settings.h"

/**
 * @brief Construct an Alert instance.
 *
 * Initializes AWS SNS and SESv2 clients using credentials loaded from JSON,
 * then reads any existing user settings from the database to populate
 * the Settings object (email/phone).
 */
Alert::Alert(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    Aws::Auth::AWSCredentials credentials;
    Aws::Client::ClientConfiguration config;

    // Determine config file path based on platform and load credentials.
    QString credentialsPath = resolveAwsConfigPath();
    if (!loadAwsCredentials(credentialsPath, credentials, config)) {
        qWarning("Failed to initialize AWS clients with credentials from JSON");
        return;
    }

    // Instantiate AWS SNS (SMS) and SESv2 (email) clients.
    m_snsClient   = std::make_unique<Aws::SNS::SNSClient>(credentials, config);
    m_sesv2Client = std::make_unique<Aws::SESV2::SESV2Client>(credentials, config);

    // Create and query the Database for any saved user contact settings.
    m_database = new Database(this);
    QVariantList userSettings = m_database->getAllUserSettings();
    if (!userSettings.isEmpty()) {
        QStringList emails, phones;
        for (const QVariant &v : userSettings) {
            QVariantMap user = v.toMap();
            QString e = user["email"].toString();
            QString p = user["phone"].toString();
            if (!e.isEmpty()) emails << e;
            if (!p.isEmpty()) phones << p;
        }
        // Update the Settings object with comma-separated contacts.
        if (!emails.isEmpty())
            m_settings->setEmail(emails.join(", "));
        if (!phones.isEmpty())
            m_settings->setPhoneNumber(phones.join(", "));
    }

    qDebug() << "[ALERT INIT] Email:" << m_settings->getEmail()
             << "Phone number:" << m_settings->getPhoneNumber();
}

/**
 * @brief Resolve the path to the AWS credentials JSON file.
 *
 * Uses a relative path under the application bundle/resources
 * folder, with a different prefix on macOS vs. other platforms.
 */
QString Alert::resolveAwsConfigPath()
{
#ifdef Q_OS_MAC
    return QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        "/../../../../../resources/awsconfig.json"
        );
#else
    return QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        "/../../resources/awsconfig.json"
        );
#endif
}

/**
 * @brief Send a combined SMS and/or email alert to all configured contacts.
 * @param message The alert text to deliver.
 * @return True if any alert was successfully sent, false otherwise.
 *
 * Enforces a per-hour rate limit based on Settings.notificationFrequency.
 * Retrieves up-to-date contact info from the database each call.
 */
bool Alert::sendAlert(const QString &message)
{
    // Ensure AWS clients were initialized.
    if (!m_snsClient && !m_sesv2Client) {
        qDebug() << "[ALERT] AWS clients not initialized. Skipping alert.";
        return false;
    }

    qDebug() << "[ALERT] sendAlert called with message:" << message;

    // Reload user settings (email/phone) from the database.
    if (!m_database) {
        qWarning() << "[ALERT] Database not initialized.";
        return false;
    }
    QVariantList userSettings = m_database->getAllUserSettings();
    if (userSettings.isEmpty()) {
        qWarning() << "[ALERT] No user settings found. Skipping alerts.";
        return false;
    }

    // Ensure at least one contact (email or phone) exists.
    bool validContactFound = false;
    for (const QVariant &v : userSettings) {
        QVariantMap user = v.toMap();
        if (!user["email"].toString().isEmpty() ||
            !user["phone"].toString().isEmpty()) {
            validContactFound = true;
            break;
        }
    }
    if (!validContactFound) {
        qWarning() << "[ALERT] No valid email or phone. Skipping alerts.";
        return false;
    }

    // Parse rate-limit frequency (alerts per hour) from settings.
    int freq = m_settings->getNotificationFrequency().toInt();
    if (freq <= 0) freq = 10;

    // Prune timestamps older than one hour to maintain sliding window.
    QDateTime oneHourAgo = QDateTime::currentDateTime().addSecs(-3600);
    m_alertTimestamps.erase(
        std::remove_if(
            m_alertTimestamps.begin(),
            m_alertTimestamps.end(),
            [oneHourAgo](const QDateTime &ts) { return ts < oneHourAgo; }
            ),
        m_alertTimestamps.end()
        );

    qDebug() << "[ALERT] Alerts sent in last hour:" << m_alertTimestamps.size()
             << " / allowed:" << freq;

    // If under limit, send to each contact.
    if (static_cast<int>(m_alertTimestamps.size()) < freq) {
        bool anySent = false;
        for (const QVariant &v : userSettings) {
            QVariantMap user = v.toMap();
            QString email = user["email"].toString();
            QString phone = user["phone"].toString();

            // Send email if configured.
            if (!email.isEmpty()) {
                qDebug() << "[ALERT] Sending email to:" << email;
                if (sendEmailAlert(email, message)) anySent = true;
                else qDebug() << "[ALERT] Email failed for:" << email;
            }

            // Send SMS if configured.
            if (!phone.isEmpty()) {
                qDebug() << "[ALERT] Sending SMS to:" << phone;
                if (sendSmsAlert(phone, message)) anySent = true;
                else qDebug() << "[ALERT] SMS failed for:" << phone;
            }
        }

        if (anySent) {
            // Record timestamp only if at least one delivery succeeded.
            m_alertTimestamps.push_back(QDateTime::currentDateTime());
            return true;
        }

        qDebug() << "[ALERT] No alerts sent (delivery failures).";
        return false;
    }

    qDebug() << "[ALERT] Rate limit reached; skipping alert.";
    return false;
}

/**
 * @brief Send an SMS via AWS SNS.
 * @param phoneNumber E.164-formatted phone number (e.g. "+15551234567").
 * @param message      Text message body.
 * @return True on success, false on failure.
 */
bool Alert::sendSmsAlert(const QString &phoneNumber, const QString &message)
{
    if (!m_snsClient) {
        qWarning() << "[SMS ALERT] SNS client not initialized; skipping.";
        return false;
    }

    Aws::SNS::Model::PublishRequest req;
    req.SetMessage(message.toStdString());
    req.SetPhoneNumber(phoneNumber.toStdString());

    auto outcome = m_snsClient->Publish(req);
    if (!outcome.IsSuccess()) {
        std::cerr << "[SMS ALERT] Failed to send SMS to "
                  << phoneNumber.toStdString()
                  << ". Error: "
                  << outcome.GetError().GetMessage()
                  << std::endl;
        return false;
    }

    std::cout << "[SMS ALERT] SMS sent to "
              << phoneNumber.toStdString() << std::endl;
    return true;
}

/**
 * @brief Send an email via AWS SESv2.
 * @param email   Recipient email address.
 * @param message Body text of the email.
 * @return True on success, false on failure.
 */
bool Alert::sendEmailAlert(const QString &email, const QString &message)
{
    if (!m_sesv2Client) {
        qWarning() << "[EMAIL ALERT] SES client not initialized; skipping.";
        return false;
    }

    // Build simple email with subject and text body.
    Aws::SESV2::Model::SendEmailRequest req;
    Aws::SESV2::Model::Content subject;
    subject.SetData("Critical Alert");
    Aws::SESV2::Model::Content textPart;
    textPart.SetData(message.toStdString());
    Aws::SESV2::Model::Body body;
    body.SetText(textPart);
    Aws::SESV2::Model::Message awsMessage;
    awsMessage.SetSubject(subject);
    awsMessage.SetBody(body);
    Aws::SESV2::Model::EmailContent content;
    content.SetSimple(awsMessage);
    req.SetContent(content);

    // Set sender and recipient.
    req.SetFromEmailAddress("ingridh2630@gmail.com");
    Aws::SESV2::Model::Destination dest;
    dest.AddToAddresses(email.toStdString());
    req.SetDestination(dest);

    auto outcome = m_sesv2Client->SendEmail(req);
    if (!outcome.IsSuccess()) {
        std::cerr << "[EMAIL ALERT] Failed to send email to "
                  << email.toStdString()
                  << ". Error: "
                  << outcome.GetError().GetMessage()
                  << std::endl;
        return false;
    }

    std::cout << "[EMAIL ALERT] Email sent to "
              << email.toStdString() << std::endl;
    return true;
}

/**
 * @brief Load AWS credentials and region from a JSON config file.
 * @param filePath Path to JSON file containing keys: accessKeyId, secretAccessKey, region.
 * @param creds    Output AWSCredentials object.
 * @param cfg      Output ClientConfiguration (region is set).
 * @return True on successful parse and assignment, false otherwise.
 */
bool loadAwsCredentials(const QString &filePath,
                        Aws::Auth::AWSCredentials &creds,
                        Aws::Client::ClientConfiguration &cfg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Failed to open AWS config file");
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject obj = doc.object();
    QString accessKeyId     = obj["accessKeyId"].toString();
    QString secretAccessKey = obj["secretAccessKey"].toString();
    QString region          = obj["region"].toString();

    if (accessKeyId.isEmpty() ||
        secretAccessKey.isEmpty() ||
        region.isEmpty()) {
        qWarning("AWS config file is missing required fields");
        return false;
    }

    creds = Aws::Auth::AWSCredentials(
        accessKeyId.toStdString(),
        secretAccessKey.toStdString()
        );
    cfg.region = region.toStdString();
    return true;
}
