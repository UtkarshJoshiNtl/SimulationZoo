#pragma once
// Deterministic random number generator.
//
// Every exhibit receives exactly one Rng, seeded from the run seed. Given the
// same seed and the same call order, it produces the same stream on every
// platform. Do not call the global rand() anywhere.
//
// Algorithm: splitmix64 for seeding + xoshiro256**. See
// https://prng.di.unimi.it/ for the reference.

#include <cstdint>

namespace zoo {

class Rng {
public:
    explicit Rng(uint64_t seed) { reseed(seed); }

    void reseed(uint64_t seed) {
        uint64_t x = seed;
        for (int i = 0; i < 4; ++i) s_[i] = splitmix64(x);
    }

    uint64_t next_u64() {
        const uint64_t result = rotl(s_[1] * 5, 7) * 9;
        const uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

    // Uniform in [0, 1). Uses the top 53 bits, so the result is exactly
    // representable as a double.
    double next_double() {
        return static_cast<double>(next_u64() >> 11) * (1.0 / 9007199254740992.0);
    }

    // Uniform in [lo, hi).
    double range(double lo, double hi) { return lo + (hi - lo) * next_double(); }

    // Uniform integer in [0, n). n must be > 0.
    uint32_t below(uint32_t n) {
        return static_cast<uint32_t>(next_u64() % static_cast<uint64_t>(n));
    }

private:
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

    static uint64_t splitmix64(uint64_t& x) {
        x += 0x9E3779B97F4A7C15ull;
        uint64_t z = x;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    uint64_t s_[4] = {0, 0, 0, 0};
};

} // namespace zoo
