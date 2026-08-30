#include "ImageBridge.hpp"

#include <cstring>

namespace paperweight::macos {

NSBitmapImageRep* makeBitmapRepresentation(const Image& image)
{
    auto* representation = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:nullptr
                      pixelsWide:static_cast<NSInteger>(image.width())
                      pixelsHigh:static_cast<NSInteger>(image.height())
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSCalibratedRGBColorSpace
                     bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
                      bytesPerRow:static_cast<NSInteger>(image.bytesPerRow())
                     bitsPerPixel:32];
    if (representation == nil || representation.bitmapData == nullptr) {
        return nil;
    }
    std::memcpy(representation.bitmapData, image.pixels().data(), image.pixels().size_bytes());
    return representation;
}

NSData* makePngData(const Image& image)
{
    auto* representation = makeBitmapRepresentation(image);
    if (representation == nil) {
        return nil;
    }
    return [representation representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
}

} // namespace paperweight::macos
