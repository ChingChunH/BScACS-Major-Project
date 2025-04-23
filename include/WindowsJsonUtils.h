#ifndef WINDOWSJSONUTILS_H
#define WINDOWSJSONUTILS_H

#include <QString>   ///< QString class for handling string data
#include <QList>     ///< QList class for managing lists of items
#include "registryKey.h" ///< RegistryKey class definition for use in this header

/**
 * @brief Utility namespace for JSON-based registry key I/O on Windows.
 *
 * Provides functions to load and save RegistryKey configurations
 * from JSON files, enabling batch setup of monitoring parameters.
 */
namespace WindowsJsonUtils {

/**
     * @brief Parse a JSON file to create RegistryKey instances.
     * @param filePath Path to the JSON file containing registry key definitions.
     * @return QList of pointers to newly created RegistryKey objects.
     *
     * The JSON should be an array of objects, each with fields:
     *   - "hive" (string): Registry hive (e.g. "HKEY_CURRENT_USER")
     *   - "keyPath" (string): Path to the registry key
     *   - "valueName" (string): Name of the registry value
     *   - "isCritical" (bool): Whether changes to this key are critical
     *
     * Ownership of the returned RegistryKey pointers is transferred to the caller.
     */
QList<RegistryKey*> readKeysFromJson(const QString &filePath);

} // namespace WindowsJsonUtils

#endif // WINDOWSJSONUTILS_H
