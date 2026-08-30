#include "ImageBridge.hpp"

#include <paperweight/generator.hpp>

#include <array>
#include <cstring>
#include <iostream>
#include <variant>

int main()
{
    @autoreleasepool {
        paperweight::GenerationRequest request;
        request.width = 19;
        request.height = 11;
        for (const auto output : std::array{
                 paperweight::MaterialOutput::colour,
                 paperweight::MaterialOutput::height,
                 paperweight::MaterialOutput::normal,
                 paperweight::MaterialOutput::roughness,
             }) {
            request.output = output;
            const auto result = paperweight::generate(request);
            const auto* image = std::get_if<paperweight::Image>(&result);
            if (image == nullptr) {
                std::cerr << "generation failed\n";
                return 1;
            }

            NSBitmapImageRep* representation = paperweight::macos::makeBitmapRepresentation(*image);
            if (representation == nil || representation.pixelsWide != 19 ||
                representation.pixelsHigh != 11 || representation.bytesPerRow != 76) {
                std::cerr << "bitmap representation metadata differs\n";
                return 1;
            }
            if (std::memcmp(
                    representation.bitmapData,
                    image->pixels().data(),
                    image->pixels().size_bytes()) != 0) {
                std::cerr << "bitmap representation pixels differ\n";
                return 1;
            }

            NSData* png = paperweight::macos::makePngData(*image);
            NSBitmapImageRep* decoded =
                png == nil ? nil : [[NSBitmapImageRep alloc] initWithData:png];
            if (decoded == nil || decoded.pixelsWide != 19 || decoded.pixelsHigh != 11) {
                std::cerr << "PNG did not decode with expected dimensions\n";
                return 1;
            }
            if (decoded.bytesPerRow < 76) {
                std::cerr << "decoded PNG row is unexpectedly short\n";
                return 1;
            }
            for (NSInteger y = 0; y < decoded.pixelsHigh; ++y) {
                const auto* expected = reinterpret_cast<const unsigned char*>(image->row(
                    static_cast<std::uint32_t>(y)).data());
                const auto* actual = decoded.bitmapData + y * decoded.bytesPerRow;
                if (std::memcmp(actual, expected, image->bytesPerRow()) != 0) {
                    std::cerr << "decoded PNG pixels differ on row " << y << '\n';
                    return 1;
                }
            }
        }
    }
    std::cout << "Paperweight macOS bridge tests passed\n";
    return 0;
}
