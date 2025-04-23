/**
 * @file WindowsJsonUtils.cpp
 * @brief JSON utility functions for loading RegistryKey objects on Windows.
 *
 * Provides functionality to parse a JSON file describing registry entries
 * and return a list of instantiated RegistryKey objects for monitoring.
 */

#include "WindowsJsonUtils.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

namespace WindowsJsonUtils {

/**
 * @brief Parse a JSON file and create RegistryKey instances.
 *
 * The JSON file must contain a top-level array, where each element is an object
 * with the following fields:
 *   - "hive"       : QString, e.g. "HKEY_CURRENT_USER"
 *   - "keyPath"    : QString, the path under the hive (e.g. "Software\\MyApp")
 *   - "valueName"  : QString, the registry value to monitor
 *   - "isCritical" : bool, whether changes to this key are critical
 *
 * @param filePath Path to the JSON configuration file.
 * @return QList<RegistryKey*> A list of new RegistryKey pointers.
 *         Ownership is transferred to the caller. Returns an empty list on error.
 */
QList<RegistryKey*> readKeysFromJson(const QString &filePath) {
    QList<RegistryKey*> registryKeys;

    // Attempt to open the JSON file for reading
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[WindowsJsonUtils] Could not open JSON file:" << filePath;
        return registryKeys;
    }

    // Parse the entire file into a QJsonDocument
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    // Ensure the document contains a JSON array at the top level
    if (!doc.isArray()) {
        qWarning() << "[WindowsJsonUtils] Expected top-level JSON array in file:" << filePath;
        return registryKeys;
    }
    QJsonArray keysArray = doc.array();

    // Iterate over each object in the array
    for (const QJsonValue &value : keysArray) {
        if (!value.isObject()) {
            qWarning() << "[WindowsJsonUtils] Skipping non-object entry in JSON array";
            continue;
        }
        QJsonObject obj = value.toObject();

        // Extract required fields with defaults
        QString hive       = obj.value("hive").toString();
        QString keyPath    = obj.value("keyPath").toString();
        QString valueName  = obj.value("valueName").toString();
        bool    isCritical = obj.value("isCritical").toBool(false);

        // Validate mandatory strings
        if (hive.isEmpty() || keyPath.isEmpty() || valueName.isEmpty()) {
            qWarning() << "[WindowsJsonUtils] Invalid entry (missing hive/keyPath/valueName)";
            continue;
        }

        // Instantiate a RegistryKey and append to the list
        registryKeys.append(new RegistryKey(hive, keyPath, valueName, isCritical));
    }

    qDebug() << "[WindowsJsonUtils] Loaded" << registryKeys.size()
             << "registry keys from JSON.";
    return registryKeys;
}

} // namespace WindowsJsonUtils
