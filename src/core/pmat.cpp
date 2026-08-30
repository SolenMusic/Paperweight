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

namespace paperweight {
namespace {

enum class Field : std::size_t {
    version,
    type,
    seed,
    frequency,
    octaves,
    lacunarity,
    gain,
    count,
};

constexpr std::array<std::string_view, static_cast<std::size_t>(Field::count)> fieldKeys{
    "pmat.version",
    "material.type",
    "material.seed",
    "noise.frequency",
    "noise.octaves",
    "noise.lacunarity",
    "noise.gain",
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

ParseDiagnostic diagnostic(std::size_t line, std::size_t column, std::string message)
{
    return ParseDiagnostic{line, column, std::move(message)};
}

std::string formatDouble(double value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return stream.str();
}

} // namespace

ParseResult parsePmat(std::string_view text)
{
    Material material;
    std::array<bool, static_cast<std::size_t>(Field::count)> seen{};
    std::array<std::size_t, static_cast<std::size_t>(Field::count)> valueLines{};
    std::array<std::size_t, static_cast<std::size_t>(Field::count)> valueColumns{};
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

            const auto field = fieldForKey(key);
            if (!field) {
                return diagnostic(
                    lineNumber,
                    static_cast<std::size_t>(key.data() - line.data()) + 1,
                    "unknown key '" + std::string(key) + "'");
            }
            const auto fieldIndex = static_cast<std::size_t>(*field);
            if (seen[fieldIndex]) {
                return diagnostic(lineNumber, 1, "duplicate key '" + std::string(key) + "'");
            }
            seen[fieldIndex] = true;
            valueLines[fieldIndex] = lineNumber;
            valueColumns[fieldIndex] = valueColumn;

            switch (*field) {
            case Field::version: {
                std::uint32_t version = 0;
                if (!parseInteger(value, version)) {
                    return diagnostic(lineNumber, valueColumn, "pmat.version must be an integer");
                }
                if (version != currentPmatVersion) {
                    return diagnostic(
                        lineNumber,
                        valueColumn,
                        "unsupported .pmat version " + std::to_string(version));
                }
                break;
            }
            case Field::type:
                if (value != "fbm") {
                    return diagnostic(lineNumber, valueColumn, "material.type must be 'fbm'");
                }
                break;
            case Field::seed:
                if (!parseInteger(value, material.seed)) {
                    return diagnostic(lineNumber, valueColumn, "material.seed must be an unsigned integer");
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
            case Field::count:
                break;
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        offset = newline + 1;
    }

    for (std::size_t index = 0; index < seen.size(); ++index) {
        if (!seen[index]) {
            return diagnostic(lineNumber + 1, 1, "missing required key '" + std::string(fieldKeys[index]) + "'");
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
    if (gain.empty()) {
        return SerialisationError{"could not format noise.gain"};
    }

    std::string output;
    output.reserve(192);
    output += "# Paperweight procedural material\n";
    output += "pmat.version = " + std::to_string(currentPmatVersion) + "\n";
    output += "material.type = fbm\n";
    output += "material.seed = " + std::to_string(material.seed) + "\n";
    output += "noise.frequency = " + std::to_string(material.frequency) + "\n";
    output += "noise.octaves = " + std::to_string(material.octaves) + "\n";
    output += "noise.lacunarity = " + std::to_string(material.lacunarity) + "\n";
    output += "noise.gain = " + gain + "\n";
    return output;
}

} // namespace paperweight
