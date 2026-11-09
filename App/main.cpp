#include <cstddef>
#include <iostream>
#include <string>

#include "Dictionary/AVLDictionary.hpp"
#include "Sequence/ArraySequence/ArraySequence.hpp"

int main() {
    AvlDictionary<std::string, ArraySequence<std::size_t>> index;

    ArraySequence<std::size_t> ids;
    ids.append(12);
    index.add("player.png", ids);
    index.get("player.png").append(37);

    const auto& file_ids = index.get("player.png");

    std::cout << "player.png:";
    for (std::size_t i = 0; i < file_ids.size(); ++i) {
        std::cout << ' ' << file_ids.get(i);
    }
    std::cout << '\n';

    return 0;
}
