#pragma once

#include <paperweight/graph.hpp>
#include <paperweight/layer.hpp>
#include <paperweight/material.hpp>
#include <paperweight/output.hpp>
#include <paperweight/region.hpp>

namespace paperweight {

struct EvaluatedSample {
    double scalar{};
    double red{};
    double green{};
    double blue{};
    double alpha{};
    RegionSample region;

    friend constexpr bool operator==(const EvaluatedSample&, const EvaluatedSample&) = default;
};

struct EvaluationContext {
    const Material& material;
    double u;
    double v;
    MaterialOutput output{MaterialOutput::colour};
};

struct EvaluatedCoordinates {
    double u;
    double v;

    friend constexpr bool operator==(
        const EvaluatedCoordinates&,
        const EvaluatedCoordinates&) = default;
};

[[nodiscard]] EvaluatedCoordinates transformCoordinates(
    const CoordinateTransform& transform,
    const EvaluationContext& context);

[[nodiscard]] double evaluateLayerMask(
    const LayerMask& mask,
    const EvaluationContext& context);

[[nodiscard]] EvaluatedSample evaluateOperation(
    const LayerOperation& operation,
    const EvaluationContext& context,
    const EvaluatedSample& input);

[[nodiscard]] EvaluatedSample compositeSamples(
    const EvaluatedSample& background,
    const EvaluatedSample& source,
    CompositeMode mode,
    double opacity);

[[nodiscard]] EvaluatedSample evaluateMaterialSample(
    const Material& material,
    double u,
    double v);

[[nodiscard]] EvaluatedSample evaluateMaterialGraphSample(
    const Material& material,
    const MaterialGraph& graph,
    MaterialOutput output,
    double u,
    double v);

} // namespace paperweight
