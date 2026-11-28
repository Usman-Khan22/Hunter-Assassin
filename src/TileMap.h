#pragma once

#include "raylib.h"
#include "DoorHash.h"

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

struct FacilityDoor {
    int id;
    TilePosition tiles[2];
    bool needsA;
    bool needsB;
    bool lockdown;
    bool open;
};

class TileMap {
public:
    static constexpr int TileSize = 48;

    void Load(const std::filesystem::path& path);
    void LoadFacility(const std::filesystem::path& path);
    void AddCrate(TilePosition tile);
    void AddSolidProp(TilePosition tile);
    void ConfigureDoors();
    int DoorAt(TilePosition tile) const;
    const FacilityDoor& Door(int index) const;
    bool OpenDoor(int index, bool hasA, bool hasB, bool lockdownDisabled = false);
    void ResetDoors();
    unsigned int WalkabilityRevision() const;
    int NearbyDoor(Vector2 position, float range, float* distance = nullptr) const;
    Vector2 Spawn() const;
    int Width() const;
    int Height() const;
    bool IsWall(int column, int row) const;
    bool HasCrate(int column, int row) const;
    bool HasSolidProp(int column, int row) const;
    bool IsPermanentWall(TilePosition tile) const;
    bool IsWall(TilePosition tile) const { return IsWall(tile.column, tile.row); }
    bool CircleHitsWall(Vector2 center, float radius) const;
    bool HasClearLine(Vector2 from, Vector2 to) const;
    float RayDistance(Vector2 origin, Vector2 direction, float limit, int ignoredDoor = -1) const;
    static TilePosition WorldTile(Vector2 world);
    static Vector2 TileCenter(TilePosition tile);
    void Draw() const;
    void DrawGrid() const;

private:
    void LoadGrid(const std::filesystem::path& path, bool facility);
    bool IsBlocked(int column, int row, int ignoredDoor) const;
    static constexpr int DoorCount = 5;
    FacilityDoor doors_[DoorCount]{
        {1, {{42, 9}, {43, 9}}, false, true, false, false},
        {2, {{35, 18}, {35, 19}}, true, false, false, false},
        {3, {{45, 25}, {46, 25}}, true, true, false, false},
        {4, {{40, 29}, {40, 30}}, true, true, false, false},
        {5, {{49, 29}, {49, 30}}, true, true, true, false}
    };
    DoorHash doorIndex_;
    unsigned int walkabilityRevision_ = 0;
    std::vector<std::string> tiles_;
    enum Occupant { EmptyCell, CrateCell, PropCell };
    std::vector<unsigned char> occupancy_;
    Vector2 spawn_{};
};
