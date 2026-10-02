#include "Analyzer.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace bc
{
namespace
{
    std::vector<float> toMono (const juce::AudioBuffer<float>& a)
    {
        const int n = a.getNumSamples();
        const int nc = juce::jmax (1, a.getNumChannels());
        std::vector<float> m ((size_t) n, 0.0f);
        for (int c = 0; c < a.getNumChannels(); ++c)
        {
            auto* d = a.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                m[(size_t) i] += d[i] / (float) nc;
        }
        return m;
    }

    float percentile (std::vector<float> v, float p)
    {
        if (v.empty()) return 0.0f;
        auto k = (size_t) juce::jlimit (0, (int) v.size() - 1, (int) std::round (p * (float) (v.size() - 1)));
        std::nth_element (v.begin(), v.begin() + (long) k, v.end());
        return v[k];
    }

    float gainToDb (float g) { return g > 1.0e-6f ? 20.0f * std::log10 (g) : -120.0f; }

    struct OdfData
    {
        std::vector<float> odf;
        int hop = 256;
        int fftSize = 1024;
        double frameRate = 172.0;
    };

    // Multi-band super-flux onset detection function. Frames are centred on f * hop.
    OdfData computeOdf (const std::vector<float>& x, double sr)
    {
        OdfData r;
        const int order = sr > 64000.0 ? 11 : 10;
        const int N = 1 << order;
        r.fftSize = N;
        r.hop = N / 4;
        r.frameRate = sr / r.hop;

        const int numFrames = (int) (x.size() / (size_t) r.hop) + 1;
        const int numBins = N / 2 + 1;
        juce::dsp::FFT fft (order);

        std::vector<float> win ((size_t) N), buf ((size_t) N * 2), prev ((size_t) numBins, 0.0f), cur ((size_t) numBins, 0.0f);
        for (int i = 0; i < N; ++i)
            win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (N - 1));

        const double binHz = sr / N;
        const double edgesHz[] = { 30.0, 150.0, 500.0, 4000.0, juce::jmin (16000.0, sr * 0.5 - binHz) };
        int edges[5];
        for (int b = 0; b < 5; ++b)
            edges[b] = juce::jlimit (1, numBins - 2, (int) std::round (edgesHz[b] / binHz));

        std::array<std::vector<float>, 4> flux;
        for (auto& f : flux) f.assign ((size_t) numFrames, 0.0f);

        const float scale = 1000.0f / (float) N;
        const int64_t len = (int64_t) x.size();

        for (int f = 0; f < numFrames; ++f)
        {
            const int64_t start = (int64_t) f * r.hop - N / 2;
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < N; ++i)
            {
                const int64_t idx = start + i;
                if (idx >= 0 && idx < len)
                    buf[(size_t) i] = x[(size_t) idx] * win[(size_t) i];
            }
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);

            for (int k = 0; k < numBins; ++k)
                cur[(size_t) k] = std::log1p (buf[(size_t) k] * scale);

            for (int b = 0; b < 4; ++b)
            {
                float sum = 0.0f;
                for (int k = edges[b]; k < edges[b + 1]; ++k)
                {
                    const float ref = juce::jmax (prev[(size_t) k - 1], prev[(size_t) k], prev[(size_t) k + 1]);
                    const float d = cur[(size_t) k] - ref;
                    if (d > 0.0f) sum += d;
                }
                flux[(size_t) b][(size_t) f] = sum / (float) juce::jmax (1, edges[b + 1] - edges[b]);
            }
            std::swap (prev, cur);
        }

        const float weights[] = { 1.0f, 1.0f, 1.0f, 0.8f };
        r.odf.assign ((size_t) numFrames, 0.0f);
        for (int b = 0; b < 4; ++b)
        {
            const float p = percentile (flux[(size_t) b], 0.98f);
            if (p < 1.0e-9f) continue;
            for (int f = 0; f < numFrames; ++f)
                r.odf[(size_t) f] += weights[b] * juce::jmin (3.0f, flux[(size_t) b][(size_t) f] / p);
        }
        const float mx = *std::max_element (r.odf.begin(), r.odf.end());
        if (mx > 0.0f)
            for (auto& v : r.odf) v /= mx;
        return r;
    }

    // Sample accurate onset position : start of the energy rise around the detected frame
    int64_t refineOnset (const std::vector<float>& x, double sr, int64_t est, int fftSize)
    {
        const int64_t len = (int64_t) x.size();
        const int64_t a = juce::jmax<int64_t> (0, est - fftSize / 2);
        const int64_t b = juce::jmin<int64_t> (len - 1, est + fftSize / 2);
        if (b <= a + 4) return juce::jlimit<int64_t> (0, len - 1, est);

        const int w = juce::jmax (8, (int) (sr * 0.001));
        std::vector<float> e ((size_t) (b - a + 1), 0.0f);
        // centred running energy
        double run = 0.0;
        auto sq = [&] (int64_t i) -> double { return (i >= 0 && i < len) ? (double) x[(size_t) i] * x[(size_t) i] : 0.0; };
        for (int64_t i = a - w / 2; i < a + w / 2; ++i) run += sq (i);
        for (int64_t i = a; i <= b; ++i)
        {
            e[(size_t) (i - a)] = (float) run;
            run += sq (i + w / 2) - sq (i - w / 2);
        }

        size_t ip = 0;
        for (size_t i = 1; i < e.size(); ++i)
            if (e[i] > e[ip]) ip = i;

        float base = e[ip];
        for (size_t i = 0; i <= ip; ++i) base = juce::jmin (base, e[i]);
        const float thr = base + 0.1f * (e[ip] - base);

        size_t i0 = ip;
        while (i0 > 0 && e[i0] > thr) --i0;
        return a + (int64_t) i0;
    }

    float peakDbAfter (const std::vector<float>& x, int64_t pos, int64_t span)
    {
        float pk = 0.0f;
        const int64_t end = juce::jmin<int64_t> ((int64_t) x.size(), pos + span);
        for (int64_t i = juce::jmax<int64_t> (0, pos); i < end; ++i)
            pk = juce::jmax (pk, std::abs (x[(size_t) i]));
        return gainToDb (pk);
    }

    int64_t snapToZeroCrossing (const std::vector<float>& x, int64_t p, int maxBack)
    {
        for (int i = 0; i < maxBack && p - i - 1 >= 0; ++i)
        {
            const float s0 = x[(size_t) (p - i - 1)], s1 = x[(size_t) (p - i)];
            if ((s0 <= 0.0f && s1 >= 0.0f) || (s0 >= 0.0f && s1 <= 0.0f))
                return p - i;
        }
        return p;
    }

    double tempoPrior (double bpm)
    {
        const double l = std::log2 (bpm / 120.0) / 0.5;
        return std::exp (-0.5 * l * l);
    }
}

//==============================================================================
AnalysisResult Analyzer::analyze (const juce::AudioBuffer<float>& audio, double sr)
{
    AnalysisResult res;
    const auto x = toMono (audio);
    const int64_t len = (int64_t) x.size();
    if (len < 512) return res;

    const auto od = computeOdf (x, sr);
    const auto& odf = od.odf;
    const int nf = (int) odf.size();

    // ---- candidate peaks (low threshold, final selection happens in makeSlices)
    struct Cand { int frame; float s; };
    std::vector<Cand> cands;
    for (int t = 0; t < nf; ++t)
    {
        const float v = odf[(size_t) t];
        if (v < 0.03f) continue;
        bool isMax = true;
        for (int k = juce::jmax (0, t - 3); k <= juce::jmin (nf - 1, t + 3) && isMax; ++k)
            if (odf[(size_t) k] > v || (k < t && odf[(size_t) k] >= v)) isMax = false;
        if (! isMax) continue;

        std::vector<float> loc;
        for (int k = juce::jmax (0, t - 10); k <= juce::jmin (nf - 1, t + 5); ++k)
            loc.push_back (odf[(size_t) k]);
        if (v - percentile (loc, 0.5f) < 0.02f) continue;
        cands.push_back ({ t, v });
    }

    // ---- refine to sample accuracy, dedupe
    std::vector<Onset> ons;
    for (auto& c : cands)
    {
        Onset o;
        o.pos = refineOnset (x, sr, (int64_t) c.frame * od.hop, od.fftSize);
        o.strength = c.s;
        o.levelDb = peakDbAfter (x, o.pos, (int64_t) (0.03 * sr));
        ons.push_back (o);
    }
    std::sort (ons.begin(), ons.end(), [] (auto& a, auto& b) { return a.pos < b.pos; });
    const int64_t dedupe = (int64_t) (0.012 * sr);
    for (auto& o : ons)
    {
        if (! res.onsets.empty() && o.pos - res.onsets.back().pos < dedupe)
        {
            if (o.strength > res.onsets.back().strength)
                res.onsets.back() = o;
            continue;
        }
        res.onsets.push_back (o);
    }

    // ---- grid origin : the first clear hit, if the file has a little lead-in
    res.gridOrigin = 0;
    for (auto& o : res.onsets)
    {
        if (o.strength >= 0.25f)
        {
            if ((double) o.pos < 0.2 * sr) res.gridOrigin = o.pos;
            break;
        }
    }
    if (res.gridOrigin < (int64_t) (0.004 * sr)) res.gridOrigin = 0;

    // ---- tempo from onset autocorrelation (on a smoothed ODF so the ACF is not too peaky)
    std::vector<double> o ((size_t) nf, 0.0);
    {
        const int rad = 4;
        double wsum = 0.0;
        std::vector<double> gk ((size_t) (2 * rad + 1));
        for (int k = -rad; k <= rad; ++k) { gk[(size_t) (k + rad)] = std::exp (-0.5 * (k * k) / 4.0); wsum += gk[(size_t) (k + rad)]; }
        for (int t = 0; t < nf; ++t)
        {
            double v = 0.0;
            for (int k = -rad; k <= rad; ++k)
            {
                const int idx = t + k;
                if (idx >= 0 && idx < nf) v += odf[(size_t) idx] * gk[(size_t) (k + rad)];
            }
            o[(size_t) t] = v / wsum;
        }
    }
    const double mean = std::accumulate (o.begin(), o.end(), 0.0) / juce::jmax (1, nf);
    for (auto& v : o) v -= mean;

    const int maxLag = juce::jmin (nf - 2, (int) (4.0 * 60.0 * od.frameRate / 60.0) + 2);
    std::vector<double> acf ((size_t) juce::jmax (2, maxLag + 2), 0.0);
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        double s = 0.0;
        for (int t = 0; t + lag < nf; ++t) s += o[(size_t) t] * o[(size_t) (t + lag)];
        acf[(size_t) lag] = s / (double) juce::jmax (1, nf - lag);
    }
    auto acfAt = [&] (double lag) -> double
    {
        if (lag < 0 || lag >= (double) maxLag) return 0.0;
        const int l0 = (int) lag; const double fr = lag - l0;
        return acf[(size_t) l0] * (1.0 - fr) + acf[(size_t) l0 + 1] * fr;
    };
    const double a0 = acf[0] > 0 ? acf[0] : 1.0;
    auto tempoScore = [&] (double bpm)
    {
        const double lag = 60.0 * od.frameRate / bpm;
        return (acfAt (lag) + 0.5 * acfAt (2.0 * lag) + 0.35 * acfAt (4.0 * lag) + 0.25 * acfAt (0.5 * lag)) / a0;
    };

    double bestBpm = 120.0, bestScore = -1.0e9;
    for (double bpm = 60.0; bpm <= 200.0; bpm += 0.25)
    {
        const double score = tempoScore (bpm) * (0.85 + 0.15 * tempoPrior (bpm));
        if (score > bestScore) { bestScore = score; bestBpm = bpm; }
    }

    // ---- candidate tempi : loop-length hypotheses (trimmed breaks are a whole number of bars)
    //      plus the autocorrelation winner and its octaves. Each candidate is scored by how well
    //      the onsets sit on its 16th-note grid ; a tempo whose odd 16ths are never used is
    //      really double-time and is rejected (breaks almost always use the 16th grid).
    const double dur = (double) (len - res.gridOrigin) / sr;
    struct TempoCand { double bpm; int bars; bool fromLength; };
    std::vector<TempoCand> tc;
    for (int bars : { 1, 2, 4, 8, 16 })
    {
        const double b = bars * 240.0 / dur;
        if (b >= 60.0 && b <= 200.0) tc.push_back ({ b, bars, true });
    }
    for (double m : { 1.0, 2.0, 0.5 })
    {
        const double b = bestBpm * m;
        if (b >= 60.0 && b <= 200.0) tc.push_back ({ b, 0, false });
    }

    auto gridFit = [&] (double bpm, double& oddShare)
    {
        const double sps = sr * 60.0 / bpm / 4.0;
        double sumW = 0.0, fit = 0.0, odd = 0.0;
        for (auto& on : res.onsets)
        {
            const double w = on.strength;
            const double g = (double) (on.pos - res.gridOrigin) / sps;
            const double cell = std::round (g);
            fit += w * std::cos (juce::MathConstants<double>::twoPi * (g - cell));
            if (std::abs (g - cell) < 0.2 && ((long long) cell % 2) != 0) odd += w;
            sumW += w;
        }
        oddShare = sumW > 0.0 ? odd / sumW : 0.0;
        return sumW > 0.0 ? fit / sumW : 0.0;
    };

    double bestCandScore = -1.0e9;
    const TempoCand* chosen = nullptr;
    for (auto& c : tc)
    {
        double odd = 0.0;
        const double fit = gridFit (c.bpm, odd);
        double sc = fit + 0.3 * tempoPrior (c.bpm) + (c.fromLength ? 0.08 : 0.0);
        if (odd < 0.04 && c.bpm / 2.0 >= 60.0) sc -= 1.0;   // double-time hypothesis
        if (sc > bestCandScore) { bestCandScore = sc; chosen = &c; }
    }

    if (chosen != nullptr)
    {
        double odd = 0.0;
        res.bpm = chosen->bpm;
        res.bars = chosen->fromLength ? chosen->bars : barsForTempo (len, sr, chosen->bpm, res.gridOrigin);
        res.confidence = juce::jlimit (0.2, 1.0, gridFit (chosen->bpm, odd));
    }
    else
    {
        double b = bestBpm;
        while (b < 80.0) b *= 2.0;
        while (b > 180.0) b *= 0.5;
        res.bpm = b;
        res.bars = barsForTempo (len, sr, b, res.gridOrigin);
        res.confidence = 0.2;
    }
    return res;
}

int Analyzer::barsForTempo (int64_t numSamples, double sr, double bpm, int64_t origin)
{
    const double dur = (double) (numSamples - origin) / sr;
    return juce::jlimit (1, 64, (int) std::round (dur * bpm / 240.0));
}

//==============================================================================
std::vector<Slice> Analyzer::makeSlices (const AnalysisResult& an, const juce::AudioBuffer<float>& audio,
                                         double sr, double bpm, int bars, int64_t origin,
                                         const SlicerSettings& st)
{
    const auto x = toMono (audio);
    const int64_t len = (int64_t) x.size();
    std::vector<Slice> out;
    if (len == 0) return out;

    const double sps = sr * 60.0 / bpm / 4.0;      // samples per 16th
    const int64_t preroll = (int64_t) (st.prerollMs * 0.001 * sr);
    const int zcMax = (int) (0.001 * sr);
    const float sens = juce::jlimit (0.0f, 1.0f, st.sensitivity);

    auto place = [&] (int64_t p) { return snapToZeroCrossing (x, juce::jmax<int64_t> (0, p - preroll), zcMax); };

    struct Pick { int64_t pos; float strength; float levelDb; };
    std::vector<Pick> picks;

    float maxDb = -120.0f;
    for (auto& o : an.onsets) maxDb = juce::jmax (maxDb, o.levelDb);

    if (st.mode == SlicerSettings::Grid)
    {
        const int div = juce::jlimit (1, 64, st.gridDivision);
        const double barLen = sps * 16.0;
        const int64_t snapWin = (int64_t) (0.015 * sr);
        for (int i = 0; i < bars * div; ++i)
        {
            const int64_t p = origin + (int64_t) std::round (i * barLen / div);
            if (p >= len) break;
            const Onset* best = nullptr;
            for (auto& o : an.onsets)
                if (std::abs (o.pos - p) <= snapWin && (best == nullptr || std::abs (o.pos - p) < std::abs (best->pos - p)))
                    best = &o;
            if (best != nullptr) picks.push_back ({ place (best->pos), best->strength, best->levelDb });
            else                 picks.push_back ({ snapToZeroCrossing (x, p, zcMax), 0.0f, peakDbAfter (x, p, (int64_t) (0.03 * sr)) });
        }
    }
    else if (st.mode == SlicerSettings::Transient)
    {
        const float thr = juce::jmap (sens, 0.5f, 0.03f);
        const int64_t minGap = (int64_t) (juce::jmap (sens, 0.06f, 0.02f) * sr);
        for (auto& o : an.onsets)
        {
            if (o.strength < thr) continue;
            if (! picks.empty() && o.pos - picks.back().pos < minGap)
            {
                if (o.strength > picks.back().strength) picks.back() = { o.pos, o.strength, o.levelDb };
                continue;
            }
            picks.push_back ({ o.pos, o.strength, o.levelDb });
        }
        for (auto& p : picks) p.pos = place (p.pos);
    }
    else // Smart : one meaningful hit per 16th cell, grid-aware, flams merged, ghosts kept if strong enough
    {
        const float thr = juce::jmap (sens, 0.45f, 0.05f);
        struct Scored { const Onset* o; float score; double g; int cell; };
        std::vector<Scored> sc;
        for (auto& o : an.onsets)
        {
            const double g = (double) (o.pos - origin) / sps;
            const int cell = (int) std::round (g);
            const double dev = std::abs (g - cell);
            const float lv = juce::jlimit (0.0f, 1.0f, (o.levelDb - maxDb + 36.0f) / 36.0f);
            const float s = (0.65f * o.strength + 0.35f * lv) * (1.0f - 0.45f * (float) juce::jmin (1.0, dev / 0.5));
            sc.push_back ({ &o, s, g, cell });
        }

        std::map<int, size_t> bestInCell;
        for (size_t i = 0; i < sc.size(); ++i)
        {
            auto it = bestInCell.find (sc[i].cell);
            if (it == bestInCell.end() || sc[i].score > sc[it->second].score)
                bestInCell[sc[i].cell] = i;
        }

        for (size_t i = 0; i < sc.size(); ++i)
        {
            const auto& s = sc[i];
            const size_t b = bestInCell[s.cell];
            bool keep = false;
            if (b == i) keep = s.score >= thr;
            else if (sens >= 0.55f)
                keep = s.score >= thr * 1.5f + 0.1f && std::abs (s.g - sc[b].g) >= 0.4;
            if (keep) picks.push_back ({ s.o->pos, s.score, s.o->levelDb });
        }

        std::sort (picks.begin(), picks.end(), [] (auto& a, auto& b) { return a.pos < b.pos; });
        const int64_t minGap = (int64_t) juce::jmin (0.035 * sr, 0.4 * sps);
        std::vector<Pick> merged;
        for (auto& p : picks)
        {
            if (! merged.empty() && p.pos - merged.back().pos < minGap)
            {
                if (p.strength > merged.back().strength) merged.back() = p;
                continue;
            }
            merged.push_back (p);
        }
        picks = merged;
        for (auto& p : picks) p.pos = place (p.pos);
    }

    std::sort (picks.begin(), picks.end(), [] (auto& a, auto& b) { return a.pos < b.pos; });

    // dedupe
    std::vector<Pick> clean;
    for (auto& p : picks)
    {
        if (p.pos >= len) continue;
        if (! clean.empty() && p.pos - clean.back().pos < (int64_t) (0.005 * sr)) continue;
        clean.push_back (p);
    }

    // keep the lead-in only if it contains audible sound
    if (clean.empty())
        clean.push_back ({ 0, 1.0f, peakDbAfter (x, 0, (int64_t) (0.03 * sr)) });
    else if (clean.front().pos > 0)
    {
        float pk = 0.0f, gpk = 0.0f;
        for (int64_t i = 0; i < len; ++i)
        {
            gpk = juce::jmax (gpk, std::abs (x[(size_t) i]));
            if (i < clean.front().pos) pk = juce::jmax (pk, std::abs (x[(size_t) i]));
        }
        if (pk > 0.1f * gpk && clean.front().pos > (int64_t) (0.01 * sr))
            clean.insert (clean.begin(), { 0, 0.2f, gainToDb (pk) });
    }

    if ((int) clean.size() > kMaxSlices)
    {
        auto byStrength = clean;
        std::sort (byStrength.begin(), byStrength.end(), [] (auto& a, auto& b) { return a.strength > b.strength; });
        byStrength.resize ((size_t) kMaxSlices);
        std::sort (byStrength.begin(), byStrength.end(), [] (auto& a, auto& b) { return a.pos < b.pos; });
        clean = byStrength;
    }

    for (auto& p : clean)
    {
        Slice s;
        s.start = p.pos;
        s.strength = juce::jlimit (0.0f, 1.0f, p.strength);
        s.levelDb = p.levelDb;
        out.push_back (s);
    }
    finaliseSlices (out, len, sr, bpm, origin);
    return out;
}

void Analyzer::finaliseSlices (std::vector<Slice>& slices, int64_t numSamples, double sr, double bpm, int64_t origin)
{
    std::sort (slices.begin(), slices.end(), [] (auto& a, auto& b) { return a.start < b.start; });
    slices.erase (std::remove_if (slices.begin(), slices.end(), [numSamples] (auto& s) { return s.start >= numSamples - 16; }), slices.end());
    const double sps = sr * 60.0 / bpm / 4.0;
    for (size_t i = 0; i < slices.size(); ++i)
    {
        slices[i].end = (i + 1 < slices.size()) ? slices[i + 1].start : numSamples;
        slices[i].gridPos = (float) ((double) (slices[i].start - origin) / sps);
    }
    slices.erase (std::remove_if (slices.begin(), slices.end(), [] (auto& s) { return s.end - s.start < 16; }), slices.end());
}

//==============================================================================
void Analyzer::classify (std::vector<Slice>& slices, const juce::AudioBuffer<float>& audio, double sr, double bpm, int64_t origin)
{
    if (slices.empty()) return;
    const auto x = toMono (audio);
    const int64_t len = (int64_t) x.size();

    const int order = sr > 64000.0 ? 12 : 11;
    const int N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) N * 2);
    const double binHz = sr / N;
    auto bin = [&] (double hz) { return juce::jlimit (1, N / 2, (int) std::round (hz / binHz)); };
    const int e1 = bin (150.0), e2 = bin (500.0), e3 = bin (4000.0);

    const double sps = sr * 60.0 / bpm / 4.0;
    std::vector<float> scoreK, scoreS, scoreH;

    // Spectrum of the attack MINUS the spectrum just before it : isolates the new hit from the
    // ringing tail of the previous one (a quiet ghost snare over a kick tail would otherwise look like a kick)
    std::vector<float> pre ((size_t) N * 2);
    std::vector<float> attackDb;
    for (auto& s : slices)
    {
        const int segLen = (int) juce::jlimit<int64_t> (64, (int64_t) N, juce::jmin (s.end - s.start, (int64_t) (0.07 * sr)));
        std::fill (buf.begin(), buf.end(), 0.0f);
        std::fill (pre.begin(), pre.end(), 0.0f);
        float pk = 0.0f;
        for (int i = 0; i < segLen; ++i)
        {
            const float pos = (float) i / (float) segLen;
            const float w = pos < 0.7f ? 1.0f : 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (pos - 0.7f) / 0.3f);
            const int64_t a = s.start + i;
            if (a < len)
            {
                buf[(size_t) i] = x[(size_t) a] * w;
                pk = juce::jmax (pk, std::abs (x[(size_t) a]));
            }
            // previous audio, time-reversed so its most recent part gets the full weight
            const int64_t b = s.start - 1 - i;
            if (b >= 0) pre[(size_t) i] = x[(size_t) b] * w;
        }
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        fft.performFrequencyOnlyForwardTransform (pre.data(), true);
        double eL = 0, eLM = 0, eM = 0, eH = 0;
        for (int k = 1; k <= N / 2; ++k)
        {
            const double m2 = juce::jmax (0.0, (double) buf[(size_t) k] * buf[(size_t) k] - 1.5 * (double) pre[(size_t) k] * pre[(size_t) k]);
            if (k < e1) eL += m2; else if (k < e2) eLM += m2; else if (k < e3) eM += m2; else eH += m2;
        }
        const double tot = eL + eLM + eM + eH + 1.0e-12;
        s.lowF = (float) (eL / tot); s.lowMidF = (float) (eLM / tot); s.midF = (float) (eM / tot); s.highF = (float) (eH / tot);
        s.levelDb = gainToDb (pk);
        attackDb.push_back ((float) (10.0 * std::log10 (tot)));
    }

    // relative level
    std::vector<float> levels;
    for (auto& s : slices) levels.push_back (s.levelDb);
    const float medLevel = percentile (levels, 0.5f);
    float spread = percentile (levels, 0.9f) - percentile (levels, 0.1f);
    if (spread < 3.0f) spread = 3.0f;

    for (auto& s : slices)
    {
        const float zl = juce::jlimit (-2.0f, 2.0f, (s.levelDb - medLevel) / (spread * 0.5f));
        float k = 2.0f * s.lowF + 0.3f * s.lowMidF - 0.5f * s.highF + 0.15f * zl;
        float sn = (s.lowMidF + s.midF) + 0.3f * s.highF - 0.6f * s.lowF + 0.10f * zl;
        float h = 1.6f * s.highF + 0.3f * s.midF - 1.5f * s.lowF - 0.5f * s.lowMidF - 0.12f * zl;

        // light rhythmic priors (kick on the one, snare on the backbeat)
        const double g = (double) (s.start - origin) / sps;
        const int cell = (int) std::round (g);
        if (std::abs (g - cell) < 0.25)
        {
            const int inBar = ((cell % 16) + 16) % 16;
            if (inBar == 0) k += 0.15f;
            if (inBar == 8) k += 0.06f;
            if (inBar == 4 || inBar == 12) sn += 0.15f;
            if (inBar % 2 == 1) h += 0.05f;
        }
        scoreK.push_back (k); scoreS.push_back (sn); scoreH.push_back (h);
    }

    for (size_t i = 0; i < slices.size(); ++i)
    {
        if (slices[i].userClass) continue;
        const float k = scoreK[i], sn = scoreS[i], h = scoreH[i];
        slices[i].cls = (k >= sn && k >= h) ? HitClass::Kick : (sn >= h ? HitClass::Snare : HitClass::Hat);
    }

    // ghost notes : snare-like hits clearly quieter than the main snares
    // (loudness of the NEW energy only, so the tail of the previous hit does not count)
    std::vector<float> snareLevels;
    for (size_t i = 0; i < slices.size(); ++i) if (slices[i].cls == HitClass::Snare) snareLevels.push_back (attackDb[i]);
    if (snareLevels.size() >= 2)
    {
        const float ref = percentile (snareLevels, 0.75f);
        for (size_t i = 0; i < slices.size(); ++i)
            if (! slices[i].userClass && slices[i].cls == HitClass::Snare && attackDb[i] < ref - 9.0f)
                slices[i].cls = HitClass::Ghost;
    }

    // guarantee at least one kick and one snare for the generator
    if (slices.size() >= 3)
    {
        auto has = [&] (HitClass c) { for (auto& s : slices) if (s.cls == c) return true; return false; };
        if (! has (HitClass::Kick))
        {
            size_t bi = 0;
            for (size_t i = 1; i < slices.size(); ++i) if (scoreK[i] > scoreK[bi]) bi = i;
            if (! slices[bi].userClass) slices[bi].cls = HitClass::Kick;
        }
        if (! has (HitClass::Snare))
        {
            int bi = -1;
            for (size_t i = 0; i < slices.size(); ++i)
                if (slices[i].cls != HitClass::Kick && ! slices[i].userClass && (bi < 0 || scoreS[i] > scoreS[(size_t) bi]))
                    bi = (int) i;
            if (bi >= 0) slices[(size_t) bi].cls = HitClass::Snare;
        }
    }
}

} // namespace bc
