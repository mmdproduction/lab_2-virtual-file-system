#include "FileNameIndex.hpp"
#include "FSNode.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>

FileNameIndex::FileNameIndex(
    std::unique_ptr<FileIndexDictionary> dictionary)
    : dictionary_(std::move(dictionary)) {
    if (!dictionary_) {
        throw std::invalid_argument(
            "index dictionary must not be null");
    }

    if (dictionary_->size() != 0) {
        throw std::invalid_argument(
            "index dictionary must be empty");
    }
}

void FileNameIndex::add(const std::string& name, NodeId id) {
    if (name.empty() || id == kInvalidNodeId) {
        throw std::invalid_argument("invalid file name or ID");
    }

    if (!dictionary_->contains_key(name)) {
        ArraySequence<NodeId> ids;
        ids.append(id);
        dictionary_->add(name, ids);
        return;
    }

    ArraySequence<NodeId>& ids = dictionary_->get(name);

    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (ids.get(i) == id) {
            throw std::invalid_argument(
                "file ID already indexed for this name");
        }
    }

    ids.append(id);
}

void FileNameIndex::remove(const std::string& name, NodeId id) {
    ArraySequence<NodeId>& ids = dictionary_->get(name);

    std::size_t position = 0;

    while (position < ids.size() && ids.get(position) != id) {
        ++position;
    }

    if (position == ids.size()) {
        throw std::out_of_range("file ID not found");
    }

    if (ids.size() == 1) {
        dictionary_->remove(name);
        return;
    }

    ids.remove_at(position);
}

ArraySequence<NodeId> FileNameIndex::find(
    const std::string& name) const {
    const FileIndexDictionary& dictionary = *dictionary_;

    if (!dictionary.contains_key(name)) {
        return {};
    }

    return dictionary.get(name);
}


void FileNameIndex::rename(
    const std::string& old_name,
    const std::string& new_name,
    NodeId id) {
    if (old_name.empty() || new_name.empty() ||
        id == kInvalidNodeId) {
        throw std::invalid_argument("invalid file name or ID");
    }

    ArraySequence<NodeId> remaining_ids = find(old_name);

    std::size_t position = 0;
    while (position < remaining_ids.size() &&
           remaining_ids.get(position) != id) {
        ++position;
    }

    if (position == remaining_ids.size()) {
        throw std::out_of_range("file ID not found");
    }

    if (old_name == new_name) {
        return;
    }

    remaining_ids.remove_at(position);

    ArraySequence<NodeId> updated_ids = find(new_name);

    for (std::size_t i = 0; i < updated_ids.size(); ++i) {
        if (updated_ids.get(i) == id) {
            throw std::invalid_argument(
                "file ID already indexed for this name");
        }
    }

    updated_ids.append(id);

    if (!dictionary_->contains_key(new_name)) {
        dictionary_->add(new_name, ArraySequence<NodeId>{});
    }

    ArraySequence<NodeId>& destination =
        dictionary_->get(new_name);

    if (remaining_ids.size() == 0) {
        dictionary_->remove(old_name);
    } else {
        dictionary_->get(old_name).swap(remaining_ids);
    }

    destination.swap(updated_ids);
}