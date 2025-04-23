#ifndef MACOSJSONUTILS_H 
#define MACOSJSONUTILS_H 

#include <QString> // Includes the QString class for handling string data
#include <QList> // Includes the QList class for managing lists of items
#include "plistFile.h" // Includes the RegistryKey class definition for use in this header

namespace MacOSJsonUtils { // Defines the JsonUtils namespace to organize related functions
QList<PlistFile*> readFilesFromJson(const QString &filePath); // Declares a function to read registry keys from a JSON file
}

#endif // MACOSJSONUTILS_H 
