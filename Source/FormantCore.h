#pragma once
// FormantFlex DSP core — independent of JUCE so it can be unit-tested.
// Method: STFT (Hann, 75% overlap). For every frame the spectral envelope is
// estimated with an iterative "true envelope" cepstral method, warped along the
// frequency axis by the formant factor and applied as a gain curve. Phases and
// harmonics (= pitch) stay untouched, so only the formants move.

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace formantflex
{

class FFT
{
public:
    void prepare (int n)
    {
        size = n;
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
        cosT.resize ((size_t) n / 2);
        sinT.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
        {
            const double a = -2.0 * 3.14159265358979323846 * k / n;
            cosT[(size_t) k] = (float) std::cos (a);
            sinT[(size_t) k] = (float) std::sin (a);
        }
    }

    // In-place complex FFT on split real/imag arrays. Inverse is scaled by 1/N.
    void perform (float* re, float* im, bool inverse) const
    {
        const int n = size;
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        const float sgn = inverse ? -1.0f : 1.0f;
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int j = 0; j < half; ++j)
                {
                    const float wr = cosT[(size_t) (j * step)];
                    const float wi = sgn * sinT[(size_t) (j * step)];
                    const int a = i + j, b = a + half;
                    const float vr = re[b] * wr - im[b] * wi;
                    const float vi = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - vr; im[b] = im[a] - vi;
                    re[a] += vr;        im[a] += vi;
                }
            }
        }
        if (inverse)
        {
            const float s = 1.0f / (float) n;
            for (int i = 0; i < n; ++i) { re[i] *= s; im[i] *= s; }
        }
    }

    int getSize() const { return size; }

private:
    int size = 0;
    std::vector<int> rev;
    std::vector<float> cosT, sinT;
};

// Real-part of the DFT of a real, even-symmetric sequence of length N, computed
// with a half-size complex FFT (twice as fast as a full complex transform).
class RealEvenDFT
{
public:
    void prepare (int n)
    {
        N = n; M = n / 2;
        half.prepare (M);
        zr.assign ((size_t) M, 0.0f); zi.assign ((size_t) M, 0.0f);
        cosN.resize ((size_t) M + 1); sinN.resize ((size_t) M + 1);
        for (int k = 0; k <= M; ++k)
        {
            const double a = -2.0 * 3.14159265358979323846 * k / N;
            cosN[(size_t) k] = (float) std::cos (a);
            sinN[(size_t) k] = (float) std::sin (a);
        }
    }

    // x: N real samples (must be even-symmetric). out: bins 0..N/2 (real part).
    void perform (const float* x, float* out)
    {
        for (int n = 0; n < M; ++n) { zr[(size_t) n] = x[2 * n]; zi[(size_t) n] = x[2 * n + 1]; }
        half.perform (zr.data(), zi.data(), false);
        for (int k = 0; k <= M; ++k)
        {
            const int a = k % M, b = (M - k) % M;
            const float ar = zr[(size_t) a], ai = zi[(size_t) a];
            const float br = zr[(size_t) b], bi = -zi[(size_t) b];      // conj(Z[M-k])
            const float er  = 0.5f * (ar + br);                          // Re(even part)
            const float orr = 0.5f * (ar - br), oi = 0.5f * (ai - bi);  // odd part
            // X[k] = E - i * W^k * O  ->  Re(X[k]) = Re(E) + Im(W^k * O)
            const float wr = cosN[(size_t) k], wi = sinN[(size_t) k];
            out[k] = er + (wr * oi + wi * orr);
        }
    }

private:
    int N = 0, M = 0;
    FFT half;
    std::vector<float> zr, zi, cosN, sinN;
};

class FormantShifter
{
public:
    static int chooseFftSize (double sampleRate)
    {
        if (sampleRate <= 50000.0)  return 2048;
        if (sampleRate <= 100000.0) return 4096;
        return 8192;
    }

    void prepare (double newSampleRate, int fftSize)
    {
        sampleRate = newSampleRate;
        N = fftSize;
        hop = N / 4;
        half = N / 2;
        fft.prepare (N);
        evenDft.prepare (N);
        evenBuf.assign ((size_t) N, 0.0f);
        cepBins.assign ((size_t) half + 1, 0.0f);

        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / N));

        inFifo.assign ((size_t) N, 0.0f);
        outFifo.assign ((size_t) N, 0.0f);
        outAccum.assign ((size_t) (2 * N), 0.0f);
        dryRing.assign ((size_t) N, 0.0f);
        re.assign ((size_t) N, 0.0f);  im.assign ((size_t) N, 0.0f);
        cre.assign ((size_t) N, 0.0f); cim.assign ((size_t) N, 0.0f);
        logMag.assign ((size_t) half + 1, 0.0f);
        logEnv.assign ((size_t) half + 1, 0.0f);
        target.assign ((size_t) half + 1, 0.0f);
        displayOrigDb.assign ((size_t) half + 1, -120.0f);
        displayShiftDb.assign ((size_t) half + 1, -120.0f);
        reset();
    }

    void reset()
    {
        std::fill (inFifo.begin(), inFifo.end(), 0.0f);
        std::fill (outFifo.begin(), outFifo.end(), 0.0f);
        std::fill (outAccum.begin(), outAccum.end(), 0.0f);
        std::fill (dryRing.begin(), dryRing.end(), 0.0f);
        rover = fifoStart();
        dryPos = 0;
        currentRatio = targetRatio;
    }

    // Total input->output delay in samples (report this to the host).
    int getLatency() const   { return N; }
    int getFftSize() const   { return N; }
    double getSampleRate() const { return sampleRate; }

    void setFormantSemitones (float semis) { targetRatio = std::pow (2.0f, semis / 12.0f); }
    // detail 0..1 -> envelope resolution (0 = very smooth, 1 = sharp formants)
    void setDetail (float detail01)        { detail = std::min (1.0f, std::max (0.0f, detail01)); }
    void setIterations (int it)            { iterations = std::max (1, it); }

    // Returns the processed (wet) sample. dryDelayed receives the input delayed
    // by exactly getLatency() samples, so dry/wet mixing stays phase aligned.
    inline float processSample (float x, float& dryDelayed)
    {
        dryDelayed = dryRing[(size_t) dryPos];
        dryRing[(size_t) dryPos] = x;
        if (++dryPos >= N) dryPos = 0;

        inFifo[(size_t) rover] = x;
        const float y = outFifo[(size_t) (rover - fifoStart())];
        if (++rover >= N)
        {
            rover = fifoStart();
            processFrame();
        }
        return y;
    }

    // Envelope of the most recent frame (dB per bin 0..N/2) for visualisation.
    const std::vector<float>& getDisplayOriginalDb() const { return displayOrigDb; }
    const std::vector<float>& getDisplayShiftedDb() const  { return displayShiftDb; }
    unsigned int getFrameCounter() const { return frameCounter; }

private:
    int fifoStart() const { return N - hop; }

    void computeEnvelope (int lifterQ)
    {
        // Floor the log spectrum 100 dB below its peak so holes don't dominate.
        float peak = -1e30f;
        for (int k = 0; k <= half; ++k) peak = std::max (peak, logMag[(size_t) k]);
        const float floorV = peak - 11.5f; // ln(10^5) ≈ 11.5 -> 100 dB
        for (int k = 0; k <= half; ++k) target[(size_t) k] = std::max (logMag[(size_t) k], floorV);

        for (int it = 0; it < iterations; ++it)
        {
            if (it > 0)
                for (int k = 0; k <= half; ++k)
                    target[(size_t) k] = std::max (target[(size_t) k], logEnv[(size_t) k]);

            // even-symmetric real log spectrum -> real cepstrum (IDFT = DFT / N for even data)
            for (int k = 0; k <= half; ++k) evenBuf[(size_t) k] = target[(size_t) k];
            for (int k = 1; k < half; ++k)  evenBuf[(size_t) (N - k)] = target[(size_t) k];
            evenDft.perform (evenBuf.data(), cepBins.data());

            // low-quefrency lifter (keeps the smooth envelope, drops pitch harmonics)
            const float invN = 1.0f / (float) N;
            evenBuf[0] = cepBins[0] * invN;
            for (int q = 1; q <= half; ++q)
            {
                float w = 0.0f;
                if (q < lifterQ) w = invN;
                else if (q == lifterQ) w = 0.5f * invN;
                evenBuf[(size_t) q] = cepBins[(size_t) q] * w;
                if (q != half) evenBuf[(size_t) (N - q)] = evenBuf[(size_t) q];
            }
            evenDft.perform (evenBuf.data(), cepBins.data());

            float maxAbove = 0.0f;
            for (int k = 0; k <= half; ++k)
            {
                logEnv[(size_t) k] = cepBins[(size_t) k];
                maxAbove = std::max (maxAbove, logMag[(size_t) k] - logEnv[(size_t) k]);
            }
            if (maxAbove < 0.23f) break; // envelope within 2 dB of all spectral peaks
        }
    }

    // Pitch-adaptive cepstral order: about half the pitch period, so the envelope
    // follows the formants without tracing individual harmonics.
    int chooseLifterOrder()
    {
        // autocorrelation = IFFT of power spectrum
        for (int k = 0; k <= half; ++k)
        {
            const float p = re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
            cre[(size_t) k] = p; cim[(size_t) k] = 0.0f;
            if (k > 0 && k < half) { cre[(size_t) (N - k)] = p; cim[(size_t) (N - k)] = 0.0f; }
        }
        fft.perform (cre.data(), cim.data(), true);

        const int minLag = std::max (2, (int) (sampleRate / 1000.0));
        const int maxLag = std::min (half - 1, (int) (sampleRate / 60.0));
        const float r0 = cre[0] + 1e-30f;
        int bestLag = 0; float best = 0.0f;
        for (int l = minLag; l <= maxLag; ++l)
        {
            // compensate the Hann-window taper of the autocorrelation
            const float v = cre[(size_t) l] / (r0 * (1.0f - (float) l / (float) N));
            if (v > best && cre[(size_t) l] >= cre[(size_t) (l - 1)] && cre[(size_t) l] >= cre[(size_t) (l + 1)])
            { best = v; bestLag = l; }
        }

        float order;
        if (best > 0.35f && bestLag > 0)
        {
            smoothedPeriod = smoothedPeriod > 0.0f ? 0.6f * smoothedPeriod + 0.4f * (float) bestLag : (float) bestLag;
            order = smoothedPeriod * (0.30f + 0.30f * detail); // 0.3..0.6 of the period
        }
        else
            order = (float) sampleRate * 0.0012f; // unvoiced/noisy: ~1.2 ms

        const int q = (int) std::lround (order);
        return std::max (8, std::min (q, half - 1));
    }

    void processFrame()
    {
        ++frameCounter;
        currentRatio += (targetRatio - currentRatio) * 0.35f; // per-frame glide (~30 ms)

        double energy = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const float v = inFifo[(size_t) i];
            energy += (double) v * v;
            re[(size_t) i] = v * window[(size_t) i];
            im[(size_t) i] = 0.0f;
        }

        const bool silent = energy < 1e-12 * N;
        const bool neutral = std::abs (currentRatio - 1.0f) < 1e-4f;
        constexpr float toDb = 8.685889638f; // 20/ln(10)
        const float dbOffset = -toDb * std::log ((float) N * 0.25f);

        if (! silent)
        {
            fft.perform (re.data(), im.data(), false);

            for (int k = 0; k <= half; ++k)
            {
                const float m2 = re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
                logMag[(size_t) k] = 0.5f * std::log (m2 + 1e-24f);
            }

            const int lifterQ = chooseLifterOrder();
            computeEnvelope (lifterQ);

            const float invRatio = 1.0f / currentRatio;
            constexpr float maxBoost = 2.07f;  // +18 dB
            constexpr float maxCut   = -4.6f;  // -40 dB

            for (int k = 0; k <= half; ++k)
            {
                float warped;
                const float src = (float) k * invRatio;
                if (src >= (float) half) warped = logEnv[(size_t) half] - 2.3f; // fade above Nyquist
                else
                {
                    const int i0 = (int) src;
                    const float fr = src - (float) i0;
                    warped = logEnv[(size_t) i0] + fr * (logEnv[(size_t) (i0 + 1)] - logEnv[(size_t) i0]);
                }

                displayOrigDb[(size_t) k] = toDb * logEnv[(size_t) k] + dbOffset;
                displayShiftDb[(size_t) k] = toDb * warped + dbOffset;

                if (! neutral)
                {
                    const float g = std::exp (std::min (maxBoost, std::max (maxCut, warped - logEnv[(size_t) k])));
                    re[(size_t) k] *= g; im[(size_t) k] *= g;
                    if (k > 0 && k < half) { re[(size_t) (N - k)] *= g; im[(size_t) (N - k)] *= g; }
                }
            }

            fft.perform (re.data(), im.data(), true);
        }
        else
        {
            std::fill (displayOrigDb.begin(), displayOrigDb.end(), -120.0f);
            std::fill (displayShiftDb.begin(), displayShiftDb.end(), -120.0f);
        }

        // Hann analysis * Hann synthesis at 75% overlap sums to 1.5
        const float norm = 1.0f / 1.5f;
        for (int i = 0; i < N; ++i)
            outAccum[(size_t) i] += re[(size_t) i] * window[(size_t) i] * norm;

        for (int i = 0; i < hop; ++i) outFifo[(size_t) i] = outAccum[(size_t) i];
        std::memmove (outAccum.data(), outAccum.data() + hop, sizeof (float) * (size_t) N);
        std::fill (outAccum.begin() + N, outAccum.end(), 0.0f);
        std::memmove (inFifo.data(), inFifo.data() + hop, sizeof (float) * (size_t) fifoStart());
    }

    FFT fft;
    RealEvenDFT evenDft;
    std::vector<float> evenBuf, cepBins;
    double sampleRate = 44100.0;
    int N = 2048, hop = 512, half = 1024, rover = 0, dryPos = 0, iterations = 6;
    float targetRatio = 1.0f, currentRatio = 1.0f, detail = 0.5f, smoothedPeriod = 0.0f;
    unsigned int frameCounter = 0;
    std::vector<float> dryRing, window, inFifo, outFifo, outAccum, re, im, cre, cim, logMag, logEnv, target;
    std::vector<float> displayOrigDb, displayShiftDb;
};

} // namespace formantflex
