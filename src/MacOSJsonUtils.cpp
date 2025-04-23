#include "MacOSJsonUtils.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

/**
 * @file MacOSJsonUtils.cpp
 * @brief JSON utility functions for loading PlistFile objects on macOS.
 *
 * Provides functions to parse a JSON file describing plist entries
 * and produce a list of PlistFile instances for monitoring.
 */

namespace MacOSJsonUtils {

/**
 * @brief Read plist definitions from a JSON file and instantiate PlistFile objects.
 *
 * The JSON file is expected to contain a top-level array, where each element is an object
 * with the following fields:
 *   - "plistPath"   : QString, filesystem path to the plist file
 *   - "valueName"   : QString, the key within the plist to monitor
 *   - "isCritical"  : bool, whether changes to this plist are critical
 *
 * @param filePath Absolute or relative path to the JSON configuration file.
 * @return QList<PlistFile*> A list of newly allocated PlistFile pointers.
 *         Ownership is transferred to the caller.
 *         Returns an empty list on error (e.g., file cannot be opened or JSON is malformed).
 */
QList<PlistFile*> readFilesFromJson(const QString &filePath) {
    QList<PlistFile*> plistFiles;

    // Attempt to open the JSON file for reading
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[MacOSJsonUtils] Could not open JSON file:" << filePath;
        return plistFiles;
    }

    // Parse entire file content into a QJsonDocument
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    // Ensure the document contains a JSON array at the top level
    if (!doc.isArray()) {
        qWarning() << "[MacOSJsonUtils] Expected JSON array in file:" << filePath;
        return plistFiles;
    }
    QJsonArray filesArray = doc.array();

    // Iterate over each element in the array
    for (const QJsonValue &value : filesArray) {
        if (!value.isObject()) {
            qWarning() << "[MacOSJsonUtils] Skipping non-object entry in array";
            continue;
        }
        QJsonObject obj = value.toObject();

        // Extract required fields with default fallbacks
        QString plistPath  = obj.value("plistPath").toString();
        QString valueName  = obj.value("valueName").toString();
        bool    isCritical = obj.value("isCritical").toBool(false);

        // Validate that mandatory strings are present
        if (plistPath.isEmpty() || valueName.isEmpty()) {
            qWarning() << "[MacOSJsonUtils] Invalid entry (missing plistPath or valueName)";
            continue;
        }

        // Create a new PlistFile and append to the list
        plistFiles.append(new PlistFile(plistPath, valueName, isCritical));
    }

    qDebug() << "[MacOSJsonUtils] Loaded" << plistFiles.size() << "plist entries from JSON.";
    return plistFiles;
}

} // namespace MacOSJsonUtils
