#include "sim/LevelStart.hpp"

#include <algorithm>
#include <utility>

namespace MagicPortals::LevelStart {

void Machine::Note(EventKind kind, int contact, std::string detail) {
    Event event;
    event.kind = kind;
    event.tick = m_ticks;
    event.draw = m_draws;
    event.contact = contact;
    event.detail = std::move(detail);
    m_events.push_back(std::move(event));
    if (m_events.size() > kMostEvents) m_events.erase(m_events.begin(), m_events.begin() + static_cast<std::ptrdiff_t>(m_events.size() - kMostEvents));
}

bool Machine::Begin(const Request& request) {
    if (m_phase != Phase::Idle) {
        Note(EventKind::BeginRefused, -1, std::string("a start is in progress (") + Name(m_phase) + ")");
        return false;
    }
    m_request = request;
    m_phase = Phase::CoverWanted;
    m_settlingTickOpen = false;
    // Every finger that is down now pressed before the level's first frame: it is ignored until it lifts.
    for (const int id : m_down) {
        if (m_ignored.insert(id).second) Note(EventKind::ContactIgnored, id, "down when the start was asked for");
    }
    m_down.clear();
    Note(EventKind::Begun);
    return true;
}

Frame Machine::Draw() {
    ++m_draws;
    switch (m_phase) {
    case Phase::CoverWanted:
        m_phase = Phase::CoverIssued;
        ++m_coverFrames;
        Note(EventKind::CoverIssued);
        return Frame::Cover;
    case Phase::CoverIssued:
    case Phase::Loading:
        ++m_coverFrames;
        return Frame::Cover;
    case Phase::AwaitFirstFrame:
        m_phase = Phase::Settling;
        m_settlingTickOpen = false;
        Note(EventKind::FirstFrameIssued);
        return Frame::Normal;
    case Phase::Idle:
    case Phase::Settling:
        break;
    }
    return Frame::Normal;
}

Action Machine::Tick() {
    ++m_ticks;
    Action action;
    switch (m_phase) {
    case Phase::CoverIssued:
        m_phase = Phase::Loading;
        action.load = true;
        action.request = m_request;
        Note(EventKind::LoadStarted);
        break;
    case Phase::Settling:
        if (!m_settlingTickOpen) {
            // This tick is the settling tick: what the system queued during the stall is read in it.
            m_settlingTickOpen = true;
        } else {
            m_phase = Phase::Idle;
            m_settlingTickOpen = false;
            Note(EventKind::Settled);
        }
        break;
    case Phase::Idle:
    case Phase::CoverWanted:
    case Phase::Loading:
    case Phase::AwaitFirstFrame:
        break;
    }
    return action;
}

void Machine::Loaded(bool ok) {
    if (m_phase != Phase::Loading) return;
    if (ok) {
        m_phase = Phase::AwaitFirstFrame;
        Note(EventKind::Loaded);
    } else {
        m_phase = Phase::Idle;
        Note(EventKind::LoadRefused);
    }
}

Verdict Machine::Filter(const Contact& contact) {
    if (contact.id == kNoContact) return Verdict::Pass;
    if (m_cacheTick != m_ticks) {
        m_cache.clear();
        m_cacheTick = m_ticks;
    }
    for (const auto& known : m_cache) {
        if (known.first == contact.id) return known.second;
    }
    const auto answer = [&](Verdict verdict) {
        m_cache.emplace_back(contact.id, verdict);
        return verdict;
    };
    // Nothing there: the contact lifted (or never was). An ignored contact that is gone is forgotten.
    if (!contact.pressed && !contact.held && !contact.released) {
        m_ignored.erase(contact.id);
        m_down.erase(contact.id);
        return answer(Verdict::Pass);
    }
    bool ignored = m_ignored.count(contact.id) != 0;
    if (contact.pressed) {
        // A press edge is a NEW contact: whatever this id was before has lifted, unseen.
        m_ignored.erase(contact.id);
        ignored = false;
        if (m_phase != Phase::Idle) {
            m_ignored.insert(contact.id);
            ignored = true;
            Note(EventKind::ContactIgnored, contact.id, std::string("pressed while the start was ") + Name(m_phase));
        }
    } else if (!ignored && m_phase != Phase::Idle) {
        // Down, or lifting, with no press edge seen: its press was before what this machine has seen, so before the level's first frame.
        m_ignored.insert(contact.id);
        ignored = true;
        Note(EventKind::ContactIgnored, contact.id, std::string("found down while the start was ") + Name(m_phase));
    }
    if (ignored) {
        m_down.erase(contact.id);
        if (contact.released || !contact.held) {
            m_ignored.erase(contact.id);
            Note(EventKind::ContactLifted, contact.id, "its release was swallowed");
        }
        return answer(Verdict::Ignore);
    }
    if (contact.held && !contact.released) {
        m_down.insert(contact.id);
    } else {
        m_down.erase(contact.id);
    }
    return answer(Verdict::Pass);
}

std::vector<Event> Machine::TakeEvents() {
    std::vector<Event> taken;
    taken.swap(m_events);
    return taken;
}

const char* Name(Phase phase) {
    switch (phase) {
    case Phase::Idle:
        return "idle";
    case Phase::CoverWanted:
        return "waiting for the cover";
    case Phase::CoverIssued:
        return "covered";
    case Phase::Loading:
        return "loading";
    case Phase::AwaitFirstFrame:
        return "waiting for the first frame";
    case Phase::Settling:
        return "settling";
    }
    return "?";
}

std::string Describe(const Event& event) {
    const std::string at = " (tick " + std::to_string(event.tick) + ", draw " + std::to_string(event.draw) + ")";
    switch (event.kind) {
    case EventKind::Begun:
        return "level start: asked for" + at;
    case EventKind::BeginRefused:
        return "level start: a second ask was refused: " + event.detail + at;
    case EventKind::CoverIssued:
        return "level start: cover issued to the draw" + at;
    case EventKind::LoadStarted:
        return "level start: load began, after the cover was drawn" + at;
    case EventKind::Loaded:
        return "level start: loaded" + at;
    case EventKind::LoadRefused:
        return "level start: the level was refused; the start is over" + at;
    case EventKind::FirstFrameIssued:
        return "level start: the level's first frame issued" + at;
    case EventKind::Settled:
        return "level start: settled; a press is the player's again" + at;
    case EventKind::ContactIgnored:
        return "level start: contact " + std::to_string(event.contact) + " ignored: " + event.detail + at;
    case EventKind::ContactLifted:
        return "level start: contact " + std::to_string(event.contact) + " lifted: " + event.detail + at;
    }
    return "level start: ?";
}

} // namespace MagicPortals::LevelStart
