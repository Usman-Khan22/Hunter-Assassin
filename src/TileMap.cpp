#include "TileMap.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

void TileMap::Load(const std::filesystem::path& path) {
    LoadGrid(path, false);
}

void TileMap::LoadFacility(const std::filesystem::path& path) {
    LoadGrid(path, true);
}

// Validate a temporary grid before replacing the current map and occupancy.
void TileMap::LoadGrid(const std::filesystem::path& path, bool facility) {
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
        if (facility && (line.size() != 50 || tiles.size() >= 35)) {
            throw std::runtime_error("Facility grid must have 35 rows of 50 tiles.");
        }
        if (line.empty() || (!tiles.empty() && line.size() != tiles.front().size())) {
            throw std::runtime_error("Map must be a nonempty rectangle.");
        }
        for (std::size_t column = 0; column < line.size(); ++column) {
            char& tile = line[column];
            if (facility) {
                if (tile != 'W' && tile != '.' && tile != ',') {
                    throw std::runtime_error("Facility tiles must be 'W', '.', or ','.");
                }
                continue;
            }
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
    if (facility && (!file.eof() || tiles.size() != 35)) {
        throw std::runtime_error("Facility grid must have 35 rows of 50 tiles.");
    }
    if (!facility && (!file.eof() || tiles.empty() || spawnCount != 1)) {
        throw std::runtime_error("Map must be readable and contain exactly one spawn.");
    }
    tiles_.swap(tiles);
    crates_.assign(Width() * Height(), 0);
    doorIndex_.Clear();
    ResetDoors();
    ++walkabilityRevision_;
    spawn_ = spawn;
}

void TileMap::AddCrate(TilePosition tile) {
    if (IsWall(tile)) {
        throw std::runtime_error("A cargo crate must occupy a free floor tile.");
    }
    crates_[tile.row * Width() + tile.column] = 1;
}

Vector2 TileMap::Spawn() const { return spawn_; }
int TileMap::Width() const { return tiles_.empty() ? 0 : static_cast<int>(tiles_.front().size()); }
int TileMap::Height() const { return static_cast<int>(tiles_.size()); }

bool TileMap::IsWall(int column, int row) const {
    return IsBlocked(column, row, -1);
}

bool TileMap::IsBlocked(int column, int row, int ignoredDoor) const {
    if (column < 0 || row < 0 || column >= Width() || row >= Height() ||
        tiles_[row][column] == '#' || tiles_[row][column] == 'W' ||
        crates_[row * Width() + column] != 0) {
        return true;
    }
    const int door = doorIndex_.Find(row * Width() + column);
    return door >= 0 && door != ignoredDoor && !doors_[door].open;
}

void TileMap::ConfigureDoors() {
    doorIndex_.Clear();
    ResetDoors();
    for (int index = 0; index < DoorCount; ++index) {
        for (const TilePosition tile : doors_[index].tiles) {
            if (IsWall(tile) || !doorIndex_.Insert(tile.row * Width() + tile.column, index)) {
                throw std::runtime_error("Invalid logical door location.");
            }
        }
    }
    ++walkabilityRevision_;
}

int TileMap::DoorAt(TilePosition tile) const {
    if (tile.column < 0 || tile.row < 0 || tile.column >= Width() || tile.row >= Height()) {
        return -1;
    }
    return doorIndex_.Find(tile.row * Width() + tile.column);
}

const FacilityDoor& TileMap::Door(int index) const { return doors_[index]; }

bool TileMap::OpenDoor(int index, bool hasA, bool hasB, bool lockdownDisabled) {
    FacilityDoor& door = doors_[index];
    if (door.open || (door.lockdown && !lockdownDisabled) ||
        (door.needsA && !hasA) || (door.needsB && !hasB)) {
        return false;
    }
    door.open = true;
    ++walkabilityRevision_;
    return true;
}

void TileMap::ResetDoors() {
    for (FacilityDoor& door : doors_) {
        if (door.open) {
            door.open = false;
            ++walkabilityRevision_;
        }
    }
}

unsigned int TileMap::WalkabilityRevision() const { return walkabilityRevision_; }

// Ignore only the candidate doorway in this interaction ray.
int TileMap::NearbyDoor(Vector2 position, float range, float* nearestDistance) const {
    const TilePosition first = WorldTile({position.x - range, position.y - range});
    const TilePosition last = WorldTile({position.x + range, position.y + range});
    int nearest = -1;
    float best = range;
    for (int row = first.row; row <= last.row; ++row) {
        for (int column = first.column; column <= last.column; ++column) {
            const int door = DoorAt({column, row});
            if (door < 0 || doors_[door].open) {
                continue;
            }
            const Vector2 center = TileCenter({column, row});
            const float dx = center.x - position.x;
            const float dy = center.y - position.y;
            const float distance = std::hypot(dx, dy);
            if (distance > best || distance == 0.0f) {
                continue;
            }
            if (RayDistance(position, {dx / distance, dy / distance}, distance, door) >= distance) {
                best = distance;
                nearest = door;
            }
        }
    }
    if (nearestDistance != nullptr) {
        *nearestDistance = best;
    }
    return nearest;
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
    for (int row = 0; row < Height(); ++row) {
        for (int column = 0; column < Width(); ++column) {
            if (IsWall(column, row)) {
                Color fill{255, 95, 75, 48};
                if (crates_[row * Width() + column]) {
                    fill = Color{255, 192, 75, 85};
                }
                DrawRectangle(column * TileSize, row * TileSize, TileSize, TileSize, fill);
            }
        }
    }
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
float TileMap::RayDistance(Vector2 origin, Vector2 direction, float limit, int ignoredDoor) const {
    TilePosition tile = WorldTile(origin);
    if (IsBlocked(tile.column, tile.row, ignoredDoor)) {
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
            if (IsBlocked(tile.column + stepX, tile.row, ignoredDoor) ||
                IsBlocked(tile.column, tile.row + stepY, ignoredDoor)) {
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
        if (IsBlocked(tile.column, tile.row, ignoredDoor)) {
            return distance;
        }
    }
    return limit;
}
