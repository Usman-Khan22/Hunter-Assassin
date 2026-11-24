#include "TileMap.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

// Validate a temporary grid before replacing the current map and spawn.
void TileMap::Load(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Unable to open map: " + path.string());
    }

    std::vector<std::string> tiles;
    Vector2 spawn{};
    int spawnCount = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || (!tiles.empty() && line.size() != tiles.front().size())) {
            throw std::runtime_error("Map must be a nonempty rectangle.");
        }
        for (std::size_t column = 0; column < line.size(); ++column) {
            char& tile = line[column];
            if (tile == 'S') {
                ++spawnCount;
                spawn = TileCenter({static_cast<int>(column), static_cast<int>(tiles.size())});
                tile = '.';
            } else if (tile != '.' && tile != '#') {
                throw std::runtime_error("Map tiles must be '.', '#', or 'S'.");
            }
        }
        tiles.push_back(line);
    }
    if (!file.eof() || tiles.empty() || spawnCount != 1) {
        throw std::runtime_error("Map must be readable and contain exactly one spawn.");
    }
    tiles_.swap(tiles);
    spawn_ = spawn;
}

Vector2 TileMap::Spawn() const { return spawn_; }
int TileMap::Width() const { return tiles_.empty() ? 0 : static_cast<int>(tiles_.front().size()); }
int TileMap::Height() const { return static_cast<int>(tiles_.size()); }

bool TileMap::IsWall(int column, int row) const {
    return column < 0 || row < 0 || column >= Width() || row >= Height() ||
           tiles_[row][column] == '#';
}

// Restrict collision checks to tiles that overlap the circle's bounding box.
bool TileMap::CircleHitsWall(Vector2 center, float radius) const {
    const int left = static_cast<int>(std::floor((center.x - radius) / TileSize));
    const int right = static_cast<int>(std::floor((center.x + radius) / TileSize));
    const int top = static_cast<int>(std::floor((center.y - radius) / TileSize));
    const int bottom = static_cast<int>(std::floor((center.y + radius) / TileSize));

    for (int row = top; row <= bottom; ++row) {
        for (int column = left; column <= right; ++column) {
            if (!IsWall(column, row)) {
                continue;
            }
            // The nearest point on the tile determines whether it overlaps the circle.
            const float x = std::clamp(center.x, float(column * TileSize),
                                       float((column + 1) * TileSize));
            const float y = std::clamp(center.y, float(row * TileSize),
                                       float((row + 1) * TileSize));
            const float dx = center.x - x;
            const float dy = center.y - y;
            if (dx * dx + dy * dy < radius * radius) {
                return true;
            }
        }
    }
    return false;
}

void TileMap::Draw() const {
    DrawRectangle(0, 0, Width() * TileSize, Height() * TileSize, Color{25, 31, 38, 255});
    for (int row = 0; row < Height(); ++row) {
        for (int column = 0; column < Width(); ++column) {
            if (IsWall(column, row)) {
                const int x = column * TileSize;
                const int y = row * TileSize;
                DrawRectangle(x, y, TileSize, TileSize, Color{65, 73, 83, 255});
                DrawRectangle(x + 1, y + 1, TileSize - 2, TileSize - 5, Color{91, 100, 110, 255});
            }
        }
    }
}

void TileMap::DrawGrid() const {
    constexpr Color grid{135, 187, 205, 95};
    for (int column = 0; column <= Width(); ++column) {
        const int x = column * TileSize;
        DrawLine(x, 0, x, Height() * TileSize, grid);
    }
    for (int row = 0; row <= Height(); ++row) {
        const int y = row * TileSize;
        DrawLine(0, y, Width() * TileSize, y, grid);
    }
}

TilePosition TileMap::WorldTile(Vector2 world) {
    return {static_cast<int>(std::floor(world.x / TileSize)),
            static_cast<int>(std::floor(world.y / TileSize))};
}

Vector2 TileMap::TileCenter(TilePosition tile) {
    return {(tile.column + 0.5f) * TileSize, (tile.row + 0.5f) * TileSize};
}

// Both melee attacks use the same wall traversal as guard vision.
bool TileMap::HasClearLine(Vector2 from, Vector2 to) const {
    if (IsWall(WorldTile(from)) || IsWall(WorldTile(to))) {
        return false;
    }
    const Vector2 offset{to.x - from.x, to.y - from.y};
    const float distance = std::hypot(offset.x, offset.y);
    if (distance == 0.0f) {
        return true;
    }
    const Vector2 direction{offset.x / distance, offset.y / distance};
    return RayDistance(from, direction, distance) >= distance;
}

// Advance a unit ray from tile boundary to tile boundary to find the first wall.
float TileMap::RayDistance(Vector2 origin, Vector2 direction, float limit) const {
    TilePosition tile = WorldTile(origin);
    if (IsWall(tile)) {
        return 0.0f;
    }

    const int stepX = direction.x < 0.0f ? -1 : 1;
    const int stepY = direction.y < 0.0f ? -1 : 1;
    const float infinity = std::numeric_limits<float>::infinity();
    const float deltaX = direction.x == 0.0f ? infinity : TileSize / std::abs(direction.x);
    const float deltaY = direction.y == 0.0f ? infinity : TileSize / std::abs(direction.y);
    const float edgeX = static_cast<float>((tile.column + (stepX > 0)) * TileSize);
    const float edgeY = static_cast<float>((tile.row + (stepY > 0)) * TileSize);
    float nextX = direction.x == 0.0f ? infinity : (edgeX - origin.x) / direction.x;
    float nextY = direction.y == 0.0f ? infinity : (edgeY - origin.y) / direction.y;

    // Visit crossed grid boundaries in distance order, including both sides of a corner.
    while (std::min(nextX, nextY) < limit) {
        const float distance = std::min(nextX, nextY);
        if (std::abs(nextX - nextY) < 0.0001f) {
            if (IsWall(tile.column + stepX, tile.row) || IsWall(tile.column, tile.row + stepY)) {
                return distance;
            }
            tile.column += stepX;
            tile.row += stepY;
            nextX += deltaX;
            nextY += deltaY;
        } else if (nextX < nextY) {
            tile.column += stepX;
            nextX += deltaX;
        } else {
            tile.row += stepY;
            nextY += deltaY;
        }
        if (IsWall(tile)) {
            return distance;
        }
    }
    return limit;
}
