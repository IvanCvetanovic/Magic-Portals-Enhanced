#include "sim/Splash.hpp"

#include <algorithm>
#include <cmath>

namespace MagicPortals::Splash {

namespace {

// Smoothstep: 0 at 0, 1 at 1, zero slope at both, monotone between.
float Ease(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

} // namespace

// ---- The timeline -------------------------------------------------------------------------------------------

float Alpha(const unsigned tick) {
    if (tick >= kTotalTicks) return 0.0f;
    if (tick < kFadeInTicks) return Ease(static_cast<float>(tick) / static_cast<float>(kFadeInTicks));
    if (tick <= kFadeOutStart) return 1.0f;
    return 1.0f - Ease(static_cast<float>(tick - kFadeOutStart) / static_cast<float>(kFadeOutTicks));
}

bool Done(const unsigned tick) { return tick >= kTotalTicks; }

void Clock::Step(const bool pressed) {
    if (Done()) return;
    // The tick that is running is numbered m_ticks; Ticks() counts it once this returns. A press during the fade-out of the timeline itself is no skip:
    // the intro is ending already and a skip would only lengthen it.
    if (pressed && !m_skipTick && m_ticks >= kSkipFromTick && m_ticks < kFadeOutStart) m_skipTick = m_ticks;
    ++m_ticks;
}

bool Clock::Done() const {
    if (m_skipTick) return m_ticks - *m_skipTick >= kFadeOutTicks;
    return Splash::Done(m_ticks);
}

float Clock::Alpha() const {
    if (!m_skipTick) return Splash::Alpha(m_ticks);
    // From what was on the screen when the press came, down to nothing.
    const float start = Splash::Alpha(*m_skipTick);
    return start * (1.0f - Ease(static_cast<float>(m_ticks - *m_skipTick) / static_cast<float>(kFadeOutTicks)));
}

// ---- What counts as a press ---------------------------------------------------------------------------------

Held HeldOf(const Devices& devices) {
    Held held;
    for (std::size_t k = 0; k < devices.keys.size(); ++k) {
        if (devices.keys[k]) held.set(kKeyBase + k);
    }
    for (std::size_t b = 0; b < devices.mouse.size(); ++b) {
        if (devices.mouse[b]) held.set(kMouseBase + b);
    }
    for (const int id : devices.contactIds) {
        if (id >= 0) held.set(kTouchBase + static_cast<std::size_t>(id % 8));
    }
    for (std::size_t p = 0; p < devices.pads.size() && p < kPads; ++p) {
        const std::size_t base = kPadBase + p * kPadBits;
        for (std::size_t b = 0; b < devices.pads[p].size(); ++b) {
            if (devices.pads[p][b]) held.set(base + b);
        }
    }
    return held;
}

bool PressWatch::Observe(const Held& held) {
    const bool pressed = m_have && (held & ~m_held).any();
    m_held = held;
    m_have = true;
    return pressed;
}

// ---- The picture --------------------------------------------------------------------------------------------

Layout ComputeLayout(const glm::uvec2& window, const Supersonic::SafeAreaInsets& safe, const glm::vec2& image) {
    Layout layout;
    if (window.x == 0 || window.y == 0 || !(image.x > 0.0f) || !(image.y > 0.0f)) return layout;

    const glm::vec2 whole(window);
    glm::vec2 areaMin(std::max(safe.left, 0.0f), std::max(safe.top, 0.0f));
    glm::vec2 areaMax(whole.x - std::max(safe.right, 0.0f), whole.y - std::max(safe.bottom, 0.0f));
    // Insets that leave nothing are not believed: the window is the area.
    if (!(areaMax.x - areaMin.x >= 1.0f) || !(areaMax.y - areaMin.y >= 1.0f)) {
        areaMin = glm::vec2(0.0f);
        areaMax = whole;
    }
    const glm::vec2 area = areaMax - areaMin;

    const float aspect = image.x / image.y;
    // 70% of the window's width, at most half its height, and inside the safe area.
    float width = std::min({whole.x * kWidthFraction, whole.y * kHeightFraction * aspect, area.x, area.y * aspect});
    width = std::max(1.0f, std::round(width));
    const float height = std::max(1.0f, std::round(width / aspect));

    layout.size = glm::vec2(width, height);
    const glm::vec2 centred = areaMin + (area - layout.size) * 0.5f;
    layout.pos = glm::vec2(std::round(centred.x), std::round(centred.y));
    return layout;
}

std::vector<Supersonic::ScreenOverlay::Quad> BuildQuads(const glm::uvec2& window, const Layout& layout, const float alpha, const std::string& logoTexture) {
    using Quad = Supersonic::ScreenOverlay::Quad;
    std::vector<Quad> quads;
    if (window.x == 0 || window.y == 0) return quads;

    Quad ground; // the whole image, untextured
    ground.min = glm::vec2(0.0f, 0.0f);
    ground.max = glm::vec2(1.0f, 1.0f);
    ground.clampToEdge = true;
    ground.color = glm::vec4(static_cast<float>(kGround[0]), static_cast<float>(kGround[1]), static_cast<float>(kGround[2]), 255.0f) / 255.0f;
    quads.push_back(std::move(ground));

    const float opacity = std::clamp(alpha, 0.0f, 1.0f);
    if (logoTexture.empty() || !(opacity > 0.0f) || !(layout.size.x > 0.0f) || !(layout.size.y > 0.0f)) return quads;

    const glm::vec2 whole(window);
    Quad logo;
    logo.min = layout.pos / whole;
    logo.max = (layout.pos + layout.size) / whole;
    logo.color = glm::vec4(1.0f, 1.0f, 1.0f, opacity);
    logo.texture = logoTexture;
    logo.clampToEdge = true;
    quads.push_back(std::move(logo));
    return quads;
}

// ---- Who plays it ----------------------------------------------------------------------------------------------

bool Wanted(const bool attended, const bool directStart, const std::optional<bool> forced) {
    return forced.value_or(attended && !directStart);
}

} // namespace MagicPortals::Splash
