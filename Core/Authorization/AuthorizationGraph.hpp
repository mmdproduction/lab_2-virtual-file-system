#pragma once
class ProjectState;
class AuthorizationGraph {
 public:
  static void validate(const ProjectState&);
};
