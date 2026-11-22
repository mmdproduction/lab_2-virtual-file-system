#pragma once
#include <QAbstractTableModel>
#include <QString>

#include "Project/ProjectService.hpp"

class FileSearchModel : public QAbstractTableModel {
 public:
  FileSearchModel(const ProjectService& vfs, UserId user,
                  QObject* parent = nullptr);
  void search(const QString& name);
  NodeId node_id(const QModelIndex& index) const noexcept;
  int rowCount(const QModelIndex& parent = {}) const override;
  int columnCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

 private:
  struct Result {
    NodeId id = kInvalidNodeId;
    QString name;
    QString path;
    FileIcon icon = FileIcon::Default;
  };
  const ProjectService& vfs_;
  UserId user_;
  ArraySequence<Result> results_;
  std::uint64_t revision_ = 0;
};
