#pragma once
#include <memory>
#include <string>
class ProjectService;
class ProjectStore;
class ProjectCodec {
  friend class ProjectService;
  friend class ProjectStore;
  static std::string encode(const ProjectService&);
  static std::unique_ptr<ProjectService> decode(const std::string&);
};
