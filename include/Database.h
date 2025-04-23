#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QVariantList>
#include <QtSql/QSqlDatabase>

/**
 * @brief The Database class handles all interactions with the SQL database,
 * including schema creation, connection management, and CRUD operations for
 * user settings, configurations, and change logs.
 */
class Database : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the Database object and initializes the database connection.
     * @param parent Optional parent QObject.
     */
    explicit Database(QObject *parent = nullptr);

    /**
     * @brief Destructor closes the database connection if it's open.
     */
    ~Database();

    /**
     * @brief Creates the necessary database schema (tables, indices, etc.).
     * @return true if schema creation succeeds; false otherwise.
     */
    Q_INVOKABLE bool createSchema();

    /**
     * @brief Ensures the database connection is established, opening it if needed.
     */
    Q_INVOKABLE void ensureConnection();

    // User settings methods

    /**
     * @brief Inserts new user notification settings.
     * @param email User's email address.
     * @param phone User's phone number.
     * @param threshold Change count threshold for triggering alerts.
     * @param notificationFrequency How often to send alerts (e.g., "daily", "hourly").
     * @return true if insertion succeeds; false otherwise.
     */
    Q_INVOKABLE bool insertUserSettings(const QString &email,
                                        const QString &phone,
                                        int threshold,
                                        const QString &notificationFrequency);

    /**
     * @brief Inserts or updates existing user settings in a single operation.
     * @param email User's email address.
     * @param phone User's phone number.
     * @param threshold Change count threshold for triggering alerts.
     * @param notificationFrequency How often to send alerts.
     * @return true if the operation succeeds; false otherwise.
     */
    Q_INVOKABLE bool insertOrUpdateUserSettings(const QString &email,
                                                const QString &phone,
                                                int threshold,
                                                const QString &notificationFrequency);

    /**
     * @brief Retrieves all user settings records from the database.
     * @return List of user settings as a QVariantList of QVariantMap entries.
     */
    Q_INVOKABLE QVariantList getAllUserSettings();

    // Configuration settings methods

    /**
     * @brief Inserts or updates a configuration entry.
     * @param configName Unique name/key of the configuration.
     * @param configPath Filesystem path associated with this configuration.
     * @param configValue Value/data for the configuration.
     * @param isCritical Flag indicating if this configuration is critical.
     * @return true if operation succeeds; false otherwise.
     */
    Q_INVOKABLE bool insertOrUpdateConfiguration(const QString &configName,
                                                 const QString &configPath,
                                                 const QString &configValue,
                                                 bool isCritical);

    /**
     * @brief Retrieves all configuration entries from the database.
     * @return List of configurations as a QVariantList of QVariantMap entries.
     */
    Q_INVOKABLE QVariantList getAllConfigurations();

    // Change log methods

    /**
     * @brief Inserts a change log entry for a tracked configuration.
     * @param configName Name/key of the configuration that changed.
     * @param oldValue Previous value before change.
     * @param newValue New value after change.
     * @param acknowledged Whether the change has been acknowledged by the user.
     * @param critical Whether this change is critical (optional).
     * @return true if insertion succeeds; false otherwise.
     */
    Q_INVOKABLE bool insertChange(const QString &configName,
                                  const QString &oldValue,
                                  const QString &newValue,
                                  bool acknowledged,
                                  bool critical = false);

    /**
     * @brief Retrieves all change log entries.
     * @return List of change logs as a QVariantList of QVariantMap entries.
     */
    Q_INVOKABLE QVariantList getAllChanges();

    /**
     * @brief Updates the acknowledgment status for a specific configuration.
     * @param configName Name/key of the configuration to mark as acknowledged.
     * @return true if update succeeds; false otherwise.
     */
    Q_INVOKABLE bool updateAcknowledgmentStatus(const QString &configName);

    /**
     * @brief Searches change logs filtered by date and configuration name.
     * @param date Date string (e.g., "YYYY-MM-DD") to filter logs by.
     * @param configName Configuration name to filter by.
     * @return Filtered list of change logs as a QVariantList.
     */
    Q_INVOKABLE QVariantList searchChangeHistory(const QString &date,
                                                 const QString &configName);

    /**
     * @brief Retrieves counts of changes grouped by date and configuration.
     * @return List of counts as a QVariantList of QVariantMap entries.
     */
    Q_INVOKABLE QVariantList getChangesCountByDateAndConfig();

    /**
     * @brief Searches change logs within a date range and optional filters.
     * @param start Start date string ("YYYY-MM-DD").
     * @param end End date string ("YYYY-MM-DD").
     * @param configName Configuration name to filter by.
     * @param ackFilter Filter for acknowledgment status (e.g., true/false).
     * @param criticalFilter Filter for critical status (e.g., true/false).
     * @return Filtered list of change logs as a QVariantList.
     */
    Q_INVOKABLE QVariantList searchChangeHistoryRange(const QString &start,
                                                      const QString &end,
                                                      const QString &configName,
                                                      const QVariant &ackFilter,
                                                      const QVariant &criticalFilter);

    /**
     * @brief Resolves the filesystem path to encryption keys for secure database operations.
     * @return Path to the encryption keys directory or file.
     */
    QString resolveEncryptionKeysPath();

private:
    // Underlying Qt SQL database connection instance used for all operations
    QSqlDatabase db;
};

#endif // DATABASE_H
