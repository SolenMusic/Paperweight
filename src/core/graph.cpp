#include <paperweight/graph.hpp>

#include <paperweight/material.hpp>

#include <array>
#include <functional>
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
        return true;
    }
    return false;
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
                          std::is_same_v<Operation, SurfaceFilterOperation>) {
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
            [&layer](const CompositeProcessing& composite) {
                layer.compositeMode = composite.mode;
                layer.opacity = composite.opacity;
            },
            [&layer](const SurfaceFilterProcessing& filter) {
                layer.operation = filter.parameters;
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
            if (!valueCompatible && actual != expected) {
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

    GraphNodeId accumulated = invalidGraphNodeId;
    if (material.layers.empty()) {
        accumulated = addGenerator(NoiseOperation{}, {}, std::nullopt);
    } else {
        accumulated = addGenerator(
            SolidColourOperation{Rgba8{0, 0, 0, 0}},
            {},
            std::nullopt);
        for (std::size_t layerIndex = 0; layerIndex < material.layers.size(); ++layerIndex) {
            const auto& layer = material.layers[layerIndex];
            if (!layer.enabled) {
                continue;
            }

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
            accumulated = addProcessing(
                CompositeProcessing{
                    accumulated,
                    source,
                    mask,
                    layer.compositeMode,
                    layer.opacity,
                },
                layerIndex);
        }
    }

    for (const auto output : materialOutputs) {
        graph.nodes.emplace_back(OutputNode{nextId++, output, accumulated});
    }
    if (const auto error = validateMaterialGraph(graph)) {
        return *error;
    }
    return graph;
}

} // namespace paperweight
