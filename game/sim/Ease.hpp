#pragma once

// The original's easing filters (ETHFramework/Interpolator.angelscript), which a
// WaypointManager applies to each leg by the leg's own waypoint and which several
// callbacks call outright on a bias of their own. Pure arithmetic, read from the
// listing (out/parity/asbc/all_functions.txt, md5 48fe65ce...):
//
//   smoothEnd(v)       = sin(v x PIb)                   bytes 313899..313971 ins 1-4
//   smoothBeginning(v) = 1 - smoothEnd(1 - v)           bytes 313971..314064 ins 1-7
//   smoothBothSides(v) = smoothBeginning(smoothEnd(v))  bytes 314064..314142 ins 1-5
//
// PIb is the engine's registered const float, 1.570796327f (GameMath.h:40,
// ETHScriptObjRegister.cpp:509-517; decode D9, 00_order 8.10). The script computes
// in single precision, and so does this - through DetMath, as every trigonometric
// call on the simulation's side is: an eased path is where a boss shoots from and
// where a blast reaches it, so the value reaches the simulation and must be the
// same on every machine.

#include <string>

namespace MagicPortals::Ease {

// A waypoint's `filter`: the function its leg is eased by. `Linear` is
// defaultFilter, which returns its argument.
enum class Filter { Linear, SmoothEnd, SmoothBeginning, SmoothBothSides };

double SmoothEnd(double v);
double SmoothBeginning(double v);
double SmoothBothSides(double v);
double Apply(Filter filter, double v);

// The names the port's JSON gives them: "linear", "smooth_end",
// "smooth_beginning", "smooth_both_sides". False for anything else.
bool ParseFilter(const std::string& name, Filter& out);

} // namespace MagicPortals::Ease
