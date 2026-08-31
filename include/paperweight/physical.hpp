#pragma once

namespace paperweight {

struct PhysicalSize {
    double widthMetres{1.0};
    double heightMetres{1.0};

    friend constexpr bool operator==(const PhysicalSize&, const PhysicalSize&) = default;
};

struct PhysicalLimits {
    static constexpr double minimumMetres = 0.000001;
    static constexpr double maximumMetres = 1000000.0;
    static constexpr unsigned int maximumRepeats = 4096;
};

} // namespace paperweight
