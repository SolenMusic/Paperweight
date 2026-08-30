#include <paperweight/noise.hpp>

#include <paperweight/hash.hpp>

#include <cmath>
#include <stdexcept>

namespace paperweight {
namespace {

std::int64_t wrap(std::int64_t value, std::uint32_t period)
{
    const auto signedPeriod = static_cast<std::int64_t>(period);
    const auto remainder = value % signedPeriod;
    return remainder < 0 ? remainder + signedPeriod : remainder;
}

double fade(double value)
{
    return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
}

double lerp(double from, double to, double amount)
{
    return from + (to - from) * amount;
}

double latticeValue(
    std::int64_t x,
    std::int64_t y,
    std::uint32_t periodX,
    std::uint32_t periodY,
    std::uint64_t seed)
{
    return unitDouble(hashCoordinates(seed, wrap(x, periodX), wrap(y, periodY)));
}

} // namespace

double periodicValueNoise2D(
    double x,
    double y,
    std::uint32_t periodX,
    std::uint32_t periodY,
    std::uint64_t seed)
{
    if (periodX == 0 || periodY == 0) {
        throw std::invalid_argument("noise periods must be greater than zero");
    }
    if (!std::isfinite(x) || !std::isfinite(y)) {
        throw std::invalid_argument("noise coordinates must be finite");
    }

    const auto x0 = static_cast<std::int64_t>(std::floor(x));
    const auto y0 = static_cast<std::int64_t>(std::floor(y));
    const double tx = fade(x - static_cast<double>(x0));
    const double ty = fade(y - static_cast<double>(y0));

    const double top = lerp(
        latticeValue(x0, y0, periodX, periodY, seed),
        latticeValue(x0 + 1, y0, periodX, periodY, seed),
        tx);
    const double bottom = lerp(
        latticeValue(x0, y0 + 1, periodX, periodY, seed),
        latticeValue(x0 + 1, y0 + 1, periodX, periodY, seed),
        tx);
    return lerp(top, bottom, ty);
}

double periodicFbm2D(double u, double v, const Material& material)
{
    if (const auto error = validateMaterial(material)) {
        throw std::invalid_argument(*error);
    }
    if (!std::isfinite(u) || !std::isfinite(v)) {
        throw std::invalid_argument("FBM coordinates must be finite");
    }

    double total = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    std::uint32_t frequency = material.frequency;

    for (std::uint32_t octave = 0; octave < material.octaves; ++octave) {
        const auto octaveSeed = mixBits(material.seed ^ static_cast<std::uint64_t>(octave));
        total += periodicValueNoise2D(
            u * frequency,
            v * frequency,
            frequency,
            frequency,
            octaveSeed) * amplitude;
        amplitudeSum += amplitude;
        amplitude *= material.gain;
        frequency *= material.lacunarity;
    }

    return total / amplitudeSum;
}

} // namespace paperweight
