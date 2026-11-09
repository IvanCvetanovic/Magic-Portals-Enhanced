// Arabic for a font that lays code points out from left to right (planning step 237, PORT-33): sim/ArabicShaping, and Strings::Display's
// single choke point over it.
//
// The cases of the first group are Penumbra Enhanced's own (tests/test_pn_render_hud.cpp TestArabicShaping, ENHANCEMENT E24: the module is
// Ivan's code adapted from there), worked by hand from Unicode's ArabicShaping.txt and UAX #9: if one fails here the port of the module is
// wrong. The second group is this game's - its chapter lines, level names "1-2", percent signs and Latin proper nouns inside Arabic, as the
// shipped strings of game/data/lang/ar.json spell them - with the expected forms derived by hand in the comments, one letter at a time.
//
//   shaping      each joining class (dual, right, transparent mark, tatweel, hamza), the four lam-alef ligatures, a Latin run left alone
//   direction    a word reversed; a Latin key name, a number, a time, brackets (Latin in Arabic, Arabic in brackets), a switch marker, a line
//                with no letter at all as it is; a left-to-right paragraph with Arabic in it; lines kept apart
//   this game    the chapter line ("Chapter 1": dashboard.chapter), the level name "Level 1-2" (the ONE deviation from UAX #9, pinned both ways: the
//                hyphen between two numbers joins them, and a hyphen between a number and a word still does not), "requires 60%" (selector.requires),
//                "from Asantee Games" (a Latin proper noun inside Arabic), a placeholder filled before display
//   Display      Strings::Display(Ar, ...) is exactly the shaper over UTF-8; for every other language it is the identity BYTE FOR BYTE (a
//                malformed byte and Arabic letters in a German string included); it is NOT idempotent, which is why it is applied once
//   invariants   a seeded fuzz: the output is a permutation of the shaped input with only brackets mirrored, line breaks stay put, a text
//                with no Arabic in a left-to-right paragraph is unchanged, nothing is read past the end
//
// No non-ASCII character is written in this file: MSVC is built without /utf-8 here, so every character is a \u escape.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/ArabicShaping.hpp"
#include "sim/Strings.hpp"
#include "sim/Utf8.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace MagicPortals;
using ArabicShaping::Shape;
using ArabicShaping::Visual;
using ArabicShaping::VisualLine;

namespace {

std::string Show(const std::u32string& text) {
    std::string out;
    for (const char32_t c : text) {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%04X ", static_cast<unsigned>(c));
        out += buffer;
    }
    return out;
}

#define CHECK_U32(got, want) CHECK_MSG((got) == (want), std::string(#got) + " is [" + Show(got) + "] and not [" + Show(want) + "]")

void PenumbrasShapingCases() {
    // Beh yeh teh: initial, medial, final.
    CHECK_U32(Shape(U"\u0628\u064A\u062A"), U"\uFE91\uFEF4\uFE96");
    // A letter alone, and one that joins nothing after it (alef): isolated.
    CHECK_U32(Shape(U"\u0628"), U"\uFE8F");
    CHECK_U32(Shape(U"\u0627\u0628"), U"\uFE8D\uFE8F");
    // Seen lam alef meem: lam-alef final after the seen; meem isolated after it.
    CHECK_U32(Shape(U"\u0633\u0644\u0627\u0645"), U"\uFEB3\uFEFC\uFEE1");
    // Lam-alef alone, and each alef it takes.
    CHECK_U32(Shape(U"\u0644\u0627"), U"\uFEFB");
    CHECK_U32(Shape(U"\u0644\u0622"), U"\uFEF5");
    CHECK_U32(Shape(U"\u0644\u0623"), U"\uFEF7");
    CHECK_U32(Shape(U"\u0644\u0625"), U"\uFEF9");
    // Harakat are transparent: the letters around one still join.
    CHECK_U32(Shape(U"\u0628\u064E\u062A"), U"\uFE91\u064E\uFE96");
    // Tatweel joins both ways.
    CHECK_U32(Shape(U"\u0628\u0640\u062A"), U"\uFE91\u0640\uFE96");
    // Teh marbuta, final; hamza never joins (and keeps its own code point).
    CHECK_U32(Shape(U"\u0644\u0639\u0628\u0629"), U"\uFEDF\uFECC\uFE92\uFE94");
    CHECK_U32(Shape(U"\u0628\u0621\u0628"), U"\uFE8F\u0621\uFE8F");
    // Latin is left alone.
    CHECK_U32(Shape(U"Alt+Enter"), U"Alt+Enter");
}

void PenumbrasDirectionCases() {
    // One word, right to left: reversed.
    CHECK_U32(VisualLine(U"\u0633\u0644\u0627\u0645", true), U"\uFEE1\uFEFC\uFEB3");
    // A Latin key name in an Arabic line keeps its order and sits to the left.
    CHECK_U32(VisualLine(U"\u0627\u0636\u063A\u0637 Alt+Enter", true), U"Alt+Enter \uFEC2\uFED0\uFEBF\uFE8D");
    // A number after Arabic stays a number, in its place.
    CHECK_U32(VisualLine(U"\u0644\u062F\u064A\u0643 25 \u0645\u0627\u0646\u0627", true), U"\uFE8E\uFEE7\uFE8E\uFEE3 25 \uFEDA\uFEF3\uFEAA\uFEDF");
    // "12:05" is one number.
    CHECK_U32(VisualLine(U"\u0627\u0644\u0648\u0642\u062A 12:05", true), U"12:05 \uFE96\uFED7\uFEEE\uFEDF\uFE8D");
    // Brackets round Latin after Latin: all left to right, not "(joystick 3(".
    CHECK_U32(VisualLine(U"\u0642\u0641\u0632: CTRL (joystick 3)", true), U"CTRL (joystick 3) :\uFEB0\uFED4\uFED7");
    // Brackets round Latin after Arabic: mirrored, so they read "( ... )".
    CHECK_U32(VisualLine(U"\u0628 (Supersonic Engine)", true), U"(Supersonic Engine) \uFE8F");
    // Brackets round Arabic: mirrored.
    CHECK_U32(VisualLine(U"(\u0628)", true), U"(\uFE8F)");
    // The switch marker stays at the left, in its order.
    CHECK_U32(VisualLine(U"[\u2022] \u0646\u0627\u0641\u0630\u0629", true), U"[\u2022] \uFE93\uFEAC\uFED3\uFE8E\uFEE7");
    // A line of Latin alone, in an Arabic paragraph: as it reads; UI chrome with no letter in it as it is; a Latin line's leading dash, at its right.
    CHECK_U32(VisualLine(U"Ivan Cvetanovi\u0107", true), U"Ivan Cvetanovi\u0107");
    CHECK_U32(VisualLine(U"[<]", true), U"[<]");
    CHECK_U32(VisualLine(U"[>]", true), U"[>]");
    CHECK_U32(VisualLine(U"[\u2022] ", true), U"[\u2022] ");
    CHECK_U32(VisualLine(U"100%", true), U"100%");
    CHECK_U32(VisualLine(U"(joystick 3)", true), U"(joystick 3)");
    CHECK_U32(VisualLine(U" -www.example.com", true), U"www.example.com- ");
    // In a left-to-right paragraph an Arabic word still reads right to left.
    CHECK_U32(VisualLine(U"\u0627\u0644\u0639\u0631\u0628\u064A\u0629", false), U"\uFE94\uFEF4\uFE91\uFEAE\uFECC\uFEDF\uFE8D");
    CHECK_U32(VisualLine(U"English", false), U"English");
    // Every line alone; the breaks where they were.
    CHECK_U32(Visual(U"\u0628\n\u0633\u0644\u0627\u0645", true), U"\uFE8F\n\uFEE1\uFEFC\uFEB3");
}

// ---- this game's strings -------------------------------------------------------------------------------------------------------------

void ChapterAndLevelLines() {
    // dashboard.chapter, "U+0627U+0644U+0641U+0635U+0644 1": alef (right-joining: isolated FE8D, and it joins nothing after it) / lam before feh: initial FEDF /
    // feh between lam and sad: medial FED4 / sad between feh and lam: medial FEBC / lam after sad, before a space: final FEDE. Then " 1": the
    // digit after Arabic letters is an Arabic number (W2), the space between them takes R (N1). Drawn left to right: the number first (a
    // single digit is its own run), the space, and the letters from the last to the first.
    CHECK_U32(VisualLine(U"\u0627\u0644\u0641\u0635\u0644 1", true), U"1 \uFEDE\uFEBC\uFED4\uFEDF\uFE8D");
    // A two-digit number is one run and keeps its order.
    CHECK_U32(VisualLine(U"\u0627\u0644\u0641\u0635\u0644 12", true), U"12 \uFEDE\uFEBC\uFED4\uFEDF\uFE8D");

    // The level of a level: "U+0627U+0644U+0645U+0631U+062DU+0644U+0629 1-2" (alef, lam, meem, reh, hah, lam, teh marbuta). Alef: FE8D. Lam before meem: initial FEDF. Meem between lam and
    // reh (right-joining, so meem still joins forward): medial FEE4. Reh after meem, joins nothing after: final FEAE. Hah after reh (which does not
    // join forward): initial FEA3. Lam between hah and teh marbuta: medial FEE0. Teh marbuta after lam: final FE94.
    const std::u32string letters = U"\uFE94\uFEE0\uFEA3\uFEAE\uFEE4\uFEDF\uFE8D";
    CHECK_U32(Shape(U"\u0627\u0644\u0645\u0631\u062D\u0644\u0629"), std::u32string(U"\uFE8D\uFEDF\uFEE4\uFEAE\uFEA3\uFEE0\uFE94"));
    // THE DEVIATION: the hyphen between two numbers joins them, so the name of level 1-2 is drawn "1-2", as the player looks for it, and not
    // "2-1" (what UAX #9 gives, because W2 has made both digits Arabic numbers and the hyphen is then a neutral of the paragraph's direction).
    CHECK_U32(VisualLine(U"\u0627\u0644\u0645\u0631\u062D\u0644\u0629 1-2", true), U"1-2 " + letters);
    CHECK_U32(VisualLine(U"\u0627\u0644\u0645\u0631\u062D\u0644\u0629 12-34", true), U"12-34 " + letters);
    // The fix is the hyphen BETWEEN TWO NUMBERS and nothing wider: a hyphen after a word and before a number is as the standard has it
    // (a neutral between a word and a number, so it takes the paragraph's direction: it sits between them), and so is a lone hyphen.
    CHECK_U32(VisualLine(U"\u0628-1", true), U"1-\uFE8F");
    CHECK_U32(VisualLine(U"1-\u0628", true), U"\uFE8F-1");
    // A hyphen between Latin digits in a Latin line is untouched (a European number sequence, as before).
    CHECK_U32(VisualLine(U"1-2", true), U"1-2");
    CHECK_U32(VisualLine(U"1-2", false), U"1-2");
    // The English number, with a Latin letter before: the same as before the change.
    CHECK_U32(VisualLine(U"A 1-2", false), U"A 1-2");

    // A text of two lines (the shape of a sentence with a hard line break and a chapter-level number at its end: a letter, then a word and the number): every line alone.
    CHECK_U32(Visual(U"\u0628\n\u0627\u0644\u0645\u0631\u062D\u0644\u0629 3-4", true), U"\uFE8F\n3-4 " + letters);
}

void PercentAndNumbers() {
    // selector.requires, "U+064AU+062AU+0637U+0644U+0628 60%": ya (initial FEF3) teh (medial FE98) tah (medial FEC4) lam (medial FEE0) beh (final FE90). The digits are
    // Arabic numbers (W2), so the percent sign does NOT join them (W5 is for European numbers): it is a neutral that takes R and is
    // drawn at the LEFT of the number - where a reader going from the right has just finished reading 60.
    const std::u32string word = U"\uFE90\uFEE0\uFEC4\uFE98\uFEF3";
    CHECK_U32(VisualLine(U"\u064A\u062A\u0637\u0644\u0628 60%", true), U"%60 " + word);
    // Lone numbers and percent are UI chrome, as they are.
    CHECK_U32(VisualLine(U"60%", true), U"60%");
    CHECK_U32(VisualLine(U"{n}", true), U"{n}");
}

void LatinProperNounsInArabic() {
    // "U+0645U+0646 Asantee Games": meem initial FEE3, noon final FEE6; the Latin words keep their order and their space, and sit at the left.
    CHECK_U32(VisualLine(U"\u0645\u0646 Asantee Games", true), U"Asantee Games \uFEE6\uFEE3");
    // The credit line as the file spells it: Arabic, then a Latin name, then Arabic - the name stays whole in the middle of the reading order.
    CHECK_U32(VisualLine(U"\u0645\u0646 Magic Portals \u0645\u0646", true), U"\uFEE6\uFEE3 Magic Portals \uFEE6\uFEE3");
    // A Latin sentence in an Arabic paragraph, alone: as it reads.
    CHECK_U32(VisualLine(U"Magic Portals Enhanced", true), U"Magic Portals Enhanced");
}

// ---- Display ---------------------------------------------------------------------------------------------------------------------------

// The text as UTF-8, from code points: no non-ASCII character in this file.
std::string U8(const std::u32string& text) {
    return ArabicShaping::ToUtf8(text);
}

void DisplayIsTheShaperForArabicAndTheIdentityForTheRest() {
    const std::u32string logical = U"\u0627\u0644\u0641\u0635\u0644 1";
    const std::string text = U8(logical);
    CHECK_MSG(Strings::Display(Strings::Lang::Ar, text) == U8(U"1 \uFEDE\uFEBC\uFED4\uFEDF\uFE8D"), "Display(Ar) is the shaped text");
    CHECK_MSG(Strings::Display(Strings::Lang::Ar, text) == ArabicShaping::VisualUtf8(text, true), "and it is VisualUtf8 right to left");

    // Every other language: the identity, byte for byte - even a text that holds Arabic letters (a German string never does, and if one
    // did it would be shown as it is, not reshaped), and even a malformed byte, which the layout then reports.
    const std::string malformed = std::string("a") + static_cast<char>(0xE9) + "b";
    for (std::size_t i = 0; i < Strings::kLangCount; ++i) {
        const Strings::Lang lang = static_cast<Strings::Lang>(i);
        if (lang == Strings::Lang::Ar) continue;
        for (const std::string& sample : {std::string(), std::string("Part {n}"), text, malformed, std::string("a\nb"), U8(U"\u0645\u0646 Asantee Games")}) {
            CHECK_MSG(Strings::Display(lang, sample) == sample, "Display is the identity for " + std::string(Strings::kLangCodes[i]));
        }
    }
    // Arabic with a malformed byte does not crash and keeps the byte's replacement: U+FFFD in the output.
    CHECK_MSG(!Strings::Display(Strings::Lang::Ar, malformed).empty(), "a malformed byte in Arabic is shaped without a crash");
    CHECK_MSG(Strings::Display(Strings::Lang::Ar, "").empty(), "an empty text is empty");
    // A left-to-right Arabic-free text in the RTL language is the same text as a left-to-right paragraph would make of it.
    CHECK_MSG(Strings::Display(Strings::Lang::Ar, "Magic Portals") == "Magic Portals", "a Latin line in Arabic is as it reads");

    // NOT idempotent: the output is visual order, and a second pass reverses it. That is why Display is applied once, after the placeholders.
    const std::string once = Strings::Display(Strings::Lang::Ar, text);
    CHECK_MSG(Strings::Display(Strings::Lang::Ar, once) != once, "a second pass would undo the first, so Display is applied once");
    // A placeholder is filled BEFORE Display: the number is in the line when it is ordered.
    const std::string filled = Strings::Fill(U8(U"\u0627\u0644\u0641\u0635\u0644 {n}"), {{"n", "12"}});
    CHECK_MSG(Strings::Display(Strings::Lang::Ar, filled) == U8(U"12 \uFEDE\uFEBC\uFED4\uFEDF\uFE8D"), "a chapter line, filled and then displayed");
}

// ---- invariants -----------------------------------------------------------------------------------------------------------------------

char32_t Unmirror(char32_t c) { // the bracket pair as one character, for comparing a line with its mirrored self
    switch (c) {
        case ')': return '(';
        case ']': return '[';
        case '}': return '{';
        case '>': return '<';
        case 0x00BB: return 0x00AB;
        case 0x203A: return 0x2039;
        default: return c;
    }
}

std::u32string Sorted(const std::u32string& text) {
    std::u32string out;
    for (const char32_t c : text) out += Unmirror(c);
    std::sort(out.begin(), out.end());
    return out;
}

void TheFuzzKeepsItsInvariants() {
    // The alphabet: Arabic letters of every joining class, harakat, tatweel, digits, separators, brackets, Latin, space, a line break.
    const std::u32string alphabet = U"\u0627\u0628\u062A\u062D\u062F\u0631\u0633\u0639\u0644\u0645\u0646\u0647\u0648\u064A\u0629\u0621\u0622\u0640\u064E"
                                    U"0123456789-:.%+ ()[]<> abcXYZ\n";
    std::mt19937 rng(0x237);
    std::size_t checked = 0;
    for (int round = 0; round < 6000; ++round) {
        std::u32string text;
        const std::size_t length = rng() % 24;
        for (std::size_t i = 0; i < length; ++i) text += alphabet[rng() % alphabet.size()];
        for (const bool rtl : {true, false}) {
            const std::u32string out = Visual(text, rtl);
            // Visual order is a permutation of the shaped text, brackets aside - per line, since each line is ordered alone.
            std::size_t start = 0;
            std::size_t outStart = 0;
            for (std::size_t i = 0; i <= text.size(); ++i) {
                if (i < text.size() && text[i] != '\n') continue;
                const std::u32string line = text.substr(start, i - start);
                const std::u32string shaped = Shape(line);
                CHECK_MSG(outStart + shaped.size() <= out.size(), "the output holds every shaped line");
                if (outStart + shaped.size() > out.size()) break;
                const std::u32string visual = out.substr(outStart, shaped.size());
                CHECK_MSG(Sorted(visual) == Sorted(shaped), "a line's output is its shaped code points, reordered: " + Show(line));
                CHECK_MSG(VisualLine(line, rtl) == visual, "Visual is VisualLine per line");
                outStart += shaped.size();
                if (i < text.size()) {
                    CHECK_MSG(outStart < out.size() && out[outStart] == '\n', "a line break stays where it was");
                    ++outStart;
                }
                start = i + 1;
                ++checked;
            }
            CHECK_MSG(outStart == out.size(), "and nothing else is in it");
            // A left-to-right paragraph with no Arabic and no right-to-left letter in it is not changed.
            const bool arabic = std::any_of(text.begin(), text.end(), [](char32_t c) { return ArabicShaping::IsArabic(c); });
            if (!rtl && !arabic) CHECK_MSG(out == text, "Latin text in a left-to-right paragraph is as it is: " + Show(text));
        }
    }
    CHECK(checked > 10000);
}

void NothingIsReadPastTheEnd() {
    // Empty, one character, a mark at either end, a lam at the end, an alef at the start: no neighbour to look at.
    for (const std::u32string& text : {std::u32string(), std::u32string(U"\u0644"), std::u32string(U"\u064E"), std::u32string(U"\u064E\u0628"),
                                        std::u32string(U"\u0628\u064E"), std::u32string(U"\u0644\u0627"), std::u32string(U"-"), std::u32string(U"1-"),
                                        std::u32string(U"-1"), std::u32string(U"(")}) {
        CHECK_MSG(Visual(text, true).size() <= text.size(), "a short text is shaped and ordered without incident: " + Show(text));
        CHECK_MSG(Visual(text, false).size() <= text.size(), "in both paragraphs: " + Show(text));
    }
    CHECK_MSG(ArabicShaping::VisualUtf8("", true).empty(), "UTF-8 empty");
    CHECK_MSG(!ArabicShaping::IsArabic('A') && ArabicShaping::IsArabic(0x0627) && ArabicShaping::IsArabic(0xFEFB) && ArabicShaping::IsArabic(0xFB50),
              "IsArabic: the letters and both presentation blocks");
}

void runTests() {
    PenumbrasShapingCases();
    PenumbrasDirectionCases();
    ChapterAndLevelLines();
    PercentAndNumbers();
    LatinProperNounsInArabic();
    DisplayIsTheShaperForArabicAndTheIdentityForTheRest();
    TheFuzzKeepsItsInvariants();
    NothingIsReadPastTheEnd();
}

} // namespace

MP_TEST_MAIN("test_mp_arabic", 500)
