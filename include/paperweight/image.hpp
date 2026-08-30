#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace paperweight {

enum class PixelFormat {
    rgba8Unorm,
};

struct Rgba8 {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{255};

    friend constexpr bool operator==(const Rgba8&, const Rgba8&) = default;
};

class Image {
public:
    Image(std::uint32_t width, std::uint32_t height, Rgba8 fill = {});

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] PixelFormat format() const noexcept { return PixelFormat::rgba8Unorm; }
    [[nodiscard]] std::size_t bytesPerRow() const noexcept;

    [[nodiscard]] std::span<Rgba8> pixels() noexcept { return pixels_; }
    [[nodiscard]] std::span<const Rgba8> pixels() const noexcept { return pixels_; }
    [[nodiscard]] std::span<Rgba8> row(std::uint32_t y);
    [[nodiscard]] std::span<const Rgba8> row(std::uint32_t y) const;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<Rgba8> pixels_;
};

static_assert(sizeof(Rgba8) == 4);

} // namespace paperweight
