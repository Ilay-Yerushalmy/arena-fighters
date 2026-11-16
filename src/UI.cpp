#include "UI.h"

#include <algorithm>
#include <cmath>

#include "Config.h"

namespace ui {

void drawCenteredText(const std::string& text, int y, int fontSize, Color color) {
    const int w = MeasureText(text.c_str(), fontSize);
    DrawText(text.c_str(), (config::kScreenWidth - w) / 2, y, fontSize, color);
}

void drawBar(int x, int y, int width, int height, float fraction, Color fill, Color background) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    DrawRectangle(x, y, width, height, background);
    DrawRectangle(x, y, static_cast<int>(width * fraction), height, fill);
    DrawRectangleLines(x, y, width, height, BLACK);
}

void drawPopup(const Popup& popup, float x, float y, float width) {
    const float t = std::clamp((popup.age - popup.delay) / 0.4f, 0.0f, 1.0f);
    if (t <= 0.0f) return;
    const float eased = 1.0f - std::pow(1.0f - t, 3.0f);  // ease-out cubic

    const float lineHeight = 28.0f;
    const float height = 60.0f + lineHeight * static_cast<float>(popup.lines.size());
    const float offsetY = (1.0f - eased) * 80.0f;
    const Rectangle card{x, y + offsetY, width, height};

    DrawRectangleRounded(card, 0.12f, 8, Fade(Color{25, 25, 35, 255}, 0.92f * eased));
    DrawRectangleRoundedLines(card, 0.12f, 8, 2.0f, Fade(popup.accent, eased));
    DrawText(popup.title.c_str(), static_cast<int>(x + 18), static_cast<int>(card.y + 14), 26,
             Fade(popup.accent, eased));

    for (size_t i = 0; i < popup.lines.size(); ++i) {
        DrawText(popup.lines[i].c_str(), static_cast<int>(x + 18),
                 static_cast<int>(card.y + 52 + lineHeight * static_cast<float>(i)), 20, Fade(RAYWHITE, eased));
    }
}

}  // namespace ui
