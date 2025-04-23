#ifndef ALERT_H
#define ALERT_H

#include <QObject>
#include <QDateTime>
#include <vector>
#include <memory>
#include <aws/sns/SNSClient.h>
#include <aws/sesv2/SESV2Client.h>

// Forward declarations for classes used by Alert
class Database;
class Settings;

/**
 * @brief The Alert class manages sending alerts via SMS and Email using AWS SNS and SESv2.
 * It also enforces frequency limiting to avoid spamming.
 */
class Alert : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an Alert object.
     * @param settings Pointer to Settings object containing configuration.
     * @param parent Optional parent QObject.
     */
    explicit Alert(Settings *settings, QObject *parent = nullptr);

    /**
     * @brief Sends an alert message using configured channels (SMS and/or Email).
     * @param message The alert message content.
     * @return true if at least one alert was successfully sent; false otherwise.
     */
    bool sendAlert(const QString &message);

private:
    /**
     * @brief Sends an SMS alert to the specified phone number.
     * @param phoneNumber Recipient phone number in E.164 format.
     * @param message The message content.
     * @return true if the SMS was successfully sent; false otherwise.
     */
    bool sendSmsAlert(const QString &phoneNumber, const QString &message);

    /**
     * @brief Sends an Email alert to the specified email address.
     * @param email Recipient email address.
     * @param message The message content.
     * @return true if the email was successfully sent; false otherwise.
     */
    bool sendEmailAlert(const QString &email, const QString &message);

    /**
     * @brief Resolves the AWS configuration file path from environment or settings.
     * @return Path to the AWS config file.
     */
    QString resolveAwsConfigPath();

    // AWS SNS client used for sending SMS notifications
    std::unique_ptr<Aws::SNS::SNSClient>    m_snsClient;

    // AWS SESv2 client used for sending Email notifications
    std::unique_ptr<Aws::SESV2::SESV2Client> m_sesv2Client;

    // Pointer to the database for logging or retrieving alert-related data
    Database *m_database;

    // Pointer to application settings containing alert configuration (e.g., recipients, thresholds)
    Settings *m_settings;

    // Tracks timestamps of recent alerts for rate limiting.
    std::vector<QDateTime> m_alertTimestamps;
};

/**
 * @brief Utility function to load AWS credentials from a file.
 * @param filePath Path to the credentials/config file.
 * @param creds Output AWS credentials structure.
 * @param cfg Output AWS client configuration.
 * @return true if credentials were successfully loaded; false otherwise.
 */
bool loadAwsCredentials(const QString &filePath,
                        Aws::Auth::AWSCredentials &creds,
                        Aws::Client::ClientConfiguration &cfg);

#endif // ALERT_H
