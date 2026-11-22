#include "GuidQt.hpp"
#include "AdminGrantModel.hpp"

#include "PermissionLabels.hpp"
AdminGrantModel::AdminGrantModel(ProjectService& p, UserId a, QObject* parent)
    : QAbstractTableModel(parent), project_(p), actor_(a) {
  reload();
}
void AdminGrantModel::reload() {
  auto v = project_.admin_grants(actor_);
  beginResetModel();
  rows_.swap(v);
  endResetModel();
}
std::optional<AdminGrant> AdminGrantModel::row(int i) const {
  if (i < 0 || static_cast<std::size_t>(i) >= rows_.size()) return {};
  return rows_.get(static_cast<std::size_t>(i));
}
int AdminGrantModel::rowCount(const QModelIndex& p) const {
  return p.isValid() ? 0 : static_cast<int>(rows_.size());
}
int AdminGrantModel::columnCount(const QModelIndex& p) const {
  return p.isValid() ? 0 : 5;
}
QVariant AdminGrantModel::data(const QModelIndex& i, int role) const {
  if (!i.isValid() || i.model() != this || i.column() < 0 || i.column() >= 5)
    return {};
  auto g = row(i.row());
  if (!g) return {};
  if (role == Qt::UserRole) return QVariant::fromValue(g->id);
  if (role == Qt::ToolTipRole)
    return QString("Назначение: %1\nПолучатель: %2\nОбласть: %3\nАвтор: %4\nИсточник: %5")
        .arg(GuidText(g->id), GuidText(g->subject_id), GuidText(g->scope_node_id),
             GuidText(g->issuer_id), GuidText(g->parent_grant_id));
  if (role != Qt::DisplayRole) return {};
  switch (i.column()) {
    case 0:
      return QString::fromStdString(project_.user(g->subject_id).name) +
             QString(" · #%1").arg(GuidLabel(g->subject_id));
    case 1:
      try {
        return QString::fromStdString(
            project_.administrative_path(g->scope_node_id, actor_));
      } catch (const std::exception&) {
        return QString("#%1").arg(GuidLabel(g->scope_node_id));
      }
    case 2:
      return CapabilityLabel(g->capabilities);
    case 3:
      return QString::fromStdString(project_.user(g->issuer_id).name);
    case 4:
      return g->parent_grant_id ? QString("#%1").arg(GuidLabel(g->parent_grant_id))
                                : QString("Суперадминистратор");
    default:
      return {};
  }
}
QVariant AdminGrantModel::headerData(int c, Qt::Orientation o, int role) const {
  if (o != Qt::Horizontal || role != Qt::DisplayRole || c < 0 || c >= 5)
    return {};
  const char* names[] = {"Администратор", "Область", "Полномочия", "Выдал",
                         "Источник"};
  return QString::fromUtf8(names[c]);
}
