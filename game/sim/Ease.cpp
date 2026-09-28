#include "sim/Ease.hpp"

#include "core/DetMath.hpp"

namespace MagicPortals::Ease {

namespace {

constexpr float kPIb = 1.570796327f;

constexpr const char* kLinear = "linear";
constexpr const char* kSmoothEnd = "smooth_end";
constexpr const char* kSmoothBeginning = "smooth_beginning";
constexpr const char* kSmoothBothSides = "smooth_both_sides";

float End(float v) { return Supersonic::DetMath::sin(v * kPIb); }

// 1f - smoothEnd(1f - v), each step in single precision as the script's SUBf does.
float Beginning(float v) { return 1.0f - End(1.0f - v); }

} // namespace

double SmoothEnd(double v) { return static_cast<double>(End(static_cast<float>(v))); }

double SmoothBeginning(double v) { return static_cast<double>(Beginning(static_cast<float>(v))); }

double SmoothBothSides(double v) { return static_cast<double>(Beginning(End(static_cast<float>(v)))); }

double Apply(Filter filter, double v) {
    switch (filter) {
    case Filter::SmoothEnd:
        return SmoothEnd(v);
    case Filter::SmoothBeginning:
        return SmoothBeginning(v);
    case Filter::SmoothBothSides:
        return SmoothBothSides(v);
    case Filter::Linear:
        break;
    }
    return v;
}

bool ParseFilter(const std::string& name, Filter& out) {
    if (name == kLinear) {
        out = Filter::Linear;
    } else if (name == kSmoothEnd) {
        out = Filter::SmoothEnd;
    } else if (name == kSmoothBeginning) {
        out = Filter::SmoothBeginning;
    } else if (name == kSmoothBothSides) {
        out = Filter::SmoothBothSides;
    } else {
        return false;
    }
    return true;
}

} // namespace MagicPortals::Ease
