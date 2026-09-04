#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <paperweight/layer.hpp>
#include <paperweight/output.hpp>

namespace paperweight {

struct Material;

using GraphNodeId = std::uint32_t;
inline constexpr GraphNodeId invalidGraphNodeId = 0;

using GeneratorOperation = std::variant<
    NoiseOperation,
    SolidColourOperation,
    BrickGridOperation,
    TileGridOperation,
    WorleyCellsOperation,
    RandomCellsOperation,
    LinesOperation,
    RectanglesOperation,
    CirclesOperation,
    SurfacePatternOperation,
    CourseLayoutOperation,
    ShapePrimitiveOperation,
    LatticeOperation,
    ScatterOperation,
    OrganicCellOperation,
    OrganicCrackOperation,
    LeafClusterOperation,
    SurfaceValueOperation,
    TextileOperation>;

struct GeneratorNode {
    GraphNodeId id{invalidGraphNodeId};
    std::optional<std::size_t> sourceLayer;
    CoordinateTransform transform;
    GeneratorOperation operation{NoiseOperation{}};

    friend constexpr bool operator==(const GeneratorNode&, const GeneratorNode&) = default;
};

struct LevelsProcessing {
    GraphNodeId input{invalidGraphNodeId};
    LevelsOperation parameters;

    friend constexpr bool operator==(
        const LevelsProcessing&,
        const LevelsProcessing&) = default;
};

struct ThresholdProcessing {
    GraphNodeId input{invalidGraphNodeId};
    ThresholdOperation parameters;

    friend constexpr bool operator==(
        const ThresholdProcessing&,
        const ThresholdProcessing&) = default;
};

struct PosteriseProcessing {
    GraphNodeId input{invalidGraphNodeId};
    PosteriseOperation parameters;

    friend constexpr bool operator==(
        const PosteriseProcessing&,
        const PosteriseProcessing&) = default;
};

struct ColourRampProcessing {
    GraphNodeId input{invalidGraphNodeId};
    ColourRampOperation parameters;

    friend bool operator==(
        const ColourRampProcessing&,
        const ColourRampProcessing&) = default;
};

struct PaletteProcessing {
    GraphNodeId input{invalidGraphNodeId};
    PaletteOperation parameters;

    friend bool operator==(
        const PaletteProcessing&,
        const PaletteProcessing&) = default;
};

struct CompositeProcessing {
    GraphNodeId background{invalidGraphNodeId};
    GraphNodeId source{invalidGraphNodeId};
    std::optional<GraphNodeId> mask;
    CompositeMode mode{CompositeMode::blend};
    double opacity{1.0};

    friend constexpr bool operator==(
        const CompositeProcessing&,
        const CompositeProcessing&) = default;
};

struct SurfaceFilterProcessing {
    GraphNodeId input{invalidGraphNodeId};
    SurfaceFilterOperation parameters;

    friend constexpr bool operator==(
        const SurfaceFilterProcessing&,
        const SurfaceFilterProcessing&) = default;
};

struct InkContourProcessing {
    GraphNodeId input{invalidGraphNodeId};
    InkContourOperation parameters;

    friend constexpr bool operator==(
        const InkContourProcessing&,
        const InkContourProcessing&) = default;
};

struct RegionFieldProcessing {
    GraphNodeId input{invalidGraphNodeId};
    RegionFieldOperation parameters;

    friend constexpr bool operator==(
        const RegionFieldProcessing&,
        const RegionFieldProcessing&) = default;
};

struct RegionSurfaceProcessing {
    GraphNodeId input{invalidGraphNodeId};
    RegionSurfaceOperation parameters;

    friend constexpr bool operator==(
        const RegionSurfaceProcessing&,
        const RegionSurfaceProcessing&) = default;
};

struct ShapeBooleanProcessing {
    GraphNodeId input{invalidGraphNodeId};
    ShapeBooleanOperation parameters;

    friend bool operator==(
        const ShapeBooleanProcessing&,
        const ShapeBooleanProcessing&) = default;
};

struct OrganicAccumulationProcessing {
    GraphNodeId input{invalidGraphNodeId};
    OrganicAccumulationOperation parameters;

    friend constexpr bool operator==(
        const OrganicAccumulationProcessing&,
        const OrganicAccumulationProcessing&) = default;
};

using ProcessingOperation = std::variant<
    LevelsProcessing,
    ThresholdProcessing,
    PosteriseProcessing,
    ColourRampProcessing,
    PaletteProcessing,
    CompositeProcessing,
    SurfaceFilterProcessing,
    InkContourProcessing,
    RegionFieldProcessing,
    RegionSurfaceProcessing,
    ShapeBooleanProcessing,
    OrganicAccumulationProcessing>;

struct ProcessingNode {
    GraphNodeId id{invalidGraphNodeId};
    std::optional<std::size_t> sourceLayer;
    ProcessingOperation operation{LevelsProcessing{}};

    friend constexpr bool operator==(const ProcessingNode&, const ProcessingNode&) = default;
};

struct MaskNode {
    GraphNodeId id{invalidGraphNodeId};
    std::optional<std::size_t> sourceLayer;
    CoordinateTransform transform;
    LayerMask mask{true, false, 0, 0.0, 1.0};

    friend constexpr bool operator==(const MaskNode&, const MaskNode&) = default;
};

struct OutputNode {
    GraphNodeId id{invalidGraphNodeId};
    MaterialOutput output{MaterialOutput::colour};
    GraphNodeId input{invalidGraphNodeId};

    friend constexpr bool operator==(const OutputNode&, const OutputNode&) = default;
};

using GraphNode = std::variant<GeneratorNode, ProcessingNode, MaskNode, OutputNode>;

struct MaterialGraph {
    std::vector<GraphNode> nodes;

    friend bool operator==(const MaterialGraph&, const MaterialGraph&) = default;
};

enum class GraphNodeCategory : std::uint8_t {
    generator,
    processing,
    mask,
    output,
};

struct GraphLimits {
    static constexpr std::size_t maximumNodes = 512;
};

enum class GraphErrorCode : std::uint8_t {
    invalidNodeCount,
    invalidNodeId,
    duplicateNodeId,
    invalidOperation,
    invalidParameter,
    missingInput,
    incompatibleInput,
    cycle,
    missingOutput,
    duplicateOutput,
};

struct GraphError {
    GraphErrorCode code;
    std::optional<GraphNodeId> node;
    std::string message;

    friend bool operator==(const GraphError&, const GraphError&) = default;
};

using GraphCompilationResult = std::variant<MaterialGraph, GraphError>;

[[nodiscard]] GraphNodeId graphNodeId(const GraphNode& node);
[[nodiscard]] GraphNodeCategory graphNodeCategory(const GraphNode& node);
[[nodiscard]] std::string_view graphNodeCategoryName(GraphNodeCategory category);
[[nodiscard]] std::optional<GraphError> validateMaterialGraph(const MaterialGraph& graph);
[[nodiscard]] GraphCompilationResult compileMaterialGraph(const Material& material);

} // namespace paperweight
