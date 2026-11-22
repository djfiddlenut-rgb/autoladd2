#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace autoladd
{
constexpr double kPi = 3.14159265358979323846;

/** Real-time monophonic pitch tracker (YIN on a 4x-decimated signal).
    Estimates are stored in a short history, time-stamped with the *centre* of the analysis
    window, so the engine (which runs with latency) can look up a pitch that is centred on
    the audio it is about to re-synthesise instead of one that lags behind it. */
class PitchTracker
{
public:
    struct Result
    {
        bool voiced = false;
        float period = 0.0f; // in input-rate samples
    };

    void prepare (double sampleRate, double minHz, double maxHz = 1000.0)
    {
        sr = sampleRate;
        dec = std::max (1, (int) std::lround (sr / 11025.0));
        srD = sr / dec;
        tauMaxD = (int) std::ceil (srD / minHz) + 2;
        tauMinD = std::max (2, (int) std::floor (srD / maxHz));
        winD = tauMaxD;
        spanD = winD + tauMaxD + 2;

        // Windowed-sinc low-pass for the decimator (cut-off ~0.45 of the decimated Nyquist-ish band)
        firLen = (dec == 1) ? 1 : 8 * dec + 1;
        fir.assign ((size_t) firLen, 0.0f);
        if (dec == 1)
            fir[0] = 1.0f;
        else
        {
            const double fc = 0.45 / dec; // cycles per input sample
            double sum = 0.0;
            for (int i = 0; i < firLen; ++i)
            {
                const double k = i - (firLen - 1) * 0.5;
                const double sinc = (k == 0.0) ? 2.0 * fc : std::sin (2.0 * kPi * fc * k) / (kPi * k);
                const double win = 0.5 - 0.5 * std::cos (2.0 * kPi * i / (firLen - 1));
                fir[(size_t) i] = (float) (sinc * win);
                sum += fir[(size_t) i];
            }
            for (auto& c : fir) c = (float) (c / sum);
        }
        groupDelay = (firLen - 1) * 0.5;

        fullRing.assign (512, 0.0f);
        fullMask = 511;
        size_t dsz = 1;
        while ((int) dsz < spanD + 2 * hopD + 8) dsz <<= 1;
        decRing.assign (dsz, 0.0f);
        decMask = (int64_t) dsz - 1;
        seg.assign ((size_t) spanD + 4, 0.0f);
        d.assign ((size_t) tauMaxD + 3, 0.0f);
        dn.assign ((size_t) tauMaxD + 3, 1.0f);
        hist.assign (256, Entry {});
        histMask = 255;
        reset();
    }

    void reset()
    {
        std::fill (fullRing.begin(), fullRing.end(), 0.0f);
        std::fill (decRing.begin(), decRing.end(), 0.0f);
        count = 0;
        nDec = 0;
        histCount = 0;
        prevTauD = 0.0f;
        prevVoiced = false;
    }

    void setThreshold (float t) { threshold = std::clamp (t, 0.02f, 0.6f); }
    void setGateDb (float db) { gateLin = std::pow (10.0f, db / 20.0f); }

    /** Feed one input-rate sample. Returns true when a new estimate was produced. */
    bool push (float x)
    {
        const int64_t n = count++;
        fullRing[(size_t) (n & fullMask)] = x;
        if (((n + 1) % dec) != 0)
            return false;

        float acc = 0.0f;
        for (int i = 0; i < firLen; ++i)
            acc += fir[(size_t) i] * fullRing[(size_t) ((n - i) & fullMask)];
        decRing[(size_t) (nDec & decMask)] = acc;
        ++nDec;

        if (nDec >= spanD && (nDec % hopD) == 0)
        {
            analyse();
            return true;
        }
        return false;
    }

    Result lookup (double t) const
    {
        Result r;
        if (histCount == 0)
            return r;
        const int64_t newest = histCount - 1;
        const int64_t oldest = std::max<int64_t> (0, histCount - (int64_t) hist.size());
        int64_t i = newest;
        while (i > oldest && hist[(size_t) (i & histMask)].centre > t)
            --i;
        const Entry& a = hist[(size_t) (i & histMask)];
        if (i == newest || t <= a.centre)
        {
            r.voiced = a.voiced;
            r.period = a.period;
            return r;
        }
        const Entry& b = hist[(size_t) ((i + 1) & histMask)];
        if (a.voiced && b.voiced)
        {
            const double span = std::max (1.0, b.centre - a.centre);
            const double f = std::clamp ((t - a.centre) / span, 0.0, 1.0);
            r.voiced = true;
            r.period = (float) (a.period + f * (b.period - a.period));
        }
        else if (a.voiced || b.voiced)
        {
            r.voiced = true;
            r.period = a.voiced ? a.period : b.period;
        }
        return r;
    }

    bool hasEstimates() const { return histCount > 0; }
    double newestCentre() const
    {
        return histCount > 0 ? hist[(size_t) ((histCount - 1) & histMask)].centre : -1.0e18;
    }
    int getDecimation() const { return dec; }

private:
    struct Entry
    {
        double centre = 0.0;
        float period = 0.0f;
        float conf = 0.0f;
        bool voiced = false;
    };

    void analyse()
    {
        const int64_t base = nDec - spanD;
        for (int i = 0; i < spanD; ++i)
            seg[(size_t) i] = decRing[(size_t) ((base + i) & decMask)];

        double energy = 0.0;
        for (int i = spanD - winD; i < spanD; ++i)
            energy += (double) seg[(size_t) i] * seg[(size_t) i];
        const float rms = (float) std::sqrt (energy / winD);

        // Difference function + cumulative mean normalised difference (YIN steps 2-3)
        d[0] = 0.0f;
        for (int tau = 1; tau <= tauMaxD; ++tau)
        {
            const float* a = seg.data();
            const float* b = seg.data() + tau;
            float s = 0.0f;
            for (int j = 0; j < winD; ++j)
            {
                const float diff = a[j] - b[j];
                s += diff * diff;
            }
            d[(size_t) tau] = s;
        }
        dn[0] = 1.0f;
        double run = 0.0;
        for (int tau = 1; tau <= tauMaxD; ++tau)
        {
            run += d[(size_t) tau];
            dn[(size_t) tau] = run > 1.0e-12 ? (float) (d[(size_t) tau] * tau / run) : 1.0f;
        }

        // Absolute threshold: first dip below the threshold, then walk down to its minimum
        int tau = -1;
        for (int t = tauMinD; t < tauMaxD; ++t)
        {
            if (dn[(size_t) t] < threshold)
            {
                while (t + 1 < tauMaxD && dn[(size_t) (t + 1)] < dn[(size_t) t])
                    ++t;
                tau = t;
                break;
            }
        }

        // Continuity: prefer a candidate close to the previous period (guards against octave jumps)
        if (prevVoiced && prevTauD > 0.0f)
        {
            const int lo = std::max (tauMinD, (int) std::floor (prevTauD * 0.9f));
            const int hi = std::min (tauMaxD - 1, (int) std::ceil (prevTauD * 1.1f));
            int tc = -1;
            float best = 10.0f;
            for (int t = lo; t <= hi; ++t)
                if (dn[(size_t) t] < best) { best = dn[(size_t) t]; tc = t; }
            if (tc > 0)
            {
                if (tau < 0 && best < threshold + 0.05f)
                    tau = tc;
                else if (tau >= 0 && std::fabs ((float) tau - prevTauD) > 0.2f * prevTauD && best < threshold + 0.08f)
                    tau = tc;
            }
        }

        Entry e;
        const bool loud = rms >= gateLin;
        if (tau >= 1 && tau < tauMaxD && loud)
        {
            const float a = dn[(size_t) (tau - 1)], b = dn[(size_t) tau], c = dn[(size_t) (tau + 1)];
            const float den = a - 2.0f * b + c;
            float shift = std::fabs (den) > 1.0e-9f ? 0.5f * (a - c) / den : 0.0f;
            shift = std::clamp (shift, -1.0f, 1.0f);
            const float tauF = (float) tau + shift;
            e.voiced = true;
            e.period = tauF * (float) dec;
            e.conf = std::clamp (1.0f - b, 0.0f, 1.0f);
            const double centreD = (double) base + 0.5 * ((double) winD + tauF);
            e.centre = (centreD + 1.0) * dec - 1.0 - groupDelay;
            prevTauD = tauF;
            prevVoiced = true;
        }
        else
        {
            e.voiced = false;
            e.period = 0.0f;
            const double centreD = (double) base + 0.5 * ((double) winD + (double) tauMaxD * 0.5);
            e.centre = (centreD + 1.0) * dec - 1.0 - groupDelay;
            prevVoiced = false;
        }
        hist[(size_t) (histCount & histMask)] = e;
        ++histCount;
    }

    double sr = 44100.0;
    int dec = 4, tauMinD = 8, tauMaxD = 184, winD = 184, spanD = 372, firLen = 33;
    static constexpr int hopD = 32;
    double srD = 11025.0, groupDelay = 16.0;
    float threshold = 0.2f, gateLin = 0.001f;
    std::vector<float> fir, fullRing, decRing, seg, d, dn;
    int64_t fullMask = 511, decMask = 0, histMask = 255;
    int64_t count = 0, nDec = 0, histCount = 0;
    std::vector<Entry> hist;
    float prevTauD = 0.0f;
    bool prevVoiced = false;
};
} // namespace autoladd
