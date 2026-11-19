#include "ProjectStore.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <charconv>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "Dictionary/AVLDictionary.hpp"

namespace {
constexpr qsizetype kMaxProjectBytes = 64 * 1024 * 1024;

void Validate(const UserManager& users, const VirtualFileSystem& vfs) {
    const User& admin = users.get_user(users.admin_id());
    ArraySequence<NodeId> ids;
    ids.append(vfs.root_id());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const FSNode& node = vfs.get_node(ids.get(i), admin);
        for (std::size_t j = 0; j < node.access_policy.rules.size(); ++j) {
            users.get_group(node.access_policy.rules.get(j).group_id);
        }
        for (std::size_t j = 0; j < node.children.size(); ++j) ids.append(node.children.get(j));
    }
}

std::string Token(std::istream& input) {
    std::string token;
    if (!(input >> token)) throw std::runtime_error("Incomplete project file");
    return token;
}
void Expect(std::istream& input, const char* token) {
    if (Token(input) != token) throw std::runtime_error("Invalid project format");
}
std::string Block(std::istream& input, const char* label) {
    Expect(input, label);
    const auto token = Token(input);
    std::uint64_t count = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), count);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
        count > static_cast<std::uint64_t>(kMaxProjectBytes) || input.get() != '\n') {
        throw std::runtime_error("Invalid project block length");
    }
    std::string result(static_cast<std::size_t>(count), '\0');
    input.read(result.data(), static_cast<std::streamsize>(count));
    if (input.gcount() != static_cast<std::streamsize>(count)) throw std::runtime_error("Truncated project block");
    return result;
}
QString NormalPath(const QString& path) {
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}
void CheckOutputPath(const QString& path, const UserManager& users, const VirtualFileSystem& vfs) {
    const QString output = NormalPath(path);
    const User& admin = users.get_user(users.admin_id());
    ArraySequence<NodeId> ids; ids.append(vfs.root_id());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const FSNode& node = vfs.get_node(ids.get(i), admin);
        if (node.type == FSType::File && NormalPath(QString::fromStdString(node.physical_path)) == output) {
            throw std::runtime_error("Project output cannot overwrite an attached physical file");
        }
        for (std::size_t j = 0; j < node.children.size(); ++j) ids.append(node.children.get(j));
    }
}
}

QByteArray ProjectStore::encode(const UserManager& users, const VirtualFileSystem& vfs) {
    Validate(users, vfs);
    std::ostringstream registry, filesystem;
    users.save(registry, users.admin_id());
    vfs.save(filesystem, users.get_user(users.admin_id()));
    const auto user_text = registry.str();
    const auto vfs_text = filesystem.str();
    if (user_text.size() + vfs_text.size() > static_cast<std::size_t>(kMaxProjectBytes - 128)) {
        throw std::runtime_error("Project exceeds 64 MiB");
    }
    QByteArray bytes("VFS_PROJECT 1\nUSERS ");
    bytes += QByteArray::number(static_cast<qulonglong>(user_text.size())) + '\n';
    bytes += QByteArray::fromStdString(user_text);
    bytes += "VFS ";
    bytes += QByteArray::number(static_cast<qulonglong>(vfs_text.size())) + '\n';
    bytes += QByteArray::fromStdString(vfs_text);
    return bytes;
}

ProjectState ProjectStore::decode(const QByteArray& bytes) {
    if (bytes.size() > kMaxProjectBytes) throw std::runtime_error("Project exceeds 64 MiB");
    std::istringstream input(bytes.toStdString());
    input.imbue(std::locale::classic());
    Expect(input, "VFS_PROJECT");
    if (Token(input) != "1" || input.get() != '\n') throw std::runtime_error("Unsupported project version");
    std::istringstream registry(Block(input, "USERS"));
    std::istringstream filesystem(Block(input, "VFS"));
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("Trailing data in project file");
    auto users = UserManager::load(registry, std::make_unique<AvlDictionary<UserId, User>>(),
                                   std::make_unique<AvlDictionary<GroupId, Group>>());
    auto vfs = VirtualFileSystem::load(filesystem, std::make_unique<AvlDictionary<NodeId, FSNode>>(),
                                      std::make_unique<AvlDictionary<std::string, ArraySequence<NodeId>>>());
    Validate(users, vfs);
    return {std::move(users), std::move(vfs)};
}

void ProjectStore::save(const QString& path, const UserManager& users, const VirtualFileSystem& vfs) {
    if (path.isEmpty()) throw std::invalid_argument("Project path is empty");
    CheckOutputPath(path, users, vfs);
    const QByteArray bytes = encode(users, vfs);
    QSaveFile output(path);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) throw std::runtime_error(output.errorString().toStdString());
    if (output.write(bytes) != bytes.size() || !output.commit()) throw std::runtime_error(output.errorString().toStdString());
}

ProjectState ProjectStore::load(const QString& path) {
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error(input.errorString().toStdString());
    if (input.size() > kMaxProjectBytes) throw std::runtime_error("Project exceeds 64 MiB");
    const auto bytes = input.readAll();
    if (input.error() != QFileDevice::NoError) throw std::runtime_error(input.errorString().toStdString());
    return decode(bytes);
}
