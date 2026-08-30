#pragma once

#import <AppKit/AppKit.h>

#include <paperweight/image.hpp>

namespace paperweight::macos {

[[nodiscard]] NSBitmapImageRep* makeBitmapRepresentation(const Image& image);
[[nodiscard]] NSData* makePngData(const Image& image);

} // namespace paperweight::macos
