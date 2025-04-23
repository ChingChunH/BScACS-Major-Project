#include "registryKey.h"   // Definition of RegistryKey class
#include <QDebug>          // qDebug / qWarning for logging
#include <QSettings>       // QSettings for registry I/O

/**
 * @brief Construct a RegistryKey monitor for a Windows registry value.
 *
 * - Builds the full registry path (hive + keyPath).
 * - Initializes QSettings in NativeFormat to read/write that key.
 * - Reads and caches the initial value for change detection.
 * - Updates the displayText and logs the initial state.
 *
 * @param hive         Registry hive (e.g., "HKEY_CURRENT_USER" or "HKEY_LOCAL_MACHINE").
 * @param keyPath      Path under the hive (e.g., "Software\\MyApp\\Settings").
 * @param valueName    Name of the registry value to monitor.
 * @param isCritical   If true, changes trigger alerts/rollback.
 * @param parent       Optional QObject parent.
 */
RegistryKey::RegistryKey(const QString &hive,
                         const QString &keyPath,
                         const QString &valueName,
                         bool isCritical,
                         QObject *parent)
    : QObject(parent)
    , m_hive(hive)
    , m_keyPath(keyPath)
    , m_valueName(valueName)
    , m_isCritical(isCritical)
{
    // Compose the full registry path for QSettings
    QString fullKey = (m_hive == "HKEY_CURRENT_USER"
                           ? "HKEY_CURRENT_USER\\"
                           : "HKEY_LOCAL_MACHINE\\")
                      + m_keyPath;

    // Initialize QSettings for native Windows registry I/O
    m_settings = new QSettings(fullKey, QSettings::NativeFormat);

    // Read and cache the current on-disk value
    m_value = readCurrentValue();
    m_previousValue = m_value;

    // Prepare display text and log initialization
    updateDisplayText();
    qDebug() << "[INIT] RegistryKey:" << m_valueName
             << "Initial Value:" << m_value;
}

/**
 * @brief Check if a previously scheduled rollback has been cancelled.
 * @return True if rollback was cancelled for this key.
 */
bool RegistryKey::isRollbackCancelled() const {
    return m_rollbackCancelled;
}

/**
 * @brief Mark or unmark the rollback-cancelled flag.
 * @param cancelled True to skip the next automatic rollback.
 */
void RegistryKey::setRollbackCancelled(bool cancelled) {
    m_rollbackCancelled = cancelled;
}

/**
 * @brief Update the displayText property based on critical status.
 *
 * Emits displayTextChanged() so any bound UI can refresh.
 */
void RegistryKey::updateDisplayText() {
    m_displayText = m_isCritical
                        ? QString("%1 - Critical").arg(m_valueName)
                        : m_valueName;
    emit displayTextChanged();
}

/**
 * @brief Query whether this registry entry is treated as critical.
 * @return True if critical changes trigger rollback/alerts.
 */
bool RegistryKey::isCritical() const {
    return m_isCritical;
}

/**
 * @brief Set or clear the critical flag.
 *
 * If changed, updates displayText and emits isCriticalChanged().
 *
 * @param critical New critical status.
 */
void RegistryKey::setCritical(bool critical) {
    if (m_isCritical != critical) {
        m_isCritical = critical;
        qDebug() << "[INFO] RegistryKey" << m_valueName
                 << "critical set to" << m_isCritical;
        updateDisplayText();
        emit isCriticalChanged();
    }
}

/**
 * @brief Return the user-facing display text.
 * @return The valueName plus an optional " - Critical" suffix.
 */
QString RegistryKey::displayText() const {
    return m_displayText;
}

/**
 * @brief Return the name (valueName) of this registry entry.
 * @return The registry value name being monitored.
 */
QString RegistryKey::name() const {
    return m_valueName;
}

/**
 * @brief Get the last-known in-memory value.
 * @return Cached m_value.
 */
QString RegistryKey::value() const {
    return m_value;
}

/**
 * @brief Read the current on-disk registry value.
 *
 * Uses the existing QSettings instance.
 *
 * @return The registry value as a QString, or empty if missing.
 */
QString RegistryKey::getCurrentValue() const {
    return readCurrentValue();
}

/**
 * @brief Retrieve the cached previous value (before the last setValue).
 * @return The previousValue field.
 */
QString RegistryKey::previousValue() const {
    return m_previousValue;
}

/**
 * @brief Store a “previous” value baseline for potential rollback.
 * @param value The value to remember as previous.
 */
void RegistryKey::setPreviousValue(const QString &value) {
    m_previousValue = value;
}

/**
 * @brief Cache the pending new value before performing rollback.
 * @param value The new (possibly unauthorized) value.
 */
void RegistryKey::setNewValue(const QString &value) {
    m_newValue = value;
}

/**
 * @brief Return the cached newValue (used when cancelling rollback).
 * @return The newValue field.
 */
QString RegistryKey::newValue() const {
    return m_newValue;
}

/**
 * @brief Write a new value to the registry and update internal state.
 *
 * - Shifts m_value into m_previousValue.
 * - Updates m_value.
 * - Writes via QSettings and sync().
 * - Re-creates QSettings instance to refresh.
 * - Confirms the write succeeded by reading back.
 *
 * @param value The new registry value to set.
 */
void RegistryKey::setValue(const QString &value) {
    if (m_value != value) {
        // Cache old value
        m_previousValue = m_value;
        m_value = value;

        // Write to registry
        m_settings->setValue(m_valueName, m_value);
        m_settings->sync();

        // Reinitialize QSettings to reload any internal state
        delete m_settings;
        QString fullKey = (m_hive == "HKEY_CURRENT_USER"
                               ? "HKEY_CURRENT_USER\\"
                               : "HKEY_LOCAL_MACHINE\\")
                          + m_keyPath;
        m_settings = new QSettings(fullKey, QSettings::NativeFormat);

        // Confirm the write
        QString confirmedValue = m_settings->value(m_valueName).toString();
        qDebug() << "[SET] RegistryKey:" << m_valueName
                 << "Confirmed Value:" << confirmedValue;
        if (confirmedValue != m_value) {
            qWarning() << "[WARNING] Mismatch after set for key:" << m_keyPath;
        }
    }
}

/**
 * @brief Read directly from QSettings the current registry value.
 * @return The on-disk value, or an empty QString if unavailable.
 */
QString RegistryKey::readCurrentValue() const {
    return m_settings->value(m_valueName).toString();
}
