#pragma once

#include <QAbstractTableModel>

#include "VirtualFileSystem/UserManager.hpp"
#include "VirtualFileSystem/VirtualFileSystem.hpp"

class GroupPolicyModel : public QAbstractTableModel {
public:
    GroupPolicyModel(UserManager& users, VirtualFileSystem& vfs, NodeId node_id,
                     UserId actor_id, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    void reload_groups();
    bool inherits() const noexcept;
    void apply(bool inherit_from_parent);

private:
    bool valid(const QModelIndex& index) const;
    UserManager& users_;
    VirtualFileSystem& vfs_;
    NodeId node_id_;
    UserId actor_id_;
    bool inherit_ = false;
    ArraySequence<GroupAccessRule> rules_;
};
