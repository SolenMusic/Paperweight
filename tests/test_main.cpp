#include <paperweight/generator.hpp>
#include <paperweight/hash.hpp>
#include <paperweight/image.hpp>
#include <paperweight/evaluation.hpp>
#include <paperweight/layer.hpp>
#include <paperweight/material.hpp>
#include <paperweight/noise.hpp>
#include <paperweight/pmat.hpp>
#include <paperweight/version.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
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

paperweight::Material materialWithNoiseParameters(
    std::uint64_t seed,
    std::uint32_t frequency,
    std::uint32_t octaves,
    std::uint32_t lacunarity,
    double gain)
{
    paperweight::Material material;
    material.seed = seed;
    material.frequency = frequency;
    material.octaves = octaves;
    material.lacunarity = lacunarity;
    material.gain = gain;
    return material;
}

void testVersion()
{
    constexpr paperweight::Version expected{0, 0, 3};
    static_assert(paperweight::currentVersion == expected);
    expect(paperweight::versionString() == "0.0.3", "version string is 0.0.3");
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
    const auto& lowColourMetadata = paperweight::metadataFor(paperweight::MaterialColour::low);
    expect(lowColourMetadata.key == "colour.low" &&
               lowColourMetadata.defaultValue == material.lowColour,
           "colour metadata describes the material defaults");
    const auto& normalMetadata =
        paperweight::metadataFor(paperweight::MaterialParameter::normalStrength);
    expect(normalMetadata.key == "normal.strength" &&
               normalMetadata.defaultValue == material.normalStrength,
           "output metadata describes the normal default");

    auto invalid = material;
    invalid.octaves = 0;
    expect(paperweight::validateMaterial(invalid).has_value(), "invalid octave count is diagnosed");
    invalid = material;
    invalid.gain = std::numeric_limits<double>::quiet_NaN();
    expect(paperweight::validateMaterial(invalid).has_value(), "non-finite gain is diagnosed");
    invalid = material;
    invalid.normalStrength = 17.0;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "out-of-range normal strength is diagnosed");
    invalid = material;
    invalid.roughnessLow = -0.01;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "out-of-range roughness is diagnosed");

    const double sample = paperweight::periodicFbm2D(-0.375, 0.625, material);
    expectNear(sample, paperweight::periodicFbm2D(0.625, 0.625, material), 1.0e-12,
               "FBM repeats after one tile on x");
    expectNear(sample, paperweight::periodicFbm2D(-0.375, 1.625, material), 1.0e-12,
               "FBM repeats after one tile on y");
    expect(sample >= 0.0 && sample <= 1.0, "normalised FBM stays in range");

    const std::array representativeMaterials{
        materialWithNoiseParameters(0, 1, 1, 1, 0.1),
        materialWithNoiseParameters(42, 7, 4, 2, 0.37),
        materialWithNoiseParameters(
            std::numeric_limits<std::uint64_t>::max(), 1, 7, 4, 0.9),
    };
    for (const auto& candidate : representativeMaterials) {
        for (const auto [u, v] : std::array{
                 std::pair{-2.25, -0.125},
                 std::pair{0.0, 0.0},
                 std::pair{0.375, 0.875},
                 std::pair{4.125, 9.625},
             }) {
            const double value = paperweight::periodicFbm2D(u, v, candidate);
            expectNear(value, paperweight::periodicFbm2D(u + 1.0, v, candidate), 1.0e-12,
                       "representative FBM repeats on x");
            expectNear(value, paperweight::periodicFbm2D(u, v + 1.0, candidate), 1.0e-12,
                       "representative FBM repeats on y");
        }
    }
}

void testLayerEvaluation()
{
    paperweight::Material legacy;
    const auto legacySample = paperweight::evaluateMaterialSample(legacy, 0.375, 0.625);

    auto explicitNoise = legacy;
    explicitNoise.layers.push_back(paperweight::makeNoiseLayer());
    const auto explicitSample = paperweight::evaluateMaterialSample(explicitNoise, 0.375, 0.625);
    expect(explicitSample == legacySample,
           "an explicit base-noise layer exactly preserves legacy evaluation");

    explicitNoise.layers.front().operation = paperweight::NoiseOperation{41};
    const auto offsetSample = paperweight::evaluateMaterialSample(explicitNoise, 0.375, 0.625);
    expect(offsetSample != legacySample, "a noise-layer seed offset changes its sample");
    expect(paperweight::evaluateMaterialSample(explicitNoise, 0.375, 0.625) == offsetSample,
           "layer evaluation is deterministic");
    expectNear(
        offsetSample.scalar,
        paperweight::evaluateMaterialSample(explicitNoise, 1.375, 0.625).scalar,
        1.0e-12,
        "layered evaluation remains periodic on x");
    expectNear(
        offsetSample.scalar,
        paperweight::evaluateMaterialSample(explicitNoise, 0.375, 1.625).scalar,
        1.0e-12,
        "layered evaluation remains periodic on y");

    const auto solid = paperweight::evaluateOperation(
        paperweight::SolidColourOperation{{255, 0, 0, 128}},
        {legacy, 0.0, 0.0},
        {});
    expectNear(solid.scalar, 0.2126, 1.0e-12,
               "solid-colour scalar uses Rec. 709 luminance");
    expectNear(solid.red, 1.0, 1.0e-12, "solid-colour operation preserves red");
    expectNear(solid.alpha, 128.0 / 255.0, 1.0e-12,
               "solid-colour operation preserves alpha");

    const paperweight::EvaluatedSample background{0.25, 0.2, 0.4, 0.6, 0.8};
    const paperweight::EvaluatedSample source{0.8, 0.9, 0.5, 0.25, 0.4};
    const auto blended = paperweight::compositeSamples(
        background, source, paperweight::CompositeMode::blend, 0.5);
    expectNear(blended.scalar, 0.525, 1.0e-12, "blend interpolates scalar values");
    expectNear(blended.red, 0.55, 1.0e-12, "blend interpolates colour channels");
    const auto added = paperweight::compositeSamples(
        background, source, paperweight::CompositeMode::add, 0.5);
    expectNear(added.scalar, 0.65, 1.0e-12, "add combines scalar values");
    expectNear(added.red, 0.65, 1.0e-12, "add combines colour channels");
    const auto multiplied = paperweight::compositeSamples(
        background, source, paperweight::CompositeMode::multiply, 0.5);
    expectNear(multiplied.scalar, 0.225, 1.0e-12, "multiply combines scalar values");
    expectNear(multiplied.red, 0.19, 1.0e-12, "multiply combines colour channels");

    paperweight::Material disabled;
    disabled.layers = {
        paperweight::makeSolidColourLayer({32, 64, 96, 255}),
        paperweight::MaterialLayer{
            false,
            1.0,
            paperweight::CompositeMode::blend,
            paperweight::SolidColourOperation{{255, 255, 255, 255}}},
    };
    const auto withoutDisabled = disabled;
    auto baseOnly = withoutDisabled;
    baseOnly.layers.pop_back();
    expect(paperweight::evaluateMaterialSample(disabled, 0.2, 0.7) ==
               paperweight::evaluateMaterialSample(baseOnly, 0.2, 0.7),
           "disabled layers do not affect evaluation");

    paperweight::Material ordered;
    ordered.layers = {
        paperweight::makeSolidColourLayer({255, 0, 0, 255}),
        paperweight::MaterialLayer{
            true,
            0.5,
            paperweight::CompositeMode::blend,
            paperweight::SolidColourOperation{{0, 0, 255, 255}}},
    };
    auto reversed = ordered;
    std::reverse(reversed.layers.begin(), reversed.layers.end());
    expect(paperweight::evaluateMaterialSample(ordered, 0.0, 0.0) !=
               paperweight::evaluateMaterialSample(reversed, 0.0, 0.0),
           "layer order affects the accumulated result");

    paperweight::Material adjusted;
    adjusted.layers = {
        paperweight::makeSolidColourLayer({64, 64, 64, 255}),
        paperweight::MaterialLayer{
            true,
            1.0,
            paperweight::CompositeMode::blend,
            paperweight::LevelsOperation{0.0, 1.0, 2.0}},
    };
    const auto levelled = paperweight::evaluateMaterialSample(adjusted, 0.0, 0.0);
    expectNear(levelled.red, std::sqrt(64.0 / 255.0), 1.0e-12,
               "levels transforms the accumulated colour");

    adjusted.layers.back().operation = paperweight::ThresholdOperation{0.6};
    const auto below = paperweight::evaluateMaterialSample(adjusted, 0.0, 0.0);
    expect(below.scalar == 0.0 && below.red == 0.0,
           "threshold selects the low endpoint below its cut-off");
    adjusted.layers.back().operation = paperweight::ThresholdOperation{0.2};
    const auto above = paperweight::evaluateMaterialSample(adjusted, 0.0, 0.0);
    expect(above.scalar == 1.0 && above.red == 1.0,
           "threshold selects the high endpoint at or above its cut-off");

    adjusted.layers.back().opacity = 0.0;
    adjusted.layers.back().operation = paperweight::SolidColourOperation{{255, 0, 0, 255}};
    adjusted.layers.pop_back();
    const auto beforeTransparentLayer = paperweight::evaluateMaterialSample(adjusted, 0.0, 0.0);
    adjusted.layers.push_back(paperweight::MaterialLayer{
        true,
        0.0,
        paperweight::CompositeMode::blend,
        paperweight::SolidColourOperation{{255, 0, 0, 255}}});
    expect(paperweight::evaluateMaterialSample(adjusted, 0.0, 0.0) == beforeTransparentLayer,
           "zero-opacity layers are exact no-ops");

    auto invalid = adjusted;
    invalid.layers.back().opacity = 1.1;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "invalid layer opacity is diagnosed");
    invalid = adjusted;
    invalid.layers.back().operation = paperweight::LevelsOperation{0.8, 0.2, 1.0};
    expect(paperweight::validateMaterial(invalid).has_value(),
           "invalid levels bounds are diagnosed");
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

    auto colouredRequest = request;
    colouredRequest.material.lowColour = {20, 40, 80, 64};
    colouredRequest.material.highColour = {240, 180, 100, 224};
    const auto colouredA = paperweight::generate(colouredRequest);
    const auto colouredB = paperweight::generate(colouredRequest);
    const auto* colouredImageA = std::get_if<paperweight::Image>(&colouredA);
    const auto* colouredImageB = std::get_if<paperweight::Image>(&colouredB);
    expect(colouredImageA != nullptr && colouredImageB != nullptr &&
               std::equal(
                   colouredImageA->pixels().begin(),
                   colouredImageA->pixels().end(),
                   colouredImageB->pixels().begin()),
           "custom two-colour generation is byte-deterministic");
    if (colouredImageA != nullptr) {
        const auto pixel = colouredImageA->pixels().front();
        expect(pixel.red >= 20 && pixel.red <= 240 && pixel.green >= 40 && pixel.green <= 180 &&
                   pixel.blue >= 80 && pixel.blue <= 100 && pixel.alpha >= 64 && pixel.alpha <= 224,
               "generated channels interpolate within the selected colour endpoints");
    }

    auto heightRequest = request;
    heightRequest.output = paperweight::MaterialOutput::height;
    const auto heightResult = paperweight::generate(heightRequest);
    const auto* heightImage = std::get_if<paperweight::Image>(&heightResult);
    expect(heightImage != nullptr && checksum(heightImage->pixels()) == checksum(first.pixels()),
           "default colour and height encode the same FBM samples");
    if (heightImage != nullptr) {
        expect(std::all_of(
                   heightImage->pixels().begin(),
                   heightImage->pixels().end(),
                   [](const auto& pixel) {
                       return pixel.red == pixel.green && pixel.green == pixel.blue &&
                           pixel.alpha == 255;
                   }),
               "height is encoded as opaque linear greyscale");
    }

    auto normalRequest = request;
    normalRequest.output = paperweight::MaterialOutput::normal;
    normalRequest.material.normalStrength = 0.0;
    const auto flatNormalResult = paperweight::generate(normalRequest);
    const auto* flatNormalImage = std::get_if<paperweight::Image>(&flatNormalResult);
    expect(flatNormalImage != nullptr &&
               std::all_of(
                   flatNormalImage->pixels().begin(),
                   flatNormalImage->pixels().end(),
                   [](const auto& pixel) {
                       return pixel == paperweight::Rgba8{128, 128, 255, 255};
                   }),
           "zero-strength height produces the neutral tangent-space normal");

    normalRequest.material.normalStrength = 1.0;
    const auto normalA = paperweight::generate(normalRequest);
    const auto normalB = paperweight::generate(normalRequest);
    const auto* normalImageA = std::get_if<paperweight::Image>(&normalA);
    const auto* normalImageB = std::get_if<paperweight::Image>(&normalB);
    expect(normalImageA != nullptr && normalImageB != nullptr &&
               checksum(normalImageA->pixels()) == checksum(normalImageB->pixels()),
           "normal generation is byte-deterministic");
    if (normalImageA != nullptr) {
        expect(std::all_of(
                   normalImageA->pixels().begin(),
                   normalImageA->pixels().end(),
                   [](const auto& pixel) { return pixel.blue >= 128 && pixel.alpha == 255; }),
               "encoded tangent-space normals face away from the surface");

        const double width = static_cast<double>(normalRequest.width);
        const double height = static_cast<double>(normalRequest.height);
        const double centreU = 0.5 / width;
        const double centreV = 0.5 / height;
        const double derivativeU =
            (paperweight::periodicFbm2D(1.5 / width, centreV, normalRequest.material) -
             paperweight::periodicFbm2D((width - 0.5) / width, centreV, normalRequest.material)) *
            width * 0.5;
        const double derivativeV =
            (paperweight::periodicFbm2D(centreU, 1.5 / height, normalRequest.material) -
             paperweight::periodicFbm2D(centreU, (height - 0.5) / height, normalRequest.material)) *
            height * 0.5;
        double x = -derivativeU * normalRequest.material.normalStrength;
        double y = -derivativeV * normalRequest.material.normalStrength;
        double z = 1.0;
        const double inverseLength = 1.0 / std::sqrt(x * x + y * y + z * z);
        x *= inverseLength;
        y *= inverseLength;
        z *= inverseLength;
        const auto encodeSigned = [](double value) {
            return static_cast<std::uint8_t>(std::round((value * 0.5 + 0.5) * 255.0));
        };
        expect(
            normalImageA->row(0)[0] == paperweight::Rgba8{
                                             encodeSigned(x),
                                             encodeSigned(y),
                                             encodeSigned(z),
                                             255},
            "normal-map boundary texels use wrapped height neighbours");
    }

    auto roughnessRequest = request;
    roughnessRequest.output = paperweight::MaterialOutput::roughness;
    const auto roughnessA = paperweight::generate(roughnessRequest);
    const auto roughnessB = paperweight::generate(roughnessRequest);
    const auto* roughnessImageA = std::get_if<paperweight::Image>(&roughnessA);
    const auto* roughnessImageB = std::get_if<paperweight::Image>(&roughnessB);
    expect(roughnessImageA != nullptr && roughnessImageB != nullptr &&
               checksum(roughnessImageA->pixels()) == checksum(roughnessImageB->pixels()),
           "roughness generation is byte-deterministic");
    if (roughnessImageA != nullptr) {
        expect(std::all_of(
                   roughnessImageA->pixels().begin(),
                   roughnessImageA->pixels().end(),
                   [](const auto& pixel) {
                       return pixel.red >= 64 && pixel.red <= 217 &&
                           pixel.red == pixel.green && pixel.green == pixel.blue &&
                           pixel.alpha == 255;
                   }),
               "roughness remains in its configured opaque greyscale range");
        const double source = paperweight::periodicFbm2D(
            0.5 / roughnessRequest.width,
            0.5 / roughnessRequest.height,
            roughnessRequest.material);
        const auto expectedRoughness = static_cast<std::uint8_t>(std::round(
            (roughnessRequest.material.roughnessLow +
             (roughnessRequest.material.roughnessHigh -
              roughnessRequest.material.roughnessLow) *
                 source) *
            255.0));
        expect(roughnessImageA->row(0)[0].red == expectedRoughness,
               "roughness maps the shared height sample between its endpoints");
    }

    auto layeredMaterial = paperweight::Material{};
    layeredMaterial.layers = {
        paperweight::makeNoiseLayer(),
        paperweight::MaterialLayer{
            true,
            1.0,
            paperweight::CompositeMode::blend,
            paperweight::LevelsOperation{0.15, 0.85, 1.2}},
        paperweight::MaterialLayer{
            true,
            0.3,
            paperweight::CompositeMode::multiply,
            paperweight::SolidColourOperation{{220, 120, 80, 255}}},
    };
    for (const auto output : std::array{
             paperweight::MaterialOutput::colour,
             paperweight::MaterialOutput::height,
             paperweight::MaterialOutput::normal,
             paperweight::MaterialOutput::roughness,
         }) {
        const paperweight::GenerationRequest layeredRequest{layeredMaterial, 31, 27, output};
        const auto layeredA = paperweight::generate(layeredRequest);
        const auto layeredB = paperweight::generate(layeredRequest);
        const auto* imageA = std::get_if<paperweight::Image>(&layeredA);
        const auto* imageB = std::get_if<paperweight::Image>(&layeredB);
        expect(imageA != nullptr && imageB != nullptr &&
                   checksum(imageA->pixels()) == checksum(imageB->pixels()),
               "every layered material output is byte-deterministic");
    }
    const auto layeredHeight = paperweight::generate(
        {layeredMaterial, 31, 27, paperweight::MaterialOutput::height});
    if (const auto* image = std::get_if<paperweight::Image>(&layeredHeight)) {
        const auto sample = paperweight::evaluateMaterialSample(
            layeredMaterial, 0.5 / 31.0, 0.5 / 27.0);
        const auto encoded = static_cast<std::uint8_t>(std::round(sample.scalar * 255.0));
        expect(image->row(0)[0] == paperweight::Rgba8{encoded, encoded, encoded, 255},
               "layered height output encodes the portable evaluator's scalar");
    }

    for (const auto [width, height] : std::array{
             std::pair<std::uint32_t, std::uint32_t>{1, 1},
             std::pair<std::uint32_t, std::uint32_t>{17, 29},
             std::pair<std::uint32_t, std::uint32_t>{65, 3},
         }) {
        const paperweight::GenerationRequest representative{paperweight::Material{}, width, height};
        const auto generatedA = paperweight::generate(representative);
        const auto generatedB = paperweight::generate(representative);
        const auto* imageA = std::get_if<paperweight::Image>(&generatedA);
        const auto* imageB = std::get_if<paperweight::Image>(&generatedB);
        expect(imageA != nullptr && imageB != nullptr &&
                   checksum(imageA->pixels()) == checksum(imageB->pixels()),
               "representative output dimensions remain byte-deterministic");
    }

    auto invalidRequest = request;
    invalidRequest.width = 0;
    const auto invalidResult = paperweight::generate(invalidRequest);
    expect(std::holds_alternative<paperweight::GenerationError>(invalidResult),
           "invalid dimensions return a structured error");
    invalidRequest = request;
    invalidRequest.output = static_cast<paperweight::MaterialOutput>(999);
    const auto invalidOutput = paperweight::generate(invalidRequest);
    expect(std::holds_alternative<paperweight::GenerationError>(invalidOutput) &&
               std::get<paperweight::GenerationError>(invalidOutput).code ==
                   paperweight::GenerationErrorCode::invalidOutput,
           "unknown material outputs return a structured error");
}

void testPmat()
{
    constexpr std::string_view canonical =
        "# Paperweight procedural material\n"
        "pmat.version = 2\n"
        "material.type = fbm\n"
        "material.seed = 18431\n"
        "colour.low = 0x000000FF\n"
        "colour.high = 0xFFFFFFFF\n"
        "noise.frequency = 4\n"
        "noise.octaves = 5\n"
        "noise.lacunarity = 2\n"
        "noise.gain = 0.5\n"
        "normal.strength = 1\n"
        "roughness.low = 0.25\n"
        "roughness.high = 0.85\n"
        "layers.count = 0\n";

    const auto parsed = paperweight::parsePmat(canonical);
    expect(std::holds_alternative<paperweight::Material>(parsed), "canonical .pmat parses");
    if (const auto* material = std::get_if<paperweight::Material>(&parsed)) {
        expect(*material == paperweight::Material{}, "canonical .pmat has default values");
        const auto serialised = paperweight::serialisePmat(*material);
        expect(std::holds_alternative<std::string>(serialised), "valid material serialises");
        if (const auto* text = std::get_if<std::string>(&serialised)) {
            expect(*text == canonical, "serialisation uses the canonical representation");
            const auto reparsed = paperweight::parsePmat(*text);
            expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                       std::get<paperweight::Material>(reparsed) == *material,
                   "parse-serialise-parse preserves the material");
        }
    }

    std::ifstream exampleFile("default.pmat", std::ios::binary);
    const std::string exampleText(
        std::istreambuf_iterator<char>{exampleFile},
        std::istreambuf_iterator<char>{});
    expect(exampleFile.good() || exampleFile.eof(), "canonical example file is readable");
    const auto example = paperweight::parsePmat(exampleText);
    expect(std::holds_alternative<paperweight::Material>(example) &&
               std::get<paperweight::Material>(example).layers.size() == 1 &&
               std::holds_alternative<paperweight::NoiseOperation>(
                   std::get<paperweight::Material>(example).layers.front().operation),
           "checked-in canonical example contains an explicit base-noise layer");
    if (const auto* material = std::get_if<paperweight::Material>(&example)) {
        const auto generated = paperweight::generate({*material, 48, 32});
        const auto* image = std::get_if<paperweight::Image>(&generated);
        expect(image != nullptr && checksum(image->pixels()) == 4981563472745378647ULL,
               "checked-in example retains the default golden image checksum");
    }

    std::ifstream emberFile("ember.pmat", std::ios::binary);
    const std::string emberText(
        std::istreambuf_iterator<char>{emberFile},
        std::istreambuf_iterator<char>{});
    const auto ember = paperweight::parsePmat(emberText);
    const auto* emberMaterial = std::get_if<paperweight::Material>(&ember);
    expect(emberMaterial != nullptr && emberMaterial->seed == 918273 &&
               emberMaterial->lowColour == paperweight::Rgba8{22, 12, 40, 255} &&
               emberMaterial->highColour == paperweight::Rgba8{255, 179, 71, 255},
           "checked-in coloured example parses with its expected endpoints");
    if (emberMaterial != nullptr) {
        const auto firstEmber = paperweight::generate({*emberMaterial, 37, 29});
        const auto secondEmber = paperweight::generate({*emberMaterial, 37, 29});
        const auto* firstImage = std::get_if<paperweight::Image>(&firstEmber);
        const auto* secondImage = std::get_if<paperweight::Image>(&secondEmber);
        expect(firstImage != nullptr && secondImage != nullptr &&
                   std::equal(
                       firstImage->pixels().begin(),
                       firstImage->pixels().end(),
                       secondImage->pixels().begin()),
               "checked-in coloured example generates deterministically");
    }

    auto layeredRoundTrip = paperweight::Material{};
    layeredRoundTrip.layers = {
        paperweight::makeNoiseLayer(7),
        paperweight::MaterialLayer{
            false,
            0.35,
            paperweight::CompositeMode::add,
            paperweight::SolidColourOperation{{12, 34, 56, 78}}},
        paperweight::MaterialLayer{
            true,
            0.8,
            paperweight::CompositeMode::multiply,
            paperweight::LevelsOperation{0.15, 0.9, 1.25}},
        paperweight::MaterialLayer{
            true,
            0.6,
            paperweight::CompositeMode::blend,
            paperweight::ThresholdOperation{0.42}},
    };
    const std::array roundTripMaterials{
        materialWithNoiseParameters(0, 1, 1, 1, 0.1),
        materialWithNoiseParameters(927364821, 13, 4, 2, 0.37),
        materialWithNoiseParameters(
            std::numeric_limits<std::uint64_t>::max(), 1, 7, 4, 0.9),
        paperweight::Material{
            42,
            8,
            3,
            2,
            0.625,
            {1, 2, 3, 4},
            {250, 240, 230, 220},
            4.5,
            0.15,
            0.95,
            {}},
        layeredRoundTrip,
    };
    for (const auto& candidate : roundTripMaterials) {
        const auto text = paperweight::serialisePmat(candidate);
        expect(std::holds_alternative<std::string>(text),
               "boundary and custom materials serialise");
        if (const auto* serialised = std::get_if<std::string>(&text)) {
            const auto roundTrip = paperweight::parsePmat(*serialised);
            expect(std::holds_alternative<paperweight::Material>(roundTrip) &&
                       std::get<paperweight::Material>(roundTrip) == candidate,
                   "boundary and custom materials round-trip exactly");
            if (const auto* reparsed = std::get_if<paperweight::Material>(&roundTrip)) {
                const auto directImage = paperweight::generate({candidate, 23, 17});
                const auto fileImage = paperweight::generate({*reparsed, 23, 17});
                const auto* directPixels = std::get_if<paperweight::Image>(&directImage);
                const auto* filePixels = std::get_if<paperweight::Image>(&fileImage);
                expect(directPixels != nullptr && filePixels != nullptr &&
                           std::equal(
                               directPixels->pixels().begin(),
                               directPixels->pixels().end(),
                               filePixels->pixels().begin()),
                       "saved definition and standalone material produce identical pixels");
            }
        }
    }

    constexpr std::string_view flexible =
        "  # Comments and CRLF are accepted\r\n"
        "noise.gain=0.75 # trailing comment\r\n"
        "noise.lacunarity = 3\r\n"
        "noise.octaves = 4\r\n"
        "noise.frequency = 7\r\n"
        "material.seed = 99\r\n"
        "material.type = fbm\r\n"
        "pmat.version = 1\r\n";
    const auto flexibleResult = paperweight::parsePmat(flexible);
    expect(std::holds_alternative<paperweight::Material>(flexibleResult),
           "comments, whitespace, CRLF, and key order are flexible");
    if (const auto* material = std::get_if<paperweight::Material>(&flexibleResult)) {
        expect(material->seed == 99 && material->frequency == 7 && material->octaves == 4 &&
                   material->lacunarity == 3 && material->gain == 0.75,
               "flexible input produces expected values");
        expect(material->lowColour == paperweight::Rgba8{0, 0, 0, 255} &&
                   material->highColour == paperweight::Rgba8{255, 255, 255, 255},
               "preview-era files without colour fields retain black-to-white defaults");
        expect(material->normalStrength == 1.0 && material->roughnessLow == 0.25 &&
                   material->roughnessHigh == 0.85,
               "v0.0.1 files retain the v0.0.2 output defaults");
    }

    const auto expectDiagnostic = [](std::string_view text, std::size_t line, std::string_view phrase) {
        const auto result = paperweight::parsePmat(text);
        expect(std::holds_alternative<paperweight::ParseDiagnostic>(result),
               "invalid .pmat produces a diagnostic");
        if (const auto* error = std::get_if<paperweight::ParseDiagnostic>(&result)) {
            const bool matches = error->line == line && error->column > 0 &&
                error->message.find(phrase) != std::string::npos;
            if (!matches) {
                std::cerr << "Expected diagnostic line " << line << " containing '" << phrase
                          << "', got line " << error->line << ": " << error->message << '\n';
            }
            expect(matches, "diagnostic contains source position and reason");
        }
    };

    expectDiagnostic("pmat.version = 3\n", 1, "unsupported");
    expectDiagnostic("unknown.key = 1\n", 1, "unknown key");
    expectDiagnostic("pmat.version = 1\npmat.version = 1\n", 2, "duplicate");
    expectDiagnostic("pmat.version = nope\n", 1, "integer");
    expectDiagnostic("pmat.version = 1\n", 3, "missing required key");
    expectDiagnostic("pmat.version = 1 = 2\n", 1, "exactly one");
    expectDiagnostic(
        "pmat.version = 1\nmaterial.type = fbm\nmaterial.seed = 0\n"
        "colour.low = blue\nnoise.frequency = 1\nnoise.octaves = 1\n"
        "noise.lacunarity = 1\nnoise.gain = 0.5\n",
        4,
        "0xRRGGBBAA");
    expectDiagnostic(
        "pmat.version = 1\nmaterial.type = fbm\nmaterial.seed = 0\n"
        "noise.frequency = 65\nnoise.octaves = 1\nnoise.lacunarity = 1\nnoise.gain = 0.5\n",
        4,
        "frequency");
    expectDiagnostic(
        std::string(canonical) + "layer.0.enabled = true\n",
        17,
        "exceeds layers.count");

    auto invalidLevelsMaterial = paperweight::Material{};
    invalidLevelsMaterial.layers = {paperweight::makeLevelsLayer()};
    auto invalidLevelsText = std::get<std::string>(
        paperweight::serialisePmat(invalidLevelsMaterial));
    const auto highPosition = invalidLevelsText.find("layer.0.levels.input_high = 1");
    expect(highPosition != std::string::npos, "levels fixture contains its high input");
    if (highPosition != std::string::npos) {
        invalidLevelsText.replace(
            highPosition,
            std::string("layer.0.levels.input_high = 1").size(),
            "layer.0.levels.input_high = 0");
        expectDiagnostic(invalidLevelsText, 20, "greater than input low");
    }

    auto invalidThresholdMaterial = paperweight::Material{};
    invalidThresholdMaterial.layers = {paperweight::makeThresholdLayer()};
    auto invalidThresholdText = std::get<std::string>(
        paperweight::serialisePmat(invalidThresholdMaterial));
    const auto thresholdPosition = invalidThresholdText.find("layer.0.threshold.value = 0.5");
    expect(thresholdPosition != std::string::npos, "threshold fixture contains its value");
    if (thresholdPosition != std::string::npos) {
        invalidThresholdText.replace(
            thresholdPosition,
            std::string("layer.0.threshold.value = 0.5").size(),
            "layer.0.threshold.value = 1.5");
        expectDiagnostic(invalidThresholdText, 19, "between 0 and 1");
    }

    auto invalid = paperweight::Material{};
    invalid.gain = 2.0;
    expect(std::holds_alternative<paperweight::SerialisationError>(
               paperweight::serialisePmat(invalid)),
           "invalid materials are not serialised");
}

} // namespace

int main()
{
    testVersion();
    testImage();
    testHashing();
    testPeriodicNoise();
    testMaterialAndFbm();
    testLayerEvaluation();
    testGenerator();
    testPmat();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All Paperweight tests passed\n";
    return 0;
}
