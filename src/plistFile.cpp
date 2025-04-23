#include "plistFile.h"
#include <QDebug>
#include <QFile>
#include <QDir>

/**
 * @brief Construct a PlistFile monitor for a given plist path and key.
 *
 * - Expands a leading "~" to the user’s home directory.
 * - Verifies the file exists and initializes QSettings for native plist I/O.
 * - Reads and caches the initial value for change detection.
 * - Emits displayTextChanged and logs the initial state.
 *
 * @param plistPath   Filesystem path to the .plist (may begin with "~").
 * @param valueName   The key inside the plist to monitor.
 * @param isCritical  If true, changes are treated as critical (rollback + alert).
 * @param parent      Optional parent QObject for Qt ownership.
 */
PlistFile::PlistFile(const QString &plistPath,
                     const QString &valueName,
                     bool isCritical,
                     QObject *parent)
    : QObject(parent)
    , m_plistPath(plistPath)
    , m_valueName(valueName)
    , m_isCritical(isCritical)
{
    // Expand "~" to home directory if present
    QString expandedPath = m_plistPath;
    if (expandedPath.startsWith("~")) {
        expandedPath.replace(0, 1, QDir::homePath());
    }

    // Warn if the file is missing
    if (!QFile::exists(expandedPath)) {
        qWarning() << "[PLISTFILE] File does not exist:" << expandedPath;
    } else {
        qDebug() << "[PLISTFILE] File found at path:" << expandedPath;
    }

    // Initialize QSettings to read/write the plist in native format
    m_settings = new QSettings(expandedPath, QSettings::NativeFormat);

    if (!m_settings) {
        qWarning() << "[PLISTFILE] QSettings initialization failed for:" << expandedPath;
    } else {
        qDebug() << "[PLISTFILE] QSettings initialized for file:" << expandedPath;
    }

    // Cache the current and previous values for change detection
    m_value = readCurrentValue();
    m_previousValue = m_value;

    // Prepare the display text and log initial state
    updateDisplayText();
    qDebug() << "[INIT] Plist key:" << m_valueName << ", Initial Value:" << m_value;
}

/**
 * @brief Check whether a previously scheduled rollback has been cancelled.
 * @return True if rollback was cancelled.
 */
bool PlistFile::isRollbackCancelled() const {
    return m_rollbackCancelled;
}

/**
 * @brief Mark rollback as cancelled (skip the next automatic restore).
 * @param cancelled New cancellation flag.
 */
void PlistFile::setRollbackCancelled(bool cancelled) {
    m_rollbackCancelled = cancelled;
}

/**
 * @brief Update the text shown in the UI, including critical flag.
 * Emits displayTextChanged() to notify views.
 */
void PlistFile::updateDisplayText() {
    m_displayText = m_isCritical
                        ? QString("%1 - Critical").arg(m_valueName)
                        : m_valueName;
    emit displayTextChanged();
}

/**
 * @brief Return whether this key is marked critical.
 * @return True if critical.
 */
bool PlistFile::isCritical() const {
    qDebug() << "[DEBUG] isCritical for" << m_valueName << ":" << m_isCritical;
    return m_isCritical;
}

/**
 * @brief Change the criticality flag.
 * Emits isCriticalChanged() and updates display text if changed.
 *
 * @param critical New critical status.
 */
void PlistFile::setCritical(bool critical) {
    if (m_isCritical != critical) {
        m_isCritical = critical;
        qDebug() << "[DEBUG] Critical status for" << m_valueName
                 << "updated to" << m_isCritical;
        updateDisplayText();
        emit isCriticalChanged();
    }
}

/**
 * @brief Return the current display text.
 * @return Display text combining key name and critical marker.
 */
QString PlistFile::displayText() const {
    return m_displayText;
}

/**
 * @brief Return the monitored plist file path (as given).
 * @return Plist file path.
 */
QString PlistFile::plistPath() const {
    return m_plistPath;
}

/**
 * @brief Return the key name inside the plist.
 * @return Name of the value being monitored.
 */
QString PlistFile::valueName() const {
    return m_valueName;
}

/**
 * @brief Return the last-known in-memory value.
 * @return Cached value.
 */
QString PlistFile::value() const {
    return m_value;
}

/**
 * @brief Read the current on-disk plist value.
 *
 * - Expands "~" to home path.
 * - Warns if the file or key is missing.
 *
 * @return The value as a QString, or empty if missing/invalid.
 */
QString PlistFile::getCurrentValue() const {
    QString expandedPath = m_plistPath;
    if (expandedPath.startsWith("~")) {
        expandedPath.replace(0, 1, QDir::homePath());
    }

    if (!QFile::exists(expandedPath)) {
        qWarning() << "[PLISTFILE] File does not exist:" << expandedPath;
        return QString();
    }

    QSettings settings(expandedPath, QSettings::NativeFormat);

    if (!settings.contains(m_valueName)) {
        qWarning() << "[PLISTFILE] Key not found in plist:" << m_valueName;
        return QString();
    }

    QVariant val = settings.value(m_valueName);
    if (!val.isValid()) {
        qWarning() << "[PLISTFILE] Invalid value retrieved for key:" << m_valueName;
        return QString();
    }

    return val.toString();
}

/**
 * @brief Return the previous cached value (before the last set).
 * @return Previous value.
 */
QString PlistFile::previousValue() const {
    return m_previousValue;
}

/**
 * @brief Cache a “previous” value for rollback baseline.
 * @param value Value to store as previous.
 */
void PlistFile::setPreviousValue(const QString &value) {
    m_previousValue = value;
}

/**
 * @brief Cache the pending new value before rollback.
 * @param value The new (possibly unauthorized) value.
 */
void PlistFile::setNewValue(const QString &value) {
    m_newValue = value;
}

/**
 * @brief Return the cached pending new value (for cancel).
 * @return New value.
 */
QString PlistFile::newValue() const {
    return m_newValue;
}

/**
 * @brief Update the in-memory and on-disk plist value.
 *
 * - Updates m_previousValue to the old m_value.
 * - Emits valueChanged() and logMessage().
 * - Writes via QSettings, syncs, and confirms the write succeeded.
 *
 * @param value The new value to set.
 */
void PlistFile::setValue(const QString &value) {
    if (m_value != value) {
        // Shift current to previous
        m_previousValue = m_value;
        m_value = value;

        // Notify UI/logging
        emit valueChanged();
        emit logMessage(QString("[LOG] Plist: %1, Prev: %2, New: %3")
                            .arg(m_valueName, m_previousValue, m_value));

        // Write to file
        m_settings->setValue(m_valueName, m_value);
        m_settings->sync();

        // Confirm the write
        QString confirmed = m_settings->value(m_valueName).toString();
        qDebug() << "[SET] Key:" << m_valueName
                 << "Confirmed after write:" << confirmed;
        if (confirmed != m_value) {
            qWarning() << "[WARNING] Mismatch after set for file:" << m_plistPath;
        }
    }
}

/**
 * @brief Internal helper to read the cached QSettings value.
 *
 * Similar to getCurrentValue(), but uses the existing m_settings pointer.
 *
 * @return The current value from QSettings, or empty if invalid.
 */
QString PlistFile::readCurrentValue() const {
    if (!m_settings) {
        qWarning() << "[PLISTFILE] QSettings not initialized for:" << m_valueName;
        return QString();
    }

    // We assume m_settings is already pointed at the correct file
    if (!m_settings->contains(m_valueName)) {
        qWarning() << "[PLISTFILE] Key not found in plist:" << m_valueName;
        return QString();
    }

    QVariant val = m_settings->value(m_valueName);
    if (!val.isValid()) {
        qWarning() << "[PLISTFILE] Invalid value retrieved for key:" << m_valueName;
        return QString();
    }

    return val.toString();
}
