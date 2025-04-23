#ifndef MACOSROLLBACK_H
#define MACOSROLLBACK_H

#include <QObject>
#include "plistFile.h"

/**
 * @brief Handles rollback operations for monitored macOS plist files.
 *
 * Registers files that require protection, detects unauthorized changes,
 * and restores the last known-good state when necessary.
 */
class MacOSRollback : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a MacOSRollback instance.
     * @param parent Optional parent QObject for ownership hierarchy.
     */
    explicit MacOSRollback(QObject *parent = nullptr);

    /**
     * @brief Mark a plist file as requiring rollback on change.
     * @param plist Pointer to the PlistFile to protect.
     *
     * Stores the current value so it can be restored if the file changes.
     */
    void plistFileForRollback(PlistFile* plist);

    /**
     * @brief Check and perform rollback on a plist file if its contents differ.
     * @param plist Pointer to the PlistFile to evaluate.
     *
     * Compares the file’s current state against the stored baseline and,
     * if they differ, restores the previous value.
     */
    void rollbackIfNeeded(PlistFile* plist);

    /**
     * @brief Cancel any pending rollback for a given plist file.
     * @param plist Pointer to the PlistFile for which to cancel rollback.
     *
     * Prevents the next detected change from triggering a restore.
     */
    void cancelRollback(PlistFile* plist);

signals:
    /**
     * @brief Emitted when a rollback operation has been completed.
     * @param fileName The path or identifier of the plist file rolled back.
     */
    void rollbackPerformed(const QString &fileName);

private:
    /**
     * @brief Restore the previous known-good contents of a plist file.
     * @param plist Pointer to the PlistFile whose value will be restored.
     */
    void restorePreviousValue(PlistFile* plist);
};

#endif // MACOSROLLBACK_H
