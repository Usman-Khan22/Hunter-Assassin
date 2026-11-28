#pragma once

#include "TileMap.h"

enum EnvironmentLayer { FloorDecal, WallMounted, SolidProp };

struct EnvironmentAsset {
    const char* name;
    int pixelWidth;
    int pixelHeight;
    int width;
    int height;
    EnvironmentLayer layer;
    int mountOffset;
    int maxRotation;
};

inline constexpr EnvironmentAsset EnvironmentAssets[]{
    {"wall_console", 48, 24, 1, 1, WallMounted, -20, 270},
    {"server_rack", 48, 48, 1, 1, SolidProp, 0, 270},
    {"research_desk", 96, 48, 2, 1, SolidProp, 0, 270},
    {"specimen_tank", 48, 48, 1, 1, SolidProp, 0, 0},
    {"power_generator", 96, 96, 2, 2, SolidProp, 0, 270},
    {"locker", 48, 48, 1, 1, SolidProp, 0, 270},
    {"cell_bed", 96, 48, 2, 1, SolidProp, 0, 270},
    {"floor_vent", 48, 48, 1, 1, FloorDecal, 0, 90},
    {"hazard_marking", 48, 48, 1, 1, FloorDecal, 0, 0},
    {"wall_light", 24, 12, 1, 1, WallMounted, -24, 270},
    {"tool_station", 96, 48, 2, 1, SolidProp, 0, 270},
    {"control_desk", 96, 48, 2, 1, SolidProp, 0, 270},
    {"cargo_container", 96, 48, 2, 1, SolidProp, 0, 270},
    {"storage_rack", 96, 48, 2, 1, SolidProp, 0, 270},
    {"floor_arrow", 48, 48, 1, 1, FloorDecal, 0, 270},
    {"floor_arrow_exit", 48, 48, 1, 1, FloorDecal, 0, 270},
    {"wall_pipe", 96, 24, 2, 1, WallMounted, -18, 270}
};
inline constexpr int EnvironmentAssetCount = sizeof(EnvironmentAssets) / sizeof(EnvironmentAssets[0]);

struct EnvironmentPlacement {
    int type;
    TilePosition tile;
    int rotation;

    int Width() const {
        const EnvironmentAsset& asset = EnvironmentAssets[type];
        return rotation == 90 || rotation == 270 ? asset.height : asset.width;
    }

    int Height() const {
        const EnvironmentAsset& asset = EnvironmentAssets[type];
        return rotation == 90 || rotation == 270 ? asset.width : asset.height;
    }
};
