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