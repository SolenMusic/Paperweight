#include <paperweight/graph.hpp>

#include <paperweight/material.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace paperweight {
namespace {

template<class... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

GraphError nodeError(GraphErrorCode code, GraphNodeId node, std::string message)
{
    return GraphError{code, node, std::move(message)};
}

bool validMaterialOutput(MaterialOutput output)
{
    switch (output) {
    case MaterialOutput::colour:
    case MaterialOutput::height:
    case MaterialOutput::normal:
    case MaterialOutput::roughness:
    case MaterialOutput::metalness:
    case MaterialOutput::coating:
    case MaterialOutput::occlusion:
    case MaterialOutput::clearCoat:
    case MaterialOutput::clearCoatRoughness:
    case MaterialOutput::emissive:
        return true;
    }
    return false;
}

std::uint64_t stableIdentityKey(std::string_view identity)
{
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t result = offset;
    for (const auto character : identity) {
        result ^= static_cast<unsigned char>(character);
        result *= prime;
    }
    return result;
}

LayerOperation layerOperation(const GeneratorOperation& operation)
{
    return std::visit(
        [](const auto& value) -> LayerOperation {
            return value;
        },
        operation);
}

std::optional<GeneratorOperation> generatorOperation(const LayerOperation& operation)
{
    return std::visit(
        [](const auto& value) -> std::optional<GeneratorOperation> {
            using Operation = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Operation, LevelsOperation> ||
                          std::is_same_v<Operation, ThresholdOperation> ||
                          std::is_same_v<Operation, SurfaceFilterOperation> ||
                          std::is_same_v<Operation, PosteriseOperation> ||
                          std::is_same_v<Operation, ColourRampOperation> ||
                          std::is_same_v<Operation, PaletteOperation> ||
                          std::is_same_v<Operation, InkContourOperation> ||
                          std::is_same_v<Operation, RegionFieldOperation> ||
                          std::is_same_v<Operation, RegionSurfaceOperation> ||
                          std::is_same_v<Operation, RegionAttachmentOperation> ||
                          std::is_same_v<Operation, RegionalDetailOperation> ||
                          std::is_same_v<Operation, ShapeBooleanOperation> ||
                          std::is_same_v<Operation, OrganicAccumulationOperation>) {
                return std::nullopt;
            } else {
                return GeneratorOperation{value};
            }
        },
        operation);
}

std::optional<std::string> validateGeneratorNode(const GeneratorNode& node)
{
    if (node.operation.valueless_by_exception()) {
        return "generator operation has no value";
    }
    MaterialLayer layer;
    layer.operation = layerOperation(node.operation);
    layer.transform = node.transform;
    return validateMaterialLayer(
        layer,
        "graph node " + std::to_string(node.id) + ": ");
}

std::optional<std::string> validateProcessingNode(const ProcessingNode& node)
{
    if (node.operation.valueless_by_exception()) {
        return "processing operation has no value";
    }
    MaterialLayer layer;
    std::visit(
        Overloaded{
            [&layer](const LevelsProcessing& levels) {
                layer.operation = levels.parameters;
            },
            [&layer](const ThresholdProcessing& threshold) {
                layer.operation = threshold.parameters;
            },
            [&layer](const PosteriseProcessing& posterise) {
                layer.operation = posterise.parameters;
            },
            [&layer](const ColourRampProcessing& ramp) {
                layer.operation = ramp.parameters;
            },
            [&layer](const PaletteProcessing& palette) {
                layer.operation = palette.parameters;
            },
            [&layer](const CompositeProcessing& composite) {
                layer.compositeMode = composite.mode;
                layer.opacity = composite.opacity;
            },
            [&layer](const SurfaceFilterProcessing& filter) {
                layer.operation = filter.parameters;
            },
            [&layer](const InkContourProcessing& contour) {
                layer.operation = contour.parameters;
            },
            [&layer](const RegionFieldProcessing& field) {
                layer.operation = field.parameters;
            },
            [&layer](const RegionSurfaceProcessing& surface) {
                layer.operation = surface.parameters;
            },
            [&layer](const RegionAttachmentProcessing& attachment) {
                layer.operation = attachment.parameters;
            },
            [&layer](const RegionalDetailProcessing& detail) {
                layer.operation = detail.parameters;
            },
            [&layer](const ShapeBooleanProcessing& shape) {
                layer.operation = shape.parameters;
            },
            [&layer](const OrganicAccumulationProcessing& organic) {
                layer.operation = organic.parameters;
            },
        },
        node.operation);
    return validateMaterialLayer(
        layer,
        "graph node " + std::to_string(node.id) + ": ");
}

std::optional<std::string> validateMaskNode(const MaskNode& node)
{
    if (!node.mask.enabled) {
        return "graph node " + std::to_string(node.id) +
            ": mask nodes must be enabled";
    }
    MaterialLayer layer;
    layer.transform = node.transform;
    layer.mask = node.mask;
    return validateMaterialLayer(
        layer,
        "graph node " + std::to_string(node.id) + ": ");
}

std::vector<GraphNodeId> dependencies(const GraphNode& node)
{
    return std::visit(
        Overloaded{
            [](const GeneratorNode&) {
                return std::vector<GraphNodeId>{};
            },
            [](const ProcessingNode& processing) {
                return std::visit(
                    Overloaded{
                        [](const LevelsProcessing& levels) {
                            return std::vector<GraphNodeId>{levels.input};
                        },
                        [](const ThresholdProcessing& threshold) {
                            return std::vector<GraphNodeId>{threshold.input};
                        },
                        [](const PosteriseProcessing& posterise) {
                            return std::vector<GraphNodeId>{posterise.input};
                        },
                        [](const ColourRampProcessing& ramp) {
                            return std::vector<GraphNodeId>{ramp.input};
                        },
                        [](const PaletteProcessing& palette) {
                            return std::vector<GraphNodeId>{palette.input};
                        },
                        [](const CompositeProcessing& composite) {
                            std::vector<GraphNodeId> result{
                                composite.background,
                                composite.source,
                            };
                            if (composite.mask) {
                                result.push_back(*composite.mask);
                            }
                            return result;
                        },
                        [](const SurfaceFilterProcessing& filter) {
                            return std::vector<GraphNodeId>{filter.input};
                        },
                        [](const InkContourProcessing& contour) {
                            return std::vector<GraphNodeId>{contour.input};
                        },
                        [](const RegionFieldProcessing& field) {
                            return std::vector<GraphNodeId>{field.input};
                        },
                        [](const RegionSurfaceProcessing& surface) {
                            return std::vector<GraphNodeId>{surface.input};
                        },
                        [](const RegionAttachmentProcessing& attachment) {
                            return std::vector<GraphNodeId>{attachment.input};
                        },
                        [](const RegionalDetailProcessing& detail) {
                            return std::vector<GraphNodeId>{detail.input};
                        },
                        [](const ShapeBooleanProcessing& shape) {
                            return std::vector<GraphNodeId>{shape.input};
                        },
                        [](const OrganicAccumulationProcessing& organic) {
                            return std::vector<GraphNodeId>{organic.input};
                        },
                    },
                    processing.operation);
            },
            [](const MaskNode&) {
                return std::vector<GraphNodeId>{};
            },
            [](const OutputNode& output) {
                return std::vector<GraphNodeId>{output.input};
            },
        },
        node);
}

} // namespace

GraphNodeId graphNodeId(const GraphNode& node)
{
    return std::visit([](const auto& value) { return value.id; }, node);
}

GraphNodeCategory graphNodeCategory(const GraphNode& node)
{
    return std::visit(
        Overloaded{
            [](const GeneratorNode&) { return GraphNodeCategory::generator; },
            [](const ProcessingNode&) { return GraphNodeCategory::processing; },
            [](const MaskNode&) { return GraphNodeCategory::mask; },
            [](const OutputNode&) { return GraphNodeCategory::output; },
        },
        node);
}

std::string_view graphNodeCategoryName(GraphNodeCategory category)
{
    switch (category) {
    case GraphNodeCategory::generator:
        return "generator";
    case GraphNodeCategory::processing:
        return "processing";
    case GraphNodeCategory::mask:
        return "mask";
    case GraphNodeCategory::output:
        return "output";
    }
    return "unknown";
}

std::optional<GraphError> validateMaterialGraph(const MaterialGraph& graph)
{
    if (graph.nodes.size() > GraphLimits::maximumNodes) {
        return GraphError{
            GraphErrorCode::invalidNodeCount,
            std::nullopt,
            "a material graph may contain at most 512 nodes",
        };
    }

    std::unordered_map<GraphNodeId, std::size_t> nodeIndices;
    nodeIndices.reserve(graph.nodes.size());
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        const auto& node = graph.nodes[index];
        if (node.valueless_by_exception()) {
            return GraphError{
                GraphErrorCode::invalidOperation,
                std::nullopt,
                "graph node has no value",
            };
        }
        const auto id = graphNodeId(node);
        if (id == invalidGraphNodeId) {
            return nodeError(
                GraphErrorCode::invalidNodeId,
                id,
                "graph node identifiers must be non-zero");
        }
        if (!nodeIndices.emplace(id, index).second) {
            return nodeError(
                GraphErrorCode::duplicateNodeId,
                id,
                "graph node identifier " + std::to_string(id) + " is duplicated");
        }

        const auto parameterError = std::visit(
            Overloaded{
                [](const GeneratorNode& generator) {
                    return validateGeneratorNode(generator);
                },
                [](const ProcessingNode& processing) {
                    return validateProcessingNode(processing);
                },
                [](const MaskNode& mask) {
                    return validateMaskNode(mask);
                },
                [](const OutputNode& output) -> std::optional<std::string> {
                    if (!validMaterialOutput(output.output)) {
                        return "output kind is not supported";
                    }
                    return std::nullopt;
                },
            },
            node);
        if (parameterError) {
            return nodeError(GraphErrorCode::invalidParameter, id, *parameterError);
        }
    }

    std::array<std::size_t, materialOutputs.size()> outputCounts{};
    for (const auto& node : graph.nodes) {
        const auto ownerId = graphNodeId(node);
        const auto checkInput = [&](GraphNodeId input, GraphNodeCategory expected) ->
            std::optional<GraphError> {
            const auto found = nodeIndices.find(input);
            if (input == invalidGraphNodeId || found == nodeIndices.end()) {
                return nodeError(
                    GraphErrorCode::missingInput,
                    ownerId,
                    "graph node " + std::to_string(ownerId) +
                        " references missing input " + std::to_string(input));
            }
            const auto actual = graphNodeCategory(graph.nodes[found->second]);
            const bool valueCompatible = expected == GraphNodeCategory::generator &&
                (actual == GraphNodeCategory::generator ||
                 actual == GraphNodeCategory::processing);
            const bool valueMaskCompatible = expected == GraphNodeCategory::mask &&
                (actual == GraphNodeCategory::generator ||
                 actual == GraphNodeCategory::processing);
            if (!valueCompatible && !valueMaskCompatible && actual != expected) {
                return nodeError(
                    GraphErrorCode::incompatibleInput,
                    ownerId,
                    "graph node " + std::to_string(ownerId) + " requires a " +
                        std::string(graphNodeCategoryName(expected)) + " input, but node " +
                        std::to_string(input) + " is " +
                        std::string(graphNodeCategoryName(actual)));
            }
            return std::nullopt;
        };

        const auto inputError = std::visit(
            Overloaded{
                [](const GeneratorNode&) -> std::optional<GraphError> {
                    return std::nullopt;
                },
                [&checkInput](const ProcessingNode& processing) ->
                    std::optional<GraphError> {
                    return std::visit(
                        Overloaded{
                            [&checkInput](const LevelsProcessing& levels) {
                                return checkInput(levels.input, GraphNodeCategory::generator);
                            },
                            [&checkInput](const ThresholdProcessing& threshold) {
                                return checkInput(threshold.input, GraphNodeCategory::generator);
                            },
                            [&checkInput](const PosteriseProcessing& posterise) {
                                return checkInput(posterise.input, GraphNodeCategory::generator);
                            },
                            [&checkInput](const ColourRampProcessing& ramp) {
                                return checkInput(ramp.input, GraphNodeCategory::generator);
                            },
                            [&checkInput](const PaletteProcessing& palette) {
                                return checkInput(palette.input, GraphNodeCategory::generator);
                            },
                            [&checkInput](const CompositeProcessing& composite) ->
                                std::optional<GraphError> {
                                if (auto error = checkInput(
                                        composite.background,
                                        GraphNodeCategory::generator)) {
                                    return error;
                                }
                                if (auto error = checkInput(
                                        composite.source,
                                        GraphNodeCategory::generator)) {
                                    return error;
                                }
                                if (composite.mask) {
                                    return checkInput(*composite.mask, GraphNodeCategory::mask);
                                }
                                return std::nullopt;
                            },
                            [&checkInput](const SurfaceFilterProcessing& filter) {
                                return checkInput(
                                    filter.input,
                                    GraphNodeCategory::generator);
                            },
                            [&checkInput](const InkContourProcessing& contour) {
                                return checkInput(
                                    contour.input,
                                    GraphNodeCategory::generator);
                            },
                            [&checkInput](const RegionFieldProcessing& field) {
                                return checkInput(
                                    field.input,
                                    GraphNodeCategory::generator);
                            },
                            [&checkInput](const RegionSurfaceProcessing& surface) {
                                return checkInput(
                                    surface.input,
                                    GraphNodeCategory::generator);
                            },
                            [&checkInput](const RegionAttachmentProcessing& attachment) {
                                return checkInput(
                                    attachment.input,
                                    GraphNodeCategory::generator);
                            },
                            [&checkInput](const RegionalDetailProcessing& detail) {
                                return checkInput(
                                    detail.input,
                                    GraphNodeCategory::generator);
                            },
                            [&checkInput](const ShapeBooleanProcessing& shape) {
                                return checkInput(
                                    shape.input,
                                    GraphNodeCategory::generator);
                            },
                            [&checkInput](const OrganicAccumulationProcessing& organic) {
                                return checkInput(
                                    organic.input,
                                    GraphNodeCategory::generator);
                            },
                        },
                        processing.operation);
                },
                [](const MaskNode&) -> std::optional<GraphError> {
                    return std::nullopt;
                },
                [&checkInput, &outputCounts](const OutputNode& output) {
                    ++outputCounts[materialOutputIndex(output.output)];
                    return checkInput(output.input, GraphNodeCategory::generator);
                },
            },
            node);
        if (inputError) {
            return inputError;
        }
    }

    for (const auto output : materialOutputs) {
        const auto count = outputCounts[materialOutputIndex(output)];
        if (count == 0) {
            return GraphError{
                GraphErrorCode::missingOutput,
                std::nullopt,
                "material graph is missing the " + std::string(materialOutputName(output)) +
                    " output",
            };
        }
        if (count > 1) {
            return GraphError{
                GraphErrorCode::duplicateOutput,
                std::nullopt,
                "material graph has more than one " +
                    std::string(materialOutputName(output)) + " output",
            };
        }
    }

    enum class VisitState : std::uint8_t {
        unseen,
        visiting,
        visited,
    };
    std::vector<VisitState> states(graph.nodes.size(), VisitState::unseen);
    std::function<std::optional<GraphError>(std::size_t)> visit;
    visit = [&](std::size_t index) -> std::optional<GraphError> {
        if (states[index] == VisitState::visited) {
            return std::nullopt;
        }
        if (states[index] == VisitState::visiting) {
            const auto id = graphNodeId(graph.nodes[index]);
            return nodeError(
                GraphErrorCode::cycle,
                id,
                "material graph contains a cycle involving node " + std::to_string(id));
        }
        states[index] = VisitState::visiting;
        for (const auto dependency : dependencies(graph.nodes[index])) {
            if (auto error = visit(nodeIndices.at(dependency))) {
                return error;
            }
        }
        states[index] = VisitState::visited;
        return std::nullopt;
    };
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        if (auto error = visit(index)) {
            return error;
        }
    }
    return std::nullopt;
}

GraphCompilationResult compileMaterialGraph(const Material& material)
{
    if (const auto error = validateMaterial(material)) {
        return GraphError{GraphErrorCode::invalidParameter, std::nullopt, *error};
    }

    MaterialGraph graph;
    GraphNodeId nextId = 1;
    const auto addGenerator = [&](GeneratorOperation operation,
                                  CoordinateTransform transform,
                                  std::optional<std::size_t> sourceLayer) {
        const auto id = nextId++;
        graph.nodes.emplace_back(GeneratorNode{
            id,
            sourceLayer,
            transform,
            std::move(operation),
        });
        return id;
    };
    const auto addProcessing = [&](ProcessingOperation operation,
                                   std::optional<std::size_t> sourceLayer) {
        const auto id = nextId++;
        graph.nodes.emplace_back(ProcessingNode{id, sourceLayer, std::move(operation)});
        return id;
    };

    using LayerCompilation = std::variant<GraphNodeId, GraphError>;
    const auto compileLayer = [&](GraphNodeId accumulated,
                                  const MaterialLayer& layer,
                                  std::size_t layerIndex,
                                  std::uint64_t groupScopeKey) -> LayerCompilation {
            GraphNodeId source = invalidGraphNodeId;
            if (const auto* levels = std::get_if<LevelsOperation>(&layer.operation)) {
                source = addProcessing(
                    LevelsProcessing{accumulated, *levels},
                    layerIndex);
            } else if (const auto* threshold =
                           std::get_if<ThresholdOperation>(&layer.operation)) {
                source = addProcessing(
                    ThresholdProcessing{accumulated, *threshold},
                    layerIndex);
            } else if (const auto* filter =
                           std::get_if<SurfaceFilterOperation>(&layer.operation)) {
                source = addProcessing(
                    SurfaceFilterProcessing{accumulated, *filter},
                    layerIndex);
            } else if (const auto* posterise =
                           std::get_if<PosteriseOperation>(&layer.operation)) {
                source = addProcessing(
                    PosteriseProcessing{accumulated, *posterise},
                    layerIndex);
            } else if (const auto* ramp =
                           std::get_if<ColourRampOperation>(&layer.operation)) {
                source = addProcessing(
                    ColourRampProcessing{accumulated, *ramp},
                    layerIndex);
            } else if (const auto* palette =
                           std::get_if<PaletteOperation>(&layer.operation)) {
                source = addProcessing(
                    PaletteProcessing{accumulated, *palette},
                    layerIndex);
            } else if (const auto* contour =
                           std::get_if<InkContourOperation>(&layer.operation)) {
                source = addProcessing(
                    InkContourProcessing{accumulated, *contour},
                    layerIndex);
            } else if (const auto* field =
                           std::get_if<RegionFieldOperation>(&layer.operation)) {
                source = addProcessing(
                    RegionFieldProcessing{accumulated, *field},
                    layerIndex);
            } else if (const auto* surface =
                           std::get_if<RegionSurfaceOperation>(&layer.operation)) {
                source = addProcessing(
                    RegionSurfaceProcessing{accumulated, *surface},
                    layerIndex);
            } else if (const auto* attachment =
                           std::get_if<RegionAttachmentOperation>(&layer.operation)) {
                source = addProcessing(
                    RegionAttachmentProcessing{accumulated, *attachment},
                    layerIndex);
            } else if (const auto* detail =
                           std::get_if<RegionalDetailOperation>(&layer.operation)) {
                source = addProcessing(
                    RegionalDetailProcessing{accumulated, *detail, groupScopeKey},
                    layerIndex);
            } else if (const auto* shape =
                           std::get_if<ShapeBooleanOperation>(&layer.operation)) {
                source = addProcessing(
                    ShapeBooleanProcessing{accumulated, *shape},
                    layerIndex);
            } else if (const auto* organic =
                           std::get_if<OrganicAccumulationOperation>(&layer.operation)) {
                source = addProcessing(
                    OrganicAccumulationProcessing{accumulated, *organic},
                    layerIndex);
            } else {
                const auto operation = generatorOperation(layer.operation);
                if (!operation) {
                    return GraphError{
                        GraphErrorCode::invalidOperation,
                        std::nullopt,
                        "layer operation could not be compiled as a graph generator",
                    };
                }
                source = addGenerator(*operation, layer.transform, layerIndex);
            }

            std::optional<GraphNodeId> mask;
            if (layer.mask.enabled) {
                const auto maskId = nextId++;
                graph.nodes.emplace_back(MaskNode{
                    maskId,
                    layerIndex,
                    layer.transform,
                    layer.mask,
                });
                mask = maskId;
            }
            return addProcessing(
                CompositeProcessing{
                    accumulated,
                    source,
                    mask,
                    layer.compositeMode,
                    layer.opacity,
                },
                layerIndex);
    };

    struct StackItem {
        std::optional<std::size_t> layerIndex;
        std::optional<std::size_t> groupIndex;
        std::vector<StackItem> children;
    };
    StackItem stackRoot;
    std::unordered_map<std::string, std::size_t> groupIndices;
    groupIndices.reserve(material.layerGroups.size());
    for (std::size_t index = 0; index < material.layerGroups.size(); ++index) {
        groupIndices.emplace(material.layerGroups[index].identity, index);
    }
    for (std::size_t layerIndex = 0; layerIndex < material.layers.size(); ++layerIndex) {
        std::vector<std::size_t> path;
        auto parent = material.layerHierarchy.empty()
            ? std::string{}
            : material.layerHierarchy[layerIndex].parentGroupIdentity;
        while (!parent.empty()) {
            const auto groupIndex = groupIndices.at(parent);
            path.push_back(groupIndex);
            parent = material.layerGroups[groupIndex].parentGroupIdentity;
        }
        std::reverse(path.begin(), path.end());

        auto* container = &stackRoot;
        for (const auto groupIndex : path) {
            if (container->children.empty() ||
                container->children.back().groupIndex != groupIndex) {
                StackItem groupItem;
                groupItem.groupIndex = groupIndex;
                container->children.push_back(std::move(groupItem));
            }
            container = &container->children.back();
        }
        StackItem layerItem;
        layerItem.layerIndex = layerIndex;
        container->children.push_back(std::move(layerItem));
    }

    struct StackCompilation {
        GraphNodeId accumulated{invalidGraphNodeId};
        bool contributed{};
    };
    using StackCompilationResult = std::variant<StackCompilation, GraphError>;
    std::function<StackCompilationResult(
        const StackItem&,
        GraphNodeId,
        std::optional<MaterialOutput>,
        std::uint64_t)>
        compileStack;
    compileStack = [&](const StackItem& container,
                       GraphNodeId initial,
                       std::optional<MaterialOutput> route,
                       std::uint64_t groupScopeKey) -> StackCompilationResult {
        StackCompilation result{initial, false};
        for (const auto& item : container.children) {
            if (item.layerIndex) {
                const auto layerIndex = *item.layerIndex;
                const auto& layer = material.layers[layerIndex];
                if (!layer.enabled || (route && !layer.outputs.includes(*route))) {
                    continue;
                }
                auto compiled = compileLayer(
                    result.accumulated,
                    layer,
                    layerIndex,
                    groupScopeKey);
                if (const auto* error = std::get_if<GraphError>(&compiled)) {
                    return *error;
                }
                result.accumulated = std::get<GraphNodeId>(compiled);
                result.contributed = true;
                continue;
            }

            const auto& group = material.layerGroups[*item.groupIndex];
            if (!group.enabled || (route && !group.outputs.includes(*route))) {
                continue;
            }
            // Groups are pass-through by default: their processing layers see
            // the accumulated material below the group. Therefore wrapping a
            // contiguous layer range in a 100%-opaque, unmasked blend group is
            // pixel-preserving. Opacity and masks blend the complete before/
            // after group result coherently for every routed output.
            auto childResultVariant = compileStack(
                item,
                result.accumulated,
                route,
                stableIdentityKey(group.identity));
            if (const auto* error = std::get_if<GraphError>(&childResultVariant)) {
                return *error;
            }
            const auto childResult = std::get<StackCompilation>(childResultVariant);
            if (!childResult.contributed) {
                continue;
            }

            std::optional<GraphNodeId> mask;
            if (group.mask.enabled) {
                const auto maskId = nextId++;
                graph.nodes.emplace_back(MaskNode{
                    maskId,
                    std::nullopt,
                    group.transform,
                    group.mask,
                });
                mask = maskId;
            }
            result.accumulated = addProcessing(
                CompositeProcessing{
                    result.accumulated,
                    childResult.accumulated,
                    mask,
                    group.compositeMode,
                    group.opacity,
                },
                std::nullopt);
            result.contributed = true;
        }
        return result;
    };

    GraphNodeId colourInput = invalidGraphNodeId;
    GraphNodeId heightInput = invalidGraphNodeId;
    GraphNodeId roughnessInput = invalidGraphNodeId;
    GraphNodeId metalnessInput = invalidGraphNodeId;
    GraphNodeId coatingInput = invalidGraphNodeId;
    GraphNodeId occlusionInput = invalidGraphNodeId;
    GraphNodeId clearCoatInput = invalidGraphNodeId;
    GraphNodeId clearCoatRoughnessInput = invalidGraphNodeId;
    GraphNodeId emissiveInput = invalidGraphNodeId;
    if (material.layers.empty()) {
        const auto noise = addGenerator(NoiseOperation{}, {}, std::nullopt);
        colourInput = noise;
        heightInput = noise;
        roughnessInput = noise;
        metalnessInput = noise;
        coatingInput = noise;
        occlusionInput = noise;
        clearCoatInput = noise;
        clearCoatRoughnessInput = noise;
        emissiveInput = noise;
    } else {
        const auto base = addGenerator(
            SolidColourOperation{Rgba8{0, 0, 0, 0}},
            {},
            std::nullopt);
        const bool useLegacySharedGraph = std::all_of(
            material.layers.begin(),
            material.layers.end(),
            [](const MaterialLayer& layer) {
                return !layer.enabled || layer.outputs.isLegacyAll();
            }) && std::all_of(
                material.layerGroups.begin(),
                material.layerGroups.end(),
                [](const MaterialLayerGroup& group) {
                    return !group.enabled || group.outputs.isLegacyAll();
                });
        if (useLegacySharedGraph) {
            auto compiled = compileStack(stackRoot, base, std::nullopt, 0);
            if (const auto* error = std::get_if<GraphError>(&compiled)) {
                return *error;
            }
            const auto accumulated = std::get<StackCompilation>(compiled).accumulated;
            colourInput = accumulated;
            heightInput = accumulated;
            roughnessInput = accumulated;
            metalnessInput = accumulated;
            coatingInput = accumulated;
            occlusionInput = accumulated;
            clearCoatInput = accumulated;
            clearCoatRoughnessInput = accumulated;
            emissiveInput = accumulated;
        } else {
            std::array<GraphNodeId, 9> accumulated{
                base, base, base, base, base, base, base, base, base};
            constexpr std::array routedOutputs{
                MaterialOutput::colour,
                MaterialOutput::height,
                MaterialOutput::roughness,
                MaterialOutput::metalness,
                MaterialOutput::coating,
                MaterialOutput::occlusion,
                MaterialOutput::clearCoat,
                MaterialOutput::clearCoatRoughness,
                MaterialOutput::emissive,
            };
            for (std::size_t route = 0; route < routedOutputs.size(); ++route) {
                auto compiled = compileStack(
                    stackRoot,
                    accumulated[route],
                    routedOutputs[route],
                    0);
                if (const auto* error = std::get_if<GraphError>(&compiled)) {
                    return *error;
                }
                accumulated[route] = std::get<StackCompilation>(compiled).accumulated;
            }
            colourInput = accumulated[0];
            heightInput = accumulated[1];
            roughnessInput = accumulated[2];
            metalnessInput = accumulated[3];
            coatingInput = accumulated[4];
            occlusionInput = accumulated[5];
            clearCoatInput = accumulated[6];
            clearCoatRoughnessInput = accumulated[7];
            emissiveInput = accumulated[8];
        }
    }

    graph.nodes.emplace_back(OutputNode{nextId++, MaterialOutput::colour, colourInput});
    graph.nodes.emplace_back(OutputNode{nextId++, MaterialOutput::height, heightInput});
    graph.nodes.emplace_back(OutputNode{nextId++, MaterialOutput::normal, heightInput});
    graph.nodes.emplace_back(OutputNode{nextId++, MaterialOutput::roughness, roughnessInput});
    graph.nodes.emplace_back(OutputNode{nextId++, MaterialOutput::metalness, metalnessInput});
    graph.nodes.emplace_back(OutputNode{nextId++, MaterialOutput::coating, coatingInput});
    graph.nodes.emplace_back(OutputNode{nextId++, MaterialOutput::occlusion, occlusionInput});
    graph.nodes.emplace_back(OutputNode{nextId++, MaterialOutput::clearCoat, clearCoatInput});
    graph.nodes.emplace_back(OutputNode{
        nextId++, MaterialOutput::clearCoatRoughness, clearCoatRoughnessInput});
    graph.nodes.emplace_back(OutputNode{nextId++, MaterialOutput::emissive, emissiveInput});
    if (const auto error = validateMaterialGraph(graph)) {
        return *error;
    }
    return graph;
}

} // namespace paperweight
