#ifndef MACOSMONITORING_H
#define MACOSMONITORING_H

#include "monitoringBase.h"
#include "plistFile.h"
#include "plistFileModel.h"
#include "MacOSRollback.h"
#include "alert.h"
#include "settings.h"
#include "Database.h"

#include <QObject>
#include <QList>
#include <QTimer>
#include <QFileSystemWatcher>

/**
 * @brief Monitors macOS plist files for unauthorized changes.
 *
 * Extends MonitoringBase to:
 *  - Periodically scan a set of plist files.
 *  - Trigger rollbacks on critical changes.
 *  - Send alerts via SMS/email.
 *  - Log all changes to the database.
 */
class MacOSMonitoring : public MonitoringBase {
    Q_OBJECT
    Q_PROPERTY(PlistFileModel* plistFiles
                   READ plistFiles
                       NOTIFY plistFilesChanged)

public:
    /**
     * @brief Construct a MacOSMonitoring instance.
     * @param settings    Pointer to global Settings object.
     * @param parent      Parent QObject (default nullptr).
     */
    explicit MacOSMonitoring(Settings *settings, QObject *parent = nullptr);

    /**
     * @brief Start monitoring:
     *  - Sets up QFileSystemWatcher callbacks.
     *  - Launches the periodic QTimer.
     */
    Q_INVOKABLE void startMonitoring();

    /**
     * @brief Stop monitoring and disable timer & watcher.
     */
    Q_INVOKABLE void stopMonitoring();

    /**
     * @brief Mark a specific plist file as critical.
     * @param fileName    The full path or identifier of the plist.
     * @param isCritical  If true, any change triggers a rollback & alert.
     */
    Q_INVOKABLE void setFileCriticalStatus(const QString &fileName, bool isCritical);

    /**
     * @brief Temporarily allow the next change on a file without alerting.
     * @param fileName    The plist file to exempt once.
     */
    Q_INVOKABLE void allowChange(const QString &fileName);

    /**
     * @brief Access the QAbstractListModel of monitored plist files.
     * @return Pointer to the PlistFileModel.
     */
    PlistFileModel* plistFiles();

signals:
    /**
     * @brief Emitted when the overall monitoring status changes.
     * @param status  New status string (e.g. "Running", "Stopped").
     */
    void statusChanged(const QString &status);

    /**
     * @brief Emitted when the plistFiles model is updated (e.g. reload).
     */
    void plistFilesChanged();

    /**
     * @brief Emit a log message to the UI or log file.
     * @param message  Descriptive log text.
     */
    void logMessage(QString message);

    /**
     * @brief Emit when a critical change is detected.
     * @param message  Alert text describing the change.
     */
    void criticalChangeDetected(QString message);

    /**
     * @brief Emit when the user acknowledges a detected change.
     * @param fileName  The file for which the change was acknowledged.
     */
    void changeAcknowledged(const QString &fileName);

private slots:
    /**
     * @brief Periodic slot invoked by QTimer to scan for changes.
     */
    void checkForChanges();

    /**
     * @brief Invoked by QFileSystemWatcher when a file on disk changes.
     * @param path  Filesystem path to the changed plist.
     */
    void onPlistFileChanged(const QString &path);

private:
    /**
     * @brief Reload the list of plist files from Settings into the model.
     */
    void reloadPlistFiles();

    QList<PlistFile*>      m_plistFiles;         ///< Raw monitoring objects
    PlistFileModel         m_plistFilesModel;    ///< Exposed QAbstractListModel for UI
    MacOSRollback          m_rollback;           ///< Handles rollback operations
    Alert                  m_alert;              ///< Sends out alerts on critical events
    Settings              *m_settings;           ///< App configuration & thresholds
    QTimer                 m_timer;              ///< Drives periodic checks
    bool                   m_monitoringActive;   ///< True if monitoring is currently running
    Database               m_database;           ///< Logs all change events
    QFileSystemWatcher     m_fileWatcher;        ///< Watches for immediate file changes

    ///< Last-alerted values per file to debounce duplicate alerts
    QHash<QString, QString> m_lastAlertedValue;
};

#endif // MACOSMONITORING_H
