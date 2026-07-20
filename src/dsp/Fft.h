#pragma once

#include <array>
#include <cmath>
#include <cstring>

// Fixed-size in-place radix-2 FFT (JUCE-free). N must be a power of two.
// Twiddle factors and bit-reversal indices are precomputed at construction.
namespace dsp
{
template <int N>
class Fft
{
    static_assert ((N & (N - 1)) == 0 && N >= 4, "N must be a power of two >= 4");

public:
    Fft() { buildTables(); }

    void forward (float* re, float* im) const
    {
        permute (re, im);
        transform (re, im, false);
    }

    void inverse (float* re, float* im) const
    {
        permute (re, im);
        transform (re, im, true);
        const float s = 1.0f / (float) N;
        for (int i = 0; i < N; ++i)
        {
            re[i] *= s;
            im[i] *= s;
        }
    }

    // Cross-correlation c[lag] = sum_{i=0}^{lenA-1} a[i]*b[i+lag], lag = 0..maxLag.
    // Requires lenA + lenB <= N (b is indexed through lenA + maxLag).
    void crosscorr (const float* a, int lenA, const float* b, int lenB, int maxLag, float* c) const
    {
        float re[N], im[N];
        float bre[N], bim[N];
        std::memset (re, 0, sizeof (re));
        std::memset (im, 0, sizeof (im));
        std::memset (bre, 0, sizeof (bre));
        std::memset (bim, 0, sizeof (bim));
        std::memcpy (re, a, (size_t) lenA * sizeof (float));
        std::memcpy (bre, b, (size_t) lenB * sizeof (float));

        forward (re, im);
        forward (bre, bim);

        for (int i = 0; i < N; ++i)
        {
            const float ar = re[i], ai = im[i];
            const float rr = bre[i], ri = -bim[i]; // conj(b)
            re[i] = ar * rr - ai * ri;
            im[i] = ar * ri + ai * rr;
        }

        inverse (re, im);

        const int lim = std::min (maxLag, N - 1);
        for (int lag = 0; lag <= lim; ++lag)
            c[lag] = re[lag];
    }

    // Linear autocorrelation r[lag] = sum_{i=0}^{len-lag-1} x[i]*x[i+lag].
    void autocorr (const float* x, int len, int maxLag, float* r) const
    {
        float re[N], im[N];
        std::memset (re, 0, sizeof (re));
        std::memset (im, 0, sizeof (im));
        std::memcpy (re, x, (size_t) len * sizeof (float));

        forward (re, im);

        for (int i = 0; i < N; ++i)
        {
            const float p = re[i] * re[i] + im[i] * im[i];
            re[i] = p;
            im[i] = 0.0f;
        }

        inverse (re, im);

        const int lim = std::min (maxLag, N - 1);
        for (int lag = 0; lag <= lim; ++lag)
            r[lag] = re[lag];
    }

private:
    std::array<int, N> bitRev {};
    std::array<float, N / 2> twRe {}, twIm {};

    static int ilog2 (int n)
    {
        int r = 0;
        while ((1 << r) < n)
            ++r;
        return r;
    }

    void buildTables()
    {
        const int log2n = ilog2 (N);
        for (int i = 0; i < N; ++i)
        {
            int j = 0, x = i;
            for (int k = 0; k < log2n; ++k)
            {
                j = (j << 1) | (x & 1);
                x >>= 1;
            }
            bitRev[(size_t) i] = j;
        }

        for (int i = 0; i < N / 2; ++i)
        {
            const float a = -2.0f * 3.14159265358979f * (float) i / (float) N;
            twRe[(size_t) i] = std::cos (a);
            twIm[(size_t) i] = std::sin (a);
        }
    }

    void permute (float* re, float* im) const
    {
        for (int i = 0; i < N; ++i)
        {
            const int j = bitRev[(size_t) i];
            if (i < j)
            {
                std::swap (re[i], re[j]);
                std::swap (im[i], im[j]);
            }
        }
    }

    void transform (float* re, float* im, bool inverse) const
    {
        for (int len = 2, stage = 1; len <= N; len <<= 1, ++stage)
        {
            const int half = len >> 1;
            const int step = N / len;
            const float sign = inverse ? 1.0f : -1.0f;
            for (int i = 0; i < N; i += len)
            {
                for (int j = 0; j < half; ++j)
                {
                    const int t = j * step;
                    const float wr = twRe[(size_t) t];
                    const float wi = sign * twIm[(size_t) t];
                    const int k = i + j;
                    const float tr = wr * re[k + half] - wi * im[k + half];
                    const float ti = wr * im[k + half] + wi * re[k + half];
                    const float ur = re[k];
                    const float ui = im[k];
                    re[k] = ur + tr;
                    im[k] = ui + ti;
                    re[k + half] = ur - tr;
                    im[k + half] = ui - ti;
                }
            }
        }
    }
};

} // namespace dsp
