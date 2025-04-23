#ifndef WINDOWSMONITORING_H
#define WINDOWSMONITORING_H

#include <QObject>
#include <QList>
#include <QTimer>
#include <QHash>
#include <QDateTime>
#include <QFileSystemWatcher>
#include "registryKey.h"
#include "registryKeyModel.h"
#include "WindowsRollback.h"
#include "alert.h"
#include "settings.h"
#include "monitoringBase.h"
#include "Database.h"

/**
 * @brief Monitors Windows registry keys for unauthorized changes.
 *
 * Inherits from MonitoringBase to:
 *  - Periodically scan configured registry keys.
 *  - Trigger rollbacks on critical changes.
 *  - Send SMS/email alerts.
 *  - Log all changes to the database.
 *  - Allow one-time exemptions for planned changes.
 */
class WindowsMonitoring : public MonitoringBase {
    Q_OBJECT

    /**
     * @brief Exposes the list-model of monitored RegistryKey objects to QML/UI.
     */
    Q_PROPERTY(RegistryKeyModel* registryKeys
                   READ registryKeys
                       NOTIFY registryKeysChanged)

public:
    /**
     * @brief Construct a WindowsMonitoring instance.
     * @param settings  Pointer to global Settings for thresholds & contact info.
     * @param parent    Optional QObject parent.
     */
    explicit WindowsMonitoring(Settings *settings, QObject *parent = nullptr);

    /**
     * @brief Begin monitoring:
     *  - Initializes file watcher for JSON config.
     *  - Starts periodic QTimer scanning.
     */
    Q_INVOKABLE void startMonitoring();

    /**
     * @brief Stop monitoring and disable timers and watchers.
     */
    Q_INVOKABLE void stopMonitoring();

    /**
     * @brief Mark or unmark a registry key as critical.
     * @param keyName    Full name or identifier of the registry key.
     * @param isCritical True to treat changes as critical (rollback+alert).
     */
    Q_INVOKABLE void setKeyCriticalStatus(const QString &keyName, bool isCritical);

    /**
     * @brief Allow the next change on a specific key without triggering rollback or alert.
     * @param keyName  The registry key to exempt once.
     */
    Q_INVOKABLE void allowChange(const QString &keyName);

    /**
     * @brief Manually reload the monitoredKeys.json configuration file.
     *
     * Useful if the JSON list of keys is updated at runtime.
     */
    Q_INVOKABLE void reloadMonitoredKeys();

    /**
     * @brief Access the QAbstractListModel of monitored registry keys.
     * @return Pointer to the RegistryKeyModel instance.
     */
    RegistryKeyModel* registryKeys();

signals:
    /**
     * @brief Emitted when the overall monitoring status changes.
     * @param status  New status string (e.g. "Running", "Stopped").
     */
    void statusChanged(const QString &status);

    /**
     * @brief Emitted whenever any key’s value changes.
     * @param key    The name of the registry key.
     * @param value  The new value read from the registry.
     */
    void keyChanged(const QString &key, const QString &value);

    /**
     * @brief Emitted when the registryKeys model is reset or reloaded.
     */
    void registryKeysChanged();

    /**
     * @brief Emit a log message for display or persistent logging.
     * @param message  Text of the log entry.
     */
    void logMessage(const QString &message);

    /**
     * @brief Emit when a critical change is detected and an alert is sent.
     * @param message  Alert description.
     */
    void criticalChangeDetected(const QString &message);

    /**
     * @brief Emitted when a user acknowledges a detected change.
     * @param keyName  The registry key for which the change was acknowledged.
     */
    void changeAcknowledged(const QString &keyName);

private slots:
    /**
     * @brief Periodic slot invoked by QTimer to scan all monitored keys.
     */
    void checkForChanges();

    /**
     * @brief Invoked by QFileSystemWatcher when the JSON config file changes.
     * @param path  Filesystem path to the modified file.
     */
    void onMonitoredFileChanged(const QString &path);

private:
    QList<RegistryKey*>   m_registryKeys;        ///< List of monitored registry keys
    RegistryKeyModel      m_registryKeysModel;   ///< Exposed model for UI binding
    WindowsRollback       m_rollback;            ///< Manages rollback operations
    Alert                 m_alert;               ///< Sends alerts on critical events
    Settings             *m_settings;            ///< User settings & thresholds
    QTimer                m_timer;               ///< Drives periodic checking
    bool                  m_monitoringActive;    ///< True if monitoring is active
    Database              m_database;            ///< Logs change events

    QHash<QString, QString> m_lastAlertedValue;  ///< Debounce duplicate alerts per key
    QVector<QDateTime>      m_alertTimestamps;   ///< Track global alert send times
    QFileSystemWatcher      m_fileWatcher;       ///< Watches monitoredKeys.json for updates
};

#endif // WINDOWSMONITORING_H
