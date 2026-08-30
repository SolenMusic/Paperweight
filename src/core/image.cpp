#include <paperweight/image.hpp>

#include <limits>
#include <stdexcept>

namespace paperweight {
namespace {

std::size_t checkedPixelCount(std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0) {
        throw std::invalid_argument("image dimensions must be greater than zero");
    }

    const auto widthValue = static_cast<std::size_t>(width);
    const auto heightValue = static_cast<std::size_t>(height);
    if (heightValue > std::numeric_limits<std::size_t>::max() / widthValue) {
        throw std::length_error("image dimensions overflow addressable storage");
    }

    const auto count = widthValue * heightValue;
    if (count > std::vector<Rgba8>{}.max_size()) {
        throw std::length_error("image dimensions exceed maximum storage");
    }
    return count;
}

} // namespace

Image::Image(std::uint32_t width, std::uint32_t height, Rgba8 fill)
    : width_(width)
    , height_(height)
    , pixels_(checkedPixelCount(width, height), fill)
{
}

std::size_t Image::bytesPerRow() const noexcept
{
    return static_cast<std::size_t>(width_) * sizeof(Rgba8);
}

std::span<Rgba8> Image::row(std::uint32_t y)
{
    if (y >= height_) {
        throw std::out_of_range("image row is outside the image");
    }
    return {pixels_.data() + static_cast<std::size_t>(y) * width_, width_};
}

std::span<const Rgba8> Image::row(std::uint32_t y) const
{
    if (y >= height_) {
        throw std::out_of_range("image row is outside the image");
    }
    return {pixels_.data() + static_cast<std::size_t>(y) * width_, width_};
}

} // namespace paperweight
