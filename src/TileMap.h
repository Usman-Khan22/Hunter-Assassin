#pragma once

#include "raylib.h"

#include <filesystem>
#include <string>
#include <vector>

struct TilePosition {
    int column;
    int row;
};

inline bool operator==(TilePosition a, TilePosition b) {
    return a.column == b.column && a.row == b.row;
}

inline bool operator!=(TilePosition a, TilePosition b) { return !(a == b); }

class TileMap {
public:
    static constexpr int TileSize = 48;

    void Load(const std::filesystem::path& path);
    Vector2 Spawn() const;
    int Width() const;
    int Height() const;
    bool IsWall(int column, int row) const;
    bool IsWall(TilePosition tile) const { return IsWall(tile.column, tile.row); }
    bool CircleHitsWall(Vector2 center, float radius) const;
    float RayDistance(Vector2 origin, Vector2 direction, float limit) const;
    static TilePosition WorldTile(Vector2 world);
    static Vector2 TileCenter(TilePosition tile);
    void Draw() const;
    void DrawGrid() const;

private:
    std::vector<std::string> tiles_;
    Vector2 spawn_{};
};
