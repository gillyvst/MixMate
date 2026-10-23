#pragma once

#include <array>
#include <cmath>
#include <complex>

namespace mixmate
{

enum class FilterType { lowShelf, peak, highShelf };

struct EqBand
{
    FilterType type = FilterType::peak;
    float freq = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.707f;
};

// A headphone correction curve as plain data, safe to copy on the audio thread.
struct EqCurve
{
    static constexpr int maxBands = 16;

    std::array<EqBand, maxBands> bands {};
    int numBands = 0;
    float preampDb = 0.0f;
};

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    // RBJ cookbook filters, matching the ones AutoEQ uses to fit its curves.
    static BiquadCoeffs make (const EqBand& band, double sampleRate, float gainScale)
    {
        const double nyquistSafe = sampleRate * 0.49;
        const double f = std::fmin ((double) band.freq, nyquistSafe);
        const double w0 = 2.0 * 3.14159265358979323846 * f / sampleRate;
        const double cosw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * (double) band.q);
        const double A = std::pow (10.0, (double) (band.gainDb * gainScale) / 40.0);

        double b0, b1, b2, a0, a1, a2;

        switch (band.type)
        {
            case FilterType::lowShelf:
            {
                const double sq = 2.0 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cosw + sq);
                b1 = 2 * A * ((A - 1) - (A + 1) * cosw);
                b2 = A * ((A + 1) - (A - 1) * cosw - sq);
                a0 = (A + 1) + (A - 1) * cosw + sq;
                a1 = -2 * ((A - 1) + (A + 1) * cosw);
                a2 = (A + 1) + (A - 1) * cosw - sq;
                break;
            }
            case FilterType::highShelf:
            {
                const double sq = 2.0 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cosw + sq);
                b1 = -2 * A * ((A - 1) + (A + 1) * cosw);
                b2 = A * ((A + 1) + (A - 1) * cosw - sq);
                a0 = (A + 1) - (A - 1) * cosw + sq;
                a1 = 2 * ((A - 1) - (A + 1) * cosw);
                a2 = (A + 1) - (A - 1) * cosw - sq;
                break;
            }
            case FilterType::peak:
            default:
            {
                b0 = 1 + alpha * A;
                b1 = -2 * cosw;
                b2 = 1 - alpha * A;
                a0 = 1 + alpha / A;
                a1 = -2 * cosw;
                a2 = 1 - alpha / A;
                break;
            }
        }

        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    double magnitudeDb (double freq, double sampleRate) const
    {
        const double w = 2.0 * 3.14159265358979323846 * freq / sampleRate;
        const std::complex<double> z1 = std::polar (1.0, -w);
        const std::complex<double> z2 = z1 * z1;
        const auto h = (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
        return 20.0 * std::log10 (std::fmax (std::abs (h), 1.0e-9));
    }
};

// Response of a whole curve in dB at one frequency (without preamp).
inline double curveResponseDb (const EqCurve& curve, double freq, float strength, double sampleRate = 48000.0)
{
    double total = 0.0;
    for (int i = 0; i < curve.numBands; ++i)
        total += BiquadCoeffs::make (curve.bands[(size_t) i], sampleRate, strength).magnitudeDb (freq, sampleRate);
    return total;
}

// How much a curve changes the loudness of typical music, in dB. Music has roughly equal
// energy per octave (like pink noise), so this is the power average of the response over
// log-spaced frequencies from 100 Hz to 10 kHz, where our ears judge loudness most.
// Level matching applies the opposite of this.
inline double curveLoudnessDb (const EqCurve& curve, float strength, double sampleRate = 48000.0)
{
    constexpr int points = 64;
    std::array<BiquadCoeffs, EqCurve::maxBands> coeffs;
    for (int i = 0; i < curve.numBands; ++i)
        coeffs[(size_t) i] = BiquadCoeffs::make (curve.bands[(size_t) i], sampleRate, strength);

    double power = 0.0;
    for (int p = 0; p < points; ++p)
    {
        const double freq = 100.0 * std::pow (100.0, (double) p / (points - 1));
        double db = 0.0;
        for (int i = 0; i < curve.numBands; ++i)
            db += coeffs[(size_t) i].magnitudeDb (freq, sampleRate);
        power += std::pow (10.0, db / 10.0);
    }

    return 10.0 * std::log10 (power / points);
}

} // namespace mixmate
