#include <paperweight/generator.hpp>
#include <paperweight/hash.hpp>
#include <paperweight/image.hpp>
#include <paperweight/evaluation.hpp>
#include <paperweight/graph.hpp>
#include <paperweight/layer.hpp>
#include <paperweight/material.hpp>
#include <paperweight/material_library.hpp>
#include <paperweight/material_template.hpp>
#include <paperweight/noise.hpp>
#include <paperweight/organic.hpp>
#include <paperweight/pmat.hpp>
#include <paperweight/region.hpp>
#include <paperweight/scatter.hpp>
#include <paperweight/sculpt.hpp>
#include <paperweight/shape.hpp>
#include <paperweight/structural.hpp>
#include <paperweight/surface.hpp>
#include <paperweight/stylised_lighting.hpp>
#include <paperweight/version.hpp>

#include <algorithm>
#include <array>
#include <atomic>
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
#include <type_traits>
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

void expectGraphError(
    const paperweight::MaterialGraph& graph,
    paperweight::GraphErrorCode code,
    std::string_view description)
{
    const auto error = paperweight::validateMaterialGraph(graph);
    expect(error && error->code == code, description);
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

void expectChecksum(
    const paperweight::Image* image,
    std::uint64_t expected,
    std::string_view description)
{
    if (image == nullptr) {
        expect(false, description);
        return;
    }
    const auto actual = checksum(image->pixels());
    if (actual != expected) {
        std::cerr << "Expected checksum " << expected << ", got " << actual << '\n';
    }
    expect(actual == expected, description);
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
    constexpr paperweight::Version expected{0, 0, 18};
    static_assert(paperweight::currentVersion == expected);
    expect(paperweight::versionString() == "0.0.19", "version string is 0.0.19");
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

    const paperweight::EvaluatedSample background{0.25, 0.2, 0.4, 0.6, 0.8, {}};
    const paperweight::EvaluatedSample source{0.8, 0.9, 0.5, 0.25, 0.4, {}};
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
            paperweight::SolidColourOperation{{255, 255, 255, 255}},
            {},
            {}},
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
            paperweight::SolidColourOperation{{0, 0, 255, 255}},
            {},
            {}},
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
            paperweight::LevelsOperation{0.0, 1.0, 2.0},
            {},
            {}},
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
        paperweight::SolidColourOperation{{255, 0, 0, 255}},
        {},
        {}});
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

void testMasksAndWarping()
{
    paperweight::Material material;
    material.layers = {paperweight::makeNoiseLayer()};
    const auto identity = paperweight::evaluateMaterialSample(material, 0.23, 0.71);
    expect(identity == paperweight::evaluateMaterialSample(paperweight::Material{}, 0.23, 0.71),
           "the default v0.0.4 transform and mask preserve legacy evaluation exactly");

    paperweight::CoordinateTransform transform;
    transform.scaleX = 2;
    transform.scaleY = 3;
    transform.offsetX = 0.1;
    transform.offsetY = -0.2;
    transform.rotation = paperweight::QuarterTurn::clockwise90;
    const auto coordinates = paperweight::transformCoordinates(
        transform, paperweight::EvaluationContext{material, 0.25, 0.75});
    expectNear(coordinates.u, -1.4, 1.0e-12,
               "coordinate rotation, X scale, and X offset compose predictably");
    expectNear(coordinates.v, 0.55, 1.0e-12,
               "coordinate rotation, Y scale, and Y offset compose predictably");

    for (const auto rotation : std::array{
             paperweight::QuarterTurn::none,
             paperweight::QuarterTurn::clockwise90,
             paperweight::QuarterTurn::clockwise180,
             paperweight::QuarterTurn::clockwise270,
         }) {
        material.layers.front().transform = transform;
        material.layers.front().transform.rotation = rotation;
        const auto sample = paperweight::evaluateMaterialSample(material, -0.37, 0.58);
        expectNear(
            sample.scalar,
            paperweight::evaluateMaterialSample(material, 0.63, 0.58).scalar,
            1.0e-12,
            "scaled quarter-turn evaluation repeats on x");
        expectNear(
            sample.scalar,
            paperweight::evaluateMaterialSample(material, -0.37, 1.58).scalar,
            1.0e-12,
            "scaled quarter-turn evaluation repeats on y");
    }

    auto offsetMaterial = paperweight::Material{};
    offsetMaterial.layers = {paperweight::makeNoiseLayer()};
    const auto unshifted = paperweight::evaluateMaterialSample(offsetMaterial, 0.31, 0.47);
    offsetMaterial.layers.front().transform.offsetX = 0.125;
    offsetMaterial.layers.front().transform.offsetY = -0.375;
    const auto shifted = paperweight::evaluateMaterialSample(offsetMaterial, 0.31, 0.47);
    expect(shifted != unshifted, "continuous coordinate offsets change the sampled material");
    expectNear(
        shifted.scalar,
        paperweight::evaluateMaterialSample(offsetMaterial, 1.31, 0.47).scalar,
        1.0e-12,
        "offset evaluation remains periodic");

    auto warped = offsetMaterial;
    warped.layers.front().transform.warpEnabled = true;
    warped.layers.front().transform.warpStrength = 0.3;
    warped.layers.front().transform.warpFrequency = 3;
    warped.layers.front().transform.warpSeedOffset = 91;
    const auto warpedSample = paperweight::evaluateMaterialSample(warped, 0.31, 0.47);
    expect(warpedSample != shifted, "enabled non-zero warp distorts the sampled material");
    expect(warpedSample == paperweight::evaluateMaterialSample(warped, 0.31, 0.47),
           "coordinate warp is deterministic");
    expectNear(
        warpedSample.scalar,
        paperweight::evaluateMaterialSample(warped, 1.31, 0.47).scalar,
        1.0e-12,
        "warped evaluation repeats on x");
    expectNear(
        warpedSample.scalar,
        paperweight::evaluateMaterialSample(warped, 0.31, 1.47).scalar,
        1.0e-12,
        "warped evaluation repeats on y");
    auto zeroWarp = warped;
    zeroWarp.layers.front().transform.warpStrength = 0.0;
    expect(paperweight::evaluateMaterialSample(zeroWarp, 0.31, 0.47) == shifted,
           "zero-strength warp is an exact no-op");
    auto disabledWarp = warped;
    disabledWarp.layers.front().transform.warpEnabled = false;
    expect(paperweight::evaluateMaterialSample(disabledWarp, 0.31, 0.47) == shifted,
           "disabled warp preserves its parameters without affecting output");
    auto alternateWarp = warped;
    alternateWarp.layers.front().transform.warpSeedOffset += 1;
    expect(paperweight::evaluateMaterialSample(alternateWarp, 0.31, 0.47) != warpedSample,
           "warp seed offsets select distinct deterministic distortion fields");

    paperweight::Material masked;
    masked.layers = {paperweight::makeSolidColourLayer({255, 255, 255, 255})};
    const paperweight::EvaluationContext maskContext{masked, 0.17, 0.83};
    expect(paperweight::evaluateLayerMask(masked.layers.front().mask, maskContext) == 1.0,
           "a disabled layer mask is an exact no-op");
    masked.layers.front().mask.enabled = true;
    masked.layers.front().mask.seedOffset = 123;
    masked.layers.front().mask.inputLow = 0.2;
    masked.layers.front().mask.inputHigh = 0.8;
    const double maskValue = paperweight::evaluateLayerMask(
        masked.layers.front().mask, maskContext);
    expect(maskValue >= 0.0 && maskValue <= 1.0, "mask remapping remains normalised");
    const auto maskedSample = paperweight::evaluateMaterialSample(masked, 0.17, 0.83);
    expectNear(maskedSample.scalar, maskValue, 1.0e-12,
               "a mask modulates the layer's scalar and colour through effective opacity");
    auto invertedMask = masked.layers.front().mask;
    invertedMask.inverted = true;
    expectNear(
        paperweight::evaluateLayerMask(invertedMask, maskContext),
        1.0 - maskValue,
        1.0e-12,
        "mask inversion complements the remapped value");
    expectNear(
        maskValue,
        paperweight::evaluateLayerMask(
            masked.layers.front().mask,
            paperweight::EvaluationContext{masked, 1.17, 0.83}),
        1.0e-12,
        "mask evaluation repeats on x");

    auto invalid = material;
    invalid.layers.front().transform.scaleX = 0;
    expect(paperweight::validateMaterial(invalid).has_value(), "zero transform scale is diagnosed");
    invalid = material;
    invalid.layers.front().transform.rotation = static_cast<paperweight::QuarterTurn>(99);
    expect(paperweight::validateMaterial(invalid).has_value(), "unknown rotation is diagnosed");
    invalid = material;
    invalid.layers.front().transform.offsetY = std::numeric_limits<double>::infinity();
    expect(paperweight::validateMaterial(invalid).has_value(), "non-finite offset is diagnosed");
    invalid = material;
    invalid.layers.front().transform.warpStrength = 1.1;
    expect(paperweight::validateMaterial(invalid).has_value(), "invalid warp strength is diagnosed");
    invalid = material;
    invalid.layers.front().transform.warpFrequency = 0;
    expect(paperweight::validateMaterial(invalid).has_value(), "invalid warp frequency is diagnosed");
    invalid = material;
    invalid.layers.front().mask.inputLow = 0.9;
    invalid.layers.front().mask.inputHigh = 0.1;
    expect(paperweight::validateMaterial(invalid).has_value(), "invalid mask range is diagnosed");
}

void testStructuralGenerators()
{
    expectNear(paperweight::wrapUnit(-0.25), 0.75, 1.0e-12,
               "structural coordinates wrap negative values into one tile");
    const auto repeated = paperweight::repeatedCoordinate(-0.01, 8);
    expect(repeated.index == 7 && repeated.local > 0.4,
           "repeated-cell addressing wraps indices and preserves local position");
    expect(paperweight::smoothCoverage(-1.0, 0.0) == 0.0 &&
               paperweight::smoothCoverage(1.0, 0.0) == 1.0,
           "zero-softness structural edges remain well-defined");

    const paperweight::BrickGridOperation brick;
    expect(paperweight::evaluateBrickGrid(
               brick, 0.5 / brick.columns, 0.5 / brick.rows) == 1.0,
           "brick cell centres are solid");
    expect(paperweight::evaluateBrickGrid(brick, 0.0, 0.0) == 0.0,
           "brick cell boundaries form mortar");
    auto unstaggeredBrick = brick;
    unstaggeredBrick.stagger = 0.0;
    expect(paperweight::evaluateBrickGrid(
               brick, 0.5 / brick.columns, 1.5 / brick.rows) !=
               paperweight::evaluateBrickGrid(
                   unstaggeredBrick, 0.5 / brick.columns, 1.5 / brick.rows),
           "brick staggering shifts alternating rows");

    const paperweight::BrickGridOperation unequalLegacyMortar{
        4, 8, 0.08, 0.0, 0.0, paperweight::BrickMortarSpace::cell, std::nullopt};
    expect(paperweight::evaluateBrickGrid(
               unequalLegacyMortar, 0.007, 0.5 / unequalLegacyMortar.rows) == 0.0 &&
               paperweight::evaluateBrickGrid(
                   unequalLegacyMortar, 0.5 / unequalLegacyMortar.columns, 0.007) == 1.0,
           "legacy brick mortar remains relative to each cell dimension");

    paperweight::BrickGridOperation equalMortar{
        4,
        8,
        0.02,
        0.0,
        0.0,
        paperweight::BrickMortarSpace::texture,
        std::nullopt,
    };
    const auto hasEqualMortarWidth = [](const paperweight::BrickGridOperation& operation) {
        return paperweight::evaluateBrickGrid(
                   operation, 0.009, 0.5 / operation.rows) == 0.0 &&
            paperweight::evaluateBrickGrid(
                operation, 0.011, 0.5 / operation.rows) == 1.0 &&
            paperweight::evaluateBrickGrid(
                operation, 0.5 / operation.columns, 0.009) == 0.0 &&
            paperweight::evaluateBrickGrid(
                operation, 0.5 / operation.columns, 0.011) == 1.0;
    };
    expect(hasEqualMortarWidth(equalMortar),
           "texture-space brick mortar has equal horizontal and vertical width");
    equalMortar.columns = 10;
    equalMortar.rows = 5;
    expect(hasEqualMortarWidth(equalMortar),
           "texture-space mortar width is independent of the column-to-row ratio");
    expectNear(
        paperweight::evaluateBrickGrid(equalMortar, -0.137, 0.421),
        paperweight::evaluateBrickGrid(equalMortar, 0.863, 1.421),
        1.0e-12,
        "texture-space brick mortar remains periodic on both axes");

    const paperweight::TileGridOperation tile;
    expect(paperweight::evaluateTileGrid(
               tile, 0.5 / tile.columns, 0.5 / tile.rows) == 1.0,
           "tile cell centres are solid");
    expect(paperweight::evaluateTileGrid(tile, 0.0, 0.0) == 0.0,
           "tile cell boundaries form grout");

    const paperweight::WorleyCellsOperation worley;
    const double worleyValue = paperweight::evaluateWorleyCells(
        worley, 0.23, 0.67, 9981);
    expect(worleyValue >= 0.0 && worleyValue <= 1.0,
           "Worley cell interiors and boundaries remain normalised");
    expect(worleyValue == paperweight::evaluateWorleyCells(
               worley, 0.23, 0.67, 9981),
           "Worley feature placement is deterministic");
    auto alternateWorley = worley;
    alternateWorley.seedOffset = 1;
    expect(worleyValue != paperweight::evaluateWorleyCells(
               alternateWorley, 0.23, 0.67, 9981),
           "Worley seed offsets select distinct feature fields");

    const paperweight::RandomCellsOperation randomCells;
    const double randomValue = paperweight::evaluateRandomCells(
        randomCells, 0.01, 0.01, 771);
    expect(randomValue == paperweight::evaluateRandomCells(
               randomCells, 0.1 / randomCells.columns, 0.1 / randomCells.rows, 771),
           "random-cell values remain constant within a cell");
    expect(randomValue != paperweight::evaluateRandomCells(
               randomCells, 1.1 / randomCells.columns, 0.1 / randomCells.rows, 771),
           "random cells receive independent deterministic values");

    const paperweight::LinesOperation lines;
    expect(paperweight::evaluateLines(lines, 0.5 / lines.count, 0.37) == 1.0 &&
               paperweight::evaluateLines(lines, 0.0, 0.37) == 0.0,
           "line generators repeat solid strokes with empty spacing");
    auto horizontalLines = lines;
    horizontalLines.direction = paperweight::LineDirection::horizontal;
    expect(paperweight::evaluateLines(horizontalLines, 0.37, 0.5 / lines.count) == 1.0,
           "line direction selects the evaluated axis");

    const paperweight::RectanglesOperation rectangles;
    expect(paperweight::evaluateRectangles(
               rectangles, 0.5 / rectangles.columns, 0.5 / rectangles.rows) == 1.0 &&
               paperweight::evaluateRectangles(rectangles, 0.0, 0.0) == 0.0,
           "rectangle generators distinguish repeated interiors and exteriors");

    const paperweight::CirclesOperation circles;
    expect(paperweight::evaluateCircles(
               circles, 0.5 / circles.columns, 0.5 / circles.rows) == 1.0 &&
               paperweight::evaluateCircles(circles, 0.0, 0.0) == 0.0,
           "circle generators distinguish repeated interiors and exteriors");

    std::array structuralLayers{
        paperweight::makeBrickGridLayer(),
        paperweight::makeTileGridLayer(),
        paperweight::makeWorleyCellsLayer(),
        paperweight::makeRandomCellsLayer(),
        paperweight::makeLinesLayer(),
        paperweight::makeRectanglesLayer(),
        paperweight::makeCirclesLayer(),
    };
    for (auto layer : structuralLayers) {
        paperweight::Material material;
        material.layers = {layer};
        const auto sample = paperweight::evaluateMaterialSample(material, -0.37, 0.58);
        expectNear(
            sample.scalar,
            paperweight::evaluateMaterialSample(material, 0.63, 0.58).scalar,
            1.0e-12,
            "every structural operation repeats on x");
        expectNear(
            sample.scalar,
            paperweight::evaluateMaterialSample(material, -0.37, 1.58).scalar,
            1.0e-12,
            "every structural operation repeats on y");
        expect(sample.scalar >= 0.0 && sample.scalar <= 1.0 &&
                   sample.red == sample.scalar && sample.green == sample.scalar &&
                   sample.blue == sample.scalar,
               "structural operations emit paired normalised scalar and colour samples");
    }

    auto composed = paperweight::Material{};
    composed.layers = {paperweight::makeWorleyCellsLayer()};
    composed.layers.front().transform.scaleX = 2;
    composed.layers.front().transform.rotation = paperweight::QuarterTurn::clockwise90;
    composed.layers.front().transform.warpEnabled = true;
    composed.layers.front().transform.warpStrength = 0.15;
    composed.layers.front().mask.enabled = true;
    const auto composedSample = paperweight::evaluateMaterialSample(composed, 0.19, 0.73);
    expectNear(
        composedSample.scalar,
        paperweight::evaluateMaterialSample(composed, 1.19, 0.73).scalar,
        1.0e-12,
        "structural generators compose with transforms, warp, and masks seamlessly");

    auto invalid = paperweight::Material{};
    invalid.layers = {paperweight::makeBrickGridLayer()};
    std::get<paperweight::BrickGridOperation>(invalid.layers.front().operation).columns = 0;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "invalid structural repeat counts are diagnosed");
    invalid.layers = {paperweight::makeWorleyCellsLayer()};
    std::get<paperweight::WorleyCellsOperation>(invalid.layers.front().operation).jitter = 1.1;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "invalid Worley jitter is diagnosed");
    invalid.layers = {paperweight::makeBrickGridLayer()};
    std::get<paperweight::BrickGridOperation>(invalid.layers.front().operation).mortarSpace =
        static_cast<paperweight::BrickMortarSpace>(99);
    expect(paperweight::validateMaterial(invalid).has_value(),
           "unknown brick mortar spaces are diagnosed");
    invalid.layers = {paperweight::makeLinesLayer()};
    std::get<paperweight::LinesOperation>(invalid.layers.front().operation).direction =
        static_cast<paperweight::LineDirection>(99);
    expect(paperweight::validateMaterial(invalid).has_value(),
           "unknown line directions are diagnosed");
    invalid.layers = {paperweight::makeCirclesLayer()};
    std::get<paperweight::CirclesOperation>(invalid.layers.front().operation).radius = 0.6;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "invalid circle radii are diagnosed");
}

void testRegionAttributes()
{
    const auto key = paperweight::makeRegionKey(
        0x74696c6567726964ULL,
        2,
        3);
    expect(key == 0x9b7350fc67ba2645ULL,
           "region keys match their exact 64-bit golden vector");
    expectNear(
        paperweight::regionRandom(18431, key, 17, 0),
        0.9114368622244505,
        0.0,
        "region random channel zero matches its golden vector");
    expectNear(
        paperweight::regionRandom(18431, key, 17, 1),
        0.15465807587006541,
        0.0,
        "independent region random channels match their golden vectors");

    const paperweight::TileGridOperation tile{5, 4, 0.1, 0.02};
    const auto tileSample = paperweight::evaluateTileGridSample(tile, 0.37, 0.61);
    const auto tileRepeat = paperweight::evaluateTileGridSample(tile, 1.37, -0.39);
    expect(tileSample.region.valid && tileSample.region.key == tileRepeat.region.key,
           "tile region identity repeats exactly across both seams");
    expectNear(tileSample.region.localU, tileRepeat.region.localU, 1.0e-12,
               "region-local U is independent of the sampled tile repeat");
    expectNear(tileSample.region.localV, tileRepeat.region.localV, 1.0e-12,
               "region-local V is independent of the sampled tile repeat");
    expect(tileSample.region.centreDistance >= 0.0 &&
               tileSample.region.centreDistance <= 1.0 &&
               tileSample.region.boundaryDistance >= 0.0 &&
               tileSample.region.boundaryDistance <= 1.0,
           "region distance fields stay normalised");
    const auto neighbouringTile = paperweight::evaluateTileGridSample(tile, 0.57, 0.61);
    expect(tileSample.region.key != neighbouringTile.region.key,
           "neighbouring structural cells receive different integer keys");

    paperweight::WorleyCellsOperation worley;
    worley.columns = 7;
    worley.rows = 6;
    worley.jitter = 0.84;
    worley.seedOffset = 91;
    const auto worleySample = paperweight::evaluateWorleyCellsSample(
        worley, -0.17, 0.43, 18431);
    const auto worleyRepeat = paperweight::evaluateWorleyCellsSample(
        worley, 0.83, 1.43, 18431);
    expect(worleySample.region.key == worleyRepeat.region.key,
           "Worley ownership remains stable for cells crossing tile seams");
    expectNear(worleySample.region.boundaryDistance,
               worleyRepeat.region.boundaryDistance,
               1.0e-12,
               "Worley boundary distance remains periodic");

    paperweight::Material material;
    material.seed = 18431;
    const paperweight::EvaluationContext context{material, 0.2, 0.3};
    paperweight::EvaluatedSample input{0.72, 0.15, 0.35, 0.55, 1.0, {}};
    input.region = tileSample.region;
    const paperweight::RegionFieldOperation colourField{
        paperweight::RegionFieldKind::random,
        17,
        1,
        0.2,
        0.8,
        false,
        paperweight::ProcessingTarget::colour,
    };
    const auto colourResult = paperweight::evaluateOperation(
        colourField, context, input);
    expectNear(colourResult.scalar, input.scalar, 0.0,
               "colour-targeted region fields preserve scalar structure");
    expect(colourResult.region.key == input.region.key &&
               colourResult.red == colourResult.green &&
               colourResult.green == colourResult.blue,
           "region processing preserves exact identity while producing a colour field");

    auto localField = colourField;
    localField.field = paperweight::RegionFieldKind::localU;
    localField.outputLow = 0.0;
    localField.outputHigh = 1.0;
    localField.target = paperweight::ProcessingTarget::scalar;
    const auto localResult = paperweight::evaluateOperation(localField, context, input);
    expectNear(localResult.scalar, input.region.localU, 0.0,
               "region-local coordinates are selectable as scalar graph values");
    expectNear(localResult.red, input.red, 0.0,
               "scalar-targeted region fields preserve authored colour");

    auto source = input;
    source.region = neighbouringTile.region;
    const auto hiddenSource = paperweight::compositeSamples(
        input, source, paperweight::CompositeMode::blend, 0.0);
    const auto visibleSource = paperweight::compositeSamples(
        input, source, paperweight::CompositeMode::blend, 0.25);
    expect(hiddenSource.region.key == input.region.key &&
               visibleSource.region.key == source.region.key,
           "composites change active region only for a visible structural source");

    paperweight::EvaluatedSample noRegion{0.5, 0.5, 0.5, 0.5, 1.0, {}};
    auto fallback = localField;
    fallback.outputLow = 0.25;
    fallback.outputHigh = 0.75;
    expectNear(
        paperweight::evaluateOperation(fallback, context, noRegion).scalar,
        0.25,
        0.0,
        "a missing active region uses the documented field-zero fallback");

    paperweight::Material layered;
    layered.seed = 18431;
    layered.layers = {
        paperweight::makeWorleyCellsLayer(),
        paperweight::makeRegionFieldLayer(),
    };
    auto& variation = std::get<paperweight::RegionFieldOperation>(
        layered.layers.back().operation);
    variation.seedOffset = 901;
    variation.channel = 3;
    variation.outputLow = 0.55;
    variation.outputHigh = 1.0;
    layered.layers.back().compositeMode = paperweight::CompositeMode::multiply;
    const auto compiled = paperweight::compileMaterialGraph(layered);
    const auto* graph = std::get_if<paperweight::MaterialGraph>(&compiled);
    bool foundRegionProcessor = false;
    if (graph != nullptr) {
        for (const auto& node : graph->nodes) {
            if (const auto* processing = std::get_if<paperweight::ProcessingNode>(&node)) {
                foundRegionProcessor = foundRegionProcessor ||
                    std::holds_alternative<paperweight::RegionFieldProcessing>(
                        processing->operation);
            }
        }
    }
    expect(graph != nullptr && foundRegionProcessor,
           "region-field layers compile into reusable processing nodes");

    paperweight::MaterialGraph maskGraph;
    maskGraph.nodes = {
        paperweight::GeneratorNode{1, std::nullopt, {}, worley},
        paperweight::ProcessingNode{2, std::nullopt,
            paperweight::RegionFieldProcessing{1, localField}},
        paperweight::GeneratorNode{3, std::nullopt, {},
            paperweight::SolidColourOperation{{255, 255, 255, 255}}},
        paperweight::ProcessingNode{4, std::nullopt,
            paperweight::CompositeProcessing{
                3, 1, paperweight::GraphNodeId{2},
                paperweight::CompositeMode::blend, 1.0}},
        paperweight::OutputNode{5, paperweight::MaterialOutput::colour, 4},
        paperweight::OutputNode{6, paperweight::MaterialOutput::height, 2},
        paperweight::OutputNode{7, paperweight::MaterialOutput::normal, 2},
        paperweight::OutputNode{8, paperweight::MaterialOutput::roughness, 2},
    };
    expect(!paperweight::validateMaterialGraph(maskGraph).has_value(),
           "value-producing region processors are accepted as graph masks");

    auto channelZero = colourField;
    channelZero.target = paperweight::ProcessingTarget::colourAndScalar;
    channelZero.channel = 0;
    auto channelOne = channelZero;
    channelOne.channel = 1;
    auto channelTwo = channelZero;
    channelTwo.channel = 2;
    paperweight::MaterialGraph routedGraph;
    routedGraph.nodes = {
        paperweight::GeneratorNode{1, std::nullopt, {}, tile},
        paperweight::ProcessingNode{2, std::nullopt,
            paperweight::RegionFieldProcessing{1, channelZero}},
        paperweight::ProcessingNode{3, std::nullopt,
            paperweight::RegionFieldProcessing{1, channelOne}},
        paperweight::ProcessingNode{4, std::nullopt,
            paperweight::RegionFieldProcessing{1, channelTwo}},
        paperweight::OutputNode{5, paperweight::MaterialOutput::colour, 2},
        paperweight::OutputNode{6, paperweight::MaterialOutput::height, 3},
        paperweight::OutputNode{7, paperweight::MaterialOutput::normal, 3},
        paperweight::OutputNode{8, paperweight::MaterialOutput::roughness, 4},
    };
    expect(!paperweight::validateMaterialGraph(routedGraph).has_value(),
           "independent region channels can route to colour, surface, and roughness outputs");
    const auto routedColour = paperweight::evaluateMaterialGraphSample(
        material, routedGraph, paperweight::MaterialOutput::colour, 0.37, 0.61);
    const auto routedHeight = paperweight::evaluateMaterialGraphSample(
        material, routedGraph, paperweight::MaterialOutput::height, 0.37, 0.61);
    const auto routedRoughness = paperweight::evaluateMaterialGraphSample(
        material, routedGraph, paperweight::MaterialOutput::roughness, 0.37, 0.61);
    expect(routedColour.scalar != routedHeight.scalar &&
               routedHeight.scalar != routedRoughness.scalar,
           "independently routed region attributes use distinct deterministic values");

    for (const auto output : paperweight::materialOutputs) {
        paperweight::GenerationRequest request{
            layered,
            72,
            56,
            output,
            std::nullopt,
            std::nullopt,
        };
        request.workerCount = 1;
        const auto serial = paperweight::generate(request);
        request.workerCount = 4;
        const auto parallel = paperweight::generate(request);
        const auto* serialImage = std::get_if<paperweight::Image>(&serial);
        const auto* parallelImage = std::get_if<paperweight::Image>(&parallel);
        expect(serialImage != nullptr && parallelImage != nullptr &&
                   std::equal(
                       serialImage->pixels().begin(),
                       serialImage->pixels().end(),
                       parallelImage->pixels().begin()),
               "region attributes are byte-identical with one or four workers");
    }

    const auto serialised = paperweight::serialisePmat(layered);
    const auto* text = std::get_if<std::string>(&serialised);
    expect(text != nullptr && text->find("pmat.version = 15") != std::string::npos &&
               text->find("operation = region_field") != std::string::npos,
           "region fields introduced in version 9 serialise canonically as current .pmat");
    if (text != nullptr) {
        const auto reparsed = paperweight::parsePmat(*text);
        expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                   std::get<paperweight::Material>(reparsed) == layered,
               "region-field materials round-trip exactly through canonical .pmat");
        auto premature = *text;
        const auto marker = premature.find("pmat.version = 15");
        premature.replace(marker, std::string("pmat.version = 15").size(),
                          "pmat.version = 8");
        const auto result = paperweight::parsePmat(premature);
        expect(std::holds_alternative<paperweight::ParseDiagnostic>(result) &&
                   std::get<paperweight::ParseDiagnostic>(result).message.find(
                       "require .pmat version 9") != std::string::npos,
               "older .pmat versions reject region-field operations explicitly");
    }

    auto invalid = layered;
    std::get<paperweight::RegionFieldOperation>(invalid.layers.back().operation).channel = 256;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "out-of-range region random channels are diagnosed");
}

void testCourseLayouts()
{
    paperweight::CourseLayoutOperation layout;
    layout.blocks = 7;
    layout.courses = 6;
    layout.blockVariation = 0.62;
    layout.courseVariation = 0.48;
    layout.stagger = 0.45;
    layout.crookedness = 0.7;
    layout.gap = 0.1;
    layout.softness = 0.025;
    layout.seedOffset = 811;
    const paperweight::PhysicalSize size{2.8, 1.2};
    const auto sample = paperweight::evaluateCourseLayoutFields(
        layout, size, -0.17, 0.43, 18431);
    const auto repeated = paperweight::evaluateCourseLayoutFields(
        layout, size, 0.83, 1.43, 18431);
    expect(sample.region.key == repeated.region.key &&
               sample.region.parentKey == repeated.region.parentKey &&
               sample.region.valid && sample.region.parentValid,
           "course and block identities remain stable across both tile seams");
    expectNear(sample.blocks, repeated.blocks, 1.0e-12,
               "crooked course layouts remain mathematically seamless");
    expectNear(sample.mortar, 1.0 - sample.blocks, 0.0,
               "course mortar is the exact complement of the block mask");
    expect(sample.course + 1.0e-12 >= sample.blocks,
           "course interiors include every visible block face");
    expect(sample.region.localU >= 0.0 && sample.region.localU <= 1.0 &&
               sample.region.localV >= 0.0 && sample.region.localV <= 1.0,
           "variable course layouts expose normalised region-local coordinates");

    auto regular = layout;
    regular.blockVariation = 0.0;
    regular.courseVariation = 0.0;
    regular.crookedness = 0.0;
    regular.stagger = 0.0;
    regular.gap = 0.0;
    regular.softness = 0.0;
    const auto regularCentre = paperweight::evaluateCourseLayoutFields(
        regular,
        size,
        0.5 / static_cast<double>(regular.blocks),
        0.5 / static_cast<double>(regular.courses),
        18431);
    expect(regularCentre.blocks == 1.0 && regularCentre.course == 1.0,
           "zero variation and crookedness reduce to a regular rectangular grid");

    auto slabs = layout;
    slabs.profile = paperweight::CourseLayoutProfile::slabs;
    const auto slab = paperweight::evaluateCourseLayoutFields(
        slabs, size, 0.37, 0.61, 18431);
    expect(slab.region.key != sample.region.key,
           "slab subdivision has a domain separate from masonry ownership");

    auto slates = layout;
    slates.profile = paperweight::CourseLayoutProfile::slates;
    slates.overlap = 0.4;
    bool foundOverlap = false;
    for (std::uint32_t y = 0; y < 64 && !foundOverlap; ++y) {
        for (std::uint32_t x = 0; x < 64; ++x) {
            const auto fields = paperweight::evaluateCourseLayoutFields(
                slates,
                size,
                (static_cast<double>(x) + 0.5) / 64.0,
                (static_cast<double>(y) + 0.5) / 64.0,
                18431);
            foundOverlap = fields.overlap > 0.5;
            if (foundOverlap) {
                break;
            }
        }
    }
    expect(foundOverlap,
           "slate profiles expose a visible deterministic overlap field");
    expect(paperweight::evaluateCourseLayoutFields(
               layout, size, 0.37, 0.61, 18431).overlap == 0.0,
           "masonry profiles do not invent a roof-overlap mask");

    paperweight::Material physicalMaterial;
    physicalMaterial.physicalSize = size;
    physicalMaterial.layers = {paperweight::makeCourseLayoutLayer()};
    auto& physical = std::get<paperweight::CourseLayoutOperation>(
        physicalMaterial.layers.front().operation);
    physical.profile = paperweight::CourseLayoutProfile::slates;
    physical.blocks = 7;
    physical.courses = 6;
    physical.physicalDimensions =
        paperweight::CourseLayoutOperation::PhysicalDimensions{0.4, 0.2, 0.012, 0.06};
    expect(!paperweight::validateMaterial(physicalMaterial).has_value(),
           "physical block, course, gap, and overlap dimensions validate together");
    auto invalidPhysicalCount = physicalMaterial;
    std::get<paperweight::CourseLayoutOperation>(
        invalidPhysicalCount.layers.front().operation).blocks = 0;
    expect(paperweight::validateMaterial(invalidPhysicalCount).has_value(),
           "stored physical course counts remain valid for relative-mode round trips");
    auto invalidPhysical = physicalMaterial;
    std::get<paperweight::CourseLayoutOperation>(
        invalidPhysical.layers.front().operation).physicalDimensions->overlapMetres = 0.2;
    expect(paperweight::validateMaterial(invalidPhysical).has_value(),
           "overlap depths that consume a whole physical course are rejected");

    auto fieldMaterial = paperweight::Material{};
    auto fieldLayer = paperweight::makeCourseLayoutLayer();
    auto& fieldLayout = std::get<paperweight::CourseLayoutOperation>(fieldLayer.operation);
    fieldLayout.blocks = 4;
    fieldLayout.courses = 2;
    fieldLayout.blockVariation = 0.0;
    fieldLayout.courseVariation = 0.0;
    fieldLayout.stagger = 0.0;
    fieldLayout.crookedness = 0.0;
    fieldLayout.gap = 0.0;
    fieldLayout.softness = 0.0;
    fieldMaterial.layers = {fieldLayer, paperweight::makeRegionFieldLayer()};
    auto& courseRandom = std::get<paperweight::RegionFieldOperation>(
        fieldMaterial.layers.back().operation);
    courseRandom.field = paperweight::RegionFieldKind::courseRandom;
    courseRandom.seedOffset = 79;
    courseRandom.channel = 4;
    const auto firstBlock = paperweight::evaluateMaterialSample(fieldMaterial, 0.1, 0.2);
    const auto secondBlock = paperweight::evaluateMaterialSample(fieldMaterial, 0.35, 0.2);
    expect(firstBlock.region.key != secondBlock.region.key &&
               firstBlock.region.parentKey == secondBlock.region.parentKey &&
               firstBlock.scalar == secondBlock.scalar,
           "course-random fields agree across distinct blocks in one course");

    auto routed = paperweight::Material{};
    routed.layers = {paperweight::makeCourseLayoutLayer()};
    const auto compiled = paperweight::compileMaterialGraph(routed);
    const auto* graph = std::get_if<paperweight::MaterialGraph>(&compiled);
    bool foundCourseGenerator = false;
    if (graph != nullptr) {
        for (const auto& node : graph->nodes) {
            if (const auto* generator = std::get_if<paperweight::GeneratorNode>(&node)) {
                foundCourseGenerator = foundCourseGenerator ||
                    std::holds_alternative<paperweight::CourseLayoutOperation>(
                        generator->operation);
            }
        }
    }
    expect(graph != nullptr && foundCourseGenerator,
           "course layouts compile as reusable graph generators");

    for (const auto output : paperweight::materialOutputs) {
        paperweight::GenerationRequest request{
            routed, 80, 56, output, std::nullopt, std::nullopt, 1};
        const auto serial = paperweight::generate(request);
        request.workerCount = 4;
        const auto parallel = paperweight::generate(request);
        const auto* serialImage = std::get_if<paperweight::Image>(&serial);
        const auto* parallelImage = std::get_if<paperweight::Image>(&parallel);
        expect(serialImage != nullptr && parallelImage != nullptr &&
                   std::equal(
                       serialImage->pixels().begin(),
                       serialImage->pixels().end(),
                       parallelImage->pixels().begin()),
               "course-layout outputs are byte-identical with one or four workers");
    }

    const auto serialised = paperweight::serialisePmat(physicalMaterial);
    const auto* text = std::get_if<std::string>(&serialised);
    expect(text != nullptr && text->find("pmat.version = 15") != std::string::npos &&
               text->find("operation = course_layout") != std::string::npos &&
               text->find("course.sizing = physical") != std::string::npos,
           "physical course layouts serialise explicitly in .pmat version 10");
    if (text != nullptr) {
        const auto reparsed = paperweight::parsePmat(*text);
        expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                   std::get<paperweight::Material>(reparsed) == physicalMaterial,
               "course layouts round-trip exactly through .pmat version 10");
        auto premature = *text;
        const auto marker = premature.find("pmat.version = 15");
        premature.replace(marker, std::string("pmat.version = 15").size(),
                          "pmat.version = 9");
        const auto rejected = paperweight::parsePmat(premature);
        expect(std::holds_alternative<paperweight::ParseDiagnostic>(rejected) &&
                   std::get<paperweight::ParseDiagnostic>(rejected).message.find(
                       "require .pmat version 10") != std::string::npos,
               ".pmat version 9 rejects Course Layout explicitly");
    }
}

void testRegionSurfaceSculpting()
{
    paperweight::Material material;
    material.seed = 140014;
    material.normalStrength = 3.0;

    auto cells = paperweight::makeWorleyCellsLayer();
    auto& worley = std::get<paperweight::WorleyCellsOperation>(cells.operation);
    worley.columns = 6;
    worley.rows = 5;
    worley.jitter = 0.82;
    worley.edgeWidth = 0.42;
    worley.seedOffset = 71;

    auto sculptLayer = paperweight::makeRegionSurfaceLayer();
    auto& sculpt = std::get<paperweight::RegionSurfaceOperation>(sculptLayer.operation);
    sculpt.profile = paperweight::BevelProfile::handCut;
    sculpt.bevelWidth = 0.42;
    sculpt.bevelHeight = 0.72;
    sculpt.facetCount = 7;
    sculpt.facetStrength = 0.34;
    sculpt.centrePeak = 0.22;
    sculpt.slopeStrength = 0.12;
    sculpt.chipAmount = 0.17;
    sculpt.chipScale = 11;
    sculpt.wearAmount = 0.24;
    sculpt.erosionAmount = 0.13;
    sculpt.seedOffset = 901;
    sculpt.facetedNormals = true;
    sculpt.target = paperweight::ProcessingTarget::scalar;
    material.layers = {cells, sculptLayer};

    const auto structural = paperweight::evaluateWorleyCellsSample(
        worley, 0.37, 0.61, material.seed);
    const auto fields = paperweight::evaluateRegionSurfaceFields(
        sculpt,
        material,
        structural.region,
        structural.value,
        0.37,
        0.61);
    for (const auto value : {
             fields.height,
             fields.cavity,
             fields.outerEdge,
             fields.exposedFace,
             fields.facet,
             fields.wear,
         }) {
        expect(value >= 0.0 && value <= 1.0,
               "every region surface field remains in the unit range");
    }
    expectNear(fields.height + fields.cavity, 1.0, 1.0e-12,
               "cavity is the exact complement of constructed height");

    constexpr std::array sculptFields{
        paperweight::RegionSurfaceField::height,
        paperweight::RegionSurfaceField::cavity,
        paperweight::RegionSurfaceField::outerEdge,
        paperweight::RegionSurfaceField::exposedFace,
        paperweight::RegionSurfaceField::facet,
        paperweight::RegionSurfaceField::wear,
    };
    constexpr std::array<std::uint64_t, 6> sculptFieldGoldens{
        15890420516092499979ULL,
        1815337865590187539ULL,
        7676403396097439073ULL,
        14718955348197570489ULL,
        15488971196823987208ULL,
        14869315570379556285ULL,
    };
    for (std::size_t fieldIndex = 0; fieldIndex < sculptFields.size(); ++fieldIndex) {
        auto fieldMaterial = material;
        std::get<paperweight::RegionSurfaceOperation>(
            fieldMaterial.layers.back().operation).field = sculptFields[fieldIndex];
        paperweight::GenerationRequest request{
            fieldMaterial,
            32,
            32,
            paperweight::MaterialOutput::height,
            std::nullopt,
            std::nullopt,
            1};
        const auto generated = paperweight::generate(request);
        expectChecksum(
            std::get_if<paperweight::Image>(&generated),
            sculptFieldGoldens[fieldIndex],
            "constructed height and sculpt masks match byte-exact golden checksums");
    }

    paperweight::RegionSample manual{
        paperweight::makeRegionKey(17, 2, 3), 0.5, 0.5, 0.0, 0.12, true};
    auto profile = sculpt;
    profile.chipAmount = 0.0;
    profile.wearAmount = 0.0;
    profile.erosionAmount = 0.0;
    profile.facetStrength = 0.0;
    profile.centrePeak = 0.0;
    profile.slopeStrength = 0.0;
    profile.bevelWidth = 0.5;
    profile.bevelHeight = 1.0;
    profile.profile = paperweight::BevelProfile::chamfered;
    const double chamfer = paperweight::evaluateRegionSurface(
        profile, material, manual, 1.0, 0.3, 0.4);
    profile.profile = paperweight::BevelProfile::rounded;
    const double rounded = paperweight::evaluateRegionSurface(
        profile, material, manual, 1.0, 0.3, 0.4);
    profile.profile = paperweight::BevelProfile::handCut;
    const double handCut = paperweight::evaluateRegionSurface(
        profile, material, manual, 1.0, 0.3, 0.4);
    expect(rounded > chamfer && handCut != chamfer,
           "rounded, chamfered, and hand-cut bevels produce distinct profiles");

    const auto compiled = paperweight::compileMaterialGraph(material);
    const auto* graph = std::get_if<paperweight::MaterialGraph>(&compiled);
    bool foundSculptProcessor = false;
    if (graph != nullptr) {
        for (const auto& node : graph->nodes) {
            if (const auto* processing = std::get_if<paperweight::ProcessingNode>(&node)) {
                foundSculptProcessor = foundSculptProcessor ||
                    std::holds_alternative<paperweight::RegionSurfaceProcessing>(
                        processing->operation);
            }
        }
    }
    expect(graph != nullptr && foundSculptProcessor,
           "region sculpting compiles as a reusable graph processor");
    if (graph != nullptr) {
        const auto sample = paperweight::evaluateMaterialGraphSample(
            material, *graph, paperweight::MaterialOutput::height, -0.19, 0.43);
        const auto repeated = paperweight::evaluateMaterialGraphSample(
            material, *graph, paperweight::MaterialOutput::height, 0.81, 1.43);
        expectNear(sample.scalar, repeated.scalar, 1.0e-12,
                   "constructed region height remains mathematically seamless");
    }

    paperweight::GenerationRequest lowRequest{
        material, 32, 24, paperweight::MaterialOutput::height,
        std::nullopt, std::nullopt, 0};
    lowRequest.workerCount = 1;
    auto highRequest = lowRequest;
    highRequest.width = 96;
    highRequest.height = 72;
    highRequest.workerCount = 4;
    const auto lowResult = paperweight::generate(lowRequest);
    const auto highResult = paperweight::generate(highRequest);
    const auto* low = std::get_if<paperweight::Image>(&lowResult);
    const auto* high = std::get_if<paperweight::Image>(&highResult);
    bool matchingSamples = low != nullptr && high != nullptr;
    if (matchingSamples) {
        for (std::uint32_t y = 0; y < low->height() && matchingSamples; ++y) {
            for (std::uint32_t x = 0; x < low->width(); ++x) {
                if (low->row(y)[x] != high->row(y * 3U + 1U)[x * 3U + 1U]) {
                    matchingSamples = false;
                    break;
                }
            }
        }
    }
    expect(matchingSamples,
           "region sculpture preserves matching samples across output resolutions");

    auto serialRequest = highRequest;
    serialRequest.workerCount = 1;
    const auto serialResult = paperweight::generate(serialRequest);
    const auto* serialImage = std::get_if<paperweight::Image>(&serialResult);
    expect(serialImage != nullptr && high != nullptr &&
               std::equal(
                   serialImage->pixels().begin(),
                   serialImage->pixels().end(),
                   high->pixels().begin()),
           "region sculpture is byte-identical with one or four workers");

    auto smooth = material;
    std::get<paperweight::RegionSurfaceOperation>(smooth.layers.back().operation)
        .facetedNormals = false;
    paperweight::GenerationRequest smoothHeight{
        smooth, 72, 56, paperweight::MaterialOutput::height,
        std::nullopt, std::nullopt, 0};
    paperweight::GenerationRequest facetedHeight{
        material, 72, 56, paperweight::MaterialOutput::height,
        std::nullopt, std::nullopt, 0};
    const auto smoothHeightResult = paperweight::generate(smoothHeight);
    const auto facetedHeightResult = paperweight::generate(facetedHeight);
    const auto* smoothHeightImage = std::get_if<paperweight::Image>(&smoothHeightResult);
    const auto* facetedHeightImage = std::get_if<paperweight::Image>(&facetedHeightResult);
    expect(smoothHeightImage != nullptr && facetedHeightImage != nullptr &&
               std::equal(
                   smoothHeightImage->pixels().begin(),
                   smoothHeightImage->pixels().end(),
                   facetedHeightImage->pixels().begin()),
           "faceted-normal treatment leaves height bytes unchanged");
    auto smoothNormal = smoothHeight;
    smoothNormal.output = paperweight::MaterialOutput::normal;
    auto facetedNormal = facetedHeight;
    facetedNormal.output = paperweight::MaterialOutput::normal;
    const auto smoothNormalResult = paperweight::generate(smoothNormal);
    const auto facetedNormalResult = paperweight::generate(facetedNormal);
    const auto* smoothNormalImage = std::get_if<paperweight::Image>(&smoothNormalResult);
    const auto* facetedNormalImage = std::get_if<paperweight::Image>(&facetedNormalResult);
    expect(smoothNormalImage != nullptr && facetedNormalImage != nullptr &&
               checksum(smoothNormalImage->pixels()) != checksum(facetedNormalImage->pixels()),
           "faceted-normal treatment changes only the normal construction path");

    const auto serialised = paperweight::serialisePmat(material);
    const auto* text = std::get_if<std::string>(&serialised);
    expect(text != nullptr && text->find("pmat.version = 15") != std::string::npos &&
               text->find("operation = region_surface") != std::string::npos &&
               text->find("sculpt.faceted_normals = true") != std::string::npos,
           "region sculpture serialises explicitly in .pmat version 11");
    if (text != nullptr) {
        const auto reparsed = paperweight::parsePmat(*text);
        expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                   std::get<paperweight::Material>(reparsed) == material,
               "region sculpture round-trips exactly through .pmat version 11");
        auto premature = *text;
        const auto marker = premature.find("pmat.version = 15");
        premature.replace(marker, std::string("pmat.version = 15").size(),
                          "pmat.version = 10");
        const auto rejected = paperweight::parsePmat(premature);
        expect(std::holds_alternative<paperweight::ParseDiagnostic>(rejected) &&
                   std::get<paperweight::ParseDiagnostic>(rejected).message.find(
                       "requires .pmat version 11") != std::string::npos,
               ".pmat version 10 rejects region sculpture explicitly");
    }

    auto invalid = material;
    std::get<paperweight::RegionSurfaceOperation>(invalid.layers.back().operation)
        .facetCount = 2;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "invalid sculpt facet counts are diagnosed");
}

void testShapePrimitivesAndLattices()
{
    paperweight::ShapePrimitiveOperation shape;
    shape.columns = 5;
    shape.rows = 4;
    shape.width = 0.68;
    shape.height = 0.52;
    shape.cornerRadius = 0.11;
    shape.inset = 0.07;
    shape.borderWidth = 0.09;
    shape.softness = 0.018;
    shape.offsetX = 0.08;
    shape.offsetY = -0.06;
    shape.stagger = 0.42;
    shape.rotationDegrees = 37.0;
    shape.seedOffset = 1515;

    constexpr std::array primitiveKinds{
        paperweight::ShapePrimitiveKind::roundedRectangle,
        paperweight::ShapePrimitiveKind::ellipse,
        paperweight::ShapePrimitiveKind::capsule,
        paperweight::ShapePrimitiveKind::diamond,
        paperweight::ShapePrimitiveKind::convexPolygon,
    };
    constexpr std::array<std::uint64_t, primitiveKinds.size()> primitiveGoldens{
        8642410819614480771ULL,
        3886759640178523859ULL,
        16180845089984606627ULL,
        6898999178736912211ULL,
        1814323174130660755ULL,
    };
    for (std::size_t kindIndex = 0; kindIndex < primitiveKinds.size(); ++kindIndex) {
        shape.kind = primitiveKinds[kindIndex];
        shape.field = paperweight::ShapeFieldKind::fill;
        for (const auto [u, v] : std::array{
                 std::pair{-0.37, 0.19},
                 std::pair{0.0, 0.0},
                 std::pair{0.48, 0.76},
                 std::pair{1.13, -2.41},
             }) {
            const auto sample = paperweight::evaluateShapePrimitive(shape, u, v);
            const auto repeatX = paperweight::evaluateShapePrimitive(shape, u + 1.0, v);
            const auto repeatY = paperweight::evaluateShapePrimitive(shape, u, v + 1.0);
            expectNear(sample.value, repeatX.value, 1.0e-12,
                       "every analytic shape repeats exactly across U");
            expectNear(sample.value, repeatY.value, 1.0e-12,
                       "every analytic shape repeats exactly across V");
            expect(sample.region.key == repeatX.region.key &&
                       sample.region.key == repeatY.region.key,
                   "repeated shapes retain stable wrapped region identities");
        }

        paperweight::Material material;
        auto layer = paperweight::makeShapePrimitiveLayer();
        std::get<paperweight::ShapePrimitiveOperation>(layer.operation) = shape;
        material.layers = {layer};
        const auto generated = paperweight::generate({
            material,
            48,
            40,
            paperweight::MaterialOutput::colour,
            std::nullopt,
            std::nullopt,
            1,
        });
        expectChecksum(
            std::get_if<paperweight::Image>(&generated),
            primitiveGoldens[kindIndex],
            "analytic shape primitives match byte-exact golden masks");
    }

    constexpr std::array shapeFields{
        paperweight::ShapeFieldKind::fill,
        paperweight::ShapeFieldKind::inset,
        paperweight::ShapeFieldKind::outline,
        paperweight::ShapeFieldKind::border,
    };
    constexpr std::array<std::uint64_t, shapeFields.size()> fieldGoldens{
        8642410819614480771ULL,
        1280363926637515491ULL,
        15250998348995193203ULL,
        1505116505883154123ULL,
    };
    shape.kind = paperweight::ShapePrimitiveKind::roundedRectangle;
    for (std::size_t fieldIndex = 0; fieldIndex < shapeFields.size(); ++fieldIndex) {
        shape.field = shapeFields[fieldIndex];
        paperweight::Material material;
        auto layer = paperweight::makeShapePrimitiveLayer();
        std::get<paperweight::ShapePrimitiveOperation>(layer.operation) = shape;
        material.layers = {layer};
        const auto generated = paperweight::generate({
            material,
            48,
            40,
            paperweight::MaterialOutput::colour,
            std::nullopt,
            std::nullopt,
            1,
        });
        expectChecksum(
            std::get_if<paperweight::Image>(&generated),
            fieldGoldens[fieldIndex],
            "shape fill, inset, outline, and border fields have stable golden masks");
        const auto seam = paperweight::evaluateShapePrimitive(shape, 0.217, 0.693);
        expectNear(
            seam.value,
            paperweight::evaluateShapePrimitive(shape, 1.217, 0.693).value,
            1.0e-12,
            "every distance-derived shape field remains seamless");
    }

    auto unrotated = shape;
    unrotated.field = paperweight::ShapeFieldKind::fill;
    unrotated.rotationDegrees = 0.0;
    const auto rotatedSample = paperweight::evaluateShapePrimitive(shape, 0.267, 0.148);
    const auto unrotatedSample = paperweight::evaluateShapePrimitive(unrotated, 0.267, 0.148);
    expect(rotatedSample.value != unrotatedSample.value,
           "bounded repeated shapes accept arbitrary local rotation");

    expectNear(
        paperweight::combineShapeMasks(0.25, 0.75, paperweight::ShapeBooleanMode::unionMask),
        0.75,
        0.0,
        "shape union selects either covered mask");
    expectNear(
        paperweight::combineShapeMasks(0.25, 0.75, paperweight::ShapeBooleanMode::intersection),
        0.25,
        0.0,
        "shape intersection retains shared coverage");
    expectNear(
        paperweight::combineShapeMasks(0.8, 0.6, paperweight::ShapeBooleanMode::subtraction),
        0.4,
        1.0e-12,
        "shape subtraction removes analytic coverage");

    paperweight::LatticeOperation lattice;
    lattice.kind = paperweight::LatticeKind::diamonds;
    lattice.windingX = 5;
    lattice.windingY = 3;
    lattice.width = 0.11;
    lattice.softness = 0.015;
    lattice.phase = 0.17;
    for (const auto [u, v] : std::array{
             std::pair{-0.31, 0.27},
             std::pair{0.0, 0.0},
             std::pair{0.79, 0.53},
         }) {
        const auto sample = paperweight::evaluateLattice(lattice, u, v);
        expectNear(sample.value, paperweight::evaluateLattice(lattice, u + 1.0, v).value,
                   1.0e-12, "integer-winding diamond lattices repeat across U");
        expectNear(sample.value, paperweight::evaluateLattice(lattice, u, v + 1.0).value,
                   1.0e-12, "integer-winding diamond lattices repeat across V");
    }
    const double diamondSample = paperweight::evaluateLattice(lattice, 0.0, 0.89).value;
    lattice.kind = paperweight::LatticeKind::lines;
    expect(diamondSample != paperweight::evaluateLattice(lattice, 0.0, 0.89).value,
           "parallel and crossed diamond lattices remain distinct fields");

    paperweight::Material graphMaterial;
    auto shapeLayer = paperweight::makeShapePrimitiveLayer();
    std::get<paperweight::ShapePrimitiveOperation>(shapeLayer.operation) = shape;
    auto booleanLayer = paperweight::makeShapeBooleanLayer();
    auto& boolean = std::get<paperweight::ShapeBooleanOperation>(booleanLayer.operation);
    boolean.mode = paperweight::ShapeBooleanMode::subtraction;
    boolean.shape.kind = paperweight::ShapePrimitiveKind::ellipse;
    boolean.shape.rotationDegrees = -23.5;
    boolean.target = paperweight::ProcessingTarget::scalar;
    auto latticeLayer = paperweight::makeLatticeLayer();
    std::get<paperweight::LatticeOperation>(latticeLayer.operation) = lattice;
    latticeLayer.opacity = 0.35;
    graphMaterial.layers = {shapeLayer, booleanLayer, latticeLayer};

    const auto compiled = paperweight::compileMaterialGraph(graphMaterial);
    const auto* graph = std::get_if<paperweight::MaterialGraph>(&compiled);
    bool foundShape = false;
    bool foundBoolean = false;
    bool foundLattice = false;
    if (graph != nullptr) {
        for (const auto& node : graph->nodes) {
            if (const auto* generator = std::get_if<paperweight::GeneratorNode>(&node)) {
                foundShape = foundShape ||
                    std::holds_alternative<paperweight::ShapePrimitiveOperation>(
                        generator->operation);
                foundLattice = foundLattice ||
                    std::holds_alternative<paperweight::LatticeOperation>(
                        generator->operation);
            } else if (const auto* processing =
                           std::get_if<paperweight::ProcessingNode>(&node)) {
                foundBoolean = foundBoolean ||
                    std::holds_alternative<paperweight::ShapeBooleanProcessing>(
                        processing->operation);
            }
        }
    }
    expect(graph != nullptr && foundShape && foundBoolean && foundLattice,
           "shapes, Boolean masks, and lattices compile into reusable graph nodes");

    for (const auto output : paperweight::materialOutputs) {
        paperweight::GenerationRequest request{
            graphMaterial, 72, 56, output, std::nullopt, std::nullopt, 1};
        const auto serial = paperweight::generate(request);
        request.workerCount = 4;
        const auto parallel = paperweight::generate(request);
        const auto* serialImage = std::get_if<paperweight::Image>(&serial);
        const auto* parallelImage = std::get_if<paperweight::Image>(&parallel);
        expect(serialImage != nullptr && parallelImage != nullptr &&
                   std::equal(
                       serialImage->pixels().begin(),
                       serialImage->pixels().end(),
                       parallelImage->pixels().begin()),
               "shape and lattice outputs are byte-identical with one or four workers");
    }

    const auto serialised = paperweight::serialisePmat(graphMaterial);
    const auto* text = std::get_if<std::string>(&serialised);
    expect(text != nullptr && text->find("pmat.version = 15") != std::string::npos &&
               text->find("operation = shape_boolean") != std::string::npos &&
               text->find("shape.vertex.5.y") != std::string::npos &&
               text->find("lattice.winding_x = 5") != std::string::npos,
           "shape vocabulary serialises explicitly and readably in .pmat version 12");
    if (text != nullptr) {
        const auto reparsed = paperweight::parsePmat(*text);
        expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                   std::get<paperweight::Material>(reparsed) == graphMaterial,
               "shape and lattice materials round-trip exactly through .pmat version 12");
        auto premature = *text;
        const auto marker = premature.find("pmat.version = 15");
        premature.replace(marker, std::string("pmat.version = 15").size(),
                          "pmat.version = 11");
        const auto rejected = paperweight::parsePmat(premature);
        expect(std::holds_alternative<paperweight::ParseDiagnostic>(rejected) &&
                   std::get<paperweight::ParseDiagnostic>(rejected).message.find(
                       "require .pmat version 12") != std::string::npos,
               ".pmat version 11 rejects shape and lattice fields explicitly");
    }

    auto invalidShape = graphMaterial;
    auto& invalidPolygon = std::get<paperweight::ShapePrimitiveOperation>(
        invalidShape.layers.front().operation);
    invalidPolygon.kind = paperweight::ShapePrimitiveKind::convexPolygon;
    invalidPolygon.vertices = {{-0.4, -0.4}, {0.4, -0.4}, {0.0, 0.0}, {0.4, 0.4}};
    expect(paperweight::validateMaterial(invalidShape).has_value(),
           "concave or degenerate polygon definitions are diagnosed");
    auto invalidLattice = graphMaterial;
    auto& invalidWinding = std::get<paperweight::LatticeOperation>(
        invalidLattice.layers.back().operation);
    invalidWinding.kind = paperweight::LatticeKind::diamonds;
    invalidWinding.windingX = 0;
    expect(paperweight::validateMaterial(invalidLattice).has_value(),
           "non-tile-compatible diamond winding definitions are diagnosed");
}

void testDeterministicScatter()
{
    auto operation = paperweight::ScatterOperation{};
    operation.columns = 14;
    operation.rows = 12;
    operation.density = 0.82;
    operation.minimumDistance = 0.035;
    operation.overlapMode = paperweight::ScatterOverlapMode::controlled;
    operation.maximumOverlap = 0.42;
    operation.seedOffset = 16016;
    operation.stamp.width = 0.065;
    operation.stamp.height = 0.052;
    operation.populations = {
        paperweight::ScatterPopulation{
            0.72, 0.65, 1.05, 0.72, 1.4, -180.0, 180.0,
            {78, 82, 76, 255}, {139, 144, 130, 255},
            0.42, 0.72, 0.66, 0.94},
        paperweight::ScatterPopulation{
            0.28, 1.25, 1.75, 0.8, 1.25, -180.0, 180.0,
            {110, 112, 104, 255}, {186, 184, 166, 255},
            0.68, 0.96, 0.48, 0.78},
    };

    auto material = paperweight::Material{};
    auto layer = paperweight::makeScatterLayer();
    layer.operation = operation;
    material.layers = {layer};
    material.roughnessLow = 0.0;
    material.roughnessHigh = 1.0;
    expect(!paperweight::validateMaterial(material).has_value(),
           "a bounded multi-population scatter material validates");

    const auto first = paperweight::buildScatterLayout(operation, material.seed);
    const auto second = paperweight::buildScatterLayout(operation, material.seed);
    expect(first == second && !first.instances.empty(),
           "scatter candidate order, rejection, attributes, and accepted layout are deterministic");
    expect(first.cellInstanceIndices.size() ==
               static_cast<std::size_t>(operation.columns) * operation.rows,
           "scatter layout memory is bounded by the authored candidate grid");

    bool populationsPresent[2]{};
    bool attributesInRange = true;
    for (const auto& instance : first.instances) {
        if (instance.populationIndex >= operation.populations.size()) {
            attributesInRange = false;
            continue;
        }
        populationsPresent[instance.populationIndex] = true;
        const auto& population = operation.populations[instance.populationIndex];
        attributesInRange = attributesInRange &&
            instance.scale >= population.minimumScale &&
            instance.scale <= population.maximumScale &&
            instance.aspect >= population.minimumAspect &&
            instance.aspect <= population.maximumAspect &&
            instance.rotationDegrees >= population.minimumRotation &&
            instance.rotationDegrees <= population.maximumRotation &&
            instance.height >= population.minimumHeight &&
            instance.height <= population.maximumHeight &&
            instance.roughness >= population.minimumRoughness &&
            instance.roughness <= population.maximumRoughness;
    }
    expect(populationsPresent[0] && populationsPresent[1] && attributesInRange,
           "weighted size populations produce independently bounded stable attributes");

    auto spaced = operation;
    spaced.columns = 16;
    spaced.rows = 16;
    spaced.density = 1.0;
    spaced.minimumDistance = 0.075;
    spaced.overlapMode = paperweight::ScatterOverlapMode::unrestricted;
    spaced.populations = {operation.populations.front()};
    spaced.populations.front().minimumScale = 1.0;
    spaced.populations.front().maximumScale = 1.0;
    spaced.populations.front().minimumAspect = 1.0;
    spaced.populations.front().maximumAspect = 1.0;
    const auto spacedLayout = paperweight::buildScatterLayout(spaced, material.seed);
    const auto torusDistance = [](const paperweight::ScatterInstance& a,
                                  const paperweight::ScatterInstance& b) {
        double du = std::abs(a.centreU - b.centreU);
        double dv = std::abs(a.centreV - b.centreV);
        du = std::min(du, 1.0 - du);
        dv = std::min(dv, 1.0 - dv);
        return std::hypot(du, dv);
    };
    bool minimumDistanceHeld = true;
    for (std::size_t firstIndex = 0;
         firstIndex < spacedLayout.instances.size() && minimumDistanceHeld;
         ++firstIndex) {
        for (std::size_t secondIndex = firstIndex + 1;
             secondIndex < spacedLayout.instances.size();
             ++secondIndex) {
            if (torusDistance(
                    spacedLayout.instances[firstIndex],
                    spacedLayout.instances[secondIndex]) + 1.0e-12 <
                spaced.minimumDistance) {
                minimumDistanceHeld = false;
                break;
            }
        }
    }
    expect(minimumDistanceHeld,
           "minimum-distance rejection uses the torus metric across opposite edges");

    auto unrestricted = operation;
    unrestricted.columns = 6;
    unrestricted.rows = 5;
    unrestricted.density = 1.0;
    unrestricted.minimumDistance = 0.0;
    unrestricted.overlapMode = paperweight::ScatterOverlapMode::unrestricted;
    const auto unrestrictedLayout = paperweight::buildScatterLayout(
        unrestricted,
        material.seed);
    expect(unrestrictedLayout.instances.size() == 30,
           "unrestricted full-density scatter accepts one stable candidate per cell");
    auto empty = unrestricted;
    empty.density = 0.0;
    expect(paperweight::buildScatterLayout(empty, material.seed).instances.empty(),
           "zero scatter density accepts no candidates");

    const auto& seamInstance = first.instances.front();
    const auto centreSample = paperweight::evaluateScatter(
        operation,
        first,
        seamInstance.centreU,
        seamInstance.centreV);
    const auto wrappedSample = paperweight::evaluateScatter(
        operation,
        first,
        seamInstance.centreU + 1.0,
        seamInstance.centreV - 1.0);
    expect(centreSample.region.key == wrappedSample.region.key &&
               centreSample.region.key == seamInstance.key &&
               std::abs(centreSample.coverage - wrappedSample.coverage) <= 1.0e-12 &&
               std::abs(centreSample.signedDistance - wrappedSample.signedDistance) <= 1.0e-12,
           "edge-crossing scatter stamps are the same wrapped instance");

    auto overlapping = operation;
    overlapping.columns = 2;
    overlapping.rows = 1;
    overlapping.density = 1.0;
    overlapping.jitter = 0.0;
    overlapping.minimumDistance = 0.0;
    overlapping.overlapMode = paperweight::ScatterOverlapMode::unrestricted;
    overlapping.stamp.width = 0.8;
    overlapping.stamp.height = 0.8;
    overlapping.stamp.softness = 0.0;
    overlapping.populations = {operation.populations.front()};
    overlapping.populations.front().minimumScale = 1.0;
    overlapping.populations.front().maximumScale = 1.0;
    overlapping.populations.front().minimumAspect = 1.0;
    overlapping.populations.front().maximumAspect = 1.0;
    const auto overlappingLayout = paperweight::buildScatterLayout(
        overlapping,
        material.seed);
    const auto overlapSample = paperweight::evaluateScatter(
        overlapping,
        overlappingLayout,
        0.5,
        0.5);
    const auto expectedFront = std::max_element(
        overlappingLayout.instances.begin(),
        overlappingLayout.instances.end(),
        [](const auto& a, const auto& b) {
            return a.occlusionPriority < b.occlusionPriority ||
                (a.occlusionPriority == b.occlusionPriority &&
                 a.candidateIndex < b.candidateIndex);
        });
    expect(expectedFront != overlappingLayout.instances.end() &&
               overlapSample.region.key == expectedFront->key,
           "overlap resolves by exact stable occlusion priority with an integer fallback");

    auto masked = unrestricted;
    masked.exclusionMask.enabled = true;
    masked.exclusionMask.inverted = false;
    masked.exclusionMask.frequency = 3;
    masked.exclusionMask.inputLow = 0.0;
    masked.exclusionMask.inputHigh = 0.001;
    const auto maskedLayout = paperweight::buildScatterLayout(masked, material.seed);
    expect(maskedLayout.instances.size() < unrestrictedLayout.instances.size() &&
               maskedLayout == paperweight::buildScatterLayout(masked, material.seed),
           "candidate-centre exclusion masks remove instances deterministically");

    const auto compiled = paperweight::compileMaterialGraph(material);
    bool foundScatterGenerator = false;
    if (const auto* graph = std::get_if<paperweight::MaterialGraph>(&compiled)) {
        for (const auto& node : graph->nodes) {
            const auto* generator = std::get_if<paperweight::GeneratorNode>(&node);
            foundScatterGenerator = foundScatterGenerator ||
                (generator != nullptr &&
                 std::holds_alternative<paperweight::ScatterOperation>(generator->operation));
        }
    }
    expect(foundScatterGenerator,
           "layer-authored scatter compiles into a reusable generator node");

    const auto serialised = paperweight::serialisePmat(material);
    const auto* scatterText = std::get_if<std::string>(&serialised);
    expect(scatterText != nullptr &&
               scatterText->find("pmat.version = 15") != std::string::npos &&
               scatterText->find("scatter.population.1.colour_high") != std::string::npos &&
               scatterText->find("scatter.exclusion_mask.enabled") != std::string::npos,
           "scatter populations, attributes, and masks serialise explicitly in .pmat version 13");
    if (scatterText != nullptr) {
        const auto parsed = paperweight::parsePmat(*scatterText);
        const auto* reparsed = std::get_if<paperweight::Material>(&parsed);
        expect(reparsed != nullptr && *reparsed == material,
               "deterministic scatter materials round-trip exactly through .pmat version 13");
        auto premature = *scatterText;
        const auto marker = premature.find("pmat.version = 15");
        premature.replace(marker, std::string("pmat.version = 15").size(),
                          "pmat.version = 12");
        const auto rejected = paperweight::parsePmat(premature);
        const auto* diagnostic = std::get_if<paperweight::ParseDiagnostic>(&rejected);
        expect(diagnostic != nullptr &&
                   diagnostic->message.find("requires .pmat version 13") != std::string::npos,
               "scatter fields cannot be smuggled into older .pmat versions");
    }

    constexpr std::array outputs{
        paperweight::MaterialOutput::colour,
        paperweight::MaterialOutput::height,
        paperweight::MaterialOutput::normal,
        paperweight::MaterialOutput::roughness,
    };
    for (const auto output : outputs) {
        paperweight::GenerationRequest request{
            material,
            32,
            24,
            output,
            std::nullopt,
            std::nullopt,
            1,
        };
        const auto lowResolution = paperweight::generate(request);
        request.width = 96;
        request.height = 72;
        request.workerCount = 4;
        const auto highResolution = paperweight::generate(request);
        const auto* lowImage = std::get_if<paperweight::Image>(&lowResolution);
        const auto* highImage = std::get_if<paperweight::Image>(&highResolution);
        bool matchingSamples = lowImage != nullptr && highImage != nullptr;
        if (matchingSamples) {
            for (std::uint32_t y = 0; y < lowImage->height() && matchingSamples; ++y) {
                for (std::uint32_t x = 0; x < lowImage->width(); ++x) {
                    if (lowImage->row(y)[x] != highImage->row(y * 3 + 1)[x * 3 + 1]) {
                        matchingSamples = false;
                        break;
                    }
                }
            }
        }
        if (output == paperweight::MaterialOutput::normal) {
            expect(lowImage != nullptr && highImage != nullptr,
                   "scatter normals generate at different sampling resolutions");
        } else {
            expect(matchingSamples,
                   "scatter placement and attributes match at identical points across resolutions");
        }

        request.width = 48;
        request.height = 40;
        request.workerCount = 1;
        const auto serial = paperweight::generate(request);
        request.workerCount = 4;
        const auto parallel = paperweight::generate(request);
        const auto* serialImage = std::get_if<paperweight::Image>(&serial);
        const auto* parallelImage = std::get_if<paperweight::Image>(&parallel);
        expect(serialImage != nullptr && parallelImage != nullptr &&
                   std::equal(
                       serialImage->pixels().begin(),
                       serialImage->pixels().end(),
                       parallelImage->pixels().begin()),
               "scatter output is byte-identical between serial and multi-worker evaluation");
    }
}

void testOrganicStructures()
{
    constexpr std::uint64_t seed = 170017;

    auto cells = paperweight::OrganicCellOperation{};
    cells.columns = 11;
    cells.rows = 5;
    cells.anisotropy = 3.1;
    cells.irregularity = 0.41;
    const auto cell = paperweight::evaluateOrganicCells(cells, -0.237, 0.681, seed);
    const auto wrappedCell = paperweight::evaluateOrganicCells(cells, 0.763, 1.681, seed);
    expect(cell == wrappedCell && cell.region.valid,
           "anisotropic organic cells repeat exactly across both tile boundaries");
    expect(cell.value >= 0.0 && cell.value <= 1.0 &&
               cell.boundary >= 0.0 && cell.boundary <= 1.0,
           "organic cell fields remain normalised");
    cells.direction = paperweight::OrganicDirection::horizontal;
    const auto horizontalCell = paperweight::evaluateOrganicCells(cells, -0.237, 0.681, seed);
    expect(horizontalCell.region.key != cell.region.key || horizontalCell.value != cell.value,
           "organic cell direction changes plate orientation deterministically");

    auto cracks = paperweight::OrganicCrackOperation{};
    cracks.roots = 6;
    cracks.segments = 8;
    cracks.branchLevels = 4;
    cracks.branchProbability = 0.92;
    cracks.seedOffset = 71;
    const auto crackLayout = paperweight::buildOrganicCrackLayout(cracks, seed);
    expect(crackLayout == paperweight::buildOrganicCrackLayout(cracks, seed) &&
               !crackLayout.segments.empty(),
           "crack trunks, branches, and splitting order are deterministic");
    expect(std::any_of(
               crackLayout.segments.begin(),
               crackLayout.segments.end(),
               [](const auto& segment) { return segment.hierarchy > 0; }),
           "branching crack layouts contain hierarchical child segments");
    const auto crack = paperweight::evaluateOrganicCracks(
        cracks, crackLayout, -0.119, 0.407);
    const auto wrappedCrack = paperweight::evaluateOrganicCracks(
        cracks, crackLayout, 0.881, 1.407);
    expect(crack == wrappedCrack,
           "branching crack networks are mathematically periodic");

    constexpr std::array species{
        paperweight::LeafSpecies::ivy,
        paperweight::LeafSpecies::laurel,
        paperweight::LeafSpecies::oak,
        paperweight::LeafSpecies::ash,
    };
    std::array<paperweight::LeafClusterOperation, species.size()> presets{};
    for (std::size_t index = 0; index < species.size(); ++index) {
        presets[index] = paperweight::leafSpeciesPreset(species[index]);
    }
    expect(presets[0] != presets[1] && presets[1] != presets[2] && presets[2] != presets[3],
           "ivy, laurel, oak, and ash presets expose distinct analytic leaf models");
    bool everySpeciesHasLeafAndVeinCoverage = true;
    for (std::size_t index = 0; index < presets.size(); ++index) {
        auto preset = presets[index];
        preset.columns = 2;
        preset.rows = 2;
        preset.density = 1.0;
        preset.seedOffset = 200 + index;
        const auto presetLayout = paperweight::buildLeafClusterLayout(preset, seed);
        double maximumFill = 0.0;
        double maximumMidrib = 0.0;
        double maximumVeins = 0.0;
        for (std::uint32_t y = 0; y < 48; ++y) {
            for (std::uint32_t x = 0; x < 48; ++x) {
                const auto sample = paperweight::evaluateLeafCluster(
                    preset,
                    presetLayout,
                    (static_cast<double>(x) + 0.5) / 48.0,
                    (static_cast<double>(y) + 0.5) / 48.0);
                maximumFill = std::max(maximumFill, sample.coverage);
                maximumMidrib = std::max(maximumMidrib, sample.midrib);
                maximumVeins = std::max(maximumVeins, sample.veins);
            }
        }
        everySpeciesHasLeafAndVeinCoverage = everySpeciesHasLeafAndVeinCoverage &&
            maximumFill > 0.0 && maximumMidrib > 0.0 && maximumVeins > 0.0;
    }
    expect(everySpeciesHasLeafAndVeinCoverage,
           "every species preset produces visible analytic silhouettes, midribs, and veins");

    auto leaves = presets[0];
    leaves.columns = 4;
    leaves.rows = 4;
    leaves.leavesPerCluster = 9;
    leaves.density = 1.0;
    leaves.seedOffset = 117;
    const auto leafLayout = paperweight::buildLeafClusterLayout(leaves, seed);
    expect(leafLayout == paperweight::buildLeafClusterLayout(leaves, seed) &&
               leafLayout.instances.size() == 4U * 4U * 9U,
           "leaf cluster placement, overlap order, and per-leaf attributes are stable");
    const auto& authoredLeaf = leafLayout.instances.front();
    const auto leaf = paperweight::evaluateLeafCluster(
        leaves, leafLayout, authoredLeaf.centreU, authoredLeaf.centreV);
    const auto wrappedLeaf = paperweight::evaluateLeafCluster(
        leaves, leafLayout, authoredLeaf.centreU + 1.0, authoredLeaf.centreV - 1.0);
    expect(leaf.region.key == wrappedLeaf.region.key && leaf.coverage > 0.0 &&
               leaf.region.valid &&
               std::abs(leaf.coverage - wrappedLeaf.coverage) <= 1.0e-12 &&
               std::abs(leaf.height - wrappedLeaf.height) <= 1.0e-12,
           "overlapping leaf silhouettes and height ordering wrap as one toroidal population");
    for (const auto field : std::array{
             paperweight::LeafField::fill,
             paperweight::LeafField::edge,
             paperweight::LeafField::midrib,
             paperweight::LeafField::veins,
         }) {
        leaves.field = field;
        const auto detail = paperweight::evaluateLeafCluster(
            leaves, leafLayout, authoredLeaf.centreU, authoredLeaf.centreV);
        expect(detail.coverage >= 0.0 && detail.coverage <= 1.0,
               "leaf fill, edge, midrib, and vein masks remain normalised");
    }

    auto accumulation = paperweight::OrganicAccumulationOperation{};
    const paperweight::RegionSample cavityRegion{
        42, 0.31, 0.72, 0.66, 0.18, true, 7, true};
    const auto moss = paperweight::evaluateOrganicAccumulation(
        accumulation, 0.35, cavityRegion, -0.213, 0.779, seed);
    const auto wrappedMoss = paperweight::evaluateOrganicAccumulation(
        accumulation, 0.35, cavityRegion, 0.787, 1.779, seed);
    expect(std::abs(moss.amount - wrappedMoss.amount) <= 1.0e-12 &&
               std::abs(moss.variation - wrappedMoss.variation) <= 1.0e-12 &&
               moss.amount >= 0.0 && moss.amount <= 1.0,
           "cavity-driven moss and lichen accumulation is seamless and bounded");

    auto material = paperweight::Material{};
    material.seed = seed;
    auto cellLayer = paperweight::makeOrganicCellLayer();
    std::get<paperweight::OrganicCellOperation>(cellLayer.operation) = cells;
    auto crackLayer = paperweight::makeOrganicCrackLayer();
    std::get<paperweight::OrganicCrackOperation>(crackLayer.operation) = cracks;
    auto leafLayer = paperweight::makeLeafClusterLayer();
    leaves.field = paperweight::LeafField::material;
    std::get<paperweight::LeafClusterOperation>(leafLayer.operation) = leaves;
    auto accumulationLayer = paperweight::makeOrganicAccumulationLayer();
    std::get<paperweight::OrganicAccumulationOperation>(accumulationLayer.operation) =
        accumulation;
    material.layers = {cellLayer, crackLayer, leafLayer, accumulationLayer};
    expect(!paperweight::validateMaterial(material).has_value(),
           "a composed organic material validates");
    auto invalidLeaves = material;
    std::get<paperweight::LeafClusterOperation>(
        invalidLeaves.layers[2].operation).clusterSpread = 0.7;
    expect(paperweight::validateMaterial(invalidLeaves).has_value(),
           "unbounded wrapped leaf clusters fail validation before evaluation");

    const auto compiled = paperweight::compileMaterialGraph(material);
    const auto* graph = std::get_if<paperweight::MaterialGraph>(&compiled);
    bool foundCells = false;
    bool foundCracks = false;
    bool foundLeaves = false;
    bool foundAccumulation = false;
    if (graph != nullptr) {
        for (const auto& node : graph->nodes) {
            if (const auto* generator = std::get_if<paperweight::GeneratorNode>(&node)) {
                foundCells = foundCells || std::holds_alternative<
                    paperweight::OrganicCellOperation>(generator->operation);
                foundCracks = foundCracks || std::holds_alternative<
                    paperweight::OrganicCrackOperation>(generator->operation);
                foundLeaves = foundLeaves || std::holds_alternative<
                    paperweight::LeafClusterOperation>(generator->operation);
            } else if (const auto* processing = std::get_if<paperweight::ProcessingNode>(&node)) {
                foundAccumulation = foundAccumulation || std::holds_alternative<
                    paperweight::OrganicAccumulationProcessing>(processing->operation);
            }
        }
    }
    expect(graph != nullptr && foundCells && foundCracks && foundLeaves && foundAccumulation,
           "organic generators and accumulation compile into reusable graph objects");

    const auto serialised = paperweight::serialisePmat(material);
    const auto* organicText = std::get_if<std::string>(&serialised);
    expect(organicText != nullptr &&
               organicText->find("pmat.version = 15") != std::string::npos &&
               organicText->find("leaf.vein_pairs") != std::string::npos &&
               organicText->find("organic.accumulation.kind") != std::string::npos,
           "organic structures serialise explicitly in .pmat version 14");
    if (organicText != nullptr) {
        const auto reparsed = paperweight::parsePmat(*organicText);
        expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                   std::get<paperweight::Material>(reparsed) == material,
               "organic materials round-trip exactly through .pmat version 14");
        auto premature = *organicText;
        const auto marker = premature.find("pmat.version = 15");
        premature.replace(marker, std::string("pmat.version = 15").size(),
                          "pmat.version = 13");
        const auto rejected = paperweight::parsePmat(premature);
        const auto* diagnostic = std::get_if<paperweight::ParseDiagnostic>(&rejected);
        expect(diagnostic != nullptr && diagnostic->message.find(
                   "require .pmat version 14") != std::string::npos,
               "organic fields cannot be smuggled into older .pmat versions");
    }

    for (const auto output : std::array{
             paperweight::MaterialOutput::colour,
             paperweight::MaterialOutput::height,
             paperweight::MaterialOutput::normal,
             paperweight::MaterialOutput::roughness,
         }) {
        paperweight::GenerationRequest request{
            material, 48, 40, output, std::nullopt, std::nullopt, 1};
        const auto serial = paperweight::generate(request);
        request.workerCount = 4;
        const auto parallel = paperweight::generate(request);
        const auto* serialImage = std::get_if<paperweight::Image>(&serial);
        const auto* parallelImage = std::get_if<paperweight::Image>(&parallel);
        expect(serialImage != nullptr && parallelImage != nullptr &&
                   std::equal(
                       serialImage->pixels().begin(),
                       serialImage->pixels().end(),
                       parallelImage->pixels().begin()),
               "organic output is byte-identical between serial and multi-worker evaluation");
    }
}

void testAdvancedSurfaceOperations()
{
    auto material = paperweight::Material{};
    material.seed = 4815162342ULL;
    for (const auto kind : std::array{
             paperweight::SurfacePatternKind::ridgedNoise,
             paperweight::SurfacePatternKind::bands,
             paperweight::SurfacePatternKind::rings,
             paperweight::SurfacePatternKind::scatter,
             paperweight::SurfacePatternKind::streaks,
         }) {
        paperweight::SurfacePatternOperation operation;
        operation.kind = kind;
        operation.scale = 7;
        operation.width = 0.18;
        operation.detail = 0.72;
        operation.distortion = 0.31;
        operation.variation = 0.64;
        operation.seedOffset = 91;
        const double value = paperweight::evaluateSurfacePattern(
            operation, material, -0.271, 0.683);
        expect(value >= 0.0 && value <= 1.0,
               "advanced surface patterns stay normalised");
        expect(value == paperweight::evaluateSurfacePattern(
                            operation, material, -0.271, 0.683),
               "advanced surface patterns are deterministic");
        expectNear(
            value,
            paperweight::evaluateSurfacePattern(operation, material, 0.729, 0.683),
            1.0e-12,
            "advanced surface patterns repeat exactly on x");
        expectNear(
            value,
            paperweight::evaluateSurfacePattern(operation, material, -0.271, 1.683),
            1.0e-12,
            "advanced surface patterns repeat exactly on y");
    }

    auto seeded = paperweight::SurfacePatternOperation{};
    seeded.kind = paperweight::SurfacePatternKind::ridgedNoise;
    const double firstSeed = paperweight::evaluateSurfacePattern(
        seeded, material, 0.37, 0.19);
    ++seeded.seedOffset;
    expect(firstSeed != paperweight::evaluateSurfacePattern(seeded, material, 0.37, 0.19),
           "surface seed offsets select distinct deterministic patterns");

    const paperweight::SurfaceNeighbourhood neighbourhood{
        0.25,
        0.0,
        0.5,
        0.125,
        0.875,
        0.0,
        1.0,
        0.375,
        0.625,
    };
    const auto filtered = [&neighbourhood](paperweight::SurfaceFilterKind kind) {
        return paperweight::evaluateSurfaceFilter(
            paperweight::SurfaceFilterOperation{kind, 0.02, 1.0},
            neighbourhood);
    };
    expectNear(filtered(paperweight::SurfaceFilterKind::invert), 0.75, 1.0e-12,
               "invert complements the source");
    expectNear(filtered(paperweight::SurfaceFilterKind::soften), 3.75 / 9.0, 1.0e-12,
               "soften averages the periodic neighbourhood");
    expectNear(filtered(paperweight::SurfaceFilterKind::expand), 1.0, 1.0e-12,
               "expand selects the neighbourhood maximum");
    expectNear(filtered(paperweight::SurfaceFilterKind::contract), 0.0, 1.0e-12,
               "contract selects the neighbourhood minimum");
    expectNear(filtered(paperweight::SurfaceFilterKind::edge), 1.0, 1.0e-12,
               "edge measures local range");
    for (const auto kind : std::array{
             paperweight::SurfaceFilterKind::slope,
             paperweight::SurfaceFilterKind::cavity,
             paperweight::SurfaceFilterKind::peaks,
         }) {
        const double value = filtered(kind);
        expect(value >= 0.0 && value <= 1.0,
               "neighbourhood-derived surface filters stay normalised");
    }
    expectNear(
        paperweight::evaluateSurfaceFilter(
            paperweight::SurfaceFilterOperation{
                paperweight::SurfaceFilterKind::invert, 0.02, 0.0},
            neighbourhood),
        neighbourhood.centre,
        1.0e-12,
        "zero-strength surface filters are exact no-ops");

    auto patternLayer = paperweight::makeSurfacePatternLayer(
        paperweight::SurfacePatternKind::ridgedNoise);
    auto& pattern = std::get<paperweight::SurfacePatternOperation>(patternLayer.operation);
    pattern.scale = 11;
    pattern.width = 0.14;
    pattern.detail = 0.8;
    pattern.distortion = 0.42;
    pattern.variation = 0.7;
    pattern.seedOffset = 17;
    auto filterLayer = paperweight::makeSurfaceFilterLayer(
        paperweight::SurfaceFilterKind::edge);
    auto& filter = std::get<paperweight::SurfaceFilterOperation>(filterLayer.operation);
    filter.radius = 0.013;
    filter.strength = 0.85;
    material.layers = {patternLayer, filterLayer};

    const auto compilation = paperweight::compileMaterialGraph(material);
    const auto* graph = std::get_if<paperweight::MaterialGraph>(&compilation);
    expect(graph != nullptr, "advanced surface layers compile into a material graph");
    bool foundPattern = false;
    bool foundFilter = false;
    if (graph != nullptr) {
        for (const auto& node : graph->nodes) {
            if (const auto* generator = std::get_if<paperweight::GeneratorNode>(&node)) {
                foundPattern = foundPattern ||
                    std::holds_alternative<paperweight::SurfacePatternOperation>(
                        generator->operation);
            } else if (const auto* processing =
                           std::get_if<paperweight::ProcessingNode>(&node)) {
                foundFilter = foundFilter ||
                    std::holds_alternative<paperweight::SurfaceFilterProcessing>(
                        processing->operation);
            }
        }
        const auto sample = paperweight::evaluateMaterialGraphSample(
            material,
            *graph,
            paperweight::MaterialOutput::height,
            -0.113,
            0.779);
        const auto repeat = paperweight::evaluateMaterialGraphSample(
            material,
            *graph,
            paperweight::MaterialOutput::height,
            0.887,
            1.779);
        expectNear(sample.scalar, repeat.scalar, 1.0e-12,
                   "neighbourhood filters remain seamless through the graph evaluator");
    }
    expect(foundPattern && foundFilter,
           "surface patterns compile as generators and filters compile as processors");

    const auto lowResult = paperweight::generate(
        {material, 24, 20, paperweight::MaterialOutput::height, std::nullopt, std::nullopt});
    const auto highResult = paperweight::generate(
        {material, 72, 60, paperweight::MaterialOutput::height, std::nullopt, std::nullopt});
    const auto* low = std::get_if<paperweight::Image>(&lowResult);
    const auto* high = std::get_if<paperweight::Image>(&highResult);
    bool matchingSamples = low != nullptr && high != nullptr;
    if (matchingSamples) {
        for (std::uint32_t y = 0; y < low->height() && matchingSamples; ++y) {
            for (std::uint32_t x = 0; x < low->width(); ++x) {
                if (low->row(y)[x] != high->row(y * 3 + 1)[x * 3 + 1]) {
                    matchingSamples = false;
                    break;
                }
            }
        }
    }
    expect(matchingSamples,
           "advanced surface recipes preserve matching samples across output resolutions");

    const auto serialised = paperweight::serialisePmat(material);
    const auto* text = std::get_if<std::string>(&serialised);
    expect(text != nullptr && text->find("surface.kind = ridged_noise") != std::string::npos &&
               text->find("filter.kind = edge") != std::string::npos,
           "format version 8 stores surface patterns and filters explicitly");
    if (text != nullptr) {
        const auto reparsed = paperweight::parsePmat(*text);
        expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                   std::get<paperweight::Material>(reparsed) == material,
               "advanced surface recipes round-trip through .pmat version 8 exactly");
        auto premature = *text;
        const auto marker = premature.find("pmat.version = 15");
        if (marker != std::string::npos) {
            premature.replace(marker, std::string("pmat.version = 15").size(),
                              "pmat.version = 7");
        }
        const auto prematureResult = paperweight::parsePmat(premature);
        expect(std::holds_alternative<paperweight::ParseDiagnostic>(prematureResult) &&
                   std::get<paperweight::ParseDiagnostic>(prematureResult).message.find(
                       "require .pmat version 8") != std::string::npos,
               "older .pmat versions reject v8 surface filter fields explicitly");
    }

    auto invalid = material;
    std::get<paperweight::SurfacePatternOperation>(invalid.layers.front().operation).scale = 0;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "invalid advanced surface scale is diagnosed");
    invalid = material;
    std::get<paperweight::SurfaceFilterOperation>(invalid.layers.back().operation).radius = 0.5;
    expect(paperweight::validateMaterial(invalid).has_value(),
           "invalid surface filter radius is diagnosed");
}

void testStylisedOperations()
{
    paperweight::Material material;
    const paperweight::EvaluationContext context{material, 0.25, 0.75};
    const paperweight::EvaluatedSample input{0.62, 0.12, 0.54, 0.91, 1.0, {}};

    const auto colourPosterised = paperweight::evaluateOperation(
        paperweight::PosteriseOperation{3, paperweight::ProcessingTarget::colour},
        context,
        input);
    expectNear(colourPosterised.scalar, input.scalar, 0.0,
               "colour posterisation preserves scalar structure exactly");
    expectNear(colourPosterised.red, 0.0, 0.0,
               "posterisation selects the nearest authored band");
    expectNear(colourPosterised.green, 0.5, 0.0,
               "posterisation produces a middle band");
    expectNear(colourPosterised.blue, 1.0, 0.0,
               "posterisation retains the upper endpoint");

    const auto scalarPosterised = paperweight::evaluateOperation(
        paperweight::PosteriseOperation{4, paperweight::ProcessingTarget::scalar},
        context,
        input);
    expectNear(scalarPosterised.scalar, 2.0 / 3.0, 1.0e-12,
               "scalar posterisation deliberately terraces material structure");
    expectNear(scalarPosterised.red, input.red, 0.0,
               "scalar-only posterisation preserves colour channels");

    paperweight::ColourRampOperation ramp;
    ramp.stops = {
        {0.0, {20, 28, 48, 255}},
        {0.5, {78, 132, 164, 255}},
        {1.0, {238, 218, 142, 255}},
    };
    const auto ramped = paperweight::evaluateOperation(ramp, context, input);
    expectNear(ramped.scalar, input.scalar, 0.0,
               "colour ramps preserve the source scalar exactly");
    expect(ramped.red > 78.0 / 255.0 && ramped.red < 238.0 / 255.0,
           "linear colour ramps interpolate between enclosing stops");
    ramp.mode = paperweight::ColourRampMode::stepped;
    const auto stepped = paperweight::evaluateOperation(ramp, context, input);
    expectNear(stepped.red, 78.0 / 255.0, 1.0e-12,
               "stepped colour ramps hold the preceding stop");

    paperweight::PaletteOperation palette;
    palette.colours = {{255, 0, 0, 255}, {0, 0, 255, 255}};
    const paperweight::EvaluatedSample tie{0.4, 1.0, 0.0, 1.0, 1.0, {}};
    const auto quantised = paperweight::evaluateOperation(palette, context, tie);
    expectNear(quantised.red, 1.0, 0.0,
               "palette ties select the first authored colour deterministically");
    expectNear(quantised.blue, 0.0, 0.0,
               "palette quantisation replaces arbitrary colour exactly");
    expectNear(quantised.scalar, tie.scalar, 0.0,
               "palette quantisation preserves scalar structure");

    const paperweight::SurfaceNeighbourhood neighbourhood{
        0.2, 0.18, 0.22, 0.19, 0.21, 0.2, 0.23, 0.17, 1.0};
    const auto edgeAware = paperweight::evaluateSurfaceFilter(
        paperweight::SurfaceFilterOperation{
            paperweight::SurfaceFilterKind::edgeAwareSoften,
            0.02,
            1.0,
            0.1,
            paperweight::ProcessingTarget::colourAndScalar,
        },
        neighbourhood);
    const auto ordinary = paperweight::evaluateSurfaceFilter(
        paperweight::SurfaceFilterOperation{
            paperweight::SurfaceFilterKind::soften, 0.02, 1.0},
        neighbourhood);
    expect(edgeAware < ordinary && edgeAware > 0.17 && edgeAware < 0.23,
           "edge-aware smoothing rejects an outlier while smoothing its local region");

    paperweight::Material base;
    base.seed = 424242;
    base.layers = {paperweight::makeNoiseLayer()};
    auto stylised = base;
    stylised.layers.push_back(paperweight::makePosteriseLayer());
    auto colourRampLayer = paperweight::makeColourRampLayer();
    auto& authoredRamp = std::get<paperweight::ColourRampOperation>(
        colourRampLayer.operation);
    authoredRamp.mode = paperweight::ColourRampMode::stepped;
    authoredRamp.stops = ramp.stops;
    stylised.layers.push_back(colourRampLayer);
    auto paletteLayer = paperweight::makePaletteLayer();
    std::get<paperweight::PaletteOperation>(paletteLayer.operation).colours = {
        {23, 31, 42, 255},
        {79, 121, 137, 255},
        {224, 202, 132, 255},
    };
    stylised.layers.push_back(paletteLayer);
    auto smoothLayer = paperweight::makeSurfaceFilterLayer(
        paperweight::SurfaceFilterKind::edgeAwareSoften);
    auto& smooth = std::get<paperweight::SurfaceFilterOperation>(
        smoothLayer.operation);
    smooth.radius = 0.01;
    smooth.sensitivity = 0.18;
    smooth.target = paperweight::ProcessingTarget::colour;
    stylised.layers.push_back(smoothLayer);
    auto inkLayer = paperweight::makeInkContourLayer();
    auto& ink = std::get<paperweight::InkContourOperation>(inkLayer.operation);
    ink.colour = {18, 20, 28, 220};
    ink.radius = 0.012;
    ink.threshold = 0.08;
    ink.softness = 0.04;
    stylised.layers.push_back(inkLayer);

    const auto baseColour = paperweight::generate(
        {base, 40, 32, paperweight::MaterialOutput::colour, std::nullopt, std::nullopt});
    const auto styledColour = paperweight::generate(
        {stylised, 40, 32, paperweight::MaterialOutput::colour, std::nullopt, std::nullopt});
    const auto* baseColourImage = std::get_if<paperweight::Image>(&baseColour);
    const auto* styledColourImage = std::get_if<paperweight::Image>(&styledColour);
    expect(baseColourImage != nullptr && styledColourImage != nullptr &&
               checksum(baseColourImage->pixels()) != checksum(styledColourImage->pixels()),
           "stylisation processing changes the colour output");

    for (const auto output : std::array{
             paperweight::MaterialOutput::height,
             paperweight::MaterialOutput::normal,
             paperweight::MaterialOutput::roughness,
         }) {
        const auto original = paperweight::generate(
            {base, 40, 32, output, std::nullopt, std::nullopt});
        const auto processed = paperweight::generate(
            {stylised, 40, 32, output, std::nullopt, std::nullopt});
        const auto* originalImage = std::get_if<paperweight::Image>(&original);
        const auto* processedImage = std::get_if<paperweight::Image>(&processed);
        expect(originalImage != nullptr && processedImage != nullptr &&
                   checksum(originalImage->pixels()) == checksum(processedImage->pixels()),
               "colour-only stylisation preserves every scalar-derived output byte");
    }

    const auto compiled = paperweight::compileMaterialGraph(stylised);
    const auto* graph = std::get_if<paperweight::MaterialGraph>(&compiled);
    expect(graph != nullptr, "stylisation layers compile into the reusable graph");
    if (graph != nullptr) {
        const auto sample = paperweight::evaluateMaterialGraphSample(
            stylised, *graph, paperweight::MaterialOutput::colour, -0.17, 0.43);
        const auto repeated = paperweight::evaluateMaterialGraphSample(
            stylised, *graph, paperweight::MaterialOutput::colour, 0.83, 1.43);
        expectNear(sample.red, repeated.red, 1.0e-12,
                   "stylised colour remains mathematically seamless on x and y");
        expectNear(sample.green, repeated.green, 1.0e-12,
                   "ink contours remain periodic through graph neighbourhood sampling");
    }

    const auto serialised = paperweight::serialisePmat(stylised);
    const auto* text = std::get_if<std::string>(&serialised);
    expect(text != nullptr && text->find("pmat.version = 15") != std::string::npos &&
               text->find("operation = colour_ramp") != std::string::npos &&
               text->find("operation = ink_contour") != std::string::npos,
           "stylisation serialises in the human-readable .pmat v9 format");
    if (text != nullptr) {
        const auto reparsed = paperweight::parsePmat(*text);
        expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                   std::get<paperweight::Material>(reparsed) == stylised,
               "stylised materials round-trip through .pmat v9 exactly");
    }
}

void testMaterialGraph()
{
    static_assert(std::is_aggregate_v<paperweight::GenerationRequest>);

    auto material = paperweight::Material{};
    material.layers = {
        paperweight::makeNoiseLayer(11),
        paperweight::MaterialLayer{
            true,
            1.0,
            paperweight::CompositeMode::blend,
            paperweight::LevelsOperation{0.1, 0.9, 1.2},
            {},
            {}},
        paperweight::makeSolidColourLayer({200, 120, 70, 255}),
        paperweight::makeCirclesLayer(),
    };
    material.layers[2].compositeMode = paperweight::CompositeMode::multiply;
    material.layers[2].opacity = 0.6;
    material.layers[2].transform.scaleX = 2;
    material.layers[2].mask.enabled = true;
    material.layers[2].mask.seedOffset = 81;
    material.layers[3].enabled = false;

    const auto compilation = paperweight::compileMaterialGraph(material);
    const auto* graph = std::get_if<paperweight::MaterialGraph>(&compilation);
    expect(graph != nullptr, "valid layer stacks compile into material graphs");
    if (graph == nullptr) {
        return;
    }
    expect(!paperweight::validateMaterialGraph(*graph),
           "compiled layer graphs satisfy graph validation");
    expect(graph->nodes.size() == 12,
           "layer compilation emits explicit source, processing, mask, and output nodes");

    std::array<std::size_t, 4> categoryCounts{};
    std::array<std::size_t, 4> outputCounts{};
    std::size_t mappedNodes = 0;
    for (const auto& node : graph->nodes) {
        ++categoryCounts[static_cast<std::size_t>(paperweight::graphNodeCategory(node))];
        const auto sourceLayer = std::visit(
            [](const auto& value) -> std::optional<std::size_t> {
                using Node = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Node, paperweight::OutputNode>) {
                    return std::nullopt;
                } else {
                    return value.sourceLayer;
                }
            },
            node);
        if (sourceLayer) {
            ++mappedNodes;
        }
        if (const auto* output = std::get_if<paperweight::OutputNode>(&node)) {
            ++outputCounts[paperweight::materialOutputIndex(output->output)];
        }
    }
    expect(categoryCounts[static_cast<std::size_t>(
               paperweight::GraphNodeCategory::generator)] == 3 &&
               categoryCounts[static_cast<std::size_t>(
                   paperweight::GraphNodeCategory::processing)] == 4 &&
               categoryCounts[static_cast<std::size_t>(
                   paperweight::GraphNodeCategory::mask)] == 1 &&
               categoryCounts[static_cast<std::size_t>(
                   paperweight::GraphNodeCategory::output)] == 4,
           "compiled graphs use all four explicit node categories");
    expect(std::all_of(outputCounts.begin(), outputCounts.end(), [](auto count) {
               return count == 1;
           }),
           "compiled graphs route each material output exactly once");
    expect(mappedNodes == 7,
           "compiled evaluation nodes retain their source-layer mapping");

    auto reordered = *graph;
    std::reverse(reordered.nodes.begin(), reordered.nodes.end());
    expect(!paperweight::validateMaterialGraph(reordered),
           "valid graph dependencies may use arbitrary storage order and forward references");
    reordered.nodes.emplace_back(paperweight::GeneratorNode{
        500,
        std::nullopt,
        {},
        paperweight::SolidColourOperation{{17, 23, 41, 255}},
    });
    expect(!paperweight::validateMaterialGraph(reordered),
           "disconnected working nodes are permitted for editor workflows");

    for (const auto output : paperweight::materialOutputs) {
        const auto layered = paperweight::generate(
            {material, 33, 25, output, std::nullopt, std::nullopt});
        const auto graphBacked = paperweight::generate(
            {material, 33, 25, output, *graph, std::nullopt});
        const auto* layeredImage = std::get_if<paperweight::Image>(&layered);
        const auto* graphImage = std::get_if<paperweight::Image>(&graphBacked);
        expect(layeredImage != nullptr && graphImage != nullptr &&
                   checksum(layeredImage->pixels()) == checksum(graphImage->pixels()),
               "explicit compiled graphs preserve layer output pixels");
    }

    paperweight::MaterialGraph routedGraph;
    routedGraph.nodes = {
        paperweight::GeneratorNode{
            10,
            std::nullopt,
            {},
            paperweight::SolidColourOperation{{255, 0, 0, 255}},
        },
        paperweight::GeneratorNode{
            20,
            std::nullopt,
            {},
            paperweight::SolidColourOperation{{255, 255, 255, 255}},
        },
        paperweight::MaskNode{30, std::nullopt, {}, {true, false, 7, 0.1, 0.9}},
        paperweight::ProcessingNode{
            40,
            std::nullopt,
            paperweight::CompositeProcessing{
                10,
                20,
                30,
                paperweight::CompositeMode::blend,
                1.0,
            },
        },
        paperweight::ProcessingNode{
            50,
            std::nullopt,
            paperweight::LevelsProcessing{40, {0.0, 1.0, 1.0}},
        },
        paperweight::OutputNode{60, paperweight::MaterialOutput::colour, 10},
        paperweight::OutputNode{61, paperweight::MaterialOutput::height, 20},
        paperweight::OutputNode{62, paperweight::MaterialOutput::normal, 20},
        paperweight::OutputNode{63, paperweight::MaterialOutput::roughness, 50},
    };
    expect(!paperweight::validateMaterialGraph(routedGraph),
           "direct branched graphs validate independently of layer stacks");
    const auto routedColour = paperweight::generate(
        {paperweight::Material{}, 8, 8, paperweight::MaterialOutput::colour, routedGraph, std::nullopt});
    const auto routedHeight = paperweight::generate(
        {paperweight::Material{}, 8, 8, paperweight::MaterialOutput::height, routedGraph, std::nullopt});
    const auto* routedColourImage = std::get_if<paperweight::Image>(&routedColour);
    const auto* routedHeightImage = std::get_if<paperweight::Image>(&routedHeight);
    expect(routedColourImage != nullptr &&
               routedColourImage->pixels().front() == paperweight::Rgba8{255, 0, 0, 255},
           "a graph output node can route colour to its own branch");
    expect(routedHeightImage != nullptr &&
               routedHeightImage->pixels().front() == paperweight::Rgba8{255, 255, 255, 255},
           "a graph output node can route height to a different branch");
    const auto directSample = paperweight::evaluateMaterialGraphSample(
        paperweight::Material{},
        routedGraph,
        paperweight::MaterialOutput::roughness,
        -0.23,
        0.61);
    const auto repeatedSample = paperweight::evaluateMaterialGraphSample(
        paperweight::Material{},
        routedGraph,
        paperweight::MaterialOutput::roughness,
        0.77,
        1.61);
    expectNear(directSample.scalar, repeatedSample.scalar, 1.0e-12,
               "direct material graphs remain periodic on both axes");

    auto duplicateId = routedGraph;
    std::get<paperweight::GeneratorNode>(duplicateId.nodes[1]).id = 10;
    expectGraphError(
        duplicateId,
        paperweight::GraphErrorCode::duplicateNodeId,
        "duplicate graph node identifiers are diagnosed");

    auto zeroId = routedGraph;
    std::get<paperweight::GeneratorNode>(zeroId.nodes.front()).id =
        paperweight::invalidGraphNodeId;
    expectGraphError(
        zeroId,
        paperweight::GraphErrorCode::invalidNodeId,
        "zero is reserved as the invalid graph node identifier");

    auto oversized = routedGraph;
    for (std::size_t index = oversized.nodes.size();
         index <= paperweight::GraphLimits::maximumNodes;
         ++index) {
        oversized.nodes.emplace_back(paperweight::GeneratorNode{
            static_cast<paperweight::GraphNodeId>(1000 + index),
            std::nullopt,
            {},
            paperweight::NoiseOperation{},
        });
    }
    expectGraphError(
        oversized,
        paperweight::GraphErrorCode::invalidNodeCount,
        "graphs larger than the portable node limit are rejected");

    auto missingInput = routedGraph;
    std::get<paperweight::OutputNode>(missingInput.nodes.back()).input = 999;
    expectGraphError(
        missingInput,
        paperweight::GraphErrorCode::missingInput,
        "dangling graph inputs are diagnosed");

    auto incompatibleInput = routedGraph;
    std::get<paperweight::OutputNode>(incompatibleInput.nodes[5]).input = 30;
    expectGraphError(
        incompatibleInput,
        paperweight::GraphErrorCode::incompatibleInput,
        "graph node categories constrain their input connections");

    auto missingOutput = routedGraph;
    missingOutput.nodes.pop_back();
    expectGraphError(
        missingOutput,
        paperweight::GraphErrorCode::missingOutput,
        "missing material output nodes are diagnosed");

    auto duplicateOutput = routedGraph;
    std::get<paperweight::OutputNode>(duplicateOutput.nodes.back()).output =
        paperweight::MaterialOutput::colour;
    expectGraphError(
        duplicateOutput,
        paperweight::GraphErrorCode::duplicateOutput,
        "duplicate material output routes are diagnosed");

    auto invalidParameter = routedGraph;
    std::get<paperweight::GeneratorNode>(invalidParameter.nodes.front()).transform.scaleX = 0;
    expectGraphError(
        invalidParameter,
        paperweight::GraphErrorCode::invalidParameter,
        "invalid generator parameters receive node-specific diagnostics");

    paperweight::MaterialGraph cyclicGraph;
    cyclicGraph.nodes = {
        paperweight::GeneratorNode{1, std::nullopt, {}, paperweight::NoiseOperation{}},
        paperweight::ProcessingNode{
            2,
            std::nullopt,
            paperweight::LevelsProcessing{3, {}},
        },
        paperweight::ProcessingNode{
            3,
            std::nullopt,
            paperweight::ThresholdProcessing{2, {}},
        },
        paperweight::OutputNode{4, paperweight::MaterialOutput::colour, 2},
        paperweight::OutputNode{5, paperweight::MaterialOutput::height, 2},
        paperweight::OutputNode{6, paperweight::MaterialOutput::normal, 2},
        paperweight::OutputNode{7, paperweight::MaterialOutput::roughness, 2},
    };
    expectGraphError(
        cyclicGraph,
        paperweight::GraphErrorCode::cycle,
        "directed cycles are rejected before graph evaluation");

    const auto invalidGraphGeneration = paperweight::generate(
        {paperweight::Material{},
         8,
         8,
         paperweight::MaterialOutput::colour,
         missingInput,
         std::nullopt});
    expect(std::holds_alternative<paperweight::GenerationError>(invalidGraphGeneration) &&
               std::get<paperweight::GenerationError>(invalidGraphGeneration).code ==
                   paperweight::GenerationErrorCode::invalidGraph,
           "invalid direct graphs return a structured generation error");
}

void testGenerator()
{
    const paperweight::GenerationRequest request{
        paperweight::Material{},
        48,
        32,
        paperweight::MaterialOutput::colour,
        std::nullopt,
        std::nullopt};
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
            paperweight::LevelsOperation{0.15, 0.85, 1.2},
            {},
            {}},
        paperweight::MaterialLayer{
            true,
            0.3,
            paperweight::CompositeMode::multiply,
            paperweight::SolidColourOperation{{220, 120, 80, 255}},
            {},
            {}},
    };
    layeredMaterial.layers.front().transform.scaleX = 2;
    layeredMaterial.layers.front().transform.rotation =
        paperweight::QuarterTurn::clockwise270;
    layeredMaterial.layers.front().transform.warpEnabled = true;
    layeredMaterial.layers.front().transform.warpStrength = 0.2;
    layeredMaterial.layers.front().transform.warpFrequency = 2;
    layeredMaterial.layers.back().mask.enabled = true;
    layeredMaterial.layers.back().mask.seedOffset = 47;
    layeredMaterial.layers.back().mask.inputLow = 0.1;
    layeredMaterial.layers.back().mask.inputHigh = 0.9;
    for (const auto output : std::array{
             paperweight::MaterialOutput::colour,
             paperweight::MaterialOutput::height,
             paperweight::MaterialOutput::normal,
             paperweight::MaterialOutput::roughness,
         }) {
        const paperweight::GenerationRequest layeredRequest{
            layeredMaterial, 31, 27, output, std::nullopt, std::nullopt};
        const auto layeredA = paperweight::generate(layeredRequest);
        const auto layeredB = paperweight::generate(layeredRequest);
        const auto* imageA = std::get_if<paperweight::Image>(&layeredA);
        const auto* imageB = std::get_if<paperweight::Image>(&layeredB);
        expect(imageA != nullptr && imageB != nullptr &&
                   checksum(imageA->pixels()) == checksum(imageB->pixels()),
               "every layered material output is byte-deterministic");
    }
    const auto layeredHeight = paperweight::generate(
        {layeredMaterial,
         31,
         27,
         paperweight::MaterialOutput::height,
         std::nullopt,
         std::nullopt});
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
        const paperweight::GenerationRequest representative{
            paperweight::Material{},
            width,
            height,
            paperweight::MaterialOutput::colour,
            std::nullopt,
            std::nullopt};
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

    std::size_t cancellationChecks = 0;
    const auto cancelled = paperweight::generate(
        paperweight::GenerationRequest{
            paperweight::Material{},
            128,
            128,
            paperweight::MaterialOutput::colour,
            std::nullopt,
            std::nullopt},
        [&cancellationChecks]() { return ++cancellationChecks >= 4; });
    expect(std::holds_alternative<paperweight::GenerationError>(cancelled) &&
               std::get<paperweight::GenerationError>(cancelled).code ==
                   paperweight::GenerationErrorCode::cancelled &&
               cancellationChecks == 4,
           "cooperative cancellation stops an in-progress colour generation");

    const auto cancelledBeforeStart = paperweight::generate(
        paperweight::GenerationRequest{
            paperweight::Material{},
            128,
            128,
            paperweight::MaterialOutput::normal,
            std::nullopt,
            std::nullopt},
        []() { return true; });
    expect(std::holds_alternative<paperweight::GenerationError>(cancelledBeforeStart) &&
               std::get<paperweight::GenerationError>(cancelledBeforeStart).code ==
                   paperweight::GenerationErrorCode::cancelled,
           "normal generation honours cancellation before allocating its height field");

    std::atomic_size_t parallelCancellationChecks{0};
    const auto cancelledParallel = paperweight::generate(
        paperweight::GenerationRequest{
            paperweight::Material{},
            512,
            512,
            paperweight::MaterialOutput::colour,
            std::nullopt,
            std::nullopt,
            4},
        [&parallelCancellationChecks]() {
            return parallelCancellationChecks.fetch_add(1, std::memory_order_relaxed) >= 3;
        });
    expect(
        std::holds_alternative<paperweight::GenerationError>(cancelledParallel) &&
            std::get<paperweight::GenerationError>(cancelledParallel).code ==
                paperweight::GenerationErrorCode::cancelled,
        "parallel generation cooperatively cancels without publishing a partial image");
}

void testPhysicalScale()
{
    auto material = paperweight::Material{};
    material.physicalSize = {0.96, 0.6};
    auto brickLayer = paperweight::makeBrickGridLayer();
    auto& brick = std::get<paperweight::BrickGridOperation>(brickLayer.operation);
    brick.stagger = 0.5;
    brick.softness = 0.01;
    brick.physicalDimensions = paperweight::BrickGridOperation::PhysicalDimensions{
        0.24,
        0.075,
        0.01,
    };
    material.layers = {brickLayer};

    expect(!paperweight::validateMaterial(material).has_value(),
           "metre-sized bricks validate when they divide the material repeat");
    auto incompatible = material;
    std::get<paperweight::BrickGridOperation>(incompatible.layers.front().operation)
        .physicalDimensions->widthMetres = 0.25;
    expect(paperweight::validateMaterial(incompatible).has_value(),
           "physical brick dimensions must divide the material repeat exactly");

    auto uncommonDimensions = material;
    uncommonDimensions.physicalSize = {2.53, 0.486};
    auto& uncommonBrick = std::get<paperweight::BrickGridOperation>(
        uncommonDimensions.layers.front().operation);
    uncommonBrick.physicalDimensions =
        paperweight::BrickGridOperation::PhysicalDimensions{0.23, 0.081, 0.01};
    expect(!paperweight::validateMaterial(uncommonDimensions).has_value(),
           "any positive brick size works when the repeat is derived from whole brick counts");

    const auto serialised = paperweight::serialisePmat(material);
    const auto* physicalText = std::get_if<std::string>(&serialised);
    expect(physicalText != nullptr &&
               physicalText->find("material.width = 0.96m") != std::string::npos &&
               physicalText->find("brick.width = 0.24m") != std::string::npos &&
               physicalText->find("brick.height = 0.075m") != std::string::npos &&
               physicalText->find("brick.mortar_width = 0.01m") != std::string::npos,
           "physical dimensions use explicit canonical metre suffixes");
    if (physicalText != nullptr) {
        const auto reparsed = paperweight::parsePmat(*physicalText);
        expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                   std::get<paperweight::Material>(reparsed) == material,
               "metre-sized material definitions round-trip exactly");
        auto invalidUnit = *physicalText;
        const auto width = invalidUnit.find("brick.width = 0.24m");
        if (width != std::string::npos) {
            invalidUnit.replace(
                width,
                std::string("brick.width = 0.24m").size(),
                "brick.width = 24cm");
        }
        const auto invalidUnitResult = paperweight::parsePmat(invalidUnit);
        expect(std::holds_alternative<paperweight::ParseDiagnostic>(invalidUnitResult) &&
                   std::get<paperweight::ParseDiagnostic>(invalidUnitResult).message.find(
                       "metre") != std::string::npos,
               "unsupported physical units fail explicitly");
    }

    const paperweight::GenerationRequest baseRequest{
        material,
        64,
        40,
        paperweight::MaterialOutput::colour,
        std::nullopt,
        paperweight::PhysicalSize{0.96, 0.6},
    };
    const auto baseResult = paperweight::generate(baseRequest);
    const auto* base = std::get_if<paperweight::Image>(&baseResult);
    expect(base != nullptr, "physical material coverage generates successfully");

    auto highResolutionRequest = baseRequest;
    highResolutionRequest.width = 192;
    highResolutionRequest.height = 120;
    const auto highResolutionResult = paperweight::generate(highResolutionRequest);
    const auto* highResolution = std::get_if<paperweight::Image>(&highResolutionResult);
    bool matchingWorldSamples = base != nullptr && highResolution != nullptr;
    if (matchingWorldSamples) {
        for (std::uint32_t y = 0; y < base->height() && matchingWorldSamples; ++y) {
            for (std::uint32_t x = 0; x < base->width(); ++x) {
                if (base->row(y)[x] != highResolution->row(y * 3 + 1)[x * 3 + 1]) {
                    matchingWorldSamples = false;
                    break;
                }
            }
        }
    }
    expect(matchingWorldSamples,
           "the same physical points match exactly at higher output resolution");

    auto widerCoverageRequest = baseRequest;
    widerCoverageRequest.width = 128;
    widerCoverageRequest.physicalCoverage = paperweight::PhysicalSize{1.92, 0.6};
    const auto widerCoverageResult = paperweight::generate(widerCoverageRequest);
    const auto* widerCoverage = std::get_if<paperweight::Image>(&widerCoverageResult);
    bool repeatedAtWorldScale = base != nullptr && widerCoverage != nullptr;
    if (repeatedAtWorldScale) {
        for (std::uint32_t y = 0; y < base->height() && repeatedAtWorldScale; ++y) {
            for (std::uint32_t x = 0; x < base->width(); ++x) {
                if (base->row(y)[x] != widerCoverage->row(y)[x] ||
                    base->row(y)[x] != widerCoverage->row(y)[x + base->width()]) {
                    repeatedAtWorldScale = false;
                    break;
                }
            }
        }
    }
    expect(repeatedAtWorldScale,
           "larger coverage contains more whole repeats at the same pixels-per-metre scale");

    auto invalidCoverageRequest = baseRequest;
    invalidCoverageRequest.physicalCoverage = paperweight::PhysicalSize{1.44, 0.6};
    const auto invalidCoverage = paperweight::generate(invalidCoverageRequest);
    const auto* coverageError = std::get_if<paperweight::GenerationError>(&invalidCoverage);
    expect(coverageError != nullptr &&
               coverageError->code == paperweight::GenerationErrorCode::invalidPhysicalCoverage,
           "fractional-repeat coverage is rejected instead of breaking seamlessness");

    auto invalidMaterialRequest = baseRequest;
    invalidMaterialRequest.material.physicalSize.widthMetres = 0.0;
    invalidMaterialRequest.physicalCoverage.reset();
    const auto invalidMaterial = paperweight::generate(invalidMaterialRequest);
    const auto* materialError = std::get_if<paperweight::GenerationError>(&invalidMaterial);
    expect(materialError != nullptr &&
               materialError->code == paperweight::GenerationErrorCode::invalidMaterial,
           "invalid material dimensions are reported as material errors");
}

void testReferenceTemplatesAndStylisedLighting()
{
    const auto& templates = paperweight::referenceMaterialTemplates();
    expect(templates.size() == 10, "reference catalogue contains all ten Blastard materials");

    std::vector<std::string_view> identifiers;
    for (const auto& descriptor : templates) {
        expect(!descriptor.identifier.empty() && !descriptor.displayName.empty() &&
                   !descriptor.recipeResourceName.empty() &&
                   !descriptor.referenceFileName.empty() && !descriptor.controls.empty(),
               "each reference template has identity, source, reference, and friendly controls");
        expect(std::find(identifiers.begin(), identifiers.end(), descriptor.identifier) ==
                   identifiers.end(),
               "reference template identifiers are unique");
        identifiers.push_back(descriptor.identifier);
        expect(paperweight::findReferenceMaterialTemplate(descriptor.identifier) == &descriptor,
               "reference template can be found by its stable identifier");
    }
    expect(paperweight::findReferenceMaterialTemplate("not-a-template") == nullptr,
           "unknown reference template identifiers are rejected");

    constexpr std::array<std::uint64_t, 10> bakedGoldens{
        2889411219681709879ULL,
        4589141016822059237ULL,
        10553936091438984031ULL,
        7665254725482328274ULL,
        4755012890138944442ULL,
        2312567766283003603ULL,
        1201190961296726153ULL,
        14585860657489692377ULL,
        3827243153770096578ULL,
        8534288170704184381ULL,
    };
    constexpr std::uint64_t chosenSeed = 180018;
    for (std::size_t templateIndex = 0; templateIndex < templates.size(); ++templateIndex) {
        const auto& descriptor = templates[templateIndex];
        std::ifstream file(
            std::string(descriptor.recipeResourceName) + ".pmat",
            std::ios::binary);
        const std::string text(
            std::istreambuf_iterator<char>{file},
            std::istreambuf_iterator<char>{});
        const auto parsed = paperweight::parsePmat(text);
        const auto* authored = std::get_if<paperweight::Material>(&parsed);
        expect(authored != nullptr, "reference template source parses");
        if (authored == nullptr) {
            continue;
        }

        const auto recipe = paperweight::makeMaterialRecipe(*authored);
        auto material = paperweight::instantiateMaterial(recipe, chosenSeed);
        expect(material.seed == chosenSeed && material.layers == authored->layers &&
                   material.physicalSize == authored->physicalSize,
               "seedless recipe instantiates with the caller's seed without changing its content");

        auto controlled = material;
        const auto controlError = paperweight::applyTemplateControl(
            controlled,
            descriptor.controls.front(),
            descriptor.controls.front().defaultValue);
        expect(!controlError.has_value() && !paperweight::validateMaterial(controlled).has_value(),
               "friendly template controls produce valid ordinary materials");
        expect(paperweight::applyTemplateControl(
                   controlled,
                   descriptor.controls.front(),
                   descriptor.controls.front().maximumValue + 1.0).has_value(),
               "friendly template controls reject values beyond their declared range");
        for (const auto& control : descriptor.controls) {
            auto low = material;
            auto high = material;
            const auto lowError = paperweight::applyTemplateControl(
                low, control, control.minimumValue);
            const auto highError = paperweight::applyTemplateControl(
                high, control, control.maximumValue);
            const bool controlWorks = !lowError.has_value() && !highError.has_value() &&
                low != high && low.seed == chosenSeed && high.seed == chosenSeed;
            if (!controlWorks) {
                std::cerr << "Template control failed: " << descriptor.identifier << '/'
                          << control.key << '\n';
            }
            expect(controlWorks,
                   "every friendly control changes authored properties without changing the seed");
        }

        std::array<std::optional<paperweight::Image>, 4> outputs;
        for (std::size_t outputIndex = 0;
             outputIndex < paperweight::materialOutputs.size();
             ++outputIndex) {
            const auto generated = paperweight::generate({
                material,
                32,
                32,
                paperweight::materialOutputs[outputIndex],
                std::nullopt,
                material.physicalSize,
                1,
            });
            if (const auto* image = std::get_if<paperweight::Image>(&generated)) {
                outputs[outputIndex] = *image;
            }
            expect(outputs[outputIndex].has_value(),
                   "every reference template generates every authoritative output");
        }
        if (!std::all_of(outputs.begin(), outputs.end(), [](const auto& image) {
                return image.has_value();
            })) {
            continue;
        }

        const auto baked = paperweight::bakeStylisedLighting(
            *outputs[paperweight::materialOutputIndex(paperweight::MaterialOutput::colour)],
            &*outputs[paperweight::materialOutputIndex(paperweight::MaterialOutput::height)],
            &*outputs[paperweight::materialOutputIndex(paperweight::MaterialOutput::normal)]);
        const auto* bakedImage = std::get_if<paperweight::Image>(&baked);
        expectChecksum(
            bakedImage,
            bakedGoldens[templateIndex],
            "reference template baked presentation matches its byte-exact golden checksum");

        const auto oneTile = paperweight::generate({
            material,
            24,
            24,
            paperweight::MaterialOutput::colour,
            std::nullopt,
            material.physicalSize,
            1,
        });
        const paperweight::PhysicalSize tripleCoverage{
            material.physicalSize.widthMetres * 3.0,
            material.physicalSize.heightMetres * 3.0,
        };
        const auto threeTiles = paperweight::generate({
            material,
            72,
            72,
            paperweight::MaterialOutput::colour,
            std::nullopt,
            tripleCoverage,
            4,
        });
        const auto* oneImage = std::get_if<paperweight::Image>(&oneTile);
        const auto* threeImage = std::get_if<paperweight::Image>(&threeTiles);
        bool repeatsExactly = oneImage != nullptr && threeImage != nullptr;
        if (repeatsExactly) {
            for (std::uint32_t y = 0; y < threeImage->height() && repeatsExactly; ++y) {
                for (std::uint32_t x = 0; x < threeImage->width(); ++x) {
                    if (threeImage->row(y)[x] != oneImage->row(y % 24)[x % 24]) {
                        repeatsExactly = false;
                        break;
                    }
                }
            }
        }
        expect(repeatsExactly,
               "reference template 1x1 and generated 3x3 coverage are byte-identical repeats");
    }

    paperweight::Image colour{3, 2, {160, 120, 80, 177}};
    paperweight::Image height{3, 2, {128, 128, 128, 255}};
    const auto originalColour = colour;
    const auto originalHeight = height;
    const auto heightOnly = paperweight::bakeStylisedLighting(colour, &height, nullptr);
    expect(std::holds_alternative<paperweight::Image>(heightOnly),
           "portable lighting can derive wrapped normals from height alone");
    expect(std::equal(colour.pixels().begin(), colour.pixels().end(), originalColour.pixels().begin()) &&
               std::equal(height.pixels().begin(), height.pixels().end(), originalHeight.pixels().begin()),
           "stylised lighting never mutates authoritative input maps");
    expect(std::holds_alternative<paperweight::StylisedLightingError>(
               paperweight::bakeStylisedLighting(colour, nullptr, nullptr)),
           "stylised lighting rejects a missing normal and height source");
    paperweight::StylisedLightingSettings invalidSettings;
    invalidSettings.diffuseBands = 1;
    expect(std::holds_alternative<paperweight::StylisedLightingError>(
               paperweight::bakeStylisedLighting(colour, &height, nullptr, invalidSettings)),
           "stylised lighting validates its portable settings");
}

void testPmat()
{
    constexpr std::string_view canonical =
        "# Paperweight procedural material\n"
        "pmat.version = 15\n"
        "material.type = fbm\n"
        "material.seed = 18431\n"
        "material.width = 1m\n"
        "material.height = 1m\n"
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
        const auto generated = paperweight::generate(
            {*material, 48, 32, paperweight::MaterialOutput::colour, std::nullopt, std::nullopt});
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
        expect(emberMaterial->layers.size() == 4 &&
                   emberMaterial->layers.front().transform.scaleX == 2 &&
                   emberMaterial->layers.front().transform.scaleY == 2 &&
                   emberMaterial->layers.front().transform.warpEnabled &&
                   emberMaterial->layers.front().transform.warpStrength == 0.18 &&
                   emberMaterial->layers[1].mask.enabled &&
                   emberMaterial->layers[1].mask.seedOffset == 77,
               "checked-in coloured example exercises transforms, warp, and masks");
        const auto firstEmber = paperweight::generate(
            {*emberMaterial, 37, 29, paperweight::MaterialOutput::colour, std::nullopt, std::nullopt});
        const auto secondEmber = paperweight::generate(
            {*emberMaterial, 37, 29, paperweight::MaterialOutput::colour, std::nullopt, std::nullopt});
        const auto* firstImage = std::get_if<paperweight::Image>(&firstEmber);
        const auto* secondImage = std::get_if<paperweight::Image>(&secondEmber);
        expect(firstImage != nullptr && secondImage != nullptr &&
                   std::equal(
                       firstImage->pixels().begin(),
                       firstImage->pixels().end(),
                       secondImage->pixels().begin()),
               "checked-in coloured example generates deterministically");
    }

    const auto readExample = [](const char* path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(
            std::istreambuf_iterator<char>{file},
            std::istreambuf_iterator<char>{});
    };
    const auto brickExample = paperweight::parsePmat(readExample("brick-wall.pmat"));
    const auto cobblestoneExample = paperweight::parsePmat(readExample("cobblestone.pmat"));
    const auto* brickMaterial = std::get_if<paperweight::Material>(&brickExample);
    const auto* cobblestoneMaterial = std::get_if<paperweight::Material>(&cobblestoneExample);
    struct ShowcaseGolden {
        const char* path;
        std::array<std::uint64_t, 4> checksums;
    };
    const std::array showcaseGoldens{
        ShowcaseGolden{
            "cracked-stone.pmat",
            {453475275262207741ULL, 6649105402743347056ULL,
             15147693073214695067ULL, 9368147851651855704ULL}},
        ShowcaseGolden{
            "weathered-metal.pmat",
            {13463650236222189030ULL, 9329744494422625172ULL,
             2788820147528998608ULL, 15949878050082813124ULL}},
        ShowcaseGolden{
            "mossy-pebbles.pmat",
            {8080888395645452509ULL, 16597892231079792016ULL,
             3236852364673261450ULL, 1390090805517284749ULL}},
        ShowcaseGolden{
            "knotty-wood.pmat",
            {6841059652416222609ULL, 12398796720431777282ULL,
             5991626458110427586ULL, 5389682451365612905ULL}},
        ShowcaseGolden{
            "marble-veins.pmat",
            {13633835698441808217ULL, 14785178712321867342ULL,
             15115531734586865140ULL, 15280871892137859466ULL}},
        ShowcaseGolden{
            "eroded-terrain.pmat",
            {2672928488154530846ULL, 11242585904416509996ULL,
             693934066486834851ULL, 17089152962988358180ULL}},
        ShowcaseGolden{
            "toon-dungeon.pmat",
            {8499675537848085354ULL, 13431479686308136619ULL,
             16421642948452846500ULL, 3171233496124241193ULL}},
        ShowcaseGolden{
            "painted-metal.pmat",
            {10753402355863875260ULL, 14470172278737105751ULL,
             4592657175009531340ULL, 4028349011060139012ULL}},
        ShowcaseGolden{
            "graphic-marble.pmat",
            {5673632620284603551ULL, 4843462098491275796ULL,
             13736793143684132695ULL, 5671637863622030837ULL}},
        ShowcaseGolden{
            "region-stones.pmat",
            {3145964603081166891ULL, 3288223741196907183ULL,
             12168598463037332862ULL, 5149046671055124938ULL}},
        ShowcaseGolden{
            "castle-flagstone.pmat",
            {13934266668256225620ULL, 13619053176943020104ULL,
             15508284524803890667ULL, 3521906500934769144ULL}},
        ShowcaseGolden{
            "castle-stone.pmat",
            {6734270194795994378ULL, 7845019198883664654ULL,
             17249611255219133448ULL, 15516935511163469849ULL}},
        ShowcaseGolden{
            "cel-castle-stone.pmat",
            {9844301835472419380ULL, 14356323194920482897ULL,
             14553100330569707255ULL, 16519552215408518380ULL}},
        ShowcaseGolden{
            "castle-roof.pmat",
            {4556783255243544328ULL, 17805210683419656455ULL,
             15736615163697282365ULL, 5145349734117541484ULL}},
        ShowcaseGolden{
            "cel-forest-rock.pmat",
            {15965981477031356603ULL, 11130753813595996712ULL,
             15975426126164431822ULL, 16434048416440545282ULL}},
        ShowcaseGolden{
            "sculpted-flagstone.pmat",
            {12855247387776217959ULL, 17520108617150934638ULL,
             14618687197654806408ULL, 18375968328722774428ULL}},
        ShowcaseGolden{
            "worn-masonry.pmat",
            {16536140133656320293ULL, 16075432134741654220ULL,
             14869146305499227944ULL, 8448790587330411558ULL}},
        ShowcaseGolden{
            "sculpted-roof-slate.pmat",
            {2738677515315863655ULL, 1140855840288707606ULL,
             4803683323561927404ULL, 12400867502020881374ULL}},
        ShowcaseGolden{
            "castle-window.pmat",
            {2940855742695968835ULL, 8803386190442183443ULL,
             12758935797147064591ULL, 17112417208134822739ULL}},
        ShowcaseGolden{
            "detailed-crate.pmat",
            {404163776177822420ULL, 13578606624915054771ULL,
             5825613498117947182ULL, 4779172065725943889ULL}},
        ShowcaseGolden{
            "decorative-fasteners.pmat",
            {15664040335747893123ULL, 6139743958895727491ULL,
             2665746260585875171ULL, 246543446198789635ULL}},
        ShowcaseGolden{
            "masonry-corner-variation.pmat",
            {8726499270484726471ULL, 13431419699875211584ULL,
             18286112188872500392ULL, 5592922460523597485ULL}},
        ShowcaseGolden{
            "cel-courtyard-gravel.pmat",
            {7061828134351751594ULL, 13758502729633312832ULL,
             15922897625282121585ULL, 1380847107545716709ULL}},
        ShowcaseGolden{
            "scattered-debris.pmat",
            {3862566863605923041ULL, 11710106090574945179ULL,
             15679416446856954908ULL, 325139394007051333ULL}},
        ShowcaseGolden{
            "foliage-foundation.pmat",
            {6813157508644990685ULL, 2005882756118611684ULL,
             7502967856720897219ULL, 13905972822234435030ULL}},
        ShowcaseGolden{
            "cel-forest-bark.pmat",
            {2509963531123175871ULL, 3988434259909993061ULL,
             8627026506781116848ULL, 5772535463161954292ULL}},
        ShowcaseGolden{
            "castle-foliage.pmat",
            {276924794914850489ULL, 3285099816653324683ULL,
             18331613028805405465ULL, 6658970271216751497ULL}},
    };
    constexpr std::array outputs{
        paperweight::MaterialOutput::colour,
        paperweight::MaterialOutput::height,
        paperweight::MaterialOutput::normal,
        paperweight::MaterialOutput::roughness,
    };
    for (const auto& showcase : showcaseGoldens) {
        const auto parsedShowcase = paperweight::parsePmat(readExample(showcase.path));
        const auto* showcaseMaterial = std::get_if<paperweight::Material>(&parsedShowcase);
        expect(showcaseMaterial != nullptr,
               "checked-in advanced surface showcase parses");
        if (showcaseMaterial != nullptr) {
            for (std::size_t outputIndex = 0; outputIndex < outputs.size(); ++outputIndex) {
                paperweight::GenerationRequest request{
                    *showcaseMaterial,
                    32,
                    32,
                    outputs[outputIndex],
                    std::nullopt,
                    std::nullopt,
                    1};
                const auto serial = paperweight::generate(request);
                request.workerCount = 4;
                const auto parallel = paperweight::generate(request);
                const auto* image = std::get_if<paperweight::Image>(&serial);
                const auto* parallelImage = std::get_if<paperweight::Image>(&parallel);
                expectChecksum(
                    image,
                    showcase.checksums[outputIndex],
                    "showcase output matches its byte-exact golden checksum");
                expect(
                    image != nullptr && parallelImage != nullptr &&
                        std::equal(
                            image->pixels().begin(),
                            image->pixels().end(),
                            parallelImage->pixels().begin()),
                    "serial and four-worker showcase output is byte-identical");
            }
        }
    }
    expect(brickMaterial != nullptr && !brickMaterial->layers.empty() &&
               std::holds_alternative<paperweight::BrickGridOperation>(
                   brickMaterial->layers.front().operation),
           "checked-in brick-wall example uses the brick-grid generator");
    if (brickMaterial != nullptr) {
        const auto& brick = std::get<paperweight::BrickGridOperation>(
            brickMaterial->layers.front().operation);
        expect(brick.physicalDimensions &&
                   brick.physicalDimensions->widthMetres == 0.24 &&
                   brick.physicalDimensions->heightMetres == 0.075 &&
                   brick.physicalDimensions->mortarMetres == 0.01,
               "checked-in brick wall uses metre-sized bricks and mortar");
        expect(brickMaterial->physicalSize == paperweight::PhysicalSize{1.92, 0.45},
               "checked-in brick wall declares its physical repeat size");

        auto legacyBrickMaterial = paperweight::Material{};
        legacyBrickMaterial.layers = {paperweight::makeBrickGridLayer()};
        auto versionFourBrick = std::get<std::string>(
            paperweight::serialisePmat(legacyBrickMaterial));
        const auto versionMarkerPosition = versionFourBrick.find("pmat.version = 15");
        expect(versionMarkerPosition != std::string::npos,
               "current brick fixture declares format version 12");
        if (versionMarkerPosition != std::string::npos) {
            versionFourBrick.replace(
                versionMarkerPosition,
                std::string("pmat.version = 15").size(),
                "pmat.version = 4");
        }
        for (const auto& field : {
                 std::string("material.width = 1m\n"),
                 std::string("material.height = 1m\n"),
                 std::string("layer.0.brick.sizing = relative\n")}) {
            const auto position = versionFourBrick.find(field);
            if (position != std::string::npos) {
                versionFourBrick.erase(position, field.size());
            }
        }
        const auto mortarSpacePosition = versionFourBrick.find(
            "layer.0.brick.mortar_space = cell\n");
        expect(mortarSpacePosition != std::string::npos,
               "current brick fixture contains its mortar-space declaration");
        if (mortarSpacePosition != std::string::npos) {
            auto prematureMortarSpace = versionFourBrick;
            const auto prematureResult = paperweight::parsePmat(prematureMortarSpace);
            expect(std::holds_alternative<paperweight::ParseDiagnostic>(prematureResult) &&
                       std::get<paperweight::ParseDiagnostic>(prematureResult).message.find(
                           "requires .pmat version 5") != std::string::npos,
                   "format version 4 rejects the version-5 mortar-space field");

            versionFourBrick.erase(
                mortarSpacePosition,
                std::string("layer.0.brick.mortar_space = cell\n").size());
            const auto migrated = paperweight::parsePmat(versionFourBrick);
            expect(std::holds_alternative<paperweight::Material>(migrated) &&
                       std::get<paperweight::Material>(migrated) == legacyBrickMaterial,
                   "format version 4 bricks migrate to cell-relative mortar exactly");
        }
    }
    expect(cobblestoneMaterial != nullptr && !cobblestoneMaterial->layers.empty() &&
               std::holds_alternative<paperweight::WorleyCellsOperation>(
                   cobblestoneMaterial->layers.front().operation),
           "checked-in cobblestone example uses the Worley-cell generator");
    if (brickMaterial != nullptr && cobblestoneMaterial != nullptr) {
        const auto brickA = paperweight::generate(
            {*brickMaterial, 41, 37, paperweight::MaterialOutput::colour, std::nullopt, std::nullopt});
        const auto brickB = paperweight::generate(
            {*brickMaterial, 41, 37, paperweight::MaterialOutput::colour, std::nullopt, std::nullopt});
        const auto cobbleA = paperweight::generate(
            {*cobblestoneMaterial,
             41,
             37,
             paperweight::MaterialOutput::colour,
             std::nullopt,
             std::nullopt});
        const auto cobbleB = paperweight::generate(
            {*cobblestoneMaterial,
             41,
             37,
             paperweight::MaterialOutput::colour,
             std::nullopt,
             std::nullopt});
        const auto* brickImageA = std::get_if<paperweight::Image>(&brickA);
        const auto* brickImageB = std::get_if<paperweight::Image>(&brickB);
        const auto* cobbleImageA = std::get_if<paperweight::Image>(&cobbleA);
        const auto* cobbleImageB = std::get_if<paperweight::Image>(&cobbleB);
        expect(brickImageA != nullptr && brickImageB != nullptr &&
                   checksum(brickImageA->pixels()) == checksum(brickImageB->pixels()),
               "checked-in brick-wall example generates deterministically");
        expect(cobbleImageA != nullptr && cobbleImageB != nullptr &&
                   checksum(cobbleImageA->pixels()) == checksum(cobbleImageB->pixels()),
               "checked-in cobblestone example generates deterministically");
        expect(brickImageA != nullptr && cobbleImageA != nullptr &&
                   checksum(brickImageA->pixels()) != checksum(cobbleImageA->pixels()),
               "brick and cobblestone examples produce distinct structural materials");
    }

    auto layeredRoundTrip = paperweight::Material{};
    layeredRoundTrip.layers = {
        paperweight::makeNoiseLayer(7),
        paperweight::MaterialLayer{
            false,
            0.35,
            paperweight::CompositeMode::add,
            paperweight::SolidColourOperation{{12, 34, 56, 78}},
            {},
            {}},
        paperweight::MaterialLayer{
            true,
            0.8,
            paperweight::CompositeMode::multiply,
            paperweight::LevelsOperation{0.15, 0.9, 1.25},
            {},
            {}},
        paperweight::MaterialLayer{
            true,
            0.6,
            paperweight::CompositeMode::blend,
            paperweight::ThresholdOperation{0.42},
            {},
            {}},
    };
    layeredRoundTrip.layers.front().transform = paperweight::CoordinateTransform{
        3,
        2,
        0.125,
        -0.375,
        paperweight::QuarterTurn::clockwise90,
        true,
        0.27,
        4,
        9821,
    };
    layeredRoundTrip.layers[1].mask = paperweight::LayerMask{
        true,
        true,
        771,
        0.2,
        0.75,
    };
    auto structuralRoundTrip = paperweight::Material{};
    structuralRoundTrip.layers = {
        paperweight::MaterialLayer{
            true, 1.0, paperweight::CompositeMode::blend,
            paperweight::BrickGridOperation{
                9,
                6,
                0.01,
                0.5,
                0.015,
                paperweight::BrickMortarSpace::texture,
                std::nullopt}, {}, {}},
        paperweight::MaterialLayer{
            true, 0.8, paperweight::CompositeMode::add,
            paperweight::TileGridOperation{5, 7, 0.09, 0.025}, {}, {}},
        paperweight::MaterialLayer{
            true, 0.7, paperweight::CompositeMode::multiply,
            paperweight::WorleyCellsOperation{8, 5, 0.9, 0.31, 71}, {}, {}},
        paperweight::MaterialLayer{
            true, 0.6, paperweight::CompositeMode::blend,
            paperweight::RandomCellsOperation{11, 13, 81}, {}, {}},
        paperweight::MaterialLayer{
            true, 0.5, paperweight::CompositeMode::add,
            paperweight::LinesOperation{
                paperweight::LineDirection::horizontal, 12, 0.16, 0.03}, {}, {}},
        paperweight::MaterialLayer{
            true, 0.4, paperweight::CompositeMode::multiply,
            paperweight::RectanglesOperation{3, 9, 0.63, 0.42, 0.04}, {}, {}},
        paperweight::MaterialLayer{
            true, 0.3, paperweight::CompositeMode::blend,
            paperweight::CirclesOperation{7, 4, 0.28, 0.05}, {}, {}},
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
            {},
            {},
            std::nullopt},
        layeredRoundTrip,
        structuralRoundTrip,
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
                const auto directImage = paperweight::generate(
                    {candidate,
                     23,
                     17,
                     paperweight::MaterialOutput::colour,
                     std::nullopt,
                     std::nullopt});
                const auto fileImage = paperweight::generate(
                    {*reparsed,
                     23,
                     17,
                     paperweight::MaterialOutput::colour,
                     std::nullopt,
                     std::nullopt});
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

    constexpr std::string_view versionTwo =
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
        "layers.count = 1\n"
        "layer.0.enabled = true\n"
        "layer.0.operation = noise\n"
        "layer.0.composite = blend\n"
        "layer.0.opacity = 1\n"
        "layer.0.noise.seed_offset = 0\n";
    const auto versionTwoResult = paperweight::parsePmat(versionTwo);
    const auto* versionTwoMaterial = std::get_if<paperweight::Material>(&versionTwoResult);
    expect(versionTwoMaterial != nullptr && versionTwoMaterial->layers.size() == 1 &&
               versionTwoMaterial->layers.front().transform == paperweight::CoordinateTransform{} &&
               versionTwoMaterial->layers.front().mask == paperweight::LayerMask{},
           "v0.0.3 .pmat files migrate to identity transforms and disabled masks");
    if (versionTwoMaterial != nullptr) {
        const auto generated = paperweight::generate(
            {*versionTwoMaterial,
             48,
             32,
             paperweight::MaterialOutput::colour,
             std::nullopt,
             std::nullopt});
        const auto* image = std::get_if<paperweight::Image>(&generated);
        expect(image != nullptr && checksum(image->pixels()) == 4981563472745378647ULL,
               "v0.0.3 .pmat migration preserves its exact generated pixels");
    }

    std::string versionThree(versionTwo);
    versionThree.replace(
        versionThree.find("pmat.version = 2"),
        std::string("pmat.version = 2").size(),
        "pmat.version = 3");
    versionThree +=
        "layer.0.transform.scale_x = 1\n"
        "layer.0.transform.scale_y = 1\n"
        "layer.0.transform.offset_x = 0\n"
        "layer.0.transform.offset_y = 0\n"
        "layer.0.transform.rotation = 0\n"
        "layer.0.warp.enabled = false\n"
        "layer.0.warp.strength = 0\n"
        "layer.0.warp.frequency = 1\n"
        "layer.0.warp.seed_offset = 0\n"
        "layer.0.mask.enabled = false\n"
        "layer.0.mask.inverted = false\n"
        "layer.0.mask.seed_offset = 0\n"
        "layer.0.mask.input_low = 0\n"
        "layer.0.mask.input_high = 1\n";
    const auto versionThreeResult = paperweight::parsePmat(versionThree);
    const auto* versionThreeMaterial = std::get_if<paperweight::Material>(&versionThreeResult);
    expect(versionThreeMaterial != nullptr && versionThreeMaterial->layers.size() == 1,
           "v0.0.4 .pmat files remain readable without structural parameters");
    if (versionThreeMaterial != nullptr) {
        const auto generated = paperweight::generate(
            {*versionThreeMaterial,
             48,
             32,
             paperweight::MaterialOutput::colour,
             std::nullopt,
             std::nullopt});
        const auto* image = std::get_if<paperweight::Image>(&generated);
        expect(image != nullptr && checksum(image->pixels()) == 4981563472745378647ULL,
               "v0.0.4 .pmat compatibility preserves its exact generated pixels");
    }
    const auto prematureStructural = paperweight::parsePmat(
        versionThree + "layer.0.brick.columns = 6\n");
    expect(std::holds_alternative<paperweight::ParseDiagnostic>(prematureStructural) &&
               std::get<paperweight::ParseDiagnostic>(prematureStructural).message.find(
                   "require .pmat version 4") != std::string::npos,
           "format version 3 rejects structural-generator fields explicitly");

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

    expectDiagnostic("pmat.version = 16\n", 1, "unsupported");
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
        19,
        "exceeds layers.count");
    expectDiagnostic(
        std::string(versionTwo) + "layer.0.transform.scale_x = 2\n",
        21,
        "require .pmat version 3");

    auto missingV3Field = std::get<std::string>(
        paperweight::serialisePmat(paperweight::Material{
            18431, 4, 5, 2, 0.5, {0, 0, 0, 255}, {255, 255, 255, 255},
            1.0, 0.25, 0.85, {paperweight::makeNoiseLayer()}, {}, std::nullopt}));
    const auto maskHighPosition = missingV3Field.find("layer.0.mask.input_high = 1\n");
    expect(maskHighPosition != std::string::npos, "v3 fixture contains its mask high field");
    if (maskHighPosition != std::string::npos) {
        const auto maskHighLine = static_cast<std::size_t>(std::count(
            missingV3Field.begin(),
            missingV3Field.begin() + static_cast<std::ptrdiff_t>(maskHighPosition),
            '\n')) + 1;
        missingV3Field.erase(
            maskHighPosition,
            std::string("layer.0.mask.input_high = 1\n").size());
        expectDiagnostic(missingV3Field, maskHighLine + 1, "mask.input_high");
    }

    auto invalidBrickMaterial = paperweight::Material{};
    invalidBrickMaterial.layers = {paperweight::makeBrickGridLayer()};
    auto invalidBrickText = std::get<std::string>(
        paperweight::serialisePmat(invalidBrickMaterial));
    const auto brickColumnsPosition = invalidBrickText.find("layer.0.brick.columns = 6");
    expect(brickColumnsPosition != std::string::npos,
           "brick fixture contains its column count");
    if (brickColumnsPosition != std::string::npos) {
        const auto brickColumnsLine = static_cast<std::size_t>(std::count(
            invalidBrickText.begin(),
            invalidBrickText.begin() + static_cast<std::ptrdiff_t>(brickColumnsPosition),
            '\n')) + 1;
        invalidBrickText.replace(
            brickColumnsPosition,
            std::string("layer.0.brick.columns = 6").size(),
            "layer.0.brick.columns = 0");
        expectDiagnostic(invalidBrickText, brickColumnsLine, "between 1 and 64");
    }

    auto invalidLevelsMaterial = paperweight::Material{};
    invalidLevelsMaterial.layers = {paperweight::makeLevelsLayer()};
    auto invalidLevelsText = std::get<std::string>(
        paperweight::serialisePmat(invalidLevelsMaterial));
    const auto highPosition = invalidLevelsText.find("layer.0.levels.input_high = 1");
    expect(highPosition != std::string::npos, "levels fixture contains its high input");
    if (highPosition != std::string::npos) {
        const auto highLine = static_cast<std::size_t>(std::count(
            invalidLevelsText.begin(),
            invalidLevelsText.begin() + static_cast<std::ptrdiff_t>(highPosition),
            '\n')) + 1;
        invalidLevelsText.replace(
            highPosition,
            std::string("layer.0.levels.input_high = 1").size(),
            "layer.0.levels.input_high = 0");
        expectDiagnostic(invalidLevelsText, highLine, "greater than input low");
    }

    auto invalidThresholdMaterial = paperweight::Material{};
    invalidThresholdMaterial.layers = {paperweight::makeThresholdLayer()};
    auto invalidThresholdText = std::get<std::string>(
        paperweight::serialisePmat(invalidThresholdMaterial));
    const auto thresholdPosition = invalidThresholdText.find("layer.0.threshold.value = 0.5");
    expect(thresholdPosition != std::string::npos, "threshold fixture contains its value");
    if (thresholdPosition != std::string::npos) {
        const auto thresholdLine = static_cast<std::size_t>(std::count(
            invalidThresholdText.begin(),
            invalidThresholdText.begin() + static_cast<std::ptrdiff_t>(thresholdPosition),
            '\n')) + 1;
        invalidThresholdText.replace(
            thresholdPosition,
            std::string("layer.0.threshold.value = 0.5").size(),
            "layer.0.threshold.value = 1.5");
        expectDiagnostic(invalidThresholdText, thresholdLine, "between 0 and 1");
    }

    auto invalid = paperweight::Material{};
    invalid.gain = 2.0;
    expect(std::holds_alternative<paperweight::SerialisationError>(
               paperweight::serialisePmat(invalid)),
           "invalid materials are not serialised");
}

void testMaterialIdentityAndLibrary()
{
    paperweight::Material identified;
    identified.metadata = paperweight::MaterialMetadata{
        "01234567-89ab-cdef-0123-456789abcdef",
        "Dungeon Flagstone",
        "Hand-cut stone for damp corridors",
        "Masonry",
        {"stone", "dungeon", "seamless"},
    };
    expect(!paperweight::validateMaterial(identified),
           "canonical material identity and metadata validate");
    expect(paperweight::isCanonicalMaterialUid(identified.metadata->uid),
           "canonical lowercase UUIDs are recognised");
    expect(!paperweight::isCanonicalMaterialUid("01234567-89AB-CDEF-0123-456789ABCDEF"),
           "uppercase UUID text is rejected to preserve one canonical spelling");
    const auto recipe = paperweight::makeMaterialRecipe(identified);
    expect(!paperweight::instantiateMaterial(recipe, 99).metadata,
           "seedless templates deliberately do not copy library identity");

    const auto serialised = paperweight::serialisePmat(identified);
    const auto* text = std::get_if<std::string>(&serialised);
    expect(text != nullptr && text->find("pmat.version = 15") != std::string::npos &&
               text->find("material.uid = 01234567-89ab-cdef-0123-456789abcdef") !=
                   std::string::npos &&
               text->find("material.name = Dungeon Flagstone") != std::string::npos &&
               text->find("material.tags = stone, dungeon, seamless") != std::string::npos,
           ".pmat version 15 writes identity and metadata readably");
    if (text != nullptr) {
        const auto reparsed = paperweight::parsePmat(*text);
        expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                   std::get<paperweight::Material>(reparsed) == identified,
               "material identity and metadata round-trip exactly");

        auto oldVersion = *text;
        const auto version = oldVersion.find("pmat.version = 15");
        oldVersion.replace(version, std::string("pmat.version = 15").size(),
                           "pmat.version = 14");
        const auto rejected = paperweight::parsePmat(oldVersion);
        const auto* diagnostic = std::get_if<paperweight::ParseDiagnostic>(&rejected);
        expect(diagnostic != nullptr &&
                   diagnostic->message.find("require .pmat version 15") != std::string::npos,
               "identity metadata cannot be smuggled into older .pmat versions");
    }

    auto withoutIdentity = identified;
    withoutIdentity.metadata.reset();
    const auto identifiedImage = paperweight::generate({
        identified, 48, 32, paperweight::MaterialOutput::colour, std::nullopt, std::nullopt});
    const auto anonymousImage = paperweight::generate({
        withoutIdentity, 48, 32, paperweight::MaterialOutput::colour, std::nullopt, std::nullopt});
    const auto* identifiedPixels = std::get_if<paperweight::Image>(&identifiedImage);
    const auto* anonymousPixels = std::get_if<paperweight::Image>(&anonymousImage);
    expect(identifiedPixels != nullptr && anonymousPixels != nullptr &&
               std::equal(
                   identifiedPixels->pixels().begin(),
                   identifiedPixels->pixels().end(),
                   anonymousPixels->pixels().begin()),
           "library identity has no effect on generated pixels");

    auto second = identified;
    second.metadata->uid = "11111111-2222-3333-4444-555555555555";
    second.metadata->name = "Castle Roof";
    second.metadata->category = "Roofing";
    const auto secondText = std::get<std::string>(paperweight::serialisePmat(second));

    auto duplicate = identified;
    duplicate.metadata->name = "Duplicate Flagstone";
    const auto duplicateText = std::get<std::string>(paperweight::serialisePmat(duplicate));
    const auto anonymousText = std::get<std::string>(paperweight::serialisePmat(withoutIdentity));

    const auto identifiedText = text != nullptr ? *text : std::string{};
    const std::array sources{
        paperweight::MaterialLibrarySource{"roof/castle-roof.pmat", secondText},
        paperweight::MaterialLibrarySource{"stone/flagstone.pmat", identifiedText},
        paperweight::MaterialLibrarySource{"broken.pmat", "not a pmat"},
        paperweight::MaterialLibrarySource{"legacy/anonymous.pmat", anonymousText},
        paperweight::MaterialLibrarySource{"stone/duplicate.pmat", duplicateText},
    };
    const auto index = paperweight::indexMaterialLibrary(sources);
    expect(index.entries().size() == 4 && index.diagnostics().size() == 5,
           "library indexing preserves parseable siblings and reports every problem");
    expect(index.entries().front().path == "legacy/anonymous.pmat" &&
               index.entries().back().path == "stone/flagstone.pmat",
           "library entries have stable path ordering independent of discovery order");
    expect(index.findByUid(second.metadata->uid) != nullptr &&
               index.findByUid(identified.metadata->uid) == nullptr,
           "UID lookup accepts unique identities and rejects ambiguous duplicates");

    auto reversedSources = sources;
    std::reverse(reversedSources.begin(), reversedSources.end());
    const auto reversedIndex = paperweight::indexMaterialLibrary(reversedSources);
    expect(std::equal(
               index.entries().begin(), index.entries().end(), reversedIndex.entries().begin()) &&
               std::equal(
                   index.diagnostics().begin(),
                   index.diagnostics().end(),
                   reversedIndex.diagnostics().begin()),
           "library results are byte-stable when filesystem discovery order changes");
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
    testMasksAndWarping();
    testStructuralGenerators();
    testRegionAttributes();
    testCourseLayouts();
    testRegionSurfaceSculpting();
    testShapePrimitivesAndLattices();
    testDeterministicScatter();
    testOrganicStructures();
    testAdvancedSurfaceOperations();
    testStylisedOperations();
    testMaterialGraph();
    testGenerator();
    testPhysicalScale();
    testReferenceTemplatesAndStylisedLighting();
    testPmat();
    testMaterialIdentityAndLibrary();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All Paperweight tests passed\n";
    return 0;
}
