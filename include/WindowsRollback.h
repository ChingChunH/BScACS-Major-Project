#ifndef WINDOWSROLLBACK_H
#define WINDOWSROLLBACK_H

#include <QObject>
#include "registryKey.h"

/**
 * @brief Handles rollback operations for monitored Windows registry keys.
 *
 * Registers critical RegistryKey objects, detects unauthorized changes,
 * and restores the last known-good value when necessary.
 */
class WindowsRollback : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a WindowsRollback instance.
     * @param parent Optional parent QObject for ownership hierarchy.
     */
    explicit WindowsRollback(QObject *parent = nullptr);

    /**
     * @brief Register a RegistryKey for rollback protection.
     * @param key Pointer to the RegistryKey to protect.
     *
     * Stores the current value so it can be restored if the key changes.
     */
    void registerKeyForRollback(RegistryKey* key);

    /**
     * @brief Check and perform rollback if the registry key's value differs.
     * @param key Pointer to the RegistryKey to evaluate.
     *
     * Compares the key’s current state against the stored baseline and,
     * if they differ, restores the previous value.
     */
    void rollbackIfNeeded(RegistryKey* key);

    /**
     * @brief Cancel any pending rollback for the specified key.
     * @param key Pointer to the RegistryKey for which to cancel rollback.
     *
     * Prevents the next detected change from triggering a restore.
     */
    void cancelRollback(RegistryKey* key);

signals:
    /**
     * @brief Emitted when a rollback operation has been executed.
     * @param keyName The path or identifier of the registry key rolled back.
     */
    void rollbackPerformed(const QString &keyName);

private:
    /**
     * @brief Restore the previous known-good value of a registry key.
     * @param key Pointer to the RegistryKey whose value will be restored.
     */
    void restorePreviousValue(RegistryKey* key);
};

#endif // WINDOWSROLLBACK_H
