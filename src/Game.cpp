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
}

void Game::DrawFacility() const {
    DrawTexture(facilityTexture_, 0, 0, WHITE);
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
    guards_.reserve(GuardCount);
    int guardIndex = 0;
    for (const FacilityObject& object : objects_) {
        if (object.type == Assassin) {
            player_.Spawn(TileMap::TileCenter(object.tile), object.rotation);
        } else if (object.type == GuardSpawn) {
            const TilePosition* route = waypoints[guardIndex++];
            guards_.emplace_back(map_, pathfinder_,
                std::initializer_list<TilePosition>{object.tile, route[0], route[1], route[2]});
            guards_.back().SetFacing(object.rotation);
        }
    }
    knifeCooldown_ = 0.0f;
    knifeEffect_ = 0.0f;
    camera_.target = ClampCameraTarget(player_.Position());
}

void Game::EndMission(MissionState result) {
    missionState_ = result;
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
        }
        return;
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
    player_.Update(deltaTime, map_, !terminalWasOpen);
    if (!terminalWasOpen) {
        UpdateAccess(deltaTime);
    } else if (!escape) {
        UpdateTerminal();
    }
    if (knifeCooldown_ > 0.0f) {
        knifeCooldown_ -= deltaTime;
    }
    if (knifeEffect_ > 0.0f) {
        knifeEffect_ -= deltaTime;
    }
    // Resolve the knife first so a guard killed this frame cannot retaliate.
    if (!terminalWasOpen && activeTerminal_ == NoTerminal &&
        IsKeyPressed(KEY_SPACE) && knifeCooldown_ <= 0.0f) {
        AttackWithKnife();
    }

    bool detected = false;
    for (Guard& guard : guards_) {
        guard.Update(deltaTime, player_.Position(), map_, pathfinder_);
        if (guard.IsChasing()) {
            detected = true;
        }
        if (guard.TryAttack(player_.Position(), map_)) {
            const int previousHealth = player_.Health();
            player_.TakeDamage();
            if (player_.Health() < previousHealth && activeTerminal_ != NoTerminal) {
                CloseTerminal();
            }
            if (player_.Health() == 0) {
                EndMission(Defeat);
                return;
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
}

// Scan once for the nearest guard the knife can actually reach.
void Game::AttackWithKnife() {
    knifeCooldown_ = KnifeCooldown;
    knifeEffect_ = 0.16f;
    knifeOrigin_ = player_.Position();
    knifeTarget_ = knifeOrigin_;
    Guard* closest = nullptr;
    float closestDistance = KnifeRange;

    for (Guard& guard : guards_) {
        if (!guard.IsAlive()) {
            continue;
        }
        const Vector2 position = guard.Position();
        const float distance = std::hypot(position.x - knifeOrigin_.x, position.y - knifeOrigin_.y);
        if (distance <= KnifeRange && (closest == nullptr || distance < closestDistance) &&
            map_.HasClearLine(knifeOrigin_, position)) {
            closest = &guard;
            closestDistance = distance;
        }
    }
    if (closest != nullptr) {
        knifeTarget_ = closest->Position();
        closest->TakeKnifeHit(knifeOrigin_);
    }
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
    for (const Guard& guard : guards_) {
        guard.DrawVision(map_);
    }
    if (debugVisible_) {
        map_.DrawGrid();
        for (const Guard& guard : guards_) {
            guard.DrawDebug();
        }
    }
    for (const Guard& guard : guards_) {
        guard.Draw(sprites_[GuardSpawn]);
    }
    player_.Draw(sprites_[Assassin]);
    if (knifeEffect_ > 0.0f) {
        const Color slash{255, 225, 139, 255};
        DrawCircleLines(static_cast<int>(knifeOrigin_.x), static_cast<int>(knifeOrigin_.y), 24.0f, slash);
        DrawLineEx(knifeOrigin_, knifeTarget_, 3.0f, slash);
    }
    EndMode2D();

    DrawTimer();
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
        DrawRectangle(x, 16, 244, 101, panel);
        DrawText(TextFormat("FPS: %d", GetFPS()), x + 12, 28, 18, RAYWHITE);
        DrawText(TextFormat("World: %.1f, %.1f", position.x, position.y), x + 12, 54, 18, RAYWHITE);
        DrawText(TextFormat("Tile: %d, %d", tile.column, tile.row), x + 12, 80, 18, RAYWHITE);
    }

    if (activeTerminal_ != NoTerminal) {
        DrawTerminal();
    }
    if (missionState_ != Active) {
        DrawMissionResult();
    }
    EndDrawing();
}

int Game::SecondsRemaining() const {
    // Show 00:01 while any fraction of the last second remains.
    return static_cast<int>(std::ceil(timeRemaining_));
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
    DrawText(TextFormat("Guards eliminated   %d/%d", eliminated, GuardCount), 400, 369, 20, RAYWHITE);
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
