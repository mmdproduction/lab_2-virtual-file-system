#pragma once
#include <QUrl>
#include <functional>
#include "VirtualFileSystem/VirtualFileSystem.hpp"

class FileOpener {
public:
    using Launcher = std::function<bool(const QUrl&)>;
    static void open(const VirtualFileSystem& vfs, NodeId id, const User& user,
                     const Launcher& launcher = {});
};
