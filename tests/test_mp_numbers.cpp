// The number parser every platform shares (planning step 199, PORT-1): sim/Numbers' ParseDouble and its fallback.
//
// The tscn reader, the lighting table and the controls flag read numbers with std::from_chars, which MSVC and libstdc++ provide for
// double and the Android NDK's libc++ deletes. ParseDouble is std::from_chars where the library has it and ParseDoubleFallback (a
// grammar check, then strtod on a copy that carries the locale's decimal point) where it does not. What is pinned:
//
//   the table      126 hard cases, the expected bits written down from Python's correctly rounded float(): subnormals and their
//                  rounding at half the smallest, the largest double and the halfway above it, ties to even, a 17-digit value, an
//                  800-digit mantissa, exponents too large to hold in an integer, signs, leading zeros, trailing characters, an
//                  exponent with no digit, an empty string, a lone sign, a plus sign, infinity and NaN. Each is held by the
//                  fallback, by ParseDouble and, where the library has it, by std::from_chars itself, so the table is the platform's
//                  own answer and not only ours.
//   the oracle     20,000 doubles from a seeded generator (random bit patterns, a third of them subnormal), printed with %.17g and read
//                  back: the fallback returns the bits that were printed. Needs no from_chars.
//   the fuzz       120,000 random strings of the grammar's own alphabet and 60,000 random decimal strings with exponents from -340
//                  to +320: the fallback and std::from_chars agree on the error code, the characters read and the bits.
//   the data       every run of number characters in out/levels/*.tscn, data/*.json and game/data/*.json, read by the fallback,
//                  by ParseDouble and by std::from_chars, compared bit for bit; the count is recorded and has a floor so that a path
//                  that finds no file cannot pass. Without the converted levels this part says so and skips, the rest runs (a suite
//                  that skips only a part of itself takes no SKIPS_WITH_77: tests/CMakeLists.txt).
//   the locale     under a locale whose decimal point is a comma (when the machine has one) the fallback still reads '1.5'.
//   the contract   the value is written only when a number was read.
//
// A helper mutated to accept a plus sign, to answer out_of_range from errno, to round through float, to read a subnormal as zero, to
// let strtod find the number's end or to leave the decimal point alone fails this suite (the mutations tried are in the Step).
// Pure: no window, no level.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Numbers.hpp"

#include <bit>
#include <charconv>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;

namespace {

namespace N = MagicPortals::Numbers;

enum class Kind { Ok, OutOfRange, Invalid };

constexpr std::uint64_t NaN = 0x7FF8000000000000ULL; // in the table: any NaN
// What the value holds before a parse: a quiet NaN with a payload no parse produces, so that "untouched" can be told from every result.
constexpr std::uint64_t kUntouched = 0x7FF8DEADBEEF0001ULL;

struct Case {
    std::string text;
    Kind kind;
    int used; // characters read: 0 when there is no number
    std::uint64_t bits;
};

const std::vector<Case>& Cases() {
    static const std::vector<Case> cases = {
    {"0", Kind::Ok, 1, 0x0000000000000000ULL},
    {"-0", Kind::Ok, 2, 0x8000000000000000ULL},
    {"0.0", Kind::Ok, 3, 0x0000000000000000ULL},
    {"-0.0", Kind::Ok, 4, 0x8000000000000000ULL},
    {"1", Kind::Ok, 1, 0x3FF0000000000000ULL},
    {"-1", Kind::Ok, 2, 0xBFF0000000000000ULL},
    {"123456789", Kind::Ok, 9, 0x419D6F3454000000ULL},
    {"0.1", Kind::Ok, 3, 0x3FB999999999999AULL},
    {"0.5", Kind::Ok, 3, 0x3FE0000000000000ULL},
    {"1.5e3", Kind::Ok, 5, 0x4097700000000000ULL},
    {"1e5", Kind::Ok, 3, 0x40F86A0000000000ULL},
    {"1E5", Kind::Ok, 3, 0x40F86A0000000000ULL},
    {"1e+5", Kind::Ok, 4, 0x40F86A0000000000ULL},
    {"1e-5", Kind::Ok, 4, 0x3EE4F8B588E368F1ULL},
    {"1e0", Kind::Ok, 3, 0x3FF0000000000000ULL},
    {".5", Kind::Ok, 2, 0x3FE0000000000000ULL},
    {"-.5", Kind::Ok, 3, 0xBFE0000000000000ULL},
    {"5.", Kind::Ok, 2, 0x4014000000000000ULL},
    {"5.e2", Kind::Ok, 4, 0x407F400000000000ULL},
    {"00012", Kind::Ok, 5, 0x4028000000000000ULL},
    {"000.500", Kind::Ok, 7, 0x3FE0000000000000ULL},
    {"007e-2", Kind::Ok, 6, 0x3FB1EB851EB851ECULL},
    {"0.1e1", Kind::Ok, 5, 0x3FF0000000000000ULL},
    {"100", Kind::Ok, 3, 0x4059000000000000ULL},
    {"3.14159265358979323846", Kind::Ok, 22, 0x400921FB54442D18ULL},
    {"0.30000000000000004", Kind::Ok, 19, 0x3FD3333333333334ULL},
    {"0.1000000000000000055511151231257827", Kind::Ok, 36, 0x3FB999999999999AULL},
    {"123456789012345678901234567890", Kind::Ok, 30, 0x45F8EE90FF6C373EULL},
    {"2.5e-3", Kind::Ok, 6, 0x3F647AE147AE147BULL},
    {"-2.5E+3", Kind::Ok, 7, 0xC0A3880000000000ULL},
    {"1e22", Kind::Ok, 4, 0x4480F0CF064DD592ULL},
    {"1e23", Kind::Ok, 4, 0x44B52D02C7E14AF6ULL},
    {"8.5e-5", Kind::Ok, 6, 0x3F164840E1719F80ULL},
    {"9007199254740993", Kind::Ok, 16, 0x4340000000000000ULL}, // 2^53+1: a tie, to even
    {"9007199254740995", Kind::Ok, 16, 0x4340000000000002ULL}, // a tie, to even (up)
    {std::string("9007199254740993.") + std::string(41, '0') + std::string("1"), Kind::Ok, 59, 0x4340000000000001ULL}, // above the tie by a sliver: up
    {"9007199254740993.0", Kind::Ok, 18, 0x4340000000000000ULL}, // still a tie
    {"4503599627370497.5", Kind::Ok, 18, 0x4330000000000002ULL}, // 2^52+1.5: a tie at spacing 1: to even
    {"0.99999999999999994", Kind::Ok, 19, 0x3FEFFFFFFFFFFFFFULL}, // 1 - 2^-54 region
    {"1.7976931348623157e308", Kind::Ok, 22, 0x7FEFFFFFFFFFFFFFULL}, // largest double
    {"1.7976931348623158e308", Kind::Ok, 22, 0x7FEFFFFFFFFFFFFFULL}, // below the halfway: still the largest
    {"1.7976931348623159e308", Kind::OutOfRange, 22, 0}, // above the halfway: overflow
    {"1e309", Kind::OutOfRange, 5, 0},
    {"-1e309", Kind::OutOfRange, 6, 0},
    {"1e999999999999999999999", Kind::OutOfRange, 23, 0},
    {"-1e999999999999999999999", Kind::OutOfRange, 24, 0},
    {"1e-999999999999999999999", Kind::OutOfRange, 24, 0},
    {"0e999999999999999999999", Kind::Ok, 23, 0x0000000000000000ULL}, // zero whatever the exponent
    {"0.0e-1000", Kind::Ok, 9, 0x0000000000000000ULL},
    {"-0e5", Kind::Ok, 4, 0x8000000000000000ULL},
    {"4.9406564584124654e-324", Kind::Ok, 23, 0x0000000000000001ULL}, // smallest subnormal
    {"5e-324", Kind::Ok, 6, 0x0000000000000001ULL},
    {"2.4703282292062327e-324", Kind::OutOfRange, 23, 0}, // just under half the smallest subnormal: rounds to zero
    {"2.4703282292062328e-324", Kind::Ok, 23, 0x0000000000000001ULL}, // just over half: the smallest subnormal
    {"3e-324", Kind::Ok, 6, 0x0000000000000001ULL},
    {"1e-324", Kind::OutOfRange, 6, 0},
    {"2.2250738585072014e-308", Kind::Ok, 23, 0x0010000000000000ULL}, // smallest normal
    {"2.2250738585072009e-308", Kind::Ok, 23, 0x000FFFFFFFFFFFFFULL}, // largest subnormal
    {"1e-310", Kind::Ok, 6, 0x000012688B70E62BULL},
    {"1e-320", Kind::Ok, 6, 0x00000000000007E8ULL},
    {"1e-323", Kind::Ok, 6, 0x0000000000000002ULL},
    {"1e-400", Kind::OutOfRange, 6, 0},
    {"-1e-400", Kind::OutOfRange, 7, 0},
    {std::string("0.") + std::string(400, '0') + std::string("1e400"), Kind::Ok, 407, 0x3FB999999999999AULL}, // 0.1 behind 400 zeros
    {std::string("1") + std::string(400, '0') + std::string("e-400"), Kind::Ok, 406, 0x3FF0000000000000ULL}, // 1 with 400 zeros
    {std::string("0.") + std::string(800, '9'), Kind::Ok, 802, 0x3FF0000000000000ULL}, // 800 nines: 1.0
    {std::string(800, '7'), Kind::OutOfRange, 800, 0}, // an 800-digit integer: overflow
    {std::string("1.") + std::string(800, '0') + std::string("1"), Kind::Ok, 803, 0x3FF0000000000000ULL}, // 1 plus a sliver
    {"1.5abc", Kind::Ok, 3, 0x3FF8000000000000ULL},
    {"12 ", Kind::Ok, 2, 0x4028000000000000ULL},
    {"1,5", Kind::Ok, 1, 0x3FF0000000000000ULL},
    {"1e", Kind::Ok, 1, 0x3FF0000000000000ULL},
    {"1e+", Kind::Ok, 1, 0x3FF0000000000000ULL},
    {"1E-", Kind::Ok, 1, 0x3FF0000000000000ULL},
    {"1e+x", Kind::Ok, 1, 0x3FF0000000000000ULL},
    {"1.5.5", Kind::Ok, 3, 0x3FF8000000000000ULL},
    {"1-2", Kind::Ok, 1, 0x3FF0000000000000ULL},
    {"0x10", Kind::Ok, 1, 0x0000000000000000ULL},
    {"1_000", Kind::Ok, 1, 0x3FF0000000000000ULL},
    {"1e5e5", Kind::Ok, 3, 0x40F86A0000000000ULL},
    {"1e+5.5", Kind::Ok, 4, 0x40F86A0000000000ULL},
    {"1p3", Kind::Ok, 1, 0x3FF0000000000000ULL},
    {"-1e5-", Kind::Ok, 4, 0xC0F86A0000000000ULL},
    {"12\n", Kind::Ok, 2, 0x4028000000000000ULL},
    {"3.0f", Kind::Ok, 3, 0x4008000000000000ULL},
    {"1.e", Kind::Ok, 2, 0x3FF0000000000000ULL},
    {"", Kind::Invalid, 0, 0},
    {"-", Kind::Invalid, 0, 0},
    {"+", Kind::Invalid, 0, 0},
    {"+1", Kind::Invalid, 0, 0},
    {"--1", Kind::Invalid, 0, 0},
    {"-+1", Kind::Invalid, 0, 0},
    {".", Kind::Invalid, 0, 0},
    {"-.", Kind::Invalid, 0, 0},
    {"e5", Kind::Invalid, 0, 0},
    {"E5", Kind::Invalid, 0, 0},
    {" 1", Kind::Invalid, 0, 0},
    {"\t1", Kind::Invalid, 0, 0},
    {"abc", Kind::Invalid, 0, 0},
    {"i", Kind::Invalid, 0, 0},
    {"in", Kind::Invalid, 0, 0},
    {"-i", Kind::Invalid, 0, 0},
    {"n", Kind::Invalid, 0, 0},
    {"na", Kind::Invalid, 0, 0},
    {"-n", Kind::Invalid, 0, 0},
    {"-e5", Kind::Invalid, 0, 0},
    {".e5", Kind::Invalid, 0, 0},
    {",5", Kind::Invalid, 0, 0},
    {"(1)", Kind::Invalid, 0, 0},
    {"x", Kind::Invalid, 0, 0},
    {"inf", Kind::Ok, 3, 0x7FF0000000000000ULL},
    {"-inf", Kind::Ok, 4, 0xFFF0000000000000ULL},
    {"INF", Kind::Ok, 3, 0x7FF0000000000000ULL},
    {"Infinity", Kind::Ok, 8, 0x7FF0000000000000ULL},
    {"INFINITY", Kind::Ok, 8, 0x7FF0000000000000ULL},
    {"-infinity", Kind::Ok, 9, 0xFFF0000000000000ULL},
    {"infinit", Kind::Ok, 3, 0x7FF0000000000000ULL},
    {"infx", Kind::Ok, 3, 0x7FF0000000000000ULL},
    {"nan", Kind::Ok, 3, NaN},
    {"-nan", Kind::Ok, 4, NaN},
    {"NaN", Kind::Ok, 3, NaN},
    {"nan(abc_1)", Kind::Ok, 10, NaN},
    {"nan(", Kind::Ok, 3, NaN},
    {"nan(a b)", Kind::Ok, 3, NaN},
    {"nan()", Kind::Ok, 5, NaN},
    {"nanx", Kind::Ok, 3, NaN},
    };
    return cases;
}

using Parser = std::from_chars_result (*)(const char*, const char*, double&);

std::uint64_t Bits(double v) { return std::bit_cast<std::uint64_t>(v); }
double FromBits(std::uint64_t b) { return std::bit_cast<double>(b); }

struct Parsed {
    Kind kind = Kind::Invalid;
    std::ptrdiff_t used = 0;
    std::uint64_t bits = kUntouched; // the value after the call
};

Kind KindOf(std::errc ec) {
    if (ec == std::errc{}) return Kind::Ok;
    if (ec == std::errc::result_out_of_range) return Kind::OutOfRange;
    return Kind::Invalid;
}

// The text in a buffer of exactly its length, no terminator after it: a parser that reads past `last` is what AddressSanitizer is
// there to find.
Parsed Run(Parser parse, const std::string& text) {
    std::vector<char> buffer(text.begin(), text.end());
    const char* first = buffer.data();
    const char* last = first + buffer.size();
    double value = FromBits(kUntouched);
    const std::from_chars_result r = parse(first, last, value);
    return {KindOf(r.ec), r.ptr - first, Bits(value)};
}

#if MAGICPORTALS_NUMBERS_STD_FROM_CHARS
// The platform's own, called directly: the old call.
std::from_chars_result RawStd(const char* first, const char* last, double& value) { return std::from_chars(first, last, value); }
#endif

bool IsNaN(std::uint64_t bits) { return std::isnan(FromBits(bits)); }

// Whether two answers are the same number: the kind, the characters read, and the bits when a number was read (any NaN is a NaN).
bool SameNumber(const Parsed& a, const Parsed& b) {
    if (a.kind != b.kind || a.used != b.used) return false;
    if (a.kind != Kind::Ok) return true;
    if (IsNaN(a.bits) || IsNaN(b.bits)) return IsNaN(a.bits) && IsNaN(b.bits);
    return a.bits == b.bits;
}

std::string Describe(const Parsed& p) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "kind %d used %lld bits %016llX", static_cast<int>(p.kind), static_cast<long long>(p.used),
                  static_cast<unsigned long long>(p.bits));
    return buffer;
}

std::string Shown(const std::string& text) {
    std::string out = text.size() > 48 ? text.substr(0, 48) + "...(" + std::to_string(text.size()) + " chars)" : text;
    for (char& c : out)
        if (static_cast<unsigned char>(c) < 0x20) c = '?';
    return "'" + out + "'";
}

// ---- the table ------------------------------------------------------------------------------------------------------

// A parser's answer against a row. The value must be untouched when no number was read.
std::string AgainstRow(const Parsed& got, const Case& row, bool valueUntouchedOnFailure) {
    if (got.kind != row.kind) return "kind " + std::to_string(static_cast<int>(got.kind)) + ", wanted " + std::to_string(static_cast<int>(row.kind));
    if (got.used != row.used) return "read " + std::to_string(got.used) + " characters, wanted " + std::to_string(row.used);
    if (row.kind == Kind::Ok) {
        if (row.bits == NaN) return IsNaN(got.bits) ? "" : "not a NaN";
        if (got.bits != row.bits) return Describe(got) + ", wanted bits " + std::to_string(row.bits);
    } else if (valueUntouchedOnFailure && got.bits != kUntouched) {
        return "wrote the value although no number was read";
    }
    return "";
}

void CheckTable(const char* name, Parser parse, bool valueUntouchedOnFailure) {
    int bad = 0;
    for (const Case& row : Cases()) {
        const std::string wrong = AgainstRow(Run(parse, row.text), row, valueUntouchedOnFailure);
        CHECK_MSG(wrong.empty(), std::string(name) + " " + Shown(row.text) + ": " + wrong);
        if (!wrong.empty()) ++bad;
    }
    CHECK_EQ(bad, 0);
    CHECK(Cases().size() >= 120);
}

void TheFallbackHoldsTheTable() { CheckTable("fallback", &N::ParseDoubleFallback, true); }

void ParseDoubleHoldsTheTable() { CheckTable("ParseDouble", &N::ParseDouble, true); }

void TheLibrarysOwnFromCharsHoldsTheTable() {
#if MAGICPORTALS_NUMBERS_STD_FROM_CHARS
    // The old call. MSVC's library writes an infinity or a zero when it reports out_of_range, so the value is not asked to be untouched.
    CheckTable("std::from_chars", &RawStd, false);
#else
    std::printf("  this library has no floating-point from_chars (the NDK's libc++ 19, Apple's): the table is held by the fallback alone\n");
#endif
}

// ---- the oracle that needs no from_chars ----------------------------------------------------------------------------------

void ADoubleReadBackFromItsOwnPrintingIsTheSameDouble() {
    std::mt19937_64 random(0x4D50ULL); // specified exactly by the standard: the same stream on every library
    int bad = 0;
    int tried = 0;
    for (int i = 0; i < 20000; ++i) {
        std::uint64_t bits = random();
        if (i % 3 == 0) bits &= 0x800FFFFFFFFFFFFFULL; // a third subnormal (or zero), the sign kept
        const double v = FromBits(bits);
        if (!std::isfinite(v)) continue;
        char text[64];
        std::snprintf(text, sizeof(text), "%.17g", v);
        const Parsed got = Run(&N::ParseDoubleFallback, text);
        ++tried;
        if (got.kind != Kind::Ok || got.used != static_cast<std::ptrdiff_t>(std::strlen(text)) || got.bits != bits) {
            if (bad++ < 5) CHECK_MSG(false, std::string("round trip of ") + text + ": " + Describe(got) + ", wanted " + std::to_string(bits));
        }
    }
    CHECK_EQ(bad, 0);
    CHECK(tried > 19000);
}

// ---- the fuzz against the library --------------------------------------------------------------------------------------------

#if MAGICPORTALS_NUMBERS_STD_FROM_CHARS
void CompareWithTheLibrary(const char* what, const std::vector<std::string>& texts) {
    int bad = 0;
    for (const std::string& text : texts) {
        const Parsed mine = Run(&N::ParseDoubleFallback, text);
        const Parsed theirs = Run(&RawStd, text);
        if (SameNumber(mine, theirs)) continue;
        if (bad++ < 5) CHECK_MSG(false, std::string(what) + " " + Shown(text) + ": fallback " + Describe(mine) + ", library " + Describe(theirs));
    }
    CHECK_EQ(bad, 0);
    CHECK(!texts.empty());
}
#endif

void TheFallbackAgreesWithTheLibraryOnRandomStrings() {
#if MAGICPORTALS_NUMBERS_STD_FROM_CHARS
    std::mt19937_64 random(0x5EEDULL);
    // The grammar's own alphabet, so that runs of it are long enough to be numbers now and then.
    static const char kAlphabet[] = "0123456789012345678901234567890123456789..--++eEeE iInNfFaAtTyY()_xX";
    std::vector<std::string> garbage;
    for (int i = 0; i < 120000; ++i) {
        std::string text;
        const int length = static_cast<int>(random() % 14);
        for (int k = 0; k < length; ++k) text += kAlphabet[random() % (sizeof(kAlphabet) - 1)];
        garbage.push_back(text);
    }
    CompareWithTheLibrary("garbage", garbage);

    std::vector<std::string> decimals;
    for (int i = 0; i < 60000; ++i) {
        std::string text;
        if (random() % 3 == 0) text += '-';
        const int digits = 1 + static_cast<int>(random() % 40);
        const int dot = random() % 4 == 0 ? -1 : static_cast<int>(random() % static_cast<unsigned>(digits + 1));
        for (int k = 0; k < digits; ++k) {
            if (k == dot) text += '.';
            text += static_cast<char>('0' + random() % 10);
        }
        if (dot == digits) text += '.';
        if (random() % 2 == 0) {
            text += random() % 2 == 0 ? 'e' : 'E';
            const int sign = static_cast<int>(random() % 3);
            if (sign == 1) text += '+';
            if (sign == 2) text += '-';
            text += std::to_string(random() % 340 + (random() % 20 == 0 ? 300 : 0));
        }
        if (random() % 10 == 0) text += "xyz";
        decimals.push_back(text);
    }
    CompareWithTheLibrary("decimal", decimals);
#else
    std::printf("  this library has no floating-point from_chars: no oracle for the fuzz here (the round trip above is the check)\n");
#endif
}

// ---- the data ---------------------------------------------------------------------------------------------------------------

struct Tally {
    std::size_t files = 0;
    std::size_t tokens = 0;
    std::size_t numbers = 0; // the tokens that read as a number
    std::size_t mismatches = 0;
};

bool NumberCharacter(char c) { return (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E'; }

// Every maximal run of number characters that starts with a digit, a sign or a point, in a file's bytes.
void TokensOf(const std::string& bytes, std::vector<std::string>& out) {
    std::size_t i = 0;
    while (i < bytes.size()) {
        const char c = bytes[i];
        if ((c >= '0' && c <= '9') || c == '-' || c == '.' || c == '+') {
            std::size_t j = i;
            while (j < bytes.size() && NumberCharacter(bytes[j])) ++j;
            out.push_back(bytes.substr(i, j - i));
            i = j;
        } else {
            ++i;
        }
    }
}

void ReadTree(const std::string& root, const char* extension, Tally& tally) {
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) return;
    std::vector<std::string> files;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == extension) files.push_back(it->path().string());
    }
    for (const std::string& file : files) {
        std::ifstream in(file, std::ios::binary);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        std::vector<std::string> tokens;
        TokensOf(buffer.str(), tokens);
        ++tally.files;
        for (const std::string& token : tokens) {
            ++tally.tokens;
            const Parsed fallback = Run(&N::ParseDoubleFallback, token);
            const Parsed helper = Run(&N::ParseDouble, token);
            bool same = SameNumber(fallback, helper) && fallback.bits == helper.bits;
#if MAGICPORTALS_NUMBERS_STD_FROM_CHARS
            same = same && SameNumber(fallback, Run(&RawStd, token));
#endif
            if (fallback.kind == Kind::Ok) ++tally.numbers;
            if (!same && tally.mismatches++ < 5) CHECK_MSG(false, file + ": " + Shown(token) + ": fallback " + Describe(fallback) + ", ParseDouble " + Describe(helper));
        }
    }
}

void EveryNumberInTheDataReadsTheSameWithinTheFallbackTheHelperAndTheLibrary() {
    // TWO HALVES (planning step 200, the rider R3). The committed tables (data/, game/data/) are in the repository and need nothing, so they
    // are read on every clone; the converted levels are the gitignored out/ and are read where it is, with the floors that go with them.
    // Until this step the first was skipped together with the second, so a clean clone and the CI that follows read no number at all.
    const std::string levels = MAGICPORTALS_LEVELS_DIR;
    std::error_code ec;
    const bool haveLevels = !levels.empty() && std::filesystem::is_directory(levels, ec);
    Tally tally;
    std::size_t tscnFiles = 0;
    std::size_t tscnTokens = 0;
    if (haveLevels) {
        ReadTree(levels, ".tscn", tally);
        tscnFiles = tally.files;
        tscnTokens = tally.tokens;
    } else {
        std::printf("  the converted levels' numbers SKIPPED - needs them at %s (the committed tables below ran)\n", levels.c_str());
    }
    const Tally levelsOnly = tally;
    ReadTree(MAGICPORTALS_DATA_DIR, ".json", tally);
    ReadTree(MAGICPORTALS_PORT_DATA_DIR, ".json", tally);
    const std::size_t jsonFiles = tally.files - levelsOnly.files;
    const std::size_t jsonNumbers = tally.numbers - levelsOnly.numbers;
    std::printf("  data: %zu files (%zu tscn; %zu committed tables with %zu numbers), %zu tokens (%zu in the tscn files), %zu of them numbers, %zu mismatches; the library's own from_chars %s\n",
                tally.files, tscnFiles, jsonFiles, jsonNumbers, tally.tokens, tscnTokens, tally.numbers, tally.mismatches,
                N::kStandardFromChars ? "was the reference" : "is not there: the helper and the fallback are the same code");
    CHECK_EQ(tally.mismatches, static_cast<std::size_t>(0));
    // The committed half's own floors, so a clean clone that finds nothing fails instead of passing over an empty folder.
    CHECK_MSG(jsonFiles >= 40, std::to_string(jsonFiles) + " committed tables read");
    CHECK_MSG(jsonNumbers >= 15000, std::to_string(jsonNumbers) + " numbers in the committed tables");
    if (haveLevels) {
        // The floors: 128 levels, and far more tokens than any path that found one folder only.
        CHECK(tscnFiles >= 128);
        CHECK(tscnTokens >= 80000);
        CHECK(tally.files >= 128 + 20);
        CHECK(tally.numbers >= 80000);
    }
}

// ---- the locale ---------------------------------------------------------------------------------------------------------------

struct LocaleGuard {
    LocaleGuard() {
        const char* now = std::setlocale(LC_NUMERIC, nullptr);
        saved = now == nullptr ? "C" : now;
    }
    ~LocaleGuard() { std::setlocale(LC_NUMERIC, saved.c_str()); }
    std::string saved;
};

void TheFallbackDoesNotReadTheLocalesDecimalPoint() {
    LocaleGuard guard;
    static const char* const kCandidates[] = {"de_DE.UTF-8", "de_DE.utf8", "de_DE", "fr_FR.UTF-8", "fr_FR.utf8", "fr_FR", "de-DE",
                                              "fr-FR",       "German_Germany.1252", "French_France.1252", "es_ES.UTF-8", "ru_RU.UTF-8"};
    std::string used;
    for (const char* name : kCandidates) {
        if (std::setlocale(LC_NUMERIC, name) == nullptr) continue;
        const std::lconv* conv = std::localeconv();
        if (conv != nullptr && conv->decimal_point != nullptr && std::strcmp(conv->decimal_point, ".") != 0) {
            used = name;
            break;
        }
    }
    if (used.empty()) {
        std::printf("  no locale with a decimal comma is installed here: the fallback was not run under one\n");
        return;
    }
    std::printf("  locale %s (decimal point '%s')\n", used.c_str(), std::localeconv()->decimal_point);
    struct Row {
        const char* text;
        std::uint64_t bits;
    };
    static const Row kRows[] = {{"1.5", 0x3FF8000000000000ULL},
                                {"-0.25e2", 0xC039000000000000ULL},
                                {"1234.5678", 0x40934A456D5CFAADULL},
                                {"3.14159265358979", 0x400921FB54442D11ULL},
                                {"0.1", 0x3FB999999999999AULL},
                                {"7", 0x401C000000000000ULL}};
    for (const Row& row : kRows) {
        const Parsed got = Run(&N::ParseDoubleFallback, row.text);
        CHECK_MSG(got.kind == Kind::Ok && got.bits == row.bits && got.used == static_cast<std::ptrdiff_t>(std::strlen(row.text)),
                  std::string("under ") + used + ": " + row.text + " " + Describe(got));
    }
    // The locale's own notation is not the grammar: a comma is a trailing character, and the number ends before it.
    const Parsed comma = Run(&N::ParseDoubleFallback, "1,5");
    CHECK(comma.kind == Kind::Ok && comma.used == 1 && comma.bits == Bits(1.0));
    // And ParseDouble, whichever it is, gives the same.
    for (const Row& row : kRows) CHECK(Run(&N::ParseDouble, row.text).bits == row.bits);
}

// ---- the contract -----------------------------------------------------------------------------------------------------------------

void AValueIsWrittenOnlyWhenANumberWasRead() {
    for (Parser parse : {&N::ParseDouble, &N::ParseDoubleFallback}) {
        for (const char* text : {"", "-", "abc", "+1", "1e999", "-1e999", "1e-999", "inx"}) {
            double v = 7.5;
            const std::string t = text;
            const std::from_chars_result r = parse(t.data(), t.data() + t.size(), v);
            CHECK_MSG(r.ec != std::errc{} && v == 7.5, std::string("the value after '") + text + "' was written");
            CHECK(r.ec == std::errc::result_out_of_range || r.ptr == t.data());
        }
        double v = 7.5;
        const std::string ok = "2.25 and more";
        const std::from_chars_result r = parse(ok.data(), ok.data() + ok.size(), v);
        CHECK(r.ec == std::errc{} && v == 2.25 && r.ptr == ok.data() + 4);
    }
}

void TheRulesTheThreeSitesBuildOnHold() {
    // Each site takes the whole text as one finite number: ec == errc{}, ptr == last, isfinite. The table says it per row; here are the
    // rows the sites' own error messages are written for.
    const auto whole = [](const std::string& text) {
        const Parsed p = Run(&N::ParseDouble, text);
        return !text.empty() && p.kind == Kind::Ok && p.used == static_cast<std::ptrdiff_t>(text.size()) && std::isfinite(FromBits(p.bits));
    };
    CHECK(whole("1"));
    CHECK(whole("-3.5"));
    CHECK(whole("1e2"));
    CHECK(whole("0.000001"));
    CHECK(!whole(""));
    CHECK(!whole("+1"));
    CHECK(!whole("1 "));
    CHECK(!whole(" 1"));
    CHECK(!whole("1x"));
    CHECK(!whole("-"));
    CHECK(!whole("."));
    CHECK(!whole("inf"));
    CHECK(!whole("nan"));
    CHECK(!whole("1e999"));
    CHECK(!whole("0x10"));
}

void runTests() {
    TheFallbackHoldsTheTable();
    ParseDoubleHoldsTheTable();
    TheLibrarysOwnFromCharsHoldsTheTable();
    ADoubleReadBackFromItsOwnPrintingIsTheSameDouble();
    TheFallbackAgreesWithTheLibraryOnRandomStrings();
    AValueIsWrittenOnlyWhenANumberWasRead();
    TheRulesTheThreeSitesBuildOnHold();
    TheFallbackDoesNotReadTheLocalesDecimalPoint();
    EveryNumberInTheDataReadsTheSameWithinTheFallbackTheHelperAndTheLibrary();
}

} // namespace

MP_TEST_MAIN("test_mp_numbers", 300)
