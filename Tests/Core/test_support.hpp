#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "Dictionary/AVLDictionary.hpp"
#include "VirtualFileSystem/UserManager.hpp"
#include "VirtualFileSystem/VirtualFileSystem.hpp"

namespace core_test {

inline auto MakeNodes() {
  return std::make_unique<AvlDictionary<NodeId, FSNode>>();
}
inline auto MakeIndex() {
  return std::make_unique<AvlDictionary<std::string, ArraySequence<NodeId>>>();
}
inline auto MakeUsers() {
  return std::make_unique<AvlDictionary<UserId, User>>();
}
inline auto MakeGroups() {
  return std::make_unique<AvlDictionary<GroupId, Group>>();
}
inline VirtualFileSystem MakeVfs() {
  return VirtualFileSystem(MakeNodes(), MakeIndex());
}
inline UserManager MakeUserManager() {
  return UserManager(MakeUsers(), MakeGroups());
}
inline User Admin() {
  User user;
  user.id = 100;
  user.is_admin = true;
  return user;
}
inline User Member(std::initializer_list<GroupId> groups = {10}) {
  User user;
  user.id = 200;
  user.name = "reader";
  for (GroupId group : groups) user.group_ids.append(group);
  return user;
}
inline AccessPolicy Policy(bool inherit,
                           std::initializer_list<GroupAccessRule> rules = {}) {
  AccessPolicy policy;
  policy.inherit_from_parent = inherit;
  for (const auto& rule : rules) policy.rules.append(rule);
  return policy;
}
template <typename T>
void ExpectSequence(const ArraySequence<T>& actual,
                    std::initializer_list<T> expected) {
  ASSERT_EQ(actual.size(), expected.size());
  std::size_t index = 0;
  for (const auto& value : expected) {
    EXPECT_EQ(actual.get(index), value) << "Index " << index;
    ++index;
  }
}
inline void ExpectRights(const AccessRights& rights, bool read, bool write) {
  EXPECT_EQ(rights.read, read);
  EXPECT_EQ(rights.write, write);
}

class TempDirectory {
 public:
  TempDirectory() {
    static std::atomic<unsigned long> counter{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
      path_ = std::filesystem::temp_directory_path() /
              ("vfs_core_gtest_" + std::to_string(stamp) + "_" +
               std::to_string(counter++));
      std::error_code error;
      if (std::filesystem::create_directory(path_, error)) return;
      if (error && error != std::errc::file_exists)
        throw std::runtime_error(error.message());
    }
    throw std::runtime_error("Cannot create unique test directory");
  }
  ~TempDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;
  const std::filesystem::path& path() const { return path_; }
  std::string WriteFile(
      const std::string& name,
      const std::string& content = "original contents") const {
    const auto result = path_ / name;
    std::ofstream out(result, std::ios::binary);
    out << content;
    out.close();
    if (!out) throw std::runtime_error("Cannot write test file");
    return std::filesystem::canonical(result).string();
  }

 private:
  std::filesystem::path path_;
};
inline std::string ReadFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot read test file");
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}
inline std::string SaveVfs(const VirtualFileSystem& vfs) {
  std::ostringstream output;
  vfs.save(output, Admin());
  return output.str();
}
inline VirtualFileSystem LoadVfs(const std::string& text) {
  std::istringstream input(text);
  return VirtualFileSystem::load(input, MakeNodes(), MakeIndex());
}
inline std::string SaveUsers(const UserManager& users) {
  std::ostringstream output;
  users.save(output, users.admin_id());
  return output.str();
}
inline UserManager LoadUsers(const std::string& text) {
  std::istringstream input(text);
  return UserManager::load(input, MakeUsers(), MakeGroups());
}
class VfsTest : public ::testing::Test {
 protected:
  TempDirectory temp;
  User admin = Admin();
  User reader = Member();
  VirtualFileSystem vfs = MakeVfs();
  NodeId root = vfs.root_id();
  std::string original;
  void SetUp() override { original = temp.WriteFile("physical.txt"); }
  NodeId File(NodeId parent, const std::string& name = "file.txt") {
    return vfs.attach_file(parent, name, original, admin);
  }
  void Grant(NodeId id, bool read = true, bool write = true,
             bool inherit = false) {
    vfs.set_access_policy(id, Policy(inherit, {{10, {read, write}}}), admin);
  }
};

// A real element that fails copying; avoids replacing the process-wide
// allocator.
struct CopyValue {
  inline static int alive = 0;
  inline static int assignments_left = -1;
  int value = 0;
  CopyValue() { ++alive; }
  CopyValue(const CopyValue& other) : value(other.value) { ++alive; }
  ~CopyValue() { --alive; }
  CopyValue& operator=(const CopyValue& other) {
    if (assignments_left == 0) throw std::runtime_error("Element copy failed");
    if (assignments_left > 0) --assignments_left;
    value = other.value;
    return *this;
  }
};
class FailCopies {
 public:
  explicit FailCopies(int count) : previous_(CopyValue::assignments_left) {
    CopyValue::assignments_left = count;
  }
  ~FailCopies() { CopyValue::assignments_left = previous_; }

 private:
  int previous_;
};
}  // namespace core_test
