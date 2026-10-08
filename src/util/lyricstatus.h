#pragma once
#include <cstdint>
// Lyrics outcome, shared by the network task (result of one fetch attempt) and the UI (what
// the dialogue box shows). Spec: docs/superpowers/specs/2026-10-07-lyrics-reliability-design.md.
// PURE, host-tested (test_netplan).
namespace lyricstatus {
enum class Result : uint8_t { Synced, Plain, Instrumental, NotFound, TempError };
enum class Status : uint8_t { Searching, Retrying, Synced, Plain, Instrumental, None };
// What the UI shows after an attempt; final = no more attempts for this track.
inline Status statusFor(Result r, bool final) {
    switch (r) {
        case Result::Synced:       return Status::Synced;
        case Result::Plain:        return Status::Plain;
        case Result::Instrumental: return Status::Instrumental;
        case Result::NotFound:     return Status::None;
        case Result::TempError:    return final ? Status::None : Status::Retrying;
    }
    return Status::None;
}
}
