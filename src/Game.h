#pragma once

#include "BfsPathfinder.h"
#include "Guard.h"
#include "Player.h"
#include "TileMap.h"

class Game {
public:
    void Run();

private:
    enum ObjectType { Assassin, GuardSpawn, Crate, SecurityDoor, ExitDoor, Terminal, Card, ObjectTypeCount };
    struct FacilityObject {
        ObjectType type;
        TilePosition tile;
        float rotation;
    };

    void Initialize();
    void LoadObjects(const std::filesystem::path& path);
    void LoadArtwork(const std::filesystem::path& directory);
    void DrawFacility() const;
    void ResetRound();
    void Update(float deltaTime);
    void AttackWithKnife();
    void Render() const;
    void Shutdown();
    Vector2 ClampCameraTarget(Vector2 target) const;

    static constexpr int ScreenWidth = 1280;
    static constexpr int ScreenHeight = 720;
    static constexpr float CameraZoom = 1.5f;
    static constexpr float KnifeRange = 52.0f;
    static constexpr float KnifeCooldown = 0.45f;
    static constexpr int GuardCount = 7;
    static constexpr const char* ObjectNames[ObjectTypeCount]{
        "assassin", "guard", "cargo_crate", "security_door", "exit_door", "security_terminal", "access_card"
    };
    TileMap map_;
    Player player_;
    BfsPathfinder pathfinder_;
    std::vector<Guard> guards_;
    std::vector<FacilityObject> objects_;
    Texture2D facilityTexture_{};
    Texture2D sprites_[ObjectTypeCount]{};
    Camera2D camera_{};
    bool debugVisible_ = false;
    float knifeCooldown_ = 0.0f;
    float knifeEffect_ = 0.0f;
    Vector2 knifeOrigin_{};
    Vector2 knifeTarget_{};
};
