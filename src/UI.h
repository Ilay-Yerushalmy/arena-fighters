#pragma once

#include <string>
#include <vector>

#include "raylib.h"

// Animated notification card (used for post-match stat pop-ups).
struct Popup {
    std::string title;
    std::vector<std::string> lines;
    Color accent = GOLD;
    float age = 0.0f;
    float delay = 0.0f;  // seconds before the card starts sliding in
};

namespace ui {

void drawCenteredText(const std::string& text, int y, int fontSize, Color color);
void drawBar(int x, int y, int width, int height, float fraction, Color fill, Color background);
void drawPopup(const Popup& popup, float x, float y, float width);

}  // namespace ui
