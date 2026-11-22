#include "AccountListModel.hpp"

#include <QSize>
#include <limits>
#include <stdexcept>
#include <utility>

#include "Gui/UiTheme.hpp"

AccountListModel::AccountListModel(ProjectService& users, UserId actor,
                                   Kind kind, QObject* parent)
    : QAbstractListModel(parent), users_(users), actor_id_(actor), kind_(kind) {
  users_.require_superadmin(actor_id_);
  reload();
}

bool AccountListModel::valid(const QModelIndex& index) const {
  return index.isValid() && index.model() == this && index.column() == 0 &&
         index.row() >= 0 && index.row() < rowCount();
}

int AccountListModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) return 0;
  const auto maximum =
      static_cast<std::size_t>(std::numeric_limits<int>::max());
  return static_cast<int>(ids_.size() > maximum ? maximum : ids_.size());
}

std::uint64_t AccountListModel::id_at(const QModelIndex& index) const {
  return valid(index) ? ids_.get(static_cast<std::size_t>(index.row())) : 0;
}

QModelIndex AccountListModel::index_for_id(std::uint64_t id) const {
  for (int row = 0; row < rowCount(); ++row) {
    if (ids_.get(static_cast<std::size_t>(row)) == id) return index(row, 0);
  }
  return {};
}

QVariant AccountListModel::data(const QModelIndex& index, int role) const {
  if (!valid(index)) return {};
  const auto id = id_at(index);
  if (role == Qt::UserRole) return QVariant::fromValue<qulonglong>(id);
  if (role == Qt::SizeHintRole) return QSize(180, 46);
  if (role == Qt::DecorationRole) {
    static const QIcon user = ui::LineIcon(ui::Icon::User);
    static const QIcon group = ui::LineIcon(ui::Icon::Group);
    return kind_ == Kind::Users ? user : group;
  }
  if (role == Qt::DisplayRole) {
    const std::string name =
        kind_ == Kind::Users ? users_.user(id).name : users_.group(id).name;
    return QString("%1  ·  #%2")
        .arg(QString::fromStdString(name))
        .arg(static_cast<qulonglong>(id));
  }
  if (role == Qt::CheckStateRole && kind_ == Kind::Groups &&
      selected_user_ != 0) {
    const User& user = users_.user(selected_user_);
    for (std::size_t i = 0; i < user.group_ids.size(); ++i) {
      if (user.group_ids.get(i) == id) return Qt::Checked;
    }
    return Qt::Unchecked;
  }
  return {};
}

Qt::ItemFlags AccountListModel::flags(const QModelIndex& index) const {
  if (!valid(index)) return Qt::NoItemFlags;
  auto result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  if (kind_ == Kind::Groups && selected_user_ != 0)
    result |= Qt::ItemIsUserCheckable;
  return result;
}

bool AccountListModel::setData(const QModelIndex& index, const QVariant& value,
                               int role) {
  if (!valid(index) || kind_ != Kind::Groups || selected_user_ == 0 ||
      role != Qt::CheckStateRole ||
      (value.toInt() != Qt::Checked && value.toInt() != Qt::Unchecked))
    return false;
  const auto before = users_.revision();
  try {
    if (value.toInt() == Qt::Checked) {
      users_.set_membership(selected_user_, id_at(index), true, actor_id_);
    } else {
      users_.set_membership(selected_user_, id_at(index), false, actor_id_);
    }
  } catch (const std::exception& error) {
    if (error_handler_) error_handler_(QString::fromUtf8(error.what()));
    return false;
  }
  emit dataChanged(index, index);
  if (change_handler_ && before != users_.revision()) change_handler_();
  return true;
}

void AccountListModel::reload() {
  auto next = kind_ == Kind::Users ? users_.user_ids() : users_.group_ids();
  beginResetModel();
  ids_.swap(next);
  endResetModel();
}

void AccountListModel::set_selected_user(UserId id) {
  if (id != kInvalidUserId) users_.user(id);
  selected_user_ = id;
  if (rowCount() > 0) emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
}

void AccountListModel::set_error_handler(
    std::function<void(const QString&)> handler) {
  error_handler_ = std::move(handler);
}

void AccountListModel::set_change_handler(std::function<void()> handler) {
  change_handler_ = std::move(handler);
}
