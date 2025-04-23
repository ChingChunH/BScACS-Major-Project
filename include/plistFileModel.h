#ifndef PLISTFILEMODEL_H
#define PLISTFILEMODEL_H

#include <QAbstractListModel>
#include "plistFile.h"

/**
 * @brief List-model wrapper for displaying and interacting with PlistFile objects.
 *
 * Exposes each PlistFile’s key name, critical flag, and formatted display text
 * as roles consumable by QML or other Qt view layers.
 */
class PlistFileModel : public QAbstractListModel {
    Q_OBJECT

public:
    /**
     * @brief Construct an empty PlistFileModel.
     * @param parent Optional QObject parent for ownership.
     */
    explicit PlistFileModel(QObject *parent = nullptr);

    /**
     * @brief Roles exposed to views for each PlistFile item.
     */
    enum PlistFileRoles {
        ValueNameRole    = Qt::UserRole + 1,  ///< The key name inside the plist
        IsCriticalRole,                      ///< Whether this entry is marked critical
        DisplayTextRole                      ///< Combined path/key/value text for UI
    };

    /**
     * @brief Number of rows (items) in the model.
     * @param parent Unused; included for interface compatibility.
     * @return Count of PlistFile pointers currently set.
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
     * @brief Replace the model’s entire list of PlistFile items.
     * @param files List of pointers to PlistFile instances to display.
     *
     * Emits beginResetModel()/endResetModel() around the update.
     */
    void setPlistFiles(const QList<PlistFile*> &files);

    /**
     * @brief Clear out all items and reset the model.
     *
     * Emits beginResetModel()/endResetModel() to notify any views.
     */
    void resetModel();

private:
    QList<PlistFile*> m_plistFiles;  ///< Underlying list of monitored PlistFile objects
};

#endif // PLISTFILEMODEL_H
