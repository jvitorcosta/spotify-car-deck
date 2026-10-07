#pragma once
// Small set of Pokedex numbers with oldest-first eviction. Used to remember, for this boot,
// which Pokemon have no usable PMD walk sheet (no AnimData, no Walk anim, sheet too big or
// unsupported) so later picks go straight to the fallback sprite instead of downloading the
// same sheet again. PURE, host-tested.
namespace dexset {
class Set {
public:
    static constexpr int N = 32;
    bool has(int dex) const;
    void add(int dex);   // ignores dex <= 0 and numbers already present
private:
    int v_[N] = {};
    int next_ = 0;
};
}
