#include <paperweight/generator.hpp>
#include <paperweight/hash.hpp>
#include <paperweight/image.hpp>
#include <paperweight/material.hpp>
#include <paperweight/noise.hpp>
#include <paperweight/version.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <utility>

namespace {

int failures = 0;

void expect(bool condition, std::string_view description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void expectNear(double actual, double expected, double tolerance, std::string_view description)
{
    expect(std::abs(actual - expected) <= tolerance, description);
}

template<typename Exception, typename Function>
void expectThrows(Function&& function, std::string_view description)
{
    try {
        function();
        expect(false, description);
    } catch (const Exception&) {
    } catch (...) {
        expect(false, description);
    }
}

std::uint64_t checksum(std::span<const paperweight::Rgba8> pixels)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto& pixel : pixels) {
        for (const auto channel : {pixel.red, pixel.green, pixel.blue, pixel.alpha}) {
            hash ^= channel;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

void testVersion()
{
    constexpr paperweight::Version expected{0, 0, 1};
    static_assert(paperweight::currentVersion == expected);
    expect(paperweight::versionString() == "0.0.1", "version string is 0.0.1");
}

void testImage()
{
    constexpr paperweight::Rgba8 fill{10, 20, 30, 40};
    paperweight::Image image(3, 2, fill);
    expect(image.width() == 3 && image.height() == 2, "image retains its dimensions");
    expect(image.format() == paperweight::PixelFormat::rgba8Unorm, "image reports RGBA8");
    expect(image.bytesPerRow() == 12, "image reports its byte stride");
    expect(image.pixels().size() == 6, "image owns contiguous pixels");
    expect(image.row(1).front() == fill, "image rows address the expected pixels");

    image.row(1)[2] = paperweight::Rgba8{1, 2, 3, 4};
    expect(image.pixels().back() == paperweight::Rgba8{1, 2, 3, 4}, "row writes update storage");

    auto copy = image;
    expect(copy.pixels().data() != image.pixels().data() &&
               std::equal(copy.pixels().begin(), copy.pixels().end(), image.pixels().begin()),
           "image copies own independent equivalent storage");
    auto moved = std::move(copy);
    expect(moved.width() == 3 && moved.pixels().back() == paperweight::Rgba8{1, 2, 3, 4},
           "image moves retain their dimensions and pixels");

    expectThrows<std::invalid_argument>([] { paperweight::Image invalid(0, 1); },
                                        "zero-width images are rejected");
    expectThrows<std::length_error>(
        [] {
            paperweight::Image enormous(
                std::numeric_limits<std::uint32_t>::max(),
                std::numeric_limits<std::uint32_t>::max());
        },
        "unaddressable image storage is rejected before allocation");
    expectThrows<std::out_of_range>([&image] { static_cast<void>(image.row(2)); },
                                    "out-of-range rows are rejected");
}

void testHashing()
{
    const auto first = paperweight::hashCoordinates(1234, -5, 9, 2);
    expect(first == 0xb189c66deeb2ea9dULL, "coordinate hash matches its golden vector");
    expect(first == paperweight::hashCoordinates(1234, -5, 9, 2),
           "coordinate hashing is repeatable");
    expect(first != paperweight::hashCoordinates(1235, -5, 9, 2), "seed changes the hash");
    expect(first != paperweight::hashCoordinates(1234, -4, 9, 2), "coordinate changes the hash");
    expect(first != paperweight::hashCoordinates(1234, -5, 9, 3), "stream changes the hash");
    expect(paperweight::unitDouble(first) >= 0.0 && paperweight::unitDouble(first) < 1.0,
           "unit conversion stays in [0, 1)");
}

void testPeriodicNoise()
{
    constexpr double tolerance = 1.0e-12;
    for (const double x : {-7.25, -0.5, 0.0, 1.125, 8.75}) {
        for (const double y : {-3.75, 0.25, 5.5}) {
            const double sample = paperweight::periodicValueNoise2D(x, y, 7, 5, 9981);
            expectNear(sample, paperweight::periodicValueNoise2D(x + 7.0, y, 7, 5, 9981),
                       tolerance, "value noise repeats on x");
            expectNear(sample, paperweight::periodicValueNoise2D(x, y + 5.0, 7, 5, 9981),
                       tolerance, "value noise repeats on y");
        }
    }

    expectThrows<std::invalid_argument>(
        [] { static_cast<void>(paperweight::periodicValueNoise2D(0.0, 0.0, 0, 2, 1)); },
        "zero noise periods are rejected");
}

void testMaterialAndFbm()
{
    const paperweight::Material material;
    expect(!paperweight::validateMaterial(material), "default material is valid");
    const auto& gainMetadata = paperweight::metadataFor(paperweight::MaterialParameter::gain);
    expect(gainMetadata.key == "noise.gain" && gainMetadata.defaultValue == material.gain,
           "parameter metadata describes the material defaults");

    auto invalid = material;
    invalid.octaves = 0;
    expect(paperweight::validateMaterial(invalid).has_value(), "invalid octave count is diagnosed");
    invalid = material;
    invalid.gain = std::numeric_limits<double>::quiet_NaN();
    expect(paperweight::validateMaterial(invalid).has_value(), "non-finite gain is diagnosed");

    const double sample = paperweight::periodicFbm2D(-0.375, 0.625, material);
    expectNear(sample, paperweight::periodicFbm2D(0.625, 0.625, material), 1.0e-12,
               "FBM repeats after one tile on x");
    expectNear(sample, paperweight::periodicFbm2D(-0.375, 1.625, material), 1.0e-12,
               "FBM repeats after one tile on y");
    expect(sample >= 0.0 && sample <= 1.0, "normalised FBM stays in range");
}

void testGenerator()
{
    const paperweight::GenerationRequest request{paperweight::Material{}, 48, 32};
    auto firstResult = paperweight::generate(request);
    auto secondResult = paperweight::generate(request);
    expect(std::holds_alternative<paperweight::Image>(firstResult), "valid request generates an image");
    expect(std::holds_alternative<paperweight::Image>(secondResult), "repeated request generates an image");
    if (!std::holds_alternative<paperweight::Image>(firstResult) ||
        !std::holds_alternative<paperweight::Image>(secondResult)) {
        return;
    }

    const auto& first = std::get<paperweight::Image>(firstResult);
    const auto& second = std::get<paperweight::Image>(secondResult);
    expect(checksum(first.pixels()) == 4981563472745378647ULL,
           "default material matches its golden checksum");
    expect(first.pixels().size() == static_cast<std::size_t>(48 * 32),
           "generator returns the requested dimensions");
    expect(checksum(first.pixels()) == checksum(second.pixels()),
           "identical generation requests are byte-identical");
    expect(first.pixels().front().alpha == 255 && first.pixels().back().alpha == 255,
           "generated texture is opaque RGBA");

    auto changedRequest = request;
    changedRequest.material.seed += 1;
    const auto changedResult = paperweight::generate(changedRequest);
    expect(std::holds_alternative<paperweight::Image>(changedResult), "changed seed still generates");
    if (const auto* changed = std::get_if<paperweight::Image>(&changedResult)) {
        expect(checksum(first.pixels()) != checksum(changed->pixels()), "seed changes generated pixels");
    }

    auto invalidRequest = request;
    invalidRequest.width = 0;
    const auto invalidResult = paperweight::generate(invalidRequest);
    expect(std::holds_alternative<paperweight::GenerationError>(invalidResult),
           "invalid dimensions return a structured error");
}

} // namespace

int main()
{
    testVersion();
    testImage();
    testHashing();
    testPeriodicNoise();
    testMaterialAndFbm();
    testGenerator();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All Paperweight tests passed\n";
    return 0;
}
