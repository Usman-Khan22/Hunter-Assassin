#include "Game.h"
#include "Artwork.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

void Game::Run() {
    Initialize();
    while (!exitRequested_ && !WindowShouldClose()) {
        Update(GetFrameTime());
        Render();
    }
    Shutdown();
}

void Game::Initialize() {
    const auto assets = std::filesystem::u8path(GetApplicationDirectory()) / "assets";
    map_.LoadFacility(assets / "maps" / "facility.txt");
    LoadObjects(assets / "maps" / "objects.txt");
    camera_.offset = {ScreenWidth / 2.0f, ScreenHeight / 2.0f};
    camera_.zoom = CameraZoom;
    ResetRound();

    InitWindow(ScreenWidth, ScreenHeight, "Hunter's Assassin");
    if (!IsWindowReady()) {
        throw std::runtime_error("Unable to initialize the game window.");
    }
    try {
        LoadArtwork(assets / "png");
    } catch (...) {
        Shutdown();
        throw;
    }
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
}

// Apply crate occupancy before checking that every placement is accessible.
void Game::LoadObjects(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Unable to open facility objects: " + path.string());
    }
    objects_.clear();
    int assassinCount = 0;
    int guardCount = 0;
    TilePosition spawn{};
    int lineNumber = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lineNumber;
        std::istringstream fields(line);
        std::string name;
        if (!(fields >> name) || name.front() == '#') {
            continue;
        }
        const std::string location = " on objects.txt line " + std::to_string(lineNumber);
        int type = 0;
        while (type < ObjectTypeCount && name != ObjectNames[type]) {
            ++type;
        }
        TilePosition tile{};
        float rotation = 0.0f;
        std::string extra;
        if (type == ObjectTypeCount || !(fields >> tile.column >> tile.row >> rotation) ||
            (fields >> extra) || !std::isfinite(rotation) || map_.IsWall(tile)) {
            throw std::runtime_error("Invalid facility object" + location);
        }
        for (const FacilityObject& object : objects_) {
            if (object.tile == tile) {
                throw std::runtime_error("Overlapping facility objects" + location);
            }
        }
        objects_.push_back({static_cast<ObjectType>(type), tile, rotation});
        if (type == Assassin) {
            ++assassinCount;
            spawn = tile;
        } else if (type == GuardSpawn) {
            ++guardCount;
        } else if (type == Crate) {
            map_.AddCrate(tile);
        }
    }
    if (!file.eof() || assassinCount != 1 || guardCount != GuardCount) {
        throw std::runtime_error("Facility needs one assassin and seven guards.");
    }
    map_.ConfigureDoors();
    int doorTiles = 0;
    int cards = 0;
    int terminals = 0;
    std::vector<TilePosition> pathToObject;
    for (const FacilityObject& object : objects_) {
        if (object.type == SecurityDoor || object.type == ExitDoor) {
            const int door = map_.DoorAt(object.tile);
            if (door < 0 || map_.Door(door).lockdown != (object.type == ExitDoor)) {
                throw std::runtime_error("Door sprite does not match a logical door.");
            }
            ++doorTiles;
            continue;
        }
        if (object.type == Card) {
            if (CardIndex(object.tile) < 0) {
                throw std::runtime_error("Unexpected access card location.");
            }
            ++cards;
        }
        if (object.type == Terminal) {
            if (TerminalAt(object.tile) == NoTerminal) {
                throw std::runtime_error("Unexpected terminal location.");
            }
            ++terminals;
        }
        if (object.type != Crate && !pathfinder_.FindPath(map_, spawn, object.tile, pathToObject)) {
            throw std::runtime_error("Unreachable facility object at (" + std::to_string(object.tile.column) +
                                     ", " + std::to_string(object.tile.row) + ").");
        }
    }
    if (doorTiles != 10 || cards != 2 || terminals != 3) {
        throw std::runtime_error("Facility needs ten door tiles, two access cards, and three terminals.");
    }
    for (const TilePosition tile : AbilityTiles) {
        for (const FacilityObject& object : objects_) {
            if (object.tile == tile) {
                throw std::runtime_error("Ability pickup overlaps a facility object.");
            }
        }
        if (!pathfinder_.FindPath(map_, spawn, tile, pathToObject)) {
            throw std::runtime_error("Ability pickup must be reachable from spawn.");
        }
    }
}

void Game::LoadArtwork(const std::filesystem::path& directory) {
    facilityTexture_ = LoadTexture((directory / "facility_base.png").string().c_str());
    if (facilityTexture_.id == 0 || facilityTexture_.width != map_.Width() * TileMap::TileSize ||
        facilityTexture_.height != map_.Height() * TileMap::TileSize) {
        throw std::runtime_error("Facility artwork must match the grid's world dimensions.");
    }
    for (int type = 0; type < ObjectTypeCount; ++type) {
        const std::string name = std::string(ObjectNames[type]) + ".png";
        sprites_[type] = LoadTexture((directory / name).string().c_str());
        if (sprites_[type].id == 0) {
            throw std::runtime_error("Unable to load facility sprite: " + name);
        }
    }
    const auto laserDirectory = directory.parent_path() / "environment_expansion" / "png";
    const char* paths[LaserTextureCount]{
        "laser_guard.png", "effects/laser_projectile.png", "effects/laser_charge.png",
        "effects/laser_impact.png", "effects/muzzle_flash.png", "effects/knife_slash.png"
    };
    for (int index = 0; index < LaserTextureCount; ++index) {
        laserTextures_[index] = LoadTexture((laserDirectory / paths[index]).string().c_str());
        if (laserTextures_[index].id == 0) {
            if (index == KnifeSlash) {
                TraceLog(LOG_WARNING, "Unable to load knife slash: %s", paths[index]);
                continue;
            }
            throw std::runtime_error(std::string("Unable to load laser artwork: ") + paths[index]);
        }
    }
    LoadAnimationArtwork(laserDirectory / "animations");
}

void Game::LoadAnimationArtwork(const std::filesystem::path& directory) {
    const char* names[]{"assassin", "guard", "laser_guard"};
    for (int kind = 0; kind < AnimationKindCount; ++kind) {
        CharacterFrames& frames = animationFrames_[kind];
        Texture2D* groups[]{frames.walk, frames.attack, frames.death};
        const char* actions[]{"walk", kind == LaserFrames ? "shoot" : "attack", "death"};
        const int counts[]{3, 3, 2};
        for (int action = 0; action < 3; ++action) {
            for (int frame = 0; frame < counts[action]; ++frame) {
                const std::string name = std::string(names[kind]) + "_" + actions[action] +
                                         "_" + std::to_string(frame + 1) + ".png";
                const auto path = directory / names[kind] / name;
                Texture2D texture = LoadTexture(path.string().c_str());
                if (texture.id == 0 || texture.width != 40 || texture.height != 40) {
                    TraceLog(LOG_WARNING, "Invalid animation frame %s; using idle sprite.", path.string().c_str());
                    if (texture.id != 0) {
                        UnloadTexture(texture);
                    }
                    texture = {};
                }
                groups[action][frame] = texture;
            }
        }
    }
}

void Game::DrawFacility() const {
    DrawTexture(facilityTexture_, 0, 0, WHITE);
    // Place bodies under props so cards and terminals remain readable.
    for (const Guard& guard : guards_) {
        if (!guard.IsAlive()) {
            if (guard.Type() == LASER) {
                guard.Draw(laserTextures_[LaserGuardIdle], &animationFrames_[LaserFrames]);
            } else {
                guard.Draw(sprites_[GuardSpawn], &animationFrames_[MeleeFrames]);
            }
        }
    }
    for (const FacilityObject& object : objects_) {
        if (object.type == Card && cards_[CardIndex(object.tile)]) {
            continue;
        }
        const int door = map_.DoorAt(object.tile);
        if (door >= 0 && map_.Door(door).open) {
            continue;
        }
        if (object.type != Assassin && object.type != GuardSpawn) {
            DrawCenteredSprite(sprites_[object.type], TileMap::TileCenter(object.tile), object.rotation, WHITE);
        }
    }
}

int Game::CardIndex(TilePosition tile) const {
    if (tile == TilePosition{47, 7}) {
        return 0;
    }
    if (tile == TilePosition{25, 32}) {
        return 1;
    }
    return -1;
}

Game::TerminalId Game::TerminalAt(TilePosition tile) const {
    if (tile == TilePosition{28, 2}) {
        return Servers;
    }
    if (tile == TilePosition{46, 2}) {
        return Security;
    }
    if (tile == TilePosition{46, 19}) {
        return Control;
    }
    return NoTerminal;
}

void Game::UpdateAccess(float deltaTime) {
    if (accessMessageTime_ > 0.0f) {
        accessMessageTime_ -= deltaTime;
    }
    const Vector2 position = player_.Position();
    for (const FacilityObject& object : objects_) {
        if (object.type != Card) {
            continue;
        }
        const int card = CardIndex(object.tile);
        const Vector2 center = TileMap::TileCenter(object.tile);
        if (!cards_[card] && std::hypot(center.x - position.x, center.y - position.y) <= 24.0f &&
            map_.HasClearLine(position, center)) {
            cards_[card] = true;
            accessMessage_ = card == 0 ? "Card A collected" : "Card B collected";
            accessMessageTime_ = 2.0f;
        }
    }

    constexpr float range = 68.0f;
    float nearestDistance = range;
    nearbyDoor_ = map_.NearbyDoor(position, range, &nearestDistance);
    nearbyTerminal_ = NoTerminal;
    for (const FacilityObject& object : objects_) {
        if (object.type != Terminal) {
            continue;
        }
        const Vector2 center = TileMap::TileCenter(object.tile);
        const float distance = std::hypot(center.x - position.x, center.y - position.y);
        if (distance <= range && (distance < nearestDistance ||
            (distance == nearestDistance && nearbyDoor_ < 0 && nearbyTerminal_ == NoTerminal)) &&
            map_.HasClearLine(position, center)) {
            nearestDistance = distance;
            nearbyTerminal_ = TerminalAt(object.tile);
            nearbyDoor_ = -1;
        }
    }
    if (!IsKeyPressed(KEY_E)) {
        return;
    }
    if (nearbyTerminal_ != NoTerminal) {
        if (nearbyTerminal_ == Control && (!cards_[0] || !cards_[1])) {
            accessMessage_ = "Access denied: both cards required";
            accessMessageTime_ = 2.0f;
            return;
        }
        activeTerminal_ = nearbyTerminal_;
        if (activeTerminal_ == Servers) {
            clues_[0] = true;
        } else if (activeTerminal_ == Security) {
            clues_[1] = true;
        }
        // Discard characters typed on the same frame as the interaction key.
        while (GetCharPressed() != 0) {}
        return;
    }
    if (nearbyDoor_ < 0) {
        return;
    }
    const FacilityDoor& door = map_.Door(nearbyDoor_);
    if (door.needsA && !cards_[0] && door.needsB && !cards_[1]) {
        accessMessage_ = "Access denied: cards A and B required";
    } else if (door.needsA && !cards_[0]) {
        accessMessage_ = "Access denied: card A required";
    } else if (door.needsB && !cards_[1]) {
        accessMessage_ = "Access denied: card B required";
    } else if (door.lockdown) {
        if (lockdownDisabled_) {
            pendingExit_ = nearbyDoor_;
            return;
        }
        accessMessage_ = "Final exit: lockdown active";
    } else if (map_.OpenDoor(nearbyDoor_, cards_[0], cards_[1])) {
        accessMessage_ = "Access granted";
        nearbyDoor_ = -1;
    }
    accessMessageTime_ = 2.0f;
}

void Game::CloseTerminal() {
    activeTerminal_ = NoTerminal;
    nearbyTerminal_ = NoTerminal;
    passwordLength_ = 0;
    password_[0] = '\0';
    passwordDenied_ = false;
}

// Keep the code as text so the leading zero is preserved.
void Game::UpdateTerminal() {
    if (activeTerminal_ != Control || lockdownDisabled_) {
        return;
    }
    if (IsKeyPressed(KEY_BACKSPACE) && passwordLength_ > 0) {
        password_[--passwordLength_] = '\0';
    }
    int character = GetCharPressed();
    while (character != 0) {
        if (character >= '0' && character <= '9' && passwordLength_ < 4) {
            password_[passwordLength_++] = static_cast<char>(character);
            password_[passwordLength_] = '\0';
            passwordDenied_ = false;
        }
        character = GetCharPressed();
    }
    if (!IsKeyPressed(KEY_ENTER) && !IsKeyPressed(KEY_KP_ENTER)) {
        return;
    }
    if (std::strcmp(password_, "0742") == 0) {
        lockdownDisabled_ = true;
        accessMessage_ = "Override complete. Reach the exit.";
        accessMessageTime_ = 3.0f;
    } else {
        passwordDenied_ = true;
    }
    passwordLength_ = 0;
    password_[0] = '\0';
}

// Recreate round state while keeping the loaded map and open window.
void Game::ResetRound() {
    ClearLaserCombat();
    for (bool& collected : abilityPickupCollected_) {
        collected = false;
    }
    events_.Clear();
    alarmActive_ = false;
    nextAlarmTime_ = 0.0;
    reinforcementTime_ = -1.0;
    reinforcementCount_ = 0;
    spawnRetries_ = 0;
    alarmLocation_ = {};
    alarmNotice_ = "";
    alarmNoticeUntil_ = 0.0;
    missionState_ = Active;
    defeatReason_ = Eliminated;
    timeRemaining_ = MissionDuration;
    pendingExit_ = -1;
    map_.ResetDoors();
    cards_[0] = cards_[1] = false;
    nearbyDoor_ = -1;
    nearbyTerminal_ = NoTerminal;
    CloseTerminal();
    clues_[0] = clues_[1] = false;
    lockdownDisabled_ = false;
    accessMessage_ = "";
    accessMessageTime_ = 0.0f;
    // Three waypoints after each spawn, in the guard order from objects.txt.
    constexpr TilePosition waypoints[GuardCount][3]{
        {{20, 10}, {20, 11}, {25, 11}},
        {{12, 23}, {12, 24}, {6, 24}},
        {{33, 21}, {34, 21}, {34, 13}},
        {{24, 17}, {24, 14}, {17, 14}},
        {{39, 5}, {39, 2}, {44, 2}},
        {{37, 26}, {38, 26}, {38, 32}},
        {{7, 30}, {7, 32}, {5, 32}}
    };
    guards_.clear();
    guards_.reserve(GuardCount + MaxReinforcements);
    int guardIndex = 0;
    for (const FacilityObject& object : objects_) {
        if (object.type == Assassin) {
            player_.Spawn(TileMap::TileCenter(object.tile), object.rotation);
        } else if (object.type == GuardSpawn) {
            const TilePosition* route = waypoints[guardIndex++];
            GuardType type = MELEE;
            if (object.tile == TilePosition{22, 17} || object.tile == TilePosition{43, 5}) {
                type = LASER;
            }
            guards_.emplace_back(map_, pathfinder_,
                std::initializer_list<TilePosition>{object.tile, route[0], route[1], route[2]}, type);
            guards_.back().SetFacing(object.rotation);
        }
    }
    knifeCooldown_ = 0.0f;
    camera_.target = ClampCameraTarget(player_.Position());
}

void Game::EndMission(MissionState result) {
    missionState_ = result;
    events_.Clear();
    alarmActive_ = false;
    reinforcementTime_ = -1.0;
    for (Guard& guard : guards_) {
        guard.CancelLaserCharge();
    }
    pendingExit_ = -1;
    nearbyDoor_ = -1;
    CloseTerminal();
}

void Game::Update(float deltaTime) {
    const bool terminalWasOpen = activeTerminal_ != NoTerminal;
    const bool escape = IsKeyPressed(KEY_ESCAPE);
    if (escape && !terminalWasOpen) {
        exitRequested_ = true;
        return;
    }
    if (escape) {
        CloseTerminal();
    }
    if (IsKeyPressed(KEY_F3)) {
        debugVisible_ = !debugVisible_;
    }
    if (missionState_ != Active) {
        if (IsKeyPressed(KEY_R)) {
            ResetRound();
        } else if (missionState_ == Defeat && defeatReason_ == Eliminated) {
            player_.AdvanceAnimation(deltaTime);
        }
        return;
    }
    if (!terminalWasOpen && IsKeyPressed(KEY_M)) {
        minimapVisible_ = !minimapVisible_;
    }
    pendingExit_ = -1;
    timeRemaining_ -= deltaTime;
    if (timeRemaining_ <= 0.0) {
        timeRemaining_ = 0.0;
        defeatReason_ = TimeExpired;
        EndMission(Defeat);
        return;
    }
    if (player_.Health() == 0) {
        EndMission(Defeat);
        return;
    }

    if (terminalWasOpen && accessMessageTime_ > 0.0f) {
        accessMessageTime_ -= deltaTime;
    }
    player_.TickAbilities(deltaTime);
    if (!terminalWasOpen) {
        if (IsKeyPressed(KEY_Q)) {
            player_.ActivateAbility(Cloak);
        }
        if (IsKeyPressed(KEY_LEFT_SHIFT)) {
            player_.ActivateAbility(Boost);
        }
    }
    player_.Update(deltaTime, map_, !terminalWasOpen);
    if (!terminalWasOpen) {
        CollectAbilityPickups();
        UpdateAccess(deltaTime);
    } else if (!escape) {
        UpdateTerminal();
    }
    if (knifeCooldown_ > 0.0f) {
        knifeCooldown_ -= deltaTime;
    }
    // Resolve the knife first so a guard killed this frame cannot retaliate.
    if (!terminalWasOpen && activeTerminal_ == NoTerminal &&
        IsKeyPressed(KEY_SPACE) && knifeCooldown_ <= 0.0f) {
        AttackWithKnife();
    }

    UpdateProjectiles(deltaTime);
    if (missionState_ != Active) {
        return;
    }
    bool detected = false;
    bool newlyDetected = false;
    const Vector2 playerPosition = player_.Position();
    const bool visibleTarget = !player_.IsInvisible();
    for (Guard& guard : guards_) {
        const bool wasChasing = guard.IsChasing();
        // A cloaked player is no target. Guards keep their existing investigation tile.
        guard.UpdateWithTarget(deltaTime, visibleTarget ? &playerPosition : nullptr, map_, pathfinder_);
        if (guard.IsChasing()) {
            detected = true;
            if (!wasChasing) {
                newlyDetected = true;
            }
        }
        if (visibleTarget && guard.TryAttack(playerPosition, map_)) {
            DamagePlayer(1);
            if (missionState_ != Active) {
                return;
            }
        }
        Vector2 muzzle{};
        Vector2 direction{};
        if (guard.TakeLaserShot(muzzle, direction)) {
            if (SpawnProjectile(muzzle, direction)) {
                guard.PlayLaserShot();
            }
        }
    }
    player_.UpdateExposure(deltaTime, detected);

    // Commit escape only after this frame's timer and guard attacks have resolved.
    if (pendingExit_ >= 0 && map_.OpenDoor(pendingExit_, cards_[0], cards_[1], lockdownDisabled_)) {
        EndMission(Victory);
        return;
    }

    const Vector2 target = ClampCameraTarget(player_.Position());
    // Exponential interpolation keeps the camera response consistent across frame rates.
    const float blend = 1.0f - std::exp(-8.0f * deltaTime);
    camera_.target.x += (target.x - camera_.target.x) * blend;
    camera_.target.y += (target.y - camera_.target.y) * blend;
    camera_.target = ClampCameraTarget(camera_.target);

    const double elapsed = MissionDuration - timeRemaining_;
    ProcessEvents(elapsed);
    if (newlyDetected) {
        StartAlarm(elapsed);
    }
}

void Game::StartAlarm(double elapsed) {
    // Reserve both queue slots before changing alarm state.
    if (alarmActive_ || reinforcementTime_ >= 0.0 || elapsed < nextAlarmTime_ ||
        reinforcementCount_ >= MaxReinforcements || events_.Size() > EventHeap::Capacity - 2) {
        return;
    }
    reinforcementTime_ = elapsed + ReinforcementDelay;
    events_.Insert(SPAWN_REINFORCEMENT, reinforcementTime_);
    events_.Insert(END_ALARM, elapsed + AlarmDuration);
    alarmActive_ = true;
    spawnRetries_ = 0;
    alarmLocation_ = TileMap::WorldTile(player_.Position());
    alarmNotice_ = "Detection reported";
    alarmNoticeUntil_ = elapsed + 3.0;
}

void Game::ProcessEvents(double elapsed) {
    ScheduledEvent event{};
    while (events_.Peek(event) && event.time <= elapsed) {
        events_.ExtractMin(event);
        if (event.type == END_ALARM) {
            alarmActive_ = false;
            nextAlarmTime_ = event.time + AlarmCooldown;
        } else {
            reinforcementTime_ = -1.0;
            if (SpawnReinforcement()) {
                alarmNotice_ = "Reinforcement arrived";
                alarmNoticeUntil_ = elapsed + 3.0;
            } else if (reinforcementCount_ < MaxReinforcements && spawnRetries_ < MaxSpawnRetries &&
                       events_.Insert(SPAWN_REINFORCEMENT, event.time + RetryDelay)) {
                ++spawnRetries_;
                reinforcementTime_ = event.time + RetryDelay;
            }
        }
    }
}

bool Game::CanSpawnAt(TilePosition tile) const {
    if (map_.IsWall(tile)) {
        return false;
    }
    for (const FacilityObject& object : objects_) {
        if (object.tile == tile) {
            return false;
        }
    }
    const Vector2 center = TileMap::TileCenter(tile);
    const Vector2 player = player_.Position();
    if (std::hypot(center.x - player.x, center.y - player.y) < 3 * TileMap::TileSize) {
        return false;
    }
    for (const Guard& guard : guards_) {
        const Vector2 position = guard.Position();
        if (guard.IsAlive() && std::hypot(center.x - position.x, center.y - position.y) < 32.0f) {
            return false;
        }
    }
    const float halfTile = TileMap::TileSize / 2.0f;
    const float left = camera_.target.x - camera_.offset.x / camera_.zoom;
    const float top = camera_.target.y - camera_.offset.y / camera_.zoom;
    const float right = left + ScreenWidth / camera_.zoom;
    const float bottom = top + ScreenHeight / camera_.zoom;
    return center.x + halfTile < left || center.x - halfTile > right ||
           center.y + halfTile < top || center.y - halfTile > bottom;
}

bool Game::SpawnReinforcement() {
    if (reinforcementCount_ >= MaxReinforcements) {
        return false;
    }
    constexpr TilePosition patrols[][2]{
        {{47, 3}, {47, 4}},
        {{2, 24}, {3, 24}},
        {{32, 11}, {31, 11}}
    };
    for (const auto& patrol : patrols) {
        if (CanSpawnAt(patrol[0]) && !map_.IsWall(patrol[1])) {
            guards_.emplace_back(map_, pathfinder_,
                std::initializer_list<TilePosition>{patrol[0], patrol[1]});
            guards_.back().Investigate(alarmLocation_, map_, pathfinder_);
            ++reinforcementCount_;
            return true;
        }
    }
    return false;
}

void Game::DamagePlayer(int damage) {
    const int previousHealth = player_.Health();
    player_.TakeDamage(damage);
    if (player_.Health() < previousHealth && activeTerminal_ != NoTerminal) {
        CloseTerminal();
    }
    if (player_.Health() == 0) {
        EndMission(Defeat);
    }
}

bool Game::SpawnProjectile(Vector2 origin, Vector2 direction) {
    for (Projectile& projectile : projectiles_) {
        if (!projectile.active) {
            projectile = {true, origin, direction, ProjectileSpeed, ProjectileLifetime, ProjectileDamage};
            const Vector2 flash{origin.x + direction.x * 6.0f, origin.y + direction.y * 6.0f};
            AddLaserEffect(flash, std::atan2(direction.y, direction.x) * RAD2DEG, true);
            return true;
        }
    }
    return false;
}

// Sweep the projectile center and compare the first wall and player hit distances.
void Game::UpdateProjectiles(float deltaTime) {
    for (LaserEffect& effect : laserEffects_) {
        if (effect.remaining > 0.0f) {
            effect.remaining -= deltaTime;
        }
    }
    for (Projectile& projectile : projectiles_) {
        if (!projectile.active || deltaTime <= 0.0f) {
            continue;
        }
        const float travel = projectile.speed * std::min(deltaTime, projectile.lifetime);
        const Vector2 from = projectile.position;
        const Vector2 direction = projectile.direction;
        const Vector2 end{from.x + direction.x * travel, from.y + direction.y * travel};
        const float wallDistance = map_.RayDistance(from, direction, travel);
        const bool wallHit = wallDistance < travel || map_.IsWall(TileMap::WorldTile(end));
        const Vector2 player = player_.Position();
        const float dx = player.x - from.x;
        const float dy = player.y - from.y;
        const float along = dx * direction.x + dy * direction.y;
        const float perpendicular = dx * direction.y - dy * direction.x;
        const float radiusSquared = Player::Radius * Player::Radius;
        float playerDistance = travel + 1.0f;
        if (perpendicular * perpendicular <= radiusSquared) {
            const float halfChord = std::sqrt(radiusSquared - perpendicular * perpendicular);
            if (along + halfChord >= 0.0f) {
                playerDistance = std::max(0.0f, along - halfChord);
            }
        }
        const bool playerHit = playerDistance <= travel && (!wallHit || playerDistance < wallDistance);
        if (playerHit || wallHit) {
            float hitDistance = wallDistance;
            if (playerHit) {
                hitDistance = playerDistance;
            }
            AddLaserEffect({from.x + direction.x * hitDistance, from.y + direction.y * hitDistance}, 0.0f, false);
            projectile.active = false;
            if (playerHit) {
                DamagePlayer(projectile.damage);
                if (missionState_ != Active) {
                    return;
                }
            }
        } else {
            projectile.position = end;
            projectile.lifetime -= deltaTime;
            if (projectile.lifetime <= 0.0f) {
                projectile.active = false;
            }
        }
    }
}

void Game::AddLaserEffect(Vector2 position, float angle, bool flash) {
    for (LaserEffect& effect : laserEffects_) {
        if (effect.remaining <= 0.0f) {
            effect = {position, angle, flash ? 0.08f : 0.14f, flash};
            return;
        }
    }
}

void Game::ClearLaserCombat() {
    for (Projectile& projectile : projectiles_) {
        projectile = {};
    }
    for (LaserEffect& effect : laserEffects_) {
        effect = {};
    }
}

void Game::DrawLaserCombat() const {
    for (const Guard& guard : guards_) {
        if (guard.IsCharging()) {
            const Vector2 from = guard.LaserMuzzle(true);
            const Vector2 direction = guard.LaserDirection();
            const float distance = map_.RayDistance(from, direction, Guard::LaserRange);
            const Vector2 end{from.x + direction.x * distance, from.y + direction.y * distance};
            DrawLineEx(from, end, 1.0f, Color{255, 140, 80, 175});
            DrawCenteredSprite(laserTextures_[LaserCharge], guard.LaserMuzzle(), 0.0f, WHITE);
        }
    }
    for (const Projectile& projectile : projectiles_) {
        if (projectile.active) {
            const float angle = std::atan2(projectile.direction.y, projectile.direction.x) * RAD2DEG;
            DrawCenteredSprite(laserTextures_[LaserProjectile], projectile.position, angle, WHITE);
        }
    }
    for (const LaserEffect& effect : laserEffects_) {
        if (effect.remaining > 0.0f) {
            const LaserTexture texture = effect.flash ? MuzzleFlash : LaserImpact;
            DrawCenteredSprite(laserTextures_[texture], effect.position, effect.angle, WHITE);
        }
    }
}

// Scan once for the nearest guard the knife can actually reach.
void Game::AttackWithKnife() {
    player_.BreakCloak();
    knifeCooldown_ = KnifeCooldown;
    const Vector2 origin = player_.Position();
    Vector2 direction = player_.Facing();
    Guard* closest = nullptr;
    float closestDistance = KnifeRange;

    for (Guard& guard : guards_) {
        if (!guard.IsAlive()) {
            continue;
        }
        const Vector2 position = guard.Position();
        const float distance = std::hypot(position.x - origin.x, position.y - origin.y);
        if (distance <= KnifeRange && (closest == nullptr || distance < closestDistance) &&
            map_.HasClearLine(origin, position)) {
            closest = &guard;
            closestDistance = distance;
        }
    }
    if (closest != nullptr) {
        const Vector2 target = closest->Position();
        if (closestDistance > 0.0f) {
            direction = {(target.x - origin.x) / closestDistance, (target.y - origin.y) / closestDistance};
        }
        closest->TakeKnifeHit(origin);
    }
    player_.StartKnifeAnimation(direction);
}

// Leave room for half the viewport on either side of the camera target.
Vector2 Game::ClampCameraTarget(Vector2 target) const {
    const float width = static_cast<float>(map_.Width() * TileMap::TileSize);
    const float height = static_cast<float>(map_.Height() * TileMap::TileSize);
    const float halfViewWidth = camera_.offset.x / camera_.zoom;
    const float halfViewHeight = camera_.offset.y / camera_.zoom;

    // Center a map axis that is smaller than the viewport.
    return {
        width <= 2.0f * halfViewWidth ? width / 2.0f :
            std::clamp(target.x, halfViewWidth, width - halfViewWidth),
        height <= 2.0f * halfViewHeight ? height / 2.0f :
            std::clamp(target.y, halfViewHeight, height - halfViewHeight)
    };
}

void Game::Render() const {
    BeginDrawing();
    ClearBackground(Color{16, 21, 27, 255});

    BeginMode2D(camera_);
    DrawFacility();
    for (int index = 0; index < AbilityCount; ++index) {
        if (!abilityPickupCollected_[index]) {
            DrawAbilityPickup(static_cast<AbilityType>(index), TileMap::TileCenter(AbilityTiles[index]), 10.0f);
        }
    }
    for (const Guard& guard : guards_) {
        guard.DrawVision(map_);
    }
    for (const Guard& guard : guards_) {
        if (!guard.IsAlive()) {
            continue;
        }
        if (guard.Type() == LASER) {
            guard.Draw(laserTextures_[LaserGuardIdle], &animationFrames_[LaserFrames]);
        } else {
            guard.Draw(sprites_[GuardSpawn], &animationFrames_[MeleeFrames]);
        }
    }
    player_.Draw(sprites_[Assassin], &animationFrames_[AssassinFrames]);
    DrawLaserCombat();
    const CharacterAnimation& animation = player_.Animation();
    if (animation.pose == CharacterPose::Attack && animation.frame == 1 && laserTextures_[KnifeSlash].id != 0) {
        const Vector2 direction = player_.KnifeDirection();
        const Vector2 position = player_.Position();
        const Vector2 slash{position.x + 12 * direction.x - 4 * direction.y,
                            position.y + 12 * direction.y + 4 * direction.x};
        DrawCenteredSprite(laserTextures_[KnifeSlash], slash,
                           std::atan2(direction.y, direction.x) * RAD2DEG, WHITE);
    }
    if (debugVisible_) {
        map_.DrawGrid();
        for (const Guard& guard : guards_) {
            guard.DrawDebug();
        }
    }
    EndMode2D();

    DrawTimer();
    if (missionState_ == Active) {
        DrawAlarm();
        if (activeTerminal_ == NoTerminal) {
            DrawAbilityHud();
        }
    }
    const bool showMinimap = minimapVisible_ && missionState_ == Active && activeTerminal_ == NoTerminal;
    if (showMinimap) {
        DrawMinimap();
    }
    constexpr Color panel{13, 19, 25, 235};
    constexpr int hudY = ScreenHeight - 160;
    DrawRectangle(16, hudY, 278, 144, panel);
    DrawText(TextFormat("HEALTH  %d / %d", player_.Health(), Player::MaxHealth),
             28, hudY + 11, 18, RAYWHITE);
    for (int point = 0; point < Player::MaxHealth; ++point) {
        Color color{57, 64, 74, 255};
        if (point < player_.Health()) {
            color = Color{67, 223, 189, 255};
        }
        DrawRectangle(174 + point * 22, hudY + 13, 15, 14, color);
    }
    DrawText("Objective: Find an escape", 28, hudY + 42, 18, RAYWHITE);
    const int cardCount = static_cast<int>(cards_[0]) + static_cast<int>(cards_[1]);
    DrawText(TextFormat("ACCESS CARDS: %d/2", cardCount), 28, hudY + 68, 18, RAYWHITE);
    const int clueCount = static_cast<int>(clues_[0]) + static_cast<int>(clues_[1]);
    DrawText(TextFormat("CLUES: %d/2", clueCount), 28, hudY + 90, 18, RAYWHITE);
    if (lockdownDisabled_) {
        DrawText("LOCKDOWN DISABLED", 28, hudY + 112, 18, Color{92, 222, 150, 255});
    } else {
        DrawText("LOCKDOWN ACTIVE", 28, hudY + 112, 18, Color{232, 201, 115, 255});
    }
    if (missionState_ == Active) {
        if (activeTerminal_ == NoTerminal && nearbyTerminal_ != NoTerminal) {
            DrawText("E  Use terminal", 320, ScreenHeight - 40, 18, RAYWHITE);
        } else if (activeTerminal_ == NoTerminal && nearbyDoor_ >= 0) {
            const char* prompt = "E  Interact with door";
            if (map_.Door(nearbyDoor_).lockdown) {
                prompt = "E  Use final exit";
            }
            DrawText(prompt, 320, ScreenHeight - 40, 18, RAYWHITE);
        }
        if (accessMessageTime_ > 0.0f) {
            Color messageColor = RAYWHITE;
            if (std::strncmp(accessMessage_, "Access denied", 13) == 0) {
                messageColor = Color{252, 115, 120, 255};
            }
            DrawText(accessMessage_, 320, ScreenHeight - 68, 18, messageColor);
        }
    }
    if (player_.IsExposed() && missionState_ == Active) {
        DrawRectangle(16, hudY - 36, 118, 29, panel);
        DrawText("EXPOSED", 28, hudY - 31, 18, Color{255, 112, 82, 255});
    }

    if (debugVisible_) {
        const Vector2 position = player_.Position();
        const TilePosition tile = TileMap::WorldTile(position);
        const int x = ScreenWidth - 260;
        int y = 16;
        if (showMinimap) {
            y = MinimapMargin + map_.Height() * MinimapTilePixels + 2 * MinimapPadding + 10;
        }
        DrawRectangle(x, y, 244, 123, panel);
        DrawText(TextFormat("FPS: %d", GetFPS()), x + 12, y + 12, 18, RAYWHITE);
        DrawText(TextFormat("World: %.1f, %.1f", position.x, position.y), x + 12, y + 38, 18, RAYWHITE);
        DrawText(TextFormat("Tile: %d, %d", tile.column, tile.row), x + 12, y + 64, 18, RAYWHITE);
        int activeProjectiles = 0;
        for (const Projectile& projectile : projectiles_) {
            if (projectile.active) {
                ++activeProjectiles;
            }
        }
        DrawText(TextFormat("Projectiles: %d", activeProjectiles), x + 12, y + 82, 16, RAYWHITE);
        DrawText(TextFormat("%s %d  %.2fs", animation.Name(), animation.frame + 1, animation.elapsed),
                 x + 12, y + 102, 16, RAYWHITE);
    }

    if (activeTerminal_ != NoTerminal) {
        DrawTerminal();
    }
    if (missionState_ != Active &&
        (animation.pose != CharacterPose::Death || animation.completed)) {
        DrawMissionResult();
    }
    EndDrawing();
}

void Game::DrawMinimap() const {
    const int width = map_.Width() * MinimapTilePixels;
    const int height = map_.Height() * MinimapTilePixels;
    const int x = ScreenWidth - MinimapMargin - MinimapPadding - width;
    const int y = MinimapMargin + MinimapPadding;
    const float pixelsPerUnit = static_cast<float>(MinimapTilePixels) / TileMap::TileSize;
    DrawRectangle(x - MinimapPadding, y - MinimapPadding,
                  width + 2 * MinimapPadding, height + 2 * MinimapPadding, Color{13, 19, 25, 235});
    BeginScissorMode(x, y, width, height);
    DrawRectangle(x, y, width, height, Color{65, 81, 95, 255});

    for (int row = 0; row < map_.Height(); ++row) {
        for (int column = 0; column < map_.Width(); ++column) {
            const int doorIndex = map_.DoorAt({column, row});
            Color color{24, 31, 44, 255};
            if (doorIndex >= 0) {
                const FacilityDoor& door = map_.Door(doorIndex);
                if (door.lockdown) {
                    color = Color{50, 108, 77, 255};
                    if (lockdownDisabled_ && cards_[0] && cards_[1]) {
                        color = Color{114, 255, 151, 255};
                    }
                } else if (door.open) {
                    continue;
                } else {
                    color = Color{244, 88, 94, 255};
                }
            } else if (map_.HasCrate(column, row)) {
                color = Color{176, 121, 75, 255};
            } else if (!map_.IsWall(column, row)) {
                continue;
            }
            DrawRectangle(x + column * MinimapTilePixels, y + row * MinimapTilePixels,
                          MinimapTilePixels, MinimapTilePixels, color);
        }
    }

    // Convert the camera's screen bounds to world units, then to minimap pixels.
    const float left = std::clamp((camera_.target.x - camera_.offset.x / camera_.zoom) * pixelsPerUnit,
                                  0.0f, static_cast<float>(width));
    const float right = std::clamp((camera_.target.x + (ScreenWidth - camera_.offset.x) / camera_.zoom) *
                                   pixelsPerUnit, 0.0f, static_cast<float>(width));
    const float top = std::clamp((camera_.target.y - camera_.offset.y / camera_.zoom) * pixelsPerUnit,
                                 0.0f, static_cast<float>(height));
    const float bottom = std::clamp((camera_.target.y + (ScreenHeight - camera_.offset.y) / camera_.zoom) *
                                    pixelsPerUnit, 0.0f, static_cast<float>(height));
    DrawRectangleLinesEx({x + left, y + top, right - left, bottom - top}, 1.0f, Color{178, 202, 222, 255});

    for (const FacilityObject& object : objects_) {
        Color color{};
        if (object.type == Card && !cards_[CardIndex(object.tile)]) {
            color = Color{255, 216, 95, 255};
        } else if (object.type == Terminal) {
            color = Color{75, 218, 235, 255};
        } else {
            continue;
        }
        const Vector2 center = TileMap::TileCenter(object.tile);
        DrawCircleV({x + center.x * pixelsPerUnit, y + center.y * pixelsPerUnit}, 2.0f, color);
    }
    for (int index = 0; index < AbilityCount; ++index) {
        if (!abilityPickupCollected_[index]) {
            const Vector2 center = TileMap::TileCenter(AbilityTiles[index]);
            DrawAbilityPickup(static_cast<AbilityType>(index),
                              {x + center.x * pixelsPerUnit, y + center.y * pixelsPerUnit}, 2.5f);
        }
    }
    if (debugVisible_) {
        for (const Guard& guard : guards_) {
            if (guard.IsAlive()) {
                const Vector2 position = guard.Position();
                DrawCircleV({x + position.x * pixelsPerUnit, y + position.y * pixelsPerUnit},
                            2.0f, Color{255, 139, 124, 255});
            }
        }
    }

    const Vector2 position = player_.Position();
    const Vector2 facing = player_.Facing();
    const Vector2 marker{x + position.x * pixelsPerUnit, y + position.y * pixelsPerUnit};
    DrawCircleV(marker, 3.0f, WHITE);
    DrawLineEx(marker, {marker.x + facing.x * 7.0f, marker.y + facing.y * 7.0f}, 2.0f, WHITE);
    EndScissorMode();
}

int Game::SecondsRemaining() const {
    // Show 00:01 while any fraction of the last second remains.
    return static_cast<int>(std::ceil(timeRemaining_));
}

void Game::CollectAbilityPickups() {
    const Vector2 position = player_.Position();
    for (int index = 0; index < AbilityCount; ++index) {
        if (abilityPickupCollected_[index]) {
            continue;
        }
        const Vector2 center = TileMap::TileCenter(AbilityTiles[index]);
        if (std::hypot(center.x - position.x, center.y - position.y) <= 24.0f &&
            map_.HasClearLine(position, center) &&
            player_.AddAbilityCharge(static_cast<AbilityType>(index))) {
            abilityPickupCollected_[index] = true;
        }
    }
}

void Game::DrawAbilityPickup(AbilityType type, Vector2 center, float size) const {
    if (type == Cloak) {
        constexpr Color teal{83, 249, 176, 255};
        DrawTriangle({center.x, center.y - size}, {center.x - size, center.y},
                     {center.x, center.y + size}, teal);
        DrawTriangle({center.x, center.y - size}, {center.x, center.y + size},
                     {center.x + size, center.y}, teal);
    } else {
        constexpr Color yellow{255, 237, 100, 255};
        DrawTriangle({center.x + size * 0.3f, center.y - size},
                     {center.x - size * 0.8f, center.y + size * 0.2f},
                     {center.x + size * 0.6f, center.y + size * 0.2f}, yellow);
        DrawTriangle({center.x - size * 0.6f, center.y - size * 0.1f},
                     {center.x - size * 0.3f, center.y + size},
                     {center.x + size * 0.8f, center.y - size * 0.1f}, yellow);
    }
}

void Game::DrawAbilityHud() const {
    const char* names[]{"Q  Invisibility", "L-SHIFT  Speed Boost"};
    const Color accents[]{Color{83, 249, 176, 255}, Color{255, 237, 100, 255}};
    constexpr int y = ScreenHeight - 80;
    for (int index = 0; index < AbilityCount; ++index) {
        const int x = ScreenWidth - 428 + 210 * index;
        const PlayerAbility& ability = player_.Ability(static_cast<AbilityType>(index));
        DrawRectangle(x, y, 202, 64, Color{13, 19, 25, 235});
        DrawText(names[index], x + 10, y + 8, 17, accents[index]);
        if (ability.Active()) {
            DrawText(TextFormat("%d/%d charges  |  %.1fs", ability.charges,
                                Player::MaxAbilityCharges, ability.remaining), x + 10, y + 30, 15, RAYWHITE);
            DrawRectangle(x + 10, y + 51, 182, 4, Color{58, 70, 82, 255});
            DrawRectangle(x + 10, y + 51, static_cast<int>(182 * ability.remaining / ability.duration),
                          4, accents[index]);
        } else {
            const char* status = ability.charges > 0 ? "Ready" : "Depleted";
            DrawText(TextFormat("%d/%d charges  |  %s", ability.charges, Player::MaxAbilityCharges, status),
                     x + 10, y + 30, 15, RAYWHITE);
        }
    }
}

void Game::DrawTimer() const {
    Color color = RAYWHITE;
    if (timeRemaining_ <= 30.0) {
        color = Color{252, 115, 120, 255};
    } else if (timeRemaining_ <= 60.0) {
        color = Color{245, 211, 119, 255};
    }
    const int seconds = SecondsRemaining();
    const char* time = TextFormat("%02d:%02d", seconds / 60, seconds % 60);
    DrawRectangle(ScreenWidth / 2 - 64, 16, 128, 42, Color{13, 19, 25, 235});
    DrawText(time, (ScreenWidth - MeasureText(time, 26)) / 2, 24, 26, color);
}

void Game::DrawAlarm() const {
    const double elapsed = MissionDuration - timeRemaining_;
    const bool showNotice = elapsed < alarmNoticeUntil_;
    if (!alarmActive_ && !showNotice) {
        return;
    }
    constexpr int x = ScreenWidth / 2 - 145;
    DrawRectangle(x, 65, 290, 77, Color{13, 19, 25, 235});
    if (alarmActive_) {
        DrawText("SECURITY ALERT", x + 14, 74, 18, Color{252, 115, 120, 255});
        if (reinforcementTime_ >= 0.0) {
            const int seconds = static_cast<int>(std::ceil(reinforcementTime_ - elapsed));
            DrawText(TextFormat("Reinforcement in %ds", seconds), x + 14, 96, 16, RAYWHITE);
        }
    }
    if (showNotice) {
        DrawText(alarmNotice_, x + 14, 119, 16, Color{245, 211, 119, 255});
    }
}

void Game::DrawMissionResult() const {
    const int seconds = SecondsRemaining();
    const int elapsed = MissionDuration - seconds;
    int eliminated = 0;
    for (const Guard& guard : guards_) {
        if (!guard.IsAlive()) {
            ++eliminated;
        }
    }
    const int cardCount = static_cast<int>(cards_[0]) + static_cast<int>(cards_[1]);
    constexpr Color teal{82, 225, 207, 255};
    DrawRectangle(0, 0, ScreenWidth, ScreenHeight, Color{13, 19, 25, 225});
    DrawRectangle(360, 124, 560, 470, Color{20, 29, 43, 255});
    if (missionState_ == Victory) {
        DrawText("MISSION COMPLETE", 400, 154, 38, teal);
        DrawText("You escaped Nexus-7.", 400, 213, 23, RAYWHITE);
        DrawText("PROTOCOL ZERO OVERRIDDEN", 400, 249, 20, Color{92, 222, 150, 255});
    } else {
        DrawText("MISSION FAILED", 400, 154, 38, Color{252, 115, 120, 255});
        const char* cause = "ASSASSIN ELIMINATED";
        if (defeatReason_ == TimeExpired) {
            cause = "TIME EXPIRED";
        }
        DrawText(cause, 400, 213, 23, RAYWHITE);
    }
    DrawText(TextFormat("Time remaining   %02d:%02d", seconds / 60, seconds % 60), 400, 305, 20, RAYWHITE);
    DrawText(TextFormat("Time taken       %02d:%02d", elapsed / 60, elapsed % 60), 400, 337, 20, RAYWHITE);
    DrawText(TextFormat("Guards eliminated   %d/%d", eliminated, static_cast<int>(guards_.size())),
             400, 369, 20, RAYWHITE);
    DrawText(TextFormat("Health remaining    %d/%d", player_.Health(), Player::MaxHealth), 400, 401, 20, RAYWHITE);
    DrawText(TextFormat("Access cards        %d/2", cardCount), 400, 433, 20, RAYWHITE);
    const char* controls = "R  Retry     ESC  Quit";
    if (missionState_ == Victory) {
        controls = "R  Restart     ESC  Quit";
    }
    DrawText(controls, 400, 526, 20, teal);
}

void Game::DrawTerminal() const {
    constexpr int x = 260;
    constexpr int y = 195;
    constexpr Color teal{82, 225, 207, 255};
    constexpr Color yellow{245, 211, 119, 255};
    DrawRectangle(x, y, 760, 300, Color{20, 29, 43, 250});
    DrawRectangle(x, y, 760, 3, teal);

    if (activeTerminal_ == Servers) {
        DrawText("RECOVERED SECURITY LOG", x + 32, y + 30, 28, teal);
        DrawText("The first two digits of the override code correspond", x + 32, y + 92, 20, RAYWHITE);
        DrawText("to the station identification number.", x + 32, y + 120, 20, RAYWHITE);
        DrawText("NEXUS-7", x + 32, y + 178, 24, yellow);
        DrawText("Station ID: 07", x + 32, y + 212, 20, yellow);
    } else if (activeTerminal_ == Security) {
        DrawText("CLASSIFIED EXPERIMENT RECORD", x + 32, y + 30, 28, teal);
        DrawText("The final two digits of the override code correspond", x + 32, y + 92, 20, RAYWHITE);
        DrawText("to the classified experiment number.", x + 32, y + 120, 20, RAYWHITE);
        DrawText("EXPERIMENT 42", x + 32, y + 184, 24, yellow);
    } else {
        DrawText("CONTROL TERMINAL", x + 32, y + 30, 28, teal);
        if (lockdownDisabled_) {
            DrawText("PROTOCOL ZERO OVERRIDDEN", x + 32, y + 110, 30, Color{92, 222, 150, 255});
            DrawText("Reach the exit.", x + 32, y + 165, 20, RAYWHITE);
        } else {
            DrawText("Enter the four-digit lockdown override.", x + 32, y + 90, 20, yellow);
            char display[5] = "____";
            for (int i = 0; i < passwordLength_; ++i) {
                display[i] = password_[i];
            }
            DrawText(display, x + 32, y + 128, 36, RAYWHITE);
            if (passwordDenied_) {
                DrawText("ACCESS DENIED", x + 32, y + 180, 20, Color{252, 115, 120, 255});
            }
            DrawText("0-9  Enter digits    BACKSPACE  Delete    ENTER  Submit",
                     x + 32, y + 222, 18, RAYWHITE);
        }
    }
    DrawText("ESC  Close terminal", x + 32, y + 266, 18, teal);
}

void Game::Shutdown() {
    for (const CharacterFrames& frames : animationFrames_) {
        for (const Texture2D texture : frames.walk) {
            if (texture.id != 0) UnloadTexture(texture);
        }
        for (const Texture2D texture : frames.attack) {
            if (texture.id != 0) UnloadTexture(texture);
        }
        for (const Texture2D texture : frames.death) {
            if (texture.id != 0) UnloadTexture(texture);
        }
    }
    for (const Texture2D texture : laserTextures_) {
        if (texture.id != 0) {
            UnloadTexture(texture);
        }
    }
    for (const Texture2D texture : sprites_) {
        if (texture.id != 0) {
            UnloadTexture(texture);
        }
    }
    if (facilityTexture_.id != 0) {
        UnloadTexture(facilityTexture_);
    }
    CloseWindow();
}
