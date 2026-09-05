#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <paperweight/image.hpp>
#include <paperweight/output.hpp>

namespace paperweight {

enum class CompositeMode {
    blend,
    add,
    multiply,
    minimum,
    maximum,
    detail,
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

struct SurfaceValueOperation {
    double value{0.5};

    friend constexpr bool operator==(
        const SurfaceValueOperation&,
        const SurfaceValueOperation&) = default;
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

enum class RegionSurfaceField : std::uint8_t {
    height = 0,
    cavity = 1,
    outerEdge = 2,
    exposedFace = 3,
    facet = 4,
    wear = 5,
};

enum class BevelProfile : std::uint8_t {
    rounded = 0,
    chamfered = 1,
    handCut = 2,
};

struct RegionSurfaceOperation {
    RegionSurfaceField field{RegionSurfaceField::height};
    BevelProfile profile{BevelProfile::rounded};
    double bevelWidth{0.35};
    double bevelHeight{0.8};
    std::uint32_t facetCount{5};
    double facetStrength{0.25};
    double centrePeak{0.15};
    double slopeStrength{0.08};
    double chipAmount{0.08};
    std::uint32_t chipScale{8};
    double wearAmount{0.12};
    double erosionAmount{0.06};
    std::uint64_t seedOffset{};
    bool facetedNormals{};
    ProcessingTarget target{ProcessingTarget::scalar};

    friend constexpr bool operator==(
        const RegionSurfaceOperation&,
        const RegionSurfaceOperation&) = default;
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

enum class ShapePrimitiveKind : std::uint8_t {
    roundedRectangle = 0,
    ellipse = 1,
    capsule = 2,
    diamond = 3,
    convexPolygon = 4,
    annulus = 5,
    arc = 6,
    sector = 7,
    crescent = 8,
};

enum class ShapeFieldKind : std::uint8_t {
    fill = 0,
    inset = 1,
    outline = 2,
    border = 3,
};

struct ShapePoint {
    double x{};
    double y{};

    friend constexpr bool operator==(const ShapePoint&, const ShapePoint&) = default;
};

enum class RadialOrientation : std::uint8_t {
    fixed = 0,
    outward = 1,
    tangent = 2,
};

struct ShapePrimitiveOperation {
    ShapePrimitiveKind kind{ShapePrimitiveKind::roundedRectangle};
    ShapeFieldKind field{ShapeFieldKind::fill};
    std::uint32_t columns{4};
    std::uint32_t rows{4};
    double width{0.7};
    double height{0.7};
    double cornerRadius{0.12};
    double inset{0.08};
    double borderWidth{0.08};
    double softness{0.02};
    double offsetX{};
    double offsetY{};
    double stagger{};
    double rotationDegrees{};
    std::uint64_t seedOffset{};
    std::vector<ShapePoint> vertices{
        {-0.45, -0.25},
        {0.0, -0.48},
        {0.45, -0.25},
        {0.42, 0.28},
        {0.0, 0.48},
        {-0.42, 0.28},
    };
    double innerRadius{0.2};
    double arcStartDegrees{};
    double arcSweepDegrees{360.0};
    double crescentOffset{0.12};
    std::uint32_t radialCopies{1};
    double radialRadius{};
    double radialPhaseDegrees{};
    RadialOrientation radialOrientation{RadialOrientation::fixed};

    friend bool operator==(
        const ShapePrimitiveOperation&,
        const ShapePrimitiveOperation&) = default;
};

enum class ShapeBooleanMode : std::uint8_t {
    unionMask = 0,
    intersection = 1,
    subtraction = 2,
};

struct ShapeBooleanOperation {
    ShapeBooleanMode mode{ShapeBooleanMode::unionMask};
    ShapePrimitiveOperation shape;
    ProcessingTarget target{ProcessingTarget::colourAndScalar};

    friend bool operator==(
        const ShapeBooleanOperation&,
        const ShapeBooleanOperation&) = default;
};

enum class LatticeKind : std::uint8_t {
    lines = 0,
    diamonds = 1,
};

struct LatticeOperation {
    LatticeKind kind{LatticeKind::diamonds};
    std::int32_t windingX{4};
    std::int32_t windingY{4};
    double width{0.08};
    double softness{0.02};
    double phase{};

    friend constexpr bool operator==(
        const LatticeOperation&,
        const LatticeOperation&) = default;
};

enum class ScatterOverlapMode : std::uint8_t {
    forbidden = 0,
    controlled = 1,
    unrestricted = 2,
};

enum class ScatterField : std::uint8_t {
    material = 0,
    fill = 1,
    instanceRandom = 2,
    localU = 3,
    localV = 4,
    boundaryDistance = 5,
};

struct ScatterMask {
    bool enabled{};
    bool inverted{};
    std::uint32_t frequency{4};
    double inputLow{0.35};
    double inputHigh{0.65};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(const ScatterMask&, const ScatterMask&) = default;
};

struct ScatterPopulation {
    double weight{1.0};
    double minimumScale{0.7};
    double maximumScale{1.3};
    double minimumAspect{0.75};
    double maximumAspect{1.35};
    double minimumRotation{-180.0};
    double maximumRotation{180.0};
    Rgba8 lowColour{92, 96, 92, 255};
    Rgba8 highColour{164, 166, 154, 255};
    double minimumHeight{0.45};
    double maximumHeight{0.9};
    double minimumRoughness{0.55};
    double maximumRoughness{0.95};

    friend constexpr bool operator==(
        const ScatterPopulation&,
        const ScatterPopulation&) = default;
};

struct ScatterOperation {
    ScatterField field{ScatterField::material};
    std::uint32_t columns{16};
    std::uint32_t rows{16};
    double density{0.72};
    double jitter{1.0};
    double minimumDistance{0.018};
    ScatterOverlapMode overlapMode{ScatterOverlapMode::controlled};
    double maximumOverlap{0.35};
    std::uint64_t seedOffset{};
    ShapePrimitiveOperation stamp{
        ShapePrimitiveKind::ellipse,
        ShapeFieldKind::fill,
        1,
        1,
        0.075,
        0.075,
        0.0,
        0.0,
        0.01,
        0.006,
        0.0,
        0.0,
        0.0,
        0.0,
        0,
        {
            {-0.45, -0.25},
            {0.0, -0.48},
            {0.45, -0.25},
            {0.42, 0.28},
            {0.0, 0.48},
            {-0.42, 0.28},
        },
    };
    std::vector<ScatterPopulation> populations{ScatterPopulation{}};
    ScatterMask densityMask;
    ScatterMask exclusionMask{false, true, 4, 0.45, 0.65, 1};

    friend bool operator==(const ScatterOperation&, const ScatterOperation&) = default;
};

enum class OrganicCellField : std::uint8_t {
    plates = 0,
    boundaries = 1,
    cellRandom = 2,
};

enum class OrganicDirection : std::uint8_t {
    vertical = 0,
    horizontal = 1,
};

struct OrganicCellOperation {
    OrganicCellField field{OrganicCellField::plates};
    OrganicDirection direction{OrganicDirection::vertical};
    std::uint32_t columns{10};
    std::uint32_t rows{4};
    double anisotropy{2.5};
    double jitter{0.72};
    double irregularity{0.28};
    double gap{0.12};
    double softness{0.025};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(
        const OrganicCellOperation&,
        const OrganicCellOperation&) = default;
};

enum class OrganicCrackField : std::uint8_t {
    cracks = 0,
    trunks = 1,
    branches = 2,
    hierarchy = 3,
    distance = 4,
};

struct OrganicCrackOperation {
    OrganicCrackField field{OrganicCrackField::cracks};
    OrganicDirection direction{OrganicDirection::vertical};
    std::uint32_t roots{5};
    std::uint32_t segments{7};
    std::uint32_t branchLevels{3};
    double branchProbability{0.58};
    double bend{0.34};
    double width{0.018};
    double taper{0.62};
    double softness{0.006};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(
        const OrganicCrackOperation&,
        const OrganicCrackOperation&) = default;
};

enum class LeafProfile : std::uint8_t {
    ovate = 0,
    lanceolate = 1,
    cordate = 2,
    lobed = 3,
    blob = 4,
    rosette = 5,
    lichen = 6,
};

enum class LeafClusterPattern : std::uint8_t {
    radial = 0,
    fan = 1,
    vine = 2,
    canopy = 3,
    groundScatter = 4,
};

enum class LeafField : std::uint8_t {
    material = 0,
    fill = 1,
    edge = 2,
    midrib = 3,
    veins = 4,
    instanceRandom = 5,
    outline = 6,
    innerHighlight = 7,
    clusterRandom = 8,
    population = 9,
};

enum class LeafSpecies : std::uint8_t {
    ivy = 0,
    laurel = 1,
    oak = 2,
    ash = 3,
};

struct LeafClusterOperation {
    LeafField field{LeafField::material};
    LeafProfile profile{LeafProfile::ovate};
    LeafClusterPattern pattern{LeafClusterPattern::canopy};
    std::uint32_t columns{5};
    std::uint32_t rows{5};
    std::uint32_t leavesPerCluster{7};
    double density{0.82};
    double clusterSpread{0.075};
    double leafLength{0.12};
    double leafWidth{0.065};
    double scaleVariation{0.28};
    double rotationVariation{55.0};
    double directionDegrees{-18.0};
    double taper{0.72};
    double baseNotch{0.12};
    double curvature{0.14};
    double serration{0.08};
    std::uint32_t serrationCount{7};
    double lobing{0.25};
    std::uint32_t lobeCount{4};
    double midribWidth{0.055};
    std::uint32_t veinPairs{4};
    double veinWidth{0.032};
    double edgeWidth{0.075};
    double softness{0.006};
    Rgba8 lowColour{42, 76, 35, 255};
    Rgba8 highColour{104, 142, 66, 255};
    double minimumHeight{0.42};
    double maximumHeight{0.92};
    double minimumRoughness{0.62};
    double maximumRoughness{0.9};
    std::uint64_t seedOffset{};
    double innerHighlightWidth{0.06};
    double innerHighlightInset{0.08};
    double clusterColourVariation{0.0};
    double instanceColourVariation{1.0};
    LeafProfile secondaryProfile{LeafProfile::blob};
    double secondaryWeight{0.0};
    double secondaryScale{0.72};
    Rgba8 secondaryLowColour{48, 82, 38, 255};
    Rgba8 secondaryHighColour{118, 150, 72, 255};
    LeafProfile tertiaryProfile{LeafProfile::lichen};
    double tertiaryWeight{0.0};
    double tertiaryScale{0.45};
    Rgba8 tertiaryLowColour{70, 91, 48, 255};
    Rgba8 tertiaryHighColour{157, 169, 98, 255};

    friend constexpr bool operator==(
        const LeafClusterOperation&,
        const LeafClusterOperation&) = default;
};

enum class OrganicAccumulationKind : std::uint8_t {
    moss = 0,
    lichen = 1,
    colourVariation = 2,
};

enum class OrganicAccumulationSource : std::uint8_t {
    cavity = 0,
    boundary = 1,
    lowHeight = 2,
    authoredMask = 3,
};

enum class OrganicAccumulationProfile : std::uint8_t {
    noise = 0,
    colonies = 1,
    speckles = 2,
};

enum class OrganicAccumulationField : std::uint8_t {
    material = 0,
    fill = 1,
    outline = 2,
    innerHighlight = 3,
    detail = 4,
};

struct OrganicAccumulationOperation {
    OrganicAccumulationKind kind{OrganicAccumulationKind::moss};
    OrganicAccumulationSource source{OrganicAccumulationSource::cavity};
    std::uint32_t scale{7};
    double coverage{0.55};
    double softness{0.14};
    double moistureBias{0.6};
    double breakup{0.45};
    double variation{0.25};
    Rgba8 lowColour{35, 61, 28, 255};
    Rgba8 highColour{95, 118, 54, 255};
    std::uint64_t seedOffset{};
    ProcessingTarget target{ProcessingTarget::colour};
    OrganicAccumulationProfile profile{OrganicAccumulationProfile::noise};
    OrganicAccumulationField field{OrganicAccumulationField::material};
    double outlineWidth{0.08};
    double innerHighlightWidth{0.1};
    double innerHighlightInset{0.08};

    friend constexpr bool operator==(
        const OrganicAccumulationOperation&,
        const OrganicAccumulationOperation&) = default;
};

enum class TextilePattern : std::uint8_t {
    plainWeave = 0,
    basketWeave = 1,
    twillWeave = 2,
    loopPile = 3,
    cutPile = 4,
};

enum class TextileField : std::uint8_t {
    material = 0,
    height = 1,
    warp = 2,
    weft = 3,
    overUnder = 4,
    fibres = 5,
    pile = 6,
    damage = 7,
    colourVariation = 8,
    direction = 9,
};

enum class YarnProfile : std::uint8_t {
    round = 0,
    flat = 1,
    twisted = 2,
};

enum class TextileTileOrientation : std::uint8_t {
    uniform = 0,
    alternatingRows = 1,
    alternatingColumns = 2,
    checkerboard = 3,
};

struct TextileOperation {
    TextilePattern pattern{TextilePattern::plainWeave};
    TextileField field{TextileField::material};
    YarnProfile yarnProfile{YarnProfile::round};
    TextileTileOrientation tileOrientation{TextileTileOrientation::uniform};
    std::uint32_t columns{32};
    std::uint32_t rows{32};
    std::uint32_t tileColumns{1};
    std::uint32_t tileRows{1};
    std::uint32_t weaveSpan{1};
    std::uint32_t twillStep{1};
    double yarnWidth{0.82};
    double yarnRoundness{0.82};
    double crossingHeight{0.18};
    double jitter{0.08};
    std::uint32_t fibreFrequency{12};
    double fibreStrength{0.12};
    double twist{0.3};
    double pileRadius{0.34};
    double pileHeight{0.78};
    double missingAmount{};
    double damageAmount{};
    double differentColourAmount{};
    double colourVariation{0.12};
    double softness{0.025};
    Rgba8 lowColour{54, 76, 70, 255};
    Rgba8 highColour{102, 132, 112, 255};
    Rgba8 accentColour{184, 168, 116, 255};
    Rgba8 damageColour{42, 38, 34, 255};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(
        const TextileOperation&,
        const TextileOperation&) = default;
};

enum class RegionAttachmentKind : std::uint8_t {
    fastener = 0,
    inlay = 1,
    glyph = 2,
    chip = 3,
    crack = 4,
    damage = 5,
};

enum class RegionAnchor : std::uint8_t {
    centre = 0,
    edge = 1,
    corner = 2,
    cavity = 3,
};

enum class RegionAttachmentField : std::uint8_t {
    material = 0,
    mask = 1,
    distance = 2,
};

enum class RegionGlyph : std::uint8_t {
    cross = 0,
    chevron = 1,
    triangle = 2,
    rune = 3,
};

struct RegionAttachmentOperation {
    RegionAttachmentKind kind{RegionAttachmentKind::fastener};
    RegionAttachmentField field{RegionAttachmentField::material};
    RegionAnchor startAnchor{RegionAnchor::centre};
    RegionAnchor endAnchor{RegionAnchor::edge};
    RegionGlyph glyph{RegionGlyph::cross};
    std::uint32_t count{1};
    double size{0.18};
    double aspect{1.0};
    double inset{0.12};
    double rotationDegrees{};
    double jitter{0.08};
    double selection{1.0};
    double lineWidth{0.045};
    double length{0.65};
    double branching{0.35};
    double softness{0.015};
    Rgba8 colour{92, 96, 102, 255};
    double height{0.82};
    double roughness{0.38};
    double metalness{0.75};
    double occlusion{0.55};
    double emissive{};
    std::uint64_t seedOffset{};

    friend constexpr bool operator==(
        const RegionAttachmentOperation&,
        const RegionAttachmentOperation&) = default;
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

enum class RegionalDetailField : std::uint8_t {
    material = 0,
    macro = 1,
    meso = 2,
    micro = 3,
    centreGradient = 4,
    directionalGradient = 5,
    planarGradient = 6,
    mottling = 7,
    grain = 8,
    directionalStrokes = 9,
    outerShadow = 10,
    bevel = 11,
    body = 12,
    innerHighlight = 13,
    wear = 14,
    combined = 15,
    palette = 16,
};

enum class RegionalDetailOrientation : std::uint8_t {
    texture = 0,
    region = 1,
};

enum class RegionalVariationScope : std::uint8_t {
    material = 0,
    group = 1,
    parentRegion = 2,
    region = 3,
};

enum class RegionalWearBias : std::uint8_t {
    exposedEdges = 0,
    cavities = 1,
    upwardFaces = 2,
    localPatches = 3,
    mixed = 4,
};

// A reusable authored-surface processor. All spatial dimensions are expressed
// in metres and converted through Material::physicalSize and RegionSample
// extents. A field layer exposes one mask; material mode uses the exact same
// stable signals to coordinate colour and physical channels.
struct RegionalDetailOperation {
    RegionalDetailField field{RegionalDetailField::material};
    RegionalDetailOrientation orientation{RegionalDetailOrientation::region};
    RegionalVariationScope variationScope{RegionalVariationScope::region};
    RegionalWearBias wearBias{RegionalWearBias::mixed};
    double macroScaleMetres{0.24};
    double mesoScaleMetres{0.055};
    double microScaleMetres{0.008};
    double macroStrength{0.22};
    double mesoStrength{0.16};
    double microStrength{0.08};
    double gradientStrength{0.12};
    double gradientAngleDegrees{315.0};
    double mottlingStrength{0.18};
    double grainStrength{0.1};
    double strokeStrength{0.08};
    double outerBandMetres{0.004};
    double bevelBandMetres{0.009};
    double innerBandMetres{0.003};
    double edgeIrregularity{0.3};
    double edgeBreakup{0.12};
    double edgeTaper{0.2};
    double wearAmount{0.18};
    double wearScaleMetres{0.04};
    Rgba8 paletteLow{62, 68, 72, 255};
    Rgba8 paletteHigh{156, 160, 154, 255};
    std::uint32_t paletteSteps{};
    double colourAmount{0.72};
    double heightAmount{0.08};
    double roughnessAmount{0.14};
    double coatingWear{0.55};
    double occlusionAmount{0.22};
    std::uint64_t seedOffset{};
    ProcessingTarget target{ProcessingTarget::colourAndScalar};

    friend constexpr bool operator==(
        const RegionalDetailOperation&,
        const RegionalDetailOperation&) = default;
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
    CourseLayoutOperation,
    RegionSurfaceOperation,
    ShapePrimitiveOperation,
    ShapeBooleanOperation,
    LatticeOperation,
    ScatterOperation,
    OrganicCellOperation,
    OrganicCrackOperation,
    LeafClusterOperation,
    OrganicAccumulationOperation,
    SurfaceValueOperation,
    TextileOperation,
    RegionAttachmentOperation,
    RegionalDetailOperation>;

struct LayerOutputRouting {
    bool colour{true};
    bool height{true};
    bool roughness{true};
    bool metalness{true};
    bool coating{true};
    bool occlusion{true};
    bool clearCoat{true};
    bool clearCoatRoughness{true};
    bool emissive{true};

    [[nodiscard]] constexpr bool includes(MaterialOutput output) const
    {
        switch (output) {
        case MaterialOutput::colour:
            return colour;
        case MaterialOutput::height:
        case MaterialOutput::normal:
            return height;
        case MaterialOutput::roughness:
            return roughness;
        case MaterialOutput::metalness:
            return metalness;
        case MaterialOutput::coating:
            return coating;
        case MaterialOutput::occlusion:
            return occlusion;
        case MaterialOutput::clearCoat:
            return clearCoat;
        case MaterialOutput::clearCoatRoughness:
            return clearCoatRoughness;
        case MaterialOutput::emissive:
            return emissive;
        }
        return false;
    }

    [[nodiscard]] constexpr bool isLegacyAll() const
    {
        return colour && height && roughness && metalness && coating && occlusion &&
            clearCoat && clearCoatRoughness && emissive;
    }

    friend constexpr bool operator==(
        const LayerOutputRouting&,
        const LayerOutputRouting&) = default;
};

struct MaterialLayer {
    bool enabled{true};
    double opacity{1.0};
    CompositeMode compositeMode{CompositeMode::blend};
    LayerOperation operation{NoiseOperation{}};
    CoordinateTransform transform;
    LayerMask mask;
    LayerOutputRouting outputs;

    friend constexpr bool operator==(const MaterialLayer&, const MaterialLayer&) = default;
};

// Groups are compositing scopes, not a second evaluation model. Their direct
// and nested children remain in Material::layers in evaluation order and refer
// to their immediate parent by identity.
struct MaterialLayerGroup {
    std::string identity;
    std::string parentGroupIdentity;
    std::string name{"Group"};
    bool enabled{true};
    double opacity{1.0};
    CompositeMode compositeMode{CompositeMode::blend};
    CoordinateTransform transform;
    LayerMask mask;
    LayerOutputRouting outputs;

    friend bool operator==(
        const MaterialLayerGroup&,
        const MaterialLayerGroup&) = default;
};

struct MaterialLayerHierarchy {
    std::string identity;
    std::string parentGroupIdentity;

    friend bool operator==(
        const MaterialLayerHierarchy&,
        const MaterialLayerHierarchy&) = default;
};

struct LayerLimits {
    static constexpr std::size_t maximumLayers = 32;
    static constexpr std::size_t maximumGroups = 32;
    static constexpr std::size_t maximumGroupDepth = 8;
    static constexpr std::size_t maximumIdentityLength = 96;
    static constexpr std::size_t maximumGroupNameLength = 128;
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
    static constexpr double minimumBevelWidth = 0.001;
    static constexpr double maximumBevelWidth = 1.0;
    static constexpr std::uint32_t minimumFacetCount = 3;
    static constexpr std::uint32_t maximumFacetCount = 16;
    static constexpr std::uint32_t minimumChipScale = 1;
    static constexpr std::uint32_t maximumChipScale = 64;
    static constexpr std::size_t minimumPolygonVertices = 3;
    static constexpr std::size_t maximumPolygonVertices = 12;
    static constexpr double minimumShapeDimension = 0.001;
    static constexpr double maximumShapeDimension = 1.0;
    static constexpr double maximumShapeOffset = 0.5;
    static constexpr double maximumShapeRotation = 360.0;
    static constexpr double maximumShapeInnerRadius = 0.5;
    static constexpr double minimumArcSweep = 0.1;
    static constexpr std::uint32_t maximumRadialCopies = 32;
    static constexpr std::int32_t maximumLatticeWinding = 64;
    static constexpr std::size_t maximumScatterPopulations = 4;
    static constexpr double minimumScatterScale = 0.1;
    static constexpr double maximumScatterScale = 4.0;
    static constexpr double minimumScatterAspect = 0.25;
    static constexpr double maximumScatterAspect = 4.0;
    static constexpr double maximumScatterDistance = 0.5;
    static constexpr double minimumOrganicAnisotropy = 1.0;
    static constexpr double maximumOrganicAnisotropy = 8.0;
    static constexpr std::uint32_t maximumCrackRoots = 16;
    static constexpr std::uint32_t maximumCrackSegments = 16;
    static constexpr std::uint32_t maximumCrackBranchLevels = 5;
    static constexpr std::uint32_t maximumLeavesPerCluster = 24;
    static constexpr std::uint32_t maximumLeafDetails = 16;
    static constexpr double maximumLeafExtent = 0.5;
    static constexpr std::uint32_t maximumTextileThreads = 128;
    static constexpr std::uint32_t maximumTextileTiles = 16;
    static constexpr std::uint32_t maximumWeaveSpan = 8;
    static constexpr std::uint32_t maximumFibreFrequency = 64;
    static constexpr std::uint32_t maximumRegionAttachments = 8;
    static constexpr double minimumPhysicalDetailScale = 0.000001;
    static constexpr double maximumPhysicalDetailScale = 1000000.0;
    static constexpr std::uint32_t maximumRegionalPaletteSteps = 16;
};

[[nodiscard]] constexpr MaterialLayer makeNoiseLayer(std::uint64_t seedOffset = 0)
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        NoiseOperation{seedOffset},
        {},
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
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeRegionSurfaceLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        RegionSurfaceOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] inline MaterialLayer makeShapePrimitiveLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        ShapePrimitiveOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] inline MaterialLayer makeShapeBooleanLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        ShapeBooleanOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeLatticeLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        LatticeOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] inline MaterialLayer makeScatterLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        ScatterOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeOrganicCellLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        OrganicCellOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeOrganicCrackLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        OrganicCrackOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeLeafClusterLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        LeafClusterOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeOrganicAccumulationLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        OrganicAccumulationOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeSurfaceValueLayer(double value = 0.5)
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        SurfaceValueOperation{value},
        {},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeTextileLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        TextileOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeRegionAttachmentLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        RegionAttachmentOperation{},
        {},
        {},
        {}};
}

[[nodiscard]] constexpr MaterialLayer makeRegionalDetailLayer()
{
    return MaterialLayer{
        true,
        1.0,
        CompositeMode::blend,
        RegionalDetailOperation{},
        {},
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
    case CompositeMode::minimum:
        return "minimum";
    case CompositeMode::maximum:
        return "maximum";
    case CompositeMode::detail:
        return "detail";
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
    case 19:
        return "region_surface";
    case 20:
        return "shape";
    case 21:
        return "shape_boolean";
    case 22:
        return "lattice";
    case 23:
        return "scatter";
    case 24:
        return "organic_cells";
    case 25:
        return "organic_cracks";
    case 26:
        return "leaf_cluster";
    case 27:
        return "organic_accumulation";
    case 28:
        return "surface_value";
    case 29:
        return "textile";
    case 30:
        return "region_attachment";
    case 31:
        return "regional_detail";
    default:
        return "unknown";
    }
}

[[nodiscard]] constexpr std::string_view regionalDetailFieldName(
    RegionalDetailField field)
{
    switch (field) {
    case RegionalDetailField::material: return "material";
    case RegionalDetailField::macro: return "macro";
    case RegionalDetailField::meso: return "meso";
    case RegionalDetailField::micro: return "micro";
    case RegionalDetailField::centreGradient: return "centre_gradient";
    case RegionalDetailField::directionalGradient: return "directional_gradient";
    case RegionalDetailField::planarGradient: return "planar_gradient";
    case RegionalDetailField::mottling: return "mottling";
    case RegionalDetailField::grain: return "grain";
    case RegionalDetailField::directionalStrokes: return "directional_strokes";
    case RegionalDetailField::outerShadow: return "outer_shadow";
    case RegionalDetailField::bevel: return "bevel";
    case RegionalDetailField::body: return "body";
    case RegionalDetailField::innerHighlight: return "inner_highlight";
    case RegionalDetailField::wear: return "wear";
    case RegionalDetailField::combined: return "combined";
    case RegionalDetailField::palette: return "palette";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view regionalDetailOrientationName(
    RegionalDetailOrientation orientation)
{
    switch (orientation) {
    case RegionalDetailOrientation::texture: return "texture";
    case RegionalDetailOrientation::region: return "region";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view regionalVariationScopeName(
    RegionalVariationScope scope)
{
    switch (scope) {
    case RegionalVariationScope::material: return "material";
    case RegionalVariationScope::group: return "group";
    case RegionalVariationScope::parentRegion: return "parent_region";
    case RegionalVariationScope::region: return "region";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view regionalWearBiasName(
    RegionalWearBias bias)
{
    switch (bias) {
    case RegionalWearBias::exposedEdges: return "exposed_edges";
    case RegionalWearBias::cavities: return "cavities";
    case RegionalWearBias::upwardFaces: return "upward_faces";
    case RegionalWearBias::localPatches: return "local_patches";
    case RegionalWearBias::mixed: return "mixed";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view regionAttachmentKindName(
    RegionAttachmentKind kind)
{
    switch (kind) {
    case RegionAttachmentKind::fastener: return "fastener";
    case RegionAttachmentKind::inlay: return "inlay";
    case RegionAttachmentKind::glyph: return "glyph";
    case RegionAttachmentKind::chip: return "chip";
    case RegionAttachmentKind::crack: return "crack";
    case RegionAttachmentKind::damage: return "damage";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view regionAnchorName(RegionAnchor anchor)
{
    switch (anchor) {
    case RegionAnchor::centre: return "centre";
    case RegionAnchor::edge: return "edge";
    case RegionAnchor::corner: return "corner";
    case RegionAnchor::cavity: return "cavity";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view regionAttachmentFieldName(
    RegionAttachmentField field)
{
    switch (field) {
    case RegionAttachmentField::material: return "material";
    case RegionAttachmentField::mask: return "mask";
    case RegionAttachmentField::distance: return "distance";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view regionGlyphName(RegionGlyph glyph)
{
    switch (glyph) {
    case RegionGlyph::cross: return "cross";
    case RegionGlyph::chevron: return "chevron";
    case RegionGlyph::triangle: return "triangle";
    case RegionGlyph::rune: return "rune";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view textilePatternName(TextilePattern pattern)
{
    switch (pattern) {
    case TextilePattern::plainWeave: return "plain_weave";
    case TextilePattern::basketWeave: return "basket_weave";
    case TextilePattern::twillWeave: return "twill_weave";
    case TextilePattern::loopPile: return "loop_pile";
    case TextilePattern::cutPile: return "cut_pile";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view textileFieldName(TextileField field)
{
    switch (field) {
    case TextileField::material: return "material";
    case TextileField::height: return "height";
    case TextileField::warp: return "warp";
    case TextileField::weft: return "weft";
    case TextileField::overUnder: return "over_under";
    case TextileField::fibres: return "fibres";
    case TextileField::pile: return "pile";
    case TextileField::damage: return "damage";
    case TextileField::colourVariation: return "colour_variation";
    case TextileField::direction: return "direction";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view yarnProfileName(YarnProfile profile)
{
    switch (profile) {
    case YarnProfile::round: return "round";
    case YarnProfile::flat: return "flat";
    case YarnProfile::twisted: return "twisted";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view textileTileOrientationName(
    TextileTileOrientation orientation)
{
    switch (orientation) {
    case TextileTileOrientation::uniform: return "uniform";
    case TextileTileOrientation::alternatingRows: return "alternating_rows";
    case TextileTileOrientation::alternatingColumns: return "alternating_columns";
    case TextileTileOrientation::checkerboard: return "checkerboard";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view organicCellFieldName(OrganicCellField field)
{
    switch (field) {
    case OrganicCellField::plates: return "plates";
    case OrganicCellField::boundaries: return "boundaries";
    case OrganicCellField::cellRandom: return "cell_random";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view organicDirectionName(OrganicDirection direction)
{
    switch (direction) {
    case OrganicDirection::vertical: return "vertical";
    case OrganicDirection::horizontal: return "horizontal";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view organicCrackFieldName(OrganicCrackField field)
{
    switch (field) {
    case OrganicCrackField::cracks: return "cracks";
    case OrganicCrackField::trunks: return "trunks";
    case OrganicCrackField::branches: return "branches";
    case OrganicCrackField::hierarchy: return "hierarchy";
    case OrganicCrackField::distance: return "distance";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view leafProfileName(LeafProfile profile)
{
    switch (profile) {
    case LeafProfile::ovate: return "ovate";
    case LeafProfile::lanceolate: return "lanceolate";
    case LeafProfile::cordate: return "cordate";
    case LeafProfile::lobed: return "lobed";
    case LeafProfile::blob: return "blob";
    case LeafProfile::rosette: return "rosette";
    case LeafProfile::lichen: return "lichen";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view leafClusterPatternName(LeafClusterPattern pattern)
{
    switch (pattern) {
    case LeafClusterPattern::radial: return "radial";
    case LeafClusterPattern::fan: return "fan";
    case LeafClusterPattern::vine: return "vine";
    case LeafClusterPattern::canopy: return "canopy";
    case LeafClusterPattern::groundScatter: return "ground_scatter";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view leafFieldName(LeafField field)
{
    switch (field) {
    case LeafField::material: return "material";
    case LeafField::fill: return "fill";
    case LeafField::edge: return "edge";
    case LeafField::midrib: return "midrib";
    case LeafField::veins: return "veins";
    case LeafField::instanceRandom: return "instance_random";
    case LeafField::outline: return "outline";
    case LeafField::innerHighlight: return "inner_highlight";
    case LeafField::clusterRandom: return "cluster_random";
    case LeafField::population: return "population";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view organicAccumulationProfileName(
    OrganicAccumulationProfile profile)
{
    switch (profile) {
    case OrganicAccumulationProfile::noise: return "noise";
    case OrganicAccumulationProfile::colonies: return "colonies";
    case OrganicAccumulationProfile::speckles: return "speckles";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view organicAccumulationFieldName(
    OrganicAccumulationField field)
{
    switch (field) {
    case OrganicAccumulationField::material: return "material";
    case OrganicAccumulationField::fill: return "fill";
    case OrganicAccumulationField::outline: return "outline";
    case OrganicAccumulationField::innerHighlight: return "inner_highlight";
    case OrganicAccumulationField::detail: return "detail";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view organicAccumulationKindName(
    OrganicAccumulationKind kind)
{
    switch (kind) {
    case OrganicAccumulationKind::moss: return "moss";
    case OrganicAccumulationKind::lichen: return "lichen";
    case OrganicAccumulationKind::colourVariation: return "colour_variation";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view organicAccumulationSourceName(
    OrganicAccumulationSource source)
{
    switch (source) {
    case OrganicAccumulationSource::cavity: return "cavity";
    case OrganicAccumulationSource::boundary: return "boundary";
    case OrganicAccumulationSource::lowHeight: return "low_height";
    case OrganicAccumulationSource::authoredMask: return "authored_mask";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view shapePrimitiveKindName(
    ShapePrimitiveKind kind)
{
    switch (kind) {
    case ShapePrimitiveKind::roundedRectangle:
        return "rounded_rectangle";
    case ShapePrimitiveKind::ellipse:
        return "ellipse";
    case ShapePrimitiveKind::capsule:
        return "capsule";
    case ShapePrimitiveKind::diamond:
        return "diamond";
    case ShapePrimitiveKind::convexPolygon:
        return "convex_polygon";
    case ShapePrimitiveKind::annulus:
        return "annulus";
    case ShapePrimitiveKind::arc:
        return "arc";
    case ShapePrimitiveKind::sector:
        return "sector";
    case ShapePrimitiveKind::crescent:
        return "crescent";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view radialOrientationName(
    RadialOrientation orientation)
{
    switch (orientation) {
    case RadialOrientation::fixed:
        return "fixed";
    case RadialOrientation::outward:
        return "outward";
    case RadialOrientation::tangent:
        return "tangent";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view shapeFieldKindName(ShapeFieldKind field)
{
    switch (field) {
    case ShapeFieldKind::fill:
        return "fill";
    case ShapeFieldKind::inset:
        return "inset";
    case ShapeFieldKind::outline:
        return "outline";
    case ShapeFieldKind::border:
        return "border";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view shapeBooleanModeName(ShapeBooleanMode mode)
{
    switch (mode) {
    case ShapeBooleanMode::unionMask:
        return "union";
    case ShapeBooleanMode::intersection:
        return "intersection";
    case ShapeBooleanMode::subtraction:
        return "subtraction";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view latticeKindName(LatticeKind kind)
{
    switch (kind) {
    case LatticeKind::lines:
        return "lines";
    case LatticeKind::diamonds:
        return "diamonds";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view scatterOverlapModeName(
    ScatterOverlapMode mode)
{
    switch (mode) {
    case ScatterOverlapMode::forbidden:
        return "forbidden";
    case ScatterOverlapMode::controlled:
        return "controlled";
    case ScatterOverlapMode::unrestricted:
        return "unrestricted";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view scatterFieldName(ScatterField field)
{
    switch (field) {
    case ScatterField::material:
        return "material";
    case ScatterField::fill:
        return "fill";
    case ScatterField::instanceRandom:
        return "instance_random";
    case ScatterField::localU:
        return "local_u";
    case ScatterField::localV:
        return "local_v";
    case ScatterField::boundaryDistance:
        return "boundary_distance";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view regionSurfaceFieldName(
    RegionSurfaceField field)
{
    switch (field) {
    case RegionSurfaceField::height:
        return "height";
    case RegionSurfaceField::cavity:
        return "cavity";
    case RegionSurfaceField::outerEdge:
        return "outer_edge";
    case RegionSurfaceField::exposedFace:
        return "exposed_face";
    case RegionSurfaceField::facet:
        return "facet";
    case RegionSurfaceField::wear:
        return "wear";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view bevelProfileName(BevelProfile profile)
{
    switch (profile) {
    case BevelProfile::rounded:
        return "rounded";
    case BevelProfile::chamfered:
        return "chamfered";
    case BevelProfile::handCut:
        return "hand_cut";
    }
    return "unknown";
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
