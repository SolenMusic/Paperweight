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
    physicalWidth,
    physicalHeight,
    lowColour,
    highColour,
    frequency,
    octaves,
    lacunarity,
    gain,
    normalStrength,
    roughnessLow,
    roughnessHigh,
    layerCount,
    count,
};

constexpr std::array<std::string_view, static_cast<std::size_t>(Field::count)> fieldKeys{
    "pmat.version",
    "material.type",
    "material.seed",
    "material.width",
    "material.height",
    "colour.low",
    "colour.high",
    "noise.frequency",
    "noise.octaves",
    "noise.lacunarity",
    "noise.gain",
    "normal.strength",
    "roughness.low",
    "roughness.high",
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

struct LayerBuilder {
    ParsedValue<bool> enabled;
    ParsedValue<double> opacity;
    ParsedValue<CompositeMode> compositeMode;
    ParsedValue<OperationKind> operation;
    ParsedValue<std::uint64_t> seedOffset;
    ParsedValue<Rgba8> solidColour;
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
    return std::nullopt;
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
        operation == OperationKind::circles;
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

bool hasStructuralFields(const LayerBuilder& builder)
{
    return hasVersionFourFields(builder) || hasVersionFiveFields(builder) ||
        hasVersionSixFields(builder) || hasVersionSevenFields(builder) ||
        hasVersionEightFields(builder) || hasVersionNineFields(builder);
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
                            "layer composite must be 'blend', 'add', or 'multiply'");
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
                } else if (property == "region.field") {
                    const auto parsed = parseRegionFieldKind(value);
                    if (!parsed) {
                        return diagnostic(
                            lineNumber,
                            valueColumn,
                            "region field must be 'random', 'local_u', 'local_v', 'centre_distance', or 'boundary_distance'");
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
        if (!seen[index] && !optionalInVersionOne &&
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
            };
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
            const int operationGroupCount = static_cast<int>(hasBrickFields) +
                static_cast<int>(hasTileFields) + static_cast<int>(hasWorleyFields) +
                static_cast<int>(hasRandomFields) + static_cast<int>(hasLineFields) +
                static_cast<int>(hasRectangleFields) + static_cast<int>(hasCircleFields) +
                static_cast<int>(hasSurfaceFields) + static_cast<int>(hasFilterFields) +
                static_cast<int>(hasPosteriseFields) + static_cast<int>(hasRampFields) +
                static_cast<int>(hasPaletteFields) + static_cast<int>(hasInkFields) +
                static_cast<int>(hasRegionFields);

            const bool hasClassicFields = builder.seedOffset.value || builder.solidColour.value ||
                builder.levelsLow.value || builder.levelsHigh.value ||
                builder.levelsGamma.value || builder.threshold.value;
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
                    hasStructuralFields(builder)) {
                    return diagnostic(
                        lineNumber + 1,
                        1,
                        "solid-colour layer contains parameters for another operation");
                }
                layer.operation = SolidColourOperation{*builder.solidColour.value};
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
                    builder.threshold.value || hasStructuralFields(builder)) {
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
        if (error->find("physical") != std::string::npos && formatVersion >= 6) {
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
    const auto physicalWidth = formatMetres(material.physicalSize.widthMetres);
    const auto physicalHeight = formatMetres(material.physicalSize.heightMetres);
    if (gain.empty() || normalStrength.empty() || roughnessLow.empty() || roughnessHigh.empty() ||
        physicalWidth.empty() || physicalHeight.empty()) {
        return SerialisationError{"could not format a decimal material parameter"};
    }

    std::string output;
    output.reserve(320 + material.layers.size() * 800);
    output += "# Paperweight procedural material\n";
    output += "pmat.version = " + std::to_string(currentPmatVersion) + "\n";
    output += "material.type = fbm\n";
    output += "material.seed = " + std::to_string(material.seed) + "\n";
    output += "material.width = " + physicalWidth + "\n";
    output += "material.height = " + physicalHeight + "\n";
    output += "colour.low = " + formatColour(material.lowColour) + "\n";
    output += "colour.high = " + formatColour(material.highColour) + "\n";
    output += "noise.frequency = " + std::to_string(material.frequency) + "\n";
    output += "noise.octaves = " + std::to_string(material.octaves) + "\n";
    output += "noise.lacunarity = " + std::to_string(material.lacunarity) + "\n";
    output += "noise.gain = " + gain + "\n";
    output += "normal.strength = " + normalStrength + "\n";
    output += "roughness.low = " + roughnessLow + "\n";
    output += "roughness.high = " + roughnessHigh + "\n";
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
        output += prefix + "composite = " + std::string(compositeModeName(layer.compositeMode)) + "\n";
        output += prefix + "opacity = " + opacity + "\n";
        if (const auto* noise = std::get_if<NoiseOperation>(&layer.operation)) {
            output += prefix + "noise.seed_offset = " + std::to_string(noise->seedOffset) + "\n";
        } else if (const auto* solid = std::get_if<SolidColourOperation>(&layer.operation)) {
            output += prefix + "solid.colour = " + formatColour(solid->colour) + "\n";
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
