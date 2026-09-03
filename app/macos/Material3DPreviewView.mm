#import "Material3DPreviewView.hpp"

#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>
#include <vector>

#include <simd/simd.h>

namespace {

constexpr float previewPi = std::numbers::pi_v<float>;

struct PreviewVertex {
    simd_float3 position;
    simd_float3 normal;
    simd_float4 tangent;
    simd_float2 uv;
};

static_assert(sizeof(PreviewVertex) == 64);

struct PreviewUniforms {
    simd_float4x4 modelViewProjection;
    simd_float4x4 model;
    simd_float4x4 normalMatrix;
    simd_float4 cameraPosition;
    simd_float4 lightDirection;
    simd_float4 settings;
    simd_float4 mapSettings;
    simd_float4 toonSettings;
    simd_float4 opticalSettings;
    simd_float4 environmentSettings;
};

struct MeshData {
    std::vector<PreviewVertex> vertices;
    std::vector<std::uint32_t> indices;
};

simd_float4x4 identityMatrix()
{
    return matrix_identity_float4x4;
}

simd_float4x4 perspectiveMatrix(float verticalRadians, float aspect, float nearZ, float farZ)
{
    const float y = 1.0F / std::tan(verticalRadians * 0.5F);
    const float x = y / std::max(aspect, 0.01F);
    const float z = farZ / (nearZ - farZ);
    return simd_matrix_from_rows(
        simd_make_float4(x, 0.0F, 0.0F, 0.0F),
        simd_make_float4(0.0F, y, 0.0F, 0.0F),
        simd_make_float4(0.0F, 0.0F, z, nearZ * z),
        simd_make_float4(0.0F, 0.0F, -1.0F, 0.0F));
}

simd_float4x4 lookAtMatrix(simd_float3 eye, simd_float3 target, simd_float3 up)
{
    const simd_float3 forward = simd_normalize(target - eye);
    const simd_float3 right = simd_normalize(simd_cross(forward, up));
    const simd_float3 correctedUp = simd_cross(right, forward);
    return simd_matrix_from_rows(
        simd_make_float4(right.x, right.y, right.z, -simd_dot(right, eye)),
        simd_make_float4(correctedUp.x, correctedUp.y, correctedUp.z, -simd_dot(correctedUp, eye)),
        simd_make_float4(-forward.x, -forward.y, -forward.z, simd_dot(forward, eye)),
        simd_make_float4(0.0F, 0.0F, 0.0F, 1.0F));
}

void appendGrid(
    MeshData& mesh,
    std::uint32_t columns,
    std::uint32_t rows,
    const std::function<PreviewVertex(float, float)>& vertexAt)
{
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (std::uint32_t row = 0; row <= rows; ++row) {
        const float v = static_cast<float>(row) / static_cast<float>(rows);
        for (std::uint32_t column = 0; column <= columns; ++column) {
            const float u = static_cast<float>(column) / static_cast<float>(columns);
            mesh.vertices.push_back(vertexAt(u, v));
        }
    }
    const std::uint32_t stride = columns + 1;
    for (std::uint32_t row = 0; row < rows; ++row) {
        for (std::uint32_t column = 0; column < columns; ++column) {
            const auto topLeft = base + row * stride + column;
            const auto bottomLeft = topLeft + stride;
            mesh.indices.insert(mesh.indices.end(), {
                topLeft,
                topLeft + 1,
                bottomLeft,
                topLeft + 1,
                bottomLeft + 1,
                bottomLeft,
            });
        }
    }
}

MeshData makePlane()
{
    MeshData mesh;
    appendGrid(mesh, 96, 96, [](float u, float v) {
        return PreviewVertex{
            {u * 1.8F - 0.9F, 0.0F, 0.9F - v * 1.8F},
            {0.0F, 1.0F, 0.0F},
            {1.0F, 0.0F, 0.0F, 1.0F},
            {u, v},
        };
    });
    return mesh;
}

MeshData makeSphere()
{
    MeshData mesh;
    appendGrid(mesh, 96, 64, [](float u, float v) {
        const float theta = u * 2.0F * previewPi;
        const float phi = v * previewPi;
        const float sinePhi = std::sin(phi);
        const float textureU = (v == 0.0F || v == 1.0F) ? 0.5F : u;
        const simd_float3 normal{
            sinePhi * std::cos(theta),
            std::cos(phi),
            sinePhi * std::sin(theta),
        };
        return PreviewVertex{
            normal * 0.82F,
            normal,
            {-std::sin(theta), 0.0F, std::cos(theta), 1.0F},
            {textureU, v},
        };
    });
    return mesh;
}

MeshData makeCube()
{
    MeshData mesh;
    struct Face {
        simd_float3 normal;
        simd_float3 tangent;
    };
    constexpr std::array<Face, 6> faces{{
        {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
        {{-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}},
        {{0.0F, -1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}},
        {{0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F}},
        {{0.0F, 0.0F, -1.0F}, {-1.0F, 0.0F, 0.0F}},
    }};
    for (const auto& face : faces) {
        const simd_float3 bitangent = simd_cross(face.normal, face.tangent);
        appendGrid(mesh, 40, 40, [=](float u, float v) {
            const simd_float3 position = face.normal * 0.68F +
                face.tangent * ((u - 0.5F) * 1.36F) +
                bitangent * ((v - 0.5F) * 1.36F);
            return PreviewVertex{
                position,
                face.normal,
                {face.tangent.x, face.tangent.y, face.tangent.z, 1.0F},
                {u, v},
            };
        });
    }
    return mesh;
}

void appendCylinderCap(MeshData& mesh, bool top)
{
    constexpr std::uint32_t segments = 96;
    constexpr std::uint32_t rings = 24;
    const simd_float3 normal = top
        ? simd_make_float3(0.0F, 1.0F, 0.0F)
        : simd_make_float3(0.0F, -1.0F, 0.0F);
    const simd_float3 tangent{1.0F, 0.0F, 0.0F};
    const float y = top ? 0.8F : -0.8F;
    const float tangentSign = 1.0F;
    const auto centre = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.0F, y, 0.0F}, normal,
        {tangent.x, tangent.y, tangent.z, tangentSign}, {0.5F, 0.5F}});

    std::uint32_t previousStart = 0;
    for (std::uint32_t ring = 1; ring <= rings; ++ring) {
        const float radius = 0.7F * static_cast<float>(ring) / static_cast<float>(rings);
        const auto ringStart = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::uint32_t segment = 0; segment <= segments; ++segment) {
            const float angle = static_cast<float>(segment) / static_cast<float>(segments) * 2.0F * previewPi;
            const float x = std::cos(angle) * radius;
            const float z = std::sin(angle) * radius;
            const simd_float2 uv = top
                ? simd_make_float2(0.5F + x / 1.4F, 0.5F - z / 1.4F)
                : simd_make_float2(0.5F + x / 1.4F, 0.5F + z / 1.4F);
            mesh.vertices.push_back({{x, y, z}, normal,
                {tangent.x, tangent.y, tangent.z, tangentSign}, uv});
        }
        if (ring == 1) {
            for (std::uint32_t segment = 0; segment < segments; ++segment) {
                if (top) {
                    mesh.indices.insert(mesh.indices.end(), {centre, ringStart + segment + 1, ringStart + segment});
                } else {
                    mesh.indices.insert(mesh.indices.end(), {centre, ringStart + segment, ringStart + segment + 1});
                }
            }
        } else {
            for (std::uint32_t segment = 0; segment < segments; ++segment) {
                const auto innerA = previousStart + segment;
                const auto innerB = previousStart + segment + 1;
                const auto outerA = ringStart + segment;
                const auto outerB = ringStart + segment + 1;
                if (top) {
                    mesh.indices.insert(mesh.indices.end(), {innerA, outerB, outerA, innerA, innerB, outerB});
                } else {
                    mesh.indices.insert(mesh.indices.end(), {innerA, outerA, outerB, innerA, outerB, innerB});
                }
            }
        }
        previousStart = ringStart;
    }
}

MeshData makeCylinder()
{
    MeshData mesh;
    appendGrid(mesh, 96, 48, [](float u, float v) {
        const float angle = -u * 2.0F * previewPi;
        const simd_float3 normal{std::cos(angle), 0.0F, std::sin(angle)};
        const simd_float3 tangent{std::sin(angle), 0.0F, -std::cos(angle)};
        return PreviewVertex{
            {normal.x * 0.7F, v * 1.6F - 0.8F, normal.z * 0.7F},
            normal,
            {tangent.x, tangent.y, tangent.z, 1.0F},
            {u, v},
        };
    });
    appendCylinderCap(mesh, true);
    appendCylinderCap(mesh, false);
    return mesh;
}

MeshData makeMesh(PWPreviewShape shape)
{
    switch (shape) {
    case PWPreviewShapePlane:
        return makePlane();
    case PWPreviewShapeSphere:
        return makeSphere();
    case PWPreviewShapeCube:
        return makeCube();
    case PWPreviewShapeCylinder:
        return makeCylinder();
    }
    return makeSphere();
}

} // namespace

@interface Material3DPreviewView () <MTKViewDelegate>
@end

@implementation Material3DPreviewView {
    id<MTLCommandQueue> commandQueue_;
    id<MTLRenderPipelineState> pipeline_;
    id<MTLDepthStencilState> depthState_;
    id<MTLBuffer> vertexBuffer_;
    id<MTLBuffer> indexBuffer_;
    NSUInteger indexCount_;
    id<MTLTexture> colourTexture_;
    id<MTLTexture> heightTexture_;
    id<MTLTexture> normalTexture_;
    id<MTLTexture> roughnessTexture_;
    id<MTLTexture> metalnessTexture_;
    BOOL rendererAvailable_;
    float cameraYaw_;
    float cameraPitch_;
    float cameraDistance_;
    NSPoint previousDragPoint_;
    CFTimeInterval previousAnimationTime_;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    self = [super initWithFrame:frameRect device:device];
    if (self) {
        _previewShape = PWPreviewShapeSphere;
        _lightAzimuthDegrees = 35.0;
        _lightElevationDegrees = 38.0;
        _lightIntensity = 1.0;
        _ambientIntensity = 0.18;
        _environmentPreset = PWPreviewEnvironmentChromeStudio;
        _environmentIntensity = 1.0;
        _environmentRotationDegrees = 0.0;
        _dielectricIor = 1.5;
        _displacementStrength = 0.04;
        _previewNormalStrength = 1.0;
        _toonBandCount = 3.0;
        _toonSpecularThreshold = 0.55;
        _toonRimStrength = 0.18;
        _animationSpeed = 0.25;
        _colourEnabled = YES;
        _heightEnabled = YES;
        _normalEnabled = YES;
        _roughnessEnabled = YES;
        _metalnessEnabled = YES;
        cameraYaw_ = 0.7F;
        cameraPitch_ = 0.38F;
        cameraDistance_ = 2.7F;
        self.translatesAutoresizingMaskIntoConstraints = NO;
        self.focusRingType = NSFocusRingTypeNone;
        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        self.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
        self.clearColor = MTLClearColorMake(0.035, 0.042, 0.052, 1.0);
        self.preferredFramesPerSecond = 60;
        self.enableSetNeedsDisplay = YES;
        self.paused = YES;
        self.delegate = self;
        [self initialiseRenderer];
    }
    return self;
}

- (BOOL)isRendererAvailable
{
    return rendererAvailable_;
}

- (void)initialiseRenderer
{
    if (self.device == nil) {
        rendererAvailable_ = NO;
        return;
    }
    commandQueue_ = [self.device newCommandQueue];
    NSString* shaderPath = [NSBundle.mainBundle pathForResource:@"Material3DPreviewShaders"
                                                         ofType:@"metal"
                                                    inDirectory:@"Shaders"];
    NSError* sourceError = nil;
    NSString* source = shaderPath == nil
        ? nil
        : [NSString stringWithContentsOfFile:shaderPath
                                    encoding:NSUTF8StringEncoding
                                       error:&sourceError];
    if (source == nil) {
        NSLog(@"Paperweight Metal shader source unavailable: %@", sourceError);
        rendererAvailable_ = NO;
        return;
    }

    MTLCompileOptions* compileOptions = [[MTLCompileOptions alloc] init];
    compileOptions.fastMathEnabled = NO;
    NSError* libraryError = nil;
    id<MTLLibrary> library = [self.device newLibraryWithSource:source
                                                       options:compileOptions
                                                         error:&libraryError];
    if (library == nil) {
        NSLog(@"Paperweight Metal shaders failed to compile: %@", libraryError);
        rendererAvailable_ = NO;
        return;
    }
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"paperweightPreviewVertex"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"paperweightPreviewFragment"];
    MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.label = @"Paperweight Material Preview";
    descriptor.vertexFunction = vertexFunction;
    descriptor.fragmentFunction = fragmentFunction;
    descriptor.colorAttachments[0].pixelFormat = self.colorPixelFormat;
    descriptor.depthAttachmentPixelFormat = self.depthStencilPixelFormat;
    NSError* pipelineError = nil;
    pipeline_ = [self.device newRenderPipelineStateWithDescriptor:descriptor error:&pipelineError];
    if (pipeline_ == nil) {
        NSLog(@"Paperweight Metal pipeline failed: %@", pipelineError);
        rendererAvailable_ = NO;
        return;
    }

    MTLDepthStencilDescriptor* depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
    depthDescriptor.depthCompareFunction = MTLCompareFunctionLess;
    depthDescriptor.depthWriteEnabled = YES;
    depthState_ = [self.device newDepthStencilStateWithDescriptor:depthDescriptor];
    rendererAvailable_ = commandQueue_ != nil && depthState_ != nil;
    [self createFallbackTextures];
    [self rebuildMesh];
}

- (id<MTLTexture>)textureWithWidth:(NSUInteger)width
                            height:(NSUInteger)height
                       pixelFormat:(MTLPixelFormat)pixelFormat
                             bytes:(const void*)bytes
                       bytesPerRow:(NSUInteger)bytesPerRow
{
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:pixelFormat
                                    width:width
                                   height:height
                                mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModeManaged;
    id<MTLTexture> texture = [self.device newTextureWithDescriptor:descriptor];
    [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
               mipmapLevel:0
                 withBytes:bytes
               bytesPerRow:bytesPerRow];
    return texture;
}

- (void)createFallbackTextures
{
    const std::array<std::uint8_t, 4> colour{{132, 132, 132, 255}};
    const std::array<std::uint8_t, 4> height{{128, 128, 128, 255}};
    const std::array<std::uint8_t, 4> normal{{128, 128, 255, 255}};
    const std::array<std::uint8_t, 4> roughness{{128, 128, 128, 255}};
    const std::array<std::uint8_t, 4> metalness{{0, 0, 0, 255}};
    colourTexture_ = [self textureWithWidth:1 height:1
                               pixelFormat:MTLPixelFormatRGBA8Unorm_sRGB
                                     bytes:colour.data() bytesPerRow:4];
    heightTexture_ = [self textureWithWidth:1 height:1
                               pixelFormat:MTLPixelFormatRGBA8Unorm
                                     bytes:height.data() bytesPerRow:4];
    normalTexture_ = [self textureWithWidth:1 height:1
                               pixelFormat:MTLPixelFormatRGBA8Unorm
                                     bytes:normal.data() bytesPerRow:4];
    roughnessTexture_ = [self textureWithWidth:1 height:1
                                  pixelFormat:MTLPixelFormatRGBA8Unorm
                                        bytes:roughness.data() bytesPerRow:4];
    metalnessTexture_ = [self textureWithWidth:1 height:1
                                  pixelFormat:MTLPixelFormatRGBA8Unorm
                                        bytes:metalness.data() bytesPerRow:4];
}

- (void)setColourImage:(const paperweight::Image&)colour
            heightImage:(const paperweight::Image&)height
            normalImage:(const paperweight::Image&)normal
         roughnessImage:(const paperweight::Image&)roughness
          metalnessImage:(const paperweight::Image&)metalness
{
    if (!rendererAvailable_) {
        return;
    }
    colourTexture_ = [self textureWithWidth:colour.width()
                                      height:colour.height()
                                 pixelFormat:MTLPixelFormatRGBA8Unorm_sRGB
                                       bytes:colour.pixels().data()
                                 bytesPerRow:colour.bytesPerRow()];
    heightTexture_ = [self textureWithWidth:height.width()
                                      height:height.height()
                                 pixelFormat:MTLPixelFormatRGBA8Unorm
                                       bytes:height.pixels().data()
                                 bytesPerRow:height.bytesPerRow()];
    normalTexture_ = [self textureWithWidth:normal.width()
                                      height:normal.height()
                                 pixelFormat:MTLPixelFormatRGBA8Unorm
                                       bytes:normal.pixels().data()
                                 bytesPerRow:normal.bytesPerRow()];
    roughnessTexture_ = [self textureWithWidth:roughness.width()
                                         height:roughness.height()
                                    pixelFormat:MTLPixelFormatRGBA8Unorm
                                          bytes:roughness.pixels().data()
                                    bytesPerRow:roughness.bytesPerRow()];
    metalnessTexture_ = [self textureWithWidth:metalness.width()
                                         height:metalness.height()
                                    pixelFormat:MTLPixelFormatRGBA8Unorm
                                          bytes:metalness.pixels().data()
                                    bytesPerRow:metalness.bytesPerRow()];
    [self requestDraw];
}

- (void)clearMaterialImages
{
    if (self.device != nil) {
        [self createFallbackTextures];
        [self requestDraw];
    }
}

- (void)rebuildMesh
{
    if (!rendererAvailable_) {
        return;
    }
    const auto mesh = makeMesh(self.previewShape);
    vertexBuffer_ = [self.device newBufferWithBytes:mesh.vertices.data()
                                           length:mesh.vertices.size() * sizeof(PreviewVertex)
                                          options:MTLResourceStorageModeManaged];
    indexBuffer_ = [self.device newBufferWithBytes:mesh.indices.data()
                                          length:mesh.indices.size() * sizeof(std::uint32_t)
                                         options:MTLResourceStorageModeManaged];
    indexCount_ = mesh.indices.size();
    [self requestDraw];
}

- (void)requestDraw
{
    if (!self.animationRunning) {
        [self setNeedsDisplay:YES];
    }
}

- (void)setPreviewShape:(PWPreviewShape)previewShape
{
    if (_previewShape == previewShape) {
        return;
    }
    _previewShape = previewShape;
    [self rebuildMesh];
}

- (void)setAnimationRunning:(BOOL)animationRunning
{
    _animationRunning = animationRunning;
    previousAnimationTime_ = CACurrentMediaTime();
    self.enableSetNeedsDisplay = !animationRunning;
    self.paused = !animationRunning;
    if (!animationRunning) {
        [self setNeedsDisplay:YES];
    }
}

- (void)setLightAzimuthDegrees:(double)value { _lightAzimuthDegrees = value; [self requestDraw]; }
- (void)setLightElevationDegrees:(double)value { _lightElevationDegrees = value; [self requestDraw]; }
- (void)setLightIntensity:(double)value { _lightIntensity = value; [self requestDraw]; }
- (void)setAmbientIntensity:(double)value { _ambientIntensity = value; [self requestDraw]; }
- (void)setEnvironmentPreset:(PWPreviewEnvironment)value { _environmentPreset = value; [self requestDraw]; }
- (void)setEnvironmentIntensity:(double)value { _environmentIntensity = value; [self requestDraw]; }
- (void)setEnvironmentRotationDegrees:(double)value { _environmentRotationDegrees = value; [self requestDraw]; }
- (void)setDielectricIor:(double)value { _dielectricIor = value; [self requestDraw]; }
- (void)setDisplacementStrength:(double)value { _displacementStrength = value; [self requestDraw]; }
- (void)setPreviewNormalStrength:(double)value { _previewNormalStrength = value; [self requestDraw]; }
- (void)setToonLightingEnabled:(BOOL)value { _toonLightingEnabled = value; [self requestDraw]; }
- (void)setToonBandCount:(double)value { _toonBandCount = value; [self requestDraw]; }
- (void)setToonSpecularThreshold:(double)value { _toonSpecularThreshold = value; [self requestDraw]; }
- (void)setToonRimStrength:(double)value { _toonRimStrength = value; [self requestDraw]; }
- (void)setAnimationPhase:(double)value { _animationPhase = value - std::floor(value); [self requestDraw]; }
- (void)setAnimationSpeed:(double)value { _animationSpeed = value; }
- (void)setColourEnabled:(BOOL)value { _colourEnabled = value; [self requestDraw]; }
- (void)setHeightEnabled:(BOOL)value { _heightEnabled = value; [self requestDraw]; }
- (void)setNormalEnabled:(BOOL)value { _normalEnabled = value; [self requestDraw]; }
- (void)setRoughnessEnabled:(BOOL)value { _roughnessEnabled = value; [self requestDraw]; }
- (void)setMetalnessEnabled:(BOOL)value { _metalnessEnabled = value; [self requestDraw]; }

- (void)resetCamera
{
    cameraYaw_ = 0.7F;
    cameraPitch_ = 0.38F;
    cameraDistance_ = 2.7F;
    [self requestDraw];
}

- (void)mouseDown:(NSEvent*)event
{
    previousDragPoint_ = [self convertPoint:event.locationInWindow fromView:nil];
}

- (void)mouseDragged:(NSEvent*)event
{
    const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    cameraYaw_ += static_cast<float>(point.x - previousDragPoint_.x) * 0.009F;
    cameraPitch_ = std::clamp(
        cameraPitch_ + static_cast<float>(point.y - previousDragPoint_.y) * 0.009F,
        -1.35F,
        1.35F);
    previousDragPoint_ = point;
    [self requestDraw];
}

- (void)scrollWheel:(NSEvent*)event
{
    cameraDistance_ = std::clamp(
        cameraDistance_ * std::exp(static_cast<float>(event.scrollingDeltaY) * 0.018F),
        1.35F,
        6.0F);
    [self requestDraw];
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size
{
    static_cast<void>(view);
    static_cast<void>(size);
    [self requestDraw];
}

- (void)drawInMTKView:(MTKView*)view
{
    if (!rendererAvailable_ || pipeline_ == nil || indexCount_ == 0) {
        return;
    }
    if (self.animationRunning) {
        const CFTimeInterval now = CACurrentMediaTime();
        const double delta = std::clamp(now - previousAnimationTime_, 0.0, 0.1);
        previousAnimationTime_ = now;
        _animationPhase = std::fmod(_animationPhase + delta * self.animationSpeed, 1.0);
    }

    MTLRenderPassDescriptor* renderPass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (renderPass == nil || drawable == nil) {
        return;
    }

    const float cosinePitch = std::cos(cameraPitch_);
    const simd_float3 eye{
        std::sin(cameraYaw_) * cosinePitch * cameraDistance_,
        std::sin(cameraPitch_) * cameraDistance_,
        std::cos(cameraYaw_) * cosinePitch * cameraDistance_,
    };
    const float aspect = static_cast<float>(view.drawableSize.width /
        std::max(view.drawableSize.height, 1.0));
    const simd_float4x4 model = identityMatrix();
    const simd_float4x4 viewMatrix = lookAtMatrix(
        eye,
        simd_make_float3(0.0F, 0.0F, 0.0F),
        simd_make_float3(0.0F, 1.0F, 0.0F));
    const simd_float4x4 projection = perspectiveMatrix(
        46.0F * previewPi / 180.0F, aspect, 0.05F, 30.0F);

    const float lightAzimuth = static_cast<float>(
        (self.lightAzimuthDegrees / 360.0 + self.animationPhase) * 2.0 * std::numbers::pi);
    const float lightElevation = static_cast<float>(self.lightElevationDegrees * std::numbers::pi / 180.0);
    const float lightCosine = std::cos(lightElevation);
    const simd_float3 lightDirection{
        std::sin(lightAzimuth) * lightCosine,
        std::sin(lightElevation),
        std::cos(lightAzimuth) * lightCosine,
    };
    const PreviewUniforms uniforms{
        simd_mul(projection, simd_mul(viewMatrix, model)),
        model,
        identityMatrix(),
        {eye.x, eye.y, eye.z, 1.0F},
        {lightDirection.x, lightDirection.y, lightDirection.z, 0.0F},
        {
            static_cast<float>(self.lightIntensity),
            static_cast<float>(self.ambientIntensity),
            static_cast<float>(self.previewNormalStrength),
            static_cast<float>(self.displacementStrength),
        },
        {
            self.colourEnabled ? 1.0F : 0.0F,
            self.heightEnabled ? 1.0F : 0.0F,
            self.normalEnabled ? 1.0F : 0.0F,
            self.roughnessEnabled ? 1.0F : 0.0F,
        },
        {
            self.toonLightingEnabled ? 1.0F : 0.0F,
            static_cast<float>(self.toonBandCount),
            static_cast<float>(self.toonSpecularThreshold),
            static_cast<float>(self.toonRimStrength),
        },
        {
            self.metalnessEnabled ? 1.0F : 0.0F,
            static_cast<float>(self.dielectricIor),
            static_cast<float>(self.environmentIntensity),
            static_cast<float>(
                (self.environmentRotationDegrees / 360.0) *
                2.0 * std::numbers::pi),
        },
        {
            static_cast<float>(self.environmentPreset),
            0.0F,
            0.0F,
            0.0F,
        },
    };

    id<MTLCommandBuffer> commandBuffer = [commandQueue_ commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [commandBuffer
        renderCommandEncoderWithDescriptor:renderPass];
    [encoder setRenderPipelineState:pipeline_];
    [encoder setDepthStencilState:depthState_];
    [encoder setCullMode:MTLCullModeBack];
    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [encoder setVertexBuffer:vertexBuffer_ offset:0 atIndex:0];
    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    [encoder setVertexTexture:heightTexture_ atIndex:1];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    [encoder setFragmentTexture:colourTexture_ atIndex:0];
    [encoder setFragmentTexture:normalTexture_ atIndex:2];
    [encoder setFragmentTexture:roughnessTexture_ atIndex:3];
    [encoder setFragmentTexture:metalnessTexture_ atIndex:4];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:indexCount_
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:indexBuffer_
                 indexBufferOffset:0];
    [encoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

@end
