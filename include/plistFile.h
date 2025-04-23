#ifndef PLISTFILE_H
#define PLISTFILE_H

#include <QString>
#include <QSettings>
#include <QObject>

/**
 * @brief Represents a single plist file entry to monitor.
 *
 * Wraps access to a macOS plist key/value pair, tracks its state,
 * handles criticality, rollback cancellation, and change counting.
 */
class PlistFile : public QObject {
    Q_OBJECT

    /// Text combining path, key name, and current/new value for display.
    Q_PROPERTY(QString displayText
                   READ displayText
                       NOTIFY displayTextChanged)

    /// Whether changes to this plist entry are considered critical.
    Q_PROPERTY(bool isCritical
                   READ isCritical
                       WRITE setCritical
                           NOTIFY isCriticalChanged)

public:
    /**
     * @brief Construct a PlistFile monitor instance.
     * @param plistPath      Filesystem path to the plist file.
     * @param valueName      The specific key within the plist to watch.
     * @param isCritical     If true, any change triggers alerts/rollback.
     * @param parent         Optional parent QObject.
     */
    explicit PlistFile(const QString &plistPath,
                       const QString &valueName,
                       bool isCritical,
                       QObject *parent = nullptr);

    /// @return The filesystem path of the monitored plist.
    QString plistPath() const;

    /// @return The key name inside the plist being tracked.
    QString valueName() const;

    /// @return The currently stored value for this plist entry.
    QString value() const;

    /**
     * @brief Update the stored value in memory.
     * @param value New value to record.
     */
    void setValue(const QString &value);

    /// @return Fresh value read from disk via QSettings.
    QString getValue();

    /// @return True if this entry is marked critical.
    bool isCritical() const;

    /**
     * @brief Mark or unmark this entry as critical.
     * @param critical New criticality state.
     */
    void setCritical(bool critical);

    /// @return Previously recorded value before the last change.
    QString previousValue() const;

    /**
     * @brief Store a “previous” value for potential rollback.
     * @param value Value to record as the baseline.
     */
    void setPreviousValue(const QString &value);

    /// @return True if rollback has been cancelled for the next change.
    bool isRollbackCancelled() const;

    /**
     * @brief Cancel the next rollback for this entry.
     * @param cancelled If true, skip one rollback cycle.
     */
    void setRollbackCancelled(bool cancelled);

    /// Recompute the displayText (path + key + values) and emit change.
    void updateDisplayText();

    /// @return The current aggregated display text.
    QString displayText() const;

    /// @return The current value read directly from disk.
    QString readCurrentValue() const;

    /// @return The new pending value set programmatically.
    QString newValue() const;

    /**
     * @brief Set a new pending value that differs from the disk value.
     * @param value Proposed new value for display and change tracking.
     */
    void setNewValue(const QString &value);

    /// @return Pointer to internal QSettings used for plist I/O.
    QSettings* settings() const { return m_settings; }

    /**
     * @brief Replace the QSettings instance used for file I/O.
     * @param settings Pointer to a QSettings configured for this plist.
     */
    void setSettings(QSettings* settings);

    /// @return Number of times this entry has changed since last reset.
    int changeCount() const { return m_changeCount; }

    /// Increment the internal change counter.
    void incrementChangeCount() { m_changeCount++; }

    /// Reset the internal change counter to zero.
    void resetChangeCount() { m_changeCount = 0; }

signals:
    /// Emitted when displayText() has been updated.
    void displayTextChanged();

    /// Emitted when isCritical() state changes.
    void isCriticalChanged();

    /// Emitted when the stored value changes via setValue().
    void valueChanged();

    /// Generic log message signal for UI or file logging.
    void logMessage(const QString &message);

    /// Emitted when rollbackCancelled state changes.
    void rollbackCancelledChanged();

    /// Emitted when previousValue() is updated.
    void previousValueChanged();

private:
    QString     m_plistPath;          ///< Full path to the plist file
    QString     m_valueName;          ///< The specific key within the plist
    QString     m_value;              ///< Last known value in memory
    QString     m_previousValue;      ///< Baseline value for rollback
    bool        m_isCritical;         ///< Whether changes are critical
    QString     m_displayText;        ///< Combined text for UI display
    bool        m_rollbackCancelled = false;  ///< Skip next rollback if true

    QSettings  *m_settings = nullptr; ///< QSettings for reading/writing plist
    QString     m_newValue;           ///< Pending new value to compare
    int         m_changeCount = 0;    ///< Number of times value has changed
};

#endif // PLISTFILE_H
