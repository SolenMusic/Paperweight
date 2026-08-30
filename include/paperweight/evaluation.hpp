#pragma once

#include <paperweight/layer.hpp>
#include <paperweight/material.hpp>

namespace paperweight {

struct EvaluatedSample {
    double scalar{};
    double red{};
    double green{};
    double blue{};
    double alpha{};

    friend constexpr bool operator==(const EvaluatedSample&, const EvaluatedSample&) = default;
};

struct EvaluationContext {
    const Material& material;
    double u;
    double v;
};

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

} // namespace paperweight
