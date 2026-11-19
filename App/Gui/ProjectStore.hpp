#pragma once

#include <QByteArray>
#include <QString>

#include "VirtualFileSystem/UserManager.hpp"
#include "VirtualFileSystem/VirtualFileSystem.hpp"

struct ProjectState {
    UserManager users;
    VirtualFileSystem vfs;
};

class ProjectStore {
public:
    static QByteArray encode(const UserManager& users, const VirtualFileSystem& vfs);
    static ProjectState decode(const QByteArray& bytes);
    static void save(const QString& path, const UserManager& users, const VirtualFileSystem& vfs);
    static ProjectState load(const QString& path);
};
