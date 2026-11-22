#pragma once
#include "GuidQt.hpp"
#include "Dictionary/AVLDictionary.hpp"

#include <QAbstractItemModel>
#include <QByteArray>
#include <functional>
#include <optional>
#include <string>

#include "Project/ProjectService.hpp"

class VfsTreeModel : public QAbstractItemModel {
 public:
  VfsTreeModel(ProjectService& vfs, UserId user, QObject* parent = nullptr);

  QModelIndex index(int row, int column,
                    const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  NodeId node_id(const QModelIndex& index) const noexcept;
  QModelIndex index_for_node(NodeId id) const;
  NodeId create_directory(NodeId parent_id, const std::string& name);
  NodeId attach_file(NodeId parent_id, const std::string& name,
                     const QString& path);
  NodeId create_file(NodeId parent_id, const std::string& name,
                     const QString& path);
  void rename(NodeId id, const std::string& name);
  void remove(NodeId id);
  void set_file_icon(NodeId id, FileIcon icon);
  void set_user(UserId user);
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
  std::optional<NodeInfo> find_node(NodeId id) const;
  std::optional<NodeInfo> node_for_index(const QModelIndex& index) const;

  bool is_visible(NodeId id) const;
  ArraySequence<NodeId> visible_children(const NodeInfo& parent) const;
  quintptr handle_for(NodeId) const;
  void clear_handles();
  mutable ArraySequence<NodeId> index_ids_;
  mutable AvlDictionary<NodeId, quintptr> index_handles_;
  NodeId dragged_node(const QMimeData* data) const;
  bool can_move(NodeId id, NodeId destination) const;

  ProjectService& vfs_;
  UserId user_;
  QByteArray drag_token_;
  quint64 drag_epoch_ = 0;
  std::function<void(const QString&)> error_handler_;
  std::function<void(NodeId)> move_handler_;
  std::function<void()> change_handler_;
};
