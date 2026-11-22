// Drives the real AutoladdProcessor offline (no host needed), then renders the editor to PNG files.
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <chrono>
#include <cmath>
#include <functional>

using namespace juce;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)

// ---- synthetic voice: glottal pulse train through three formant resonators ---------------------
static std::vector<float> makeVoice (double sr, int n, const std::function<double (double)>& f0At, double level = 0.3)
{
    std::vector<float> out ((size_t) n);
    double phase = 0.0, prevSrc = 0.0;
    struct Res { double a1, a2, g, z1 = 0, z2 = 0; };
    auto mk = [sr] (double fc, double bw) {
        const double r = std::exp (-autoladd::kPi * bw / sr);
        return Res { 2.0 * r * std::cos (2.0 * autoladd::kPi * fc / sr), -r * r, 1.0 - r };
    };
    Res f[3] = { mk (700, 110), mk (1200, 140), mk (2600, 200) };
    const double gains[3] = { 1.0, 0.6, 0.3 };
    for (int i = 0; i < n; ++i)
    {
        phase += f0At (i / sr) / sr;
        if (phase >= 1.0) phase -= 1.0;
        const double g = phase < 0.6 ? 0.5 * (1.0 - std::cos (autoladd::kPi * phase / 0.6))
                                     : (phase < 0.8 ? std::cos (autoladd::kPi * 0.5 * (phase - 0.6) / 0.2) : 0.0);
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
        out[(size_t) i] = (float) y;
    }
    double e = 0;
    for (float v : out) e += (double) v * v;
    const double rms = std::sqrt (e / n);
    for (auto& v : out) v = (float) (v * level / std::max (rms, 1e-9));
    return out;
}

static double estimateNote (const float* x, int n, double sr)
{
    double mean = 0;
    for (int i = 0; i < n; ++i) mean += x[i];
    mean /= n;
    const int lo = (int) (sr / 500.0), hi = (int) (sr / 80.0);
    std::vector<double> ac ((size_t) hi + 2, 0.0);
    for (int k = lo - 1; k <= hi + 1; ++k)
    {
        double s = 0;
        for (int i = 0; i + k < n; ++i) s += (x[i] - mean) * (x[i + k] - mean);
        ac[(size_t) k] = s;
    }
    int best = lo;
    for (int k = lo; k <= hi; ++k)
        if (ac[(size_t) k] > ac[(size_t) best]) best = k;
    const double y0 = ac[(size_t) best - 1], y1 = ac[(size_t) best], y2 = ac[(size_t) best + 1];
    const double d = y0 - 2 * y1 + y2;
    const double shift = d != 0 ? 0.5 * (y0 - y2) / d : 0.0;
    return 69.0 + 12.0 * std::log2 (sr / (best + shift) / 440.0);
}

static double medianNote (const std::vector<float>& x, int from, int to, double sr)
{
    std::vector<double> v;
    for (int i = from; i + 4096 <= to; i += 4096)
    {
        double e = 0;
        for (int k = 0; k < 4096; ++k) e += (double) x[(size_t) (i + k)] * x[(size_t) (i + k)];
        if (std::sqrt (e / 4096) < 0.01) continue;
        v.push_back (estimateNote (&x[(size_t) i], 4096, sr));
    }
    if (v.empty()) return -1000.0;
    std::sort (v.begin(), v.end());
    return v[v.size() / 2];
}

static void setParam (AutoladdProcessor& p, const String& id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

static float getParam (AutoladdProcessor& p, const String& id)
{
    auto* prm = p.apvts.getParameter (id);
    return prm->convertFrom0to1 (prm->getValue());
}

struct Run { std::vector<float> out, in; int latency = 0; bool finite = true; };

static Run process (AutoladdProcessor& p, double sr, const std::vector<float>& voice, int channels = 2,
                    const MidiBuffer* midiFirst = nullptr)
{
    AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (channels == 1 ? AudioChannelSet::mono() : AudioChannelSet::stereo());
    layout.outputBuses.add (channels == 1 ? AudioChannelSet::mono() : AudioChannelSet::stereo());
    p.setBusesLayout (layout);
    p.setRateAndBufferSizeDetails (sr, 512);
    p.prepareToPlay (sr, 512);
    Run r;
    r.latency = p.getLatencySamples();
    r.in = voice;
    r.out.resize (voice.size());
    AudioBuffer<float> buf (channels, 512);
    for (size_t pos = 0; pos < voice.size(); pos += 512)
    {
        const int n = (int) std::min<size_t> (512, voice.size() - pos);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < n; ++i)
                buf.setSample (c, i, voice[pos + (size_t) i]);
        MidiBuffer m;
        if (pos == 0 && midiFirst != nullptr) m = *midiFirst;
        p.processBlock (buf, m);
        for (int i = 0; i < n; ++i)
        {
            r.out[pos + (size_t) i] = buf.getSample (0, i);
            r.finite = r.finite && std::isfinite (buf.getSample (0, i)) && std::abs (buf.getSample (0, i)) < 8.0f;
        }
    }
    return r;
}

static void savePng (Component& c, const File& f, float scale = 1.0f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    FileOutputStream os (f);
    os.setPosition (0);
    os.truncate();
    PNGImageFormat png;
    png.writeImageToStream (img, os);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juceInit;
    const File outDir = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : ".");
    outDir.createDirectory();

    const double SR = 44100.0;
    const int N = (int) (SR * 3.5);

    {
        std::printf ("[1] parameters / layout\n");
        AutoladdProcessor p;
        CHECK (p.getParameters().size() >= 27, "all parameters are exposed");
        CHECK (p.apvts.getParameter (ids::autotune) != nullptr, "autotune knob parameter exists");
        CHECK (std::abs (getParam (p, ids::autotune) - 100.0f) < 0.01f, "autotune defaults to 100%");
        CHECK (std::abs (getParam (p, ids::speed)) < 0.01f, "retune speed defaults to instant");
        p.prepareToPlay (44100.0, 512);
        CHECK (p.getLatencySamples() > 500 && p.getLatencySamples() < 4000, "latency is reported to the host");
        std::printf ("  latency = %d samples (%.1f ms)\n", p.getLatencySamples(), 1000.0 * p.getLatencySamples() / 44100.0);
    }

    {
        std::printf ("[2] hard autotune locks a wobbly voice onto G#3\n");
        AutoladdProcessor p;
        setParam (p, ids::robot, 0.0f);
        auto in = makeVoice (SR, N, [] (double t) { return 207.0 * std::pow (2.0, 0.35 / 12.0 * std::sin (2 * autoladd::kPi * 5.5 * t)); });
        auto r = process (p, SR, in);
        const double nin = medianNote (in, 40000, N, SR), nout = medianNote (r.out, 40000 + r.latency, N, SR);
        std::printf ("  in %.3f  out %.3f  (target 56.000)\n", nin, nout);
        CHECK (r.finite, "output finite");
        CHECK (std::abs (nout - 56.0) < 0.06, "output sits on the note");
    }

    {
        std::printf ("[3] key + scale: C major pulls 213 Hz (G#3/A3) up to A3\n");
        AutoladdProcessor p;
        setParam (p, ids::robot, 0.0f);
        setParam (p, ids::scale, 1.0f); // Major
        setParam (p, ids::key, 0.0f);   // C
        auto in = makeVoice (SR, N, [] (double) { return 213.0; });
        auto r = process (p, SR, in);
        const double nout = medianNote (r.out, 40000 + r.latency, N, SR);
        std::printf ("  out %.3f (target 57.000)\n", nout);
        CHECK (std::abs (nout - 57.0) < 0.06, "lands on A3 (in key)");
        CHECK (p.getEffectiveMask() == autoladd::absoluteMaskFor (1, 0), "effective mask = C major");
    }

    {
        std::printf ("[4] key customization: only E enabled (monotone robot)\n");
        AutoladdProcessor p;
        setParam (p, ids::robot, 0.0f);
        p.setCustomMask ((uint16_t) (1u << 4));
        CHECK (p.getEffectiveMask() == (1u << 4), "custom mask applied");
        CHECK ((int) getParam (p, ids::scale) == autoladd::kCustomScaleIndex, "scale switched to Custom");
        auto in = makeVoice (SR, N, [] (double t) { return 213.0 * std::pow (2.0, 0.6 / 12.0 * std::sin (2 * autoladd::kPi * 0.8 * t)); });
        auto r = process (p, SR, in);
        const double nout = medianNote (r.out, 40000 + r.latency, N, SR);
        std::printf ("  out %.3f (target 52.000 = E3)\n", nout);
        CHECK (std::abs (nout - 52.0) < 0.08, "voice is held on E");
    }

    {
        std::printf ("[5] autotune level knob: 0%% leaves pitch alone, 50%% is halfway\n");
        auto in = makeVoice (SR, N, [] (double) { return 213.0; }); // 56.44
        const double nin = medianNote (in, 40000, N, SR);
        double res[3];
        int i = 0;
        for (float lvl : { 0.0f, 50.0f, 100.0f })
        {
            AutoladdProcessor p;
            setParam (p, ids::robot, 0.0f);
            setParam (p, ids::autotune, lvl);
            auto r = process (p, SR, in);
            res[i++] = medianNote (r.out, 40000 + r.latency, N, SR);
        }
        std::printf ("  in %.3f | 0%%: %.3f | 50%%: %.3f | 100%%: %.3f\n", nin, res[0], res[1], res[2]);
        CHECK (std::abs (res[0] - nin) < 0.06, "0% = untouched pitch");
        CHECK (std::abs (res[1] - 0.5 * (nin + 56.0)) < 0.08, "50% = halfway to the note");
        CHECK (std::abs (res[2] - 56.0) < 0.06, "100% = on the note");
    }

    {
        std::printf ("[6] mix 0%% and bypass give the (delayed) dry signal\n");
        auto in = makeVoice (SR, N, [] (double) { return 190.0; });
        for (int mode = 0; mode < 2; ++mode)
        {
            AutoladdProcessor p;
            if (mode == 0) setParam (p, ids::mix, 0.0f); else setParam (p, ids::bypass, 1.0f);
            auto r = process (p, SR, in);
            double err = 0, ref = 0;
            for (int i = 20000; i < N - 1000; ++i)
            {
                const double d = (double) r.out[(size_t) i] - in[(size_t) (i - r.latency)];
                err += d * d; ref += (double) in[(size_t) (i - r.latency)] * in[(size_t) (i - r.latency)];
            }
            CHECK (err / ref < 1e-5, mode == 0 ? "mix 0% == dry" : "bypass == dry");
        }
    }

    {
        std::printf ("[7] MIDI note target: holding E tunes the voice to E\n");
        AutoladdProcessor p;
        setParam (p, ids::robot, 0.0f);
        setParam (p, ids::midiTarget, 1.0f);
        MidiBuffer m;
        m.addEvent (MidiMessage::noteOn (1, 52, (uint8) 100), 0); // MIDI 52 = E3
        auto in = makeVoice (SR, N, [] (double) { return 213.0; });
        auto r = process (p, SR, in, 2, &m);
        const double nout = medianNote (r.out, 40000 + r.latency, N, SR);
        std::printf ("  out %.3f (target 52.000)\n", nout);
        CHECK (std::abs (nout - 52.0) < 0.08, "voice follows the held MIDI note");
    }

    {
        std::printf ("[8] sample rates + mono\n");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            AutoladdProcessor p;
            setParam (p, ids::robot, 0.0f);
            const int n = (int) (sr * 3.0);
            auto in = makeVoice (sr, n, [] (double t) { return 146.83 * std::pow (2.0, 0.4 / 12.0 * std::sin (2 * autoladd::kPi * 5.0 * t)); }); // D3 = 50
            auto r = process (p, sr, in, sr == 48000.0 ? 1 : 2);
            const double nout = medianNote (r.out, (int) (sr * 0.9) + r.latency, n, sr);
            std::printf ("  %.0f Hz (%s): out %.3f, latency %.1f ms\n", sr, sr == 48000.0 ? "mono" : "stereo", nout, 1000.0 * r.latency / sr);
            CHECK (r.finite && std::abs (nout - std::round (nout)) < 0.07, "finite and on a note");
        }
    }

    {
        std::printf ("[9] presets + CPU load\n");
        AutoladdProcessor p;
        auto in = makeVoice (SR, (int) (SR * 2.0), [] (double t) { return 180.0 * std::pow (2.0, std::sin (2 * autoladd::kPi * 0.6 * t) / 5.0); });
        bool ok = true;
        for (int i = 0; i < AutoladdProcessor::getPresetNames().size(); ++i)
        {
            p.applyPreset (i);
            ok = ok && process (p, SR, in).finite;
        }
        CHECK (ok, "every preset renders finite audio");
        p.applyPreset (4); // Extreme Robot
        CHECK (getParam (p, ids::robot) > 99.0f && getParam (p, ids::autotune) > 99.0f, "Extreme Robot = autotune 100% + robot 100%");
        p.setCustomMask (autoladd::absoluteMaskFor (1, 0));
        auto big = makeVoice (48000.0, 48000 * 10, [] (double t) { return 160.0 + 60.0 * std::sin (t); });
        AutoladdProcessor q;
        const auto t0 = std::chrono::steady_clock::now();
        process (q, 48000.0, big);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  10 s of stereo audio @48k in %.3f s  (%.0fx real time)\n", sec, 10.0 / sec);
        CHECK (sec < 3.0, "comfortably real-time");
    }

    {
        std::printf ("[10] state save / restore (incl. look & feel)\n");
        AutoladdProcessor a;
        setParam (a, ids::autotune, 73.0f);
        setParam (a, ids::key, 9.0f);
        a.setCustomMask (0b101010110101);
        UiSettings u; u.theme = 3; u.glass = 0.25f; u.scale = 1.25f; u.showNames = false;
        a.setUiSettings (u);
        MemoryBlock mb;
        a.getStateInformation (mb);
        AutoladdProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (getParam (b, ids::autotune) - 73.0f) < 0.2f, "autotune level restored");
        CHECK ((int) getParam (b, ids::key) == 9, "key restored");
        CHECK (b.getEffectiveMask() == 0b101010110101, "custom notes restored");
        const auto u2 = b.getUiSettings();
        CHECK (u2.theme == 3 && std::abs (u2.glass - 0.25f) < 1e-3f && std::abs (u2.scale - 1.25f) < 1e-3f && ! u2.showNames, "theme/glass/size restored");
    }

    {
        std::printf ("[11] interface renders\n");
        AutoladdProcessor p;
        p.applyPreset (5);
        p.setCustomMask (autoladd::absoluteMaskFor (6, 9)); // A minor pentatonic
        auto in = makeVoice (SR, (int) (SR * 1.5), [] (double) { return 213.0; });
        process (p, SR, in);

        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        auto* e = static_cast<AutoladdEditor*> (ed.get());
        MessageManager::getInstance()->runDispatchLoopUntil (150);
        savePng (*e, outDir.getChildFile ("ui_default.png"), 1.5f);
        std::printf ("  editor size %d x %d\n", e->getWidth(), e->getHeight());
        CHECK (e->getWidth() == AutoladdEditor::baseW, "editor opens at the base size");

        e->showCustomize (true);
        savePng (*e, outDir.getChildFile ("ui_customize.png"), 1.5f);
        e->showCustomize (false);
        ed.reset();

        UiSettings u; u.theme = 1; u.glass = 0.9f; u.scale = 1.0f;
        p.setUiSettings (u);
        p.setNoteToggles (0x0FFF);
        p.applyPreset (2);
        process (p, SR, in);
        ed.reset (p.createEditor());
        MessageManager::getInstance()->runDispatchLoopUntil (150);
        savePng (*ed, outDir.getChildFile ("ui_graphite.png"), 1.0f);

        u.theme = 4; u.scale = 0.75f;
        p.setUiSettings (u);
        ed.reset (p.createEditor());
        std::printf ("  restored editor size %d x %d\n", ed->getWidth(), ed->getHeight());
        CHECK (ed->getWidth() == roundToInt (AutoladdEditor::baseW * 0.75f), "saved window size is restored");
        ed.reset();
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
