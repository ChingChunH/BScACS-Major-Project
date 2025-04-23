#include "MacOSMonitoring.h"
#include "MacOSJsonUtils.h"

#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <algorithm>

/**
 * @file MacOSMonitoring.cpp
 * @brief Implementation of MacOSMonitoring: watches a list of plist files,
 *        detects changes, logs them, triggers rollbacks for critical entries,
 *        and sends alerts via the Alert subsystem.
 */

////////////////////////////////////////////////////////////////////////////////
// Constructor / Initialization
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Construct and initialize a MacOSMonitoring instance.
 *
 * - Sets up a periodic QTimer to drive checkForChanges().
 * - Hooks rollbackPerformed → alert emission.
 * - Loads the initial plist list from JSON and watches that file for updates.
 * - Loads/stores user contact settings from the database.
 *
 * @param settings Pointer to the shared Settings object.
 * @param parent   Optional parent QObject.
 */
MacOSMonitoring::MacOSMonitoring(Settings *settings, QObject *parent)
    : MonitoringBase(parent)
    , m_monitoringActive(false)
    , m_settings(settings)
    , m_alert(settings, this)
    , m_database()  // Uses default-constructed Database instance
{
    // When the timer fires, invoke our change-checking routine
    connect(&m_timer, &QTimer::timeout,
            this, &MacOSMonitoring::checkForChanges);

    // When a rollback is performed, emit a criticalChangeDetected signal
    // and forward it to our Alert component.
    connect(&m_rollback, &MacOSRollback::rollbackPerformed,
            this, [this](const QString &valueName) {
                QString alertMessage =
                    "[CRITICAL] Revert performed for file: " + valueName;
                emit criticalChangeDetected(alertMessage);
                m_alert.sendAlert(alertMessage);
                qDebug() << "[INFO] Revert performed for file:" << valueName;
            });

    // Load the initial set of plist files from JSON configuration
    reloadPlistFiles();

    // Watch the JSON file itself so changes to the list of monitored plists
    // are picked up at runtime.
    QString filePath = QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        "/../../../../../resources/monitoredPlists.json");
    if (QFile::exists(filePath)) {
        m_fileWatcher.addPath(filePath);
        connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged,
                this, &MacOSMonitoring::onPlistFileChanged);
        qDebug() << "[MONITORING INIT] Watching JSON config:" << filePath;
    } else {
        qWarning() << "[MONITORING INIT] JSON file not found:" << filePath;
    }

    // Ensure we have email/phone in Settings; if not, load from DB and save back.
    if (m_settings) {
        QString email = m_settings->getEmail();
        QString phone = m_settings->getPhoneNumber();
        if (email.isEmpty() || phone.isEmpty()) {
            QVariantList userSettings = m_database.getAllUserSettings();
            if (!userSettings.isEmpty()) {
                QVariantMap user = userSettings.first().toMap();
                if (email.isEmpty()) {
                    email = user["email"].toString();
                    m_settings->setEmail(email);
                }
                if (phone.isEmpty()) {
                    phone = user["phone"].toString();
                    m_settings->setPhoneNumber(phone);
                }
            }
        }
        qDebug() << "[MONITORING INIT] Final email:" << email
                 << "phone:" << phone;

        // Persist back into the database if both are now set
        if (!email.isEmpty() && !phone.isEmpty()) {
            QString freq = m_settings->getNotificationFrequency();
            if (!m_database.insertUserSettings(email, phone, 0, freq)) {
                qWarning() << "[MONITORING INIT] Failed to save user settings.";
            } else {
                qDebug() << "[MONITORING INIT] User settings saved.";
            }
        } else {
            qWarning() << "[MONITORING INIT] Missing contact info; alerts disabled.";
        }
    } else {
        qWarning() << "[MONITORING INIT] Settings object is null.";
    }
}

////////////////////////////////////////////////////////////////////////////////
// JSON Reloading
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Reload the list of monitored plist files from JSON config.
 *
 * - Reads JSON array of {plistPath, valueName, isCritical}.
 * - Updates the model, emits plistFilesChanged().
 * - Inserts/updates each entry into ConfigurationSettings.
 */
void MacOSMonitoring::reloadPlistFiles() {
    QString filePath = QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        "/../../../../../resources/monitoredPlists.json");
    if (!QFile::exists(filePath)) {
        qWarning() << "[RELOAD PLIST] JSON not found:" << filePath;
        return;
    }

    // Parse JSON into new list
    QList<PlistFile*> newPlistFiles =
        MacOSJsonUtils::readFilesFromJson(filePath);
    if (newPlistFiles.isEmpty()) {
        qWarning() << "[RELOAD PLIST] No entries in JSON.";
        return;
    }

    // Update internal containers and notify UI
    m_plistFiles = newPlistFiles;
    m_plistFilesModel.setPlistFiles(m_plistFiles);
    emit plistFilesChanged();

    // Persist these plist definitions into the database
    Database db;
    for (PlistFile *plist : m_plistFiles) {
        db.insertOrUpdateConfiguration(
            plist->valueName(),
            plist->plistPath(),
            plist->value(),
            plist->isCritical()
            );
    }

    qDebug() << "[RELOAD PLIST] Loaded" << m_plistFiles.size()
             << "plist files from JSON.";
}

/**
 * @brief Slot invoked when the JSON config file itself is modified.
 * @param path Filesystem path of the changed JSON file.
 */
void MacOSMonitoring::onPlistFileChanged(const QString &path) {
    qDebug() << "[FILE WATCHER] JSON changed:" << path;
    reloadPlistFiles();
    // Re-add watcher if needed
    if (!m_fileWatcher.files().contains(path)) {
        m_fileWatcher.addPath(path);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Monitoring Control
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Start the periodic monitoring process.
 */
void MacOSMonitoring::startMonitoring() {
    if (!m_monitoringActive) {
        m_monitoringActive = true;
        m_timer.start(1000);  // Check every second
        qDebug() << "[START MONITORING] Started.";
        emit statusChanged("Monitoring started");
    }
}

/**
 * @brief Stop the periodic monitoring process.
 */
void MacOSMonitoring::stopMonitoring() {
    if (m_monitoringActive) {
        m_monitoringActive = false;
        m_timer.stop();
        qDebug() << "[STOP MONITORING] Stopped.";
        emit statusChanged("Monitoring stopped");
    }
}

////////////////////////////////////////////////////////////////////////////////
// Change Detection & Handling
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Scan all monitored plist files for value changes.
 *
 * For each changed file:
 *  - Log the change in the Changes table.
 *  - Debounce duplicate alerts by tracking m_lastAlertedValue.
 *  - If critical: perform rollback & send critical alert.
 *  - If non-critical: track change count, alert if threshold met.
 *  - Update ConfigurationSettings and in-memory value.
 */
void MacOSMonitoring::checkForChanges() {
    for (PlistFile *plist : m_plistFiles) {
        QString currentValue = plist->getCurrentValue();
        QString prevValue    = plist->value();

        // If value differs, handle the change
        if (currentValue != prevValue) {
            qDebug() << "[DEBUG] Change:" << plist->plistPath()
            << "from" << prevValue
            << "to"   << currentValue;

            Database db;
            db.insertChange(plist->valueName(), prevValue, currentValue, false);

            // Skip if we already alerted for this exact new value
            if (m_lastAlertedValue.value(plist->valueName()) == currentValue) {
                qDebug() << "[DEBUG] Debounced duplicate change for"
                         << plist->plistPath();
                plist->setValue(currentValue);
                continue;
            }
            m_lastAlertedValue.insert(plist->valueName(), currentValue);

            QString alertMessage;

            if (plist->isCritical()) {
                // Critical: perform rollback if needed
                plist->setRollbackCancelled(false);
                plist->setNewValue(currentValue);
                m_rollback.rollbackIfNeeded(plist);
                alertMessage = "[CRITICAL ALERT] " +
                               plist->plistPath() +
                               " changed to " + currentValue;
            } else {
                // Non-critical: accumulate count and compare to threshold
                plist->incrementChangeCount();
                int threshold = m_settings->getNonCriticalAlertThreshold().toInt();
                int count     = plist->changeCount();

                qDebug() << "[INFO] Non-critical count for"
                         << plist->plistPath()
                         << "=" << count
                         << "threshold=" << threshold;

                if (threshold > 0 && count >= threshold) {
                    alertMessage = "[ALERT] Threshold reached for " +
                                   plist->plistPath() +
                                   ": " + currentValue;
                    plist->resetChangeCount();
                    qDebug() << "[DEBUG] Reset count for"
                             << plist->plistPath();
                } else {
                    // Below threshold: just update config and continue
                    db.insertOrUpdateConfiguration(
                        plist->valueName(),
                        plist->plistPath(),
                        currentValue,
                        false
                        );
                    plist->setValue(currentValue);
                    continue;
                }
            }

            // Send the alert and persist final state
            m_alert.sendAlert(alertMessage);
            db.insertOrUpdateConfiguration(
                plist->valueName(),
                plist->plistPath(),
                currentValue,
                plist->isCritical()
                );
            plist->setValue(currentValue);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// User Overrides
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Allow the next change on a given plist without rollback/alert.
 * @param fileName The full path of the plist file to exempt.
 *
 * - Marks any matching Changes entry as acknowledged.
 * - Cancels the pending rollback on the corresponding PlistFile.
 */
void MacOSMonitoring::allowChange(const QString &fileName) {
    qDebug() << "[ALLOW CHANGE] for" << fileName;

    Database db;
    // Acknowledge in the Changes table if not already done
    if (db.updateAcknowledgmentStatus(fileName)) {
        qDebug() << "[ALLOW CHANGE] Acknowledged in DB for" << fileName;
        emit changeAcknowledged(fileName);
    } else {
        qDebug() << "[ALLOW CHANGE] Nothing to acknowledge for" << fileName;
    }

    // Cancel rollback on the in-memory PlistFile
    for (PlistFile *plist : m_plistFiles) {
        if (plist->plistPath() == fileName) {
            plist->setRollbackCancelled(true);
            m_rollback.cancelRollback(plist);
            plist->setPreviousValue(plist->newValue());
            qDebug() << "[ALLOW CHANGE] Cancelled rollback for" << fileName;
            break;
        }
    }
}

/**
 * @brief Mark or unmark a plist file as critical at runtime.
 * @param fileName   The valueName identifier of the plist entry.
 * @param isCritical True to treat subsequent changes as critical.
 */
void MacOSMonitoring::setFileCriticalStatus(const QString &fileName, bool isCritical) {
    qDebug() << "[DEBUG] setFileCriticalStatus:" << fileName << isCritical;

    for (int i = 0; i < m_plistFiles.size(); ++i) {
        PlistFile *plist = m_plistFiles[i];
        if (plist->valueName() == fileName) {
            // Update in-memory flag and notify any bound views
            plist->setCritical(isCritical);
            QModelIndex idx = m_plistFilesModel.index(i);
            emit m_plistFilesModel.dataChanged(
                idx, idx, { PlistFileModel::DisplayTextRole }
                );

            // Register or unregister rollback as needed
            Database db;
            if (isCritical) {
                m_rollback.plistFileForRollback(plist);
            }
            db.insertOrUpdateConfiguration(
                plist->valueName(),
                plist->plistPath(),
                plist->value(),
                isCritical
                );
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Accessors
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Provide access to the underlying PlistFileModel for UI binding.
 * @return Pointer to the model.
 */
PlistFileModel* MacOSMonitoring::plistFiles() {
    return &m_plistFilesModel;
}
