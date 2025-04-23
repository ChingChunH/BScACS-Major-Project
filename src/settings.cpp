#include "settings.h"
#include "Database.h"
#include <QDebug>

/**
 * @brief Construct a Settings instance with default values.
 * @param parent Optional parent QObject for ownership.
 *
 * Initializes notificationFrequency to "Never".
 */
Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_notificationFrequency("Never")
{ }

/**
 * @brief Retrieve the stored email address.
 * @return Current email as QString.
 */
QString Settings::getEmail() const {
    return m_email;
}

/**
 * @brief Update the stored email address.
 * @param email New email to store.
 *
 * Emits emailChanged() if the value actually changes, and logs the update.
 */
void Settings::setEmail(const QString &email) {
    if (m_email != email) {
        m_email = email;
        emit emailChanged();
        qDebug() << "[SETTINGS] Email set to:" << email;
    }
}

/**
 * @brief Retrieve the stored phone number.
 * @return Current phone number as QString.
 */
QString Settings::getPhoneNumber() const {
    return m_phoneNumber;
}

/**
 * @brief Update the stored phone number.
 * @param phoneNumber New phone number to store.
 *
 * Emits phoneNumberChanged() if the value actually changes, and logs the update.
 */
void Settings::setPhoneNumber(const QString &phoneNumber) {
    if (m_phoneNumber != phoneNumber) {
        m_phoneNumber = phoneNumber;
        emit phoneNumberChanged();
        qDebug() << "[SETTINGS] Phone number set to:" << phoneNumber;
    }
}

/**
 * @brief Retrieve the threshold for non-critical alerts.
 * @return Non-critical alert threshold as QString.
 */
QString Settings::getNonCriticalAlertThreshold() const {
    return m_nonCriticalAlertThreshold;
}

/**
 * @brief Update the non-critical alert threshold.
 * @param threshold New threshold (as string) to store.
 *
 * Emits nonCriticalAlertThresholdChanged() if the value actually changes, and logs the update.
 */
void Settings::setNonCriticalAlertThreshold(const QString &threshold) {
    if (m_nonCriticalAlertThreshold != threshold) {
        m_nonCriticalAlertThreshold = threshold;
        emit nonCriticalAlertThresholdChanged();
        qDebug() << "[SETTINGS] Non-critical alert threshold set to:" << threshold;
    }
}

/**
 * @brief Retrieve the notification frequency setting.
 * @return Notification frequency (e.g. "Never", "Hourly") as QString.
 */
QString Settings::getNotificationFrequency() const {
    return m_notificationFrequency;
}

/**
 * @brief Update the notification frequency.
 * @param frequency New frequency string to store.
 *
 * Emits notificationFrequencyChanged() if the value actually changes, and logs the update.
 */
void Settings::setNotificationFrequency(const QString &frequency) {
    if (m_notificationFrequency != frequency) {
        m_notificationFrequency = frequency;
        emit notificationFrequencyChanged();
        qDebug() << "[SETTINGS] Notification frequency set to:" << frequency;
    }
}

/**
 * @brief Persist the current settings to the database.
 *
 * - Converts the non-critical threshold from QString to int.
 * - Calls Database::insertOrUpdateUserSettings() with email, phone, threshold, and frequency.
 * - Logs success or failure.
 * - Emits settingsSaved(success) to notify listeners of the result.
 */
void Settings::saveSettings() {
    int threshold = m_nonCriticalAlertThreshold.toInt();
    Database db;
    bool success = db.insertOrUpdateUserSettings(
        m_email,
        m_phoneNumber,
        threshold,
        m_notificationFrequency
        );

    if (success) {
        qDebug() << "[SETTINGS] Settings successfully saved to database.";
    } else {
        qDebug() << "[SETTINGS] Failed to save settings to database.";
    }
    emit settingsSaved(success);
}
