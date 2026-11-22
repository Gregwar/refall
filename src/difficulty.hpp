#pragma once
// The difficulties (free play's gravity, the Voyage's velocities, the hyperdrop and the lock delay depend on them).
// EXPERT came after HARD (an older EXPERT was renamed HARD: see Progress for the files that still say so). HARDCORE
// is EXPERT and more: every song at the top velocity, a shorter lock delay, fewer move resets.
enum Difficulty { DIFF_EASY = 0, DIFF_NORMAL, DIFF_HARD, DIFF_EXPERT, DIFF_HARDCORE, DIFF_COUNT };
// EXPERT's rules (no breather in the songs, short warm-ups, hyperdrop from 24, the weaker bonus): EXPERT and HARDCORE.
inline bool expertRules(int d) { return d >= DIFF_EXPERT; }
