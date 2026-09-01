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
#include <variant>
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

struct GeneratorPlan {
    CoordinateTransform transform;
    LayerOperation operation;
};

struct LevelsPlan {
    std::size_t input;
    LevelsOperation parameters;
};

struct ThresholdPlan {
    std::size_t input;
    ThresholdOperation parameters;
};

struct PosterisePlan {
    std::size_t input;
    PosteriseOperation parameters;
};

struct ColourRampPlan {
    std::size_t input;
    ColourRampOperation parameters;
};

struct PalettePlan {
    std::size_t input;
    PaletteOperation parameters;
};

struct CompositePlan {
    std::size_t background;
    std::size_t source;
    std::optional<std::size_t> mask;
    CompositeMode mode;
    double opacity;
};

struct SurfaceFilterPlan {
    std::size_t input;
    SurfaceFilterOperation parameters;
};

struct InkContourPlan {
    std::size_t input;
    InkContourOperation parameters;
};

struct RegionFieldPlan {
    std::size_t input;
    RegionFieldOperation parameters;
};

struct MaskPlan {
    CoordinateTransform transform;
    LayerMask mask;
};

struct OutputPlan {
    std::size_t input;
};

using EvaluationPlan = std::variant<
    GeneratorPlan,
    LevelsPlan,
    ThresholdPlan,
    PosterisePlan,
    ColourRampPlan,
    PalettePlan,
    CompositePlan,
    SurfaceFilterPlan,
    InkContourPlan,
    RegionFieldPlan,
    MaskPlan,
    OutputPlan>;

bool affectsColour(ProcessingTarget target)
{
    return target == ProcessingTarget::colour ||
        target == ProcessingTarget::colourAndScalar;
}

bool affectsScalar(ProcessingTarget target)
{
    return target == ProcessingTarget::scalar ||
        target == ProcessingTarget::colourAndScalar;
}

double smoothStep(double edge0, double edge1, double value)
{
    if (edge0 == edge1) {
        return value >= edge1 ? 1.0 : 0.0;
    }
    const double amount = std::clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
    return amount * amount * (3.0 - 2.0 * amount);
}

} // namespace

class GraphEvaluator::Impl {
public:
    Impl(const Material& material, const MaterialGraph& graph)
        : material_(material), samples_(graph.nodes.size()),
          sampleStamps_(graph.nodes.size())
    {
        if (const auto error = validateMaterialGraph(graph)) {
            throw std::invalid_argument(error->message);
        }

        std::unordered_map<GraphNodeId, std::size_t> nodeIndices;
        nodeIndices.reserve(graph.nodes.size());
        for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
            nodeIndices.emplace(graphNodeId(graph.nodes[index]), index);
        }
        const auto indexOf = [&nodeIndices](GraphNodeId id) {
            return nodeIndices.at(id);
        };

        plans_.reserve(graph.nodes.size());
        for (const auto& node : graph.nodes) {
            plans_.push_back(std::visit(
                Overloaded{
                    [](const GeneratorNode& generator) -> EvaluationPlan {
                        return GeneratorPlan{
                            generator.transform,
                            asLayerOperation(generator.operation),
                        };
                    },
                    [&indexOf](const ProcessingNode& processing) -> EvaluationPlan {
                        return std::visit(
                            Overloaded{
                                [&indexOf](const LevelsProcessing& levels) -> EvaluationPlan {
                                    return LevelsPlan{indexOf(levels.input), levels.parameters};
                                },
                                [&indexOf](const ThresholdProcessing& threshold) -> EvaluationPlan {
                                    return ThresholdPlan{
                                        indexOf(threshold.input),
                                        threshold.parameters,
                                    };
                                },
                                [&indexOf](const PosteriseProcessing& posterise) -> EvaluationPlan {
                                    return PosterisePlan{
                                        indexOf(posterise.input),
                                        posterise.parameters,
                                    };
                                },
                                [&indexOf](const ColourRampProcessing& ramp) -> EvaluationPlan {
                                    return ColourRampPlan{
                                        indexOf(ramp.input),
                                        ramp.parameters,
                                    };
                                },
                                [&indexOf](const PaletteProcessing& palette) -> EvaluationPlan {
                                    return PalettePlan{
                                        indexOf(palette.input),
                                        palette.parameters,
                                    };
                                },
                                [&indexOf](const CompositeProcessing& composite) -> EvaluationPlan {
                                    return CompositePlan{
                                        indexOf(composite.background),
                                        indexOf(composite.source),
                                        composite.mask
                                            ? std::optional<std::size_t>{indexOf(*composite.mask)}
                                            : std::nullopt,
                                        composite.mode,
                                        composite.opacity,
                                    };
                                },
                                [&indexOf](
                                    const SurfaceFilterProcessing& filter) -> EvaluationPlan {
                                    return SurfaceFilterPlan{
                                        indexOf(filter.input),
                                        filter.parameters,
                                    };
                                },
                                [&indexOf](
                                    const InkContourProcessing& contour) -> EvaluationPlan {
                                    return InkContourPlan{
                                        indexOf(contour.input),
                                        contour.parameters,
                                    };
                                },
                                [&indexOf](
                                    const RegionFieldProcessing& field) -> EvaluationPlan {
                                    return RegionFieldPlan{
                                        indexOf(field.input),
                                        field.parameters,
                                    };
                                },
                            },
                            processing.operation);
                    },
                    [](const MaskNode& mask) -> EvaluationPlan {
                        return MaskPlan{mask.transform, mask.mask};
                    },
                    [&indexOf, this](const OutputNode& output) -> EvaluationPlan {
                        const auto input = indexOf(output.input);
                        outputSources_[materialOutputIndex(output.output)] = input;
                        return OutputPlan{input};
                    },
                },
                node));
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
        return plans_.size();
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
                [&context](const GeneratorPlan& generator) {
                    const auto coordinates = transformCoordinates(generator.transform, context);
                    const EvaluationContext transformed{
                        context.material,
                        coordinates.u,
                        coordinates.v,
                    };
                    return evaluateOperation(generator.operation, transformed, {});
                },
                [this, &context, cacheResult](const LevelsPlan& levels) {
                    const auto input = evaluateNode(levels.input, context, cacheResult);
                    return evaluateOperation(
                        LayerOperation{levels.parameters},
                        context,
                        input);
                },
                [this, &context, cacheResult](const ThresholdPlan& threshold) {
                    const auto input = evaluateNode(threshold.input, context, cacheResult);
                    return evaluateOperation(
                        LayerOperation{threshold.parameters},
                        context,
                        input);
                },
                [this, &context, cacheResult](const PosterisePlan& posterise) {
                    const auto input = evaluateNode(posterise.input, context, cacheResult);
                    return evaluateOperation(
                        LayerOperation{posterise.parameters},
                        context,
                        input);
                },
                [this, &context, cacheResult](const ColourRampPlan& ramp) {
                    const auto input = evaluateNode(ramp.input, context, cacheResult);
                    return evaluateOperation(
                        LayerOperation{ramp.parameters},
                        context,
                        input);
                },
                [this, &context, cacheResult](const PalettePlan& palette) {
                    const auto input = evaluateNode(palette.input, context, cacheResult);
                    return evaluateOperation(
                        LayerOperation{palette.parameters},
                        context,
                        input);
                },
                [this, &context, cacheResult](const CompositePlan& composite) {
                    const auto background = evaluateNode(
                        composite.background,
                        context,
                        cacheResult);
                    const auto source = evaluateNode(
                        composite.source,
                        context,
                        cacheResult);
                    const double mask = composite.mask
                        ? evaluateNode(*composite.mask, context, cacheResult).scalar
                        : 1.0;
                    return compositeSamples(
                        background,
                        source,
                        composite.mode,
                        composite.opacity * mask);
                },
                [this, &context, cacheResult](const SurfaceFilterPlan& filter) {
                    const auto centre = evaluateNode(filter.input, context, cacheResult);
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
                                filter.input,
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
                        return evaluateSurfaceFilter(filter.parameters, neighbourhood);
                    };
                    return EvaluatedSample{
                        affectsScalar(filter.parameters.target)
                            ? channel(&EvaluatedSample::scalar)
                            : centre.scalar,
                        affectsColour(filter.parameters.target)
                            ? channel(&EvaluatedSample::red)
                            : centre.red,
                        affectsColour(filter.parameters.target)
                            ? channel(&EvaluatedSample::green)
                            : centre.green,
                        affectsColour(filter.parameters.target)
                            ? channel(&EvaluatedSample::blue)
                            : centre.blue,
                        centre.alpha,
                        centre.region,
                    };
                },
                [this, &context, cacheResult](const InkContourPlan& contour) {
                    const auto centre = evaluateNode(contour.input, context, cacheResult);
                    std::array<double, 9> scalarSamples{};
                    scalarSamples[0] = centre.scalar;
                    if (contour.parameters.radius == 0.0) {
                        std::fill(scalarSamples.begin() + 1, scalarSamples.end(), centre.scalar);
                    } else {
                        const double radius = contour.parameters.radius;
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
                            scalarSamples[sampleIndex + 1] = evaluateNode(
                                contour.input,
                                neighbour,
                                false).scalar;
                        }
                    }
                    const double edge = *std::max_element(
                        scalarSamples.begin(), scalarSamples.end()) -
                        *std::min_element(scalarSamples.begin(), scalarSamples.end());
                    const double halfSoftness = contour.parameters.softness * 0.5;
                    double amount = smoothStep(
                        contour.parameters.threshold - halfSoftness,
                        contour.parameters.threshold + halfSoftness,
                        edge);
                    if (contour.parameters.inverted) {
                        amount = 1.0 - amount;
                    }
                    amount *= contour.parameters.strength *
                        (static_cast<double>(contour.parameters.colour.alpha) / 255.0);
                    const auto blend = [amount](double from, std::uint8_t to) {
                        const double target = static_cast<double>(to) / 255.0;
                        return from + (target - from) * amount;
                    };
                    return EvaluatedSample{
                        centre.scalar,
                        blend(centre.red, contour.parameters.colour.red),
                        blend(centre.green, contour.parameters.colour.green),
                        blend(centre.blue, contour.parameters.colour.blue),
                        centre.alpha,
                        centre.region,
                    };
                },
                [this, &context, cacheResult](const RegionFieldPlan& field) {
                    const auto input = evaluateNode(field.input, context, cacheResult);
                    return evaluateOperation(
                        LayerOperation{field.parameters},
                        context,
                        input);
                },
                [&context](const MaskPlan& mask) {
                    const auto coordinates = transformCoordinates(mask.transform, context);
                    const EvaluationContext transformed{
                        context.material,
                        coordinates.u,
                        coordinates.v,
                    };
                    const double value = evaluateLayerMask(mask.mask, transformed);
                    return EvaluatedSample{value, value, value, value, 1.0, {}};
                },
                [this, &context, cacheResult](const OutputPlan& output) {
                    return evaluateNode(output.input, context, cacheResult);
                },
            },
            plans_[index]);
        if (cacheResult) {
            samples_[index] = result;
            sampleStamps_[index] = currentStamp_;
        }
        return result;
    }

    const Material& material_;
    std::vector<EvaluationPlan> plans_;
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
