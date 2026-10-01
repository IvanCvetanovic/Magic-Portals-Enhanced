#pragma once

// ETHFramework's Notificator, which the original builds once in main and updates
// and draws after every state's loop, on every screen: the "Achievement unlocked!"
// card an unlock raises and the "This level is locked!" card the dashboard's
// locked rows raise (game/data/achievements.json `notificator`, `card`,
// `locked_card`; planning step 143).
//
// Pure, like Popup.hpp: a Queue steps on the clock it is handed and says what
// happened - a card shown, which plays its sound and turns the music down, and a
// card gone, which turns it back - and LayOut places the first card's pieces for
// a view, given its text's size in font pixels, which only the caller's font can
// measure. The layer owns the clock, the sounds, the music and the drawing.
//
// Units are design units on the VIEW, (0, 0) its top-left, +y down; the
// original's scale(N) is N units.

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "sim/Hud.hpp"

namespace MagicPortals::Notificator {

struct Rules {
    glm::dvec2 normalizedPos{0.5, 1.0};
    glm::dvec2 origin{0.5, 1.0};
    std::string font;
    double unitsPerFontPx = 0.0;
    glm::dvec2 textOffsetUnits{0.0};
    glm::dvec2 iconUnits{0.0};
    glm::dvec2 backgroundUnits{0.0}; // the background's whole picture
    int backgroundColumns = 1;
    double effectMsCap = 0.0;
    double fadeMsCap = 0.0;
    double musicVolumeShown = 0.0;
    double musicVolumeDefaultFirstTrack = 0.0;
    double musicVolumeDefaultOther = 0.0;
    std::string musicFirstTrack; // sounds.json's name for g_musicNames[0]
    // Interpolator's elastic (bytes 314142..314380), gravitywell.json's numbers.
    double elasticPeriod = 0.0;
    double elasticShiftDivisor = 0.0;
    double elasticDecay = 0.0;
};

// achievements.json's `notificator`, and the elastic from gravitywell.json's
// `roundabout`, strictly.
bool LoadRules(const std::string& achievementsPath, const std::string& wellsPath, Rules& out, std::string& error);

// A Notification.
struct Card {
    std::string background;
    std::string message;
    std::string icon;  // empty for none
    std::string sound; // a sounds.json event; empty for none
    double durationMs = 0.0;
    double delayMs = 0.0;
    double insertionMs = 0.0; // on the queue's clock
    int id = -1;              // the achievement it is for, or -1
};

// What a step did, in the order it did it.
struct Event {
    enum class Kind { Shown, Dismissed };
    Kind kind = Kind::Shown;
    Card card;
};

class Queue {
public:
    // notify: onto the stack, its time starting now.
    void Notify(Card card);

    // update: the clock on by `dtMs`; then updateStack - the first card on the
    // stack whose delay has passed goes to the shown list, at most one a step;
    // then the first shown card goes once its duration has passed, and every card
    // behind it starts its time again.
    std::vector<Event> Update(double dtMs);

    // The card drawn - the first shown - and how long it has been shown.
    const Card* Front() const { return m_shown.empty() ? nullptr : &m_shown.front(); }
    double FrontAgeMs() const { return m_shown.empty() ? 0.0 : m_elapsedMs - m_shown.front().insertionMs; }
    double ElapsedMs() const { return m_elapsedMs; }
    std::size_t Stacked() const { return m_stack.size(); }
    std::size_t ShownCount() const { return m_shown.size(); }

private:
    double m_elapsedMs = 0.0;
    std::vector<Card> m_stack;
    std::vector<Card> m_shown;
};

// Interpolator's elastic (bytes 314142..314380) on these rules' numbers.
double Elastic(const Rules& rules, double n);

// computeEffectScale: the card's scale and alpha `ageMs` into its time.
struct Effect {
    double scale = 1.0;
    double alpha = 1.0;
};
Effect EffectOf(const Rules& rules, const Card& card, double ageMs);

// The card's pieces as draw places them.
struct Layout {
    Hud::Rect pieces[3];             // the background's three columns, left to right
    glm::dvec2 uvMin[3];
    glm::dvec2 uvMax[3];
    glm::dvec2 textCentre{0.0};      // drawCenteredText's point
    double unitsPerFontPx = 0.0;
    bool hasIcon = false;
    Hud::Rect icon;
    double alpha = 1.0;
};

// `textFontPx` is ComputeTextBoxSize of the card's message: the widest line's
// advances by its lines' height, in font pixels.
Layout LayOut(const Rules& rules, const Card& card, double ageMs, const glm::dvec2& viewUnits,
              const glm::dvec2& textFontPx);

// ETHFramework's TextBoxFrame over an ImageFrame with borderOutside, the frame the
// pause's AchievementsPopup draws its text in: the text box ComputeTextBoxSize x
// `unitsPerFontPx`, its top-left `at` less box x `origin`; the tiles cut
// columns x rows, a tile its frame x `tileScale`, the corners outside the box, the
// edges stretched along it and the middle over it (TextBoxFrame::draw
// 36781..37291, ImageFrame::draw 34075..35953).
struct Box {
    Hud::Rect pieces[9]; // in SetSpriteRect's order: the top row, the middle, the bottom
    glm::dvec2 uvMin[9];
    glm::dvec2 uvMax[9];
    glm::dvec2 textTopLeft{0.0};
};
Box LayOutBox(const glm::dvec2& tileUnits, int columns, int rows, double tileScale, double unitsPerFontPx,
              const glm::dvec2& at, const glm::dvec2& origin, const glm::dvec2& textFontPx);

} // namespace MagicPortals::Notificator
