#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QString>

/**
 * @brief Holds user-configurable alert and notification settings.
 *
 * Exposes properties to QML/UI for email, SMS, alert thresholds, and notification frequency,
 * and provides a method to persist them to the database.
 */
class Settings : public QObject {
    Q_OBJECT

    /** @brief User’s email address for email alerts. */
    Q_PROPERTY(QString email
                   READ getEmail
                       WRITE setEmail
                           NOTIFY emailChanged)

    /** @brief User’s phone number for SMS alerts. */
    Q_PROPERTY(QString phoneNumber
                   READ getPhoneNumber
                       WRITE setPhoneNumber
                           NOTIFY phoneNumberChanged)

    /** @brief Threshold for non-critical alerts (e.g., number of changes). */
    Q_PROPERTY(QString nonCriticalAlertThreshold
                   READ getNonCriticalAlertThreshold
                       WRITE setNonCriticalAlertThreshold
                           NOTIFY nonCriticalAlertThresholdChanged)

    /** @brief How often notifications are sent (e.g., "hourly", "daily"). */
    Q_PROPERTY(QString notificationFrequency
                   READ getNotificationFrequency
                       WRITE setNotificationFrequency
                           NOTIFY notificationFrequencyChanged)

public:
    /**
     * @brief Construct a Settings object.
     * @param parent Optional parent QObject for ownership management.
     */
    explicit Settings(QObject *parent = nullptr);

    /**
     * @brief Persist the current settings to the database.
     *
     * Emits settingsSaved(true) on success, or settingsSaved(false) on failure.
     */
    Q_INVOKABLE void saveSettings();

    /** @brief Get the current email address. */
    QString getEmail() const;

    /**
     * @brief Update the email address.
     * @param email New email to store.
     *
     * Emits emailChanged() if the value differs.
     */
    Q_INVOKABLE void setEmail(const QString &email);

    /** @brief Get the current phone number. */
    QString getPhoneNumber() const;

    /**
     * @brief Update the phone number.
     * @param phoneNumber New phone number to store.
     *
     * Emits phoneNumberChanged() if the value differs.
     */
    Q_INVOKABLE void setPhoneNumber(const QString &phoneNumber);

    /** @brief Get the threshold for non-critical alerts. */
    QString getNonCriticalAlertThreshold() const;

    /**
     * @brief Update the non-critical alert threshold.
     * @param threshold New threshold value.
     *
     * Emits nonCriticalAlertThresholdChanged() if the value differs.
     */
    Q_INVOKABLE void setNonCriticalAlertThreshold(const QString &threshold);

    /** @brief Get the notification frequency setting. */
    QString getNotificationFrequency() const;

    /**
     * @brief Update the notification frequency.
     * @param frequency New frequency value.
     *
     * Emits notificationFrequencyChanged() if the value differs.
     */
    Q_INVOKABLE void setNotificationFrequency(const QString &frequency);

signals:
    /** @brief Emitted when the email property changes. */
    void emailChanged();

    /** @brief Emitted when the phoneNumber property changes. */
    void phoneNumberChanged();

    /** @brief Emitted when the nonCriticalAlertThreshold property changes. */
    void nonCriticalAlertThresholdChanged();

    /** @brief Emitted when the notificationFrequency property changes. */
    void notificationFrequencyChanged();

    /**
     * @brief Emitted after attempting to save settings.
     * @param success True if save succeeded, false otherwise.
     */
    void settingsSaved(bool success);

private:
    QString m_email;                       ///< Stored email address
    QString m_phoneNumber;                 ///< Stored phone number
    QString m_nonCriticalAlertThreshold;   ///< Non-critical alert threshold
    QString m_notificationFrequency;       ///< Notification frequency
};

#endif // SETTINGS_H
