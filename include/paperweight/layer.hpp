#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include <paperweight/image.hpp>

namespace paperweight {

enum class CompositeMode {
    blend,
    add,
    multiply,
};

enum class QuarterTurn : std::uint8_t {
    none = 0,
    clockwise90 = 1,
    clockwise180 = 2,
    clockwise270 = 3,
};

struct CoordinateTransform {
    std::uint32_t scaleX{1};
    std::uint32_t scaleY{1};
    double offsetX{};
    double offsetY{};
    QuarterTurn rotation{QuarterTurn::none};
    bool warpEnabled{};
    double warpStrength{};
    std::uint32_t warpFrequency{1};
    std::uint64_t warpSeedOffset{};

    friend constexpr bool operator==(
        const CoordinateTransform&,
        const CoordinateTransform&) = default;
};

struct LayerMask {
    bool enabled{};
    bool inverted{};
    std::uint64_t seedOffset{};
    double inputLow{};
    double inputHigh{1.0};

    friend constexpr bool operator==(const LayerMask&, const LayerMask&) = default;
};

struct NoiseOperation {
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(const NoiseOperation&, const NoiseOperation&) = default;
};

struct SolidColourOperation {
    Rgba8 colour{128, 128, 128, 255};

    friend constexpr bool operator==(
        const SolidColourOperation&,
        const SolidColourOperation&) = default;
};

struct LevelsOperation {
    double inputLow{0.0};
    double inputHigh{1.0};
    double gamma{1.0};

    friend constexpr bool operator==(const LevelsOperation&, const LevelsOperation&) = default;
};

struct ThresholdOperation {
    double threshold{0.5};

    friend constexpr bool operator==(
        const ThresholdOperation&,
        const ThresholdOperation&) = default;
};

enum class ProcessingTarget : std::uint8_t {
    colour = 0,
    scalar = 1,
    colourAndScalar = 2,
};

enum class RegionFieldKind : std::uint8_t {
    random = 0,
    localU = 1,
    localV = 2,
    centreDistance = 3,
    boundaryDistance = 4,
    courseRandom = 5,
};

struct RegionFieldOperation {
    RegionFieldKind field{RegionFieldKind::random};
    std::uint64_t seedOffset{};
    std::uint32_t channel{};
    double outputLow{0.0};
    double outputHigh{1.0};
    bool inverted{};
    ProcessingTarget target{ProcessingTarget::colourAndScalar};

    friend constexpr bool operator==(
        const RegionFieldOperation&,
        const RegionFieldOperation&) = default;
};

struct PosteriseOperation {
    std::uint32_t bands{4};
    ProcessingTarget target{ProcessingTarget::colour};

    friend constexpr bool operator==(
        const PosteriseOperation&,
        const PosteriseOperation&) = default;
};

enum class ColourRampMode : std::uint8_t {
    linear = 0,
    stepped = 1,
};

struct ColourRampStop {
    double position{};
    Rgba8 colour{};

    friend constexpr bool operator==(
        const ColourRampStop&,
        const ColourRampStop&) = default;
};

struct ColourRampOperation {
    ColourRampMode mode{ColourRampMode::linear};
    std::vector<ColourRampStop> stops{
        {0.0, {0, 0, 0, 255}},
        {1.0, {255, 255, 255, 255}},
    };

    friend bool operator==(
        const ColourRampOperation&,
        const ColourRampOperation&) = default;
};

struct PaletteOperation {
    std::vector<Rgba8> colours{
        {0, 0, 0, 255},
        {255, 255, 255, 255},
    };

    friend bool operator==(
        const PaletteOperation&,
        const PaletteOperation&) = default;
};

enum class BrickMortarSpace : std::uint8_t {
    cell = 0,
    texture = 1,
};

struct BrickGridOperation {
    std::uint32_t columns{6};
    std::uint32_t rows{8};
    double mortar{0.08};
    double stagger{0.5};
    double softness{0.02};
    BrickMortarSpace mortarSpace{BrickMortarSpace::cell};
    struct PhysicalDimensions {
        double widthMetres{0.24};
        double heightMetres{0.075};
        double mortarMetres{0.01};

        friend constexpr bool operator==(
            const PhysicalDimensions&,
            const PhysicalDimensions&) = default;
    };
    std::optional<PhysicalDimensions> physicalDimensions;

    friend constexpr bool operator==(
        const BrickGridOperation&,
        const BrickGridOperation&) = default;
};

struct TileGridOperation {
    std::uint32_t columns{6};
    std::uint32_t rows{6};
    double grout{0.08};
    double softness{0.02};

    friend constexpr bool operator==(
        const TileGridOperation&,
        const TileGridOperation&) = default;
};

enum class CourseLayoutProfile : std::uint8_t {
    masonry = 0,
    slabs = 1,
    slates = 2,
};

enum class CourseLayoutField : std::uint8_t {
    blocks = 0,
    mortar = 1,
    course = 2,
    overlap = 3,
};

struct CourseLayoutOperation {
    CourseLayoutProfile profile{CourseLayoutProfile::masonry};
    CourseLayoutField field{CourseLayoutField::blocks};
    std::uint32_t blocks{6};
    std::uint32_t courses{8};
    double blockVariation{0.35};
    double courseVariation{0.2};
    double stagger{0.5};
    double crookedness{0.08};
    double gap{0.08};
    double softness{0.02};
    double overlap{0.25};
    std::uint64_t seedOffset{};

    struct PhysicalDimensions {
        double blockWidthMetres{0.32};
        double courseHeightMetres{0.14};
        double gapMetres{0.012};
        double overlapMetres{0.04};

        friend constexpr bool operator==(
            const PhysicalDimensions&,
            const PhysicalDimensions&) = default;
    };
    std::optional<PhysicalDimensions> physicalDimensions;

    friend constexpr bool operator==(
        const CourseLayoutOperation&,
        const CourseLayoutOperation&) = default;
};

struct WorleyCellsOperation {
    std::uint32_t columns{6};
    std::uint32_t rows{6};
    double jitter{0.75};
    double edgeWidth{0.18};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(
        const WorleyCellsOperation&,
        const WorleyCellsOperation&) = default;
};

struct RandomCellsOperation {
    std::uint32_t columns{8};
    std::uint32_t rows{8};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(
        const RandomCellsOperation&,
        const RandomCellsOperation&) = default;
};

enum class LineDirection : std::uint8_t {
    vertical = 0,
    horizontal = 1,
};

struct LinesOperation {
    LineDirection direction{LineDirection::vertical};
    std::uint32_t count{8};
    double width{0.12};
    double softness{0.02};

    friend constexpr bool operator==(
        const LinesOperation&,
        const LinesOperation&) = default;
};

struct RectanglesOperation {
    std::uint32_t columns{4};
    std::uint32_t rows{4};
    double width{0.7};
    double height{0.7};
    double softness{0.02};

    friend constexpr bool operator==(
        const RectanglesOperation&,
        const RectanglesOperation&) = default;
};

struct CirclesOperation {
    std::uint32_t columns{6};
    std::uint32_t rows{6};
    double radius{0.35};
    double softness{0.02};

    friend constexpr bool operator==(
        const CirclesOperation&,
        const CirclesOperation&) = default;
};

enum class SurfacePatternKind : std::uint8_t {
    ridgedNoise = 0,
    bands = 1,
    rings = 2,
    scatter = 3,
    streaks = 4,
};

struct SurfacePatternOperation {
    SurfacePatternKind kind{SurfacePatternKind::ridgedNoise};
    std::uint32_t scale{8};
    double width{0.12};
    double detail{0.5};
    double distortion{0.25};
    double variation{0.5};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(
        const SurfacePatternOperation&,
        const SurfacePatternOperation&) = default;
};

enum class SurfaceFilterKind : std::uint8_t {
    invert = 0,
    soften = 1,
    expand = 2,
    contract = 3,
    edge = 4,
    slope = 5,
    cavity = 6,
    peaks = 7,
    edgeAwareSoften = 8,
};

struct SurfaceFilterOperation {
    SurfaceFilterKind kind{SurfaceFilterKind::edge};
    double radius{0.02};
    double strength{1.0};
    double sensitivity{0.2};
    ProcessingTarget target{ProcessingTarget::colourAndScalar};

    friend constexpr bool operator==(
        const SurfaceFilterOperation&,
        const SurfaceFilterOperation&) = default;
};

struct InkContourOperation {
    Rgba8 colour{24, 24, 28, 255};
    double radius{0.01};
    double threshold{0.12};
    double softness{0.05};
    double strength{1.0};
    bool inverted{};

    friend constexpr bool operator==(
        const InkContourOperation&,
        const InkContourOperation&) = default;
};

using LayerOperation = std::variant<
    NoiseOperation,
    SolidColourOperation,
    LevelsOperation,
    ThresholdOperation,
    BrickGridOperation,
    TileGridOperation,
    WorleyCellsOperation,
    RandomCellsOperation,
    LinesOperation,
    RectanglesOperation,
    CirclesOperation,
    SurfacePatternOperation,
    SurfaceFilterOperation,
    PosteriseOperation,
    ColourRampOperation,
    PaletteOperation,
    InkContourOperation,
    RegionFieldOperation,
    CourseLayoutOperation>;

struct MaterialLayer {
    bool enabled{true};
    double opacity{1.0};
    CompositeMode compositeMode{CompositeMode::blend};
    LayerOperation operation{NoiseOperation{}};
    CoordinateTransform transform;
    LayerMask mask;

    friend constexpr bool operator==(const MaterialLayer&, const MaterialLayer&) = default;
};

struct LayerLimits {
    static constexpr std::size_t maximumLayers = 32;
    static constexpr double minimumOpacity = 0.0;
    static constexpr double maximumOpacity = 1.0;
    static constexpr double minimumLevel = 0.0;
    static constexpr double maximumLevel = 1.0;
    static constexpr double minimumGamma = 0.1;
    static constexpr double maximumGamma = 4.0;
    static constexpr double minimumThreshold = 0.0;
    static constexpr double maximumThreshold = 1.0;
    static constexpr std::uint32_t minimumScale = 1;
    static constexpr std::uint32_t maximumScale = 16;
    static constexpr double maximumOffsetMagnitude = 1024.0;
    static constexpr double minimumWarpStrength = 0.0;
    static constexpr double maximumWarpStrength = 1.0;
    static constexpr std::uint32_t minimumWarpFrequency = 1;
    static constexpr std::uint32_t maximumWarpFrequency = 16;
    static constexpr std::uint32_t minimumPatternCount = 1;
    static constexpr std::uint32_t maximumPatternCount = 64;
    static constexpr double minimumGap = 0.0;
    static constexpr double maximumGap = 0.95;
    static constexpr double minimumStagger = 0.0;
    static constexpr double maximumStagger = 1.0;
    static constexpr double minimumSoftness = 0.0;
    static constexpr double maximumSoftness = 0.25;
    static constexpr double minimumJitter = 0.0;
    static constexpr double maximumJitter = 1.0;
    static constexpr double minimumCellEdgeWidth = 0.01;
    static constexpr double maximumCellEdgeWidth = 2.0;
    static constexpr double minimumShapeSize = 0.0;
    static constexpr double maximumShapeSize = 1.0;
    static constexpr double minimumCircleRadius = 0.0;
    static constexpr double maximumCircleRadius = 0.5;
    static constexpr double minimumSurfaceWidth = 0.001;
    static constexpr double maximumSurfaceWidth = 1.0;
    static constexpr double minimumSurfaceControl = 0.0;
    static constexpr double maximumSurfaceControl = 1.0;
    static constexpr double minimumFilterRadius = 0.0;
    static constexpr double maximumFilterRadius = 0.25;
    static constexpr std::uint32_t minimumPosteriseBands = 2;
    static constexpr std::uint32_t maximumPosteriseBands = 16;
    static constexpr std::size_t minimumColourStops = 2;
    static constexpr std::size_t maximumColourStops = 8;
    static constexpr double minimumFilterSensitivity = 0.0;
    static constexpr double maximumFilterSensitivity = 1.0;
    static constexpr double minimumContourSoftness = 0.0;
    static constexpr double maximumContourSoftness = 0.5;
    static constexpr std::uint32_t maximumRegionChannel = 255;
    static constexpr double minimumLayoutVariation = 0.0;
    static constexpr double maximumLayoutVariation = 1.0;
    static constexpr double minimumLayoutOverlap = 0.0;
    static constexpr double maximumLayoutOverlap = 0.95;
};

[[nodiscard]] constexpr MaterialLayer makeNoiseLayer(std::uint64_t seedOffset = 0)
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        NoiseOperation{seedOffset},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeSolidColourLayer(
    Rgba8 colour = {128, 128, 128, 255})
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        SolidColourOperation{colour},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeLevelsLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        LevelsOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeThresholdLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        ThresholdOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeBrickGridLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        BrickGridOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeTileGridLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        TileGridOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeWorleyCellsLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        WorleyCellsOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeRandomCellsLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        RandomCellsOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeLinesLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        LinesOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeRectanglesLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        RectanglesOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeCirclesLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        CirclesOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeSurfacePatternLayer(
    SurfacePatternKind kind = SurfacePatternKind::ridgedNoise)
{
    auto operation = SurfacePatternOperation{};
    operation.kind = kind;
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        operation,
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeSurfaceFilterLayer(
    SurfaceFilterKind kind = SurfaceFilterKind::edge)
{
    auto operation = SurfaceFilterOperation{};
    operation.kind = kind;
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        operation,
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makePosteriseLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        PosteriseOperation{},
        {},
        {}};
}

[[nodiscard]] inline MaterialLayer makeColourRampLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        ColourRampOperation{},
        {},
        {}};
}

[[nodiscard]] inline MaterialLayer makePaletteLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        PaletteOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeInkContourLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        InkContourOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeRegionFieldLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        RegionFieldOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeCourseLayoutLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        CourseLayoutOperation{},
        {},
        {}};
}

[[nodiscard]] constexpr std::uint32_t rotationDegrees(QuarterTurn rotation)
{
    return static_cast<std::uint32_t>(rotation) * 90U;
}

[[nodiscard]] constexpr std::string_view compositeModeName(CompositeMode mode)
{
    switch (mode) {
    case CompositeMode::blend:
        return "blend";
    case CompositeMode::add:
        return "add";
    case CompositeMode::multiply:
        return "multiply";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view operationName(const LayerOperation& operation)
{
    switch (operation.index()) {
    case 0:
        return "noise";
    case 1:
        return "solid_colour";
    case 2:
        return "levels";
    case 3:
        return "threshold";
    case 4:
        return "brick_grid";
    case 5:
        return "tile_grid";
    case 6:
        return "worley_cells";
    case 7:
        return "random_cells";
    case 8:
        return "lines";
    case 9:
        return "rectangles";
    case 10:
        return "circles";
    case 11:
        return "surface_pattern";
    case 12:
        return "surface_filter";
    case 13:
        return "posterise";
    case 14:
        return "colour_ramp";
    case 15:
        return "palette";
    case 16:
        return "ink_contour";
    case 17:
        return "region_field";
    case 18:
        return "course_layout";
    default:
        return "unknown";
    }
}

[[nodiscard]] constexpr std::string_view regionFieldKindName(RegionFieldKind field)
{
    switch (field) {
    case RegionFieldKind::random:
        return "random";
    case RegionFieldKind::localU:
        return "local_u";
    case RegionFieldKind::localV:
        return "local_v";
    case RegionFieldKind::centreDistance:
        return "centre_distance";
    case RegionFieldKind::boundaryDistance:
        return "boundary_distance";
    case RegionFieldKind::courseRandom:
        return "course_random";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view courseLayoutProfileName(
    CourseLayoutProfile profile)
{
    switch (profile) {
    case CourseLayoutProfile::masonry:
        return "masonry";
    case CourseLayoutProfile::slabs:
        return "slabs";
    case CourseLayoutProfile::slates:
        return "slates";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view courseLayoutFieldName(
    CourseLayoutField field)
{
    switch (field) {
    case CourseLayoutField::blocks:
        return "blocks";
    case CourseLayoutField::mortar:
        return "mortar";
    case CourseLayoutField::course:
        return "course";
    case CourseLayoutField::overlap:
        return "overlap";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view surfacePatternKindName(
    SurfacePatternKind kind)
{
    switch (kind) {
    case SurfacePatternKind::ridgedNoise:
        return "ridged_noise";
    case SurfacePatternKind::bands:
        return "bands";
    case SurfacePatternKind::rings:
        return "rings";
    case SurfacePatternKind::scatter:
        return "scatter";
    case SurfacePatternKind::streaks:
        return "streaks";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view surfaceFilterKindName(
    SurfaceFilterKind kind)
{
    switch (kind) {
    case SurfaceFilterKind::invert:
        return "invert";
    case SurfaceFilterKind::soften:
        return "soften";
    case SurfaceFilterKind::expand:
        return "expand";
    case SurfaceFilterKind::contract:
        return "contract";
    case SurfaceFilterKind::edge:
        return "edge";
    case SurfaceFilterKind::slope:
        return "slope";
    case SurfaceFilterKind::cavity:
        return "cavity";
    case SurfaceFilterKind::peaks:
        return "peaks";
    case SurfaceFilterKind::edgeAwareSoften:
        return "edge_aware_soften";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view processingTargetName(
    ProcessingTarget target)
{
    switch (target) {
    case ProcessingTarget::colour:
        return "colour";
    case ProcessingTarget::scalar:
        return "scalar";
    case ProcessingTarget::colourAndScalar:
        return "all";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view colourRampModeName(ColourRampMode mode)
{
    switch (mode) {
    case ColourRampMode::linear:
        return "linear";
    case ColourRampMode::stepped:
        return "stepped";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view lineDirectionName(LineDirection direction)
{
    switch (direction) {
    case LineDirection::vertical:
        return "vertical";
    case LineDirection::horizontal:
        return "horizontal";
    }
    return "unknown";
}

} // namespace paperweight
