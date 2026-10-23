#include "sim/Scratch.hpp"

#include <algorithm>
#include <exception>
#include <fstream>
#include <system_error>

#include "sim/Files.hpp"

namespace MagicPortals::Scratch {

std::vector<Candidate> ArrangedFor(std::vector<Candidate> candidates, bool phone) {
    if (phone) {
        std::stable_partition(candidates.begin(), candidates.end(), [](const Candidate& candidate) { return candidate.kind == Kind::UserDataScratch; });
    }
    return candidates;
}

Chosen Choose(const std::vector<Candidate>& candidates, const Probe& probe) {
    Chosen chosen;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const Candidate& candidate = candidates[i];
        std::filesystem::path base;
        try {
            if (candidate.base) base = candidate.base();
        } catch (const std::exception& e) {
            // temp_directory_path() throws when %TEMP% names nothing; a start must survive that.
            chosen.passedOver.push_back(candidate.label + ": " + e.what());
            continue;
        }
        if (base.empty()) {
            chosen.passedOver.push_back(candidate.label + ": not available here");
            continue;
        }
        const std::filesystem::path prisms = base / kPrismFolder;
        std::string why;
        // The prism folder is asked as well as the folder it sits in: a folder that exists and was made by another account is
        // writable in itself (a shared /tmp) and the prisms' folder in it is not.
        if (!probe(base, why) || !probe(prisms, why)) {
            chosen.passedOver.push_back(candidate.label + ": " + base.string() + ": " + why);
            continue;
        }
        chosen.ok = true;
        chosen.base = base;
        chosen.prisms = prisms;
        chosen.label = candidate.label;
        chosen.index = i;
        chosen.note = "scratch folder: " + base.string() + " (" + candidate.label + (candidate.makes ? ", made for it" : "") + "), prisms in " +
                      prisms.string();
        for (const std::string& skipped : chosen.passedOver) chosen.note += "; passed over " + skipped;
        return chosen;
    }
    chosen.note = "no scratch folder";
    for (const std::string& skipped : chosen.passedOver) chosen.note += "; " + skipped;
    return chosen;
}

bool ProbeWritable(const std::filesystem::path& dir, std::string& why) {
    try {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            why = "cannot make " + dir.string() + ": " + ec.message();
            return false;
        }
        if (!std::filesystem::is_directory(dir, ec)) {
            why = dir.string() + " is not a folder";
            return false;
        }
        const std::filesystem::path file = dir / (".write-probe-" + Files::UniqueSuffix() + ".tmp");
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << "probe";
            out.flush();
            if (!out) {
                why = "cannot write " + file.string();
                out.close();
                std::filesystem::remove(file, ec);
                return false;
            }
        }
        std::filesystem::remove(file, ec);
        if (ec) {
            why = "cannot remove " + file.string() + ": " + ec.message();
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        why = e.what();
        return false;
    }
}

} // namespace MagicPortals::Scratch
