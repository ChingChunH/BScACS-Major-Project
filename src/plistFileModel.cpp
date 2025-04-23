#include "plistFileModel.h"
#include <QDebug>

/**
 * @file PlistFileModel.cpp
 * @brief Implements a Qt list model for presenting PlistFile objects in QML/Views.
 */

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Construct an empty PlistFileModel.
 * @param parent Optional QObject parent for ownership.
 */
PlistFileModel::PlistFileModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Nothing else to initialize here
}

////////////////////////////////////////////////////////////////////////////////
// Model Population
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Replace the model’s contents with a new list of PlistFile pointers.
 * @param files List of PlistFile* to expose via this model.
 *
 * Emits beginResetModel()/endResetModel() to notify attached views.
 */
void PlistFileModel::setPlistFiles(const QList<PlistFile*> &files) {
    beginResetModel();
    m_plistFiles = files;
    endResetModel();
}

////////////////////////////////////////////////////////////////////////////////
// QAbstractListModel Overrides
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Return the number of rows in the model.
 * @param parent Unused; included for interface compatibility.
 * @return The number of PlistFile items when parent is invalid, else 0.
 */
int PlistFileModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_plistFiles.size();
}

/**
 * @brief Retrieve data for a given row and role.
 * @param index Index identifying the row in the list.
 * @param role  Role specifying which data to return.
 * @return A QVariant containing the requested data, or invalid if out-of-bounds.
 */
QVariant PlistFileModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) {
        return QVariant();
    }

    PlistFile *file = m_plistFiles.at(index.row());

    switch (role) {
    case ValueNameRole:
        // Return the key name inside the plist
        return file->valueName();
    case IsCriticalRole:
        // Return whether this entry is marked critical
        return file->isCritical();
    case DisplayTextRole:
        // Return the formatted display text (e.g., "KeyName - Critical")
        return file->displayText();
    default:
        return QVariant();
    }
}

/**
 * @brief Provide the mapping from custom role enums to role names.
 * @return Hash mapping role integer values to their corresponding names.
 *
 * These names are used for property bindings in QML (e.g., model.displayText).
 */
QHash<int, QByteArray> PlistFileModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[ValueNameRole]   = "valueName";
    roles[IsCriticalRole]  = "isCritical";
    roles[DisplayTextRole] = "displayText";
    return roles;
}
