#include <paperweight/stylised_lighting.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace paperweight {
namespace {

struct Vector3 {
    double x{};
    double y{};
    double z{1.0};
};

bool validSettings(const StylisedLightingSettings& settings)
{
    return std::isfinite(settings.lightAzimuthDegrees) &&
        std::isfinite(settings.lightElevationDegrees) &&
        settings.diffuseBands >= 2 && settings.diffuseBands <= 16 &&
        std::isfinite(settings.highlightThreshold) &&
        settings.highlightThreshold >= 0.0 && settings.highlightThreshold <= 1.0 &&
        std::isfinite(settings.ambientContribution) &&
        settings.ambientContribution >= 0.0 && settings.ambientContribution <= 1.0 &&
        std::isfinite(settings.highlightContribution) &&
        settings.highlightContribution >= 0.0 && settings.highlightContribution <= 1.0 &&
        std::isfinite(settings.heightNormalStrength) &&
        settings.heightNormalStrength >= 0.0 && settings.heightNormalStrength <= 32.0;
}

bool sameDimensions(const Image& left, const Image& right)
{
    return left.width() == right.width() && left.height() == right.height();
}

double luminance(const Rgba8& pixel)
{
    constexpr double inverseByte = 1.0 / 255.0;
    return (0.2126 * static_cast<double>(pixel.red) +
            0.7152 * static_cast<double>(pixel.green) +
            0.0722 * static_cast<double>(pixel.blue)) * inverseByte;
}

Vector3 normalise(Vector3 value)
{
    const double length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
    if (!(length > 0.0) || !std::isfinite(length)) {
        return {};
    }
    return {value.x / length, value.y / length, value.z / length};
}

Vector3 decodedNormal(const Rgba8& pixel)
{
    constexpr double scale = 2.0 / 255.0;
    return normalise({
        static_cast<double>(pixel.red) * scale - 1.0,
        static_cast<double>(pixel.green) * scale - 1.0,
        static_cast<double>(pixel.blue) * scale - 1.0,
    });
}

Vector3 heightNormal(
    const Image& height,
    std::uint32_t x,
    std::uint32_t y,
    double strength)
{
    const auto leftX = x == 0 ? height.width() - 1 : x - 1;
    const auto rightX = x + 1 == height.width() ? 0 : x + 1;
    const auto downY = y == 0 ? height.height() - 1 : y - 1;
    const auto upY = y + 1 == height.height() ? 0 : y + 1;
    const double horizontal =
        luminance(height.row(y)[rightX]) - luminance(height.row(y)[leftX]);
    const double vertical =
        luminance(height.row(upY)[x]) - luminance(height.row(downY)[x]);
    return normalise({-horizontal * strength, -vertical * strength, 1.0});
}

std::uint8_t channel(double value)
{
    return static_cast<std::uint8_t>(std::llround(std::clamp(value, 0.0, 255.0)));
}

} // namespace

StylisedLightingResult bakeStylisedLighting(
    const Image& unlitColour,
    const Image* height,
    const Image* normal,
    const StylisedLightingSettings& settings)
{
    if (!validSettings(settings)) {
        return StylisedLightingError{"stylised lighting settings are outside their supported range"};
    }
    if (normal == nullptr && height == nullptr) {
        return StylisedLightingError{"stylised lighting requires a normal or height image"};
    }
    if ((normal != nullptr && !sameDimensions(unlitColour, *normal)) ||
        (height != nullptr && !sameDimensions(unlitColour, *height))) {
        return StylisedLightingError{"stylised lighting inputs must have identical dimensions"};
    }

    constexpr double degreesToRadians = std::numbers::pi / 180.0;
    const double azimuth = settings.lightAzimuthDegrees * degreesToRadians;
    const double elevation = settings.lightElevationDegrees * degreesToRadians;
    const Vector3 light = normalise({
        std::cos(elevation) * std::cos(azimuth),
        std::cos(elevation) * std::sin(azimuth),
        std::sin(elevation),
    });
    const double bandScale = static_cast<double>(settings.diffuseBands - 1);

    Image result{unlitColour.width(), unlitColour.height()};
    for (std::uint32_t y = 0; y < result.height(); ++y) {
        for (std::uint32_t x = 0; x < result.width(); ++x) {
            const Vector3 surfaceNormal = normal != nullptr
                ? decodedNormal(normal->row(y)[x])
                : heightNormal(*height, x, y, settings.heightNormalStrength);
            const double diffuse = std::clamp(
                surfaceNormal.x * light.x + surfaceNormal.y * light.y +
                    surfaceNormal.z * light.z,
                0.0,
                1.0);
            const double banded = std::round(diffuse * bandScale) / bandScale;
            const double illumination = settings.ambientContribution +
                (1.0 - settings.ambientContribution) * banded;
            const double highlight = diffuse >= settings.highlightThreshold
                ? settings.highlightContribution
                : 0.0;
            const auto source = unlitColour.row(y)[x];
            result.row(y)[x] = {
                channel(static_cast<double>(source.red) * illumination + 255.0 * highlight),
                channel(static_cast<double>(source.green) * illumination + 255.0 * highlight),
                channel(static_cast<double>(source.blue) * illumination + 255.0 * highlight),
                source.alpha,
            };
        }
    }
    return result;
}

} // namespace paperweight
