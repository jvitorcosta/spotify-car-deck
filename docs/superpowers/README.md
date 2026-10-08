# Design records

Specs (what and why) and plans (step-by-step tasks) from the structured design workflow, in
date order. Later specs extend earlier ones; the README "Design & performance history"
summarises the outcome of each.

| Date | Spec | Plan | Status |
|---|---|---|---|
| 2026-10-04 | `specs/2026-10-04-pokedeck-esp32-spotify-design.md` | `plans/2026-10-04-spotify-pokemon-deck.md` | implemented; walker superseded, touch not built |
| 2026-10-06 | `specs/2026-10-06-merged-status-panel-pmd-walker-design.md` | `plans/2026-10-06-gen3-battle-ui-pmd-walker.md` (Parts 1–4) | implemented |
| 2026-10-06 | `specs/2026-10-06-perf-network-task-design.md` | same plan, Part 2 | implemented |
| 2026-10-06 | `specs/2026-10-06-memory-resilience-design.md` | same plan, Part 3 | implemented |
| 2026-10-07 | `specs/2026-10-07-cjk-text-design.md` | same plan, Part 4 | implemented |
| 2026-10-07 | `specs/2026-10-07-lyrics-reliability-design.md` | `plans/2026-10-07-lyrics-reliability.md` | implemented (f45da1a) |
| 2026-10-08 | `specs/2026-10-08-genre-badge-design.md` | `plans/2026-10-08-genre-badge.md` | implemented (3ccf495, aca6c90, 6836503) |

Plans keep the machine-specific commands they were executed with (`.devtools\…`, `COM11`);
see the main README for portable commands. Code review and cleanup: `../review/`.
