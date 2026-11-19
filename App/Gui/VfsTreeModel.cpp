#include "VfsTreeModel.hpp"

#include <QDataStream>
#include <QFile>
#include <QIcon>
#include <QMimeData>
#include <QUuid>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

#include "Gui/UiTheme.hpp"

namespace {
constexpr auto kNodeMime = "application/x-vfs-node";
}

VfsTreeModel::VfsTreeModel(VirtualFileSystem& vfs, const User& user, QObject* parent)
    : QAbstractItemModel(parent), vfs_(vfs), user_(&user),
      drag_token_(QUuid::createUuid().toRfc4122()) {
    static_assert(sizeof(quintptr) >= sizeof(NodeId), "This model requires a 64-bit Qt build");
}

const FSNode* VfsTreeModel::find_node(NodeId id) const {
    try { return &vfs_.get_node(id, *user_); }
    catch (const std::exception&) { return nullptr; }
}

bool VfsTreeModel::is_visible(NodeId id) const {
    while (id != kInvalidNodeId) {
        const FSNode* node = find_node(id);
        if (!node) return false;
        if (id == vfs_.root_id()) return true;
        id = node->parent_id;
    }
    return false;
}

ArraySequence<NodeId> VfsTreeModel::visible_children(const FSNode& parent) const {
    ArraySequence<NodeId> result;
    if (parent.type != FSType::Folder) return result;
    for (std::size_t i = 0; i < parent.children.size(); ++i) {
        const NodeId id = parent.children.get(i);
        if (find_node(id)) result.append(id);
    }
    return result;
}

NodeId VfsTreeModel::node_id(const QModelIndex& index) const noexcept {
    return index.isValid() && index.model() == this
        ? static_cast<NodeId>(index.internalId()) : kInvalidNodeId;
}

const FSNode* VfsTreeModel::node_for_index(const QModelIndex& index) const {
    if (!index.isValid() || index.model() != this || index.column() < 0 || index.column() >= 3) return nullptr;
    return find_node(node_id(index));
}

int VfsTreeModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() && parent.model() != this ? 0 : 3;
}

int VfsTreeModel::rowCount(const QModelIndex& parent) const {
    if (!parent.isValid()) return find_node(vfs_.root_id()) ? 1 : 0;
    if (parent.column() != 0) return 0;
    const FSNode* node = node_for_index(parent);
    if (!node) return 0;
    const auto children = visible_children(*node);
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(children.size() > maximum ? maximum : children.size());
}

QModelIndex VfsTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent)) return {};
    NodeId id = vfs_.root_id();
    if (parent.isValid()) {
        const FSNode* node = node_for_index(parent);
        if (!node) return {};
        id = visible_children(*node).get(static_cast<std::size_t>(row));
    }
    return createIndex(row, column, static_cast<quintptr>(id));
}

QModelIndex VfsTreeModel::index_for_node(NodeId id) const {
    if (!is_visible(id)) return {};
    if (id == vfs_.root_id()) return createIndex(0, 0, static_cast<quintptr>(id));
    const FSNode* node = find_node(id);
    const FSNode* parent = find_node(node->parent_id);
    if (!parent) return {};
    const auto children = visible_children(*parent);
    for (std::size_t row = 0; row < children.size(); ++row) {
        if (children.get(row) == id) {
            if (row >= static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
            return createIndex(static_cast<int>(row), 0, static_cast<quintptr>(id));
        }
    }
    return {};
}

QModelIndex VfsTreeModel::parent(const QModelIndex& child) const {
    const FSNode* node = node_for_index(child);
    if (!node || node->parent_id == kInvalidNodeId) return {};
    return index_for_node(node->parent_id);
}

QVariant VfsTreeModel::data(const QModelIndex& index, int role) const {
    const FSNode* node = node_for_index(index);
    if (!node) return {};
    if (role == Qt::UserRole) return QVariant::fromValue<qulonglong>(node->id);
    if (role == Qt::UserRole + 1) return static_cast<int>(node->file_icon);
    if (role == Qt::ToolTipRole) return QString("ID: %1\n%2")
        .arg(static_cast<qulonglong>(node->id)).arg(QString::fromStdString(node->physical_path));
    if (role == Qt::DecorationRole && index.column() == 0) {
        static const QIcon folder = ui::LineIcon(ui::Icon::Folder);
        static const QIcon files[] = {
            ui::LineIcon(ui::Icon::File), ui::LineIcon(ui::Icon::Text),
            ui::LineIcon(ui::Icon::Image), ui::LineIcon(ui::Icon::Audio),
            ui::LineIcon(ui::Icon::Video), ui::LineIcon(ui::Icon::Code)
        };
        return node->type == FSType::Folder ? folder : files[static_cast<unsigned>(node->file_icon)];
    }
    if (role != Qt::DisplayRole) return {};
    switch (index.column()) {
        case 0: return QString::fromStdString(node->name);
        case 1: return node->type == FSType::Folder ? QString("Папка") : QString("Файл");
        case 2: return QString::fromStdString(node->physical_path);
        default: return {};
    }
}

QVariant VfsTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case 0: return QString("Имя");
        case 1: return QString("Тип");
        case 2: return QString("Физический путь");
        default: return {};
    }
}

Qt::ItemFlags VfsTreeModel::flags(const QModelIndex& index) const {
    const FSNode* node = node_for_index(index);
    if (!node) return Qt::NoItemFlags;
    auto result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (vfs_.get_access_rights(node->id, *user_).write) {
        if (node->type == FSType::Folder) result |= Qt::ItemIsDropEnabled;
        if (node->id != vfs_.root_id() && vfs_.get_access_rights(node->parent_id, *user_).write) {
            result |= Qt::ItemIsDragEnabled;
        }
    }
    return result;
}

NodeId VfsTreeModel::create_directory(NodeId parent_id, const std::string& name) {
    beginResetModel();
    NodeId id;
    try { id = vfs_.create_directory(parent_id, name, *user_); }
    catch (...) { endResetModel(); throw; }
    endResetModel();
    if (change_handler_) change_handler_();
    return id;
}

NodeId VfsTreeModel::attach_file(NodeId parent_id, const std::string& name, const QString& path) {
    beginResetModel();
    NodeId id;
    try { id = vfs_.attach_file(parent_id, name, path.toUtf8().toStdString(), *user_); }
    catch (...) { endResetModel(); throw; }
    endResetModel();
    if (change_handler_) change_handler_();
    return id;
}

NodeId VfsTreeModel::create_file(NodeId parent_id, const std::string& name, const QString& path) {
    if (!vfs_.get_access_rights(parent_id, *user_).write) {
        throw std::runtime_error("Write access required");
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        throw std::runtime_error(file.errorString().toStdString());
    }
    file.close();
    try { return attach_file(parent_id, name, path); }
    catch (...) {
        if (!file.remove()) {
            throw std::runtime_error(("Не удалось добавить файл в VFS и удалить созданный файл: " + path).toStdString());
        }
        throw;
    }
}

void VfsTreeModel::set_user(const User& user) {
    beginResetModel();
    user_ = &user;
    ++drag_epoch_;
    endResetModel();
}
void VfsTreeModel::refresh() { beginResetModel(); endResetModel(); }
void VfsTreeModel::set_change_handler(std::function<void()> handler) { change_handler_ = std::move(handler); }
void VfsTreeModel::set_error_handler(std::function<void(const QString&)> handler) { error_handler_ = std::move(handler); }
void VfsTreeModel::set_move_handler(std::function<void(NodeId)> handler) { move_handler_ = std::move(handler); }

QStringList VfsTreeModel::mimeTypes() const { return {QString::fromLatin1(kNodeMime)}; }
Qt::DropActions VfsTreeModel::supportedDragActions() const { return Qt::MoveAction; }
Qt::DropActions VfsTreeModel::supportedDropActions() const { return Qt::MoveAction; }

QMimeData* VfsTreeModel::mimeData(const QModelIndexList& indexes) const {
    NodeId id = kInvalidNodeId;
    for (const auto& index : indexes) {
        if (index.column() != 0) continue;
        if (!(flags(index) & Qt::ItemIsDragEnabled)) return nullptr;
        const NodeId next = node_id(index);
        if (id != kInvalidNodeId && id != next) return nullptr;
        id = next;
    }
    if (id == kInvalidNodeId) return nullptr;
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.writeRawData(drag_token_.constData(), 16);
    stream << drag_epoch_ << static_cast<quint64>(id);
    auto* result = new QMimeData;
    result->setData(kNodeMime, bytes);
    return result;
}

NodeId VfsTreeModel::dragged_node(const QMimeData* data) const {
    if (!data || !data->hasFormat(kNodeMime)) return kInvalidNodeId;
    const QByteArray bytes = data->data(kNodeMime);
    if (bytes.size() != 32) return kInvalidNodeId;
    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_6_0);
    char token[16];
    quint64 epoch = 0;
    quint64 id = 0;
    stream.readRawData(token, 16);
    stream >> epoch >> id;
    if (stream.status() != QDataStream::Ok || QByteArray(token,16) != drag_token_ || epoch != drag_epoch_) return kInvalidNodeId;
    return id;
}

bool VfsTreeModel::can_move(NodeId id, NodeId destination) const {
    if (id == kInvalidNodeId || id == vfs_.root_id() || !is_visible(id) || !is_visible(destination)) return false;
    const FSNode* node = find_node(id);
    const FSNode* target = find_node(destination);
    if (!target || target->type != FSType::Folder) return false;
    if (!vfs_.get_access_rights(id, *user_).write ||
        !vfs_.get_access_rights(node->parent_id, *user_).write ||
        !vfs_.get_access_rights(destination, *user_).write) return false;
    for (NodeId ancestor = destination; ancestor != kInvalidNodeId;) {
        if (ancestor == id) return false;
        const FSNode* parent = find_node(ancestor);
        if (!parent) return false;
        ancestor = parent->parent_id;
    }
    return true;
}

bool VfsTreeModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                    int column, const QModelIndex& parent) const {
    if (action != Qt::MoveAction || row < -1 || column < -1 || column >= 3) return false;
    try { return can_move(dragged_node(data), node_id(parent)); }
    catch (const std::exception&) { return false; }
}

bool VfsTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                 int column, const QModelIndex& parent) {
    if (action == Qt::IgnoreAction) return true;
    if (!canDropMimeData(data, action, row, column, parent)) return false;
    const NodeId id = dragged_node(data);
    const NodeId destination = node_id(parent);
    QString error;
    beginResetModel();
    try { vfs_.move(id, destination, *user_); }
    catch (const std::exception& e) { error = QString::fromUtf8(e.what()); }
    endResetModel();
    if (!error.isEmpty()) {
        if (error_handler_) error_handler_(error);
        return false;
    }
    if (move_handler_) move_handler_(id);
    if (change_handler_) change_handler_();
    return true;
}

void VfsTreeModel::rename(NodeId id, const std::string& name) {
    beginResetModel();
    try { vfs_.rename(id, name, *user_); }
    catch (...) { endResetModel(); throw; }
    endResetModel();
    if (change_handler_) change_handler_();
}
void VfsTreeModel::remove(NodeId id) {
    beginResetModel();
    try { vfs_.remove_recursive(id, *user_); }
    catch (...) { endResetModel(); throw; }
    endResetModel();
    if (change_handler_) change_handler_();
}
void VfsTreeModel::set_file_icon(NodeId id, FileIcon icon) {
    vfs_.set_file_icon(id, icon, *user_);
    const QModelIndex index = index_for_node(id);
    if (index.isValid()) emit dataChanged(index, index, {Qt::DecorationRole, Qt::UserRole + 1});
    if (change_handler_) change_handler_();
}
