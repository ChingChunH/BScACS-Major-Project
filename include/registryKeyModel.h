#ifndef REGISTRYKEYMODEL_H
#define REGISTRYKEYMODEL_H

#include <QAbstractListModel>
#include "registryKey.h"

/**
 * @brief List-model wrapper for displaying and interacting with RegistryKey objects.
 *
 * Exposes each RegistryKey’s name, critical flag, and formatted display text
 * as roles consumable by QML or other Qt view layers.
 */
class RegistryKeyModel : public QAbstractListModel {
    Q_OBJECT

public:
    /**
     * @brief Construct an empty RegistryKeyModel.
     * @param parent Optional QObject parent for ownership.
     */
    explicit RegistryKeyModel(QObject *parent = nullptr);

    /**
     * @brief Roles exposed to views for each RegistryKey item.
     */
    enum RegistryKeyRoles {
        NameRole         = Qt::UserRole + 1,  ///< The key’s full path/name
        IsCriticalRole,                      ///< Whether this entry is marked critical
        DisplayTextRole                      ///< Combined hive/key/value text for UI
    };

    /**
     * @brief Number of rows (items) in the model.
     * @param parent Unused; included for interface compatibility.
     * @return Count of RegistryKey pointers currently set.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief Retrieve data for a given item and role.
     * @param index Index identifying the row.
     * @param role  Role indicating which data to return.
     * @return QVariant holding the requested data (e.g., QString or bool).
     */
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    /**
     * @brief Map role enums to role names for QML binding.
     * @return Hash mapping role integers to byte-array names.
     */
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief Replace the model’s entire list of RegistryKey items.
     * @param keys List of pointers to RegistryKey instances to display.
     *
     * Emits beginResetModel()/endResetModel() around the update.
     */
    void setRegistryKeys(const QList<RegistryKey*> &keys);

    /**
     * @brief Clear out all items and reset the model.
     *
     * Emits beginResetModel()/endResetModel() to notify any views.
     */
    void resetModel();

private:
    QList<RegistryKey*> m_registryKeys;  ///< Underlying list of monitored RegistryKey objects
};

#endif // REGISTRYKEYMODEL_H
