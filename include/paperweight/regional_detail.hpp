#pragma once

#include <paperweight/evaluation.hpp>
#include <paperweight/layer.hpp>
#include <paperweight/material.hpp>

namespace paperweight {

struct RegionalDetailSample {
    double macro{};
    double meso{};
    double micro{};
    double centreGradient{};
    double directionalGradient{};
    double planarGradient{};
    double mottling{};
    double grain{};
    double directionalStrokes{};
    double outerShadow{};
    double bevel{};
    double body{};
    double innerHighlight{};
    double wear{};
    double combined{};
    double palette{};

    friend constexpr bool operator==(
        const RegionalDetailSample&,
        const RegionalDetailSample&) = default;
};

[[nodiscard]] RegionalDetailSample evaluateRegionalDetailFields(
    const RegionalDetailOperation& operation,
    const Material& material,
    const RegionSample& region,
    double inputCoverage,
    double u,
    double v,
    std::uint64_t groupScopeKey = 0);

[[nodiscard]] EvaluatedSample evaluateRegionalDetail(
    const RegionalDetailOperation& operation,
    const EvaluationContext& context,
    const EvaluatedSample& input,
    std::uint64_t groupScopeKey = 0);

} // namespace paperweight
