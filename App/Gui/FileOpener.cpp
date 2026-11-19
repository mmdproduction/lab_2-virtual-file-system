#include "FileOpener.hpp"

#include <QDesktopServices>
#include <QFileInfo>
#include <stdexcept>

void FileOpener::open(const VirtualFileSystem& vfs, NodeId id, const User& user, const Launcher& launcher) {
    const FSNode& node = vfs.get_node(id, user);
    if (node.type != FSType::File) throw std::invalid_argument("Selected node is not a file");
    const QString path = QString::fromStdString(node.physical_path);
    if (!QFileInfo(path).isFile()) throw std::runtime_error(("Физический файл не найден: " + path).toStdString());
    const QUrl url = QUrl::fromLocalFile(path);
    if (!(launcher ? launcher(url) : QDesktopServices::openUrl(url))) {
        throw std::runtime_error("Не удалось передать файл системной программе для открытия");
    }
}
