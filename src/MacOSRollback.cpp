#include "MacOSRollback.h"
#include "Database.h"
#include <QDebug>
#include <QDateTime>

/**
 * @file MacOSRollback.cpp
 * @brief Implementation of MacOSRollback: manages rollback of critical plist entries on macOS.
 *
 * Detects unauthorized changes to critical PlistFile objects and restores
 * their last known-good values. Also allows cancellation of a pending rollback.
 */

////////////////////////////////////////////////////////////////////////////////
// Construction / Registration
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Construct a MacOSRollback instance.
 * @param parent Optional parent QObject for ownership hierarchy.
 */
MacOSRollback::MacOSRollback(QObject *parent)
    : QObject(parent)
{
    // Nothing to initialize here beyond QObject parent.
}

////////////////////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Register a PlistFile for future rollback if it changes.
 * @param plist Pointer to the PlistFile marked as critical.
 *
 * This function is called when a file’s critical status is enabled.
 * Currently logs registration; actual snapshot of baseline is handled elsewhere.
 */
void MacOSRollback::plistFileForRollback(PlistFile* plist) {
    if (plist->isCritical()) {
        qDebug() << "[ROLLBACK] Registered for rollback:" << plist->plistPath();
    }
}

/**
 * @brief Cancel any pending rollback for the given PlistFile.
 * @param plist Pointer to the PlistFile to exempt.
 *
 * - Marks the PlistFile’s rollbackCancelled flag.
 * - If a “newValue” was stored, reapplies it to the plist.
 * - Syncs via QSettings and confirms the update succeeded.
 */
void MacOSRollback::cancelRollback(PlistFile* plist) {
    if (!plist) {
        qWarning() << "[CANCEL ROLLBACK] Null PlistFile pointer";
        return;
    }

    plist->setRollbackCancelled(true);

    QString newValue = plist->newValue();
    if (!newValue.isEmpty()) {
        // Apply stored new value
        plist->setValue(newValue);
        plist->settings()->sync();

        // Confirm the on-disk value matches
        QString confirmed = plist->getCurrentValue();
        if (confirmed == newValue) {
            qDebug() << "[CANCEL ROLLBACK] Successfully reapplied new value for:"
                     << plist->plistPath() << "→" << confirmed;
        } else {
            qWarning() << "[CANCEL ROLLBACK] Reapply failed for:"
                       << plist->plistPath()
                       << "expected:" << newValue
                       << "got:"      << confirmed;
        }
    } else {
        qDebug() << "[CANCEL ROLLBACK] No stored newValue; nothing to reapply for:"
                 << plist->plistPath();
    }
}

/**
 * @brief Perform rollback on a PlistFile if its current value differs from previousValue.
 * @param plist Pointer to the PlistFile to evaluate.
 *
 * - Checks that the file is marked critical.
 * - If the on-disk value ≠ previousValue, restores previousValue.
 * - Emits rollbackPerformed() after a successful restoration.
 * - Logs changes to ConfigurationSettings via Database.
 */
void MacOSRollback::rollbackIfNeeded(PlistFile* plist) {
    if (!plist) {
        qWarning() << "[ROLLBACK IF NEEDED] Null PlistFile pointer";
        return;
    }

    QString prevValue = plist->previousValue();
    QString current   = plist->getCurrentValue();

    // Only rollback critical entries with a divergence
    if (plist->isCritical() && current != prevValue) {
        qDebug() << "[ROLLBACK] Unauthorized change detected for:"
                 << plist->valueName();

        // Store the current (bad) value as “newValue” for potential cancel
        plist->setNewValue(current);

        // Restore the baseline previous value
        restorePreviousValue(plist);

        // Persist the restored state in the database
        Database db;
        db.insertOrUpdateConfiguration(
            plist->valueName(),
            plist->plistPath(),
            prevValue,
            true
            );

        // Notify listeners that rollback occurred
        emit rollbackPerformed(plist->valueName());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Internal Helpers
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Restore the PlistFile’s on-disk value back to previousValue.
 * @param plist Pointer to the PlistFile to restore.
 *
 * - Writes previousValue via QSettings.
 * - Syncs and confirms the on-disk value matches previousValue.
 */
void MacOSRollback::restorePreviousValue(PlistFile* plist) {
    QString prevValue = plist->previousValue();
    QString current   = plist->getCurrentValue();

    if (current == prevValue) {
        qDebug() << "[RESTORE] No change needed; already at previous value for:"
                 << plist->plistPath()
                 << "value:" << prevValue;
        return;
    }

    // Apply the baseline value
    plist->settings()->setValue(plist->valueName(), prevValue);
    plist->settings()->sync();

    // Confirm restoration
    QString confirmed = plist->getCurrentValue();
    if (confirmed == prevValue) {
        qDebug() << "[ROLLBACK] Successfully restored:"
                 << plist->plistPath()
                 << "to" << confirmed;
    } else {
        qWarning() << "[ROLLBACK FAILURE] Could not restore:"
                   << plist->plistPath()
                   << "expected:" << prevValue
                   << "got:"      << confirmed;
    }
}
