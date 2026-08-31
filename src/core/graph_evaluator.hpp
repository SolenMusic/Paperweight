#pragma once

#include <cstddef>
#include <memory>

#include <paperweight/evaluation.hpp>
#include <paperweight/graph.hpp>
#include <paperweight/material.hpp>
#include <paperweight/output.hpp>

namespace paperweight::detail {

class GraphEvaluator {
public:
    GraphEvaluator(const Material& material, const MaterialGraph& graph);
    ~GraphEvaluator();

    GraphEvaluator(const GraphEvaluator&) = delete;
    GraphEvaluator& operator=(const GraphEvaluator&) = delete;
    GraphEvaluator(GraphEvaluator&&) noexcept;
    GraphEvaluator& operator=(GraphEvaluator&&) noexcept;

    [[nodiscard]] EvaluatedSample evaluate(
        MaterialOutput output,
        double u,
        double v);
    [[nodiscard]] std::size_t nodeCount() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace paperweight::detail
