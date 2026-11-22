#pragma once
#include <QByteArray>
#include <QString>
#include <functional>

#include "Project/ProjectService.hpp"
class ProjectStore {
  friend class MainWindow;
  static std::function<void(const QString&)> autosave_writer(
      std::function<const ProjectService&()> provider);
  static void write(const QString&, const ProjectService&);

 public:
  static QByteArray encode(const ProjectService&, UserId actor);
  static std::unique_ptr<ProjectService> decode(const QByteArray&,
                                                const ProjectService& current,
                                                UserId actor);
  static void save(const QString&, const ProjectService&, UserId actor);
  static std::unique_ptr<ProjectService> load(const QString&,
                                              const ProjectService& current,
                                              UserId actor);
};
