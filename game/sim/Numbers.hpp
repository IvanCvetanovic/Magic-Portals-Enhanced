#pragma once

// Reading a decimal number out of text, the same way on every platform (planning step 199, PORT-1).
//
// The tscn reader, the lighting table and the controls flag read their numbers with std::from_chars, which is
// locale independent and correctly rounded, and which MSVC, libstdc++ 11+ and libc++ 20+ provide for double. The
// Android NDK r28's libc++ (19) declares that overload DELETED, and Apple's libc++ is expected to be the same, so
// the port could not even compile there. ParseDouble is the one place that knows: it calls std::from_chars where
// the library has the floating-point overload (the feature-test macro __cpp_lib_to_chars is the library saying so)
// and ParseDoubleFallback where it does not. The three sites keep their own accept/reject rules; only the call
// moved.
//
// THE GRAMMAR, which is std::from_chars's chars_format::general and so what the port has always read:
//   [-] ( digits [ . [digits] ] | . digits ) [ (e|E) [+|-] digits ]
//   [-] ( inf | infinity | nan | nan( n-chars ) )     (letters in either case)
// No leading space, no '+', no hex ("0x10" reads the 0 and stops at the x), and an exponent that has no digit
// ("1e", "1e+") is not part of the number: the number ends before the 'e'. The result is the one std::from_chars
// gives: ec == errc{} and ptr past the number; ec == invalid_argument and ptr == first when no number starts there;
// ec == result_out_of_range and ptr past the number when the value is not representable (a nonzero mantissa that
// rounds to zero, as MSVC's library decides it - a subnormal is representable - or beyond the largest double).
// The value is written ONLY when ec == errc{}: the standard says so, MSVC's library writes an infinity or a zero on
// out_of_range and libstdc++'s leaves it, and no reader may depend on the difference.

#include <charconv>
#include <system_error>
#include <version>

namespace MagicPortals::Numbers {

// Whether this build's standard library parses doubles itself (and ParseDouble is std::from_chars).
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
#define MAGICPORTALS_NUMBERS_STD_FROM_CHARS 1 // a preprocessor test, because a deleted overload is an error even in a discarded branch
inline constexpr bool kStandardFromChars = true;
#else
#define MAGICPORTALS_NUMBERS_STD_FROM_CHARS 0
inline constexpr bool kStandardFromChars = false;
#endif

// std::from_chars(first, last, value) as chars_format::general, on every platform. See the grammar above.
std::from_chars_result ParseDouble(const char* first, const char* last, double& value);

// The parser used where the library has no floating-point from_chars, compiled everywhere so that the suite
// (tests/test_mp_numbers.cpp) holds it against std::from_chars on the platforms that have both. It checks the
// grammar itself and takes the value from strtod, which is correctly rounded on glibc, bionic, the UCRT and Apple's
// libc but reads its decimal point from LC_NUMERIC: the text handed to it carries the locale's own decimal point in
// place of the '.', so the answer does not depend on the locale (neither the game nor the engine calls setlocale,
// so the locale is "C" in practice; this makes it true by construction).
std::from_chars_result ParseDoubleFallback(const char* first, const char* last, double& value);

} // namespace MagicPortals::Numbers
