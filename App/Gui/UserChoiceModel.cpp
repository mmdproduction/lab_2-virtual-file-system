#include "GuidQt.hpp"
#include "UserChoiceModel.hpp"

#include <limits>

UserChoiceModel::UserChoiceModel(ProjectService& users, QObject* parent)
    : QAbstractListModel(parent), users_(users) {
  reload();
}

int UserChoiceModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) return 0;
  const auto limit = static_cast<std::size_t>(std::numeric_limits<int>::max());
  return static_cast<int>(ids_.size() > limit ? limit : ids_.size());
}

UserId UserChoiceModel::id_at(int row) const {
  return row >= 0 && row < rowCount() ? ids_.get(static_cast<std::size_t>(row))
                                      : kInvalidUserId;
}

int UserChoiceModel::row_for_id(UserId id) const {
  for (int row = 0; row < rowCount(); ++row)
    if (id_at(row) == id) return row;
  return -1;
}

QVariant UserChoiceModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.model() != this || index.column() != 0 ||
      id_at(index.row()) == kInvalidUserId)
    return {};
  const User& user = users_.user(id_at(index.row()));
  if (role == Qt::UserRole) return QVariant::fromValue(user.id);
  if (role == Qt::ToolTipRole) return GuidText(user.id);
  if (role == Qt::DisplayRole)
    return QString("%1  ·  #%2")
        .arg(QString::fromStdString(user.name))
        .arg(GuidLabel(user.id));
  return {};
}

void UserChoiceModel::reload() {
  auto all = users_.user_ids();
  ArraySequence<UserId> ids;
  ids.append(users_.superadmin_id());
  for (std::size_t i = 0; i < all.size(); ++i)
    if (all.get(i) != users_.superadmin_id()) ids.append(all.get(i));
  beginResetModel();
  ids_.swap(ids);
  endResetModel();
}
