#pragma once

#include <QAbstractListModel>

#include <functional>

#include "VirtualFileSystem/UserManager.hpp"

class AccountListModel : public QAbstractListModel {
public:
    enum class Kind { Users, Groups };
    AccountListModel(UserManager& users, UserId actor_id, Kind kind, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    void reload();
    std::uint64_t id_at(const QModelIndex& index) const;
    QModelIndex index_for_id(std::uint64_t id) const;
    void set_selected_user(UserId id);
    void set_error_handler(std::function<void(const QString&)> handler);
    void set_change_handler(std::function<void()> handler);

private:
    bool valid(const QModelIndex& index) const;
    UserManager& users_;
    UserId actor_id_;
    Kind kind_;
    UserId selected_user_ = kInvalidUserId;
    ArraySequence<std::uint64_t> ids_;
    std::function<void(const QString&)> error_handler_;
    std::function<void()> change_handler_;
};
