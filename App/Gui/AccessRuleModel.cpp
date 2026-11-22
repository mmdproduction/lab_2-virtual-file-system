#include "AccessRuleModel.hpp"

#include "PermissionLabels.hpp"
AccessRuleModel::AccessRuleModel(ProjectService& p, NodeId n, UserId a,
                                 QObject* parent)
    : QAbstractTableModel(parent), project_(p), actor_(a), node_(n) {
  reload(n);
}
void AccessRuleModel::reload(NodeId n) {
  ArraySequence<RuleView> next;
  try {
    auto v = project_.policy_view(n, actor_);
    next.swap(v.rules);
  } catch (const AccessDenied&) {
  }
  beginResetModel();
  node_ = n;
  rows_.swap(next);
  endResetModel();
}
std::optional<RuleView> AccessRuleModel::row(int r) const {
  if (r < 0 || static_cast<std::size_t>(r) >= rows_.size()) return {};
  return rows_.get(static_cast<std::size_t>(r));
}
int AccessRuleModel::rowCount(const QModelIndex& p) const {
  return p.isValid() ? 0 : static_cast<int>(rows_.size());
}
int AccessRuleModel::columnCount(const QModelIndex& p) const {
  return p.isValid() ? 0 : 6;
}
QVariant AccessRuleModel::data(const QModelIndex& i, int role) const {
  if (!i.isValid() || i.model() != this || i.column() < 0 || i.column() >= 6)
    return {};
  auto v = row(i.row());
  if (!v) return {};
  const auto& r = v->rule;
  if (role == Qt::UserRole) return QVariant::fromValue<qulonglong>(r.id);
  if (role == Qt::ToolTipRole)
    return QString::fromStdString(v->reason) +
           QString(" · источник #%1").arg(r.source_grant_id);
  if (role != Qt::DisplayRole) return {};
  switch (i.column()) {
    case 0:
      return (r.principal.kind == PrincipalKind::User
                  ? QString("Пользователь: ")
                  : QString("Группа: ")) +
             QString::fromStdString(r.principal.kind == PrincipalKind::User
                                        ? project_.user(r.principal.id).name
                                        : project_.group(r.principal.id).name) +
             QString(" · #%1").arg(r.principal.id);
    case 1:
      return DecisionLabel(r.read) +
             (!v->allow_active && r.read == PermissionDecision::Allow
                  ? QString(" (не действует)") : QString());
    case 2:
      return DecisionLabel(r.write) +
             (!v->allow_active && r.write == PermissionDecision::Allow
                  ? QString(" (не действует)") : QString());
    case 3:
      return v->inherited ? QString("Унаследовано · #%1").arg(v->origin)
                          : QString("Здесь");
    case 4:
      return QString::fromStdString(project_.user(r.issuer_id).name);
    case 5:
      return v->editable ? QString("Можно изменить") : QString("Защищено");
    default:
      return {};
  }
}
QVariant AccessRuleModel::headerData(int c, Qt::Orientation o, int role) const {
  if (o != Qt::Horizontal || role != Qt::DisplayRole || c < 0 || c >= 6)
    return {};
  const char* names[] = {"Получатель", "Чтение", "Запись",
                         "Область",    "Автор",  "Изменение"};
  return QString::fromUtf8(names[c]);
}
