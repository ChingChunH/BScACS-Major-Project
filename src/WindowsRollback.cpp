/**
 * @file WindowsRollback.cpp
 * @brief Implements rollback operations for critical Windows registry keys.
 *
 * Detects unauthorized changes to RegistryKey objects marked as critical,
 * restores their last known-good values, and allows cancellation of rollbacks.
 */

#include "WindowsRollback.h"
#include "Database.h"
#include <QDebug>
#include <QDateTime>

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Construct a WindowsRollback instance.
 * @param parent Optional QObject parent for Qt ownership.
 */
WindowsRollback::WindowsRollback(QObject *parent)
    : QObject(parent)
{
    // No additional state to initialize
}

////////////////////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Check and perform rollback if the registry key has diverged from its previous value.
 * @param key Pointer to the RegistryKey to evaluate.
 *
 * - Only acts if the key is marked critical and its current on-disk value
 *   differs from the cached previousValue().
 * - Stores the “bad” current value as newValue() for potential cancel.
 * - Calls restorePreviousValue() to write the baseline value back.
 * - Updates the ConfigurationSettings table with the restored value.
 * - Emits rollbackPerformed() on success.
 */
void WindowsRollback::rollbackIfNeeded(RegistryKey* key) {
    if (!key) {
        qWarning() << "[ROLLBACK] Null key pointer provided; skipping.";
        return;
    }

    QString prevValue = key->previousValue();
    QString current   = key->getCurrentValue();

    if (key->isCritical() && current != prevValue) {
        qDebug() << "[ROLLBACK] Unauthorized change detected for key:" << key->name();

        // Cache the unauthorized value so it can be reapplied if cancelled
        key->setNewValue(current);

        // Restore the baseline value
        restorePreviousValue(key);

        // Persist the restored value in the database
        Database db;
        db.insertOrUpdateConfiguration(
            key->name(),
            key->keyPath(),
            prevValue,
            true
            );

        // Notify listeners that rollback occurred
        emit rollbackPerformed(key->name());
    } else {
        qDebug() << "[ROLLBACK] No rollback needed for key:" << key->name();
    }
}

/**
 * @brief Register a registry key for future rollback protection.
 * @param key Pointer to the RegistryKey to protect.
 *
 * Typically called when a key’s critical flag is enabled.
 * Currently logs registration; snapshotting of baseline value is handled elsewhere.
 */
void WindowsRollback::registerKeyForRollback(RegistryKey* key) {
    if (key && key->isCritical()) {
        qDebug() << "[ROLLBACK] Key registered for rollback protection:" << key->name();
    }
}

////////////////////////////////////////////////////////////////////////////////
// Cancel / Restore
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Cancel any pending rollback and reapply the stored newValue().
 * @param key Pointer to the RegistryKey to exempt.
 *
 * - Marks the key’s rollbackCancelled flag.
 * - If newValue() is set, writes it back to the registry via QSettings.
 * - Syncs and confirms that the on-disk value matches newValue().
 */
void WindowsRollback::cancelRollback(RegistryKey* key) {
    if (!key) {
        qWarning() << "[CANCEL ROLLBACK] Null key pointer provided; skipping.";
        return;
    }

    key->setRollbackCancelled(true);
    QString newVal = key->newValue();

    if (!newVal.isEmpty()) {
        // Reapply the stored new value
        key->setValue(newVal);
        key->settings()->sync();

        QString confirmed = key->getCurrentValue();
        if (confirmed == newVal) {
            qDebug() << "[CANCEL ROLLBACK] Successfully reapplied new value for key:"
                     << key->name() << "→" << confirmed;
        } else {
            qWarning() << "[CANCEL ROLLBACK] Reapply failed for key:" << key->name()
            << "Expected:" << newVal << "Found:" << confirmed;
        }
    } else {
        qDebug() << "[CANCEL ROLLBACK] No stored newValue for key:" << key->name()
        << "; nothing to reapply.";
    }
}

/**
 * @brief Restore the registry key’s on-disk value to its previousValue().
 * @param key Pointer to the RegistryKey to restore.
 *
 * - Writes previousValue() via QSettings.
 * - Calls sync() to commit the change.
 * - Reads back and logs whether the restoration succeeded.
 */
void WindowsRollback::restorePreviousValue(RegistryKey* key) {
    if (!key) {
        qWarning() << "[RESTORE] Null key pointer provided; skipping.";
        return;
    }

    QString prevValue = key->previousValue();
    QString current   = key->getCurrentValue();

    if (current == prevValue) {
        qDebug() << "[RESTORE] No action needed; key already at previous value:"
                 << key->name() << "→" << prevValue;
        return;
    }

    // Write the baseline value back to the registry
    key->settings()->setValue(key->valueName(), prevValue);
    key->settings()->sync();

    // Confirm the write
    QString confirmed = key->getCurrentValue();
    if (confirmed == prevValue) {
        qDebug() << "[ROLLBACK] Successfully restored key:" << key->name()
        << "to previous value:" << confirmed;
    } else {
        qWarning() << "[ROLLBACK] Failed to restore key:" << key->name()
        << "Expected:" << prevValue << "Found:" << confirmed;
    }
}
