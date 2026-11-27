#pragma once

#include "raylib.h"

// Rotate about the world-space center and keep the PNG's native dimensions.
inline void DrawCenteredSprite(Texture2D texture, Vector2 center, float degrees, Color tint) {
    const float width = static_cast<float>(texture.width);
    const float height = static_cast<float>(texture.height);
    DrawTexturePro(texture, {0, 0, width, height}, {center.x, center.y, width, height},
                   {width / 2.0f, height / 2.0f}, degrees, tint);
}
