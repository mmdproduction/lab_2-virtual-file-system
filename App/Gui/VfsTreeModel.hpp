#pragma once

#include <QAbstractItemModel>
#include <QByteArray>

#include <string>
#include <functional>

#include "VirtualFileSystem/VirtualFileSystem.hpp"

class VfsTreeModel : public QAbstractItemModel {
public:
    VfsTreeModel(VirtualFileSystem& vfs, const User& user,
                 QObject* parent = nullptr);

    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    NodeId node_id(const QModelIndex& index) const noexcept;
    QModelIndex index_for_node(NodeId id) const;
    NodeId create_directory(NodeId parent_id, const std::string& name);
    NodeId attach_file(NodeId parent_id, const std::string& name, const QString& path);
    NodeId create_file(NodeId parent_id, const std::string& name, const QString& path);
    void rename(NodeId id, const std::string& name);
    void remove(NodeId id);
    void set_file_icon(NodeId id, FileIcon icon);
    void set_user(const User& user);
    void refresh();
    void set_error_handler(std::function<void(const QString&)> handler);
    void set_change_handler(std::function<void()> handler);
    void set_move_handler(std::function<void(NodeId)> handler);
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                         int column, const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                      int column, const QModelIndex& parent) override;
    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;

private:
    const FSNode* find_node(NodeId id) const;
    const FSNode* node_for_index(const QModelIndex& index) const;

    bool is_visible(NodeId id) const;
    ArraySequence<NodeId> visible_children(const FSNode& parent) const;
    NodeId dragged_node(const QMimeData* data) const;
    bool can_move(NodeId id, NodeId destination) const;

    VirtualFileSystem& vfs_;
    const User* user_;
    QByteArray drag_token_;
    quint64 drag_epoch_ = 0;
    std::function<void(const QString&)> error_handler_;
    std::function<void(NodeId)> move_handler_;
    std::function<void()> change_handler_;
};
