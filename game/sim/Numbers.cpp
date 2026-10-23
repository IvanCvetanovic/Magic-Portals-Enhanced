#include "sim/Numbers.hpp"

#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <string>

namespace MagicPortals::Numbers {

namespace {

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

// ASCII only, as the library's own scan is: std::tolower would read the locale.
char Lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; }

// `lower` (all lower-case letters) begins at `from`, in either case.
bool StartsWithNoCase(const char* from, const char* last, const char* lower) {
    for (; *lower != '\0'; ++from, ++lower) {
        if (from == last || Lower(*from) != *lower) return false;
    }
    return true;
}

// `inf` or `infinity`, after the sign. `at` is the 'i'.
std::from_chars_result ReadInfinity(const char* first, const char* at, const char* last, bool negative, double& value) {
    if (!StartsWithNoCase(at + 1, last, "nf")) return {first, std::errc::invalid_argument};
    const char* end = at + 3;
    if (StartsWithNoCase(end, last, "inity")) end += 5;
    value = negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    return {end, std::errc{}};
}

// `nan` or `nan(n-chars)`, after the sign. `at` is the 'n'. A '(' that is not closed by a ')' after letters, digits and
// underscores alone is left outside the number.
std::from_chars_result ReadNan(const char* first, const char* at, const char* last, bool negative, double& value) {
    if (!StartsWithNoCase(at + 1, last, "an")) return {first, std::errc::invalid_argument};
    const char* end = at + 3;
    if (end != last && *end == '(') {
        for (const char* scan = end + 1; scan != last; ++scan) {
            if (*scan == ')') {
                end = scan + 1;
                break;
            }
            const bool nameChar = *scan == '_' || IsDigit(*scan) || (Lower(*scan) >= 'a' && Lower(*scan) <= 'z');
            if (!nameChar) break;
        }
    }
    value = std::copysign(std::numeric_limits<double>::quiet_NaN(), negative ? -1.0 : 1.0);
    return {end, std::errc{}};
}

} // namespace

std::from_chars_result ParseDoubleFallback(const char* first, const char* last, double& value) {
    const std::from_chars_result invalid{first, std::errc::invalid_argument};
    const char* at = first;
    if (at == last) return invalid;
    const bool negative = *at == '-';
    if (negative && ++at == last) return invalid;

    const char lead = Lower(*at);
    if (lead == 'i') return ReadInfinity(first, at, last, negative, value);
    if (lead == 'n') return ReadNan(first, at, last, negative, value);

    // The number's extent, found here and not by strtod, whose grammar is wider (leading space, '+', hex, "0x1p3").
    bool anyNonzeroDigit = false;
    std::size_t digits = 0;
    while (at != last && IsDigit(*at)) {
        anyNonzeroDigit = anyNonzeroDigit || *at != '0';
        ++digits;
        ++at;
    }
    if (at != last && *at == '.') {
        ++at;
        while (at != last && IsDigit(*at)) {
            anyNonzeroDigit = anyNonzeroDigit || *at != '0';
            ++digits;
            ++at;
        }
    }
    if (digits == 0) return invalid;
    if (at != last && Lower(*at) == 'e') {
        const char* exponent = at + 1;
        if (exponent != last && (*exponent == '+' || *exponent == '-')) ++exponent;
        if (exponent != last && IsDigit(*exponent)) { // an exponent with no digit is not part of the number
            while (exponent != last && IsDigit(*exponent)) ++exponent;
            at = exponent;
        }
    }
    const char* const end = at;

    // Every digit zero: zero (signed) whatever the exponent says, as the library reads it.
    if (!anyNonzeroDigit) {
        value = negative ? -0.0 : 0.0;
        return {end, std::errc{}};
    }

    // strtod on a copy it can read to the end: the text is NUL-terminated, and its '.' is whatever this locale calls
    // the decimal point. strtod rounds to nearest even on every libc the port runs on. errno is left as it was found.
    std::string text(first, end);
    const char* const point = std::localeconv()->decimal_point;
    if (point != nullptr && point[0] != '\0' && !(point[0] == '.' && point[1] == '\0')) {
        const std::size_t dot = text.find('.');
        if (dot != std::string::npos) text.replace(dot, 1, point);
    }
    const int savedErrno = errno;
    errno = 0;
    char* stop = nullptr;
    const double parsed = std::strtod(text.c_str(), &stop);
    errno = savedErrno;
    if (stop != text.c_str() + text.size()) return invalid; // a locale whose notation this does not know: no number

    // ERANGE is not asked: whether a subnormal sets it is the libc's choice. A nonzero mantissa is out of range exactly
    // when it rounds to infinity (too large) or to zero (too small); a subnormal result is a number.
    if (std::isinf(parsed) || parsed == 0.0) return {end, std::errc::result_out_of_range};
    value = parsed;
    return {end, std::errc{}};
}

std::from_chars_result ParseDouble(const char* first, const char* last, double& value) {
#if MAGICPORTALS_NUMBERS_STD_FROM_CHARS
    // MSVC's library writes an infinity or a zero into `value` when it reports out_of_range, libstdc++'s does not:
    // the value is kept unless the number read.
    const double before = value;
    const std::from_chars_result result = std::from_chars(first, last, value);
    if (result.ec != std::errc{}) value = before;
    return result;
#else
    return ParseDoubleFallback(first, last, value);
#endif
}

} // namespace MagicPortals::Numbers
