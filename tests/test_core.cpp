#include "../Source/FormantCore.h"
#include <cstdio>
#include <chrono>
#include <random>
using namespace formantflex;
static const double PI = 3.14159265358979323846;

// synthetic vowel: pulse train through 3 two-pole resonators
std::vector<float> makeVowel (double sr, int len, double f0)
{
    std::vector<float> x (len, 0.0f);
    double phase = 0;
    for (int i = 0; i < len; ++i) { phase += f0 / sr; if (phase >= 1) { phase -= 1; x[i] = 1.0f; } }
    double F[3] = {700, 1200, 2600}, B[3] = {80, 90, 120};
    for (int r = 0; r < 3; ++r)
    {
        double R = std::exp (-PI * B[r] / sr), th = 2 * PI * F[r] / sr;
        double a1 = 2 * R * std::cos (th), a2 = -R * R, y1 = 0, y2 = 0;
        for (int i = 0; i < len; ++i) { double y = x[i] + a1 * y1 + a2 * y2; y2 = y1; y1 = y; x[i] = (float) y; }
    }
    float mx = 0; for (float v : x) mx = std::max (mx, std::abs (v));
    for (float& v : x) v *= 0.5f / mx;
    return x;
}

std::vector<float> run (const std::vector<float>& in, double sr, float semis, std::vector<float>* dry = nullptr)
{
    FormantShifter s; s.prepare (sr, FormantShifter::chooseFftSize (sr));
    s.setFormantSemitones (semis); s.reset();
    std::vector<float> out (in.size());
    if (dry) dry->resize (in.size());
    for (size_t i = 0; i < in.size(); ++i) { float d; out[i] = s.processSample (in[i], d); if (dry) (*dry)[i] = d; }
    return out;
}

void writeRaw (const char* path, const std::vector<float>& v) { FILE* f = fopen (path, "wb"); fwrite (v.data(), 4, v.size(), f); fclose (f); }

int main (int argc, char** argv)
{
    const double sr = 48000; const int len = 48000 * 2;
    std::mt19937 rng (1); std::uniform_real_distribution<float> U (-0.5f, 0.5f);
    std::vector<float> noise (len); for (auto& v : noise) v = U (rng);

    FormantShifter probe; probe.prepare (sr, 2048); const int lat = probe.getLatency();
    std::vector<float> dry; auto y0 = run (noise, sr, 0.0f, &dry);
    double maxErr = 0, maxDryErr = 0;
    for (int i = lat + 4096; i < len; ++i)
    {
        maxErr = std::max (maxErr, (double) std::abs (y0[i] - noise[i - lat]));
        maxDryErr = std::max (maxDryErr, (double) std::abs (dry[i] - noise[i - lat]));
    }
    printf ("latency=%d  reconstruction maxErr=%.2e  dryAlign maxErr=%.2e\n", lat, maxErr, maxDryErr);

    double F0 = argc > 1 ? atof (argv[1]) : 150; auto vowel = makeVowel (sr, len, F0);
    writeRaw ("vowel.f32", vowel);
    float shifts[] = {-4, 4, 7};
    for (float s : shifts)
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        auto y = run (vowel, sr, s);
        double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        char p[64]; snprintf (p, 64, "out_%+.0f.f32", s); writeRaw (p, y);
        float mx = 0; for (float v : y) if (!std::isfinite (v)) { printf ("NaN!\n"); return 1; } else mx = std::max (mx, std::abs (v));
        printf ("shift %+.0f st: peak=%.3f  CPU=%.1f%% of realtime (mono)\n", s, mx, 100.0 * sec / 2.0);
    }
    return 0;
}
