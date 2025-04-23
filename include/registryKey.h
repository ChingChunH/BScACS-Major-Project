#ifndef REGISTRYKEY_H
#define REGISTRYKEY_H

#include <QString>
#include <QSettings>
#include <QObject>

/**
 * @brief Represents and monitors a Windows registry key/value pair.
 *
 * Wraps QSettings access to the registry, tracks value changes,
 * criticality, rollback cancellation, and counts modifications.
 */
class RegistryKey : public QObject {
    Q_OBJECT

    /// Combined hive, key path, name, and current/new value for display.
    Q_PROPERTY(QString displayText
                   READ displayText
                       NOTIFY displayTextChanged)

    /// Whether this registry entry is considered critical.
    Q_PROPERTY(bool isCritical
                   READ isCritical
                       WRITE setCritical
                           NOTIFY isCriticalChanged)

public:
    /**
     * @brief Construct a RegistryKey monitor.
     * @param hive           Registry hive identifier (e.g. "HKEY_CURRENT_USER").
     * @param keyPath        Path to the registry key.
     * @param valueName      Name of the registry value to monitor.
     * @param isCritical     If true, triggers alerts/rollback on change.
     * @param parent         Optional QObject parent.
     */
    explicit RegistryKey(const QString &hive,
                         const QString &keyPath,
                         const QString &valueName,
                         bool isCritical,
                         QObject *parent = nullptr);

    /// @return True if the key is marked critical.
    bool isCritical() const;

    /**
     * @brief Mark or unmark this key as critical.
     * @param critical New criticality state.
     */
    void setCritical(bool critical);

    /// @return Registry hive string.
    QString hive() const;

    /// @return Registry key path.
    QString keyPath() const;

    /// @return Monitored value name.
    QString valueName() const;

    /// @return In-memory current value.
    QString value() const;

    /**
     * @brief Update the in-memory stored value.
     * @param value New value to store.
     */
    void setValue(const QString &value);

    /// @return Previously recorded value before last change.
    QString previousValue() const;

    /**
     * @brief Record a baseline previous value for rollback.
     * @param value Value to store as previous.
     */
    void setPreviousValue(const QString &value);

    /// @return True if rollback has been cancelled for the next change.
    bool isRollbackCancelled() const;

    /**
     * @brief Cancel the next automatic rollback for this key.
     * @param cancelled If true, skips one rollback cycle.
     */
    void setRollbackCancelled(bool cancelled);

    /// @return Combined display text.
    QString displayText() const;

    /**
     * @brief Set a new pending value for comparison/display.
     * @param value New candidate value.
     */
    void setNewValue(const QString &value);

    /// @return The new pending value.
    QString newValue() const;

    /// @return Number of times this key changed.
    int changeCount() const;

    /// @brief Increment the internal change counter.
    void incrementChangeCount();

    /// @brief Reset the change counter to zero.
    void resetChangeCount();

    /// @return Pointer to QSettings used for registry access.
    QSettings* settings() const;

    /**
     * @brief Set a custom QSettings instance for I/O.
     * @param settings Configured QSettings pointer.
     */
    void setSettings(QSettings* settings);

signals:
    /// Emitted when displayText() is updated.
    void displayTextChanged();

    /// Emitted when isCritical() changes.
    void isCriticalChanged();

private:
    /**
     * @brief Update the cached display text and emit change signal.
     */
    void updateDisplayText();

    /**
     * @brief Read and return the current registry value directly.
     * @return Latest value from the registry.
     */
    QString readCurrentValue() const;

    QString    m_hive;               ///< Hive identifier
    QString    m_keyPath;            ///< Key path within the hive
    QString    m_valueName;          ///< Registry value name
    bool       m_isCritical;         ///< Criticality flag
    QString    m_value;              ///< Stored current value
    QString    m_previousValue;      ///< Baseline value for rollback
    QString    m_displayText;        ///< Aggregated text for UI
    QString    m_newValue;           ///< Pending new value
    int        m_changeCount = 0;    ///< Modification counter
    bool       m_rollbackCancelled = false; ///< Skip next rollback if true
    QSettings* m_settings = nullptr; ///< QSettings for registry I/O
};

#endif // REGISTRYKEY_H
