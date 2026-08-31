#include <paperweight/generator.hpp>
#include <paperweight/hash.hpp>
#include <paperweight/image.hpp>
#include <paperweight/evaluation.hpp>
#include <paperweight/graph.hpp>
#include <paperweight/layer.hpp>
#include <paperweight/material.hpp>
#include <paperweight/noise.hpp>
#include <paperweight/pmat.hpp>
#include <paperweight/structural.hpp>
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
    constexpr paperweight::Version expected{0, 0, 7};
    static_assert(paperweight::currentVersion == expected);
    expect(paperweight::versionString() == "0.0.7", "version string is 0.0.7");
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

void testPmat()
{
    constexpr std::string_view canonical =
        "# Paperweight procedural material\n"
        "pmat.version = 6\n"
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
        const auto versionMarkerPosition = versionFourBrick.find("pmat.version = 6");
        expect(versionMarkerPosition != std::string::npos,
               "current brick fixture declares format version 6");
        if (versionMarkerPosition != std::string::npos) {
            versionFourBrick.replace(
                versionMarkerPosition,
                std::string("pmat.version = 6").size(),
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
            {}},
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

    expectDiagnostic("pmat.version = 7\n", 1, "unsupported");
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
            1.0, 0.25, 0.85, {paperweight::makeNoiseLayer()}, {}}));
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
    testMaterialGraph();
    testGenerator();
    testPhysicalScale();
    testPmat();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All Paperweight tests passed\n";
    return 0;
}
