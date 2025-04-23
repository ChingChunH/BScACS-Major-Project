/**
 * @file WindowsMonitoring.cpp
 * @brief Implements WindowsMonitoring: watches a set of registry keys,
 *        detects changes, logs them, triggers rollbacks for critical entries,
 *        and sends alerts via the Alert subsystem.
 */

#include "WindowsMonitoring.h"
#include "WindowsJsonUtils.h"
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////
// Constructor / Initialization
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Construct and initialize a WindowsMonitoring instance.
 *
 * - Sets up a periodic timer to invoke checkForChanges().
 * - Hooks rollbackPerformed → criticalChangeDetected.
 * - Loads the initial key list from JSON and watches that file for updates.
 * - Ensures user contact settings are loaded or stored in the database.
 *
 * @param settings Pointer to the shared Settings object.
 * @param parent   Optional parent QObject.
 */
WindowsMonitoring::WindowsMonitoring(Settings *settings, QObject *parent)
    : MonitoringBase(parent)
    , m_monitoringActive(false)
    , m_settings(settings)
    , m_alert(settings, this)
    , m_database()  // Uses default-constructed Database instance
{
    // When the timer fires, perform change detection
    connect(&m_timer, &QTimer::timeout,
            this, &WindowsMonitoring::checkForChanges);

    // When a rollback is performed, emit a criticalChangeDetected signal
    connect(&m_rollback, &WindowsRollback::rollbackPerformed,
            this, [this](const QString &valueName) {
                QString alertMessage =
                    "[CRITICAL] Rollback performed for key: " + valueName;
                emit criticalChangeDetected(alertMessage);
                qDebug() << "[INFO] Rollback performed for key:" << valueName;
            });

    // Load the list of monitored registry keys from JSON
    reloadMonitoredKeys();

    // Watch the JSON file itself for runtime updates
    QString filePath = QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        "/../../resources/monitoredKeys.json");
    if (QFile::exists(filePath)) {
        m_fileWatcher.addPath(filePath);
        connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged,
                this, &WindowsMonitoring::onMonitoredFileChanged);
        qDebug() << "[MONITORING INIT] Watching JSON config:" << filePath;
    } else {
        qWarning() << "[MONITORING INIT] JSON file not found at:" << filePath;
    }

    // Ensure Settings has email/phone; otherwise load from DB and persist
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

        // Persist back into the database if at least one contact is set
        if (!email.isEmpty() || !phone.isEmpty()) {
            QString freq = m_settings->getNotificationFrequency();
            if (!m_database.insertUserSettings(email, phone, 0, freq)) {
                qWarning() << "[MONITORING INIT] Failed to save settings.";
            } else {
                qDebug() << "[MONITORING INIT] User settings saved.";
            }
        } else {
            qWarning() << "[MONITORING INIT]"
                       << "Both email and phone empty; alerts disabled.";
        }
    } else {
        qWarning() << "[MONITORING INIT] Settings object is null.";
    }
}

////////////////////////////////////////////////////////////////////////////////
// JSON Reloading
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Reload the list of monitored registry keys from JSON config.
 *
 * - Reads JSON array of {hive, keyPath, valueName, isCritical}.
 * - Updates the model, emits registryKeysChanged().
 * - Inserts/updates each entry into the ConfigurationSettings table.
 */
void WindowsMonitoring::reloadMonitoredKeys() {
    QString filePath = QDir::cleanPath(
        QCoreApplication::applicationDirPath() +
        "/../../resources/monitoredKeys.json");
    if (!QFile::exists(filePath)) {
        qWarning() << "[RELOAD KEYS] JSON not found at:" << filePath;
        return;
    }

    // Parse JSON into new list
    QList<RegistryKey*> newKeys =
        WindowsJsonUtils::readKeysFromJson(filePath);
    if (newKeys.isEmpty()) {
        qWarning() << "[RELOAD KEYS] No entries in JSON.";
        return;
    }

    // Update internal containers and notify UI
    m_registryKeys = newKeys;
    m_registryKeysModel.setRegistryKeys(m_registryKeys);
    emit registryKeysChanged();

    // Persist these key definitions into the database
    for (RegistryKey *key : m_registryKeys) {
        m_database.insertOrUpdateConfiguration(
            key->name(),
            key->keyPath(),
            key->value(),
            key->isCritical()
            );
    }

    qDebug() << "[RELOAD KEYS] Loaded"
             << m_registryKeys.size()
             << "registry keys from JSON.";
}

/**
 * @brief Slot invoked when the JSON config file itself is modified.
 * @param path Filesystem path of the changed JSON file.
 */
void WindowsMonitoring::onMonitoredFileChanged(const QString &path) {
    qDebug() << "[FILE WATCHER] JSON changed:" << path;
    reloadMonitoredKeys();
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
void WindowsMonitoring::startMonitoring() {
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
void WindowsMonitoring::stopMonitoring() {
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
 * @brief Scan all monitored registry keys for value changes.
 *
 * For each changed key:
 *  - Log the change in the Changes table.
 *  - Debounce duplicate alerts by tracking m_lastAlertedValue.
 *  - If critical: schedule rollback and a delayed alert after 10 seconds.
 *  - If non-critical: track change count, alert when threshold met.
 *  - Update ConfigurationSettings and in-memory value.
 */
void WindowsMonitoring::checkForChanges() {
    for (RegistryKey *key : m_registryKeys) {
        QString currentValue = key->getCurrentValue();
        QString prevValue    = key->value();

        // Detect a change
        if (currentValue != prevValue) {
            qDebug() << "[DEBUG] Change:" << key->name()
            << "from" << prevValue
            << "to"   << currentValue;

            // Log the change
            m_database.insertChange(
                key->name(),
                prevValue,
                currentValue,
                false
                );

            // Debounce duplicate alerts
            if (m_lastAlertedValue.value(key->name()) == currentValue) {
                qDebug() << "[DEBUG] Debounced duplicate for"
                         << key->name();
                key->setValue(currentValue);
                continue;
            }
            m_lastAlertedValue.insert(key->name(), currentValue);

            if (key->isCritical()) {
                // Critical: allow user 10s to acknowledge before alert
                key->setRollbackCancelled(false);
                key->setNewValue(currentValue);
                m_rollback.rollbackIfNeeded(key);

                QString pendingMsg =
                    "[CRITICAL ALERT] Key: " + key->name() +
                    " changed to " + currentValue;

                // Delay alert by 10 seconds
                QTimer::singleShot(10000, this, [this, key, pendingMsg]() {
                    if (!key->isRollbackCancelled()) {
                        if (m_settings->getNotificationFrequency().compare(
                                "Never", Qt::CaseInsensitive) == 0) {
                            qDebug() << "[ALERT] Alerts are disabled (Never).";
                        } else {
                            bool sent = m_alert.sendAlert(pendingMsg);
                            qDebug() << (sent
                                             ? "[INFO] Delayed critical alert sent for" + key->name()
                                             : "[INFO] Delayed alert skipped for" + key->name());
                        }
                    } else {
                        qDebug() << "[INFO] Change acknowledged before delay for"
                                 << key->name();
                    }
                });
            } else {
                // Non-critical: count and threshold logic
                key->incrementChangeCount();
                int threshold = m_settings->getNonCriticalAlertThreshold().toInt();
                int count     = key->changeCount();
                int remaining = (threshold > 0) ? (threshold - count) : 0;

                qDebug() << "[INFO] Key:" << key->name()
                         << "count:" << count
                         << "threshold:" << threshold;

                if (threshold > 0 && count >= threshold) {
                    qDebug() << "[INFO] Threshold reached for" << key->name();
                    QString msg = "[ALERT] Non-critical threshold reached for " +
                                  key->name() + ": " + currentValue;
                    key->resetChangeCount();
                    qDebug() << "[DEBUG] Reset count for" << key->name();

                    if (m_settings->getNotificationFrequency().compare(
                            "Never", Qt::CaseInsensitive) != 0) {
                        bool sent = m_alert.sendAlert(msg);
                        qDebug() << (sent
                                         ? "[INFO] Non-critical alert sent for" + key->name()
                                         : "[INFO] Non-critical alert skipped for" + key->name());
                    } else {
                        qDebug() << "[INFO] Alerts disabled (Never) for" << key->name();
                    }
                } else {
                    if (threshold > 0) {
                        qDebug() << "[INFO]" << remaining
                                 << "more change(s) needed for" << key->name();
                    }
                    // Update DB and memory without alert
                    m_database.insertOrUpdateConfiguration(
                        key->name(),
                        key->keyPath(),
                        currentValue,
                        false
                        );
                    key->setValue(currentValue);
                    continue;
                }
            }

            // Persist the final state
            m_database.insertOrUpdateConfiguration(
                key->name(),
                key->keyPath(),
                currentValue,
                key->isCritical()
                );
            key->setValue(currentValue);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// User Overrides
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Allow the next change on a given key without rollback/alert.
 * @param keyName The name of the registry key to exempt.
 *
 * - Marks matching Changes entry as acknowledged.
 * - Cancels rollback on the RegistryKey instance.
 */
void WindowsMonitoring::allowChange(const QString &keyName) {
    qDebug() << "[ALLOW CHANGE] for" << keyName;
    bool alreadyAck = false;

    // Acknowledge in Changes table
    QVariantList changes = m_database.getAllChanges();
    for (const QVariant &change : changes) {
        QVariantMap m = change.toMap();
        if (m["config_name"] == keyName && m["acknowledged"].toBool()) {
            alreadyAck = true;
            break;
        }
    }
    if (!alreadyAck && m_database.updateAcknowledgmentStatus(keyName)) {
        qDebug() << "[ALLOW CHANGE] Acknowledged in DB for" << keyName;
        emit changeAcknowledged(keyName);
    } else {
        qDebug() << "[ALLOW CHANGE] Nothing to acknowledge for" << keyName;
    }

    // Cancel rollback on the in-memory key
    for (RegistryKey *key : m_registryKeys) {
        if (key->name() == keyName) {
            key->setRollbackCancelled(true);
            m_rollback.cancelRollback(key);
            key->setPreviousValue(key->newValue());
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Critical Status Toggling
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Mark or unmark a registry key as critical at runtime.
 * @param keyName   The name identifier of the registry key.
 * @param isCritical True to treat changes as critical (rollback+alert).
 */
void WindowsMonitoring::setKeyCriticalStatus(const QString &keyName, bool isCritical) {
    for (int i = 0; i < m_registryKeys.size(); ++i) {
        RegistryKey *key = m_registryKeys[i];
        if (key->name() == keyName) {
            key->setCritical(isCritical);
            QModelIndex idx = m_registryKeysModel.index(i);
            emit m_registryKeysModel.dataChanged(
                idx, idx, { RegistryKeyModel::DisplayTextRole }
                );

            if (isCritical) {
                m_rollback.registerKeyForRollback(key);
            }
            m_database.insertOrUpdateConfiguration(
                key->name(),
                key->keyPath(),
                key->value(),
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
 * @brief Provide access to the underlying RegistryKeyModel for UI binding.
 * @return Pointer to the model instance.
 */
RegistryKeyModel* WindowsMonitoring::registryKeys() {
    return &m_registryKeysModel;
}
