#include "UserManager.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace {

std::string ReadToken(std::istream& input) {
    std::string token;
    if (!(input >> token)) {
        throw std::runtime_error("Unexpected end of user registry data");
    }
    return token;
}

void ExpectToken(std::istream& input, const char* expected) {
    if (ReadToken(input) != expected) {
        throw std::runtime_error("Unexpected token in user registry data");
    }
}

std::uint64_t ReadNumber(std::istream& input) {
    const std::string token = ReadToken(input);
    std::uint64_t value = 0;
    const char* end = token.data() + token.size();
    const auto result = std::from_chars(token.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::runtime_error("Expected an unsigned decimal number");
    }
    return value;
}

std::size_t ReadCount(std::istream& input) {
    const std::uint64_t value = ReadNumber(input);
    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Record count is too large");
    }
    return static_cast<std::size_t>(value);
}

std::string ReadName(std::istream& input) {
    input >> std::ws;
    if (input.peek() != '"') {
        throw std::runtime_error("Expected a quoted name");
    }
    std::string name;
    if (!(input >> std::quoted(name)) || name.empty()) {
        throw std::runtime_error("Invalid or incomplete name");
    }
    return name;
}

bool ReadFlag(std::istream& input) {
    const std::uint64_t flag = ReadNumber(input);
    if (flag > 1) {
        throw std::runtime_error("Administrator flag must be 0 or 1");
    }
    return flag == 1;
}

}  // namespace

UserManager UserManager::load(std::istream& input,
                               std::unique_ptr<UserDictionary> users,
                               std::unique_ptr<GroupDictionary> groups) {
    UserManager result(std::move(users), std::move(groups));
    if (!input) {
        throw std::runtime_error("Input stream is not readable");
    }

    // Read the same buffer using our own decimal/whitespace/locale settings.
    std::istream reader(input.rdbuf());
    reader.exceptions(std::ios::badbit);
    reader.imbue(std::locale::classic());

    ExpectToken(reader, "USER_MANAGER");
    if (ReadNumber(reader) != 1) {
        throw std::runtime_error("Unsupported user registry format version");
    }
    ExpectToken(reader, "ADMIN");
    const UserId saved_admin = ReadNumber(reader);
    if (saved_admin == kInvalidUserId) {
        throw std::runtime_error("Invalid administrator ID");
    }
    ExpectToken(reader, "NEXT_USER_ID");
    const UserId saved_next_user = ReadNumber(reader);
    ExpectToken(reader, "NEXT_GROUP_ID");
    const GroupId saved_next_group = ReadNumber(reader);

    // The ordinary constructor validates storage. Its bootstrap user is
    // replaced by records with the exact IDs from the snapshot.
    result.users_->clear();
    UserId maximum_user = kInvalidUserId;
    GroupId maximum_group = kInvalidGroupId;

    ExpectToken(reader, "GROUPS");
    const std::size_t group_count = ReadCount(reader);
    for (std::size_t i = 0; i < group_count; ++i) {
        ExpectToken(reader, "GROUP");
        Group group;
        group.id = ReadNumber(reader);
        group.name = ReadName(reader);
        if (group.id == kInvalidGroupId || result.groups_->contains_key(group.id)) {
            throw std::runtime_error("Zero or duplicate group ID");
        }
        result.groups_->add(group.id, group);
        if (group.id > maximum_group) {
            maximum_group = group.id;
        }
    }

    ExpectToken(reader, "USERS");
    const std::size_t user_count = ReadCount(reader);
    if (user_count == 0) {
        throw std::runtime_error("User registry must contain an administrator");
    }
    for (std::size_t i = 0; i < user_count; ++i) {
        ExpectToken(reader, "USER");
        User user;
        user.id = ReadNumber(reader);
        user.name = ReadName(reader);
        user.is_admin = ReadFlag(reader);
        if (user.id == kInvalidUserId || result.users_->contains_key(user.id)) {
            throw std::runtime_error("Zero or duplicate user ID");
        }

        const std::size_t membership_count = ReadCount(reader);
        if (membership_count > group_count) {
            throw std::runtime_error("Too many group memberships");
        }
        for (std::size_t j = 0; j < membership_count; ++j) {
            const GroupId group_id = ReadNumber(reader);
            if (group_id == kInvalidGroupId || !result.groups_->contains_key(group_id)) {
                throw std::runtime_error("User refers to an unknown group");
            }
            for (std::size_t k = 0; k < user.group_ids.size(); ++k) {
                if (user.group_ids.get(k) == group_id) {
                    throw std::runtime_error("Duplicate group membership");
                }
            }
            user.group_ids.append(group_id);
        }

        result.users_->add(user.id, user);
        if (user.id > maximum_user) {
            maximum_user = user.id;
        }
    }

    const UserDictionary& restored_users = *result.users_;
    if (!restored_users.contains_key(saved_admin) ||
        !restored_users.get(saved_admin).is_admin) {
        throw std::runtime_error("Designated administrator is missing or has no privileges");
    }
    if (saved_next_user != kInvalidUserId && saved_next_user <= maximum_user) {
        throw std::runtime_error("Next user ID would reuse an existing ID");
    }
    if (saved_next_group != kInvalidGroupId && saved_next_group <= maximum_group) {
        throw std::runtime_error("Next group ID would reuse an existing ID");
    }

    reader >> std::ws;
    if (reader.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected data after user registry");
    }

    result.admin_id_ = saved_admin;
    result.next_user_id_ = saved_next_user;
    result.next_group_id_ = saved_next_group;
    return result;
}
