#include "BfsPathfinder.h"

#include <algorithm>
#include <cassert>

// Each search enqueues a tile at most once, so the queue needs no wraparound.
void BfsPathfinder::ArrayQueue::Reset(std::size_t capacity) {
    cells_.resize(capacity);
    head_ = tail_ = 0;
}

void BfsPathfinder::ArrayQueue::Push(int cell) {
    assert(tail_ < cells_.size());
    cells_[tail_++] = cell;
}

int BfsPathfinder::ArrayQueue::Pop() {
    assert(!Empty());
    return cells_[head_++];
}

bool BfsPathfinder::ArrayQueue::Empty() const { return head_ == tail_; }

// Explore equal-cost tile steps in FIFO order and reconstruct the route from parents.
bool BfsPathfinder::FindPath(const TileMap& map, TilePosition start, TilePosition destination,
                             std::vector<TilePosition>& path) {
    path.clear();
    if (map.IsWall(start) || map.IsWall(destination)) {
        return false;
    }
    if (start == destination) {
        return true;
    }

    const int width = map.Width();
    const int count = width * map.Height();
    const int first = start.row * width + start.column;
    const int goal = destination.row * width + destination.column;
    parents_.resize(count);
    std::fill(parents_.begin(), parents_.end(), -1);
    frontier_.Reset(count);
    parents_[first] = first;
    frontier_.Push(first);

    constexpr TilePosition offsets[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    while (!frontier_.Empty() && parents_[goal] == -1) {
        const int current = frontier_.Pop();
        for (const TilePosition offset : offsets) {
            const TilePosition neighbor{current % width + offset.column, current / width + offset.row};
            if (map.IsWall(neighbor)) {
                continue;
            }
            const int index = neighbor.row * width + neighbor.column;
            if (parents_[index] == -1) {
                // Mark on enqueue so no tile can consume more than one queue slot.
                parents_[index] = current;
                frontier_.Push(index);
            }
        }
    }

    if (parents_[goal] == -1) {
        return false;
    }
    for (int cell = goal; cell != first; cell = parents_[cell]) {
        path.push_back({cell % width, cell / width});
    }
    std::reverse(path.begin(), path.end());
    return true;
}
