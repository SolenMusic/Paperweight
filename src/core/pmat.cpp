#include <paperweight/pmat.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace paperweight {
namespace {

enum class Field : std::size_t {
    version,
    type,
    seed,
    uid,
    name,
    description,
    category,
    tags,
    physicalWidth,
    physicalHeight,
    reliefDepth,
    lowColour,
    highColour,
    frequency,
    octaves,
    lacunarity,
    gain,
    normalStrength,
    roughnessLow,
    roughnessHigh,
    metalnessLow,
    metalnessHigh,
    dielectricIor,
    coatingLow,
    coatingHigh,
    occlusionLow,
    occlusionHigh,
    clearCoatLow,
    clearCoatHigh,
    clearCoatRoughnessLow,
    clearCoatRoughnessHigh,
    emissiveIntensity,
    anisotropyStrength,
    anisotropyRotation,
    layerCount,
    count,
};

constexpr std::array<std::string_view, static_cast<std::size_t>(Field::count)> fieldKeys{
    "pmat.version",
    "material.type",
    "material.seed",
    "material.uid",
    "material.name",
    "material.description",
    "material.category",
    "material.tags",
    "material.width",
    "material.height",
    "surface.relief_depth",
    "colour.low",
    "colour.high",
    "noise.frequency",
    "noise.octaves",
    "noise.lacunarity",
    "noise.gain",
    "normal.strength",
    "roughness.low",
    "roughness.high",
    "metalness.low",
    "metalness.high",
    "surface.ior",
    "coating.low",
    "coating.high",
    "occlusion.low",
    "occlusion.high",
    "clearcoat.low",
    "clearcoat.high",
    "clearcoat.roughness_low",
    "clearcoat.roughness_high",
    "emissive.intensity",
    "anisotropy.strength",
    "anisotropy.rotation",
    "layers.count",
};

enum class OperationKind {
    noise,
    solidColour,
    levels,
    threshold,
    brickGrid,
    tileGrid,
    worleyCells,
    randomCells,
    lines,
    rectangles,
    circles,
    surfacePattern,
    surfaceFilter,
    posterise,
    colourRamp,
    palette,
    inkContour,
    regionField,
    courseLayout,
    regionSurface,
    shape,
    shapeBoolean,
    lattice,
    scatter,
    organicCells,
    organicCracks,
    leafCluster,
    organicAccumulation,
    surfaceValue,
    textile,
    regionAttachment,
};

enum class BrickSizing {
    relative,
    physical,
};

template<typename Value>
struct ParsedValue {
    std::optional<Value> value;
    std::size_t line{};
    std::size_t column{};
};

struct ScatterPopulationBuilder {
    ParsedValue<double> weight;
    ParsedValue<double> minimumScale;
    ParsedValue<double> maximumScale;
    ParsedValue<double> minimumAspect;
    ParsedValue<double> maximumAspect;
    ParsedValue<double> minimumRotation;
    ParsedValue<double> maximumRotation;
    ParsedValue<Rgba8> lowColour;
    ParsedValue<Rgba8> highColour;
    ParsedValue<double> minimumHeight;
    ParsedValue<double> maximumHeight;
    ParsedValue<double> minimumRoughness;
    ParsedValue<double> maximumRoughness;
};

struct ScatterMaskBuilder {
    ParsedValue<bool> enabled;
    ParsedValue<bool> inverted;
    ParsedValue<std::uint32_t> frequency;
    ParsedValue<double> inputLow;
    ParsedValue<double> inputHigh;
    ParsedValue<std::uint64_t> seedOffset;
};

struct LayerBuilder {
    ParsedValue<bool> enabled;
    ParsedValue<double> opacity;
    ParsedValue<CompositeMode> compositeMode;
    ParsedValue<OperationKind> operation;
    ParsedValue<LayerOutputRouting> outputs;
    ParsedValue<std::uint64_t> seedOffset;
    ParsedValue<Rgba8> solidColour;
    ParsedValue<double> surfaceValue;
    ParsedValue<double> levelsLow;
    ParsedValue<double> levelsHigh;
    ParsedValue<double> levelsGamma;
    ParsedValue<double> threshold;
    ParsedValue<std::uint32_t> scaleX;
    ParsedValue<std::uint32_t> scaleY;
    ParsedValue<double> offsetX;
    ParsedValue<double> offsetY;
    ParsedValue<QuarterTurn> rotation;
    ParsedValue<bool> warpEnabled;
    ParsedValue<double> warpStrength;
    ParsedValue<std::uint32_t> warpFrequency;
    ParsedValue<std::uint64_t> warpSeedOffset;
    ParsedValue<bool> maskEnabled;
    ParsedValue<bool> maskInverted;
    ParsedValue<std::uint64_t> maskSeedOffset;
    ParsedValue<double> maskLow;
    ParsedValue<double> maskHigh;
    ParsedValue<std::uint32_t> brickColumns;
    ParsedValue<std::uint32_t> brickRows;
    ParsedValue<double> brickMortar;
    ParsedValue<BrickMortarSpace> brickMortarSpace;
    ParsedValue<BrickSizing> brickSizing;
    ParsedValue<double> brickWidthMetres;
    ParsedValue<double> brickHeightMetres;
    ParsedValue<double> brickMortarMetres;
    ParsedValue<double> brickStagger;
    ParsedValue<double> brickSoftness;
    ParsedValue<std::uint32_t> tileColumns;
    ParsedValue<std::uint32_t> tileRows;
    ParsedValue<double> tileGrout;
    ParsedValue<double> tileSoftness;
    ParsedValue<std::uint32_t> worleyColumns;
    ParsedValue<std::uint32_t> worleyRows;
    ParsedValue<double> worleyJitter;
    ParsedValue<double> worleyEdgeWidth;
    ParsedValue<std::uint64_t> worleySeedOffset;
    ParsedValue<std::uint32_t> randomColumns;
    ParsedValue<std::uint32_t> randomRows;
    ParsedValue<std::uint64_t> randomSeedOffset;
    ParsedValue<LineDirection> lineDirection;
    ParsedValue<std::uint32_t> lineCount;
    ParsedValue<double> lineWidth;
    ParsedValue<double> lineSoftness;
    ParsedValue<std::uint32_t> rectangleColumns;
    ParsedValue<std::uint32_t> rectangleRows;
    ParsedValue<double> rectangleWidth;
    ParsedValue<double> rectangleHeight;
    ParsedValue<double> rectangleSoftness;
    ParsedValue<std::uint32_t> circleColumns;
    ParsedValue<std::uint32_t> circleRows;
    ParsedValue<double> circleRadius;
    ParsedValue<double> circleSoftness;
    ParsedValue<SurfacePatternKind> surfaceKind;
    ParsedValue<std::uint32_t> surfaceScale;
    ParsedValue<double> surfaceWidth;
    ParsedValue<double> surfaceDetail;
    ParsedValue<double> surfaceDistortion;
    ParsedValue<double> surfaceVariation;
    ParsedValue<std::uint64_t> surfaceSeedOffset;
    ParsedValue<SurfaceFilterKind> filterKind;
    ParsedValue<double> filterRadius;
    ParsedValue<double> filterStrength;
    ParsedValue<double> filterSensitivity;
    ParsedValue<ProcessingTarget> filterTarget;
    ParsedValue<std::uint32_t> posteriseBands;
    ParsedValue<ProcessingTarget> posteriseTarget;
    ParsedValue<ColourRampMode> rampMode;
    ParsedValue<std::uint32_t> rampStopCount;
    std::array<ParsedValue<double>, LayerLimits::maximumColourStops> rampPositions;
    std::array<ParsedValue<Rgba8>, LayerLimits::maximumColourStops> rampColours;
    ParsedValue<std::uint32_t> paletteColourCount;
    std::array<ParsedValue<Rgba8>, LayerLimits::maximumColourStops> paletteColours;
    ParsedValue<Rgba8> inkColour;
    ParsedValue<double> inkRadius;
    ParsedValue<double> inkThreshold;
    ParsedValue<double> inkSoftness;
    ParsedValue<double> inkStrength;
    ParsedValue<bool> inkInverted;
    ParsedValue<RegionFieldKind> regionField;
    ParsedValue<std::uint64_t> regionSeedOffset;
    ParsedValue<std::uint32_t> regionChannel;
    ParsedValue<double> regionOutputLow;
    ParsedValue<double> regionOutputHigh;
    ParsedValue<bool> regionInverted;
    ParsedValue<ProcessingTarget> regionTarget;
    ParsedValue<CourseLayoutProfile> courseProfile;
    ParsedValue<CourseLayoutField> courseField;
    ParsedValue<BrickSizing> courseSizing;
    ParsedValue<std::uint32_t> courseBlocks;
    ParsedValue<std::uint32_t> courseCount;
    ParsedValue<double> courseBlockVariation;
    ParsedValue<double> courseHeightVariation;
    ParsedValue<double> courseStagger;
    ParsedValue<double> courseCrookedness;
    ParsedValue<double> courseGap;
    ParsedValue<double> courseSoftness;
    ParsedValue<double> courseOverlap;
    ParsedValue<std::uint64_t> courseSeedOffset;
    ParsedValue<double> courseBlockWidthMetres;
    ParsedValue<double> courseHeightMetres;
    ParsedValue<double> courseGapMetres;
    ParsedValue<double> courseOverlapMetres;
    ParsedValue<RegionSurfaceField> sculptField;
    ParsedValue<BevelProfile> sculptProfile;
    ParsedValue<double> sculptBevelWidth;
    ParsedValue<double> sculptBevelHeight;
    ParsedValue<std::uint32_t> sculptFacetCount;
    ParsedValue<double> sculptFacetStrength;
    ParsedValue<double> sculptCentrePeak;
    ParsedValue<double> sculptSlope;
    ParsedValue<double> sculptChips;
    ParsedValue<std::uint32_t> sculptChipScale;
    ParsedValue<double> sculptWear;
    ParsedValue<double> sculptErosion;
    ParsedValue<std::uint64_t> sculptSeedOffset;
    ParsedValue<bool> sculptFacetedNormals;
    ParsedValue<ProcessingTarget> sculptTarget;
    ParsedValue<ShapePrimitiveKind> shapeKind;
    ParsedValue<ShapeFieldKind> shapeField;
    ParsedValue<std::uint32_t> shapeColumns;
    ParsedValue<std::uint32_t> shapeRows;
    ParsedValue<double> shapeWidth;
    ParsedValue<double> shapeHeight;
    ParsedValue<double> shapeCornerRadius;
    ParsedValue<double> shapeInset;
    ParsedValue<double> shapeBorderWidth;
    ParsedValue<double> shapeSoftness;
    ParsedValue<double> shapeOffsetX;
    ParsedValue<double> shapeOffsetY;
    ParsedValue<double> shapeStagger;
    ParsedValue<double> shapeRotation;
    ParsedValue<std::uint64_t> shapeSeedOffset;
    ParsedValue<std::uint32_t> shapeVertexCount;
    std::array<ParsedValue<double>, LayerLimits::maximumPolygonVertices> shapeVertexX;
    std::array<ParsedValue<double>, LayerLimits::maximumPolygonVertices> shapeVertexY;
    ParsedValue<double> shapeInnerRadius;
    ParsedValue<double> shapeArcStart;
    ParsedValue<double> shapeArcSweep;
    ParsedValue<double> shapeCrescentOffset;
    ParsedValue<std::uint32_t> shapeRadialCopies;
    ParsedValue<double> shapeRadialRadius;
    ParsedValue<double> shapeRadialPhase;
    ParsedValue<RadialOrientation> shapeRadialOrientation;
    ParsedValue<ShapeBooleanMode> shapeBooleanMode;
    ParsedValue<ProcessingTarget> shapeBooleanTarget;
    ParsedValue<LatticeKind> latticeKind;
    ParsedValue<std::int32_t> latticeWindingX;
    ParsedValue<std::int32_t> latticeWindingY;
    ParsedValue<double> latticeWidth;
    ParsedValue<double> latticeSoftness;
    ParsedValue<double> latticePhase;
    ParsedValue<ScatterField> scatterField;
    ParsedValue<std::uint32_t> scatterColumns;
    ParsedValue<std::uint32_t> scatterRows;
    ParsedValue<double> scatterDensity;
    ParsedValue<double> scatterJitter;
    ParsedValue<double> scatterMinimumDistance;
    ParsedValue<ScatterOverlapMode> scatterOverlapMode;
    ParsedValue<double> scatterMaximumOverlap;
    ParsedValue<std::uint64_t> scatterSeedOffset;
    ParsedValue<std::uint32_t> scatterPopulationCount;
    std::array<ScatterPopulationBuilder, LayerLimits::maximumScatterPopulations>
        scatterPopulations;
    ScatterMaskBuilder scatterDensityMask;
    ScatterMaskBuilder scatterExclusionMask;
    ParsedValue<OrganicCellField> organicCellField;
    ParsedValue<OrganicDirection> organicCellDirection;
    ParsedValue<std::uint32_t> organicCellColumns;
    ParsedValue<std::uint32_t> organicCellRows;
    ParsedValue<double> organicCellAnisotropy;
    ParsedValue<double> organicCellJitter;
    ParsedValue<double> organicCellIrregularity;
    ParsedValue<double> organicCellGap;
    ParsedValue<double> organicCellSoftness;
    ParsedValue<std::uint64_t> organicCellSeedOffset;
    ParsedValue<OrganicCrackField> organicCrackField;
    ParsedValue<OrganicDirection> organicCrackDirection;
    ParsedValue<std::uint32_t> organicCrackRoots;
    ParsedValue<std::uint32_t> organicCrackSegments;
    ParsedValue<std::uint32_t> organicCrackBranchLevels;
    ParsedValue<double> organicCrackBranchProbability;
    ParsedValue<double> organicCrackBend;
    ParsedValue<double> organicCrackWidth;
    ParsedValue<double> organicCrackTaper;
    ParsedValue<double> organicCrackSoftness;
    ParsedValue<std::uint64_t> organicCrackSeedOffset;
    ParsedValue<LeafField> leafField;
    ParsedValue<LeafProfile> leafProfile;
    ParsedValue<LeafClusterPattern> leafPattern;
    ParsedValue<std::uint32_t> leafColumns;
    ParsedValue<std::uint32_t> leafRows;
    ParsedValue<std::uint32_t> leavesPerCluster;
    ParsedValue<double> leafDensity;
    ParsedValue<double> leafClusterSpread;
    ParsedValue<double> leafLength;
    ParsedValue<double> leafWidth;
    ParsedValue<double> leafScaleVariation;
    ParsedValue<double> leafRotationVariation;
    ParsedValue<double> leafDirection;
    ParsedValue<double> leafTaper;
    ParsedValue<double> leafBaseNotch;
    ParsedValue<double> leafCurvature;
    ParsedValue<double> leafSerration;
    ParsedValue<std::uint32_t> leafSerrationCount;
    ParsedValue<double> leafLobing;
    ParsedValue<std::uint32_t> leafLobeCount;
    ParsedValue<double> leafMidribWidth;
    ParsedValue<std::uint32_t> leafVeinPairs;
    ParsedValue<double> leafVeinWidth;
    ParsedValue<double> leafEdgeWidth;
    ParsedValue<double> leafSoftness;
    ParsedValue<Rgba8> leafLowColour;
    ParsedValue<Rgba8> leafHighColour;
    ParsedValue<double> leafMinimumHeight;
    ParsedValue<double> leafMaximumHeight;
    ParsedValue<double> leafMinimumRoughness;
    ParsedValue<double> leafMaximumRoughness;
    ParsedValue<std::uint64_t> leafSeedOffset;
    ParsedValue<double> leafInnerHighlightWidth;
    ParsedValue<double> leafInnerHighlightInset;
    ParsedValue<double> leafClusterColourVariation;
    ParsedValue<double> leafInstanceColourVariation;
    ParsedValue<LeafProfile> leafSecondaryProfile;
    ParsedValue<double> leafSecondaryWeight;
    ParsedValue<double> leafSecondaryScale;
    ParsedValue<Rgba8> leafSecondaryLowColour;
    ParsedValue<Rgba8> leafSecondaryHighColour;
    ParsedValue<LeafProfile> leafTertiaryProfile;
    ParsedValue<double> leafTertiaryWeight;
    ParsedValue<double> leafTertiaryScale;
    ParsedValue<Rgba8> leafTertiaryLowColour;
    ParsedValue<Rgba8> leafTertiaryHighColour;
    ParsedValue<OrganicAccumulationKind> organicAccumulationKind;
    ParsedValue<OrganicAccumulationSource> organicAccumulationSource;
    ParsedValue<std::uint32_t> organicAccumulationScale;
    ParsedValue<double> organicAccumulationCoverage;
    ParsedValue<double> organicAccumulationSoftness;
    ParsedValue<double> organicAccumulationMoisture;
    ParsedValue<double> organicAccumulationBreakup;
    ParsedValue<double> organicAccumulationVariation;
    ParsedValue<Rgba8> organicAccumulationLowColour;
    ParsedValue<Rgba8> organicAccumulationHighColour;
    ParsedValue<std::uint64_t> organicAccumulationSeedOffset;
    ParsedValue<ProcessingTarget> organicAccumulationTarget;
    ParsedValue<OrganicAccumulationProfile> organicAccumulationProfile;
    ParsedValue<OrganicAccumulationField> organicAccumulationField;
    ParsedValue<double> organicAccumulationOutlineWidth;
    ParsedValue<double> organicAccumulationInnerHighlightWidth;
    ParsedValue<double> organicAccumulationInnerHighlightInset;
    ParsedValue<TextilePattern> textilePattern;
    ParsedValue<TextileField> textileField;
    ParsedValue<YarnProfile> textileYarnProfile;
    ParsedValue<TextileTileOrientation> textileTileOrientation;
    ParsedValue<std::uint32_t> textileColumns;
    ParsedValue<std::uint32_t> textileRows;
    ParsedValue<std::uint32_t> textileTileColumns;
    ParsedValue<std::uint32_t> textileTileRows;
    ParsedValue<std::uint32_t> textileWeaveSpan;
    ParsedValue<std::uint32_t> textileTwillStep;
    ParsedValue<double> textileYarnWidth;
    ParsedValue<double> textileYarnRoundness;
    ParsedValue<double> textileCrossingHeight;
    ParsedValue<double> textileJitter;
    ParsedValue<std::uint32_t> textileFibreFrequency;
    ParsedValue<double> textileFibreStrength;
    ParsedValue<double> textileTwist;
    ParsedValue<double> textilePileRadius;
    ParsedValue<double> textilePileHeight;
    ParsedValue<double> textileMissingAmount;
    ParsedValue<double> textileDamageAmount;
    ParsedValue<double> textileDifferentColourAmount;
    ParsedValue<double> textileColourVariation;
    ParsedValue<double> textileSoftness;
    ParsedValue<Rgba8> textileLowColour;
    ParsedValue<Rgba8> textileHighColour;
    ParsedValue<Rgba8> textileAccentColour;
    ParsedValue<Rgba8> textileDamageColour;
    ParsedValue<std::uint64_t> textileSeedOffset;
    ParsedValue<RegionAttachmentKind> attachmentKind;
    ParsedValue<RegionAttachmentField> attachmentField;
    ParsedValue<RegionAnchor> attachmentStartAnchor;
    ParsedValue<RegionAnchor> attachmentEndAnchor;
    ParsedValue<RegionGlyph> attachmentGlyph;
    ParsedValue<std::uint32_t> attachmentCount;
    ParsedValue<double> attachmentSize;
    ParsedValue<double> attachmentAspect;
    ParsedValue<double> attachmentInset;
    ParsedValue<double> attachmentRotation;
    ParsedValue<double> attachmentJitter;
    ParsedValue<double> attachmentSelection;
    ParsedValue<double> attachmentLineWidth;
    ParsedValue<double> attachmentLength;
    ParsedValue<double> attachmentBranching;
    ParsedValue<double> attachmentSoftness;
    ParsedValue<Rgba8> attachmentColour;
    ParsedValue<double> attachmentHeight;
    ParsedValue<double> attachmentRoughness;
    ParsedValue<double> attachmentMetalness;
    ParsedValue<double> attachmentOcclusion;
    ParsedValue<double> attachmentEmissive;
    ParsedValue<std::uint64_t> attachmentSeedOffset;
};

std::string_view trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

std::optional<Field> fieldForKey(std::string_view key)
{
    for (std::size_t index = 0; index < fieldKeys.size(); ++index) {
        if (key == fieldKeys[index]) {
            return static_cast<Field>(index);
        }
    }
    return std::nullopt;
}

std::optional<std::pair<std::size_t, std::string_view>> parseLayerKey(std::string_view key)
{
    constexpr std::string_view prefix = "layer.";
    if (!key.starts_with(prefix)) {
        return std::nullopt;
    }
    const auto remainder = key.substr(prefix.size());
    const auto dot = remainder.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= remainder.size()) {
        return std::nullopt;
    }
    std::size_t index = 0;
    const auto indexText = remainder.substr(0, dot);
    const auto result = std::from_chars(
        indexText.data(),
        indexText.data() + indexText.size(),
        index,
        10);
    if (result.ec != std::errc{} || result.ptr != indexText.data() + indexText.size()) {
        return std::nullopt;
    }
    return std::pair{index, remainder.substr(dot + 1)};
}

std::optional<std::pair<std::size_t, std::string_view>> parseIndexedProperty(
    std::string_view property,
    std::string_view prefix)
{
    if (!property.starts_with(prefix)) {
        return std::nullopt;
    }
    const auto remainder = property.substr(prefix.size());
    const auto dot = remainder.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 >= remainder.size()) {
        return std::nullopt;
    }
    std::size_t index = 0;
    const auto indexText = remainder.substr(0, dot);
    const auto result = std::from_chars(
        indexText.data(),
        indexText.data() + indexText.size(),
        index,
        10);
    if (result.ec != std::errc{} || result.ptr != indexText.data() + indexText.size()) {
        return std::nullopt;
    }
    return std::pair{index, remainder.substr(dot + 1)};
}

template<typename Integer>
bool parseInteger(std::string_view value, Integer& output)
{
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, output, 10);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseDouble(std::string_view value, double& output)
{
    std::istringstream stream{std::string(value)};
    stream.imbue(std::locale::classic());
    stream >> std::noskipws >> output;
    return stream && stream.peek() == std::char_traits<char>::eof();
}

bool parseMetres(std::string_view value, double& output)
{
    if (value.size() < 2 || value.back() != 'm') {
        return false;
    }
    value.remove_suffix(1);
    return !value.empty() && parseDouble(value, output);
}

bool parseBoolean(std::string_view value, bool& output)
{
    if (value == "true") {
        output = true;
        return true;
    }
    if (value == "false") {
        output = false;
        return true;
    }
    return false;
}

bool parseColour(std::string_view value, Rgba8& output)
{
    if (value.size() != 10 || value[0] != '0' || (value[1] != 'x' && value[1] != 'X')) {
        return false;
    }
    std::uint32_t packed = 0;
    const char* begin = value.data() + 2;
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, packed, 16);
    if (result.ec != std::errc{} || result.ptr != end) {
        return false;
    }
    output = {
        static_cast<std::uint8_t>((packed >> 24U) & 0xffU),
        static_cast<std::uint8_t>((packed >> 16U) & 0xffU),
        static_cast<std::uint8_t>((packed >> 8U) & 0xffU),
        static_cast<std::uint8_t>(packed & 0xffU),
    };
    return true;
}

ParseDiagnostic diagnostic(std::size_t line, std::size_t column, std::string message)
{
    return ParseDiagnostic{line, column, std::move(message)};
}

std::string formatDouble(double value)
{
    for (int precision = 1; precision <= std::numeric_limits<double>::max_digits10; ++precision) {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(precision) << value;
        const auto candidate = stream.str();
        double parsed = 0.0;
        if (parseDouble(candidate, parsed) && parsed == value) {
            return candidate;
        }
    }
    return {};
}

std::string formatMetres(double value)
{
    const auto number = formatDouble(value);
    return number.empty() ? std::string{} : number + "m";
}

std::string formatColour(const Rgba8& colour)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string output{"0x00000000"};
    const std::array channels{colour.red, colour.green, colour.blue, colour.alpha};
    for (std::size_t index = 0; index < channels.size(); ++index) {
        output[2 + index * 2] = digits[channels[index] >> 4U];
        output[3 + index * 2] = digits[channels[index] & 0x0fU];
    }
    return output;
}

std::string formatLayerOutputs(const LayerOutputRouting& outputs)
{
    std::string value;
    const auto append = [&value](std::string_view name) {
        if (!value.empty()) {
            value += ", ";
        }
        value += name;
    };
    if (outputs.colour) append("colour");
    if (outputs.height) append("height");
    if (outputs.roughness) append("roughness");
    if (outputs.metalness) append("metalness");
    if (outputs.coating) append("coating");
    if (outputs.occlusion) append("occlusion");
    if (outputs.clearCoat) append("clearcoat");
    if (outputs.clearCoatRoughness) append("clearcoat_roughness");
    if (outputs.emissive) append("emissive");
    return value;
}

std::optional<CompositeMode> parseCompositeMode(std::string_view value)
{
    if (value == "blend") {
        return CompositeMode::blend;
    }
    if (value == "add") {
        return CompositeMode::add;
    }
    if (value == "multiply") {
        return CompositeMode::multiply;
    }
    if (value == "minimum") {
        return CompositeMode::minimum;
    }
    if (value == "maximum") {
        return CompositeMode::maximum;
    }
    if (value == "detail") {
        return CompositeMode::detail;
    }
    return std::nullopt;
}

std::optional<LayerOutputRouting> parseLayerOutputs(std::string_view value)
{
    LayerOutputRouting outputs{
        false, false, false, false, false, false, false, false, false};
    std::size_t offset = 0;
    while (offset <= value.size()) {
        const auto comma = value.find(',', offset);
        const auto end = comma == std::string_view::npos ? value.size() : comma;
        const auto output = trim(value.substr(offset, end - offset));
        bool* destination = nullptr;
        if (output == "colour") {
            destination = &outputs.colour;
        } else if (output == "height") {
            destination = &outputs.height;
        } else if (output == "roughness") {
            destination = &outputs.roughness;
        } else if (output == "metalness") {
            destination = &outputs.metalness;
        } else if (output == "coating") {
            destination = &outputs.coating;
        } else if (output == "occlusion") {
            destination = &outputs.occlusion;
        } else if (output == "clearcoat") {
            destination = &outputs.clearCoat;
        } else if (output == "clearcoat_roughness") {
            destination = &outputs.clearCoatRoughness;
        } else if (output == "emissive") {
            destination = &outputs.emissive;
        } else {
            return std::nullopt;
        }
        if (*destination) {
            return std::nullopt;
        }
        *destination = true;
        if (comma == std::string_view::npos) {
            break;
        }
        offset = comma + 1;
    }
    if (!outputs.colour && !outputs.height && !outputs.roughness && !outputs.metalness &&
        !outputs.coating && !outputs.occlusion && !outputs.clearCoat &&
        !outputs.clearCoatRoughness && !outputs.emissive) {
        return std::nullopt;
    }
    return outputs;
}

std::optional<OperationKind> parseOperationKind(std::string_view value)
{
    if (value == "noise") {
        return OperationKind::noise;
    }
    if (value == "solid_colour") {
        return OperationKind::solidColour;
    }
    if (value == "levels") {
        return OperationKind::levels;
    }
    if (value == "threshold") {
        return OperationKind::threshold;
    }
    if (value == "brick_grid") {
        return OperationKind::brickGrid;
    }
    if (value == "tile_grid") {
        return OperationKind::tileGrid;
    }
    if (value == "worley_cells") {
        return OperationKind::worleyCells;
    }
    if (value == "random_cells") {
        return OperationKind::randomCells;
    }
    if (value == "lines") {
        return OperationKind::lines;
    }
    if (value == "rectangles") {
        return OperationKind::rectangles;
    }
    if (value == "circles") {
        return OperationKind::circles;
    }
    if (value == "surface_pattern") {
        return OperationKind::surfacePattern;
    }
    if (value == "surface_filter") {
        return OperationKind::surfaceFilter;
    }
    if (value == "posterise") {
        return OperationKind::posterise;
    }
    if (value == "colour_ramp") {
        return OperationKind::colourRamp;
    }
    if (value == "palette") {
        return OperationKind::palette;
    }
    if (value == "ink_contour") {
        return OperationKind::inkContour;
    }
    if (value == "region_field") {
        return OperationKind::regionField;
    }
    if (value == "course_layout") {
        return OperationKind::courseLayout;
    }
    if (value == "region_surface") {
        return OperationKind::regionSurface;
    }
    if (value == "shape") {
        return OperationKind::shape;
    }
    if (value == "shape_boolean") {
        return OperationKind::shapeBoolean;
    }
    if (value == "lattice") {
        return OperationKind::lattice;
    }
    if (value == "scatter") {
        return OperationKind::scatter;
    }
    if (value == "organic_cells") {
        return OperationKind::organicCells;
    }
    if (value == "organic_cracks") {
        return OperationKind::organicCracks;
    }
    if (value == "leaf_cluster") {
        return OperationKind::leafCluster;
    }
    if (value == "organic_accumulation") {
        return OperationKind::organicAccumulation;
    }
    if (value == "surface_value") {
        return OperationKind::surfaceValue;
    }
    if (value == "textile") {
        return OperationKind::textile;
    }
    if (value == "region_attachment") {
        return OperationKind::regionAttachment;
    }
    return std::nullopt;
}

std::optional<RegionAttachmentKind> parseRegionAttachmentKind(std::string_view value)
{
    if (value == "fastener") return RegionAttachmentKind::fastener;
    if (value == "inlay") return RegionAttachmentKind::inlay;
    if (value == "glyph") return RegionAttachmentKind::glyph;
    if (value == "chip") return RegionAttachmentKind::chip;
    if (value == "crack") return RegionAttachmentKind::crack;
    if (value == "damage") return RegionAttachmentKind::damage;
    return std::nullopt;
}

std::optional<RegionAttachmentField> parseRegionAttachmentField(std::string_view value)
{
    if (value == "material") return RegionAttachmentField::material;
    if (value == "mask") return RegionAttachmentField::mask;
    if (value == "distance") return RegionAttachmentField::distance;
    return std::nullopt;
}

std::optional<RegionAnchor> parseRegionAnchor(std::string_view value)
{
    if (value == "centre") return RegionAnchor::centre;
    if (value == "edge") return RegionAnchor::edge;
    if (value == "corner") return RegionAnchor::corner;
    if (value == "cavity") return RegionAnchor::cavity;
    return std::nullopt;
}

std::optional<RegionGlyph> parseRegionGlyph(std::string_view value)
{
    if (value == "cross") return RegionGlyph::cross;
    if (value == "chevron") return RegionGlyph::chevron;
    if (value == "triangle") return RegionGlyph::triangle;
    if (value == "rune") return RegionGlyph::rune;
    return std::nullopt;
}

std::optional<TextilePattern> parseTextilePattern(std::string_view value)
{
    if (value == "plain_weave") return TextilePattern::plainWeave;
    if (value == "basket_weave") return TextilePattern::basketWeave;
    if (value == "twill_weave") return TextilePattern::twillWeave;
    if (value == "loop_pile") return TextilePattern::loopPile;
    if (value == "cut_pile") return TextilePattern::cutPile;
    return std::nullopt;
}

std::optional<TextileField> parseTextileField(std::string_view value)
{
    if (value == "material") return TextileField::material;
    if (value == "height") return TextileField::height;
    if (value == "warp") return TextileField::warp;
    if (value == "weft") return TextileField::weft;
    if (value == "over_under") return TextileField::overUnder;
    if (value == "fibres") return TextileField::fibres;
    if (value == "pile") return TextileField::pile;
    if (value == "damage") return TextileField::damage;
    if (value == "colour_variation") return TextileField::colourVariation;
    if (value == "direction") return TextileField::direction;
    return std::nullopt;
}

std::optional<YarnProfile> parseYarnProfile(std::string_view value)
{
    if (value == "round") return YarnProfile::round;
    if (value == "flat") return YarnProfile::flat;
    if (value == "twisted") return YarnProfile::twisted;
    return std::nullopt;
}

std::optional<TextileTileOrientation> parseTextileTileOrientation(
    std::string_view value)
{
    if (value == "uniform") return TextileTileOrientation::uniform;
    if (value == "alternating_rows") return TextileTileOrientation::alternatingRows;
    if (value == "alternating_columns") return TextileTileOrientation::alternatingColumns;
    if (value == "checkerboard") return TextileTileOrientation::checkerboard;
    return std::nullopt;
}

std::optional<OrganicDirection> parseOrganicDirection(std::string_view value)
{
    if (value == "vertical") return OrganicDirection::vertical;
    if (value == "horizontal") return OrganicDirection::horizontal;
    return std::nullopt;
}

std::optional<OrganicCellField> parseOrganicCellField(std::string_view value)
{
    if (value == "plates") return OrganicCellField::plates;
    if (value == "boundaries") return OrganicCellField::boundaries;
    if (value == "cell_random") return OrganicCellField::cellRandom;
    return std::nullopt;
}

std::optional<OrganicCrackField> parseOrganicCrackField(std::string_view value)
{
    if (value == "cracks") return OrganicCrackField::cracks;
    if (value == "trunks") return OrganicCrackField::trunks;
    if (value == "branches") return OrganicCrackField::branches;
    if (value == "hierarchy") return OrganicCrackField::hierarchy;
    if (value == "distance") return OrganicCrackField::distance;
    return std::nullopt;
}

std::optional<LeafField> parseLeafField(std::string_view value)
{
    if (value == "material") return LeafField::material;
    if (value == "fill") return LeafField::fill;
    if (value == "edge") return LeafField::edge;
    if (value == "midrib") return LeafField::midrib;
    if (value == "veins") return LeafField::veins;
    if (value == "instance_random") return LeafField::instanceRandom;
    if (value == "outline") return LeafField::outline;
    if (value == "inner_highlight") return LeafField::innerHighlight;
    if (value == "cluster_random") return LeafField::clusterRandom;
    if (value == "population") return LeafField::population;
    return std::nullopt;
}

std::optional<LeafProfile> parseLeafProfile(std::string_view value)
{
    if (value == "ovate") return LeafProfile::ovate;
    if (value == "lanceolate") return LeafProfile::lanceolate;
    if (value == "cordate") return LeafProfile::cordate;
    if (value == "lobed") return LeafProfile::lobed;
    if (value == "blob") return LeafProfile::blob;
    if (value == "rosette") return LeafProfile::rosette;
    if (value == "lichen") return LeafProfile::lichen;
    return std::nullopt;
}

std::optional<LeafClusterPattern> parseLeafClusterPattern(std::string_view value)
{
    if (value == "radial") return LeafClusterPattern::radial;
    if (value == "fan") return LeafClusterPattern::fan;
    if (value == "vine") return LeafClusterPattern::vine;
    if (value == "canopy") return LeafClusterPattern::canopy;
    if (value == "ground_scatter") return LeafClusterPattern::groundScatter;
    return std::nullopt;
}

std::optional<OrganicAccumulationKind> parseOrganicAccumulationKind(
    std::string_view value)
{
    if (value == "moss") return OrganicAccumulationKind::moss;
    if (value == "lichen") return OrganicAccumulationKind::lichen;
    if (value == "colour_variation") return OrganicAccumulationKind::colourVariation;
    return std::nullopt;
}

std::optional<OrganicAccumulationSource> parseOrganicAccumulationSource(
    std::string_view value)
{
    if (value == "cavity") return OrganicAccumulationSource::cavity;
    if (value == "boundary") return OrganicAccumulationSource::boundary;
    if (value == "low_height") return OrganicAccumulationSource::lowHeight;
    if (value == "authored_mask") return OrganicAccumulationSource::authoredMask;
    return std::nullopt;
}

std::optional<OrganicAccumulationProfile> parseOrganicAccumulationProfile(
    std::string_view value)
{
    if (value == "noise") return OrganicAccumulationProfile::noise;
    if (value == "colonies") return OrganicAccumulationProfile::colonies;
    if (value == "speckles") return OrganicAccumulationProfile::speckles;
    return std::nullopt;
}

std::optional<OrganicAccumulationField> parseOrganicAccumulationField(
    std::string_view value)
{
    if (value == "material") return OrganicAccumulationField::material;
    if (value == "fill") return OrganicAccumulationField::fill;
    if (value == "outline") return OrganicAccumulationField::outline;
    if (value == "inner_highlight") return OrganicAccumulationField::innerHighlight;
    if (value == "detail") return OrganicAccumulationField::detail;
    return std::nullopt;
}

std::optional<ScatterField> parseScatterField(std::string_view value)
{
    if (value == "material") {
        return ScatterField::material;
    }
    if (value == "fill") {
        return ScatterField::fill;
    }
    if (value == "instance_random") {
        return ScatterField::instanceRandom;
    }
    if (value == "local_u") {
        return ScatterField::localU;
    }
    if (value == "local_v") {
        return ScatterField::localV;
    }
    if (value == "boundary_distance") {
        return ScatterField::boundaryDistance;
    }
    return std::nullopt;
}

std::optional<ScatterOverlapMode> parseScatterOverlapMode(std::string_view value)
{
    if (value == "forbidden") {
        return ScatterOverlapMode::forbidden;
    }
    if (value == "controlled") {
        return ScatterOverlapMode::controlled;
    }
    if (value == "unrestricted") {
        return ScatterOverlapMode::unrestricted;
    }
    return std::nullopt;
}

std::optional<ShapePrimitiveKind> parseShapePrimitiveKind(std::string_view value)
{
    if (value == "rounded_rectangle") {
        return ShapePrimitiveKind::roundedRectangle;
    }
    if (value == "ellipse") {
        return ShapePrimitiveKind::ellipse;
    }
    if (value == "capsule") {
        return ShapePrimitiveKind::capsule;
    }
    if (value == "diamond") {
        return ShapePrimitiveKind::diamond;
    }
    if (value == "convex_polygon") {
        return ShapePrimitiveKind::convexPolygon;
    }
    if (value == "annulus") {
        return ShapePrimitiveKind::annulus;
    }
    if (value == "arc") {
        return ShapePrimitiveKind::arc;
    }
    if (value == "sector") {
        return ShapePrimitiveKind::sector;
    }
    if (value == "crescent") {
        return ShapePrimitiveKind::crescent;
    }
    return std::nullopt;
}

std::optional<RadialOrientation> parseRadialOrientation(std::string_view value)
{
    if (value == "fixed") {
        return RadialOrientation::fixed;
    }
    if (value == "outward") {
        return RadialOrientation::outward;
    }
    if (value == "tangent") {
        return RadialOrientation::tangent;
    }
    return std::nullopt;
}

std::optional<ShapeFieldKind> parseShapeFieldKind(std::string_view value)
{
    if (value == "fill") {
        return ShapeFieldKind::fill;
    }
    if (value == "inset") {
        return ShapeFieldKind::inset;
    }
    if (value == "outline") {
        return ShapeFieldKind::outline;
    }
    if (value == "border") {
        return ShapeFieldKind::border;
    }
    return std::nullopt;
}

std::optional<ShapeBooleanMode> parseShapeBooleanMode(std::string_view value)
{
    if (value == "union") {
        return ShapeBooleanMode::unionMask;
    }
    if (value == "intersection") {
        return ShapeBooleanMode::intersection;
    }
    if (value == "subtraction") {
        return ShapeBooleanMode::subtraction;
    }
    return std::nullopt;
}

std::optional<LatticeKind> parseLatticeKind(std::string_view value)
{
    if (value == "lines") {
        return LatticeKind::lines;
    }
    if (value == "diamonds") {
        return LatticeKind::diamonds;
    }
    return std::nullopt;
}

std::optional<SurfacePatternKind> parseSurfacePatternKind(std::string_view value)
{
    if (value == "ridged_noise") {
        return SurfacePatternKind::ridgedNoise;
    }
    if (value == "bands") {
        return SurfacePatternKind::bands;
    }
    if (value == "rings") {
        return SurfacePatternKind::rings;
    }
    if (value == "scatter") {
        return SurfacePatternKind::scatter;
    }
    if (value == "streaks") {
        return SurfacePatternKind::streaks;
    }
    return std::nullopt;
}

std::optional<SurfaceFilterKind> parseSurfaceFilterKind(std::string_view value)
{
    if (value == "invert") {
        return SurfaceFilterKind::invert;
    }
    if (value == "soften") {
        return SurfaceFilterKind::soften;
    }
    if (value == "expand") {
        return SurfaceFilterKind::expand;
    }
    if (value == "contract") {
        return SurfaceFilterKind::contract;
    }
    if (value == "edge") {
        return SurfaceFilterKind::edge;
    }
    if (value == "slope") {
        return SurfaceFilterKind::slope;
    }
    if (value == "cavity") {
        return SurfaceFilterKind::cavity;
    }
    if (value == "peaks") {
        return SurfaceFilterKind::peaks;
    }
    if (value == "edge_aware_soften") {
        return SurfaceFilterKind::edgeAwareSoften;
    }
    return std::nullopt;
}

std::optional<ProcessingTarget> parseProcessingTarget(std::string_view value)
{
    if (value == "colour") {
        return ProcessingTarget::colour;
    }
    if (value == "scalar") {
        return ProcessingTarget::scalar;
    }
    if (value == "all") {
        return ProcessingTarget::colourAndScalar;
    }
    return std::nullopt;
}

std::optional<RegionFieldKind> parseRegionFieldKind(std::string_view value)
{
    if (value == "random") {
        return RegionFieldKind::random;
    }
    if (value == "local_u") {
        return RegionFieldKind::localU;
    }
    if (value == "local_v") {
        return RegionFieldKind::localV;
    }
    if (value == "centre_distance") {
        return RegionFieldKind::centreDistance;
    }
    if (value == "boundary_distance") {
        return RegionFieldKind::boundaryDistance;
    }
    if (value == "course_random") {
        return RegionFieldKind::courseRandom;
    }
    return std::nullopt;
}

std::optional<CourseLayoutProfile> parseCourseLayoutProfile(std::string_view value)
{
    if (value == "masonry") {
        return CourseLayoutProfile::masonry;
    }
    if (value == "slabs") {
        return CourseLayoutProfile::slabs;
    }
    if (value == "slates") {
        return CourseLayoutProfile::slates;
    }
    return std::nullopt;
}

std::optional<RegionSurfaceField> parseRegionSurfaceField(std::string_view value)
{
    if (value == "height") {
        return RegionSurfaceField::height;
    }
    if (value == "cavity") {
        return RegionSurfaceField::cavity;
    }
    if (value == "outer_edge") {
        return RegionSurfaceField::outerEdge;
    }
    if (value == "exposed_face") {
        return RegionSurfaceField::exposedFace;
    }
    if (value == "facet") {
        return RegionSurfaceField::facet;
    }
    if (value == "wear") {
        return RegionSurfaceField::wear;
    }
    return std::nullopt;
}

std::optional<BevelProfile> parseBevelProfile(std::string_view value)
{
    if (value == "rounded") {
        return BevelProfile::rounded;
    }
    if (value == "chamfered") {
        return BevelProfile::chamfered;
    }
    if (value == "hand_cut") {
        return BevelProfile::handCut;
    }
    return std::nullopt;
}

std::optional<CourseLayoutField> parseCourseLayoutField(std::string_view value)
{
    if (value == "blocks") {
        return CourseLayoutField::blocks;
    }
    if (value == "mortar") {
        return CourseLayoutField::mortar;
    }
    if (value == "course") {
        return CourseLayoutField::course;
    }
    if (value == "overlap") {
        return CourseLayoutField::overlap;
    }
    return std::nullopt;
}

std::optional<ColourRampMode> parseColourRampMode(std::string_view value)
{
    if (value == "linear") {
        return ColourRampMode::linear;
    }
    if (value == "stepped") {
        return ColourRampMode::stepped;
    }
    return std::nullopt;
}

std::optional<LineDirection> parseLineDirection(std::string_view value)
{
    if (value == "vertical") {
        return LineDirection::vertical;
    }
    if (value == "horizontal") {
        return LineDirection::horizontal;
    }
    return std::nullopt;
}

std::optional<BrickMortarSpace> parseBrickMortarSpace(std::string_view value)
{
    if (value == "cell") {
        return BrickMortarSpace::cell;
    }
    if (value == "texture") {
        return BrickMortarSpace::texture;
    }
    return std::nullopt;
}

std::optional<BrickSizing> parseBrickSizing(std::string_view value)
{
    if (value == "relative") {
        return BrickSizing::relative;
    }
    if (value == "physical") {
        return BrickSizing::physical;
    }
    return std::nullopt;
}

std::optional<QuarterTurn> parseRotation(std::string_view value)
{
    std::uint32_t degrees = 0;
    if (!parseInteger(value, degrees)) {
        return std::nullopt;
    }
    switch (degrees) {
    case 0:
        return QuarterTurn::none;
    case 90:
        return QuarterTurn::clockwise90;
    case 180:
        return QuarterTurn::clockwise180;
    case 270:
        return QuarterTurn::clockwise270;
    default:
        return std::nullopt;
    }
}

bool hasVersionThreeFields(const LayerBuilder& builder)
{
    return builder.scaleX.value || builder.scaleY.value || builder.offsetX.value ||
        builder.offsetY.value || builder.rotation.value || builder.warpEnabled.value ||
        builder.warpStrength.value || builder.warpFrequency.value ||
        builder.warpSeedOffset.value || builder.maskEnabled.value ||
        builder.maskInverted.value || builder.maskSeedOffset.value ||
        builder.maskLow.value || builder.maskHigh.value;
}

bool isStructuralOperation(OperationKind operation)
{
    return operation == OperationKind::brickGrid ||
        operation == OperationKind::tileGrid ||
        operation == OperationKind::worleyCells ||
        operation == OperationKind::randomCells ||
        operation == OperationKind::lines ||
        operation == OperationKind::rectangles ||
        operation == OperationKind::circles ||
        operation == OperationKind::courseLayout ||
        operation == OperationKind::shape ||
        operation == OperationKind::lattice ||
        operation == OperationKind::scatter ||
        operation == OperationKind::organicCells ||
        operation == OperationKind::organicCracks ||
        operation == OperationKind::leafCluster;
}

bool hasVersionFourFields(const LayerBuilder& builder)
{
    return builder.brickColumns.value || builder.brickRows.value ||
        builder.brickMortar.value || builder.brickStagger.value ||
        builder.brickSoftness.value || builder.tileColumns.value ||
        builder.tileRows.value || builder.tileGrout.value ||
        builder.tileSoftness.value || builder.worleyColumns.value ||
        builder.worleyRows.value || builder.worleyJitter.value ||
        builder.worleyEdgeWidth.value || builder.worleySeedOffset.value ||
        builder.randomColumns.value || builder.randomRows.value ||
        builder.randomSeedOffset.value || builder.lineDirection.value ||
        builder.lineCount.value || builder.lineWidth.value ||
        builder.lineSoftness.value || builder.rectangleColumns.value ||
        builder.rectangleRows.value || builder.rectangleWidth.value ||
        builder.rectangleHeight.value || builder.rectangleSoftness.value ||
        builder.circleColumns.value || builder.circleRows.value ||
        builder.circleRadius.value || builder.circleSoftness.value;
}

bool hasVersionFiveFields(const LayerBuilder& builder)
{
    return builder.brickMortarSpace.value.has_value();
}

bool hasVersionSixFields(const LayerBuilder& builder)
{
    return builder.brickSizing.value || builder.brickWidthMetres.value ||
        builder.brickHeightMetres.value || builder.brickMortarMetres.value;
}

bool hasVersionSevenFields(const LayerBuilder& builder)
{
    return builder.surfaceKind.value || builder.surfaceScale.value ||
        builder.surfaceWidth.value || builder.surfaceDetail.value ||
        builder.surfaceDistortion.value || builder.surfaceVariation.value ||
        builder.surfaceSeedOffset.value || builder.filterKind.value ||
        builder.filterRadius.value || builder.filterStrength.value;
}

bool hasVersionEightFields(const LayerBuilder& builder)
{
    const auto any = [](const auto& fields) {
        return std::any_of(fields.begin(), fields.end(), [](const auto& field) {
            return field.value.has_value();
        });
    };
    return builder.filterSensitivity.value || builder.filterTarget.value ||
        builder.posteriseBands.value || builder.posteriseTarget.value ||
        builder.rampMode.value || builder.rampStopCount.value ||
        any(builder.rampPositions) || any(builder.rampColours) ||
        builder.paletteColourCount.value || any(builder.paletteColours) ||
        builder.inkColour.value || builder.inkRadius.value ||
        builder.inkThreshold.value || builder.inkSoftness.value ||
        builder.inkStrength.value || builder.inkInverted.value;
}

bool hasVersionNineFields(const LayerBuilder& builder)
{
    return builder.regionField.value || builder.regionSeedOffset.value ||
        builder.regionChannel.value || builder.regionOutputLow.value ||
        builder.regionOutputHigh.value || builder.regionInverted.value ||
        builder.regionTarget.value;
}

bool hasVersionTenFields(const LayerBuilder& builder)
{
    return builder.courseProfile.value || builder.courseField.value ||
        builder.courseSizing.value || builder.courseBlocks.value ||
        builder.courseCount.value || builder.courseBlockVariation.value ||
        builder.courseHeightVariation.value || builder.courseStagger.value ||
        builder.courseCrookedness.value || builder.courseGap.value ||
        builder.courseSoftness.value || builder.courseOverlap.value ||
        builder.courseSeedOffset.value || builder.courseBlockWidthMetres.value ||
        builder.courseHeightMetres.value || builder.courseGapMetres.value ||
        builder.courseOverlapMetres.value;
}

bool hasVersionElevenFields(const LayerBuilder& builder)
{
    return builder.sculptField.value || builder.sculptProfile.value ||
        builder.sculptBevelWidth.value || builder.sculptBevelHeight.value ||
        builder.sculptFacetCount.value || builder.sculptFacetStrength.value ||
        builder.sculptCentrePeak.value || builder.sculptSlope.value ||
        builder.sculptChips.value || builder.sculptChipScale.value ||
        builder.sculptWear.value || builder.sculptErosion.value ||
        builder.sculptSeedOffset.value || builder.sculptFacetedNormals.value ||
        builder.sculptTarget.value;
}

bool hasVersionTwelveShapeFields(const LayerBuilder& builder)
{
    const auto any = [](const auto& fields) {
        return std::any_of(fields.begin(), fields.end(), [](const auto& field) {
            return field.value.has_value();
        });
    };
    return builder.shapeKind.value || builder.shapeField.value ||
        builder.shapeColumns.value || builder.shapeRows.value ||
        builder.shapeWidth.value || builder.shapeHeight.value ||
        builder.shapeCornerRadius.value || builder.shapeInset.value ||
        builder.shapeBorderWidth.value || builder.shapeSoftness.value ||
        builder.shapeOffsetX.value || builder.shapeOffsetY.value ||
        builder.shapeStagger.value || builder.shapeRotation.value ||
        builder.shapeSeedOffset.value || builder.shapeVertexCount.value ||
        builder.shapeInnerRadius.value || builder.shapeArcStart.value ||
        builder.shapeArcSweep.value || builder.shapeCrescentOffset.value ||
        builder.shapeRadialCopies.value || builder.shapeRadialRadius.value ||
        builder.shapeRadialPhase.value || builder.shapeRadialOrientation.value ||
        any(builder.shapeVertexX) || any(builder.shapeVertexY) ||
        builder.shapeBooleanMode.value || builder.shapeBooleanTarget.value;
}

bool hasVersionNineteenFields(const LayerBuilder& builder)
{
    return builder.shapeInnerRadius.value || builder.shapeArcStart.value ||
        builder.shapeArcSweep.value || builder.shapeCrescentOffset.value ||
        builder.shapeRadialCopies.value || builder.shapeRadialRadius.value ||
        builder.shapeRadialPhase.value || builder.shapeRadialOrientation.value;
}

bool hasVersionTwelveLatticeFields(const LayerBuilder& builder)
{
    return builder.latticeKind.value || builder.latticeWindingX.value ||
        builder.latticeWindingY.value || builder.latticeWidth.value ||
        builder.latticeSoftness.value || builder.latticePhase.value;
}

bool hasVersionTwelveFields(const LayerBuilder& builder)
{
    return hasVersionTwelveShapeFields(builder) ||
        hasVersionTwelveLatticeFields(builder);
}

bool hasVersionThirteenFields(const LayerBuilder& builder)
{
    const auto populationHasFields = [](const ScatterPopulationBuilder& population) {
        return population.weight.value || population.minimumScale.value ||
            population.maximumScale.value || population.minimumAspect.value ||
            population.maximumAspect.value || population.minimumRotation.value ||
            population.maximumRotation.value || population.lowColour.value ||
            population.highColour.value || population.minimumHeight.value ||
            population.maximumHeight.value || population.minimumRoughness.value ||
            population.maximumRoughness.value;
    };
    const auto maskHasFields = [](const ScatterMaskBuilder& mask) {
        return mask.enabled.value || mask.inverted.value || mask.frequency.value ||
            mask.inputLow.value || mask.inputHigh.value || mask.seedOffset.value;
    };
    return builder.scatterField.value || builder.scatterColumns.value ||
        builder.scatterRows.value || builder.scatterDensity.value ||
        builder.scatterJitter.value || builder.scatterMinimumDistance.value ||
        builder.scatterOverlapMode.value || builder.scatterMaximumOverlap.value ||
        builder.scatterSeedOffset.value || builder.scatterPopulationCount.value ||
        std::any_of(
            builder.scatterPopulations.begin(),
            builder.scatterPopulations.end(),
            populationHasFields) ||
        maskHasFields(builder.scatterDensityMask) ||
        maskHasFields(builder.scatterExclusionMask);
}

bool hasVersionFourteenFields(const LayerBuilder& builder)
{
    return builder.organicCellField.value || builder.organicCellDirection.value ||
        builder.organicCellColumns.value || builder.organicCellRows.value ||
        builder.organicCellAnisotropy.value || builder.organicCellJitter.value ||
        builder.organicCellIrregularity.value || builder.organicCellGap.value ||
        builder.organicCellSoftness.value || builder.organicCellSeedOffset.value ||
        builder.organicCrackField.value || builder.organicCrackDirection.value ||
        builder.organicCrackRoots.value || builder.organicCrackSegments.value ||
        builder.organicCrackBranchLevels.value ||
        builder.organicCrackBranchProbability.value || builder.organicCrackBend.value ||
        builder.organicCrackWidth.value || builder.organicCrackTaper.value ||
        builder.organicCrackSoftness.value || builder.organicCrackSeedOffset.value ||
        builder.leafField.value || builder.leafProfile.value || builder.leafPattern.value ||
        builder.leafColumns.value || builder.leafRows.value ||
        builder.leavesPerCluster.value || builder.leafDensity.value ||
        builder.leafClusterSpread.value || builder.leafLength.value ||
        builder.leafWidth.value || builder.leafScaleVariation.value ||
        builder.leafRotationVariation.value || builder.leafDirection.value ||
        builder.leafTaper.value || builder.leafBaseNotch.value ||
        builder.leafCurvature.value || builder.leafSerration.value ||
        builder.leafSerrationCount.value || builder.leafLobing.value ||
        builder.leafLobeCount.value || builder.leafMidribWidth.value ||
        builder.leafVeinPairs.value || builder.leafVeinWidth.value ||
        builder.leafEdgeWidth.value || builder.leafSoftness.value ||
        builder.leafLowColour.value || builder.leafHighColour.value ||
        builder.leafMinimumHeight.value || builder.leafMaximumHeight.value ||
        builder.leafMinimumRoughness.value || builder.leafMaximumRoughness.value ||
        builder.leafSeedOffset.value || builder.organicAccumulationKind.value ||
        builder.organicAccumulationSource.value || builder.organicAccumulationScale.value ||
        builder.organicAccumulationCoverage.value ||
        builder.organicAccumulationSoftness.value ||
        builder.organicAccumulationMoisture.value ||
        builder.organicAccumulationBreakup.value ||
        builder.organicAccumulationVariation.value ||
        builder.organicAccumulationLowColour.value ||
        builder.organicAccumulationHighColour.value ||
        builder.organicAccumulationSeedOffset.value ||
        builder.organicAccumulationTarget.value;
}

bool hasVersionTwentyFields(const LayerBuilder& builder)
{
    return builder.textilePattern.value || builder.textileField.value ||
        builder.textileYarnProfile.value || builder.textileTileOrientation.value ||
        builder.textileColumns.value || builder.textileRows.value ||
        builder.textileTileColumns.value || builder.textileTileRows.value ||
        builder.textileWeaveSpan.value || builder.textileTwillStep.value ||
        builder.textileYarnWidth.value || builder.textileYarnRoundness.value ||
        builder.textileCrossingHeight.value || builder.textileJitter.value ||
        builder.textileFibreFrequency.value || builder.textileFibreStrength.value ||
        builder.textileTwist.value || builder.textilePileRadius.value ||
        builder.textilePileHeight.value || builder.textileMissingAmount.value ||
        builder.textileDamageAmount.value ||
        builder.textileDifferentColourAmount.value ||
        builder.textileColourVariation.value || builder.textileSoftness.value ||
        builder.textileLowColour.value || builder.textileHighColour.value ||
        builder.textileAccentColour.value || builder.textileDamageColour.value ||
        builder.textileSeedOffset.value;
}

bool hasVersionTwentyOneFields(const LayerBuilder& builder)
{
    return builder.attachmentKind.value || builder.attachmentField.value ||
        builder.attachmentStartAnchor.value || builder.attachmentEndAnchor.value ||
        builder.attachmentGlyph.value || builder.attachmentCount.value ||
        builder.attachmentSize.value || builder.attachmentAspect.value ||
        builder.attachmentInset.value || builder.attachmentRotation.value ||
        builder.attachmentJitter.value || builder.attachmentSelection.value ||
        builder.attachmentLineWidth.value || builder.attachmentLength.value ||
        builder.attachmentBranching.value || builder.attachmentSoftness.value ||
        builder.attachmentColour.value || builder.attachmentHeight.value ||
        builder.attachmentRoughness.value || builder.attachmentMetalness.value ||
        builder.attachmentOcclusion.value || builder.attachmentEmissive.value ||
        builder.attachmentSeedOffset.value;
}

bool hasVersionTwentyTwoFields(const LayerBuilder& builder)
{
    return builder.leafInnerHighlightWidth.value ||
        builder.leafInnerHighlightInset.value ||
        builder.leafClusterColourVariation.value ||
        builder.leafInstanceColourVariation.value ||
        builder.leafSecondaryProfile.value || builder.leafSecondaryWeight.value ||
        builder.leafSecondaryScale.value || builder.leafSecondaryLowColour.value ||
        builder.leafSecondaryHighColour.value || builder.leafTertiaryProfile.value ||
        builder.leafTertiaryWeight.value || builder.leafTertiaryScale.value ||
        builder.leafTertiaryLowColour.value || builder.leafTertiaryHighColour.value ||
        builder.organicAccumulationProfile.value ||
        builder.organicAccumulationField.value ||
        builder.organicAccumulationOutlineWidth.value ||
        builder.organicAccumulationInnerHighlightWidth.value ||
        builder.organicAccumulationInnerHighlightInset.value;
}

bool hasStructuralFields(const LayerBuilder& builder)
{
    return hasVersionFourFields(builder) || hasVersionFiveFields(builder) ||
        hasVersionSixFields(builder) || hasVersionSevenFields(builder) ||
        hasVersionEightFields(builder) || hasVersionNineFields(builder) ||
        hasVersionTenFields(builder) || hasVersionElevenFields(builder) ||
        hasVersionTwelveFields(builder) || hasVersionThirteenFields(builder) ||
        hasVersionFourteenFields(builder) || hasVersionTwentyFields(builder) ||
        hasVersionTwentyOneFields(builder) || hasVersionTwentyTwoFields(builder);
}

template<typename Value>
bool storeValue(
    ParsedValue<Value>& destination,
    Value value,
    std::size_t line,
    std::size_t column)
{
    if (destination.value) {
        return false;
    }
    destination.value = std::move(value);
    destination.line = line;
    destination.column = column;
    return true;
}

ParseDiagnostic missingLayerField(
    std::size_t line,
    std::size_t index,
    std::string_view property)
{
    return diagnostic(
        line,
        1,
        "missing required key 'layer." + std::to_string(index) + "." +
            std::string(property) + "'");
}

template<typename Value>
std::optional<ParseDiagnostic> unexpectedLayerField(
    const ParsedValue<Value>& field,
    std::size_t index,
    std::string_view property,
    std::string_view operation)
{
    if (!field.value) {
        return std::nullopt;
    }
    return diagnostic(
        field.line,
        field.column,
        "layer." + std::to_string(index) + "." + std::string(property) +
            " is not valid for operation '" + std::string(operation) + "'");
}

} // namespace

ParseResult parsePmat(std::string_view text)
{
    Material material;
    std::uint32_t formatVersion = 0;
    std::uint32_t layerCount = 0;
    std::array<bool, static_cast<std::size_t>(Field::count)> seen{};
    std::array<std::size_t, static_cast<std::size_t>(Field::count)> valueLines{};
    std::array<std::size_t, static_cast<std::size_t>(Field::count)> valueColumns{};
    std::vector<LayerBuilder> layerBuilders;
    std::size_t lineNumber = 0;
    std::size_t offset = 0;

    while (offset <= text.size()) {
        ++lineNumber;
        const auto newline = text.find('\n', offset);
        const auto lineEnd = newline == std::string_view::npos ? text.size() : newline;
        auto line = text.substr(offset, lineEnd - offset);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (const auto comment = line.find('#'); comment != std::string_view::npos) {
            line = line.substr(0, comment);
        }
        const auto content = trim(line);
        if (!content.empty()) {
            const auto equals = content.find('=');
            const auto secondEquals = equals == std::string_view::npos
                ? std::string_view::npos
                : content.find('=', equals + 1);
            if (equals == std::string_view::npos || secondEquals != std::string_view::npos) {
                return diagnostic(lineNumber, 1, "expected exactly one '=' between key and value");
            }

            const auto key = trim(content.substr(0, equals));
            const auto value = trim(content.substr(equals + 1));
            const auto contentStart = static_cast<std::size_t>(content.data() - line.data());
            const auto valueColumn = value.empty()
                ? contentStart + equals + 2
                : static_cast<std::size_t>(value.data() - line.data()) + 1;
            if (key.empty()) {
                return diagnostic(lineNumber, contentStart + 1, "key must not be empty");
            }
            if (value.empty()) {
                return diagnostic(lineNumber, valueColumn, "value must not be empty");
            }

            if (const auto field = fieldForKey(key)) {
                const auto fieldIndex = static_cast<std::size_t>(*field);
                if (seen[fieldIndex]) {
                    return diagnostic(lineNumber, 1, "duplicate key '" + std::string(key) + "'");
                }
                seen[fieldIndex] = true;
                valueLines[fieldIndex] = lineNumber;
                valueColumns[fieldIndex] = valueColumn;

                switch (*field) {
                case Field::version:
                    if (!parseInteger(value, formatVersion)) {
                        return diagnostic(lineNumber, valueColumn, "pmat.version must be an integer");
                    }
                    if (formatVersion < minimumSupportedPmatVersion ||
                        formatVersion > currentPmatVersion) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "unsupported .pmat version " + std::to_string(formatVersion));
                    }
                    break;
                case Field::type:
                    if (value != "fbm") {
                        return diagnostic(lineNumber, valueColumn, "material.type must be 'fbm'");
                    }
                    break;
                case Field::seed:
                    if (!parseInteger(value, material.seed)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "material.seed must be an unsigned integer");
                    }
                    break;
                case Field::uid:
                case Field::name:
                case Field::description:
                case Field::category:
                case Field::tags: {
                    if (!material.metadata) {
                        material.metadata.emplace();
                    }
                    if (*field == Field::uid) {
                        material.metadata->uid = value;
                    } else if (*field == Field::name) {
                        material.metadata->name = value;
                    } else if (*field == Field::description) {
                        material.metadata->description = value;
                    } else if (*field == Field::category) {
                        material.metadata->category = value;
                    } else {
                        std::size_t tagOffset = 0;
                        while (tagOffset <= value.size()) {
                            const auto comma = value.find(',', tagOffset);
                            const auto tagEnd = comma == std::string_view::npos
                                ? value.size()
                                : comma;
                            const auto tag = trim(value.substr(tagOffset, tagEnd - tagOffset));
                            if (tag.empty()) {
                                return diagnostic(
                                    lineNumber,
                                    valueColumn + tagOffset,
                                    "material.tags must be a comma-separated list of non-empty tags");
                            }
                            material.metadata->tags.emplace_back(tag);
                            if (comma == std::string_view::npos) {
                                break;
                            }
                            tagOffset = comma + 1;
                        }
                    }
                    break;
                }
                case Field::physicalWidth:
                    if (!parseMetres(value, material.physicalSize.widthMetres)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "material.width must be a decimal metre value such as 1.92m");
                    }
                    break;
                case Field::physicalHeight:
                    if (!parseMetres(value, material.physicalSize.heightMetres)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "material.height must be a decimal metre value such as 0.6m");
                    }
                    break;
                case Field::reliefDepth: {
                    double parsed = 0.0;
                    if (!parseMetres(value, parsed)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "surface.relief_depth must be a decimal metre value such as 0.003m");
                    }
                    material.reliefDepthMetres = parsed;
                    break;
                }
                case Field::lowColour:
                    if (!parseColour(value, material.lowColour)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "colour.low must use 0xRRGGBBAA hexadecimal notation");
                    }
                    break;
                case Field::highColour:
                    if (!parseColour(value, material.highColour)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "colour.high must use 0xRRGGBBAA hexadecimal notation");
                    }
                    break;
                case Field::frequency:
                    if (!parseInteger(value, material.frequency)) {
                        return diagnostic(lineNumber, valueColumn, "noise.frequency must be an integer");
                    }
                    break;
                case Field::octaves:
                    if (!parseInteger(value, material.octaves)) {
                        return diagnostic(lineNumber, valueColumn, "noise.octaves must be an integer");
                    }
                    break;
                case Field::lacunarity:
                    if (!parseInteger(value, material.lacunarity)) {
                        return diagnostic(lineNumber, valueColumn, "noise.lacunarity must be an integer");
                    }
                    break;
                case Field::gain:
                    if (!parseDouble(value, material.gain)) {
                        return diagnostic(lineNumber, valueColumn, "noise.gain must be a decimal number");
                    }
                    break;
                case Field::normalStrength:
                    if (!parseDouble(value, material.normalStrength)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "normal.strength must be a decimal number");
                    }
                    break;
                case Field::roughnessLow:
                    if (!parseDouble(value, material.roughnessLow)) {
                        return diagnostic(lineNumber, valueColumn, "roughness.low must be a decimal number");
                    }
                    break;
                case Field::roughnessHigh:
                    if (!parseDouble(value, material.roughnessHigh)) {
                        return diagnostic(lineNumber, valueColumn, "roughness.high must be a decimal number");
                    }
                    break;
                case Field::metalnessLow:
                    if (!parseDouble(value, material.metalnessLow)) {
                        return diagnostic(lineNumber, valueColumn, "metalness.low must be a decimal number");
                    }
                    break;
                case Field::metalnessHigh:
                    if (!parseDouble(value, material.metalnessHigh)) {
                        return diagnostic(lineNumber, valueColumn, "metalness.high must be a decimal number");
                    }
                    break;
                case Field::dielectricIor:
                    if (!parseDouble(value, material.dielectricIor)) {
                        return diagnostic(lineNumber, valueColumn, "surface.ior must be a decimal number");
                    }
                    break;
                case Field::coatingLow:
                    if (!parseDouble(value, material.coatingLow)) {
                        return diagnostic(lineNumber, valueColumn, "coating.low must be a decimal number");
                    }
                    break;
                case Field::coatingHigh:
                    if (!parseDouble(value, material.coatingHigh)) {
                        return diagnostic(lineNumber, valueColumn, "coating.high must be a decimal number");
                    }
                    break;
                case Field::occlusionLow:
                    if (!parseDouble(value, material.occlusionLow)) {
                        return diagnostic(lineNumber, valueColumn, "occlusion.low must be a decimal number");
                    }
                    break;
                case Field::occlusionHigh:
                    if (!parseDouble(value, material.occlusionHigh)) {
                        return diagnostic(lineNumber, valueColumn, "occlusion.high must be a decimal number");
                    }
                    break;
                case Field::clearCoatLow:
                    if (!parseDouble(value, material.clearCoatLow)) {
                        return diagnostic(lineNumber, valueColumn, "clearcoat.low must be a decimal number");
                    }
                    break;
                case Field::clearCoatHigh:
                    if (!parseDouble(value, material.clearCoatHigh)) {
                        return diagnostic(lineNumber, valueColumn, "clearcoat.high must be a decimal number");
                    }
                    break;
                case Field::clearCoatRoughnessLow:
                    if (!parseDouble(value, material.clearCoatRoughnessLow)) {
                        return diagnostic(lineNumber, valueColumn, "clearcoat.roughness_low must be a decimal number");
                    }
                    break;
                case Field::clearCoatRoughnessHigh:
                    if (!parseDouble(value, material.clearCoatRoughnessHigh)) {
                        return diagnostic(lineNumber, valueColumn, "clearcoat.roughness_high must be a decimal number");
                    }
                    break;
                case Field::emissiveIntensity:
                    if (!parseDouble(value, material.emissiveIntensity)) {
                        return diagnostic(lineNumber, valueColumn, "emissive.intensity must be a decimal number");
                    }
                    break;
                case Field::anisotropyStrength:
                    if (!parseDouble(value, material.anisotropyStrength)) {
                        return diagnostic(lineNumber, valueColumn, "anisotropy.strength must be a decimal number");
                    }
                    break;
                case Field::anisotropyRotation:
                    if (!parseDouble(value, material.anisotropyRotationDegrees)) {
                        return diagnostic(lineNumber, valueColumn, "anisotropy.rotation must be decimal degrees");
                    }
                    break;
                case Field::layerCount:
                    if (!parseInteger(value, layerCount)) {
                        return diagnostic(lineNumber, valueColumn, "layers.count must be an integer");
                    }
                    if (layerCount > LayerLimits::maximumLayers) {
                        return diagnostic(lineNumber, valueColumn, "layers.count must not exceed 32");
                    }
                    break;
                case Field::count:
                    break;
                }
            } else if (key.starts_with("layer.")) {
                const auto parsedKey = parseLayerKey(key);
                if (!parsedKey) {
                    return diagnostic(lineNumber, 1, "invalid layer key '" + std::string(key) + "'");
                }
                const auto [layerIndex, property] = *parsedKey;
                if (layerIndex >= LayerLimits::maximumLayers) {
                    return diagnostic(lineNumber, 1, "layer index must be between 0 and 31");
                }
                if (layerBuilders.size() <= layerIndex) {
                    layerBuilders.resize(layerIndex + 1);
                }
                auto& builder = layerBuilders[layerIndex];
                const auto duplicate = [&] {
                    return diagnostic(lineNumber, 1, "duplicate key '" + std::string(key) + "'");
                };

                if (property == "enabled") {
                    bool parsed = false;
                    if (!parseBoolean(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "layer enabled must be true or false");
                    }
                    if (!storeValue(builder.enabled, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "opacity") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "layer opacity must be a decimal number");
                    }
                    if (!storeValue(builder.opacity, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "composite") {
                    const auto parsed = parseCompositeMode(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "layer composite must be 'blend', 'add', 'multiply', 'minimum', 'maximum', or 'detail'");
                    }
                    if (!storeValue(builder.compositeMode, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "operation") {
                    const auto parsed = parseOperationKind(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "layer operation is not supported");
                    }
                    if (!storeValue(builder.operation, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "outputs") {
                    const auto parsed = parseLayerOutputs(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "layer outputs must list one or more supported material channels without duplicates");
                    }
                    if (!storeValue(builder.outputs, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "noise.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "noise seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.seedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "solid.colour") {
                    Rgba8 parsed;
                    if (!parseColour(value, parsed)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "solid colour must use 0xRRGGBBAA hexadecimal notation");
                    }
                    if (!storeValue(builder.solidColour, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "surface.value") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "surface value must be a decimal number");
                    }
                    if (!storeValue(builder.surfaceValue, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "levels.input_low") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "levels input low must be a decimal number");
                    }
                    if (!storeValue(builder.levelsLow, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "levels.input_high") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "levels input high must be a decimal number");
                    }
                    if (!storeValue(builder.levelsHigh, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "levels.gamma") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "levels gamma must be a decimal number");
                    }
                    if (!storeValue(builder.levelsGamma, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "threshold.value") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "threshold must be a decimal number");
                    }
                    if (!storeValue(builder.threshold, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.columns") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "brick columns must be an integer");
                    }
                    if (!storeValue(builder.brickColumns, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.rows") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "brick rows must be an integer");
                    }
                    if (!storeValue(builder.brickRows, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.mortar") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "brick mortar must be a decimal number");
                    }
                    if (!storeValue(builder.brickMortar, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.mortar_space") {
                    const auto parsed = parseBrickMortarSpace(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "brick mortar space must be 'cell' or 'texture'");
                    }
                    if (!storeValue(
                            builder.brickMortarSpace,
                            *parsed,
                            lineNumber,
                            valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.sizing") {
                    const auto parsed = parseBrickSizing(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "brick sizing must be 'relative' or 'physical'");
                    }
                    if (!storeValue(builder.brickSizing, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.width") {
                    double parsed = 0.0;
                    if (!parseMetres(value, parsed)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "brick width must be a decimal metre value such as 0.24m");
                    }
                    if (!storeValue(builder.brickWidthMetres, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.height") {
                    double parsed = 0.0;
                    if (!parseMetres(value, parsed)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "brick height must be a decimal metre value such as 0.075m");
                    }
                    if (!storeValue(builder.brickHeightMetres, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.mortar_width") {
                    double parsed = 0.0;
                    if (!parseMetres(value, parsed)) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "brick mortar width must be a decimal metre value such as 0.01m");
                    }
                    if (!storeValue(builder.brickMortarMetres, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.stagger") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "brick stagger must be a decimal number");
                    }
                    if (!storeValue(builder.brickStagger, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "brick.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "brick softness must be a decimal number");
                    }
                    if (!storeValue(builder.brickSoftness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "tile.columns") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "tile columns must be an integer");
                    }
                    if (!storeValue(builder.tileColumns, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "tile.rows") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "tile rows must be an integer");
                    }
                    if (!storeValue(builder.tileRows, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "tile.grout") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "tile grout must be a decimal number");
                    }
                    if (!storeValue(builder.tileGrout, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "tile.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "tile softness must be a decimal number");
                    }
                    if (!storeValue(builder.tileSoftness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "worley.columns") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "Worley columns must be an integer");
                    }
                    if (!storeValue(builder.worleyColumns, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "worley.rows") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "Worley rows must be an integer");
                    }
                    if (!storeValue(builder.worleyRows, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "worley.jitter") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "Worley jitter must be a decimal number");
                    }
                    if (!storeValue(builder.worleyJitter, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "worley.edge_width") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "Worley edge width must be a decimal number");
                    }
                    if (!storeValue(builder.worleyEdgeWidth, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "worley.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "Worley seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.worleySeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "random_cells.columns") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "random-cell columns must be an integer");
                    }
                    if (!storeValue(builder.randomColumns, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "random_cells.rows") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "random-cell rows must be an integer");
                    }
                    if (!storeValue(builder.randomRows, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "random_cells.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "random-cell seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.randomSeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lines.direction") {
                    const auto parsed = parseLineDirection(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "line direction must be 'vertical' or 'horizontal'");
                    }
                    if (!storeValue(builder.lineDirection, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lines.count") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "line count must be an integer");
                    }
                    if (!storeValue(builder.lineCount, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lines.width") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "line width must be a decimal number");
                    }
                    if (!storeValue(builder.lineWidth, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lines.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "line softness must be a decimal number");
                    }
                    if (!storeValue(builder.lineSoftness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "rectangles.columns") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "rectangle columns must be an integer");
                    }
                    if (!storeValue(builder.rectangleColumns, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "rectangles.rows") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "rectangle rows must be an integer");
                    }
                    if (!storeValue(builder.rectangleRows, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "rectangles.width") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "rectangle width must be a decimal number");
                    }
                    if (!storeValue(builder.rectangleWidth, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "rectangles.height") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "rectangle height must be a decimal number");
                    }
                    if (!storeValue(builder.rectangleHeight, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "rectangles.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "rectangle softness must be a decimal number");
                    }
                    if (!storeValue(builder.rectangleSoftness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "circles.columns") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "circle columns must be an integer");
                    }
                    if (!storeValue(builder.circleColumns, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "circles.rows") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "circle rows must be an integer");
                    }
                    if (!storeValue(builder.circleRows, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "circles.radius") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "circle radius must be a decimal number");
                    }
                    if (!storeValue(builder.circleRadius, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "circles.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "circle softness must be a decimal number");
                    }
                    if (!storeValue(builder.circleSoftness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "surface.kind") {
                    const auto parsed = parseSurfacePatternKind(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "surface kind must be 'ridged_noise', 'bands', 'rings', 'scatter', or 'streaks'");
                    }
                    if (!storeValue(builder.surfaceKind, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "surface.scale") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "surface scale must be an integer");
                    }
                    if (!storeValue(builder.surfaceScale, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "surface.width") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "surface width must be a decimal number");
                    }
                    if (!storeValue(builder.surfaceWidth, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "surface.detail") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "surface detail must be a decimal number");
                    }
                    if (!storeValue(builder.surfaceDetail, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "surface.distortion") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "surface distortion must be a decimal number");
                    }
                    if (!storeValue(builder.surfaceDistortion, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "surface.variation") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "surface variation must be a decimal number");
                    }
                    if (!storeValue(builder.surfaceVariation, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "surface.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "surface seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.surfaceSeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "filter.kind") {
                    const auto parsed = parseSurfaceFilterKind(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "filter kind must be 'invert', 'soften', 'expand', 'contract', 'edge', 'slope', 'cavity', 'peaks', or 'edge_aware_soften'");
                    }
                    if (!storeValue(builder.filterKind, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "filter.radius") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "filter radius must be a decimal number");
                    }
                    if (!storeValue(builder.filterRadius, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "filter.strength") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "filter strength must be a decimal number");
                    }
                    if (!storeValue(builder.filterStrength, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "filter.sensitivity") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "filter sensitivity must be a decimal number");
                    }
                    if (!storeValue(builder.filterSensitivity, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "filter.target") {
                    const auto parsed = parseProcessingTarget(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "filter target must be 'colour', 'scalar', or 'all'");
                    }
                    if (!storeValue(builder.filterTarget, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "posterise.bands") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "posterise bands must be an integer");
                    }
                    if (!storeValue(builder.posteriseBands, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "posterise.target") {
                    const auto parsed = parseProcessingTarget(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "posterise target must be 'colour', 'scalar', or 'all'");
                    }
                    if (!storeValue(builder.posteriseTarget, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "ramp.mode") {
                    const auto parsed = parseColourRampMode(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "colour ramp mode must be 'linear' or 'stepped'");
                    }
                    if (!storeValue(builder.rampMode, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "ramp.stops") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "colour ramp stop count must be an integer");
                    }
                    if (!storeValue(builder.rampStopCount, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property.starts_with("ramp.stop.")) {
                    const auto indexed = parseIndexedProperty(property, "ramp.stop.");
                    if (!indexed || indexed->first >= LayerLimits::maximumColourStops) {
                        return diagnostic(lineNumber, 1, "colour ramp stop index must be between 0 and 7");
                    }
                    const auto stopIndex = indexed->first;
                    if (indexed->second == "position") {
                        double parsed = 0.0;
                        if (!parseDouble(value, parsed)) {
                            return diagnostic(lineNumber, valueColumn, "colour ramp stop position must be a decimal number");
                        }
                        if (!storeValue(builder.rampPositions[stopIndex], parsed, lineNumber, valueColumn)) {
                            return duplicate();
                        }
                    } else if (indexed->second == "colour") {
                        Rgba8 parsed;
                        if (!parseColour(value, parsed)) {
                            return diagnostic(lineNumber, valueColumn, "colour ramp stop colour must use 0xRRGGBBAA notation");
                        }
                        if (!storeValue(builder.rampColours[stopIndex], parsed, lineNumber, valueColumn)) {
                            return duplicate();
                        }
                    } else {
                        return diagnostic(lineNumber, 1, "unknown colour ramp stop key '" + std::string(key) + "'");
                    }
                } else if (property == "palette.colours") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "palette colour count must be an integer");
                    }
                    if (!storeValue(builder.paletteColourCount, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property.starts_with("palette.entry.")) {
                    const auto indexed = parseIndexedProperty(property, "palette.entry.");
                    if (!indexed || indexed->first >= LayerLimits::maximumColourStops ||
                        indexed->second != "colour") {
                        return diagnostic(lineNumber, 1, "palette entry must be palette.entry.0.colour through palette.entry.7.colour");
                    }
                    Rgba8 parsed;
                    if (!parseColour(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "palette colour must use 0xRRGGBBAA notation");
                    }
                    if (!storeValue(builder.paletteColours[indexed->first], parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "ink.colour") {
                    Rgba8 parsed;
                    if (!parseColour(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "ink colour must use 0xRRGGBBAA notation");
                    }
                    if (!storeValue(builder.inkColour, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "ink.radius") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "ink radius must be a decimal number");
                    }
                    if (!storeValue(builder.inkRadius, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "ink.threshold") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "ink threshold must be a decimal number");
                    }
                    if (!storeValue(builder.inkThreshold, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "ink.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "ink softness must be a decimal number");
                    }
                    if (!storeValue(builder.inkSoftness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "ink.strength") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "ink strength must be a decimal number");
                    }
                    if (!storeValue(builder.inkStrength, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "ink.inverted") {
                    bool parsed = false;
                    if (!parseBoolean(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "ink inverted must be true or false");
                    }
                    if (!storeValue(builder.inkInverted, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.field") {
                    const auto parsed = parseRegionSurfaceField(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "sculpt field must be 'height', 'cavity', 'outer_edge', 'exposed_face', 'facet', or 'wear'");
                    }
                    if (!storeValue(builder.sculptField, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.profile") {
                    const auto parsed = parseBevelProfile(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "sculpt profile must be 'rounded', 'chamfered', or 'hand_cut'");
                    }
                    if (!storeValue(builder.sculptProfile, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.bevel_width") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt bevel width must be a decimal number");
                    }
                    if (!storeValue(builder.sculptBevelWidth, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.bevel_height") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt bevel height must be a decimal number");
                    }
                    if (!storeValue(builder.sculptBevelHeight, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.facet_count") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt facet count must be an integer");
                    }
                    if (!storeValue(builder.sculptFacetCount, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.facet_strength") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt facet strength must be a decimal number");
                    }
                    if (!storeValue(builder.sculptFacetStrength, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.centre_peak") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt centre peak must be a decimal number");
                    }
                    if (!storeValue(builder.sculptCentrePeak, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.slope") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt slope must be a decimal number");
                    }
                    if (!storeValue(builder.sculptSlope, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.chips") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt chips must be a decimal number");
                    }
                    if (!storeValue(builder.sculptChips, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.chip_scale") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt chip scale must be an integer");
                    }
                    if (!storeValue(builder.sculptChipScale, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.wear") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt wear must be a decimal number");
                    }
                    if (!storeValue(builder.sculptWear, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.erosion") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt erosion must be a decimal number");
                    }
                    if (!storeValue(builder.sculptErosion, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.sculptSeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.faceted_normals") {
                    bool parsed = false;
                    if (!parseBoolean(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "sculpt faceted normals must be true or false");
                    }
                    if (!storeValue(builder.sculptFacetedNormals, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "sculpt.target") {
                    const auto parsed = parseProcessingTarget(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "sculpt target must be 'colour', 'scalar', or 'all'");
                    }
                    if (!storeValue(builder.sculptTarget, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.profile") {
                    const auto parsed = parseCourseLayoutProfile(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "course profile must be 'masonry', 'slabs', or 'slates'");
                    }
                    if (!storeValue(builder.courseProfile, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.field") {
                    const auto parsed = parseCourseLayoutField(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "course field must be 'blocks', 'mortar', 'course', or 'overlap'");
                    }
                    if (!storeValue(builder.courseField, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.sizing") {
                    const auto parsed = parseBrickSizing(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "course sizing must be 'relative' or 'physical'");
                    }
                    if (!storeValue(builder.courseSizing, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.blocks") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course blocks must be an integer");
                    }
                    if (!storeValue(builder.courseBlocks, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.courses") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course count must be an integer");
                    }
                    if (!storeValue(builder.courseCount, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.block_variation") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course block variation must be a decimal number");
                    }
                    if (!storeValue(builder.courseBlockVariation, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.height_variation") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course height variation must be a decimal number");
                    }
                    if (!storeValue(builder.courseHeightVariation, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.stagger") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course stagger must be a decimal number");
                    }
                    if (!storeValue(builder.courseStagger, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.crookedness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course crookedness must be a decimal number");
                    }
                    if (!storeValue(builder.courseCrookedness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.gap") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course gap must be a decimal number");
                    }
                    if (!storeValue(builder.courseGap, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course softness must be a decimal number");
                    }
                    if (!storeValue(builder.courseSoftness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.overlap") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course overlap must be a decimal number");
                    }
                    if (!storeValue(builder.courseOverlap, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.courseSeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.block_width") {
                    double parsed = 0.0;
                    if (!parseMetres(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course block width must be a decimal metre value");
                    }
                    if (!storeValue(builder.courseBlockWidthMetres, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.course_height") {
                    double parsed = 0.0;
                    if (!parseMetres(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course height must be a decimal metre value");
                    }
                    if (!storeValue(builder.courseHeightMetres, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.gap_width") {
                    double parsed = 0.0;
                    if (!parseMetres(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course gap width must be a decimal metre value");
                    }
                    if (!storeValue(builder.courseGapMetres, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "course.overlap_depth") {
                    double parsed = 0.0;
                    if (!parseMetres(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "course overlap depth must be a decimal metre value");
                    }
                    if (!storeValue(builder.courseOverlapMetres, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "region.field") {
                    const auto parsed = parseRegionFieldKind(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "region field must be 'random', 'local_u', 'local_v', 'centre_distance', 'boundary_distance', or 'course_random'");
                    }
                    if (!storeValue(builder.regionField, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "region.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "region seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.regionSeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "region.channel") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "region channel must be an integer");
                    }
                    if (!storeValue(builder.regionChannel, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "region.output_low") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "region output low must be a decimal number");
                    }
                    if (!storeValue(builder.regionOutputLow, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "region.output_high") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "region output high must be a decimal number");
                    }
                    if (!storeValue(builder.regionOutputHigh, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "region.inverted") {
                    bool parsed = false;
                    if (!parseBoolean(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "region inverted must be true or false");
                    }
                    if (!storeValue(builder.regionInverted, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "region.target") {
                    const auto parsed = parseProcessingTarget(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "region target must be 'colour', 'scalar', or 'all'");
                    }
                    if (!storeValue(builder.regionTarget, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.kind") {
                    const auto parsed = parseShapePrimitiveKind(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "shape kind must be 'rounded_rectangle', 'ellipse', 'capsule', 'diamond', 'convex_polygon', 'annulus', 'arc', 'sector', or 'crescent'");
                    }
                    if (!storeValue(builder.shapeKind, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.field") {
                    const auto parsed = parseShapeFieldKind(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "shape field must be 'fill', 'inset', 'outline', or 'border'");
                    }
                    if (!storeValue(builder.shapeField, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.columns") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape columns must be an integer");
                    }
                    if (!storeValue(builder.shapeColumns, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.rows") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape rows must be an integer");
                    }
                    if (!storeValue(builder.shapeRows, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.width") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape width must be a decimal number");
                    }
                    if (!storeValue(builder.shapeWidth, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.height") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape height must be a decimal number");
                    }
                    if (!storeValue(builder.shapeHeight, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.corner_radius") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape corner radius must be a decimal number");
                    }
                    if (!storeValue(builder.shapeCornerRadius, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.inset") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape inset must be a decimal number");
                    }
                    if (!storeValue(builder.shapeInset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.border_width") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape border width must be a decimal number");
                    }
                    if (!storeValue(builder.shapeBorderWidth, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape softness must be a decimal number");
                    }
                    if (!storeValue(builder.shapeSoftness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.offset_x") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape offset X must be a decimal number");
                    }
                    if (!storeValue(builder.shapeOffsetX, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.offset_y") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape offset Y must be a decimal number");
                    }
                    if (!storeValue(builder.shapeOffsetY, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.stagger") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape stagger must be a decimal number");
                    }
                    if (!storeValue(builder.shapeStagger, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.rotation") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "local shape rotation must be a decimal degree value");
                    }
                    if (!storeValue(builder.shapeRotation, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.inner_radius") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape inner radius must be a decimal number");
                    }
                    if (!storeValue(builder.shapeInnerRadius, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.arc_start") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape arc start must be a decimal degree value");
                    }
                    if (!storeValue(builder.shapeArcStart, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.arc_sweep") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape arc sweep must be a decimal degree value");
                    }
                    if (!storeValue(builder.shapeArcSweep, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.crescent_offset") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape crescent offset must be a decimal number");
                    }
                    if (!storeValue(builder.shapeCrescentOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.radial_copies") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape radial copies must be an integer");
                    }
                    if (!storeValue(builder.shapeRadialCopies, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.radial_radius") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape radial radius must be a decimal number");
                    }
                    if (!storeValue(builder.shapeRadialRadius, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.radial_phase") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape radial phase must be a decimal degree value");
                    }
                    if (!storeValue(builder.shapeRadialPhase, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.radial_orientation") {
                    const auto parsed = parseRadialOrientation(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "shape radial orientation must be 'fixed', 'outward', or 'tangent'");
                    }
                    if (!storeValue(builder.shapeRadialOrientation, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.shapeSeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.vertices") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape vertex count must be an integer");
                    }
                    if (!storeValue(builder.shapeVertexCount, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (const auto vertex =
                               parseIndexedProperty(property, "shape.vertex.")) {
                    const auto [vertexIndex, member] = *vertex;
                    if (vertexIndex >= LayerLimits::maximumPolygonVertices ||
                        (member != "x" && member != "y")) {
                        return diagnostic(lineNumber, valueColumn, "invalid shape vertex property");
                    }
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "shape vertex coordinates must be decimal numbers");
                    }
                    auto& destination = member == "x"
                        ? builder.shapeVertexX[vertexIndex]
                        : builder.shapeVertexY[vertexIndex];
                    if (!storeValue(destination, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.boolean") {
                    const auto parsed = parseShapeBooleanMode(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "shape Boolean mode must be 'union', 'intersection', or 'subtraction'");
                    }
                    if (!storeValue(builder.shapeBooleanMode, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "shape.target") {
                    const auto parsed = parseProcessingTarget(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "shape target must be 'colour', 'scalar', or 'all'");
                    }
                    if (!storeValue(builder.shapeBooleanTarget, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lattice.kind") {
                    const auto parsed = parseLatticeKind(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "lattice kind must be 'lines' or 'diamonds'");
                    }
                    if (!storeValue(builder.latticeKind, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lattice.winding_x") {
                    std::int32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "lattice winding X must be an integer");
                    }
                    if (!storeValue(builder.latticeWindingX, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lattice.winding_y") {
                    std::int32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "lattice winding Y must be an integer");
                    }
                    if (!storeValue(builder.latticeWindingY, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lattice.width") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "lattice width must be a decimal number");
                    }
                    if (!storeValue(builder.latticeWidth, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lattice.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "lattice softness must be a decimal number");
                    }
                    if (!storeValue(builder.latticeSoftness, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "lattice.phase") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "lattice phase must be a decimal number");
                    }
                    if (!storeValue(builder.latticePhase, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "scatter.field") {
                    const auto parsed = parseScatterField(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "scatter field must be 'material', 'fill', 'instance_random', 'local_u', 'local_v', or 'boundary_distance'");
                    }
                    if (!storeValue(builder.scatterField, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "scatter.columns") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "scatter columns must be an integer");
                    }
                    if (!storeValue(builder.scatterColumns, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "scatter.rows") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "scatter rows must be an integer");
                    }
                    if (!storeValue(builder.scatterRows, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "scatter.density" ||
                           property == "scatter.jitter" ||
                           property == "scatter.minimum_distance" ||
                           property == "scatter.maximum_overlap") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "scatter parameter must be a decimal number");
                    }
                    auto* destination = property == "scatter.density"
                        ? &builder.scatterDensity
                        : property == "scatter.jitter"
                            ? &builder.scatterJitter
                            : property == "scatter.minimum_distance"
                                ? &builder.scatterMinimumDistance
                                : &builder.scatterMaximumOverlap;
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "scatter.overlap") {
                    const auto parsed = parseScatterOverlapMode(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "scatter overlap must be 'forbidden', 'controlled', or 'unrestricted'");
                    }
                    if (!storeValue(builder.scatterOverlapMode, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "scatter.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "scatter seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.scatterSeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "scatter.populations") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "scatter population count must be an integer");
                    }
                    if (!storeValue(builder.scatterPopulationCount, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (const auto population =
                               parseIndexedProperty(property, "scatter.population.")) {
                    const auto [populationIndex, member] = *population;
                    if (populationIndex >= LayerLimits::maximumScatterPopulations) {
                        return diagnostic(lineNumber, valueColumn, "scatter population index is out of range");
                    }
                    auto& destination = builder.scatterPopulations[populationIndex];
                    if (member == "colour_low" || member == "colour_high") {
                        Rgba8 parsed{};
                        if (!parseColour(value, parsed)) {
                            return diagnostic(lineNumber, valueColumn, "scatter population colour must use 0xRRGGBBAA hexadecimal notation");
                        }
                        auto& field = member == "colour_low"
                            ? destination.lowColour
                            : destination.highColour;
                        if (!storeValue(field, parsed, lineNumber, valueColumn)) {
                            return duplicate();
                        }
                    } else {
                        double parsed = 0.0;
                        if (!parseDouble(value, parsed)) {
                            return diagnostic(lineNumber, valueColumn, "scatter population parameter must be a decimal number");
                        }
                        ParsedValue<double>* field = nullptr;
                        if (member == "weight") field = &destination.weight;
                        else if (member == "min_scale") field = &destination.minimumScale;
                        else if (member == "max_scale") field = &destination.maximumScale;
                        else if (member == "min_aspect") field = &destination.minimumAspect;
                        else if (member == "max_aspect") field = &destination.maximumAspect;
                        else if (member == "min_rotation") field = &destination.minimumRotation;
                        else if (member == "max_rotation") field = &destination.maximumRotation;
                        else if (member == "min_height") field = &destination.minimumHeight;
                        else if (member == "max_height") field = &destination.maximumHeight;
                        else if (member == "min_roughness") field = &destination.minimumRoughness;
                        else if (member == "max_roughness") field = &destination.maximumRoughness;
                        if (field == nullptr) {
                            return diagnostic(lineNumber, valueColumn, "invalid scatter population property");
                        }
                        if (!storeValue(*field, parsed, lineNumber, valueColumn)) {
                            return duplicate();
                        }
                    }
                } else if (property.starts_with("scatter.density_mask.") ||
                           property.starts_with("scatter.exclusion_mask.")) {
                    constexpr std::string_view densityPrefix = "scatter.density_mask.";
                    constexpr std::string_view exclusionPrefix = "scatter.exclusion_mask.";
                    const bool isDensity = property.starts_with(densityPrefix);
                    const auto member = property.substr(
                        isDensity ? densityPrefix.size() : exclusionPrefix.size());
                    auto& mask = isDensity
                        ? builder.scatterDensityMask
                        : builder.scatterExclusionMask;
                    if (member == "enabled" || member == "inverted") {
                        bool parsed = false;
                        if (!parseBoolean(value, parsed)) {
                            return diagnostic(lineNumber, valueColumn, "scatter mask flag must be true or false");
                        }
                        auto& field = member == "enabled" ? mask.enabled : mask.inverted;
                        if (!storeValue(field, parsed, lineNumber, valueColumn)) {
                            return duplicate();
                        }
                    } else if (member == "frequency") {
                        std::uint32_t parsed = 0;
                        if (!parseInteger(value, parsed)) {
                            return diagnostic(lineNumber, valueColumn, "scatter mask frequency must be an integer");
                        }
                        if (!storeValue(mask.frequency, parsed, lineNumber, valueColumn)) {
                            return duplicate();
                        }
                    } else if (member == "seed_offset") {
                        std::uint64_t parsed = 0;
                        if (!parseInteger(value, parsed)) {
                            return diagnostic(lineNumber, valueColumn, "scatter mask seed offset must be an unsigned integer");
                        }
                        if (!storeValue(mask.seedOffset, parsed, lineNumber, valueColumn)) {
                            return duplicate();
                        }
                    } else if (member == "input_low" || member == "input_high") {
                        double parsed = 0.0;
                        if (!parseDouble(value, parsed)) {
                            return diagnostic(lineNumber, valueColumn, "scatter mask threshold must be a decimal number");
                        }
                        auto& field = member == "input_low" ? mask.inputLow : mask.inputHigh;
                        if (!storeValue(field, parsed, lineNumber, valueColumn)) {
                            return duplicate();
                        }
                    } else {
                        return diagnostic(lineNumber, valueColumn, "invalid scatter mask property");
                    }
                } else if (property == "organic.cell.field") {
                    const auto parsed = parseOrganicCellField(value);
                    if (!parsed || !storeValue(builder.organicCellField, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic cell field must be 'plates', 'boundaries', or 'cell_random'");
                    }
                } else if (property == "organic.cell.direction") {
                    const auto parsed = parseOrganicDirection(value);
                    if (!parsed || !storeValue(builder.organicCellDirection, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic direction must be 'vertical' or 'horizontal'");
                    }
                } else if (property == "organic.cell.columns" || property == "organic.cell.rows") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic cell count must be an integer");
                    auto& destination = property == "organic.cell.columns"
                        ? builder.organicCellColumns : builder.organicCellRows;
                    if (!storeValue(destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "organic.cell.anisotropy" ||
                           property == "organic.cell.jitter" ||
                           property == "organic.cell.irregularity" ||
                           property == "organic.cell.gap" ||
                           property == "organic.cell.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic cell parameter must be a decimal number");
                    ParsedValue<double>* destination = property == "organic.cell.anisotropy"
                        ? &builder.organicCellAnisotropy
                        : property == "organic.cell.jitter" ? &builder.organicCellJitter
                        : property == "organic.cell.irregularity" ? &builder.organicCellIrregularity
                        : property == "organic.cell.gap" ? &builder.organicCellGap
                        : &builder.organicCellSoftness;
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "organic.cell.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic cell seed offset must be an unsigned integer");
                    if (!storeValue(builder.organicCellSeedOffset, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "organic.crack.field") {
                    const auto parsed = parseOrganicCrackField(value);
                    if (!parsed || !storeValue(builder.organicCrackField, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic crack field must be 'cracks', 'trunks', 'branches', 'hierarchy', or 'distance'");
                    }
                } else if (property == "organic.crack.direction") {
                    const auto parsed = parseOrganicDirection(value);
                    if (!parsed || !storeValue(builder.organicCrackDirection, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic direction must be 'vertical' or 'horizontal'");
                    }
                } else if (property == "organic.crack.roots" ||
                           property == "organic.crack.segments" ||
                           property == "organic.crack.branch_levels") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic crack count must be an integer");
                    ParsedValue<std::uint32_t>* destination = property == "organic.crack.roots"
                        ? &builder.organicCrackRoots
                        : property == "organic.crack.segments" ? &builder.organicCrackSegments
                        : &builder.organicCrackBranchLevels;
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "organic.crack.branch_probability" ||
                           property == "organic.crack.bend" ||
                           property == "organic.crack.width" ||
                           property == "organic.crack.taper" ||
                           property == "organic.crack.softness") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic crack parameter must be a decimal number");
                    ParsedValue<double>* destination = property == "organic.crack.branch_probability"
                        ? &builder.organicCrackBranchProbability
                        : property == "organic.crack.bend" ? &builder.organicCrackBend
                        : property == "organic.crack.width" ? &builder.organicCrackWidth
                        : property == "organic.crack.taper" ? &builder.organicCrackTaper
                        : &builder.organicCrackSoftness;
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "organic.crack.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic crack seed offset must be an unsigned integer");
                    if (!storeValue(builder.organicCrackSeedOffset, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "leaf.field") {
                    const auto parsed = parseLeafField(value);
                    if (!parsed || !storeValue(builder.leafField, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "leaf field must be 'material', 'fill', 'edge', 'midrib', 'veins', 'instance_random', 'outline', 'inner_highlight', 'cluster_random', or 'population'");
                    }
                } else if (property == "leaf.profile") {
                    const auto parsed = parseLeafProfile(value);
                    if (!parsed || !storeValue(builder.leafProfile, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "leaf profile must be 'ovate', 'lanceolate', 'cordate', 'lobed', 'blob', 'rosette', or 'lichen'");
                    }
                } else if (property == "leaf.pattern") {
                    const auto parsed = parseLeafClusterPattern(value);
                    if (!parsed || !storeValue(builder.leafPattern, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "leaf pattern must be 'radial', 'fan', 'vine', 'canopy', or 'ground_scatter'");
                    }
                } else if (property == "leaf.secondary_profile" ||
                           property == "leaf.tertiary_profile") {
                    const auto parsed = parseLeafProfile(value);
                    auto& destination = property == "leaf.secondary_profile"
                        ? builder.leafSecondaryProfile : builder.leafTertiaryProfile;
                    if (!parsed || !storeValue(destination, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic population profile is not supported");
                    }
                } else if (property == "leaf.columns" || property == "leaf.rows" ||
                           property == "leaf.per_cluster" || property == "leaf.serration_count" ||
                           property == "leaf.lobe_count" || property == "leaf.vein_pairs") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) return diagnostic(lineNumber, valueColumn, "leaf count must be an integer");
                    ParsedValue<std::uint32_t>* destination = property == "leaf.columns" ? &builder.leafColumns
                        : property == "leaf.rows" ? &builder.leafRows
                        : property == "leaf.per_cluster" ? &builder.leavesPerCluster
                        : property == "leaf.serration_count" ? &builder.leafSerrationCount
                        : property == "leaf.lobe_count" ? &builder.leafLobeCount
                        : &builder.leafVeinPairs;
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "leaf.colour_low" || property == "leaf.colour_high" ||
                           property == "leaf.secondary_colour_low" ||
                           property == "leaf.secondary_colour_high" ||
                           property == "leaf.tertiary_colour_low" ||
                           property == "leaf.tertiary_colour_high") {
                    Rgba8 parsed{};
                    if (!parseColour(value, parsed)) return diagnostic(lineNumber, valueColumn, "leaf colour must use 0xRRGGBBAA hexadecimal notation");
                    auto* destination = property == "leaf.colour_low" ? &builder.leafLowColour
                        : property == "leaf.colour_high" ? &builder.leafHighColour
                        : property == "leaf.secondary_colour_low" ? &builder.leafSecondaryLowColour
                        : property == "leaf.secondary_colour_high" ? &builder.leafSecondaryHighColour
                        : property == "leaf.tertiary_colour_low" ? &builder.leafTertiaryLowColour
                        : &builder.leafTertiaryHighColour;
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "leaf.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) return diagnostic(lineNumber, valueColumn, "leaf seed offset must be an unsigned integer");
                    if (!storeValue(builder.leafSeedOffset, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property.starts_with("leaf.")) {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) return diagnostic(lineNumber, valueColumn, "leaf parameter must be a decimal number");
                    ParsedValue<double>* destination = property == "leaf.density" ? &builder.leafDensity
                        : property == "leaf.cluster_spread" ? &builder.leafClusterSpread
                        : property == "leaf.length" ? &builder.leafLength
                        : property == "leaf.width" ? &builder.leafWidth
                        : property == "leaf.scale_variation" ? &builder.leafScaleVariation
                        : property == "leaf.rotation_variation" ? &builder.leafRotationVariation
                        : property == "leaf.direction" ? &builder.leafDirection
                        : property == "leaf.taper" ? &builder.leafTaper
                        : property == "leaf.base_notch" ? &builder.leafBaseNotch
                        : property == "leaf.curvature" ? &builder.leafCurvature
                        : property == "leaf.serration" ? &builder.leafSerration
                        : property == "leaf.lobing" ? &builder.leafLobing
                        : property == "leaf.midrib_width" ? &builder.leafMidribWidth
                        : property == "leaf.vein_width" ? &builder.leafVeinWidth
                        : property == "leaf.edge_width" ? &builder.leafEdgeWidth
                        : property == "leaf.inner_highlight_width" ? &builder.leafInnerHighlightWidth
                        : property == "leaf.inner_highlight_inset" ? &builder.leafInnerHighlightInset
                        : property == "leaf.cluster_colour_variation" ? &builder.leafClusterColourVariation
                        : property == "leaf.instance_colour_variation" ? &builder.leafInstanceColourVariation
                        : property == "leaf.secondary_weight" ? &builder.leafSecondaryWeight
                        : property == "leaf.secondary_scale" ? &builder.leafSecondaryScale
                        : property == "leaf.tertiary_weight" ? &builder.leafTertiaryWeight
                        : property == "leaf.tertiary_scale" ? &builder.leafTertiaryScale
                        : property == "leaf.softness" ? &builder.leafSoftness
                        : property == "leaf.min_height" ? &builder.leafMinimumHeight
                        : property == "leaf.max_height" ? &builder.leafMaximumHeight
                        : property == "leaf.min_roughness" ? &builder.leafMinimumRoughness
                        : property == "leaf.max_roughness" ? &builder.leafMaximumRoughness
                        : nullptr;
                    if (destination == nullptr) return diagnostic(lineNumber, valueColumn, "invalid leaf property");
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "organic.accumulation.kind") {
                    const auto parsed = parseOrganicAccumulationKind(value);
                    if (!parsed || !storeValue(builder.organicAccumulationKind, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic accumulation kind must be 'moss', 'lichen', or 'colour_variation'");
                    }
                } else if (property == "organic.accumulation.source") {
                    const auto parsed = parseOrganicAccumulationSource(value);
                    if (!parsed || !storeValue(builder.organicAccumulationSource, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic accumulation source must be 'cavity', 'boundary', 'low_height', or 'authored_mask'");
                    }
                } else if (property == "organic.accumulation.profile") {
                    const auto parsed = parseOrganicAccumulationProfile(value);
                    if (!parsed || !storeValue(builder.organicAccumulationProfile, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic accumulation profile must be 'noise', 'colonies', or 'speckles'");
                    }
                } else if (property == "organic.accumulation.field") {
                    const auto parsed = parseOrganicAccumulationField(value);
                    if (!parsed || !storeValue(builder.organicAccumulationField, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic accumulation field must be 'material', 'fill', 'outline', 'inner_highlight', or 'detail'");
                    }
                } else if (property == "organic.accumulation.scale") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic accumulation scale must be an integer");
                    if (!storeValue(builder.organicAccumulationScale, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "organic.accumulation.colour_low" ||
                           property == "organic.accumulation.colour_high") {
                    Rgba8 parsed{};
                    if (!parseColour(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic accumulation colour must use 0xRRGGBBAA hexadecimal notation");
                    auto& destination = property == "organic.accumulation.colour_low"
                        ? builder.organicAccumulationLowColour : builder.organicAccumulationHighColour;
                    if (!storeValue(destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "organic.accumulation.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic accumulation seed offset must be an unsigned integer");
                    if (!storeValue(builder.organicAccumulationSeedOffset, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "organic.accumulation.target") {
                    const auto parsed = parseProcessingTarget(value);
                    if (!parsed || !storeValue(builder.organicAccumulationTarget, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "organic accumulation target must be 'colour', 'scalar', or 'all'");
                    }
                } else if (property.starts_with("organic.accumulation.")) {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) return diagnostic(lineNumber, valueColumn, "organic accumulation parameter must be a decimal number");
                    ParsedValue<double>* destination = property == "organic.accumulation.coverage"
                        ? &builder.organicAccumulationCoverage
                        : property == "organic.accumulation.softness" ? &builder.organicAccumulationSoftness
                        : property == "organic.accumulation.moisture" ? &builder.organicAccumulationMoisture
                        : property == "organic.accumulation.breakup" ? &builder.organicAccumulationBreakup
                        : property == "organic.accumulation.variation" ? &builder.organicAccumulationVariation
                        : property == "organic.accumulation.outline_width" ? &builder.organicAccumulationOutlineWidth
                        : property == "organic.accumulation.inner_highlight_width" ? &builder.organicAccumulationInnerHighlightWidth
                        : property == "organic.accumulation.inner_highlight_inset" ? &builder.organicAccumulationInnerHighlightInset
                        : nullptr;
                    if (destination == nullptr) return diagnostic(lineNumber, valueColumn, "invalid organic accumulation property");
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "textile.pattern") {
                    const auto parsed = parseTextilePattern(value);
                    if (!parsed || !storeValue(builder.textilePattern, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "textile pattern must be 'plain_weave', 'basket_weave', 'twill_weave', 'loop_pile', or 'cut_pile'");
                    }
                } else if (property == "textile.field") {
                    const auto parsed = parseTextileField(value);
                    if (!parsed || !storeValue(builder.textileField, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "textile field is not supported");
                    }
                } else if (property == "textile.yarn_profile") {
                    const auto parsed = parseYarnProfile(value);
                    if (!parsed || !storeValue(builder.textileYarnProfile, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "textile yarn profile must be 'round', 'flat', or 'twisted'");
                    }
                } else if (property == "textile.tile_orientation") {
                    const auto parsed = parseTextileTileOrientation(value);
                    if (!parsed || !storeValue(builder.textileTileOrientation, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "textile tile orientation is not supported");
                    }
                } else if (property == "textile.columns" || property == "textile.rows" ||
                           property == "textile.tile_columns" || property == "textile.tile_rows" ||
                           property == "textile.weave_span" || property == "textile.twill_step" ||
                           property == "textile.fibre_frequency") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "textile count must be an integer");
                    }
                    ParsedValue<std::uint32_t>* destination =
                        property == "textile.columns" ? &builder.textileColumns
                        : property == "textile.rows" ? &builder.textileRows
                        : property == "textile.tile_columns" ? &builder.textileTileColumns
                        : property == "textile.tile_rows" ? &builder.textileTileRows
                        : property == "textile.weave_span" ? &builder.textileWeaveSpan
                        : property == "textile.twill_step" ? &builder.textileTwillStep
                        : &builder.textileFibreFrequency;
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "textile.colour_low" ||
                           property == "textile.colour_high" ||
                           property == "textile.colour_accent" ||
                           property == "textile.colour_damage") {
                    Rgba8 parsed{};
                    if (!parseColour(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "textile colour must use 0xRRGGBBAA hexadecimal notation");
                    }
                    ParsedValue<Rgba8>* destination =
                        property == "textile.colour_low" ? &builder.textileLowColour
                        : property == "textile.colour_high" ? &builder.textileHighColour
                        : property == "textile.colour_accent" ? &builder.textileAccentColour
                        : &builder.textileDamageColour;
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "textile.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "textile seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.textileSeedOffset, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property.starts_with("textile.")) {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "textile parameter must be a decimal number");
                    }
                    ParsedValue<double>* destination =
                        property == "textile.yarn_width" ? &builder.textileYarnWidth
                        : property == "textile.yarn_roundness" ? &builder.textileYarnRoundness
                        : property == "textile.crossing_height" ? &builder.textileCrossingHeight
                        : property == "textile.jitter" ? &builder.textileJitter
                        : property == "textile.fibre_strength" ? &builder.textileFibreStrength
                        : property == "textile.twist" ? &builder.textileTwist
                        : property == "textile.pile_radius" ? &builder.textilePileRadius
                        : property == "textile.pile_height" ? &builder.textilePileHeight
                        : property == "textile.missing" ? &builder.textileMissingAmount
                        : property == "textile.damage" ? &builder.textileDamageAmount
                        : property == "textile.different_colour" ? &builder.textileDifferentColourAmount
                        : property == "textile.colour_variation" ? &builder.textileColourVariation
                        : property == "textile.softness" ? &builder.textileSoftness
                        : nullptr;
                    if (destination == nullptr) {
                        return diagnostic(lineNumber, valueColumn, "invalid textile property");
                    }
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "attachment.kind") {
                    const auto parsed = parseRegionAttachmentKind(value);
                    if (!parsed || !storeValue(builder.attachmentKind, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "attachment kind must be 'fastener', 'inlay', 'glyph', 'chip', 'crack', or 'damage'");
                    }
                } else if (property == "attachment.field") {
                    const auto parsed = parseRegionAttachmentField(value);
                    if (!parsed || !storeValue(builder.attachmentField, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "attachment field must be 'material', 'mask', or 'distance'");
                    }
                } else if (property == "attachment.start_anchor" ||
                           property == "attachment.end_anchor") {
                    const auto parsed = parseRegionAnchor(value);
                    if (!parsed) {
                        return diagnostic(lineNumber, valueColumn, "attachment anchor must be 'centre', 'edge', 'corner', or 'cavity'");
                    }
                    auto& destination = property == "attachment.start_anchor"
                        ? builder.attachmentStartAnchor : builder.attachmentEndAnchor;
                    if (!storeValue(destination, *parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "attachment.glyph") {
                    const auto parsed = parseRegionGlyph(value);
                    if (!parsed || !storeValue(builder.attachmentGlyph, *parsed, lineNumber, valueColumn)) {
                        return parsed ? duplicate() : diagnostic(lineNumber, valueColumn, "attachment glyph must be 'cross', 'chevron', 'triangle', or 'rune'");
                    }
                } else if (property == "attachment.count") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "attachment count must be an integer");
                    }
                    if (!storeValue(builder.attachmentCount, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "attachment.colour") {
                    Rgba8 parsed{};
                    if (!parseColour(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "attachment colour must use 0xRRGGBBAA hexadecimal notation");
                    }
                    if (!storeValue(builder.attachmentColour, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "attachment.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "attachment seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.attachmentSeedOffset, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property.starts_with("attachment.")) {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "attachment parameter must be a decimal number");
                    }
                    ParsedValue<double>* destination =
                        property == "attachment.size" ? &builder.attachmentSize
                        : property == "attachment.aspect" ? &builder.attachmentAspect
                        : property == "attachment.inset" ? &builder.attachmentInset
                        : property == "attachment.rotation" ? &builder.attachmentRotation
                        : property == "attachment.jitter" ? &builder.attachmentJitter
                        : property == "attachment.selection" ? &builder.attachmentSelection
                        : property == "attachment.line_width" ? &builder.attachmentLineWidth
                        : property == "attachment.length" ? &builder.attachmentLength
                        : property == "attachment.branching" ? &builder.attachmentBranching
                        : property == "attachment.softness" ? &builder.attachmentSoftness
                        : property == "attachment.height" ? &builder.attachmentHeight
                        : property == "attachment.roughness" ? &builder.attachmentRoughness
                        : property == "attachment.metalness" ? &builder.attachmentMetalness
                        : property == "attachment.occlusion" ? &builder.attachmentOcclusion
                        : property == "attachment.emissive" ? &builder.attachmentEmissive
                        : nullptr;
                    if (destination == nullptr) {
                        return diagnostic(lineNumber, valueColumn, "invalid attachment property");
                    }
                    if (!storeValue(*destination, parsed, lineNumber, valueColumn)) return duplicate();
                } else if (property == "transform.scale_x") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "transform scale X must be an integer");
                    }
                    if (!storeValue(builder.scaleX, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "transform.scale_y") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "transform scale Y must be an integer");
                    }
                    if (!storeValue(builder.scaleY, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "transform.offset_x") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "transform offset X must be a decimal number");
                    }
                    if (!storeValue(builder.offsetX, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "transform.offset_y") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "transform offset Y must be a decimal number");
                    }
                    if (!storeValue(builder.offsetY, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "transform.rotation") {
                    const auto parsed = parseRotation(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "transform rotation must be 0, 90, 180, or 270");
                    }
                    if (!storeValue(builder.rotation, *parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "warp.enabled") {
                    bool parsed = false;
                    if (!parseBoolean(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "warp enabled must be true or false");
                    }
                    if (!storeValue(builder.warpEnabled, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "warp.strength") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "warp strength must be a decimal number");
                    }
                    if (!storeValue(builder.warpStrength, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "warp.frequency") {
                    std::uint32_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "warp frequency must be an integer");
                    }
                    if (!storeValue(builder.warpFrequency, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "warp.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "warp seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.warpSeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "mask.enabled") {
                    bool parsed = false;
                    if (!parseBoolean(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "mask enabled must be true or false");
                    }
                    if (!storeValue(builder.maskEnabled, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "mask.inverted") {
                    bool parsed = false;
                    if (!parseBoolean(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "mask inverted must be true or false");
                    }
                    if (!storeValue(builder.maskInverted, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "mask.seed_offset") {
                    std::uint64_t parsed = 0;
                    if (!parseInteger(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "mask seed offset must be an unsigned integer");
                    }
                    if (!storeValue(builder.maskSeedOffset, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "mask.input_low") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "mask input low must be a decimal number");
                    }
                    if (!storeValue(builder.maskLow, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else if (property == "mask.input_high") {
                    double parsed = 0.0;
                    if (!parseDouble(value, parsed)) {
                        return diagnostic(lineNumber, valueColumn, "mask input high must be a decimal number");
                    }
                    if (!storeValue(builder.maskHigh, parsed, lineNumber, valueColumn)) {
                        return duplicate();
                    }
                } else {
                    return diagnostic(lineNumber, 1, "unknown layer key '" + std::string(key) + "'");
                }
            } else {
                return diagnostic(lineNumber, 1, "unknown key '" + std::string(key) + "'");
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        offset = newline + 1;
    }

    for (std::size_t index = 0; index < seen.size(); ++index) {
        const auto field = static_cast<Field>(index);
        const bool optionalInVersionOne = formatVersion == 1 &&
            (field == Field::lowColour || field == Field::highColour ||
             field == Field::normalStrength || field == Field::roughnessLow ||
             field == Field::roughnessHigh || field == Field::layerCount);
        const bool introducedInVersionSix =
            field == Field::physicalWidth || field == Field::physicalHeight;
        const bool optionalMetadata = field == Field::uid || field == Field::name ||
            field == Field::description || field == Field::category || field == Field::tags;
        const bool optionalSurfaceAuthoring = field == Field::reliefDepth;
        const bool introducedInVersionSeventeen =
            field == Field::metalnessLow || field == Field::metalnessHigh ||
            field == Field::dielectricIor;
        const bool introducedInVersionEighteen =
            field == Field::coatingLow || field == Field::coatingHigh ||
            field == Field::occlusionLow || field == Field::occlusionHigh ||
            field == Field::clearCoatLow || field == Field::clearCoatHigh ||
            field == Field::clearCoatRoughnessLow ||
            field == Field::clearCoatRoughnessHigh ||
            field == Field::emissiveIntensity || field == Field::anisotropyStrength ||
            field == Field::anisotropyRotation;
        if (!seen[index] && !optionalInVersionOne &&
            !optionalMetadata && !optionalSurfaceAuthoring &&
            !(formatVersion < 18 && introducedInVersionEighteen) &&
            !(formatVersion < 17 && introducedInVersionSeventeen) &&
            !(formatVersion < 6 && introducedInVersionSix)) {
            return diagnostic(
                lineNumber + 1,
                1,
                "missing required key '" + std::string(fieldKeys[index]) + "'");
        }
    }

    if (formatVersion < 6 &&
        (seen[static_cast<std::size_t>(Field::physicalWidth)] ||
         seen[static_cast<std::size_t>(Field::physicalHeight)])) {
        return diagnostic(
            lineNumber + 1,
            1,
            "physical material dimensions require .pmat version 6");
    }

    if (formatVersion < 15 &&
        (seen[static_cast<std::size_t>(Field::uid)] ||
         seen[static_cast<std::size_t>(Field::name)] ||
         seen[static_cast<std::size_t>(Field::description)] ||
         seen[static_cast<std::size_t>(Field::category)] ||
         seen[static_cast<std::size_t>(Field::tags)])) {
        return diagnostic(
            lineNumber + 1,
            1,
            "material identity and library metadata require .pmat version 15");
    }

    if (formatVersion < 16 && seen[static_cast<std::size_t>(Field::reliefDepth)]) {
        return diagnostic(
            valueLines[static_cast<std::size_t>(Field::reliefDepth)],
            valueColumns[static_cast<std::size_t>(Field::reliefDepth)],
            "physical surface relief requires .pmat version 16");
    }

    if (formatVersion < 17 &&
        (seen[static_cast<std::size_t>(Field::metalnessLow)] ||
         seen[static_cast<std::size_t>(Field::metalnessHigh)] ||
         seen[static_cast<std::size_t>(Field::dielectricIor)])) {
        return diagnostic(
            lineNumber + 1,
            1,
            "metalness and dielectric optics require .pmat version 17");
    }

    if (formatVersion < 18) {
        constexpr std::array versionEighteenFields{
            Field::coatingLow,
            Field::coatingHigh,
            Field::occlusionLow,
            Field::occlusionHigh,
            Field::clearCoatLow,
            Field::clearCoatHigh,
            Field::clearCoatRoughnessLow,
            Field::clearCoatRoughnessHigh,
            Field::emissiveIntensity,
            Field::anisotropyStrength,
            Field::anisotropyRotation,
        };
        for (const auto field : versionEighteenFields) {
            if (seen[static_cast<std::size_t>(field)]) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "coatings and special-surface fields require .pmat version 18");
            }
        }
    }

    if (formatVersion == 1) {
        if (seen[static_cast<std::size_t>(Field::layerCount)] || !layerBuilders.empty()) {
            return diagnostic(lineNumber + 1, 1, "layer stacks require .pmat version 2");
        }
    } else {
        if (layerBuilders.size() > layerCount) {
            return diagnostic(lineNumber + 1, 1, "layer index exceeds layers.count");
        }
        material.layers.reserve(layerCount);
        for (std::size_t index = 0; index < layerCount; ++index) {
            if (index >= layerBuilders.size()) {
                return missingLayerField(lineNumber + 1, index, "enabled");
            }
            const auto& builder = layerBuilders[index];
            if (!builder.enabled.value) {
                return missingLayerField(lineNumber + 1, index, "enabled");
            }
            if (!builder.opacity.value) {
                return missingLayerField(lineNumber + 1, index, "opacity");
            }
            if (!builder.compositeMode.value) {
                return missingLayerField(lineNumber + 1, index, "composite");
            }
            if (!builder.operation.value) {
                return missingLayerField(lineNumber + 1, index, "operation");
            }
            if (formatVersion >= 16 && !builder.outputs.value) {
                return missingLayerField(lineNumber + 1, index, "outputs");
            }
            if (formatVersion < 17 && builder.outputs.value &&
                builder.outputs.value->metalness) {
                return diagnostic(
                    builder.outputs.line,
                    builder.outputs.column,
                    "metalness output routing requires .pmat version 17");
            }
            if (formatVersion < 18 && builder.outputs.value &&
                (builder.outputs.value->coating || builder.outputs.value->occlusion ||
                 builder.outputs.value->clearCoat ||
                 builder.outputs.value->clearCoatRoughness ||
                 builder.outputs.value->emissive)) {
                return diagnostic(
                    builder.outputs.line,
                    builder.outputs.column,
                    "coating, occlusion, clear-coat, and emissive routing require .pmat version 18");
            }
            if (formatVersion < 16 &&
                ((builder.outputs.value &&
                     (!builder.outputs.value->colour || !builder.outputs.value->height ||
                      !builder.outputs.value->roughness)) ||
                 *builder.operation.value == OperationKind::surfaceValue ||
                 *builder.compositeMode.value == CompositeMode::minimum ||
                 *builder.compositeMode.value == CompositeMode::maximum ||
                 *builder.compositeMode.value == CompositeMode::detail)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "surface channel routing and authoring tools require .pmat version 16");
            }
            if (!std::isfinite(*builder.opacity.value) ||
                *builder.opacity.value < LayerLimits::minimumOpacity ||
                *builder.opacity.value > LayerLimits::maximumOpacity) {
                return diagnostic(
                    builder.opacity.line,
                    builder.opacity.column,
                    "layer opacity must be finite and between 0 and 1");
            }

            if (formatVersion < 5 && hasVersionFiveFields(builder)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "brick mortar space requires .pmat version 5");
            }

            if (formatVersion < 6 && hasVersionSixFields(builder)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "physical brick dimensions require .pmat version 6");
            }

            if (formatVersion < 7 &&
                (hasVersionSevenFields(builder) ||
                 *builder.operation.value == OperationKind::surfacePattern ||
                 *builder.operation.value == OperationKind::surfaceFilter)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "advanced surface operations require .pmat version 7");
            }

            if (formatVersion < 8 &&
                (hasVersionEightFields(builder) ||
                 *builder.operation.value == OperationKind::posterise ||
                 *builder.operation.value == OperationKind::colourRamp ||
                 *builder.operation.value == OperationKind::palette ||
                 *builder.operation.value == OperationKind::inkContour)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "stylisation operations require .pmat version 8");
            }

            if (formatVersion < 9 &&
                (hasVersionNineFields(builder) ||
                 *builder.operation.value == OperationKind::regionField)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "region fields require .pmat version 9");
            }

            if (formatVersion < 10 &&
                (hasVersionTenFields(builder) ||
                 *builder.operation.value == OperationKind::courseLayout ||
                 (builder.regionField.value &&
                  *builder.regionField.value == RegionFieldKind::courseRandom))) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "course layouts and course-random fields require .pmat version 10");
            }

            if (formatVersion < 11 &&
                (hasVersionElevenFields(builder) ||
                 *builder.operation.value == OperationKind::regionSurface)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "region surface sculpting requires .pmat version 11");
            }

            if (formatVersion < 12 &&
                (hasVersionTwelveFields(builder) ||
                 *builder.operation.value == OperationKind::shape ||
                 *builder.operation.value == OperationKind::shapeBoolean ||
                 *builder.operation.value == OperationKind::lattice)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "shape primitives and lattices require .pmat version 12");
            }

            if (formatVersion < 13 &&
                (hasVersionThirteenFields(builder) ||
                 *builder.operation.value == OperationKind::scatter)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "deterministic scattering requires .pmat version 13");
            }

            if (formatVersion < 14 &&
                (hasVersionFourteenFields(builder) ||
                 *builder.operation.value == OperationKind::organicCells ||
                 *builder.operation.value == OperationKind::organicCracks ||
                 *builder.operation.value == OperationKind::leafCluster ||
                 *builder.operation.value == OperationKind::organicAccumulation)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "organic structures require .pmat version 14");
            }

            const bool usesRadialShapeKind = builder.shapeKind.value &&
                (*builder.shapeKind.value == ShapePrimitiveKind::annulus ||
                 *builder.shapeKind.value == ShapePrimitiveKind::arc ||
                 *builder.shapeKind.value == ShapePrimitiveKind::sector ||
                 *builder.shapeKind.value == ShapePrimitiveKind::crescent);
            if (formatVersion < 19 &&
                (hasVersionNineteenFields(builder) || usesRadialShapeKind)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "radial profiles and motif repetition require .pmat version 19");
            }

            if (formatVersion < 20 &&
                (hasVersionTwentyFields(builder) ||
                 *builder.operation.value == OperationKind::textile)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "textiles and fibres require .pmat version 20");
            }

            if (formatVersion < 21 &&
                (hasVersionTwentyOneFields(builder) ||
                 *builder.operation.value == OperationKind::regionAttachment)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "region-attached detail and damage require .pmat version 21");
            }

            const bool usesVersionTwentyTwoLeafProfile = builder.leafProfile.value &&
                (*builder.leafProfile.value == LeafProfile::blob ||
                 *builder.leafProfile.value == LeafProfile::rosette ||
                 *builder.leafProfile.value == LeafProfile::lichen);
            const bool usesVersionTwentyTwoLeafPattern = builder.leafPattern.value &&
                *builder.leafPattern.value == LeafClusterPattern::groundScatter;
            const bool usesVersionTwentyTwoLeafField = builder.leafField.value &&
                (*builder.leafField.value == LeafField::outline ||
                 *builder.leafField.value == LeafField::innerHighlight ||
                 *builder.leafField.value == LeafField::clusterRandom ||
                 *builder.leafField.value == LeafField::population);
            if (formatVersion < 22 &&
                (hasVersionTwentyTwoFields(builder) ||
                 usesVersionTwentyTwoLeafProfile ||
                 usesVersionTwentyTwoLeafPattern ||
                 usesVersionTwentyTwoLeafField)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "organic silhouettes and hierarchical clusters require .pmat version 22");
            }

            if (formatVersion < 4 &&
                (hasVersionFourFields(builder) ||
                 isStructuralOperation(*builder.operation.value))) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "structural generators require .pmat version 4");
            }

            if (formatVersion < 3 && hasVersionThreeFields(builder)) {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "coordinate transforms, warp, and masks require .pmat version 3");
            }
            if (formatVersion >= 3) {
                if (!builder.scaleX.value) {
                    return missingLayerField(lineNumber + 1, index, "transform.scale_x");
                }
                if (!builder.scaleY.value) {
                    return missingLayerField(lineNumber + 1, index, "transform.scale_y");
                }
                if (!builder.offsetX.value) {
                    return missingLayerField(lineNumber + 1, index, "transform.offset_x");
                }
                if (!builder.offsetY.value) {
                    return missingLayerField(lineNumber + 1, index, "transform.offset_y");
                }
                if (!builder.rotation.value) {
                    return missingLayerField(lineNumber + 1, index, "transform.rotation");
                }
                if (!builder.warpEnabled.value) {
                    return missingLayerField(lineNumber + 1, index, "warp.enabled");
                }
                if (!builder.warpStrength.value) {
                    return missingLayerField(lineNumber + 1, index, "warp.strength");
                }
                if (!builder.warpFrequency.value) {
                    return missingLayerField(lineNumber + 1, index, "warp.frequency");
                }
                if (!builder.warpSeedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "warp.seed_offset");
                }
                if (!builder.maskEnabled.value) {
                    return missingLayerField(lineNumber + 1, index, "mask.enabled");
                }
                if (!builder.maskInverted.value) {
                    return missingLayerField(lineNumber + 1, index, "mask.inverted");
                }
                if (!builder.maskSeedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "mask.seed_offset");
                }
                if (!builder.maskLow.value) {
                    return missingLayerField(lineNumber + 1, index, "mask.input_low");
                }
                if (!builder.maskHigh.value) {
                    return missingLayerField(lineNumber + 1, index, "mask.input_high");
                }

                if (*builder.scaleX.value < LayerLimits::minimumScale ||
                    *builder.scaleX.value > LayerLimits::maximumScale) {
                    return diagnostic(
                        builder.scaleX.line,
                        builder.scaleX.column,
                        "transform scale X must be between 1 and 16");
                }
                if (*builder.scaleY.value < LayerLimits::minimumScale ||
                    *builder.scaleY.value > LayerLimits::maximumScale) {
                    return diagnostic(
                        builder.scaleY.line,
                        builder.scaleY.column,
                        "transform scale Y must be between 1 and 16");
                }
                if (!std::isfinite(*builder.offsetX.value) ||
                    std::abs(*builder.offsetX.value) > LayerLimits::maximumOffsetMagnitude) {
                    return diagnostic(
                        builder.offsetX.line,
                        builder.offsetX.column,
                        "transform offset X must be finite and between -1024 and 1024");
                }
                if (!std::isfinite(*builder.offsetY.value) ||
                    std::abs(*builder.offsetY.value) > LayerLimits::maximumOffsetMagnitude) {
                    return diagnostic(
                        builder.offsetY.line,
                        builder.offsetY.column,
                        "transform offset Y must be finite and between -1024 and 1024");
                }
                if (!std::isfinite(*builder.warpStrength.value) ||
                    *builder.warpStrength.value < LayerLimits::minimumWarpStrength ||
                    *builder.warpStrength.value > LayerLimits::maximumWarpStrength) {
                    return diagnostic(
                        builder.warpStrength.line,
                        builder.warpStrength.column,
                        "warp strength must be finite and between 0 and 1");
                }
                if (*builder.warpFrequency.value < LayerLimits::minimumWarpFrequency ||
                    *builder.warpFrequency.value > LayerLimits::maximumWarpFrequency) {
                    return diagnostic(
                        builder.warpFrequency.line,
                        builder.warpFrequency.column,
                        "warp frequency must be between 1 and 16");
                }
                if (!std::isfinite(*builder.maskLow.value) ||
                    !std::isfinite(*builder.maskHigh.value) ||
                    *builder.maskLow.value < LayerLimits::minimumLevel ||
                    *builder.maskHigh.value > LayerLimits::maximumLevel ||
                    *builder.maskLow.value >= *builder.maskHigh.value) {
                    return diagnostic(
                        builder.maskHigh.line,
                        builder.maskHigh.column,
                        "mask input range must be finite, within 0 to 1, and increasing");
                }
            }

            MaterialLayer layer{
                *builder.enabled.value,
                *builder.opacity.value,
                *builder.compositeMode.value,
                NoiseOperation{},
                {},
                {},
                builder.outputs.value.value_or(LayerOutputRouting{}),
            };
            if (formatVersion < 17) {
                // Metalness did not exist yet. Preserve the legacy meaning of an
                // all-channel layer now that a fifth deterministic map exists.
                layer.outputs.metalness = true;
            }
            if (formatVersion < 18) {
                // Version 18 adds five channels. Neutral material-level defaults
                // make them inert while all-channel migration preserves the
                // historical shared graph used by old definitions.
                layer.outputs.coating = true;
                layer.outputs.occlusion = true;
                layer.outputs.clearCoat = true;
                layer.outputs.clearCoatRoughness = true;
                layer.outputs.emissive = true;
            }
            if (formatVersion >= 3) {
                layer.transform = CoordinateTransform{
                    *builder.scaleX.value,
                    *builder.scaleY.value,
                    *builder.offsetX.value,
                    *builder.offsetY.value,
                    *builder.rotation.value,
                    *builder.warpEnabled.value,
                    *builder.warpStrength.value,
                    *builder.warpFrequency.value,
                    *builder.warpSeedOffset.value,
                };
                layer.mask = LayerMask{
                    *builder.maskEnabled.value,
                    *builder.maskInverted.value,
                    *builder.maskSeedOffset.value,
                    *builder.maskLow.value,
                    *builder.maskHigh.value,
                };
            }
            const bool hasBrickFields = builder.brickColumns.value || builder.brickRows.value ||
                builder.brickMortar.value || builder.brickMortarSpace.value ||
                builder.brickSizing.value || builder.brickWidthMetres.value ||
                builder.brickHeightMetres.value || builder.brickMortarMetres.value ||
                builder.brickStagger.value || builder.brickSoftness.value;
            const bool hasTileFields = builder.tileColumns.value || builder.tileRows.value ||
                builder.tileGrout.value || builder.tileSoftness.value;
            const bool hasWorleyFields = builder.worleyColumns.value || builder.worleyRows.value ||
                builder.worleyJitter.value || builder.worleyEdgeWidth.value ||
                builder.worleySeedOffset.value;
            const bool hasRandomFields = builder.randomColumns.value || builder.randomRows.value ||
                builder.randomSeedOffset.value;
            const bool hasLineFields = builder.lineDirection.value || builder.lineCount.value ||
                builder.lineWidth.value || builder.lineSoftness.value;
            const bool hasRectangleFields = builder.rectangleColumns.value ||
                builder.rectangleRows.value || builder.rectangleWidth.value ||
                builder.rectangleHeight.value || builder.rectangleSoftness.value;
            const bool hasCircleFields = builder.circleColumns.value || builder.circleRows.value ||
                builder.circleRadius.value || builder.circleSoftness.value;
            const bool hasSurfaceFields = builder.surfaceKind.value ||
                builder.surfaceScale.value || builder.surfaceWidth.value ||
                builder.surfaceDetail.value || builder.surfaceDistortion.value ||
                builder.surfaceVariation.value || builder.surfaceSeedOffset.value;
            const bool hasFilterFields = builder.filterKind.value ||
                builder.filterRadius.value || builder.filterStrength.value ||
                builder.filterSensitivity.value || builder.filterTarget.value;
            const auto anyParsed = [](const auto& fields) {
                return std::any_of(fields.begin(), fields.end(), [](const auto& field) {
                    return field.value.has_value();
                });
            };
            const bool hasPosteriseFields = builder.posteriseBands.value ||
                builder.posteriseTarget.value;
            const bool hasRampFields = builder.rampMode.value ||
                builder.rampStopCount.value || anyParsed(builder.rampPositions) ||
                anyParsed(builder.rampColours);
            const bool hasPaletteFields = builder.paletteColourCount.value ||
                anyParsed(builder.paletteColours);
            const bool hasInkFields = builder.inkColour.value || builder.inkRadius.value ||
                builder.inkThreshold.value || builder.inkSoftness.value ||
                builder.inkStrength.value || builder.inkInverted.value;
            const bool hasRegionFields = hasVersionNineFields(builder);
            const bool hasCourseFields = hasVersionTenFields(builder);
            const bool hasSculptFields = hasVersionElevenFields(builder);
            const bool hasShapeFields = hasVersionTwelveShapeFields(builder);
            const bool hasLatticeFields = hasVersionTwelveLatticeFields(builder);
            const bool hasScatterFields = hasVersionThirteenFields(builder);
            const bool hasOrganicFields =
                hasVersionFourteenFields(builder) || hasVersionTwentyTwoFields(builder);
            const bool hasTextileFields = hasVersionTwentyFields(builder);
            const bool hasAttachmentFields = hasVersionTwentyOneFields(builder);
            const bool shapeBelongsToScatter =
                *builder.operation.value == OperationKind::scatter;
            const int operationGroupCount = static_cast<int>(hasBrickFields) +
                static_cast<int>(hasTileFields) + static_cast<int>(hasWorleyFields) +
                static_cast<int>(hasRandomFields) + static_cast<int>(hasLineFields) +
                static_cast<int>(hasRectangleFields) + static_cast<int>(hasCircleFields) +
                static_cast<int>(hasSurfaceFields) + static_cast<int>(hasFilterFields) +
                static_cast<int>(hasPosteriseFields) + static_cast<int>(hasRampFields) +
                static_cast<int>(hasPaletteFields) + static_cast<int>(hasInkFields) +
                static_cast<int>(hasRegionFields) + static_cast<int>(hasCourseFields) +
                static_cast<int>(hasSculptFields) +
                static_cast<int>(hasShapeFields && !shapeBelongsToScatter) +
                static_cast<int>(hasLatticeFields) +
                static_cast<int>(hasScatterFields ||
                    (shapeBelongsToScatter && hasShapeFields)) +
                static_cast<int>(hasOrganicFields) +
                static_cast<int>(hasTextileFields) +
                static_cast<int>(hasAttachmentFields);

            const bool hasClassicFields = builder.seedOffset.value || builder.solidColour.value ||
                builder.levelsLow.value || builder.levelsHigh.value ||
                builder.levelsGamma.value || builder.threshold.value ||
                builder.surfaceValue.value;
            const auto invalidCount = [](std::uint32_t value) {
                return value < LayerLimits::minimumPatternCount ||
                    value > LayerLimits::maximumPatternCount;
            };
            const auto outside = [](double value, double minimum, double maximum) {
                return !std::isfinite(value) || value < minimum || value > maximum;
            };
            const auto crossOperationError = [&] {
                return diagnostic(
                    lineNumber + 1,
                    1,
                    "layer contains parameters for another operation");
            };

            switch (*builder.operation.value) {
            case OperationKind::noise:
                if (!builder.seedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "noise.seed_offset");
                }
                if (const auto error = unexpectedLayerField(
                        builder.solidColour, index, "solid.colour", "noise")) {
                    return *error;
                }
                if (builder.levelsLow.value || builder.levelsHigh.value ||
                    builder.levelsGamma.value || builder.threshold.value ||
                    builder.surfaceValue.value ||
                    hasStructuralFields(builder)) {
                    return diagnostic(lineNumber + 1, 1, "noise layer contains parameters for another operation");
                }
                layer.operation = NoiseOperation{*builder.seedOffset.value};
                break;
            case OperationKind::solidColour:
                if (!builder.solidColour.value) {
                    return missingLayerField(lineNumber + 1, index, "solid.colour");
                }
                if (builder.seedOffset.value || builder.levelsLow.value || builder.levelsHigh.value ||
                    builder.levelsGamma.value || builder.threshold.value ||
                    builder.surfaceValue.value ||
                    hasStructuralFields(builder)) {
                    return diagnostic(
                        lineNumber + 1,
                        1,
                        "solid-colour layer contains parameters for another operation");
                }
                layer.operation = SolidColourOperation{*builder.solidColour.value};
                break;
            case OperationKind::surfaceValue:
                if (!builder.surfaceValue.value) {
                    return missingLayerField(lineNumber + 1, index, "surface.value");
                }
                if (builder.seedOffset.value || builder.solidColour.value ||
                    builder.levelsLow.value || builder.levelsHigh.value ||
                    builder.levelsGamma.value || builder.threshold.value ||
                    hasStructuralFields(builder)) {
                    return crossOperationError();
                }
                if (outside(
                        *builder.surfaceValue.value,
                        LayerLimits::minimumLevel,
                        LayerLimits::maximumLevel)) {
                    return diagnostic(
                        builder.surfaceValue.line,
                        builder.surfaceValue.column,
                        "surface value must be finite and between 0 and 1");
                }
                layer.operation = SurfaceValueOperation{*builder.surfaceValue.value};
                break;
            case OperationKind::levels:
                if (!builder.levelsLow.value) {
                    return missingLayerField(lineNumber + 1, index, "levels.input_low");
                }
                if (!builder.levelsHigh.value) {
                    return missingLayerField(lineNumber + 1, index, "levels.input_high");
                }
                if (!builder.levelsGamma.value) {
                    return missingLayerField(lineNumber + 1, index, "levels.gamma");
                }
                if (builder.seedOffset.value || builder.solidColour.value ||
                    builder.threshold.value || builder.surfaceValue.value ||
                    hasStructuralFields(builder)) {
                    return diagnostic(lineNumber + 1, 1, "levels layer contains parameters for another operation");
                }
                if (!std::isfinite(*builder.levelsLow.value) ||
                    *builder.levelsLow.value < LayerLimits::minimumLevel ||
                    *builder.levelsLow.value > LayerLimits::maximumLevel) {
                    return diagnostic(
                        builder.levelsLow.line,
                        builder.levelsLow.column,
                        "levels input low must be finite and between 0 and 1");
                }
                if (!std::isfinite(*builder.levelsHigh.value) ||
                    *builder.levelsHigh.value < LayerLimits::minimumLevel ||
                    *builder.levelsHigh.value > LayerLimits::maximumLevel) {
                    return diagnostic(
                        builder.levelsHigh.line,
                        builder.levelsHigh.column,
                        "levels input high must be finite and between 0 and 1");
                }
                if (*builder.levelsLow.value >= *builder.levelsHigh.value) {
                    return diagnostic(
                        builder.levelsHigh.line,
                        builder.levelsHigh.column,
                        "levels input high must be greater than input low");
                }
                if (!std::isfinite(*builder.levelsGamma.value) ||
                    *builder.levelsGamma.value < LayerLimits::minimumGamma ||
                    *builder.levelsGamma.value > LayerLimits::maximumGamma) {
                    return diagnostic(
                        builder.levelsGamma.line,
                        builder.levelsGamma.column,
                        "levels gamma must be finite and between 0.1 and 4");
                }
                layer.operation = LevelsOperation{
                    *builder.levelsLow.value,
                    *builder.levelsHigh.value,
                    *builder.levelsGamma.value,
                };
                break;
            case OperationKind::threshold:
                if (!builder.threshold.value) {
                    return missingLayerField(lineNumber + 1, index, "threshold.value");
                }
                if (builder.seedOffset.value || builder.solidColour.value || builder.levelsLow.value ||
                    builder.levelsHigh.value || builder.levelsGamma.value ||
                    builder.surfaceValue.value ||
                    hasStructuralFields(builder)) {
                    return diagnostic(lineNumber + 1, 1, "threshold layer contains parameters for another operation");
                }
                if (!std::isfinite(*builder.threshold.value) ||
                    *builder.threshold.value < LayerLimits::minimumThreshold ||
                    *builder.threshold.value > LayerLimits::maximumThreshold) {
                    return diagnostic(
                        builder.threshold.line,
                        builder.threshold.column,
                        "threshold must be finite and between 0 and 1");
                }
                layer.operation = ThresholdOperation{*builder.threshold.value};
                break;
            case OperationKind::brickGrid:
                if (formatVersion >= 6 && !builder.brickSizing.value) {
                    return missingLayerField(lineNumber + 1, index, "brick.sizing");
                }
                if (!builder.brickStagger.value) {
                    return missingLayerField(lineNumber + 1, index, "brick.stagger");
                }
                if (!builder.brickSoftness.value) {
                    return missingLayerField(lineNumber + 1, index, "brick.softness");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (outside(*builder.brickStagger.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.brickStagger.line,
                        builder.brickStagger.column,
                        "brick stagger must be finite and between 0 and 1");
                }
                if (outside(*builder.brickSoftness.value, 0.0, 0.25)) {
                    return diagnostic(
                        builder.brickSoftness.line,
                        builder.brickSoftness.column,
                        "brick softness must be finite and between 0 and 0.25");
                }
                if (builder.brickSizing.value.value_or(BrickSizing::relative) ==
                    BrickSizing::physical) {
                    if (!builder.brickWidthMetres.value) {
                        return missingLayerField(lineNumber + 1, index, "brick.width");
                    }
                    if (!builder.brickHeightMetres.value) {
                        return missingLayerField(lineNumber + 1, index, "brick.height");
                    }
                    if (!builder.brickMortarMetres.value) {
                        return missingLayerField(lineNumber + 1, index, "brick.mortar_width");
                    }
                    if (builder.brickColumns.value || builder.brickRows.value ||
                        builder.brickMortar.value || builder.brickMortarSpace.value) {
                        return diagnostic(
                            lineNumber + 1,
                            1,
                            "a physically sized brick cannot also declare relative brick fields");
                    }
                    const auto width = *builder.brickWidthMetres.value;
                    const auto height = *builder.brickHeightMetres.value;
                    const auto mortar = *builder.brickMortarMetres.value;
                    if (!std::isfinite(width) || !std::isfinite(height) ||
                        !std::isfinite(mortar) || width <= 0.0 || height <= 0.0 ||
                        mortar < 0.0 || mortar >= std::min(width, height)) {
                        return diagnostic(
                            builder.brickWidthMetres.line,
                            builder.brickWidthMetres.column,
                            "physical brick dimensions must be positive and mortar must be smaller than width and height");
                    }
                    auto brick = BrickGridOperation{};
                    brick.stagger = *builder.brickStagger.value;
                    brick.softness = *builder.brickSoftness.value;
                    brick.physicalDimensions = BrickGridOperation::PhysicalDimensions{
                        width,
                        height,
                        mortar,
                    };
                    layer.operation = brick;
                } else {
                    if (!builder.brickColumns.value) {
                        return missingLayerField(lineNumber + 1, index, "brick.columns");
                    }
                    if (!builder.brickRows.value) {
                        return missingLayerField(lineNumber + 1, index, "brick.rows");
                    }
                    if (!builder.brickMortar.value) {
                        return missingLayerField(lineNumber + 1, index, "brick.mortar");
                    }
                    if (formatVersion >= 5 && !builder.brickMortarSpace.value) {
                        return missingLayerField(lineNumber + 1, index, "brick.mortar_space");
                    }
                    if (builder.brickWidthMetres.value || builder.brickHeightMetres.value ||
                        builder.brickMortarMetres.value) {
                        return diagnostic(
                            lineNumber + 1,
                            1,
                            "a relatively sized brick cannot also declare physical brick fields");
                    }
                    if (invalidCount(*builder.brickColumns.value) ||
                        invalidCount(*builder.brickRows.value)) {
                        return diagnostic(
                            builder.brickColumns.line,
                            builder.brickColumns.column,
                            "brick columns and rows must be between 1 and 64");
                    }
                    if (outside(*builder.brickMortar.value, 0.0, 0.95)) {
                        return diagnostic(
                            builder.brickMortar.line,
                            builder.brickMortar.column,
                            "brick mortar must be finite and between 0 and 0.95");
                    }
                    layer.operation = BrickGridOperation{
                        *builder.brickColumns.value,
                        *builder.brickRows.value,
                        *builder.brickMortar.value,
                        *builder.brickStagger.value,
                        *builder.brickSoftness.value,
                        builder.brickMortarSpace.value.value_or(BrickMortarSpace::cell),
                        std::nullopt,
                    };
                }
                break;
            case OperationKind::tileGrid:
                if (!builder.tileColumns.value) {
                    return missingLayerField(lineNumber + 1, index, "tile.columns");
                }
                if (!builder.tileRows.value) {
                    return missingLayerField(lineNumber + 1, index, "tile.rows");
                }
                if (!builder.tileGrout.value) {
                    return missingLayerField(lineNumber + 1, index, "tile.grout");
                }
                if (!builder.tileSoftness.value) {
                    return missingLayerField(lineNumber + 1, index, "tile.softness");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (invalidCount(*builder.tileColumns.value) ||
                    invalidCount(*builder.tileRows.value)) {
                    return diagnostic(
                        builder.tileColumns.line,
                        builder.tileColumns.column,
                        "tile columns and rows must be between 1 and 64");
                }
                if (outside(*builder.tileGrout.value, 0.0, 0.95)) {
                    return diagnostic(
                        builder.tileGrout.line,
                        builder.tileGrout.column,
                        "tile grout must be finite and between 0 and 0.95");
                }
                if (outside(*builder.tileSoftness.value, 0.0, 0.25)) {
                    return diagnostic(
                        builder.tileSoftness.line,
                        builder.tileSoftness.column,
                        "tile softness must be finite and between 0 and 0.25");
                }
                layer.operation = TileGridOperation{
                    *builder.tileColumns.value,
                    *builder.tileRows.value,
                    *builder.tileGrout.value,
                    *builder.tileSoftness.value,
                };
                break;
            case OperationKind::courseLayout: {
                if (!builder.courseProfile.value) {
                    return missingLayerField(lineNumber + 1, index, "course.profile");
                }
                if (!builder.courseField.value) {
                    return missingLayerField(lineNumber + 1, index, "course.field");
                }
                if (!builder.courseSizing.value) {
                    return missingLayerField(lineNumber + 1, index, "course.sizing");
                }
                if (!builder.courseBlocks.value) {
                    return missingLayerField(lineNumber + 1, index, "course.blocks");
                }
                if (!builder.courseCount.value) {
                    return missingLayerField(lineNumber + 1, index, "course.courses");
                }
                if (!builder.courseBlockVariation.value) {
                    return missingLayerField(lineNumber + 1, index, "course.block_variation");
                }
                if (!builder.courseHeightVariation.value) {
                    return missingLayerField(lineNumber + 1, index, "course.height_variation");
                }
                if (!builder.courseStagger.value) {
                    return missingLayerField(lineNumber + 1, index, "course.stagger");
                }
                if (!builder.courseCrookedness.value) {
                    return missingLayerField(lineNumber + 1, index, "course.crookedness");
                }
                if (!builder.courseGap.value) {
                    return missingLayerField(lineNumber + 1, index, "course.gap");
                }
                if (!builder.courseSoftness.value) {
                    return missingLayerField(lineNumber + 1, index, "course.softness");
                }
                if (!builder.courseOverlap.value) {
                    return missingLayerField(lineNumber + 1, index, "course.overlap");
                }
                if (!builder.courseSeedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "course.seed_offset");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (invalidCount(*builder.courseBlocks.value) ||
                    invalidCount(*builder.courseCount.value)) {
                    return diagnostic(
                        builder.courseBlocks.line,
                        builder.courseBlocks.column,
                        "course blocks and courses must be between 1 and 64");
                }
                if (outside(*builder.courseBlockVariation.value, 0.0, 1.0) ||
                    outside(*builder.courseHeightVariation.value, 0.0, 1.0) ||
                    outside(*builder.courseStagger.value, 0.0, 1.0) ||
                    outside(*builder.courseCrookedness.value, 0.0, 1.0) ||
                    outside(*builder.courseGap.value, 0.0, 0.95) ||
                    outside(*builder.courseSoftness.value, 0.0, 0.25) ||
                    outside(*builder.courseOverlap.value, 0.0, 0.95)) {
                    return diagnostic(
                        builder.courseBlockVariation.line,
                        builder.courseBlockVariation.column,
                        "course variation, stagger, crookedness, gap, softness, or overlap is outside its supported range");
                }
                CourseLayoutOperation course{
                    *builder.courseProfile.value,
                    *builder.courseField.value,
                    *builder.courseBlocks.value,
                    *builder.courseCount.value,
                    *builder.courseBlockVariation.value,
                    *builder.courseHeightVariation.value,
                    *builder.courseStagger.value,
                    *builder.courseCrookedness.value,
                    *builder.courseGap.value,
                    *builder.courseSoftness.value,
                    *builder.courseOverlap.value,
                    *builder.courseSeedOffset.value,
                    std::nullopt,
                };
                if (*builder.courseSizing.value == BrickSizing::physical) {
                    if (!builder.courseBlockWidthMetres.value) {
                        return missingLayerField(lineNumber + 1, index, "course.block_width");
                    }
                    if (!builder.courseHeightMetres.value) {
                        return missingLayerField(lineNumber + 1, index, "course.course_height");
                    }
                    if (!builder.courseGapMetres.value) {
                        return missingLayerField(lineNumber + 1, index, "course.gap_width");
                    }
                    if (!builder.courseOverlapMetres.value) {
                        return missingLayerField(lineNumber + 1, index, "course.overlap_depth");
                    }
                    const double width = *builder.courseBlockWidthMetres.value;
                    const double height = *builder.courseHeightMetres.value;
                    const double gap = *builder.courseGapMetres.value;
                    const double overlap = *builder.courseOverlapMetres.value;
                    if (!std::isfinite(width) || !std::isfinite(height) ||
                        !std::isfinite(gap) || !std::isfinite(overlap) ||
                        width <= 0.0 || height <= 0.0 || gap < 0.0 || overlap < 0.0 ||
                        gap >= std::min(width, height) || overlap >= height) {
                        return diagnostic(
                            builder.courseBlockWidthMetres.line,
                            builder.courseBlockWidthMetres.column,
                            "physical course dimensions must be positive; gap and overlap must fit within a block and course");
                    }
                    course.physicalDimensions = CourseLayoutOperation::PhysicalDimensions{
                        width,
                        height,
                        gap,
                        overlap,
                    };
                } else if (builder.courseBlockWidthMetres.value ||
                           builder.courseHeightMetres.value ||
                           builder.courseGapMetres.value ||
                           builder.courseOverlapMetres.value) {
                    return diagnostic(
                        lineNumber + 1,
                        1,
                        "a relatively sized course layout cannot declare physical dimensions");
                }
                layer.operation = course;
                break;
            }
            case OperationKind::regionSurface:
                if (!builder.sculptField.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.field");
                }
                if (!builder.sculptProfile.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.profile");
                }
                if (!builder.sculptBevelWidth.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.bevel_width");
                }
                if (!builder.sculptBevelHeight.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.bevel_height");
                }
                if (!builder.sculptFacetCount.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.facet_count");
                }
                if (!builder.sculptFacetStrength.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.facet_strength");
                }
                if (!builder.sculptCentrePeak.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.centre_peak");
                }
                if (!builder.sculptSlope.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.slope");
                }
                if (!builder.sculptChips.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.chips");
                }
                if (!builder.sculptChipScale.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.chip_scale");
                }
                if (!builder.sculptWear.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.wear");
                }
                if (!builder.sculptErosion.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.erosion");
                }
                if (!builder.sculptSeedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.seed_offset");
                }
                if (!builder.sculptFacetedNormals.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.faceted_normals");
                }
                if (!builder.sculptTarget.value) {
                    return missingLayerField(lineNumber + 1, index, "sculpt.target");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (outside(*builder.sculptBevelWidth.value, 0.001, 1.0) ||
                    outside(*builder.sculptBevelHeight.value, 0.0, 1.0) ||
                    outside(*builder.sculptFacetStrength.value, 0.0, 1.0) ||
                    outside(*builder.sculptCentrePeak.value, 0.0, 1.0) ||
                    outside(*builder.sculptSlope.value, 0.0, 1.0) ||
                    outside(*builder.sculptChips.value, 0.0, 1.0) ||
                    outside(*builder.sculptWear.value, 0.0, 1.0) ||
                    outside(*builder.sculptErosion.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.sculptBevelWidth.line,
                        builder.sculptBevelWidth.column,
                        "sculpt decimal controls must be finite and within their supported ranges");
                }
                if (*builder.sculptFacetCount.value < LayerLimits::minimumFacetCount ||
                    *builder.sculptFacetCount.value > LayerLimits::maximumFacetCount) {
                    return diagnostic(
                        builder.sculptFacetCount.line,
                        builder.sculptFacetCount.column,
                        "sculpt facet count must be between 3 and 16");
                }
                if (*builder.sculptChipScale.value < LayerLimits::minimumChipScale ||
                    *builder.sculptChipScale.value > LayerLimits::maximumChipScale) {
                    return diagnostic(
                        builder.sculptChipScale.line,
                        builder.sculptChipScale.column,
                        "sculpt chip scale must be between 1 and 64");
                }
                layer.operation = RegionSurfaceOperation{
                    *builder.sculptField.value,
                    *builder.sculptProfile.value,
                    *builder.sculptBevelWidth.value,
                    *builder.sculptBevelHeight.value,
                    *builder.sculptFacetCount.value,
                    *builder.sculptFacetStrength.value,
                    *builder.sculptCentrePeak.value,
                    *builder.sculptSlope.value,
                    *builder.sculptChips.value,
                    *builder.sculptChipScale.value,
                    *builder.sculptWear.value,
                    *builder.sculptErosion.value,
                    *builder.sculptSeedOffset.value,
                    *builder.sculptFacetedNormals.value,
                    *builder.sculptTarget.value,
                };
                break;
            case OperationKind::shape:
            case OperationKind::shapeBoolean: {
                if (!builder.shapeKind.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.kind");
                }
                if (!builder.shapeField.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.field");
                }
                if (!builder.shapeColumns.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.columns");
                }
                if (!builder.shapeRows.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.rows");
                }
                if (!builder.shapeWidth.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.width");
                }
                if (!builder.shapeHeight.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.height");
                }
                if (!builder.shapeCornerRadius.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.corner_radius");
                }
                if (!builder.shapeInset.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.inset");
                }
                if (!builder.shapeBorderWidth.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.border_width");
                }
                if (!builder.shapeSoftness.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.softness");
                }
                if (!builder.shapeOffsetX.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.offset_x");
                }
                if (!builder.shapeOffsetY.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.offset_y");
                }
                if (!builder.shapeStagger.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.stagger");
                }
                if (!builder.shapeRotation.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.rotation");
                }
                if (!builder.shapeSeedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.seed_offset");
                }
                if (!builder.shapeVertexCount.value) {
                    return missingLayerField(lineNumber + 1, index, "shape.vertices");
                }
                if (formatVersion >= 19) {
                    if (!builder.shapeInnerRadius.value) return missingLayerField(lineNumber + 1, index, "shape.inner_radius");
                    if (!builder.shapeArcStart.value) return missingLayerField(lineNumber + 1, index, "shape.arc_start");
                    if (!builder.shapeArcSweep.value) return missingLayerField(lineNumber + 1, index, "shape.arc_sweep");
                    if (!builder.shapeCrescentOffset.value) return missingLayerField(lineNumber + 1, index, "shape.crescent_offset");
                    if (!builder.shapeRadialCopies.value) return missingLayerField(lineNumber + 1, index, "shape.radial_copies");
                    if (!builder.shapeRadialRadius.value) return missingLayerField(lineNumber + 1, index, "shape.radial_radius");
                    if (!builder.shapeRadialPhase.value) return missingLayerField(lineNumber + 1, index, "shape.radial_phase");
                    if (!builder.shapeRadialOrientation.value) return missingLayerField(lineNumber + 1, index, "shape.radial_orientation");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                const auto vertexCount = *builder.shapeVertexCount.value;
                if (vertexCount < LayerLimits::minimumPolygonVertices ||
                    vertexCount > LayerLimits::maximumPolygonVertices) {
                    return diagnostic(
                        builder.shapeVertexCount.line,
                        builder.shapeVertexCount.column,
                        "shape vertex count must be between 3 and 12");
                }
                std::vector<ShapePoint> vertices;
                vertices.reserve(vertexCount);
                for (std::size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                    if (!builder.shapeVertexX[vertexIndex].value) {
                        return missingLayerField(
                            lineNumber + 1,
                            index,
                            "shape.vertex." + std::to_string(vertexIndex) + ".x");
                    }
                    if (!builder.shapeVertexY[vertexIndex].value) {
                        return missingLayerField(
                            lineNumber + 1,
                            index,
                            "shape.vertex." + std::to_string(vertexIndex) + ".y");
                    }
                    vertices.push_back({
                        *builder.shapeVertexX[vertexIndex].value,
                        *builder.shapeVertexY[vertexIndex].value,
                    });
                }
                for (std::size_t vertexIndex = vertexCount;
                     vertexIndex < LayerLimits::maximumPolygonVertices;
                     ++vertexIndex) {
                    if (builder.shapeVertexX[vertexIndex].value ||
                        builder.shapeVertexY[vertexIndex].value) {
                        return diagnostic(
                            lineNumber + 1,
                            1,
                            "shape declares a vertex beyond shape.vertices");
                    }
                }
                ShapePrimitiveOperation shape{
                    *builder.shapeKind.value,
                    *builder.shapeField.value,
                    *builder.shapeColumns.value,
                    *builder.shapeRows.value,
                    *builder.shapeWidth.value,
                    *builder.shapeHeight.value,
                    *builder.shapeCornerRadius.value,
                    *builder.shapeInset.value,
                    *builder.shapeBorderWidth.value,
                    *builder.shapeSoftness.value,
                    *builder.shapeOffsetX.value,
                    *builder.shapeOffsetY.value,
                    *builder.shapeStagger.value,
                    *builder.shapeRotation.value,
                    *builder.shapeSeedOffset.value,
                    std::move(vertices),
                };
                shape.innerRadius = builder.shapeInnerRadius.value.value_or(shape.innerRadius);
                shape.arcStartDegrees = builder.shapeArcStart.value.value_or(shape.arcStartDegrees);
                shape.arcSweepDegrees = builder.shapeArcSweep.value.value_or(shape.arcSweepDegrees);
                shape.crescentOffset = builder.shapeCrescentOffset.value.value_or(shape.crescentOffset);
                shape.radialCopies = builder.shapeRadialCopies.value.value_or(shape.radialCopies);
                shape.radialRadius = builder.shapeRadialRadius.value.value_or(shape.radialRadius);
                shape.radialPhaseDegrees = builder.shapeRadialPhase.value.value_or(shape.radialPhaseDegrees);
                shape.radialOrientation = builder.shapeRadialOrientation.value.value_or(shape.radialOrientation);
                if (*builder.operation.value == OperationKind::shape) {
                    if (builder.shapeBooleanMode.value ||
                        builder.shapeBooleanTarget.value) {
                        return diagnostic(
                            lineNumber + 1,
                            1,
                            "shape generators cannot declare shape.boolean or shape.target");
                    }
                    layer.operation = std::move(shape);
                } else {
                    if (!builder.shapeBooleanMode.value) {
                        return missingLayerField(lineNumber + 1, index, "shape.boolean");
                    }
                    if (!builder.shapeBooleanTarget.value) {
                        return missingLayerField(lineNumber + 1, index, "shape.target");
                    }
                    layer.operation = ShapeBooleanOperation{
                        *builder.shapeBooleanMode.value,
                        std::move(shape),
                        *builder.shapeBooleanTarget.value,
                    };
                }
                break;
            }
            case OperationKind::scatter: {
                if (!builder.scatterField.value) return missingLayerField(lineNumber + 1, index, "scatter.field");
                if (!builder.scatterColumns.value) return missingLayerField(lineNumber + 1, index, "scatter.columns");
                if (!builder.scatterRows.value) return missingLayerField(lineNumber + 1, index, "scatter.rows");
                if (!builder.scatterDensity.value) return missingLayerField(lineNumber + 1, index, "scatter.density");
                if (!builder.scatterJitter.value) return missingLayerField(lineNumber + 1, index, "scatter.jitter");
                if (!builder.scatterMinimumDistance.value) return missingLayerField(lineNumber + 1, index, "scatter.minimum_distance");
                if (!builder.scatterOverlapMode.value) return missingLayerField(lineNumber + 1, index, "scatter.overlap");
                if (!builder.scatterMaximumOverlap.value) return missingLayerField(lineNumber + 1, index, "scatter.maximum_overlap");
                if (!builder.scatterSeedOffset.value) return missingLayerField(lineNumber + 1, index, "scatter.seed_offset");
                if (!builder.scatterPopulationCount.value) return missingLayerField(lineNumber + 1, index, "scatter.populations");
                if (!builder.shapeKind.value) return missingLayerField(lineNumber + 1, index, "shape.kind");
                if (!builder.shapeField.value) return missingLayerField(lineNumber + 1, index, "shape.field");
                if (!builder.shapeColumns.value) return missingLayerField(lineNumber + 1, index, "shape.columns");
                if (!builder.shapeRows.value) return missingLayerField(lineNumber + 1, index, "shape.rows");
                if (!builder.shapeWidth.value) return missingLayerField(lineNumber + 1, index, "shape.width");
                if (!builder.shapeHeight.value) return missingLayerField(lineNumber + 1, index, "shape.height");
                if (!builder.shapeCornerRadius.value) return missingLayerField(lineNumber + 1, index, "shape.corner_radius");
                if (!builder.shapeInset.value) return missingLayerField(lineNumber + 1, index, "shape.inset");
                if (!builder.shapeBorderWidth.value) return missingLayerField(lineNumber + 1, index, "shape.border_width");
                if (!builder.shapeSoftness.value) return missingLayerField(lineNumber + 1, index, "shape.softness");
                if (!builder.shapeOffsetX.value) return missingLayerField(lineNumber + 1, index, "shape.offset_x");
                if (!builder.shapeOffsetY.value) return missingLayerField(lineNumber + 1, index, "shape.offset_y");
                if (!builder.shapeStagger.value) return missingLayerField(lineNumber + 1, index, "shape.stagger");
                if (!builder.shapeRotation.value) return missingLayerField(lineNumber + 1, index, "shape.rotation");
                if (!builder.shapeSeedOffset.value) return missingLayerField(lineNumber + 1, index, "shape.seed_offset");
                if (!builder.shapeVertexCount.value) return missingLayerField(lineNumber + 1, index, "shape.vertices");
                if (formatVersion >= 19) {
                    if (!builder.shapeInnerRadius.value) return missingLayerField(lineNumber + 1, index, "shape.inner_radius");
                    if (!builder.shapeArcStart.value) return missingLayerField(lineNumber + 1, index, "shape.arc_start");
                    if (!builder.shapeArcSweep.value) return missingLayerField(lineNumber + 1, index, "shape.arc_sweep");
                    if (!builder.shapeCrescentOffset.value) return missingLayerField(lineNumber + 1, index, "shape.crescent_offset");
                    if (!builder.shapeRadialCopies.value) return missingLayerField(lineNumber + 1, index, "shape.radial_copies");
                    if (!builder.shapeRadialRadius.value) return missingLayerField(lineNumber + 1, index, "shape.radial_radius");
                    if (!builder.shapeRadialPhase.value) return missingLayerField(lineNumber + 1, index, "shape.radial_phase");
                    if (!builder.shapeRadialOrientation.value) return missingLayerField(lineNumber + 1, index, "shape.radial_orientation");
                }
                if (builder.shapeBooleanMode.value || builder.shapeBooleanTarget.value ||
                    hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }

                const auto requireMask = [&](
                    const ScatterMaskBuilder& mask,
                    std::string_view name) -> std::optional<ParseDiagnostic> {
                    if (!mask.enabled.value) return missingLayerField(lineNumber + 1, index, std::string(name) + ".enabled");
                    if (!mask.inverted.value) return missingLayerField(lineNumber + 1, index, std::string(name) + ".inverted");
                    if (!mask.frequency.value) return missingLayerField(lineNumber + 1, index, std::string(name) + ".frequency");
                    if (!mask.inputLow.value) return missingLayerField(lineNumber + 1, index, std::string(name) + ".input_low");
                    if (!mask.inputHigh.value) return missingLayerField(lineNumber + 1, index, std::string(name) + ".input_high");
                    if (!mask.seedOffset.value) return missingLayerField(lineNumber + 1, index, std::string(name) + ".seed_offset");
                    return std::nullopt;
                };
                if (const auto error = requireMask(builder.scatterDensityMask, "scatter.density_mask")) return *error;
                if (const auto error = requireMask(builder.scatterExclusionMask, "scatter.exclusion_mask")) return *error;

                const auto vertexCount = *builder.shapeVertexCount.value;
                if (vertexCount < LayerLimits::minimumPolygonVertices ||
                    vertexCount > LayerLimits::maximumPolygonVertices) {
                    return diagnostic(builder.shapeVertexCount.line, builder.shapeVertexCount.column, "shape vertex count must be between 3 and 12");
                }
                std::vector<ShapePoint> vertices;
                vertices.reserve(vertexCount);
                for (std::size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                    if (!builder.shapeVertexX[vertexIndex].value) return missingLayerField(lineNumber + 1, index, "shape.vertex." + std::to_string(vertexIndex) + ".x");
                    if (!builder.shapeVertexY[vertexIndex].value) return missingLayerField(lineNumber + 1, index, "shape.vertex." + std::to_string(vertexIndex) + ".y");
                    vertices.push_back({*builder.shapeVertexX[vertexIndex].value, *builder.shapeVertexY[vertexIndex].value});
                }
                for (std::size_t vertexIndex = vertexCount; vertexIndex < LayerLimits::maximumPolygonVertices; ++vertexIndex) {
                    if (builder.shapeVertexX[vertexIndex].value || builder.shapeVertexY[vertexIndex].value) {
                        return diagnostic(lineNumber + 1, 1, "shape declares a vertex beyond shape.vertices");
                    }
                }
                ShapePrimitiveOperation stamp{
                    *builder.shapeKind.value, *builder.shapeField.value,
                    *builder.shapeColumns.value, *builder.shapeRows.value,
                    *builder.shapeWidth.value, *builder.shapeHeight.value,
                    *builder.shapeCornerRadius.value, *builder.shapeInset.value,
                    *builder.shapeBorderWidth.value, *builder.shapeSoftness.value,
                    *builder.shapeOffsetX.value, *builder.shapeOffsetY.value,
                    *builder.shapeStagger.value, *builder.shapeRotation.value,
                    *builder.shapeSeedOffset.value, std::move(vertices),
                };
                stamp.innerRadius = builder.shapeInnerRadius.value.value_or(stamp.innerRadius);
                stamp.arcStartDegrees = builder.shapeArcStart.value.value_or(stamp.arcStartDegrees);
                stamp.arcSweepDegrees = builder.shapeArcSweep.value.value_or(stamp.arcSweepDegrees);
                stamp.crescentOffset = builder.shapeCrescentOffset.value.value_or(stamp.crescentOffset);
                stamp.radialCopies = builder.shapeRadialCopies.value.value_or(stamp.radialCopies);
                stamp.radialRadius = builder.shapeRadialRadius.value.value_or(stamp.radialRadius);
                stamp.radialPhaseDegrees = builder.shapeRadialPhase.value.value_or(stamp.radialPhaseDegrees);
                stamp.radialOrientation = builder.shapeRadialOrientation.value.value_or(stamp.radialOrientation);

                const auto populationCount = *builder.scatterPopulationCount.value;
                if (populationCount < 1 || populationCount > LayerLimits::maximumScatterPopulations) {
                    return diagnostic(builder.scatterPopulationCount.line, builder.scatterPopulationCount.column, "scatter population count must be between 1 and 4");
                }
                std::vector<ScatterPopulation> populations;
                populations.reserve(populationCount);
                const auto populationHasFields = [](const ScatterPopulationBuilder& population) {
                    return population.weight.value || population.minimumScale.value || population.maximumScale.value ||
                        population.minimumAspect.value || population.maximumAspect.value ||
                        population.minimumRotation.value || population.maximumRotation.value ||
                        population.lowColour.value || population.highColour.value ||
                        population.minimumHeight.value || population.maximumHeight.value ||
                        population.minimumRoughness.value || population.maximumRoughness.value;
                };
                for (std::size_t populationIndex = 0; populationIndex < populationCount; ++populationIndex) {
                    const auto& population = builder.scatterPopulations[populationIndex];
                    const auto populationPrefix = "scatter.population." + std::to_string(populationIndex) + ".";
                    if (!population.weight.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "weight");
                    if (!population.minimumScale.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "min_scale");
                    if (!population.maximumScale.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "max_scale");
                    if (!population.minimumAspect.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "min_aspect");
                    if (!population.maximumAspect.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "max_aspect");
                    if (!population.minimumRotation.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "min_rotation");
                    if (!population.maximumRotation.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "max_rotation");
                    if (!population.lowColour.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "colour_low");
                    if (!population.highColour.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "colour_high");
                    if (!population.minimumHeight.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "min_height");
                    if (!population.maximumHeight.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "max_height");
                    if (!population.minimumRoughness.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "min_roughness");
                    if (!population.maximumRoughness.value) return missingLayerField(lineNumber + 1, index, populationPrefix + "max_roughness");
                    populations.push_back({
                        *population.weight.value,
                        *population.minimumScale.value, *population.maximumScale.value,
                        *population.minimumAspect.value, *population.maximumAspect.value,
                        *population.minimumRotation.value, *population.maximumRotation.value,
                        *population.lowColour.value, *population.highColour.value,
                        *population.minimumHeight.value, *population.maximumHeight.value,
                        *population.minimumRoughness.value, *population.maximumRoughness.value,
                    });
                }
                for (std::size_t populationIndex = populationCount; populationIndex < LayerLimits::maximumScatterPopulations; ++populationIndex) {
                    if (populationHasFields(builder.scatterPopulations[populationIndex])) {
                        return diagnostic(lineNumber + 1, 1, "scatter declares a population beyond scatter.populations");
                    }
                }
                const auto makeMask = [](const ScatterMaskBuilder& mask) {
                    return ScatterMask{
                        *mask.enabled.value, *mask.inverted.value, *mask.frequency.value,
                        *mask.inputLow.value, *mask.inputHigh.value, *mask.seedOffset.value,
                    };
                };
                layer.operation = ScatterOperation{
                    *builder.scatterField.value,
                    *builder.scatterColumns.value, *builder.scatterRows.value,
                    *builder.scatterDensity.value, *builder.scatterJitter.value,
                    *builder.scatterMinimumDistance.value,
                    *builder.scatterOverlapMode.value,
                    *builder.scatterMaximumOverlap.value,
                    *builder.scatterSeedOffset.value,
                    std::move(stamp), std::move(populations),
                    makeMask(builder.scatterDensityMask),
                    makeMask(builder.scatterExclusionMask),
                };
                break;
            }
            case OperationKind::organicCells:
                if (!builder.organicCellField.value) return missingLayerField(lineNumber + 1, index, "organic.cell.field");
                if (!builder.organicCellDirection.value) return missingLayerField(lineNumber + 1, index, "organic.cell.direction");
                if (!builder.organicCellColumns.value) return missingLayerField(lineNumber + 1, index, "organic.cell.columns");
                if (!builder.organicCellRows.value) return missingLayerField(lineNumber + 1, index, "organic.cell.rows");
                if (!builder.organicCellAnisotropy.value) return missingLayerField(lineNumber + 1, index, "organic.cell.anisotropy");
                if (!builder.organicCellJitter.value) return missingLayerField(lineNumber + 1, index, "organic.cell.jitter");
                if (!builder.organicCellIrregularity.value) return missingLayerField(lineNumber + 1, index, "organic.cell.irregularity");
                if (!builder.organicCellGap.value) return missingLayerField(lineNumber + 1, index, "organic.cell.gap");
                if (!builder.organicCellSoftness.value) return missingLayerField(lineNumber + 1, index, "organic.cell.softness");
                if (!builder.organicCellSeedOffset.value) return missingLayerField(lineNumber + 1, index, "organic.cell.seed_offset");
                if (hasClassicFields || operationGroupCount != 1) return crossOperationError();
                layer.operation = OrganicCellOperation{
                    *builder.organicCellField.value,
                    *builder.organicCellDirection.value,
                    *builder.organicCellColumns.value,
                    *builder.organicCellRows.value,
                    *builder.organicCellAnisotropy.value,
                    *builder.organicCellJitter.value,
                    *builder.organicCellIrregularity.value,
                    *builder.organicCellGap.value,
                    *builder.organicCellSoftness.value,
                    *builder.organicCellSeedOffset.value,
                };
                break;
            case OperationKind::organicCracks:
                if (!builder.organicCrackField.value) return missingLayerField(lineNumber + 1, index, "organic.crack.field");
                if (!builder.organicCrackDirection.value) return missingLayerField(lineNumber + 1, index, "organic.crack.direction");
                if (!builder.organicCrackRoots.value) return missingLayerField(lineNumber + 1, index, "organic.crack.roots");
                if (!builder.organicCrackSegments.value) return missingLayerField(lineNumber + 1, index, "organic.crack.segments");
                if (!builder.organicCrackBranchLevels.value) return missingLayerField(lineNumber + 1, index, "organic.crack.branch_levels");
                if (!builder.organicCrackBranchProbability.value) return missingLayerField(lineNumber + 1, index, "organic.crack.branch_probability");
                if (!builder.organicCrackBend.value) return missingLayerField(lineNumber + 1, index, "organic.crack.bend");
                if (!builder.organicCrackWidth.value) return missingLayerField(lineNumber + 1, index, "organic.crack.width");
                if (!builder.organicCrackTaper.value) return missingLayerField(lineNumber + 1, index, "organic.crack.taper");
                if (!builder.organicCrackSoftness.value) return missingLayerField(lineNumber + 1, index, "organic.crack.softness");
                if (!builder.organicCrackSeedOffset.value) return missingLayerField(lineNumber + 1, index, "organic.crack.seed_offset");
                if (hasClassicFields || operationGroupCount != 1) return crossOperationError();
                layer.operation = OrganicCrackOperation{
                    *builder.organicCrackField.value,
                    *builder.organicCrackDirection.value,
                    *builder.organicCrackRoots.value,
                    *builder.organicCrackSegments.value,
                    *builder.organicCrackBranchLevels.value,
                    *builder.organicCrackBranchProbability.value,
                    *builder.organicCrackBend.value,
                    *builder.organicCrackWidth.value,
                    *builder.organicCrackTaper.value,
                    *builder.organicCrackSoftness.value,
                    *builder.organicCrackSeedOffset.value,
                };
                break;
            case OperationKind::leafCluster:
                if (!builder.leafField.value) return missingLayerField(lineNumber + 1, index, "leaf.field");
                if (!builder.leafProfile.value) return missingLayerField(lineNumber + 1, index, "leaf.profile");
                if (!builder.leafPattern.value) return missingLayerField(lineNumber + 1, index, "leaf.pattern");
                if (!builder.leafColumns.value) return missingLayerField(lineNumber + 1, index, "leaf.columns");
                if (!builder.leafRows.value) return missingLayerField(lineNumber + 1, index, "leaf.rows");
                if (!builder.leavesPerCluster.value) return missingLayerField(lineNumber + 1, index, "leaf.per_cluster");
                if (!builder.leafDensity.value) return missingLayerField(lineNumber + 1, index, "leaf.density");
                if (!builder.leafClusterSpread.value) return missingLayerField(lineNumber + 1, index, "leaf.cluster_spread");
                if (!builder.leafLength.value) return missingLayerField(lineNumber + 1, index, "leaf.length");
                if (!builder.leafWidth.value) return missingLayerField(lineNumber + 1, index, "leaf.width");
                if (!builder.leafScaleVariation.value) return missingLayerField(lineNumber + 1, index, "leaf.scale_variation");
                if (!builder.leafRotationVariation.value) return missingLayerField(lineNumber + 1, index, "leaf.rotation_variation");
                if (!builder.leafDirection.value) return missingLayerField(lineNumber + 1, index, "leaf.direction");
                if (!builder.leafTaper.value) return missingLayerField(lineNumber + 1, index, "leaf.taper");
                if (!builder.leafBaseNotch.value) return missingLayerField(lineNumber + 1, index, "leaf.base_notch");
                if (!builder.leafCurvature.value) return missingLayerField(lineNumber + 1, index, "leaf.curvature");
                if (!builder.leafSerration.value) return missingLayerField(lineNumber + 1, index, "leaf.serration");
                if (!builder.leafSerrationCount.value) return missingLayerField(lineNumber + 1, index, "leaf.serration_count");
                if (!builder.leafLobing.value) return missingLayerField(lineNumber + 1, index, "leaf.lobing");
                if (!builder.leafLobeCount.value) return missingLayerField(lineNumber + 1, index, "leaf.lobe_count");
                if (!builder.leafMidribWidth.value) return missingLayerField(lineNumber + 1, index, "leaf.midrib_width");
                if (!builder.leafVeinPairs.value) return missingLayerField(lineNumber + 1, index, "leaf.vein_pairs");
                if (!builder.leafVeinWidth.value) return missingLayerField(lineNumber + 1, index, "leaf.vein_width");
                if (!builder.leafEdgeWidth.value) return missingLayerField(lineNumber + 1, index, "leaf.edge_width");
                if (!builder.leafSoftness.value) return missingLayerField(lineNumber + 1, index, "leaf.softness");
                if (!builder.leafLowColour.value) return missingLayerField(lineNumber + 1, index, "leaf.colour_low");
                if (!builder.leafHighColour.value) return missingLayerField(lineNumber + 1, index, "leaf.colour_high");
                if (!builder.leafMinimumHeight.value) return missingLayerField(lineNumber + 1, index, "leaf.min_height");
                if (!builder.leafMaximumHeight.value) return missingLayerField(lineNumber + 1, index, "leaf.max_height");
                if (!builder.leafMinimumRoughness.value) return missingLayerField(lineNumber + 1, index, "leaf.min_roughness");
                if (!builder.leafMaximumRoughness.value) return missingLayerField(lineNumber + 1, index, "leaf.max_roughness");
                if (!builder.leafSeedOffset.value) return missingLayerField(lineNumber + 1, index, "leaf.seed_offset");
                if (formatVersion >= 22) {
                    if (!builder.leafInnerHighlightWidth.value) return missingLayerField(lineNumber + 1, index, "leaf.inner_highlight_width");
                    if (!builder.leafInnerHighlightInset.value) return missingLayerField(lineNumber + 1, index, "leaf.inner_highlight_inset");
                    if (!builder.leafClusterColourVariation.value) return missingLayerField(lineNumber + 1, index, "leaf.cluster_colour_variation");
                    if (!builder.leafInstanceColourVariation.value) return missingLayerField(lineNumber + 1, index, "leaf.instance_colour_variation");
                    if (!builder.leafSecondaryProfile.value) return missingLayerField(lineNumber + 1, index, "leaf.secondary_profile");
                    if (!builder.leafSecondaryWeight.value) return missingLayerField(lineNumber + 1, index, "leaf.secondary_weight");
                    if (!builder.leafSecondaryScale.value) return missingLayerField(lineNumber + 1, index, "leaf.secondary_scale");
                    if (!builder.leafSecondaryLowColour.value) return missingLayerField(lineNumber + 1, index, "leaf.secondary_colour_low");
                    if (!builder.leafSecondaryHighColour.value) return missingLayerField(lineNumber + 1, index, "leaf.secondary_colour_high");
                    if (!builder.leafTertiaryProfile.value) return missingLayerField(lineNumber + 1, index, "leaf.tertiary_profile");
                    if (!builder.leafTertiaryWeight.value) return missingLayerField(lineNumber + 1, index, "leaf.tertiary_weight");
                    if (!builder.leafTertiaryScale.value) return missingLayerField(lineNumber + 1, index, "leaf.tertiary_scale");
                    if (!builder.leafTertiaryLowColour.value) return missingLayerField(lineNumber + 1, index, "leaf.tertiary_colour_low");
                    if (!builder.leafTertiaryHighColour.value) return missingLayerField(lineNumber + 1, index, "leaf.tertiary_colour_high");
                }
                if (hasClassicFields || operationGroupCount != 1) return crossOperationError();
                layer.operation = LeafClusterOperation{
                    *builder.leafField.value,
                    *builder.leafProfile.value,
                    *builder.leafPattern.value,
                    *builder.leafColumns.value,
                    *builder.leafRows.value,
                    *builder.leavesPerCluster.value,
                    *builder.leafDensity.value,
                    *builder.leafClusterSpread.value,
                    *builder.leafLength.value,
                    *builder.leafWidth.value,
                    *builder.leafScaleVariation.value,
                    *builder.leafRotationVariation.value,
                    *builder.leafDirection.value,
                    *builder.leafTaper.value,
                    *builder.leafBaseNotch.value,
                    *builder.leafCurvature.value,
                    *builder.leafSerration.value,
                    *builder.leafSerrationCount.value,
                    *builder.leafLobing.value,
                    *builder.leafLobeCount.value,
                    *builder.leafMidribWidth.value,
                    *builder.leafVeinPairs.value,
                    *builder.leafVeinWidth.value,
                    *builder.leafEdgeWidth.value,
                    *builder.leafSoftness.value,
                    *builder.leafLowColour.value,
                    *builder.leafHighColour.value,
                    *builder.leafMinimumHeight.value,
                    *builder.leafMaximumHeight.value,
                    *builder.leafMinimumRoughness.value,
                    *builder.leafMaximumRoughness.value,
                    *builder.leafSeedOffset.value,
                    builder.leafInnerHighlightWidth.value.value_or(LeafClusterOperation{}.innerHighlightWidth),
                    builder.leafInnerHighlightInset.value.value_or(LeafClusterOperation{}.innerHighlightInset),
                    builder.leafClusterColourVariation.value.value_or(LeafClusterOperation{}.clusterColourVariation),
                    builder.leafInstanceColourVariation.value.value_or(LeafClusterOperation{}.instanceColourVariation),
                    builder.leafSecondaryProfile.value.value_or(LeafClusterOperation{}.secondaryProfile),
                    builder.leafSecondaryWeight.value.value_or(LeafClusterOperation{}.secondaryWeight),
                    builder.leafSecondaryScale.value.value_or(LeafClusterOperation{}.secondaryScale),
                    builder.leafSecondaryLowColour.value.value_or(LeafClusterOperation{}.secondaryLowColour),
                    builder.leafSecondaryHighColour.value.value_or(LeafClusterOperation{}.secondaryHighColour),
                    builder.leafTertiaryProfile.value.value_or(LeafClusterOperation{}.tertiaryProfile),
                    builder.leafTertiaryWeight.value.value_or(LeafClusterOperation{}.tertiaryWeight),
                    builder.leafTertiaryScale.value.value_or(LeafClusterOperation{}.tertiaryScale),
                    builder.leafTertiaryLowColour.value.value_or(LeafClusterOperation{}.tertiaryLowColour),
                    builder.leafTertiaryHighColour.value.value_or(LeafClusterOperation{}.tertiaryHighColour),
                };
                break;
            case OperationKind::organicAccumulation:
                if (!builder.organicAccumulationKind.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.kind");
                if (!builder.organicAccumulationSource.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.source");
                if (!builder.organicAccumulationScale.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.scale");
                if (!builder.organicAccumulationCoverage.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.coverage");
                if (!builder.organicAccumulationSoftness.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.softness");
                if (!builder.organicAccumulationMoisture.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.moisture");
                if (!builder.organicAccumulationBreakup.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.breakup");
                if (!builder.organicAccumulationVariation.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.variation");
                if (!builder.organicAccumulationLowColour.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.colour_low");
                if (!builder.organicAccumulationHighColour.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.colour_high");
                if (!builder.organicAccumulationSeedOffset.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.seed_offset");
                if (!builder.organicAccumulationTarget.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.target");
                if (formatVersion >= 22) {
                    if (!builder.organicAccumulationProfile.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.profile");
                    if (!builder.organicAccumulationField.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.field");
                    if (!builder.organicAccumulationOutlineWidth.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.outline_width");
                    if (!builder.organicAccumulationInnerHighlightWidth.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.inner_highlight_width");
                    if (!builder.organicAccumulationInnerHighlightInset.value) return missingLayerField(lineNumber + 1, index, "organic.accumulation.inner_highlight_inset");
                }
                if (hasClassicFields || operationGroupCount != 1) return crossOperationError();
                layer.operation = OrganicAccumulationOperation{
                    *builder.organicAccumulationKind.value,
                    *builder.organicAccumulationSource.value,
                    *builder.organicAccumulationScale.value,
                    *builder.organicAccumulationCoverage.value,
                    *builder.organicAccumulationSoftness.value,
                    *builder.organicAccumulationMoisture.value,
                    *builder.organicAccumulationBreakup.value,
                    *builder.organicAccumulationVariation.value,
                    *builder.organicAccumulationLowColour.value,
                    *builder.organicAccumulationHighColour.value,
                    *builder.organicAccumulationSeedOffset.value,
                    *builder.organicAccumulationTarget.value,
                    builder.organicAccumulationProfile.value.value_or(OrganicAccumulationOperation{}.profile),
                    builder.organicAccumulationField.value.value_or(OrganicAccumulationOperation{}.field),
                    builder.organicAccumulationOutlineWidth.value.value_or(OrganicAccumulationOperation{}.outlineWidth),
                    builder.organicAccumulationInnerHighlightWidth.value.value_or(OrganicAccumulationOperation{}.innerHighlightWidth),
                    builder.organicAccumulationInnerHighlightInset.value.value_or(OrganicAccumulationOperation{}.innerHighlightInset),
                };
                break;
            case OperationKind::textile:
                if (!builder.textilePattern.value) return missingLayerField(lineNumber + 1, index, "textile.pattern");
                if (!builder.textileField.value) return missingLayerField(lineNumber + 1, index, "textile.field");
                if (!builder.textileYarnProfile.value) return missingLayerField(lineNumber + 1, index, "textile.yarn_profile");
                if (!builder.textileTileOrientation.value) return missingLayerField(lineNumber + 1, index, "textile.tile_orientation");
                if (!builder.textileColumns.value) return missingLayerField(lineNumber + 1, index, "textile.columns");
                if (!builder.textileRows.value) return missingLayerField(lineNumber + 1, index, "textile.rows");
                if (!builder.textileTileColumns.value) return missingLayerField(lineNumber + 1, index, "textile.tile_columns");
                if (!builder.textileTileRows.value) return missingLayerField(lineNumber + 1, index, "textile.tile_rows");
                if (!builder.textileWeaveSpan.value) return missingLayerField(lineNumber + 1, index, "textile.weave_span");
                if (!builder.textileTwillStep.value) return missingLayerField(lineNumber + 1, index, "textile.twill_step");
                if (!builder.textileYarnWidth.value) return missingLayerField(lineNumber + 1, index, "textile.yarn_width");
                if (!builder.textileYarnRoundness.value) return missingLayerField(lineNumber + 1, index, "textile.yarn_roundness");
                if (!builder.textileCrossingHeight.value) return missingLayerField(lineNumber + 1, index, "textile.crossing_height");
                if (!builder.textileJitter.value) return missingLayerField(lineNumber + 1, index, "textile.jitter");
                if (!builder.textileFibreFrequency.value) return missingLayerField(lineNumber + 1, index, "textile.fibre_frequency");
                if (!builder.textileFibreStrength.value) return missingLayerField(lineNumber + 1, index, "textile.fibre_strength");
                if (!builder.textileTwist.value) return missingLayerField(lineNumber + 1, index, "textile.twist");
                if (!builder.textilePileRadius.value) return missingLayerField(lineNumber + 1, index, "textile.pile_radius");
                if (!builder.textilePileHeight.value) return missingLayerField(lineNumber + 1, index, "textile.pile_height");
                if (!builder.textileMissingAmount.value) return missingLayerField(lineNumber + 1, index, "textile.missing");
                if (!builder.textileDamageAmount.value) return missingLayerField(lineNumber + 1, index, "textile.damage");
                if (!builder.textileDifferentColourAmount.value) return missingLayerField(lineNumber + 1, index, "textile.different_colour");
                if (!builder.textileColourVariation.value) return missingLayerField(lineNumber + 1, index, "textile.colour_variation");
                if (!builder.textileSoftness.value) return missingLayerField(lineNumber + 1, index, "textile.softness");
                if (!builder.textileLowColour.value) return missingLayerField(lineNumber + 1, index, "textile.colour_low");
                if (!builder.textileHighColour.value) return missingLayerField(lineNumber + 1, index, "textile.colour_high");
                if (!builder.textileAccentColour.value) return missingLayerField(lineNumber + 1, index, "textile.colour_accent");
                if (!builder.textileDamageColour.value) return missingLayerField(lineNumber + 1, index, "textile.colour_damage");
                if (!builder.textileSeedOffset.value) return missingLayerField(lineNumber + 1, index, "textile.seed_offset");
                if (hasClassicFields || operationGroupCount != 1) return crossOperationError();
                layer.operation = TextileOperation{
                    *builder.textilePattern.value,
                    *builder.textileField.value,
                    *builder.textileYarnProfile.value,
                    *builder.textileTileOrientation.value,
                    *builder.textileColumns.value,
                    *builder.textileRows.value,
                    *builder.textileTileColumns.value,
                    *builder.textileTileRows.value,
                    *builder.textileWeaveSpan.value,
                    *builder.textileTwillStep.value,
                    *builder.textileYarnWidth.value,
                    *builder.textileYarnRoundness.value,
                    *builder.textileCrossingHeight.value,
                    *builder.textileJitter.value,
                    *builder.textileFibreFrequency.value,
                    *builder.textileFibreStrength.value,
                    *builder.textileTwist.value,
                    *builder.textilePileRadius.value,
                    *builder.textilePileHeight.value,
                    *builder.textileMissingAmount.value,
                    *builder.textileDamageAmount.value,
                    *builder.textileDifferentColourAmount.value,
                    *builder.textileColourVariation.value,
                    *builder.textileSoftness.value,
                    *builder.textileLowColour.value,
                    *builder.textileHighColour.value,
                    *builder.textileAccentColour.value,
                    *builder.textileDamageColour.value,
                    *builder.textileSeedOffset.value,
                };
                break;
            case OperationKind::regionAttachment:
                if (!builder.attachmentKind.value) return missingLayerField(lineNumber + 1, index, "attachment.kind");
                if (!builder.attachmentField.value) return missingLayerField(lineNumber + 1, index, "attachment.field");
                if (!builder.attachmentStartAnchor.value) return missingLayerField(lineNumber + 1, index, "attachment.start_anchor");
                if (!builder.attachmentEndAnchor.value) return missingLayerField(lineNumber + 1, index, "attachment.end_anchor");
                if (!builder.attachmentGlyph.value) return missingLayerField(lineNumber + 1, index, "attachment.glyph");
                if (!builder.attachmentCount.value) return missingLayerField(lineNumber + 1, index, "attachment.count");
                if (!builder.attachmentSize.value) return missingLayerField(lineNumber + 1, index, "attachment.size");
                if (!builder.attachmentAspect.value) return missingLayerField(lineNumber + 1, index, "attachment.aspect");
                if (!builder.attachmentInset.value) return missingLayerField(lineNumber + 1, index, "attachment.inset");
                if (!builder.attachmentRotation.value) return missingLayerField(lineNumber + 1, index, "attachment.rotation");
                if (!builder.attachmentJitter.value) return missingLayerField(lineNumber + 1, index, "attachment.jitter");
                if (!builder.attachmentSelection.value) return missingLayerField(lineNumber + 1, index, "attachment.selection");
                if (!builder.attachmentLineWidth.value) return missingLayerField(lineNumber + 1, index, "attachment.line_width");
                if (!builder.attachmentLength.value) return missingLayerField(lineNumber + 1, index, "attachment.length");
                if (!builder.attachmentBranching.value) return missingLayerField(lineNumber + 1, index, "attachment.branching");
                if (!builder.attachmentSoftness.value) return missingLayerField(lineNumber + 1, index, "attachment.softness");
                if (!builder.attachmentColour.value) return missingLayerField(lineNumber + 1, index, "attachment.colour");
                if (!builder.attachmentHeight.value) return missingLayerField(lineNumber + 1, index, "attachment.height");
                if (!builder.attachmentRoughness.value) return missingLayerField(lineNumber + 1, index, "attachment.roughness");
                if (!builder.attachmentMetalness.value) return missingLayerField(lineNumber + 1, index, "attachment.metalness");
                if (!builder.attachmentOcclusion.value) return missingLayerField(lineNumber + 1, index, "attachment.occlusion");
                if (!builder.attachmentEmissive.value) return missingLayerField(lineNumber + 1, index, "attachment.emissive");
                if (!builder.attachmentSeedOffset.value) return missingLayerField(lineNumber + 1, index, "attachment.seed_offset");
                if (hasClassicFields || operationGroupCount != 1) return crossOperationError();
                layer.operation = RegionAttachmentOperation{
                    *builder.attachmentKind.value,
                    *builder.attachmentField.value,
                    *builder.attachmentStartAnchor.value,
                    *builder.attachmentEndAnchor.value,
                    *builder.attachmentGlyph.value,
                    *builder.attachmentCount.value,
                    *builder.attachmentSize.value,
                    *builder.attachmentAspect.value,
                    *builder.attachmentInset.value,
                    *builder.attachmentRotation.value,
                    *builder.attachmentJitter.value,
                    *builder.attachmentSelection.value,
                    *builder.attachmentLineWidth.value,
                    *builder.attachmentLength.value,
                    *builder.attachmentBranching.value,
                    *builder.attachmentSoftness.value,
                    *builder.attachmentColour.value,
                    *builder.attachmentHeight.value,
                    *builder.attachmentRoughness.value,
                    *builder.attachmentMetalness.value,
                    *builder.attachmentOcclusion.value,
                    *builder.attachmentEmissive.value,
                    *builder.attachmentSeedOffset.value,
                };
                break;
            case OperationKind::lattice:
                if (!builder.latticeKind.value) {
                    return missingLayerField(lineNumber + 1, index, "lattice.kind");
                }
                if (!builder.latticeWindingX.value) {
                    return missingLayerField(lineNumber + 1, index, "lattice.winding_x");
                }
                if (!builder.latticeWindingY.value) {
                    return missingLayerField(lineNumber + 1, index, "lattice.winding_y");
                }
                if (!builder.latticeWidth.value) {
                    return missingLayerField(lineNumber + 1, index, "lattice.width");
                }
                if (!builder.latticeSoftness.value) {
                    return missingLayerField(lineNumber + 1, index, "lattice.softness");
                }
                if (!builder.latticePhase.value) {
                    return missingLayerField(lineNumber + 1, index, "lattice.phase");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                layer.operation = LatticeOperation{
                    *builder.latticeKind.value,
                    *builder.latticeWindingX.value,
                    *builder.latticeWindingY.value,
                    *builder.latticeWidth.value,
                    *builder.latticeSoftness.value,
                    *builder.latticePhase.value,
                };
                break;
            case OperationKind::worleyCells:
                if (!builder.worleyColumns.value) {
                    return missingLayerField(lineNumber + 1, index, "worley.columns");
                }
                if (!builder.worleyRows.value) {
                    return missingLayerField(lineNumber + 1, index, "worley.rows");
                }
                if (!builder.worleyJitter.value) {
                    return missingLayerField(lineNumber + 1, index, "worley.jitter");
                }
                if (!builder.worleyEdgeWidth.value) {
                    return missingLayerField(lineNumber + 1, index, "worley.edge_width");
                }
                if (!builder.worleySeedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "worley.seed_offset");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (invalidCount(*builder.worleyColumns.value) ||
                    invalidCount(*builder.worleyRows.value)) {
                    return diagnostic(
                        builder.worleyColumns.line,
                        builder.worleyColumns.column,
                        "Worley columns and rows must be between 1 and 64");
                }
                if (outside(*builder.worleyJitter.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.worleyJitter.line,
                        builder.worleyJitter.column,
                        "Worley jitter must be finite and between 0 and 1");
                }
                if (outside(*builder.worleyEdgeWidth.value, 0.01, 2.0)) {
                    return diagnostic(
                        builder.worleyEdgeWidth.line,
                        builder.worleyEdgeWidth.column,
                        "Worley edge width must be finite and between 0.01 and 2");
                }
                layer.operation = WorleyCellsOperation{
                    *builder.worleyColumns.value,
                    *builder.worleyRows.value,
                    *builder.worleyJitter.value,
                    *builder.worleyEdgeWidth.value,
                    *builder.worleySeedOffset.value,
                };
                break;
            case OperationKind::randomCells:
                if (!builder.randomColumns.value) {
                    return missingLayerField(lineNumber + 1, index, "random_cells.columns");
                }
                if (!builder.randomRows.value) {
                    return missingLayerField(lineNumber + 1, index, "random_cells.rows");
                }
                if (!builder.randomSeedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "random_cells.seed_offset");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (invalidCount(*builder.randomColumns.value) ||
                    invalidCount(*builder.randomRows.value)) {
                    return diagnostic(
                        builder.randomColumns.line,
                        builder.randomColumns.column,
                        "random-cell columns and rows must be between 1 and 64");
                }
                layer.operation = RandomCellsOperation{
                    *builder.randomColumns.value,
                    *builder.randomRows.value,
                    *builder.randomSeedOffset.value,
                };
                break;
            case OperationKind::lines:
                if (!builder.lineDirection.value) {
                    return missingLayerField(lineNumber + 1, index, "lines.direction");
                }
                if (!builder.lineCount.value) {
                    return missingLayerField(lineNumber + 1, index, "lines.count");
                }
                if (!builder.lineWidth.value) {
                    return missingLayerField(lineNumber + 1, index, "lines.width");
                }
                if (!builder.lineSoftness.value) {
                    return missingLayerField(lineNumber + 1, index, "lines.softness");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (invalidCount(*builder.lineCount.value)) {
                    return diagnostic(
                        builder.lineCount.line,
                        builder.lineCount.column,
                        "line count must be between 1 and 64");
                }
                if (outside(*builder.lineWidth.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.lineWidth.line,
                        builder.lineWidth.column,
                        "line width must be finite and between 0 and 1");
                }
                if (outside(*builder.lineSoftness.value, 0.0, 0.25)) {
                    return diagnostic(
                        builder.lineSoftness.line,
                        builder.lineSoftness.column,
                        "line softness must be finite and between 0 and 0.25");
                }
                layer.operation = LinesOperation{
                    *builder.lineDirection.value,
                    *builder.lineCount.value,
                    *builder.lineWidth.value,
                    *builder.lineSoftness.value,
                };
                break;
            case OperationKind::rectangles:
                if (!builder.rectangleColumns.value) {
                    return missingLayerField(lineNumber + 1, index, "rectangles.columns");
                }
                if (!builder.rectangleRows.value) {
                    return missingLayerField(lineNumber + 1, index, "rectangles.rows");
                }
                if (!builder.rectangleWidth.value) {
                    return missingLayerField(lineNumber + 1, index, "rectangles.width");
                }
                if (!builder.rectangleHeight.value) {
                    return missingLayerField(lineNumber + 1, index, "rectangles.height");
                }
                if (!builder.rectangleSoftness.value) {
                    return missingLayerField(lineNumber + 1, index, "rectangles.softness");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (invalidCount(*builder.rectangleColumns.value) ||
                    invalidCount(*builder.rectangleRows.value)) {
                    return diagnostic(
                        builder.rectangleColumns.line,
                        builder.rectangleColumns.column,
                        "rectangle columns and rows must be between 1 and 64");
                }
                if (outside(*builder.rectangleWidth.value, 0.0, 1.0) ||
                    outside(*builder.rectangleHeight.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.rectangleWidth.line,
                        builder.rectangleWidth.column,
                        "rectangle width and height must be finite and between 0 and 1");
                }
                if (outside(*builder.rectangleSoftness.value, 0.0, 0.25)) {
                    return diagnostic(
                        builder.rectangleSoftness.line,
                        builder.rectangleSoftness.column,
                        "rectangle softness must be finite and between 0 and 0.25");
                }
                layer.operation = RectanglesOperation{
                    *builder.rectangleColumns.value,
                    *builder.rectangleRows.value,
                    *builder.rectangleWidth.value,
                    *builder.rectangleHeight.value,
                    *builder.rectangleSoftness.value,
                };
                break;
            case OperationKind::circles:
                if (!builder.circleColumns.value) {
                    return missingLayerField(lineNumber + 1, index, "circles.columns");
                }
                if (!builder.circleRows.value) {
                    return missingLayerField(lineNumber + 1, index, "circles.rows");
                }
                if (!builder.circleRadius.value) {
                    return missingLayerField(lineNumber + 1, index, "circles.radius");
                }
                if (!builder.circleSoftness.value) {
                    return missingLayerField(lineNumber + 1, index, "circles.softness");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (invalidCount(*builder.circleColumns.value) ||
                    invalidCount(*builder.circleRows.value)) {
                    return diagnostic(
                        builder.circleColumns.line,
                        builder.circleColumns.column,
                        "circle columns and rows must be between 1 and 64");
                }
                if (outside(*builder.circleRadius.value, 0.0, 0.5)) {
                    return diagnostic(
                        builder.circleRadius.line,
                        builder.circleRadius.column,
                        "circle radius must be finite and between 0 and 0.5");
                }
                if (outside(*builder.circleSoftness.value, 0.0, 0.25)) {
                    return diagnostic(
                        builder.circleSoftness.line,
                        builder.circleSoftness.column,
                        "circle softness must be finite and between 0 and 0.25");
                }
                layer.operation = CirclesOperation{
                    *builder.circleColumns.value,
                    *builder.circleRows.value,
                    *builder.circleRadius.value,
                    *builder.circleSoftness.value,
                };
                break;
            case OperationKind::surfacePattern:
                if (!builder.surfaceKind.value) {
                    return missingLayerField(lineNumber + 1, index, "surface.kind");
                }
                if (!builder.surfaceScale.value) {
                    return missingLayerField(lineNumber + 1, index, "surface.scale");
                }
                if (!builder.surfaceWidth.value) {
                    return missingLayerField(lineNumber + 1, index, "surface.width");
                }
                if (!builder.surfaceDetail.value) {
                    return missingLayerField(lineNumber + 1, index, "surface.detail");
                }
                if (!builder.surfaceDistortion.value) {
                    return missingLayerField(lineNumber + 1, index, "surface.distortion");
                }
                if (!builder.surfaceVariation.value) {
                    return missingLayerField(lineNumber + 1, index, "surface.variation");
                }
                if (!builder.surfaceSeedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "surface.seed_offset");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (invalidCount(*builder.surfaceScale.value)) {
                    return diagnostic(
                        builder.surfaceScale.line,
                        builder.surfaceScale.column,
                        "surface scale must be between 1 and 64");
                }
                if (outside(
                        *builder.surfaceWidth.value,
                        LayerLimits::minimumSurfaceWidth,
                        LayerLimits::maximumSurfaceWidth)) {
                    return diagnostic(
                        builder.surfaceWidth.line,
                        builder.surfaceWidth.column,
                        "surface width must be finite and between 0.001 and 1");
                }
                if (outside(*builder.surfaceDetail.value, 0.0, 1.0) ||
                    outside(*builder.surfaceDistortion.value, 0.0, 1.0) ||
                    outside(*builder.surfaceVariation.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.surfaceDetail.line,
                        builder.surfaceDetail.column,
                        "surface detail, distortion, and variation must be finite and between 0 and 1");
                }
                layer.operation = SurfacePatternOperation{
                    *builder.surfaceKind.value,
                    *builder.surfaceScale.value,
                    *builder.surfaceWidth.value,
                    *builder.surfaceDetail.value,
                    *builder.surfaceDistortion.value,
                    *builder.surfaceVariation.value,
                    *builder.surfaceSeedOffset.value,
                };
                break;
            case OperationKind::surfaceFilter:
                if (!builder.filterKind.value) {
                    return missingLayerField(lineNumber + 1, index, "filter.kind");
                }
                if (!builder.filterRadius.value) {
                    return missingLayerField(lineNumber + 1, index, "filter.radius");
                }
                if (!builder.filterStrength.value) {
                    return missingLayerField(lineNumber + 1, index, "filter.strength");
                }
                if (formatVersion >= 8 && !builder.filterSensitivity.value) {
                    return missingLayerField(lineNumber + 1, index, "filter.sensitivity");
                }
                if (formatVersion >= 8 && !builder.filterTarget.value) {
                    return missingLayerField(lineNumber + 1, index, "filter.target");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (outside(
                        *builder.filterRadius.value,
                        LayerLimits::minimumFilterRadius,
                        LayerLimits::maximumFilterRadius)) {
                    return diagnostic(
                        builder.filterRadius.line,
                        builder.filterRadius.column,
                        "filter radius must be finite and between 0 and 0.25");
                }
                if (outside(*builder.filterStrength.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.filterStrength.line,
                        builder.filterStrength.column,
                        "filter strength must be finite and between 0 and 1");
                }
                if (builder.filterSensitivity.value &&
                    outside(*builder.filterSensitivity.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.filterSensitivity.line,
                        builder.filterSensitivity.column,
                        "filter sensitivity must be finite and between 0 and 1");
                }
                layer.operation = SurfaceFilterOperation{
                    *builder.filterKind.value,
                    *builder.filterRadius.value,
                    *builder.filterStrength.value,
                    builder.filterSensitivity.value.value_or(0.2),
                    builder.filterTarget.value.value_or(
                        ProcessingTarget::colourAndScalar),
                };
                break;
            case OperationKind::posterise:
                if (!builder.posteriseBands.value) {
                    return missingLayerField(lineNumber + 1, index, "posterise.bands");
                }
                if (!builder.posteriseTarget.value) {
                    return missingLayerField(lineNumber + 1, index, "posterise.target");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (*builder.posteriseBands.value < LayerLimits::minimumPosteriseBands ||
                    *builder.posteriseBands.value > LayerLimits::maximumPosteriseBands) {
                    return diagnostic(
                        builder.posteriseBands.line,
                        builder.posteriseBands.column,
                        "posterise bands must be between 2 and 16");
                }
                layer.operation = PosteriseOperation{
                    *builder.posteriseBands.value,
                    *builder.posteriseTarget.value,
                };
                break;
            case OperationKind::colourRamp: {
                if (!builder.rampMode.value) {
                    return missingLayerField(lineNumber + 1, index, "ramp.mode");
                }
                if (!builder.rampStopCount.value) {
                    return missingLayerField(lineNumber + 1, index, "ramp.stops");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                const auto count = *builder.rampStopCount.value;
                if (count < LayerLimits::minimumColourStops ||
                    count > LayerLimits::maximumColourStops) {
                    return diagnostic(
                        builder.rampStopCount.line,
                        builder.rampStopCount.column,
                        "colour ramp must contain between 2 and 8 stops");
                }
                ColourRampOperation ramp;
                ramp.mode = *builder.rampMode.value;
                ramp.stops.clear();
                ramp.stops.reserve(count);
                double previous = -1.0;
                for (std::size_t stopIndex = 0; stopIndex < count; ++stopIndex) {
                    if (!builder.rampPositions[stopIndex].value) {
                        return missingLayerField(
                            lineNumber + 1,
                            index,
                            "ramp.stop." + std::to_string(stopIndex) + ".position");
                    }
                    if (!builder.rampColours[stopIndex].value) {
                        return missingLayerField(
                            lineNumber + 1,
                            index,
                            "ramp.stop." + std::to_string(stopIndex) + ".colour");
                    }
                    const double position = *builder.rampPositions[stopIndex].value;
                    if (!std::isfinite(position) || position < 0.0 || position > 1.0 ||
                        position <= previous) {
                        return diagnostic(
                            builder.rampPositions[stopIndex].line,
                            builder.rampPositions[stopIndex].column,
                            "colour ramp stop positions must be within 0 to 1 and strictly increasing");
                    }
                    previous = position;
                    ramp.stops.push_back({position, *builder.rampColours[stopIndex].value});
                }
                for (std::size_t stopIndex = count;
                     stopIndex < LayerLimits::maximumColourStops;
                     ++stopIndex) {
                    if (builder.rampPositions[stopIndex].value ||
                        builder.rampColours[stopIndex].value) {
                        return diagnostic(
                            lineNumber + 1,
                            1,
                            "colour ramp declares a stop beyond ramp.stops");
                    }
                }
                layer.operation = std::move(ramp);
                break;
            }
            case OperationKind::palette: {
                if (!builder.paletteColourCount.value) {
                    return missingLayerField(lineNumber + 1, index, "palette.colours");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                const auto count = *builder.paletteColourCount.value;
                if (count < LayerLimits::minimumColourStops ||
                    count > LayerLimits::maximumColourStops) {
                    return diagnostic(
                        builder.paletteColourCount.line,
                        builder.paletteColourCount.column,
                        "palette must contain between 2 and 8 colours");
                }
                PaletteOperation palette;
                palette.colours.clear();
                palette.colours.reserve(count);
                for (std::size_t colourIndex = 0; colourIndex < count; ++colourIndex) {
                    if (!builder.paletteColours[colourIndex].value) {
                        return missingLayerField(
                            lineNumber + 1,
                            index,
                            "palette.entry." + std::to_string(colourIndex) + ".colour");
                    }
                    palette.colours.push_back(*builder.paletteColours[colourIndex].value);
                }
                for (std::size_t colourIndex = count;
                     colourIndex < LayerLimits::maximumColourStops;
                     ++colourIndex) {
                    if (builder.paletteColours[colourIndex].value) {
                        return diagnostic(
                            lineNumber + 1,
                            1,
                            "palette declares an entry beyond palette.colours");
                    }
                }
                layer.operation = std::move(palette);
                break;
            }
            case OperationKind::inkContour:
                if (!builder.inkColour.value) {
                    return missingLayerField(lineNumber + 1, index, "ink.colour");
                }
                if (!builder.inkRadius.value) {
                    return missingLayerField(lineNumber + 1, index, "ink.radius");
                }
                if (!builder.inkThreshold.value) {
                    return missingLayerField(lineNumber + 1, index, "ink.threshold");
                }
                if (!builder.inkSoftness.value) {
                    return missingLayerField(lineNumber + 1, index, "ink.softness");
                }
                if (!builder.inkStrength.value) {
                    return missingLayerField(lineNumber + 1, index, "ink.strength");
                }
                if (!builder.inkInverted.value) {
                    return missingLayerField(lineNumber + 1, index, "ink.inverted");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (outside(*builder.inkRadius.value, 0.0, 0.25) ||
                    outside(*builder.inkThreshold.value, 0.0, 1.0) ||
                    outside(*builder.inkSoftness.value, 0.0, 0.5) ||
                    outside(*builder.inkStrength.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.inkRadius.line,
                        builder.inkRadius.column,
                        "ink radius, threshold, softness, or strength is outside its supported range");
                }
                layer.operation = InkContourOperation{
                    *builder.inkColour.value,
                    *builder.inkRadius.value,
                    *builder.inkThreshold.value,
                    *builder.inkSoftness.value,
                    *builder.inkStrength.value,
                    *builder.inkInverted.value,
                };
                break;
            case OperationKind::regionField:
                if (!builder.regionField.value) {
                    return missingLayerField(lineNumber + 1, index, "region.field");
                }
                if (!builder.regionSeedOffset.value) {
                    return missingLayerField(lineNumber + 1, index, "region.seed_offset");
                }
                if (!builder.regionChannel.value) {
                    return missingLayerField(lineNumber + 1, index, "region.channel");
                }
                if (!builder.regionOutputLow.value) {
                    return missingLayerField(lineNumber + 1, index, "region.output_low");
                }
                if (!builder.regionOutputHigh.value) {
                    return missingLayerField(lineNumber + 1, index, "region.output_high");
                }
                if (!builder.regionInverted.value) {
                    return missingLayerField(lineNumber + 1, index, "region.inverted");
                }
                if (!builder.regionTarget.value) {
                    return missingLayerField(lineNumber + 1, index, "region.target");
                }
                if (hasClassicFields || operationGroupCount != 1) {
                    return crossOperationError();
                }
                if (*builder.regionChannel.value > LayerLimits::maximumRegionChannel) {
                    return diagnostic(
                        builder.regionChannel.line,
                        builder.regionChannel.column,
                        "region channel must be between 0 and 255");
                }
                if (outside(*builder.regionOutputLow.value, 0.0, 1.0) ||
                    outside(*builder.regionOutputHigh.value, 0.0, 1.0)) {
                    return diagnostic(
                        builder.regionOutputLow.line,
                        builder.regionOutputLow.column,
                        "region output range must be finite and between 0 and 1");
                }
                layer.operation = RegionFieldOperation{
                    *builder.regionField.value,
                    *builder.regionSeedOffset.value,
                    *builder.regionChannel.value,
                    *builder.regionOutputLow.value,
                    *builder.regionOutputHigh.value,
                    *builder.regionInverted.value,
                    *builder.regionTarget.value,
                };
                break;
            }
            material.layers.push_back(std::move(layer));
        }
    }

    if (const auto error = validateMaterial(material)) {
        Field relevantField = Field::frequency;
        if (error->find("UID") != std::string::npos &&
            seen[static_cast<std::size_t>(Field::uid)]) {
            relevantField = Field::uid;
        } else if (error->find("name") != std::string::npos &&
                   seen[static_cast<std::size_t>(Field::name)]) {
            relevantField = Field::name;
        } else if (error->find("description") != std::string::npos &&
                   seen[static_cast<std::size_t>(Field::description)]) {
            relevantField = Field::description;
        } else if (error->find("category") != std::string::npos &&
                   seen[static_cast<std::size_t>(Field::category)]) {
            relevantField = Field::category;
        } else if (error->find("tag") != std::string::npos &&
                   seen[static_cast<std::size_t>(Field::tags)]) {
            relevantField = Field::tags;
        } else if (error->find("relief") != std::string::npos &&
                   seen[static_cast<std::size_t>(Field::reliefDepth)]) {
            relevantField = Field::reliefDepth;
        } else if (error->find("physical") != std::string::npos && formatVersion >= 6) {
            relevantField = Field::physicalWidth;
        } else if (material.frequency >= MaterialLimits::minimumFrequency &&
            material.frequency <= MaterialLimits::maximumFrequency) {
            if (material.octaves < MaterialLimits::minimumOctaves ||
                material.octaves > MaterialLimits::maximumOctaves) {
                relevantField = Field::octaves;
            } else if (material.lacunarity < MaterialLimits::minimumLacunarity ||
                       material.lacunarity > MaterialLimits::maximumLacunarity) {
                relevantField = Field::lacunarity;
            } else if (!std::isfinite(material.gain) ||
                       material.gain < MaterialLimits::minimumGain ||
                       material.gain > MaterialLimits::maximumGain) {
                relevantField = Field::gain;
            } else if (!std::isfinite(material.normalStrength) ||
                       material.normalStrength < MaterialLimits::minimumNormalStrength ||
                       material.normalStrength > MaterialLimits::maximumNormalStrength) {
                relevantField = Field::normalStrength;
            } else if (!std::isfinite(material.roughnessLow) ||
                       material.roughnessLow < MaterialLimits::minimumRoughness ||
                       material.roughnessLow > MaterialLimits::maximumRoughness) {
                relevantField = Field::roughnessLow;
            } else if (!std::isfinite(material.roughnessHigh) ||
                       material.roughnessHigh < MaterialLimits::minimumRoughness ||
                       material.roughnessHigh > MaterialLimits::maximumRoughness) {
                relevantField = Field::roughnessHigh;
            } else if (!std::isfinite(material.metalnessLow) ||
                       material.metalnessLow < MaterialLimits::minimumMetalness ||
                       material.metalnessLow > MaterialLimits::maximumMetalness) {
                relevantField = Field::metalnessLow;
            } else if (!std::isfinite(material.metalnessHigh) ||
                       material.metalnessHigh < MaterialLimits::minimumMetalness ||
                       material.metalnessHigh > MaterialLimits::maximumMetalness) {
                relevantField = Field::metalnessHigh;
            } else if (!std::isfinite(material.dielectricIor) ||
                       material.dielectricIor < MaterialLimits::minimumDielectricIor ||
                       material.dielectricIor > MaterialLimits::maximumDielectricIor) {
                relevantField = Field::dielectricIor;
            } else if (!std::isfinite(material.coatingLow) ||
                       material.coatingLow < 0.0 || material.coatingLow > 1.0) {
                relevantField = Field::coatingLow;
            } else if (!std::isfinite(material.coatingHigh) ||
                       material.coatingHigh < 0.0 || material.coatingHigh > 1.0) {
                relevantField = Field::coatingHigh;
            } else if (!std::isfinite(material.occlusionLow) ||
                       material.occlusionLow < 0.0 || material.occlusionLow > 1.0) {
                relevantField = Field::occlusionLow;
            } else if (!std::isfinite(material.occlusionHigh) ||
                       material.occlusionHigh < 0.0 || material.occlusionHigh > 1.0) {
                relevantField = Field::occlusionHigh;
            } else if (!std::isfinite(material.clearCoatLow) ||
                       material.clearCoatLow < 0.0 || material.clearCoatLow > 1.0) {
                relevantField = Field::clearCoatLow;
            } else if (!std::isfinite(material.clearCoatHigh) ||
                       material.clearCoatHigh < 0.0 || material.clearCoatHigh > 1.0) {
                relevantField = Field::clearCoatHigh;
            } else if (!std::isfinite(material.clearCoatRoughnessLow) ||
                       material.clearCoatRoughnessLow < 0.0 ||
                       material.clearCoatRoughnessLow > 1.0) {
                relevantField = Field::clearCoatRoughnessLow;
            } else if (!std::isfinite(material.clearCoatRoughnessHigh) ||
                       material.clearCoatRoughnessHigh < 0.0 ||
                       material.clearCoatRoughnessHigh > 1.0) {
                relevantField = Field::clearCoatRoughnessHigh;
            } else if (!std::isfinite(material.emissiveIntensity) ||
                       material.emissiveIntensity < 0.0 ||
                       material.emissiveIntensity > 1.0) {
                relevantField = Field::emissiveIntensity;
            } else if (!std::isfinite(material.anisotropyStrength) ||
                       material.anisotropyStrength < 0.0 ||
                       material.anisotropyStrength > 1.0) {
                relevantField = Field::anisotropyStrength;
            } else if (!std::isfinite(material.anisotropyRotationDegrees) ||
                       material.anisotropyRotationDegrees < 0.0 ||
                       material.anisotropyRotationDegrees > 360.0) {
                relevantField = Field::anisotropyRotation;
            }
        }
        const auto index = static_cast<std::size_t>(relevantField);
        return diagnostic(valueLines[index], valueColumns[index], *error);
    }
    return material;
}

SerialisationResult serialisePmat(const Material& material)
{
    if (const auto error = validateMaterial(material)) {
        return SerialisationError{*error};
    }
    const auto gain = formatDouble(material.gain);
    const auto normalStrength = formatDouble(material.normalStrength);
    const auto roughnessLow = formatDouble(material.roughnessLow);
    const auto roughnessHigh = formatDouble(material.roughnessHigh);
    const auto metalnessLow = formatDouble(material.metalnessLow);
    const auto metalnessHigh = formatDouble(material.metalnessHigh);
    const auto dielectricIor = formatDouble(material.dielectricIor);
    const auto coatingLow = formatDouble(material.coatingLow);
    const auto coatingHigh = formatDouble(material.coatingHigh);
    const auto occlusionLow = formatDouble(material.occlusionLow);
    const auto occlusionHigh = formatDouble(material.occlusionHigh);
    const auto clearCoatLow = formatDouble(material.clearCoatLow);
    const auto clearCoatHigh = formatDouble(material.clearCoatHigh);
    const auto clearCoatRoughnessLow = formatDouble(material.clearCoatRoughnessLow);
    const auto clearCoatRoughnessHigh = formatDouble(material.clearCoatRoughnessHigh);
    const auto emissiveIntensity = formatDouble(material.emissiveIntensity);
    const auto anisotropyStrength = formatDouble(material.anisotropyStrength);
    const auto anisotropyRotation = formatDouble(material.anisotropyRotationDegrees);
    const auto physicalWidth = formatMetres(material.physicalSize.widthMetres);
    const auto physicalHeight = formatMetres(material.physicalSize.heightMetres);
    const auto reliefDepth = material.reliefDepthMetres
        ? formatMetres(*material.reliefDepthMetres)
        : std::string{};
    if (gain.empty() || normalStrength.empty() || roughnessLow.empty() || roughnessHigh.empty() ||
        metalnessLow.empty() || metalnessHigh.empty() || dielectricIor.empty() ||
        coatingLow.empty() || coatingHigh.empty() || occlusionLow.empty() ||
        occlusionHigh.empty() || clearCoatLow.empty() || clearCoatHigh.empty() ||
        clearCoatRoughnessLow.empty() || clearCoatRoughnessHigh.empty() ||
        emissiveIntensity.empty() || anisotropyStrength.empty() ||
        anisotropyRotation.empty() ||
        physicalWidth.empty() || physicalHeight.empty()) {
        return SerialisationError{"could not format a decimal material parameter"};
    }

    std::string output;
    output.reserve(320 + material.layers.size() * 800);
    output += "# Paperweight procedural material\n";
    output += "pmat.version = " + std::to_string(currentPmatVersion) + "\n";
    output += "material.type = fbm\n";
    output += "material.seed = " + std::to_string(material.seed) + "\n";
    if (material.metadata) {
        const auto& metadata = *material.metadata;
        if (!metadata.uid.empty()) {
            output += "material.uid = " + metadata.uid + "\n";
        }
        if (!metadata.name.empty()) {
            output += "material.name = " + metadata.name + "\n";
        }
        if (!metadata.description.empty()) {
            output += "material.description = " + metadata.description + "\n";
        }
        if (!metadata.category.empty()) {
            output += "material.category = " + metadata.category + "\n";
        }
        if (!metadata.tags.empty()) {
            output += "material.tags = ";
            for (std::size_t index = 0; index < metadata.tags.size(); ++index) {
                if (index != 0) {
                    output += ", ";
                }
                output += metadata.tags[index];
            }
            output += "\n";
        }
    }
    output += "material.width = " + physicalWidth + "\n";
    output += "material.height = " + physicalHeight + "\n";
    if (material.reliefDepthMetres) {
        if (reliefDepth.empty()) {
            return SerialisationError{"could not format physical relief depth"};
        }
        output += "surface.relief_depth = " + reliefDepth + "\n";
    }
    output += "colour.low = " + formatColour(material.lowColour) + "\n";
    output += "colour.high = " + formatColour(material.highColour) + "\n";
    output += "noise.frequency = " + std::to_string(material.frequency) + "\n";
    output += "noise.octaves = " + std::to_string(material.octaves) + "\n";
    output += "noise.lacunarity = " + std::to_string(material.lacunarity) + "\n";
    output += "noise.gain = " + gain + "\n";
    output += "normal.strength = " + normalStrength + "\n";
    output += "roughness.low = " + roughnessLow + "\n";
    output += "roughness.high = " + roughnessHigh + "\n";
    output += "metalness.low = " + metalnessLow + "\n";
    output += "metalness.high = " + metalnessHigh + "\n";
    output += "surface.ior = " + dielectricIor + "\n";
    output += "coating.low = " + coatingLow + "\n";
    output += "coating.high = " + coatingHigh + "\n";
    output += "occlusion.low = " + occlusionLow + "\n";
    output += "occlusion.high = " + occlusionHigh + "\n";
    output += "clearcoat.low = " + clearCoatLow + "\n";
    output += "clearcoat.high = " + clearCoatHigh + "\n";
    output += "clearcoat.roughness_low = " + clearCoatRoughnessLow + "\n";
    output += "clearcoat.roughness_high = " + clearCoatRoughnessHigh + "\n";
    output += "emissive.intensity = " + emissiveIntensity + "\n";
    output += "anisotropy.strength = " + anisotropyStrength + "\n";
    output += "anisotropy.rotation = " + anisotropyRotation + "\n";
    output += "layers.count = " + std::to_string(material.layers.size()) + "\n";

    for (std::size_t index = 0; index < material.layers.size(); ++index) {
        const auto& layer = material.layers[index];
        const auto prefix = "layer." + std::to_string(index) + ".";
        const auto opacity = formatDouble(layer.opacity);
        if (opacity.empty()) {
            return SerialisationError{"could not format layer opacity"};
        }
        output += prefix + "enabled = " + (layer.enabled ? "true\n" : "false\n");
        output += prefix + "operation = " + std::string(operationName(layer.operation)) + "\n";
        output += prefix + "outputs = " + formatLayerOutputs(layer.outputs) + "\n";
        output += prefix + "composite = " + std::string(compositeModeName(layer.compositeMode)) + "\n";
        output += prefix + "opacity = " + opacity + "\n";
        const auto appendShape = [&](const ShapePrimitiveOperation& shape)
            -> std::optional<SerialisationError> {
            const auto width = formatDouble(shape.width);
            const auto height = formatDouble(shape.height);
            const auto cornerRadius = formatDouble(shape.cornerRadius);
            const auto inset = formatDouble(shape.inset);
            const auto borderWidth = formatDouble(shape.borderWidth);
            const auto softness = formatDouble(shape.softness);
            const auto offsetX = formatDouble(shape.offsetX);
            const auto offsetY = formatDouble(shape.offsetY);
            const auto stagger = formatDouble(shape.stagger);
            const auto rotation = formatDouble(shape.rotationDegrees);
            const auto innerRadius = formatDouble(shape.innerRadius);
            const auto arcStart = formatDouble(shape.arcStartDegrees);
            const auto arcSweep = formatDouble(shape.arcSweepDegrees);
            const auto crescentOffset = formatDouble(shape.crescentOffset);
            const auto radialRadius = formatDouble(shape.radialRadius);
            const auto radialPhase = formatDouble(shape.radialPhaseDegrees);
            if (width.empty() || height.empty() || cornerRadius.empty() || inset.empty() ||
                borderWidth.empty() || softness.empty() || offsetX.empty() ||
                offsetY.empty() || stagger.empty() || rotation.empty() ||
                innerRadius.empty() || arcStart.empty() || arcSweep.empty() ||
                crescentOffset.empty() || radialRadius.empty() || radialPhase.empty()) {
                return SerialisationError{"could not format shape parameters"};
            }
            output += prefix + "shape.kind = " +
                std::string(shapePrimitiveKindName(shape.kind)) + "\n";
            output += prefix + "shape.field = " +
                std::string(shapeFieldKindName(shape.field)) + "\n";
            output += prefix + "shape.columns = " + std::to_string(shape.columns) + "\n";
            output += prefix + "shape.rows = " + std::to_string(shape.rows) + "\n";
            output += prefix + "shape.width = " + width + "\n";
            output += prefix + "shape.height = " + height + "\n";
            output += prefix + "shape.corner_radius = " + cornerRadius + "\n";
            output += prefix + "shape.inset = " + inset + "\n";
            output += prefix + "shape.border_width = " + borderWidth + "\n";
            output += prefix + "shape.softness = " + softness + "\n";
            output += prefix + "shape.offset_x = " + offsetX + "\n";
            output += prefix + "shape.offset_y = " + offsetY + "\n";
            output += prefix + "shape.stagger = " + stagger + "\n";
            output += prefix + "shape.rotation = " + rotation + "\n";
            output += prefix + "shape.inner_radius = " + innerRadius + "\n";
            output += prefix + "shape.arc_start = " + arcStart + "\n";
            output += prefix + "shape.arc_sweep = " + arcSweep + "\n";
            output += prefix + "shape.crescent_offset = " + crescentOffset + "\n";
            output += prefix + "shape.radial_copies = " +
                std::to_string(shape.radialCopies) + "\n";
            output += prefix + "shape.radial_radius = " + radialRadius + "\n";
            output += prefix + "shape.radial_phase = " + radialPhase + "\n";
            output += prefix + "shape.radial_orientation = " +
                std::string(radialOrientationName(shape.radialOrientation)) + "\n";
            output += prefix + "shape.seed_offset = " +
                std::to_string(shape.seedOffset) + "\n";
            output += prefix + "shape.vertices = " +
                std::to_string(shape.vertices.size()) + "\n";
            for (std::size_t vertexIndex = 0;
                 vertexIndex < shape.vertices.size();
                 ++vertexIndex) {
                const auto x = formatDouble(shape.vertices[vertexIndex].x);
                const auto y = formatDouble(shape.vertices[vertexIndex].y);
                if (x.empty() || y.empty()) {
                    return SerialisationError{"could not format convex polygon vertices"};
                }
                const auto vertexPrefix = prefix + "shape.vertex." +
                    std::to_string(vertexIndex) + ".";
                output += vertexPrefix + "x = " + x + "\n";
                output += vertexPrefix + "y = " + y + "\n";
            }
            return std::nullopt;
        };
        if (const auto* noise = std::get_if<NoiseOperation>(&layer.operation)) {
            output += prefix + "noise.seed_offset = " + std::to_string(noise->seedOffset) + "\n";
        } else if (const auto* solid = std::get_if<SolidColourOperation>(&layer.operation)) {
            output += prefix + "solid.colour = " + formatColour(solid->colour) + "\n";
        } else if (const auto* value = std::get_if<SurfaceValueOperation>(&layer.operation)) {
            const auto formatted = formatDouble(value->value);
            if (formatted.empty()) {
                return SerialisationError{"could not format surface value"};
            }
            output += prefix + "surface.value = " + formatted + "\n";
        } else if (const auto* textile = std::get_if<TextileOperation>(&layer.operation)) {
            output += prefix + "textile.pattern = " +
                std::string(textilePatternName(textile->pattern)) + "\n";
            output += prefix + "textile.field = " +
                std::string(textileFieldName(textile->field)) + "\n";
            output += prefix + "textile.yarn_profile = " +
                std::string(yarnProfileName(textile->yarnProfile)) + "\n";
            output += prefix + "textile.tile_orientation = " +
                std::string(textileTileOrientationName(textile->tileOrientation)) + "\n";
            output += prefix + "textile.columns = " + std::to_string(textile->columns) + "\n";
            output += prefix + "textile.rows = " + std::to_string(textile->rows) + "\n";
            output += prefix + "textile.tile_columns = " + std::to_string(textile->tileColumns) + "\n";
            output += prefix + "textile.tile_rows = " + std::to_string(textile->tileRows) + "\n";
            output += prefix + "textile.weave_span = " + std::to_string(textile->weaveSpan) + "\n";
            output += prefix + "textile.twill_step = " + std::to_string(textile->twillStep) + "\n";
            output += prefix + "textile.fibre_frequency = " + std::to_string(textile->fibreFrequency) + "\n";
            const std::array<std::pair<std::string_view, double>, 13> values{{
                {"yarn_width", textile->yarnWidth},
                {"yarn_roundness", textile->yarnRoundness},
                {"crossing_height", textile->crossingHeight},
                {"jitter", textile->jitter},
                {"fibre_strength", textile->fibreStrength},
                {"twist", textile->twist},
                {"pile_radius", textile->pileRadius},
                {"pile_height", textile->pileHeight},
                {"missing", textile->missingAmount},
                {"damage", textile->damageAmount},
                {"different_colour", textile->differentColourAmount},
                {"colour_variation", textile->colourVariation},
                {"softness", textile->softness},
            }};
            for (const auto& [name, value] : values) {
                const auto formatted = formatDouble(value);
                if (formatted.empty()) {
                    return SerialisationError{"could not format textile parameters"};
                }
                output += prefix + "textile." + std::string(name) + " = " + formatted + "\n";
            }
            output += prefix + "textile.colour_low = " + formatColour(textile->lowColour) + "\n";
            output += prefix + "textile.colour_high = " + formatColour(textile->highColour) + "\n";
            output += prefix + "textile.colour_accent = " + formatColour(textile->accentColour) + "\n";
            output += prefix + "textile.colour_damage = " + formatColour(textile->damageColour) + "\n";
            output += prefix + "textile.seed_offset = " + std::to_string(textile->seedOffset) + "\n";
        } else if (const auto* attachment =
                       std::get_if<RegionAttachmentOperation>(&layer.operation)) {
            output += prefix + "attachment.kind = " +
                std::string(regionAttachmentKindName(attachment->kind)) + "\n";
            output += prefix + "attachment.field = " +
                std::string(regionAttachmentFieldName(attachment->field)) + "\n";
            output += prefix + "attachment.start_anchor = " +
                std::string(regionAnchorName(attachment->startAnchor)) + "\n";
            output += prefix + "attachment.end_anchor = " +
                std::string(regionAnchorName(attachment->endAnchor)) + "\n";
            output += prefix + "attachment.glyph = " +
                std::string(regionGlyphName(attachment->glyph)) + "\n";
            output += prefix + "attachment.count = " +
                std::to_string(attachment->count) + "\n";
            const std::array<std::pair<std::string_view, double>, 15> values{{
                {"size", attachment->size},
                {"aspect", attachment->aspect},
                {"inset", attachment->inset},
                {"rotation", attachment->rotationDegrees},
                {"jitter", attachment->jitter},
                {"selection", attachment->selection},
                {"line_width", attachment->lineWidth},
                {"length", attachment->length},
                {"branching", attachment->branching},
                {"softness", attachment->softness},
                {"height", attachment->height},
                {"roughness", attachment->roughness},
                {"metalness", attachment->metalness},
                {"occlusion", attachment->occlusion},
                {"emissive", attachment->emissive},
            }};
            for (const auto& [name, value] : values) {
                const auto formatted = formatDouble(value);
                if (formatted.empty()) {
                    return SerialisationError{"could not format region attachment parameters"};
                }
                output += prefix + "attachment." + std::string(name) + " = " +
                    formatted + "\n";
            }
            output += prefix + "attachment.colour = " +
                formatColour(attachment->colour) + "\n";
            output += prefix + "attachment.seed_offset = " +
                std::to_string(attachment->seedOffset) + "\n";
        } else if (const auto* levels = std::get_if<LevelsOperation>(&layer.operation)) {
            const auto low = formatDouble(levels->inputLow);
            const auto high = formatDouble(levels->inputHigh);
            const auto gamma = formatDouble(levels->gamma);
            if (low.empty() || high.empty() || gamma.empty()) {
                return SerialisationError{"could not format levels parameters"};
            }
            output += prefix + "levels.input_low = " + low + "\n";
            output += prefix + "levels.input_high = " + high + "\n";
            output += prefix + "levels.gamma = " + gamma + "\n";
        } else if (const auto* threshold = std::get_if<ThresholdOperation>(&layer.operation)) {
            const auto value = formatDouble(threshold->threshold);
            if (value.empty()) {
                return SerialisationError{"could not format threshold"};
            }
            output += prefix + "threshold.value = " + value + "\n";
        } else if (const auto* brick = std::get_if<BrickGridOperation>(&layer.operation)) {
            const auto stagger = formatDouble(brick->stagger);
            const auto softness = formatDouble(brick->softness);
            if (stagger.empty() || softness.empty()) {
                return SerialisationError{"could not format brick parameters"};
            }
            if (brick->physicalDimensions) {
                const auto width = formatMetres(brick->physicalDimensions->widthMetres);
                const auto height = formatMetres(brick->physicalDimensions->heightMetres);
                const auto mortar = formatMetres(brick->physicalDimensions->mortarMetres);
                if (width.empty() || height.empty() || mortar.empty()) {
                    return SerialisationError{"could not format physical brick parameters"};
                }
                output += prefix + "brick.sizing = physical\n";
                output += prefix + "brick.width = " + width + "\n";
                output += prefix + "brick.height = " + height + "\n";
                output += prefix + "brick.mortar_width = " + mortar + "\n";
            } else {
                const auto mortar = formatDouble(brick->mortar);
                if (mortar.empty()) {
                    return SerialisationError{"could not format brick mortar"};
                }
                output += prefix + "brick.sizing = relative\n";
                output += prefix + "brick.columns = " + std::to_string(brick->columns) + "\n";
                output += prefix + "brick.rows = " + std::to_string(brick->rows) + "\n";
                output += prefix + "brick.mortar = " + mortar + "\n";
                output += prefix + "brick.mortar_space = " +
                    std::string(
                        brick->mortarSpace == BrickMortarSpace::texture ? "texture" : "cell") +
                    "\n";
            }
            output += prefix + "brick.stagger = " + stagger + "\n";
            output += prefix + "brick.softness = " + softness + "\n";
        } else if (const auto* tile = std::get_if<TileGridOperation>(&layer.operation)) {
            const auto grout = formatDouble(tile->grout);
            const auto softness = formatDouble(tile->softness);
            if (grout.empty() || softness.empty()) {
                return SerialisationError{"could not format tile parameters"};
            }
            output += prefix + "tile.columns = " + std::to_string(tile->columns) + "\n";
            output += prefix + "tile.rows = " + std::to_string(tile->rows) + "\n";
            output += prefix + "tile.grout = " + grout + "\n";
            output += prefix + "tile.softness = " + softness + "\n";
        } else if (const auto* course =
                       std::get_if<CourseLayoutOperation>(&layer.operation)) {
            const auto blockVariation = formatDouble(course->blockVariation);
            const auto heightVariation = formatDouble(course->courseVariation);
            const auto stagger = formatDouble(course->stagger);
            const auto crookedness = formatDouble(course->crookedness);
            const auto gap = formatDouble(course->gap);
            const auto softness = formatDouble(course->softness);
            const auto overlap = formatDouble(course->overlap);
            if (blockVariation.empty() || heightVariation.empty() || stagger.empty() ||
                crookedness.empty() || gap.empty() || softness.empty() || overlap.empty()) {
                return SerialisationError{"could not format course layout parameters"};
            }
            output += prefix + "course.profile = " +
                std::string(courseLayoutProfileName(course->profile)) + "\n";
            output += prefix + "course.field = " +
                std::string(courseLayoutFieldName(course->field)) + "\n";
            output += prefix + "course.sizing = " +
                (course->physicalDimensions ? "physical\n" : "relative\n");
            output += prefix + "course.blocks = " + std::to_string(course->blocks) + "\n";
            output += prefix + "course.courses = " + std::to_string(course->courses) + "\n";
            output += prefix + "course.block_variation = " + blockVariation + "\n";
            output += prefix + "course.height_variation = " + heightVariation + "\n";
            output += prefix + "course.stagger = " + stagger + "\n";
            output += prefix + "course.crookedness = " + crookedness + "\n";
            output += prefix + "course.gap = " + gap + "\n";
            output += prefix + "course.softness = " + softness + "\n";
            output += prefix + "course.overlap = " + overlap + "\n";
            output += prefix + "course.seed_offset = " +
                std::to_string(course->seedOffset) + "\n";
            if (course->physicalDimensions) {
                const auto width = formatMetres(
                    course->physicalDimensions->blockWidthMetres);
                const auto height = formatMetres(
                    course->physicalDimensions->courseHeightMetres);
                const auto physicalGap = formatMetres(
                    course->physicalDimensions->gapMetres);
                const auto physicalOverlap = formatMetres(
                    course->physicalDimensions->overlapMetres);
                if (width.empty() || height.empty() || physicalGap.empty() ||
                    physicalOverlap.empty()) {
                    return SerialisationError{
                        "could not format physical course layout parameters"};
                }
                output += prefix + "course.block_width = " + width + "\n";
                output += prefix + "course.course_height = " + height + "\n";
                output += prefix + "course.gap_width = " + physicalGap + "\n";
                output += prefix + "course.overlap_depth = " + physicalOverlap + "\n";
            }
        } else if (const auto* sculpt =
                       std::get_if<RegionSurfaceOperation>(&layer.operation)) {
            const auto bevelWidth = formatDouble(sculpt->bevelWidth);
            const auto bevelHeight = formatDouble(sculpt->bevelHeight);
            const auto facetStrength = formatDouble(sculpt->facetStrength);
            const auto centrePeak = formatDouble(sculpt->centrePeak);
            const auto slope = formatDouble(sculpt->slopeStrength);
            const auto chips = formatDouble(sculpt->chipAmount);
            const auto wear = formatDouble(sculpt->wearAmount);
            const auto erosion = formatDouble(sculpt->erosionAmount);
            if (bevelWidth.empty() || bevelHeight.empty() || facetStrength.empty() ||
                centrePeak.empty() || slope.empty() || chips.empty() || wear.empty() ||
                erosion.empty()) {
                return SerialisationError{
                    "could not format region surface sculpt parameters"};
            }
            output += prefix + "sculpt.field = " +
                std::string(regionSurfaceFieldName(sculpt->field)) + "\n";
            output += prefix + "sculpt.profile = " +
                std::string(bevelProfileName(sculpt->profile)) + "\n";
            output += prefix + "sculpt.bevel_width = " + bevelWidth + "\n";
            output += prefix + "sculpt.bevel_height = " + bevelHeight + "\n";
            output += prefix + "sculpt.facet_count = " +
                std::to_string(sculpt->facetCount) + "\n";
            output += prefix + "sculpt.facet_strength = " + facetStrength + "\n";
            output += prefix + "sculpt.centre_peak = " + centrePeak + "\n";
            output += prefix + "sculpt.slope = " + slope + "\n";
            output += prefix + "sculpt.chips = " + chips + "\n";
            output += prefix + "sculpt.chip_scale = " +
                std::to_string(sculpt->chipScale) + "\n";
            output += prefix + "sculpt.wear = " + wear + "\n";
            output += prefix + "sculpt.erosion = " + erosion + "\n";
            output += prefix + "sculpt.seed_offset = " +
                std::to_string(sculpt->seedOffset) + "\n";
            output += prefix + "sculpt.faceted_normals = " +
                (sculpt->facetedNormals ? "true\n" : "false\n");
            output += prefix + "sculpt.target = " +
                std::string(processingTargetName(sculpt->target)) + "\n";
        } else if (const auto* worley = std::get_if<WorleyCellsOperation>(&layer.operation)) {
            const auto jitter = formatDouble(worley->jitter);
            const auto edgeWidth = formatDouble(worley->edgeWidth);
            if (jitter.empty() || edgeWidth.empty()) {
                return SerialisationError{"could not format Worley parameters"};
            }
            output += prefix + "worley.columns = " + std::to_string(worley->columns) + "\n";
            output += prefix + "worley.rows = " + std::to_string(worley->rows) + "\n";
            output += prefix + "worley.jitter = " + jitter + "\n";
            output += prefix + "worley.edge_width = " + edgeWidth + "\n";
            output += prefix + "worley.seed_offset = " +
                std::to_string(worley->seedOffset) + "\n";
        } else if (const auto* cells = std::get_if<RandomCellsOperation>(&layer.operation)) {
            output += prefix + "random_cells.columns = " +
                std::to_string(cells->columns) + "\n";
            output += prefix + "random_cells.rows = " +
                std::to_string(cells->rows) + "\n";
            output += prefix + "random_cells.seed_offset = " +
                std::to_string(cells->seedOffset) + "\n";
        } else if (const auto* lines = std::get_if<LinesOperation>(&layer.operation)) {
            const auto width = formatDouble(lines->width);
            const auto softness = formatDouble(lines->softness);
            if (width.empty() || softness.empty()) {
                return SerialisationError{"could not format line parameters"};
            }
            output += prefix + "lines.direction = " +
                std::string(lineDirectionName(lines->direction)) + "\n";
            output += prefix + "lines.count = " + std::to_string(lines->count) + "\n";
            output += prefix + "lines.width = " + width + "\n";
            output += prefix + "lines.softness = " + softness + "\n";
        } else if (const auto* rectangles =
                       std::get_if<RectanglesOperation>(&layer.operation)) {
            const auto width = formatDouble(rectangles->width);
            const auto height = formatDouble(rectangles->height);
            const auto softness = formatDouble(rectangles->softness);
            if (width.empty() || height.empty() || softness.empty()) {
                return SerialisationError{"could not format rectangle parameters"};
            }
            output += prefix + "rectangles.columns = " +
                std::to_string(rectangles->columns) + "\n";
            output += prefix + "rectangles.rows = " +
                std::to_string(rectangles->rows) + "\n";
            output += prefix + "rectangles.width = " + width + "\n";
            output += prefix + "rectangles.height = " + height + "\n";
            output += prefix + "rectangles.softness = " + softness + "\n";
        } else if (const auto* circles = std::get_if<CirclesOperation>(&layer.operation)) {
            const auto radius = formatDouble(circles->radius);
            const auto softness = formatDouble(circles->softness);
            if (radius.empty() || softness.empty()) {
                return SerialisationError{"could not format circle parameters"};
            }
            output += prefix + "circles.columns = " +
                std::to_string(circles->columns) + "\n";
            output += prefix + "circles.rows = " + std::to_string(circles->rows) + "\n";
            output += prefix + "circles.radius = " + radius + "\n";
            output += prefix + "circles.softness = " + softness + "\n";
        } else if (const auto* shape =
                       std::get_if<ShapePrimitiveOperation>(&layer.operation)) {
            if (const auto error = appendShape(*shape)) {
                return *error;
            }
        } else if (const auto* boolean =
                       std::get_if<ShapeBooleanOperation>(&layer.operation)) {
            if (const auto error = appendShape(boolean->shape)) {
                return *error;
            }
            output += prefix + "shape.boolean = " +
                std::string(shapeBooleanModeName(boolean->mode)) + "\n";
            output += prefix + "shape.target = " +
                std::string(processingTargetName(boolean->target)) + "\n";
        } else if (const auto* scatter =
                       std::get_if<ScatterOperation>(&layer.operation)) {
            if (const auto error = appendShape(scatter->stamp)) {
                return *error;
            }
            const auto density = formatDouble(scatter->density);
            const auto jitter = formatDouble(scatter->jitter);
            const auto minimumDistance = formatDouble(scatter->minimumDistance);
            const auto maximumOverlap = formatDouble(scatter->maximumOverlap);
            if (density.empty() || jitter.empty() || minimumDistance.empty() ||
                maximumOverlap.empty()) {
                return SerialisationError{"could not format scatter placement parameters"};
            }
            output += prefix + "scatter.field = " +
                std::string(scatterFieldName(scatter->field)) + "\n";
            output += prefix + "scatter.columns = " +
                std::to_string(scatter->columns) + "\n";
            output += prefix + "scatter.rows = " +
                std::to_string(scatter->rows) + "\n";
            output += prefix + "scatter.density = " + density + "\n";
            output += prefix + "scatter.jitter = " + jitter + "\n";
            output += prefix + "scatter.minimum_distance = " + minimumDistance + "\n";
            output += prefix + "scatter.overlap = " +
                std::string(scatterOverlapModeName(scatter->overlapMode)) + "\n";
            output += prefix + "scatter.maximum_overlap = " + maximumOverlap + "\n";
            output += prefix + "scatter.seed_offset = " +
                std::to_string(scatter->seedOffset) + "\n";
            output += prefix + "scatter.populations = " +
                std::to_string(scatter->populations.size()) + "\n";
            for (std::size_t populationIndex = 0;
                 populationIndex < scatter->populations.size();
                 ++populationIndex) {
                const auto& population = scatter->populations[populationIndex];
                const std::array<std::pair<std::string_view, double>, 11> values{{
                    {"weight", population.weight},
                    {"min_scale", population.minimumScale},
                    {"max_scale", population.maximumScale},
                    {"min_aspect", population.minimumAspect},
                    {"max_aspect", population.maximumAspect},
                    {"min_rotation", population.minimumRotation},
                    {"max_rotation", population.maximumRotation},
                    {"min_height", population.minimumHeight},
                    {"max_height", population.maximumHeight},
                    {"min_roughness", population.minimumRoughness},
                    {"max_roughness", population.maximumRoughness},
                }};
                const auto populationPrefix = prefix + "scatter.population." +
                    std::to_string(populationIndex) + ".";
                for (const auto& [name, value] : values) {
                    const auto formatted = formatDouble(value);
                    if (formatted.empty()) {
                        return SerialisationError{"could not format scatter population parameters"};
                    }
                    output += populationPrefix + std::string(name) + " = " + formatted + "\n";
                }
                output += populationPrefix + "colour_low = " +
                    formatColour(population.lowColour) + "\n";
                output += populationPrefix + "colour_high = " +
                    formatColour(population.highColour) + "\n";
            }
            const auto appendScatterMask = [&](std::string_view name, const ScatterMask& mask)
                -> std::optional<SerialisationError> {
                const auto inputLow = formatDouble(mask.inputLow);
                const auto inputHigh = formatDouble(mask.inputHigh);
                if (inputLow.empty() || inputHigh.empty()) {
                    return SerialisationError{"could not format scatter mask parameters"};
                }
                const auto maskPrefix = prefix + "scatter." + std::string(name) + ".";
                output += maskPrefix + "enabled = " + (mask.enabled ? "true\n" : "false\n");
                output += maskPrefix + "inverted = " + (mask.inverted ? "true\n" : "false\n");
                output += maskPrefix + "frequency = " + std::to_string(mask.frequency) + "\n";
                output += maskPrefix + "input_low = " + inputLow + "\n";
                output += maskPrefix + "input_high = " + inputHigh + "\n";
                output += maskPrefix + "seed_offset = " + std::to_string(mask.seedOffset) + "\n";
                return std::nullopt;
            };
            if (const auto error = appendScatterMask("density_mask", scatter->densityMask)) {
                return *error;
            }
            if (const auto error = appendScatterMask("exclusion_mask", scatter->exclusionMask)) {
                return *error;
            }
        } else if (const auto* organic =
                       std::get_if<OrganicCellOperation>(&layer.operation)) {
            const std::array<std::pair<std::string_view, double>, 5> values{{
                {"anisotropy", organic->anisotropy},
                {"jitter", organic->jitter},
                {"irregularity", organic->irregularity},
                {"gap", organic->gap},
                {"softness", organic->softness},
            }};
            output += prefix + "organic.cell.field = " +
                std::string(organicCellFieldName(organic->field)) + "\n";
            output += prefix + "organic.cell.direction = " +
                std::string(organicDirectionName(organic->direction)) + "\n";
            output += prefix + "organic.cell.columns = " + std::to_string(organic->columns) + "\n";
            output += prefix + "organic.cell.rows = " + std::to_string(organic->rows) + "\n";
            for (const auto& [name, value] : values) {
                const auto formatted = formatDouble(value);
                if (formatted.empty()) return SerialisationError{"could not format organic cell parameters"};
                output += prefix + "organic.cell." + std::string(name) + " = " + formatted + "\n";
            }
            output += prefix + "organic.cell.seed_offset = " +
                std::to_string(organic->seedOffset) + "\n";
        } else if (const auto* cracks =
                       std::get_if<OrganicCrackOperation>(&layer.operation)) {
            const std::array<std::pair<std::string_view, double>, 5> values{{
                {"branch_probability", cracks->branchProbability},
                {"bend", cracks->bend},
                {"width", cracks->width},
                {"taper", cracks->taper},
                {"softness", cracks->softness},
            }};
            output += prefix + "organic.crack.field = " +
                std::string(organicCrackFieldName(cracks->field)) + "\n";
            output += prefix + "organic.crack.direction = " +
                std::string(organicDirectionName(cracks->direction)) + "\n";
            output += prefix + "organic.crack.roots = " + std::to_string(cracks->roots) + "\n";
            output += prefix + "organic.crack.segments = " + std::to_string(cracks->segments) + "\n";
            output += prefix + "organic.crack.branch_levels = " +
                std::to_string(cracks->branchLevels) + "\n";
            for (const auto& [name, value] : values) {
                const auto formatted = formatDouble(value);
                if (formatted.empty()) return SerialisationError{"could not format organic crack parameters"};
                output += prefix + "organic.crack." + std::string(name) + " = " + formatted + "\n";
            }
            output += prefix + "organic.crack.seed_offset = " +
                std::to_string(cracks->seedOffset) + "\n";
        } else if (const auto* leaves =
                       std::get_if<LeafClusterOperation>(&layer.operation)) {
            const std::array<std::pair<std::string_view, double>, 27> values{{
                {"density", leaves->density},
                {"cluster_spread", leaves->clusterSpread},
                {"length", leaves->leafLength},
                {"width", leaves->leafWidth},
                {"scale_variation", leaves->scaleVariation},
                {"rotation_variation", leaves->rotationVariation},
                {"direction", leaves->directionDegrees},
                {"taper", leaves->taper},
                {"base_notch", leaves->baseNotch},
                {"curvature", leaves->curvature},
                {"serration", leaves->serration},
                {"lobing", leaves->lobing},
                {"midrib_width", leaves->midribWidth},
                {"vein_width", leaves->veinWidth},
                {"edge_width", leaves->edgeWidth},
                {"softness", leaves->softness},
                {"min_height", leaves->minimumHeight},
                {"max_height", leaves->maximumHeight},
                {"min_roughness", leaves->minimumRoughness},
                {"inner_highlight_width", leaves->innerHighlightWidth},
                {"inner_highlight_inset", leaves->innerHighlightInset},
                {"cluster_colour_variation", leaves->clusterColourVariation},
                {"instance_colour_variation", leaves->instanceColourVariation},
                {"secondary_weight", leaves->secondaryWeight},
                {"secondary_scale", leaves->secondaryScale},
                {"tertiary_weight", leaves->tertiaryWeight},
                {"tertiary_scale", leaves->tertiaryScale},
            }};
            output += prefix + "leaf.field = " + std::string(leafFieldName(leaves->field)) + "\n";
            output += prefix + "leaf.profile = " + std::string(leafProfileName(leaves->profile)) + "\n";
            output += prefix + "leaf.pattern = " +
                std::string(leafClusterPatternName(leaves->pattern)) + "\n";
            output += prefix + "leaf.columns = " + std::to_string(leaves->columns) + "\n";
            output += prefix + "leaf.rows = " + std::to_string(leaves->rows) + "\n";
            output += prefix + "leaf.per_cluster = " +
                std::to_string(leaves->leavesPerCluster) + "\n";
            for (const auto& [name, value] : values) {
                const auto formatted = formatDouble(value);
                if (formatted.empty()) return SerialisationError{"could not format leaf parameters"};
                output += prefix + "leaf." + std::string(name) + " = " + formatted + "\n";
            }
            const auto maximumRoughness = formatDouble(leaves->maximumRoughness);
            if (maximumRoughness.empty()) return SerialisationError{"could not format leaf roughness"};
            output += prefix + "leaf.max_roughness = " + maximumRoughness + "\n";
            output += prefix + "leaf.serration_count = " +
                std::to_string(leaves->serrationCount) + "\n";
            output += prefix + "leaf.lobe_count = " + std::to_string(leaves->lobeCount) + "\n";
            output += prefix + "leaf.vein_pairs = " + std::to_string(leaves->veinPairs) + "\n";
            output += prefix + "leaf.colour_low = " + formatColour(leaves->lowColour) + "\n";
            output += prefix + "leaf.colour_high = " + formatColour(leaves->highColour) + "\n";
            output += prefix + "leaf.secondary_profile = " +
                std::string(leafProfileName(leaves->secondaryProfile)) + "\n";
            output += prefix + "leaf.secondary_colour_low = " +
                formatColour(leaves->secondaryLowColour) + "\n";
            output += prefix + "leaf.secondary_colour_high = " +
                formatColour(leaves->secondaryHighColour) + "\n";
            output += prefix + "leaf.tertiary_profile = " +
                std::string(leafProfileName(leaves->tertiaryProfile)) + "\n";
            output += prefix + "leaf.tertiary_colour_low = " +
                formatColour(leaves->tertiaryLowColour) + "\n";
            output += prefix + "leaf.tertiary_colour_high = " +
                formatColour(leaves->tertiaryHighColour) + "\n";
            output += prefix + "leaf.seed_offset = " + std::to_string(leaves->seedOffset) + "\n";
        } else if (const auto* growth =
                       std::get_if<OrganicAccumulationOperation>(&layer.operation)) {
            const std::array<std::pair<std::string_view, double>, 8> values{{
                {"coverage", growth->coverage},
                {"softness", growth->softness},
                {"moisture", growth->moistureBias},
                {"breakup", growth->breakup},
                {"variation", growth->variation},
                {"outline_width", growth->outlineWidth},
                {"inner_highlight_width", growth->innerHighlightWidth},
                {"inner_highlight_inset", growth->innerHighlightInset},
            }};
            output += prefix + "organic.accumulation.kind = " +
                std::string(organicAccumulationKindName(growth->kind)) + "\n";
            output += prefix + "organic.accumulation.source = " +
                std::string(organicAccumulationSourceName(growth->source)) + "\n";
            output += prefix + "organic.accumulation.profile = " +
                std::string(organicAccumulationProfileName(growth->profile)) + "\n";
            output += prefix + "organic.accumulation.field = " +
                std::string(organicAccumulationFieldName(growth->field)) + "\n";
            output += prefix + "organic.accumulation.scale = " +
                std::to_string(growth->scale) + "\n";
            for (const auto& [name, value] : values) {
                const auto formatted = formatDouble(value);
                if (formatted.empty()) return SerialisationError{"could not format organic accumulation parameters"};
                output += prefix + "organic.accumulation." + std::string(name) + " = " + formatted + "\n";
            }
            output += prefix + "organic.accumulation.colour_low = " +
                formatColour(growth->lowColour) + "\n";
            output += prefix + "organic.accumulation.colour_high = " +
                formatColour(growth->highColour) + "\n";
            output += prefix + "organic.accumulation.seed_offset = " +
                std::to_string(growth->seedOffset) + "\n";
            output += prefix + "organic.accumulation.target = " +
                std::string(processingTargetName(growth->target)) + "\n";
        } else if (const auto* lattice =
                       std::get_if<LatticeOperation>(&layer.operation)) {
            const auto width = formatDouble(lattice->width);
            const auto softness = formatDouble(lattice->softness);
            const auto phase = formatDouble(lattice->phase);
            if (width.empty() || softness.empty() || phase.empty()) {
                return SerialisationError{"could not format lattice parameters"};
            }
            output += prefix + "lattice.kind = " +
                std::string(latticeKindName(lattice->kind)) + "\n";
            output += prefix + "lattice.winding_x = " +
                std::to_string(lattice->windingX) + "\n";
            output += prefix + "lattice.winding_y = " +
                std::to_string(lattice->windingY) + "\n";
            output += prefix + "lattice.width = " + width + "\n";
            output += prefix + "lattice.softness = " + softness + "\n";
            output += prefix + "lattice.phase = " + phase + "\n";
        } else if (const auto* surface =
                       std::get_if<SurfacePatternOperation>(&layer.operation)) {
            const auto width = formatDouble(surface->width);
            const auto detail = formatDouble(surface->detail);
            const auto distortion = formatDouble(surface->distortion);
            const auto variation = formatDouble(surface->variation);
            if (width.empty() || detail.empty() || distortion.empty() || variation.empty()) {
                return SerialisationError{"could not format surface pattern parameters"};
            }
            output += prefix + "surface.kind = " +
                std::string(surfacePatternKindName(surface->kind)) + "\n";
            output += prefix + "surface.scale = " + std::to_string(surface->scale) + "\n";
            output += prefix + "surface.width = " + width + "\n";
            output += prefix + "surface.detail = " + detail + "\n";
            output += prefix + "surface.distortion = " + distortion + "\n";
            output += prefix + "surface.variation = " + variation + "\n";
            output += prefix + "surface.seed_offset = " +
                std::to_string(surface->seedOffset) + "\n";
        } else if (const auto* filter =
                       std::get_if<SurfaceFilterOperation>(&layer.operation)) {
            const auto radius = formatDouble(filter->radius);
            const auto strength = formatDouble(filter->strength);
            const auto sensitivity = formatDouble(filter->sensitivity);
            if (radius.empty() || strength.empty() || sensitivity.empty()) {
                return SerialisationError{"could not format surface filter parameters"};
            }
            output += prefix + "filter.kind = " +
                std::string(surfaceFilterKindName(filter->kind)) + "\n";
            output += prefix + "filter.radius = " + radius + "\n";
            output += prefix + "filter.strength = " + strength + "\n";
            output += prefix + "filter.sensitivity = " + sensitivity + "\n";
            output += prefix + "filter.target = " +
                std::string(processingTargetName(filter->target)) + "\n";
        } else if (const auto* posterise =
                       std::get_if<PosteriseOperation>(&layer.operation)) {
            output += prefix + "posterise.bands = " +
                std::to_string(posterise->bands) + "\n";
            output += prefix + "posterise.target = " +
                std::string(processingTargetName(posterise->target)) + "\n";
        } else if (const auto* ramp =
                       std::get_if<ColourRampOperation>(&layer.operation)) {
            output += prefix + "ramp.mode = " +
                std::string(colourRampModeName(ramp->mode)) + "\n";
            output += prefix + "ramp.stops = " +
                std::to_string(ramp->stops.size()) + "\n";
            for (std::size_t stopIndex = 0; stopIndex < ramp->stops.size(); ++stopIndex) {
                const auto position = formatDouble(ramp->stops[stopIndex].position);
                if (position.empty()) {
                    return SerialisationError{"could not format colour ramp stop position"};
                }
                const auto stopPrefix = prefix + "ramp.stop." +
                    std::to_string(stopIndex) + ".";
                output += stopPrefix + "position = " + position + "\n";
                output += stopPrefix + "colour = " +
                    formatColour(ramp->stops[stopIndex].colour) + "\n";
            }
        } else if (const auto* palette =
                       std::get_if<PaletteOperation>(&layer.operation)) {
            output += prefix + "palette.colours = " +
                std::to_string(palette->colours.size()) + "\n";
            for (std::size_t colourIndex = 0;
                 colourIndex < palette->colours.size();
                 ++colourIndex) {
                output += prefix + "palette.entry." + std::to_string(colourIndex) +
                    ".colour = " + formatColour(palette->colours[colourIndex]) + "\n";
            }
        } else if (const auto* contour =
                       std::get_if<InkContourOperation>(&layer.operation)) {
            const auto radius = formatDouble(contour->radius);
            const auto threshold = formatDouble(contour->threshold);
            const auto softness = formatDouble(contour->softness);
            const auto strength = formatDouble(contour->strength);
            if (radius.empty() || threshold.empty() || softness.empty() ||
                strength.empty()) {
                return SerialisationError{"could not format ink contour parameters"};
            }
            output += prefix + "ink.colour = " + formatColour(contour->colour) + "\n";
            output += prefix + "ink.radius = " + radius + "\n";
            output += prefix + "ink.threshold = " + threshold + "\n";
            output += prefix + "ink.softness = " + softness + "\n";
            output += prefix + "ink.strength = " + strength + "\n";
            output += prefix + "ink.inverted = " +
                (contour->inverted ? "true\n" : "false\n");
        } else if (const auto* region =
                       std::get_if<RegionFieldOperation>(&layer.operation)) {
            const auto outputLow = formatDouble(region->outputLow);
            const auto outputHigh = formatDouble(region->outputHigh);
            if (outputLow.empty() || outputHigh.empty()) {
                return SerialisationError{"could not format region field parameters"};
            }
            output += prefix + "region.field = " +
                std::string(regionFieldKindName(region->field)) + "\n";
            output += prefix + "region.seed_offset = " +
                std::to_string(region->seedOffset) + "\n";
            output += prefix + "region.channel = " +
                std::to_string(region->channel) + "\n";
            output += prefix + "region.output_low = " + outputLow + "\n";
            output += prefix + "region.output_high = " + outputHigh + "\n";
            output += prefix + "region.inverted = " +
                (region->inverted ? "true\n" : "false\n");
            output += prefix + "region.target = " +
                std::string(processingTargetName(region->target)) + "\n";
        }

        const auto offsetX = formatDouble(layer.transform.offsetX);
        const auto offsetY = formatDouble(layer.transform.offsetY);
        const auto warpStrength = formatDouble(layer.transform.warpStrength);
        const auto maskLow = formatDouble(layer.mask.inputLow);
        const auto maskHigh = formatDouble(layer.mask.inputHigh);
        if (offsetX.empty() || offsetY.empty() || warpStrength.empty() ||
            maskLow.empty() || maskHigh.empty()) {
            return SerialisationError{"could not format transform or mask parameters"};
        }
        output += prefix + "transform.scale_x = " + std::to_string(layer.transform.scaleX) + "\n";
        output += prefix + "transform.scale_y = " + std::to_string(layer.transform.scaleY) + "\n";
        output += prefix + "transform.offset_x = " + offsetX + "\n";
        output += prefix + "transform.offset_y = " + offsetY + "\n";
        output += prefix + "transform.rotation = " +
            std::to_string(rotationDegrees(layer.transform.rotation)) + "\n";
        output += prefix + "warp.enabled = " +
            (layer.transform.warpEnabled ? "true\n" : "false\n");
        output += prefix + "warp.strength = " + warpStrength + "\n";
        output += prefix + "warp.frequency = " +
            std::to_string(layer.transform.warpFrequency) + "\n";
        output += prefix + "warp.seed_offset = " +
            std::to_string(layer.transform.warpSeedOffset) + "\n";
        output += prefix + "mask.enabled = " + (layer.mask.enabled ? "true\n" : "false\n");
        output += prefix + "mask.inverted = " + (layer.mask.inverted ? "true\n" : "false\n");
        output += prefix + "mask.seed_offset = " + std::to_string(layer.mask.seedOffset) + "\n";
        output += prefix + "mask.input_low = " + maskLow + "\n";
        output += prefix + "mask.input_high = " + maskHigh + "\n";
    }
    return output;
}

} // namespace paperweight
