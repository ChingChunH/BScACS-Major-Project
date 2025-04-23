/**
 * @file registryKeyModel.cpp
 * @brief Implements the QAbstractListModel for RegistryKey objects.
 *
 * Provides a list model wrapping a QList<RegistryKey*> so that
 * QML or Qt views can bind to registry key properties such as
 * name, critical status, and display text.
 */

#include "registryKeyModel.h"
#include <QDebug>

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Construct an empty RegistryKeyModel.
 * @param parent Optional parent QObject for ownership.
 *
 * Initializes the base QAbstractListModel without any items.
 */
RegistryKeyModel::RegistryKeyModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // No additional initialization required
}

////////////////////////////////////////////////////////////////////////////////
// Model Population
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Replace the model’s contents with a new list of RegistryKey pointers.
 * @param keys List of RegistryKey* to expose via this model
 *
 * Emits beginResetModel()/endResetModel() to notify any attached views
 * that the underlying data has changed completely.
 */
void RegistryKeyModel::setRegistryKeys(const QList<RegistryKey*> &keys) {
    beginResetModel();
    m_registryKeys = keys;
    endResetModel();
}

////////////////////////////////////////////////////////////////////////////////
// QAbstractListModel Overrides
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Return the number of rows (items) in the model.
 * @param parent Unused; must be invalid for a list model.
 * @return Number of RegistryKey objects or 0 if parent is valid.
 */
int RegistryKeyModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_registryKeys.size();
}

/**
 * @brief Retrieve data for a given row and role.
 * @param index Index identifying the row in the list.
 * @param role  Role enum specifying which data to return.
 * @return QVariant containing the requested data, or invalid QVariant if out of range.
 *
 * Roles supported:
 *  - NameRole         : returns key->name()
 *  - IsCriticalRole   : returns key->isCritical()
 *  - DisplayTextRole  : returns key->displayText()
 */
QVariant RegistryKeyModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) {
        return QVariant();
    }

    RegistryKey *key = m_registryKeys.at(index.row());

    switch (role) {
    case NameRole:
        return key->name();
    case IsCriticalRole:
        return key->isCritical();
    case DisplayTextRole:
        return key->displayText();
    default:
        return QVariant();
    }
}

/**
 * @brief Map custom role enums to role names for QML bindings.
 * @return Hash mapping role integers to byte-array names.
 *
 * These names appear in QML as properties on each model item.
 */
QHash<int, QByteArray> RegistryKeyModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[NameRole]         = "name";
    roles[IsCriticalRole]   = "isCritical";
    roles[DisplayTextRole]  = "displayText";
    return roles;
}
