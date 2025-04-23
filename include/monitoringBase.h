#ifndef MONITORINGBASE_H
#define MONITORINGBASE_H

#include <QObject>

/**
 * @brief Abstract base class for all monitoring modules.
 *
 * Provides a common logging signal and manages QObject parent/child lifetime.
 */
class MonitoringBase : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a MonitoringBase.
     * @param parent Optional parent QObject for ownership hierarchy.
     *
     * Initializes the QObject base with the given parent.
     */
    explicit MonitoringBase(QObject *parent = nullptr)
        : QObject(parent)
    {}

    /**
     * @brief Virtual destructor.
     *
     * Ensures proper cleanup in derived classes.
     */
    virtual ~MonitoringBase() = default;

signals:
    /**
     * @brief Emit informational or debug messages.
     * @param message The text of the log entry.
     *
     * Derived classes should emit this to report status or errors.
     */
    void logMessage(const QString &message);
};

#endif // MONITORINGBASE_H
