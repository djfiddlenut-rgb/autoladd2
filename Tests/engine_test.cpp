// Offline test harness for the autoladd DSP core (no JUCE needed).
//   g++ -O2 -std=c++17 -I../Source/Dsp engine_test.cpp -o engine_test && ./engine_test <outdir>
// Writes raw float32 mono files that analyze.py inspects.
#include "AutoTuneEngine.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <random>
#include <string>

using namespace autoladd;

static const double SR = 44100.0;

// Synthetic "voice": glottal-ish pulse train -> three formant resonators. f0 given per sample by contour.
static std::vector<float> makeVoice (int numSamples, const std::function<double (double)>& f0At, double level = 0.3)
{
    std::vector<float> out ((size_t) numSamples);
    double phase = 0.0;
    struct Res { double a1, a2, g, z1 = 0, z2 = 0; };
    auto mk = [] (double fc, double bw) {
        const double r = std::exp (-kPi * bw / SR);
        return Res { 2.0 * r * std::cos (2.0 * kPi * fc / SR), -r * r, 1.0 - r };
    };
    Res f[3] = { mk (700, 110), mk (1200, 140), mk (2600, 200) };
    const double gains[3] = { 1.0, 0.6, 0.3 };
    double prevSrc = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const double t = i / SR;
        phase += f0At (t) / SR;
        if (phase >= 1.0) phase -= 1.0;
        // Rosenberg-like pulse: smooth rise, fast fall, then derivative (radiation) for a bright source
        const double ph = phase;
        double g = ph < 0.6 ? 0.5 * (1.0 - std::cos (kPi * ph / 0.6)) : (ph < 0.8 ? std::cos (kPi * 0.5 * (ph - 0.6) / 0.2) : 0.0);
        const double src = g - prevSrc;
        prevSrc = g;
        double y = 0.0;
        for (int k = 0; k < 3; ++k)
        {
            const double v = f[k].g * src * 8.0 + f[k].a1 * f[k].z1 + f[k].a2 * f[k].z2;
            f[k].z2 = f[k].z1;
            f[k].z1 = v;
            y += gains[k] * v;
        }
        out[(size_t) i] = (float) (level * y);
    }
    // normalise RMS to the requested level
    double e = 0;
    for (float v : out) e += (double) v * v;
    const double rms = std::sqrt (e / numSamples);
    for (auto& v : out) v = (float) (v * level / std::max (rms, 1e-9));
    return out;
}

struct RunResult { std::vector<float> wetL, wetR, dry; int latency = 0; int64_t starved = 0; };

static RunResult run (const std::vector<float>& in, const EngineParams& p, double minHz = 85.0, bool stereoDiff = false)
{
    AutoTuneEngine eng;
    eng.prepare (SR, minHz, 2);
    eng.setParams (p);
    RunResult r;
    r.latency = eng.getLatency();
    const size_t N = in.size();
    r.wetL.resize (N); r.wetR.resize (N); r.dry.resize (N);
    for (size_t i = 0; i < N; ++i)
    {
        float inp[2] = { in[i], stereoDiff ? in[i] * 0.5f : in[i] };
        float w[2], d[2];
        eng.processSample (inp, w, d);
        r.wetL[i] = w[0]; r.wetR[i] = w[1]; r.dry[i] = d[0];
    }
    r.starved = eng.getStarvedReads();
    return r;
}

static void save (const std::string& path, const std::vector<float>& v)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    std::fwrite (v.data(), sizeof (float), v.size(), f);
    std::fclose (f);
}

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)

static bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x) || std::fabs (x) > 8.0f) return false;
    return true;
}

static double rmsOf (const std::vector<float>& v, size_t a, size_t b)
{
    double e = 0; for (size_t i = a; i < b; ++i) e += (double) v[i] * v[i];
    return std::sqrt (e / (double) (b - a));
}

int main (int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : ".";
    const int N = (int) (SR * 4.0);

    // 1) Vibrato voice around 207 Hz (G#3 = 207.65 Hz) -> chromatic hard snap
    {
        std::printf ("[1] hard snap, chromatic, vibrato voice\n");
        auto in = makeVoice (N, [] (double t) { return 207.0 * std::pow (2.0, 0.3 / 12.0 * std::sin (2 * kPi * 5.5 * t)); });
        EngineParams p; p.amount = 1.0f; p.speedMs = 0.0f;
        auto r = run (in, p);
        save (out + "/t1_in.f32", in); save (out + "/t1_wet.f32", r.wetL); save (out + "/t1_dry.f32", r.dry);
        CHECK (allFinite (r.wetL), "output finite");
        CHECK (r.starved == 0, "no starved reads");
        const double rin = rmsOf (in, 20000, 160000), rout = rmsOf (r.wetL, 20000 + r.latency, 160000);
        CHECK (std::fabs (20.0 * std::log10 (rout / rin)) < 2.0, "level preserved within 2 dB");
        std::printf ("  latency = %d samples (%.1f ms)\n", r.latency, 1000.0 * r.latency / SR);
    }

    // 2) Sweep 150 -> 330 Hz into C major, instant snap
    {
        std::printf ("[2] sweep into C major\n");
        auto in = makeVoice (N, [] (double t) { return 150.0 * std::pow (330.0 / 150.0, t / 4.0); });
        EngineParams p; p.noteMask = absoluteMaskFor (1, 0);
        auto r = run (in, p);
        save (out + "/t2_in.f32", in); save (out + "/t2_wet.f32", r.wetL);
        CHECK (allFinite (r.wetL), "output finite");
        CHECK (r.starved == 0, "no starved reads");
    }

    // 3) amount = 0.5 (half the correction)
    {
        std::printf ("[3] amount 50%%\n");
        auto in = makeVoice (N, [] (double) { return 213.0; }); // between G#3 (207.65) and A3 (220)
        EngineParams p; p.amount = 0.5f;
        auto r = run (in, p);
        save (out + "/t3_wet.f32", r.wetL);
        CHECK (allFinite (r.wetL), "output finite");
    }

    // 4) Unvoiced noise passes through at the same level
    {
        std::printf ("[4] noise passthrough\n");
        std::mt19937 rng (1);
        std::normal_distribution<float> nd (0.0f, 0.1f);
        std::vector<float> in ((size_t) N);
        for (auto& v : in) v = nd (rng);
        EngineParams p;
        auto r = run (in, p);
        CHECK (allFinite (r.wetL), "output finite");
        const double rin = rmsOf (in, 30000, 150000), rout = rmsOf (r.wetL, 30000 + r.latency, 150000);
        std::printf ("  in %.4f out %.4f\n", rin, rout);
        CHECK (std::fabs (20.0 * std::log10 (rout / rin)) < 1.5, "noise level preserved");
    }

    // 5) Latency / impulse alignment of the dry path
    {
        std::printf ("[5] dry-path latency\n");
        std::vector<float> in ((size_t) 20000, 0.0f);
        in[5000] = 1.0f;
        EngineParams p; p.amount = 0.0f;
        auto r = run (in, p);
        size_t pk = 0; for (size_t i = 0; i < r.dry.size(); ++i) if (std::fabs (r.dry[i]) > std::fabs (r.dry[pk])) pk = i;
        CHECK ((int) pk == 5000 + r.latency, "dry impulse delayed by reported latency");
    }

    // 6) Robot hold + formant + transpose + stereo stress, random params
    {
        std::printf ("[6] stress with random parameters\n");
        auto in = makeVoice (N, [] (double t) { return 140.0 * std::pow (2.0, (std::sin (2 * kPi * 0.7 * t) + 0.3 * std::sin (2 * kPi * 7 * t)) / 3.0); });
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> U (0.0f, 1.0f);
        bool ok = true; int64_t starved = 0;
        for (int it = 0; it < 12; ++it)
        {
            EngineParams p;
            p.amount = U (rng); p.speedMs = U (rng) * 300.0f; p.robot = U (rng);
            p.formantSemis = (U (rng) - 0.5f) * 12.0f; p.transposeSemis = (float) std::lround ((U (rng) - 0.5f) * 24.0f);
            p.sensitivity = 0.1f + 0.2f * U (rng);
            p.noteMask = (uint16_t) (1 + (rng() % 4095));
            auto r = run (in, p, 85.0, true);
            ok = ok && allFinite (r.wetL) && allFinite (r.wetR);
            starved += r.starved;
        }
        CHECK (ok, "all outputs finite and bounded");
        CHECK (starved == 0, "no starved reads across random parameter sets");
    }

    // 7) robot mode wav for listening analysis
    {
        std::printf ("[7] robot render\n");
        auto in = makeVoice (N, [] (double t) { return 190.0 * std::pow (2.0, (std::sin (2 * kPi * 0.5 * t)) / 4.0); });
        EngineParams p; p.robot = 1.0f; p.noteMask = absoluteMaskFor (6, 9);
        auto r = run (in, p);
        save (out + "/t7_in.f32", in); save (out + "/t7_wet.f32", r.wetL);
        CHECK (allFinite (r.wetL), "robot output finite");
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
