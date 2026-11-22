#pragma once
#include <QUrl>
#include <functional>

#include "Project/ProjectService.hpp"

class FileOpener {
 public:
  using Launcher = std::function<bool(const QUrl&)>;
  static void open(const ProjectService& vfs, NodeId id, UserId user,
                   const Launcher& launcher = {});
};
