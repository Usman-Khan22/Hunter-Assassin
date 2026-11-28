#pragma once

#include "raylib.h"

#include <cmath>

enum class CharacterPose { Idle, Walk, Attack, Charge, Shoot, Death };

struct CharacterFrames {
    Texture2D walk[3]{};
    Texture2D attack[3]{};
    Texture2D death[2]{};
};

struct CharacterAnimation {
    CharacterPose pose = CharacterPose::Idle;
    int frame = 0;
    float elapsed = 0.0f;
    bool completed = false;

    void Start(CharacterPose next) {
        pose = next;
        frame = 0;
        elapsed = 0.0f;
        completed = false;
        if (pose == CharacterPose::Shoot) {
            frame = 1;
        }
    }

    void Advance(float deltaTime, float attackFrameTime) {
        if (deltaTime <= 0.0f || completed || pose == CharacterPose::Idle) {
            return;
        }
        if (pose == CharacterPose::Walk) {
            constexpr float step = 0.11f;
            constexpr int order[]{0, 1, 2, 1};
            elapsed = std::fmod(elapsed + std::fmod(deltaTime, 4 * step), 4 * step);
            int index = static_cast<int>(elapsed / step);
            if (index > 3) {
                index = 3;
            }
            frame = order[index];
            return;
        }
        if (pose == CharacterPose::Charge) {
            elapsed += deltaTime;
            return;
        }

        elapsed += deltaTime;
        if (pose == CharacterPose::Shoot) {
            frame = elapsed < 0.08f ? 1 : 2;
            completed = elapsed >= 0.23f;
            return;
        }
        int frameCount = 3;
        float step = attackFrameTime;
        if (pose == CharacterPose::Death) {
            frameCount = 2;
            step = 0.18f;
        }
        if (elapsed >= frameCount * step) {
            frame = frameCount - 1;
            completed = true;
        } else {
            frame = static_cast<int>(elapsed / step);
        }
    }

    void SetMovement(bool moved) {
        if (pose == CharacterPose::Death ||
            ((pose == CharacterPose::Attack || pose == CharacterPose::Shoot) && !completed) ||
            pose == CharacterPose::Charge) {
            return;
        }
        const CharacterPose next = moved ? CharacterPose::Walk : CharacterPose::Idle;
        if (pose != next) {
            Start(next);
        }
    }

    Texture2D Texture(const CharacterFrames& frames, Texture2D idle) const {
        Texture2D selected{};
        if (pose == CharacterPose::Walk) {
            selected = frames.walk[frame];
        } else if (pose == CharacterPose::Death) {
            selected = frames.death[frame];
        } else if (pose != CharacterPose::Idle) {
            selected = frames.attack[frame];
        }
        if (selected.id == 0) {
            return idle;
        }
        return selected;
    }

    const char* Name() const {
        switch (pose) {
            case CharacterPose::Walk: return "Walk";
            case CharacterPose::Attack: return "Attack";
            case CharacterPose::Charge: return "Charge";
            case CharacterPose::Shoot: return "Shoot";
            case CharacterPose::Death: return "Death";
            default: return "Idle";
        }
    }
};
