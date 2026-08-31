#include "graph_evaluator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace paperweight::detail {
namespace {

template<class... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

LayerOperation asLayerOperation(const GeneratorOperation& operation)
{
    return std::visit(
        [](const auto& value) -> LayerOperation {
            return value;
        },
        operation);
}

} // namespace

class GraphEvaluator::Impl {
public:
    Impl(const Material& material, const MaterialGraph& graph)
        : material_(material), graph_(graph), samples_(graph.nodes.size()),
          sampleStamps_(graph.nodes.size())
    {
        if (const auto error = validateMaterialGraph(graph_)) {
            throw std::invalid_argument(error->message);
        }
        nodeIndices_.reserve(graph_.nodes.size());
        for (std::size_t index = 0; index < graph_.nodes.size(); ++index) {
            nodeIndices_.emplace(graphNodeId(graph_.nodes[index]), index);
        }
        for (const auto& node : graph_.nodes) {
            if (const auto* output = std::get_if<OutputNode>(&node)) {
                outputSources_[materialOutputIndex(output->output)] =
                    nodeIndices_.at(output->input);
            }
        }
    }

    EvaluatedSample evaluate(MaterialOutput output, double u, double v)
    {
        if (!std::isfinite(u) || !std::isfinite(v)) {
            throw std::invalid_argument("evaluation coordinates must be finite");
        }
        if (materialOutputIndex(output) >= outputSources_.size()) {
            throw std::invalid_argument("material output is not supported");
        }
        if (currentStamp_ == std::numeric_limits<std::uint64_t>::max()) {
            std::fill(sampleStamps_.begin(), sampleStamps_.end(), 0);
            currentStamp_ = 1;
        } else {
            ++currentStamp_;
        }
        const EvaluationContext context{material_, u, v};
        return evaluateNode(outputSources_[materialOutputIndex(output)], context);
    }

    std::size_t nodeCount() const noexcept
    {
        return graph_.nodes.size();
    }

private:
    EvaluatedSample evaluateNode(std::size_t index, const EvaluationContext& context)
    {
        if (sampleStamps_[index] == currentStamp_) {
            return samples_[index];
        }
        const auto result = std::visit(
            Overloaded{
                [&context](const GeneratorNode& generator) {
                    const auto coordinates = transformCoordinates(generator.transform, context);
                    const EvaluationContext transformed{
                        context.material,
                        coordinates.u,
                        coordinates.v,
                    };
                    return evaluateOperation(
                        asLayerOperation(generator.operation),
                        transformed,
                        {});
                },
                [this, &context](const ProcessingNode& processing) {
                    return std::visit(
                        Overloaded{
                            [this, &context](const LevelsProcessing& levels) {
                                const auto input = evaluateNode(
                                    nodeIndices_.at(levels.input),
                                    context);
                                return evaluateOperation(
                                    LayerOperation{levels.parameters},
                                    context,
                                    input);
                            },
                            [this, &context](const ThresholdProcessing& threshold) {
                                const auto input = evaluateNode(
                                    nodeIndices_.at(threshold.input),
                                    context);
                                return evaluateOperation(
                                    LayerOperation{threshold.parameters},
                                    context,
                                    input);
                            },
                            [this, &context](const CompositeProcessing& composite) {
                                const auto background = evaluateNode(
                                    nodeIndices_.at(composite.background),
                                    context);
                                const auto source = evaluateNode(
                                    nodeIndices_.at(composite.source),
                                    context);
                                const double mask = composite.mask
                                    ? evaluateNode(nodeIndices_.at(*composite.mask), context).scalar
                                    : 1.0;
                                return compositeSamples(
                                    background,
                                    source,
                                    composite.mode,
                                    composite.opacity * mask);
                            },
                        },
                        processing.operation);
                },
                [&context](const MaskNode& mask) {
                    const auto coordinates = transformCoordinates(mask.transform, context);
                    const EvaluationContext transformed{
                        context.material,
                        coordinates.u,
                        coordinates.v,
                    };
                    const double value = evaluateLayerMask(mask.mask, transformed);
                    return EvaluatedSample{value, value, value, value, 1.0};
                },
                [this, &context](const OutputNode& output) {
                    return evaluateNode(nodeIndices_.at(output.input), context);
                },
            },
            graph_.nodes[index]);
        samples_[index] = result;
        sampleStamps_[index] = currentStamp_;
        return result;
    }

    const Material& material_;
    const MaterialGraph& graph_;
    std::unordered_map<GraphNodeId, std::size_t> nodeIndices_;
    std::array<std::size_t, materialOutputs.size()> outputSources_{};
    std::vector<EvaluatedSample> samples_;
    std::vector<std::uint64_t> sampleStamps_;
    std::uint64_t currentStamp_{};
};

GraphEvaluator::GraphEvaluator(const Material& material, const MaterialGraph& graph)
    : impl_(std::make_unique<Impl>(material, graph))
{
}

GraphEvaluator::~GraphEvaluator() = default;
GraphEvaluator::GraphEvaluator(GraphEvaluator&&) noexcept = default;
GraphEvaluator& GraphEvaluator::operator=(GraphEvaluator&&) noexcept = default;

EvaluatedSample GraphEvaluator::evaluate(MaterialOutput output, double u, double v)
{
    return impl_->evaluate(output, u, v);
}

std::size_t GraphEvaluator::nodeCount() const noexcept
{
    return impl_->nodeCount();
}

} // namespace paperweight::detail
