#include "sim/Utf8.hpp"

namespace MagicPortals::Utf8 {

namespace {

// How a lead byte opens a sequence: how many continuation bytes follow, the bits the lead carries, and the range the FIRST continuation
// byte may take (the Unicode Standard's Table 3-7: it is what keeps overlong forms, surrogates and values above U+10FFFF out).
struct Lead {
    int continuations = 0;
    std::uint32_t bits = 0;
    unsigned char firstLow = 0x80;
    unsigned char firstHigh = 0xBF;
};

// continuations 0 is "this byte starts nothing": a stray continuation byte, C0, C1 and F5-FF.
Lead LeadOf(unsigned char b) {
    if (b >= 0xC2 && b <= 0xDF) return {1, b & 0x1Fu, 0x80, 0xBF};
    if (b == 0xE0) return {2, b & 0x0Fu, 0xA0, 0xBF};
    if (b == 0xED) return {2, b & 0x0Fu, 0x80, 0x9F};
    if (b >= 0xE1 && b <= 0xEF) return {2, b & 0x0Fu, 0x80, 0xBF};
    if (b == 0xF0) return {3, b & 0x07u, 0x90, 0xBF};
    if (b == 0xF4) return {3, b & 0x07u, 0x80, 0x8F};
    if (b >= 0xF1 && b <= 0xF3) return {3, b & 0x07u, 0x80, 0xBF};
    return {};
}

} // namespace

std::vector<std::uint32_t> Decode(std::string_view text, std::size_t& replaced) {
    std::vector<std::uint32_t> out;
    out.reserve(text.size());
    replaced = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const auto b = static_cast<unsigned char>(text[i]);
        if (b < 0x80) {
            out.push_back(b);
            ++i;
            continue;
        }
        const Lead lead = LeadOf(b);
        if (lead.continuations == 0) {
            out.push_back(kReplacement);
            ++replaced;
            ++i;
            continue;
        }
        std::uint32_t value = lead.bits;
        std::size_t j = i + 1;
        bool whole = true;
        for (int k = 0; k < lead.continuations; ++k) {
            if (j >= text.size()) {
                whole = false;
                break;
            }
            const auto c = static_cast<unsigned char>(text[j]);
            const unsigned char low = k == 0 ? lead.firstLow : static_cast<unsigned char>(0x80);
            const unsigned char high = k == 0 ? lead.firstHigh : static_cast<unsigned char>(0xBF);
            if (c < low || c > high) {
                whole = false;
                break;
            }
            value = (value << 6) | (c & 0x3Fu);
            ++j;
        }
        // A broken sequence is ONE replacement for the valid bytes it had; the byte that broke it is not consumed.
        out.push_back(whole ? value : kReplacement);
        if (!whole) ++replaced;
        i = j;
    }
    return out;
}

std::vector<std::uint32_t> Decode(std::string_view text) {
    std::size_t replaced = 0;
    return Decode(text, replaced);
}

bool IsValid(std::string_view text) {
    std::size_t replaced = 0;
    Decode(text, replaced);
    return replaced == 0;
}

std::string Encode(std::uint32_t codePoint) {
    if (codePoint > kLastCodePoint || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) codePoint = kReplacement;
    std::string out;
    if (codePoint < 0x80) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    return out;
}

std::string Encode(const std::vector<std::uint32_t>& codePoints) {
    std::string out;
    out.reserve(codePoints.size());
    for (const std::uint32_t codePoint : codePoints) out += Encode(codePoint);
    return out;
}

} // namespace MagicPortals::Utf8
