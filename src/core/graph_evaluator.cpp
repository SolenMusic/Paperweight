#include "graph_evaluator.hpp"

#include <paperweight/surface.hpp>

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
        return evaluateNode(outputSources_[materialOutputIndex(output)], context, true);
    }

    std::size_t nodeCount() const noexcept
    {
        return graph_.nodes.size();
    }

private:
    EvaluatedSample evaluateNode(
        std::size_t index,
        const EvaluationContext& context,
        bool cacheResult)
    {
        if (cacheResult && sampleStamps_[index] == currentStamp_) {
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
                [this, &context, cacheResult](const ProcessingNode& processing) {
                    return std::visit(
                        Overloaded{
                            [this, &context, cacheResult](const LevelsProcessing& levels) {
                                const auto input = evaluateNode(
                                    nodeIndices_.at(levels.input),
                                    context,
                                    cacheResult);
                                return evaluateOperation(
                                    LayerOperation{levels.parameters},
                                    context,
                                    input);
                            },
                            [this, &context, cacheResult](const ThresholdProcessing& threshold) {
                                const auto input = evaluateNode(
                                    nodeIndices_.at(threshold.input),
                                    context,
                                    cacheResult);
                                return evaluateOperation(
                                    LayerOperation{threshold.parameters},
                                    context,
                                    input);
                            },
                            [this, &context, cacheResult](const CompositeProcessing& composite) {
                                const auto background = evaluateNode(
                                    nodeIndices_.at(composite.background),
                                    context,
                                    cacheResult);
                                const auto source = evaluateNode(
                                    nodeIndices_.at(composite.source),
                                    context,
                                    cacheResult);
                                const double mask = composite.mask
                                    ? evaluateNode(
                                          nodeIndices_.at(*composite.mask),
                                          context,
                                          cacheResult).scalar
                                    : 1.0;
                                return compositeSamples(
                                    background,
                                    source,
                                    composite.mode,
                                    composite.opacity * mask);
                            },
                            [this, &context, cacheResult](
                                const SurfaceFilterProcessing& filter) {
                                const auto inputIndex = nodeIndices_.at(filter.input);
                                const auto centre = evaluateNode(
                                    inputIndex,
                                    context,
                                    cacheResult);
                                std::array<EvaluatedSample, 9> samples{};
                                samples[0] = centre;
                                if (filter.parameters.kind == SurfaceFilterKind::invert ||
                                    filter.parameters.radius == 0.0) {
                                    std::fill(samples.begin() + 1, samples.end(), centre);
                                } else {
                                    const double radius = filter.parameters.radius;
                                    const std::array<std::pair<double, double>, 8> offsets{
                                        std::pair{-radius, 0.0},
                                        std::pair{radius, 0.0},
                                        std::pair{0.0, -radius},
                                        std::pair{0.0, radius},
                                        std::pair{-radius, -radius},
                                        std::pair{radius, -radius},
                                        std::pair{-radius, radius},
                                        std::pair{radius, radius},
                                    };
                                    for (std::size_t sampleIndex = 0;
                                         sampleIndex < offsets.size();
                                         ++sampleIndex) {
                                        const auto [offsetU, offsetV] = offsets[sampleIndex];
                                        const EvaluationContext neighbour{
                                            context.material,
                                            context.u + offsetU,
                                            context.v + offsetV,
                                        };
                                        samples[sampleIndex + 1] = evaluateNode(
                                            inputIndex,
                                            neighbour,
                                            false);
                                    }
                                }
                                const auto channel = [&filter, &samples](
                                    double EvaluatedSample::* member) {
                                    const SurfaceNeighbourhood neighbourhood{
                                        samples[0].*member,
                                        samples[1].*member,
                                        samples[2].*member,
                                        samples[3].*member,
                                        samples[4].*member,
                                        samples[5].*member,
                                        samples[6].*member,
                                        samples[7].*member,
                                        samples[8].*member,
                                    };
                                    return evaluateSurfaceFilter(
                                        filter.parameters,
                                        neighbourhood);
                                };
                                return EvaluatedSample{
                                    channel(&EvaluatedSample::scalar),
                                    channel(&EvaluatedSample::red),
                                    channel(&EvaluatedSample::green),
                                    channel(&EvaluatedSample::blue),
                                    centre.alpha,
                                };
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
                [this, &context, cacheResult](const OutputNode& output) {
                    return evaluateNode(
                        nodeIndices_.at(output.input),
                        context,
                        cacheResult);
                },
            },
            graph_.nodes[index]);
        if (cacheResult) {
            samples_[index] = result;
            sampleStamps_[index] = currentStamp_;
        }
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
