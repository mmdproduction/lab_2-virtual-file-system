#include "GuidQt.hpp"
#include "FileSearchModel.hpp"

#include <QIcon>
#include <limits>
#include <stdexcept>

#include "Gui/UiTheme.hpp"

FileSearchModel::FileSearchModel(const ProjectService& vfs, UserId user,
                                 QObject* parent)
    : QAbstractTableModel(parent), vfs_(vfs), user_(user) {}

void FileSearchModel::search(const QString& name) {
  ArraySequence<Result> next;
  if (!name.isEmpty()) {
    const auto candidates = vfs_.find_files(name.toStdString(), user_);
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      const NodeId id = candidates.get(i);
      QString path = QString::fromStdString(vfs_.virtual_path(id, user_));
      if (next.size() >=
          static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("Too many search results");
      }
      const NodeInfo file = vfs_.node(id, user_);
      next.append(
          {id, QString::fromStdString(file.name), path, file.file_icon});
    }
  }
  beginResetModel();
  results_.swap(next);
  revision_ = vfs_.revision();
  endResetModel();
}

NodeId FileSearchModel::node_id(const QModelIndex& index) const noexcept {
  return revision_ == vfs_.revision() && index.isValid() &&
                 index.model() == this && index.row() >= 0 &&
                 static_cast<std::size_t>(index.row()) < results_.size() &&
                 index.column() < 2
             ? results_.get(static_cast<std::size_t>(index.row())).id
             : kInvalidNodeId;
}

int FileSearchModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() || revision_ != vfs_.revision()
             ? 0
             : static_cast<int>(results_.size());
}
int FileSearchModel::columnCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : 2;
}

QVariant FileSearchModel::data(const QModelIndex& index, int role) const {
  if (node_id(index) == kInvalidNodeId) return {};
  const auto& result = results_.get(static_cast<std::size_t>(index.row()));
  if (role == Qt::UserRole) return QVariant::fromValue(result.id);
  if (role == Qt::ToolTipRole) return result.path;
  if (role == Qt::DisplayRole)
    return index.column() == 0 ? result.name : result.path;
  if (role == Qt::DecorationRole && index.column() == 0) {
    static const QIcon icons[] = {
        ui::LineIcon(ui::Icon::File),  ui::LineIcon(ui::Icon::Text),
        ui::LineIcon(ui::Icon::Image), ui::LineIcon(ui::Icon::Audio),
        ui::LineIcon(ui::Icon::Video), ui::LineIcon(ui::Icon::Code)};
    return icons[static_cast<unsigned>(result.icon)];
  }
  return {};
}

QVariant FileSearchModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
  if (section == 0) return QString("Имя");
  if (section == 1) return QString("Виртуальный путь");
  return {};
}
