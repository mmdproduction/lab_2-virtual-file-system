#pragma once

#include <QAbstractListModel>

#include "VirtualFileSystem/UserManager.hpp"

// A read-only account chooser, available to ordinary users as well.
class UserChoiceModel : public QAbstractListModel {
public:
    explicit UserChoiceModel(UserManager& users, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    void reload();
    UserId id_at(int row) const;
    int row_for_id(UserId id) const;
private:
    UserManager& users_;
    ArraySequence<UserId> ids_;
};
