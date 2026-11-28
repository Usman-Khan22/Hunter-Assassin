#pragma once

#include "BfsPathfinder.h"
#include "EventHeap.h"
#include "Environment.h"
#include "Guard.h"
#include "Player.h"
#include "TileMap.h"

class Game {
public:
    void Run();

private:
    enum ObjectType { Assassin, GuardSpawn, Crate, SecurityDoor, ExitDoor, Terminal, Card, ObjectTypeCount };
    enum TerminalId { NoTerminal, Servers, Security, Control };
    enum MissionState { Active, Victory, Defeat };
    enum DefeatReason { Eliminated, TimeExpired };
    struct FacilityObject {
        ObjectType type;
        TilePosition tile;
        float rotation;
    };
    struct Projectile {
        bool active;
        Vector2 position;
        Vector2 direction;
        float speed;
        float lifetime;
        int damage;
    };
    struct LaserEffect {
        Vector2 position;
        float angle;
        float remaining;
        bool flash;
    };

    void Initialize();
    void LoadObjects(const std::filesystem::path& path);
    void LoadEnvironment(const std::filesystem::path& path);
    bool IsReservedForSolidProp(TilePosition tile) const;
    void ValidateDecoratedFacility();
    void DrawEnvironment() const;
    void LoadArtwork(const std::filesystem::path& directory);
    void LoadAnimationArtwork(const std::filesystem::path& directory);
    void DrawFacility() const;
    int CardIndex(TilePosition tile) const;
    TerminalId TerminalAt(TilePosition tile) const;
    void UpdateAccess(float deltaTime);
    void UpdateTerminal();
    void CloseTerminal();
    void DrawTerminal() const;
    void EndMission(MissionState result);
    int SecondsRemaining() const;
    void DrawTimer() const;
    void DrawMinimap() const;
    void StartAlarm(double elapsed);
    void ProcessEvents(double elapsed);
    bool SpawnReinforcement();
    bool CanSpawnAt(TilePosition tile) const;
    void DrawAlarm() const;
    void DrawMissionResult() const;
    void ResetRound();
    void Update(float deltaTime);
    void AttackWithKnife();
    void DamagePlayer(int damage);
    bool SpawnProjectile(Vector2 origin, Vector2 direction);
    void UpdateProjectiles(float deltaTime);
    void AddLaserEffect(Vector2 position, float angle, bool flash);
    void DrawLaserCombat() const;
    void CollectAbilityPickups();
    void DrawAbilityPickup(AbilityType type, Vector2 center, float size) const;
    void DrawAbilityHud() const;
    void ClearLaserCombat();
    void Render() const;
    void Shutdown();
    Vector2 ClampCameraTarget(Vector2 target) const;

    static constexpr int ScreenWidth = 1280;
    static constexpr int ScreenHeight = 720;
    static constexpr float CameraZoom = 1.5f;
    static constexpr float KnifeRange = 52.0f;
    static constexpr float KnifeCooldown = 0.45f;
    static constexpr int GuardCount = 7;
    static constexpr TilePosition GuardWaypoints[GuardCount][3]{
        {{20, 10}, {20, 11}, {25, 11}},
        {{12, 23}, {12, 24}, {6, 24}},
        {{33, 21}, {34, 21}, {34, 13}},
        {{24, 17}, {24, 14}, {17, 14}},
        {{39, 5}, {39, 2}, {44, 2}},
        {{37, 26}, {38, 26}, {38, 32}},
        {{7, 30}, {7, 32}, {5, 32}}
    };
    static constexpr TilePosition ReinforcementPatrols[][2]{
        {{47, 3}, {47, 4}},
        {{2, 24}, {3, 24}},
        {{32, 11}, {31, 11}}
    };
    static constexpr int MaxReinforcements = 3;
    static constexpr double ReinforcementDelay = 8.0;
    static constexpr double AlarmDuration = 15.0;
    static constexpr double AlarmCooldown = 30.0;
    static constexpr double RetryDelay = 1.0;
    static constexpr int MaxSpawnRetries = 3;
    static constexpr int ProjectileCapacity = 32;
    static constexpr float ProjectileSpeed = 480.0f;
    static constexpr float ProjectileLifetime = 1.0f;
    static constexpr int ProjectileDamage = 1;
    static constexpr int MissionDuration = 360;
    static constexpr int MinimapTilePixels = 5;
    static constexpr int MinimapMargin = 16;
    static constexpr int MinimapPadding = 6;
    static constexpr TilePosition AbilityTiles[AbilityCount]{{8, 18}, {22, 30}};
    static constexpr const char* ObjectNames[ObjectTypeCount]{
        "assassin", "guard", "cargo_crate", "security_door", "exit_door", "security_terminal", "access_card"
    };
    TileMap map_;
    Player player_;
    BfsPathfinder pathfinder_;
    std::vector<Guard> guards_;
    std::vector<FacilityObject> objects_;
    std::vector<EnvironmentPlacement> environment_;
    Texture2D environmentTextures_[EnvironmentAssetCount]{};
    Texture2D facilityTexture_{};
    Texture2D sprites_[ObjectTypeCount]{};
    enum LaserTexture { LaserGuardIdle, LaserProjectile, LaserCharge, LaserImpact, MuzzleFlash, KnifeSlash, LaserTextureCount };
    Texture2D laserTextures_[LaserTextureCount]{};
    enum AnimationKind { AssassinFrames, MeleeFrames, LaserFrames, AnimationKindCount };
    CharacterFrames animationFrames_[AnimationKindCount]{};
    Projectile projectiles_[ProjectileCapacity]{};
    LaserEffect laserEffects_[ProjectileCapacity]{};
    Camera2D camera_{};
    bool debugVisible_ = false;
    bool minimapVisible_ = true;
    float knifeCooldown_ = 0.0f;
    bool cards_[2]{};
    int nearbyDoor_ = -1;
    const char* accessMessage_ = "";
    float accessMessageTime_ = 0.0f;
    TerminalId nearbyTerminal_ = NoTerminal;
    TerminalId activeTerminal_ = NoTerminal;
    bool clues_[2]{};
    bool lockdownDisabled_ = false;
    char password_[5]{};
    int passwordLength_ = 0;
    bool passwordDenied_ = false;
    bool exitRequested_ = false;
    MissionState missionState_ = Active;
    DefeatReason defeatReason_ = Eliminated;
    double timeRemaining_ = MissionDuration;
    int pendingExit_ = -1;
    EventHeap events_;
    bool alarmActive_ = false;
    double nextAlarmTime_ = 0.0;
    double reinforcementTime_ = -1.0;
    int reinforcementCount_ = 0;
    int spawnRetries_ = 0;
    TilePosition alarmLocation_{};
    const char* alarmNotice_ = "";
    double alarmNoticeUntil_ = 0.0;
    bool abilityPickupCollected_[AbilityCount]{};
};
