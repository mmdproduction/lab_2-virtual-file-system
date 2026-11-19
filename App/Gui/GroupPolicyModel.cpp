#include "GroupPolicyModel.hpp"

#include <limits>
#include <stdexcept>

GroupPolicyModel::GroupPolicyModel(UserManager& users, VirtualFileSystem& vfs, NodeId node,
                                   UserId actor, QObject* parent)
    : QAbstractTableModel(parent), users_(users), vfs_(vfs), node_id_(node), actor_id_(actor) {
    const User& admin = users_.get_user(actor_id_);
    if (!admin.is_admin) throw std::runtime_error("Administrator access required");
    const AccessPolicy& policy = vfs_.get_node(node_id_, admin).access_policy;
    for (std::size_t i = 0; i < policy.rules.size(); ++i) {
        users_.get_group(policy.rules.get(i).group_id);
    }
    inherit_ = policy.inherit_from_parent;
    rules_ = policy.rules;
    reload_groups();
}

int GroupPolicyModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(rules_.size() > maximum ? maximum : rules_.size());
}

int GroupPolicyModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 3;
}

bool GroupPolicyModel::valid(const QModelIndex& index) const {
    return index.isValid() && index.model() == this && index.row() >= 0 &&
           index.row() < rowCount() && index.column() >= 0 && index.column() < 3;
}

QVariant GroupPolicyModel::data(const QModelIndex& index, int role) const {
    if (!valid(index)) return {};
    const GroupAccessRule& rule = rules_.get(static_cast<std::size_t>(index.row()));
    if (role == Qt::UserRole) return QVariant::fromValue<qulonglong>(rule.group_id);
    if (role == Qt::DisplayRole && index.column() == 0) {
        return QString("%1  ·  #%2").arg(QString::fromStdString(users_.get_group(rule.group_id).name))
            .arg(static_cast<qulonglong>(rule.group_id));
    }
    if (role == Qt::CheckStateRole && index.column() != 0) {
        const bool allowed = index.column() == 1 ? rule.rights.read : rule.rights.write;
        return allowed ? Qt::Checked : Qt::Unchecked;
    }
    return {};
}

QVariant GroupPolicyModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case 0: return QString("Группа");
        case 1: return QString("Чтение");
        case 2: return QString("Запись");
        default: return {};
    }
}

Qt::ItemFlags GroupPolicyModel::flags(const QModelIndex& index) const {
    if (!valid(index)) return Qt::NoItemFlags;
    auto result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() != 0) result |= Qt::ItemIsUserCheckable;
    return result;
}

bool GroupPolicyModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!valid(index) || index.column() == 0 || role != Qt::CheckStateRole ||
        (value.toInt() != Qt::Checked && value.toInt() != Qt::Unchecked)) return false;
    GroupAccessRule& rule = rules_.get(static_cast<std::size_t>(index.row()));
    bool& field = index.column() == 1 ? rule.rights.read : rule.rights.write;
    field = value.toInt() == Qt::Checked;
    emit dataChanged(index, index);
    return true;
}

void GroupPolicyModel::reload_groups() {
    const auto ids = users_.group_ids();
    ArraySequence<GroupAccessRule> next;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        GroupAccessRule rule;
        rule.group_id = ids.get(i);
        for (std::size_t j = 0; j < rules_.size(); ++j) {
            if (rules_.get(j).group_id == rule.group_id) {
                rule.rights = rules_.get(j).rights;
                break;
            }
        }
        next.append(rule);
    }
    beginResetModel();
    rules_.swap(next);
    endResetModel();
}

bool GroupPolicyModel::inherits() const noexcept { return inherit_; }

void GroupPolicyModel::apply(bool inherit_from_parent) {
    AccessPolicy policy;
    policy.inherit_from_parent = inherit_from_parent;
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        const GroupAccessRule& rule = rules_.get(i);
        users_.get_group(rule.group_id);
        if (rule.rights.read || rule.rights.write) policy.rules.append(rule);
    }
    vfs_.set_access_policy(node_id_, policy, users_.get_user(actor_id_));
    inherit_ = inherit_from_parent;
}
