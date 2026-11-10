#pragma once

#include <memory>
#include <string>

#include "Dictionary/IDictionary.hpp"
#include "FSNode.hpp"
#include "Sequence/ArraySequence/ArraySequence.hpp"

using FileIndexDictionary =
    IDictionary<std::string, ArraySequence<NodeId>>;

class FileNameIndex {
public:
    explicit FileNameIndex(
        std::unique_ptr<FileIndexDictionary> dictionary);

    void add(const std::string& name, NodeId id);
    void remove(const std::string& name, NodeId id);

    ArraySequence<NodeId> find(const std::string& name) const;

private:
    std::unique_ptr<FileIndexDictionary> dictionary_;
};