#pragma once

#include "TileMap.h"

#include <cstddef>
#include <vector>

class BfsPathfinder {
public:
    // The result omits the start tile and includes the destination.
    bool FindPath(const TileMap& map, TilePosition start, TilePosition destination,
                  std::vector<TilePosition>& path);

private:
    class ArrayQueue {
    public:
        void Reset(std::size_t capacity);
        void Push(int cell);
        int Pop();
        bool Empty() const;

    private:
        std::vector<int> cells_;
        std::size_t head_ = 0;
        std::size_t tail_ = 0;
    };

    ArrayQueue frontier_;
    std::vector<int> parents_;
};
