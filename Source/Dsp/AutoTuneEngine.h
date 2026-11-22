#pragma once
#include "PitchTracker.h"
#include "Scales.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace autoladd
{
struct EngineParams
{
    float amount = 1.0f;         // 0..1   fraction of the correction that is applied (pitch domain)
    float speedMs = 0.0f;        // retune speed in ms, 0 = instant (hard / robotic)
    float robot = 0.0f;          // 0..1   grain-freeze "buzz" on top of the correction
    float formantSemis = 0.0f;   // formant shift in semitones
    float transposeSemis = 0.0f; // output transpose (semitones, incl. fine cents / 100)
    float refA = 440.0f;         // tuning reference for A4
    float sensitivity = 0.2f;    // YIN threshold (lower = stricter voicing decision)
    float gateDb = -60.0f;       // input level under which nothing is tracked
    uint16_t noteMask = 0x0FFF;  // allowed pitch classes, bit 0 = C
};

/** Time-domain pitch-synchronous overlap-add (TD-PSOLA) pitch corrector.
    Analysis marks are placed on the (low-passed) waveform peaks, synthesis marks are placed at exactly
    1 / target-frequency spacing, so the output pitch lands precisely on the target note while the
    formants of the voice stay where they were. */
class AutoTuneEngine
{
public:
    static constexpr int kMaxCh = 2;

    void prepare (double sampleRate, double minPitchHz, int numChannels)
    {
        sr = sampleRate;
        numCh = std::clamp (numChannels, 1, kMaxCh);
        pMax = sr / minPitchHz;
        hSched = (int) std::ceil (pMax);
        latency = (int) std::ceil (3.0 * pMax) + 64;
        fallbackP = std::max (64.0, std::round (0.006 * sr));

        size_t sz = 1;
        while ((double) sz < (double) latency + 4.0 * pMax + 4096.0)
            sz <<= 1;
        mask = (int64_t) sz - 1;
        for (int c = 0; c < kMaxCh; ++c)
        {
            inRing[c].assign (sz, 0.0f);
            acc[c].assign (sz, 0.0f);
        }
        wAcc.assign (sz, 0.0f);
        yRing.assign (sz, 0.0f);
        marks.assign (1024, Mark {});

        tracker.prepare (sr, minPitchHz);
        lpA = (float) (1.0 - std::exp (-2.0 * kPi * 700.0 / sr));
        gdLP = 2.0 * (1.0 - lpA) / lpA;
        fadeCoef = (float) (1.0 - std::exp (-1.0 / (0.010 * sr)));
        reset();
    }

    int getLatency() const { return latency; }
    double getMinPitchSamples() const { return pMax; }

    void reset()
    {
        for (int c = 0; c < kMaxCh; ++c)
        {
            std::fill (inRing[c].begin(), inRing[c].end(), 0.0f);
            std::fill (acc[c].begin(), acc[c].end(), 0.0f);
        }
        std::fill (wAcc.begin(), wAcc.end(), 0.0f);
        std::fill (yRing.begin(), yRing.end(), 0.0f);
        tracker.reset();
        counter = 0;
        curN = 0;
        markCount = 0;
        cursor = 0;
        synthNext = 0.0;
        holdLeft = 0;
        heldMark = -1;
        polarity = 1.0f;
        lp1 = lp2 = 0.0f;
        wasVoiced = false;
        haveSnap = false;
        offS = 0.0;
        lastPs = fallbackP;
        wetGain = 0.0f;
        starved = 0;
        dispVoiced.store (false);
    }

    void setParams (const EngineParams& p)
    {
        params = p;
        holdCount = 1 + (int) std::lround (std::clamp (p.robot, 0.0f, 1.0f) * 6.0f);
        formantRatio = std::exp2 ((double) p.formantSemis / 12.0);
        tracker.setThreshold (p.sensitivity);
        tracker.setGateDb (p.gateDb);
        active = p.amount > 0.001f || std::fabs (p.transposeSemis) > 0.001f
                 || std::fabs (p.formantSemis) > 0.01f || p.robot > 0.01f;
    }

    /** in: numCh samples. wet / dry: numCh samples each (both delayed by getLatency()). */
    void processSample (const float* in, float* wet, float* dry)
    {
        const int64_t n = counter;
        curN = n;
        float mono = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            inRing[c][(size_t) (n & mask)] = in[c];
            mono += in[c];
        }
        mono /= (float) numCh;

        tracker.push (mono);
        lp1 += lpA * (mono - lp1);
        lp2 += lpA * (lp1 - lp2);
        yRing[(size_t) (n & mask)] = lp2;

        generateMarks (n);
        scheduleGrains (n);

        const size_t idx = (size_t) (n & mask);
        const float inv = 1.0f / std::max (wAcc[idx], 0.15f);
        wetGain += fadeCoef * ((active ? 1.0f : 0.0f) - wetGain);
        for (int c = 0; c < numCh; ++c)
        {
            const float d = inRing[c][(size_t) ((n - latency) & mask)];
            const float w = acc[c][idx] * inv;
            dry[c] = d;
            wet[c] = d + wetGain * (w - d);
            acc[c][idx] = 0.0f;
        }
        wAcc[idx] = 0.0f;
        ++counter;
    }

    // Display / test info (written by the audio thread, read anywhere)
    float getDisplayNoteIn() const { return dispIn.load (std::memory_order_relaxed); }
    float getDisplayNoteOut() const { return dispOut.load (std::memory_order_relaxed); }
    bool getDisplayVoiced() const { return dispVoiced.load (std::memory_order_relaxed); }
    int64_t getStarvedReads() const { return starved; }

private:
    struct Mark
    {
        double pos = 0.0;
        float period = 0.0f;
        bool voiced = false;
    };

    const Mark& markAt (int64_t i) const { return marks[(size_t) (i & 1023)]; }
    void pushMark (double pos, float period, bool voiced)
    {
        marks[(size_t) (markCount & 1023)] = { pos, period, voiced };
        ++markCount;
    }
    float yAt (int64_t i) const { return yRing[(size_t) (i & mask)]; }

    double findPeak (double pred, double halfWin, bool detectPolarity)
    {
        const double centre = pred + gdLP;
        const int64_t lo = (int64_t) std::floor (centre - halfWin);
        const int64_t hi = (int64_t) std::ceil (centre + halfWin);
        float mx = -1.0e30f, mn = 1.0e30f;
        for (int64_t i = lo; i <= hi; ++i)
        {
            mx = std::max (mx, yAt (i));
            mn = std::min (mn, yAt (i));
        }
        if (detectPolarity)
            polarity = (mx >= -mn) ? 1.0f : -1.0f;
        const float amp = std::max (std::max (mx, -mn), 1.0e-9f);

        float best = -1.0e30f;
        int64_t bi = lo;
        for (int64_t i = lo; i <= hi; ++i)
        {
            const float dist = (float) ((double) i - centre) / (float) halfWin;
            const float score = polarity * yAt (i) - 0.10f * amp * dist * dist;
            if (score > best) { best = score; bi = i; }
        }
        double shift = 0.0;
        if (bi > lo && bi < hi)
        {
            const float a = polarity * yAt (bi - 1), b = polarity * yAt (bi), c = polarity * yAt (bi + 1);
            const float den = a - 2.0f * b + c;
            if (std::fabs (den) > 1.0e-12f)
                shift = std::clamp (0.5 * (double) (a - c) / (double) den, -1.0, 1.0);
        }
        return (double) bi + shift - gdLP;
    }

    void generateMarks (int64_t n)
    {
        for (int guard = 0; guard < 64; ++guard)
        {
            if (! tracker.hasEstimates())
                return;
            if (markCount == 0)
            {
                pushMark (0.0, (float) fallbackP, false);
                continue;
            }
            const Mark last = markAt (markCount - 1);
            const auto lk = tracker.lookup (last.pos);
            const bool voiced = lk.voiced;
            const double P = voiced ? (double) lk.period : fallbackP;
            const double pred = last.pos + P;
            if (pred > tracker.newestCentre())
                return;
            if (pred + 0.5 * P + gdLP + 4.0 > (double) n)
                return;
            double pos = pred;
            if (voiced)
                pos = findPeak (pred, (last.voiced ? 0.25 : 0.5) * P, ! last.voiced);
            pushMark (pos, (float) P, voiced);
        }
    }

    int64_t chooseMark (double tau)
    {
        if (markCount == 0)
            return -1;
        int64_t i = std::max<int64_t> (cursor, markCount - 1023);
        i = std::min<int64_t> (i, markCount - 1);
        while (i + 1 < markCount && std::fabs (markAt (i + 1).pos - tau) <= std::fabs (markAt (i).pos - tau))
            ++i;
        cursor = i;
        return i;
    }

    double snapNote (double note)
    {
        const int base = (int) std::floor (note);
        double best = 1.0e9;
        int bestP = base;
        for (int p = base - 6; p <= base + 7; ++p)
        {
            const int pc = ((p % 12) + 12) % 12;
            if ((params.noteMask >> pc) & 1)
            {
                const double dd = std::fabs ((double) p - note);
                if (dd < best) { best = dd; bestP = p; }
            }
        }
        if (haveSnap && bestP != lastSnap)
        {
            const int pc = ((lastSnap % 12) + 12) % 12;
            if (((params.noteMask >> pc) & 1) && std::fabs ((double) lastSnap - note) <= best + 0.12)
                bestP = lastSnap; // hysteresis: don't flip-flop around the boundary between two notes
        }
        lastSnap = bestP;
        haveSnap = true;
        return (double) bestP;
    }

    double computeSynthPeriod (double periodIn)
    {
        const double fIn = sr / periodIn;
        const double noteIn = 69.0 + 12.0 * std::log2 (fIn / (double) params.refA);
        double target = noteIn;
        if (params.noteMask != 0)
        {
            const double snapped = snapNote (noteIn);
            const double offTarget = (snapped - noteIn) * (double) std::clamp (params.amount, 0.0f, 1.0f);
            const double tauS = (double) params.speedMs * 0.001 * sr;
            if (! wasVoiced || tauS < 1.0)
                offS = offTarget;
            else
                offS = offTarget + std::exp (-lastPs / tauS) * (offS - offTarget);
            target = noteIn + offS;
        }
        wasVoiced = true;
        const double noteOut = target + (double) params.transposeSemis;
        const double ratio = std::clamp (std::exp2 ((noteOut - noteIn) / 12.0), 0.4, 2.5);
        const double Ps = periodIn / ratio;
        lastPs = Ps;
        dispIn.store ((float) noteIn, std::memory_order_relaxed);
        dispOut.store ((float) (noteIn + 12.0 * std::log2 (ratio)), std::memory_order_relaxed);
        dispVoiced.store (true, std::memory_order_relaxed);
        return Ps;
    }

    void scheduleGrains (int64_t n)
    {
        for (int guard = 0; guard < 256 && synthNext - (double) hSched <= (double) n; ++guard)
        {
            const double s = synthNext;
            const double tau = s - (double) latency;
            const auto lk = tracker.lookup (tau);
            double Pa = fallbackP, Ps = fallbackP, centre = tau;
            bool voiced = lk.voiced;
            int64_t mk = -1;
            if (voiced)
            {
                if (holdLeft > 0 && heldMark >= 0)
                {
                    mk = heldMark;
                    --holdLeft;
                }
                else
                {
                    mk = chooseMark (tau);
                    heldMark = mk;
                    holdLeft = holdCount - 1;
                }
                voiced = mk >= 0;
            }
            if (voiced)
            {
                centre = markAt (mk).pos;
                Pa = (double) lk.period;
                Ps = computeSynthPeriod (Pa);
            }
            else
            {
                wasVoiced = false;
                haveSnap = false;
                holdLeft = 0;
                heldMark = -1;
                lastPs = fallbackP;
                dispVoiced.store (false, std::memory_order_relaxed);
            }
            renderGrain (s, centre, Pa, Ps);
            synthNext += Ps;
        }
    }

    float readCubic (int ch, double pos)
    {
        const int64_t i = (int64_t) std::floor (pos);
        const float f = (float) (pos - (double) i);
        if (i + 2 > curN)
            ++starved;
        auto at = [&] (int64_t k) { return k > curN ? 0.0f : inRing[ch][(size_t) (k & mask)]; };
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    void renderGrain (double s, double centre, double Pa, double Ps)
    {
        const double phi = formantRatio;
        const double Ho = std::clamp (std::max (Pa / phi, 0.75 * Ps), 16.0, (double) hSched);
        const int64_t i0 = (int64_t) std::floor (s);
        const double frac = s - (double) i0;
        const int K = (int) std::ceil (Ho) + 1;
        const double ang = kPi / Ho;
        for (int k = -K; k <= K; ++k)
        {
            const int64_t p = i0 + k;
            if (p < curN)
                continue;
            const double u = (double) k - frac;
            if (std::fabs (u) >= Ho)
                continue;
            const float w = (float) (0.5 + 0.5 * std::cos (u * ang));
            const double rp = centre + u * phi;
            const size_t pi = (size_t) (p & mask);
            wAcc[pi] += w;
            for (int c = 0; c < numCh; ++c)
                acc[c][pi] += w * readCubic (c, rp);
        }
    }

    // configuration
    double sr = 44100.0, pMax = 600.0, fallbackP = 265.0, gdLP = 19.0;
    int numCh = 2, latency = 1900, hSched = 600, holdCount = 1;
    int64_t mask = 0;
    double formantRatio = 1.0;
    float lpA = 0.09f, fadeCoef = 0.002f;
    EngineParams params;
    bool active = true;

    // state
    PitchTracker tracker;
    std::vector<float> inRing[kMaxCh], acc[kMaxCh], wAcc, yRing;
    std::vector<Mark> marks;
    int64_t counter = 0, curN = 0, markCount = 0, cursor = 0, heldMark = -1, starved = 0;
    double synthNext = 0.0, offS = 0.0, lastPs = 265.0;
    int holdLeft = 0, lastSnap = 0;
    bool wasVoiced = false, haveSnap = false;
    float polarity = 1.0f, lp1 = 0.0f, lp2 = 0.0f, wetGain = 0.0f;

    std::atomic<float> dispIn { 0.0f }, dispOut { 0.0f };
    std::atomic<bool> dispVoiced { false };
};
} // namespace autoladd
