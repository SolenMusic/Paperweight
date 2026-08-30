#include <paperweight/pmat.hpp>

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
    return std::nullopt;
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
                            "layer operation must be 'noise', 'solid_colour', 'levels', or 'threshold'");
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
        if (!seen[index] && !optionalInVersionOne) {
            return diagnostic(
                lineNumber + 1,
                1,
                "missing required key '" + std::string(fieldKeys[index]) + "'");
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
            if (!std::isfinite(*builder.opacity.value) ||
                *builder.opacity.value < LayerLimits::minimumOpacity ||
                *builder.opacity.value > LayerLimits::maximumOpacity) {
                return diagnostic(
                    builder.opacity.line,
                    builder.opacity.column,
                    "layer opacity must be finite and between 0 and 1");
            }

            MaterialLayer layer{
                *builder.enabled.value,
                *builder.opacity.value,
                *builder.compositeMode.value,
                NoiseOperation{},
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
                    builder.levelsGamma.value || builder.threshold.value) {
                    return diagnostic(lineNumber + 1, 1, "noise layer contains parameters for another operation");
                }
                layer.operation = NoiseOperation{*builder.seedOffset.value};
                break;
            case OperationKind::solidColour:
                if (!builder.solidColour.value) {
                    return missingLayerField(lineNumber + 1, index, "solid.colour");
                }
                if (builder.seedOffset.value || builder.levelsLow.value || builder.levelsHigh.value ||
                    builder.levelsGamma.value || builder.threshold.value) {
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
                if (builder.seedOffset.value || builder.solidColour.value || builder.threshold.value) {
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
                    builder.levelsHigh.value || builder.levelsGamma.value) {
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
            }
            material.layers.push_back(std::move(layer));
        }
    }

    if (const auto error = validateMaterial(material)) {
        Field relevantField = Field::frequency;
        if (material.frequency >= MaterialLimits::minimumFrequency &&
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
    if (gain.empty() || normalStrength.empty() || roughnessLow.empty() || roughnessHigh.empty()) {
        return SerialisationError{"could not format a decimal material parameter"};
    }

    std::string output;
    output.reserve(320 + material.layers.size() * 192);
    output += "# Paperweight procedural material\n";
    output += "pmat.version = " + std::to_string(currentPmatVersion) + "\n";
    output += "material.type = fbm\n";
    output += "material.seed = " + std::to_string(material.seed) + "\n";
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
        }
    }
    return output;
}

} // namespace paperweight
