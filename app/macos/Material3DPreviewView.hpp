#pragma once

#import <MetalKit/MetalKit.h>

#include <paperweight/image.hpp>

typedef NS_ENUM(NSInteger, PWPreviewShape) {
    PWPreviewShapePlane = 0,
    PWPreviewShapeSphere = 1,
    PWPreviewShapeCube = 2,
    PWPreviewShapeCylinder = 3,
};

typedef NS_ENUM(NSInteger, PWPreviewEnvironment) {
    PWPreviewEnvironmentChromeStudio = 0,
    PWPreviewEnvironmentBrushedMetal = 1,
    PWPreviewEnvironmentCeramic = 2,
    PWPreviewEnvironmentWetSurface = 3,
    PWPreviewEnvironmentNeutral = 4,
};

@interface Material3DPreviewView : MTKView

@property(nonatomic, readonly, getter=isRendererAvailable) BOOL rendererAvailable;
@property(nonatomic) PWPreviewShape previewShape;
@property(nonatomic) double lightAzimuthDegrees;
@property(nonatomic) double lightElevationDegrees;
@property(nonatomic) double lightIntensity;
@property(nonatomic) double ambientIntensity;
@property(nonatomic) PWPreviewEnvironment environmentPreset;
@property(nonatomic) double environmentIntensity;
@property(nonatomic) double environmentRotationDegrees;
@property(nonatomic) double dielectricIor;
@property(nonatomic) double displacementStrength;
@property(nonatomic) double previewNormalStrength;
@property(nonatomic) BOOL toonLightingEnabled;
@property(nonatomic) double toonBandCount;
@property(nonatomic) double toonSpecularThreshold;
@property(nonatomic) double toonRimStrength;
@property(nonatomic) double animationPhase;
@property(nonatomic) double animationSpeed;
@property(nonatomic, getter=isAnimationRunning) BOOL animationRunning;
@property(nonatomic) BOOL colourEnabled;
@property(nonatomic) BOOL heightEnabled;
@property(nonatomic) BOOL normalEnabled;
@property(nonatomic) BOOL roughnessEnabled;
@property(nonatomic) BOOL metalnessEnabled;

- (void)setColourImage:(const paperweight::Image&)colour
            heightImage:(const paperweight::Image&)height
            normalImage:(const paperweight::Image&)normal
         roughnessImage:(const paperweight::Image&)roughness
          metalnessImage:(const paperweight::Image&)metalness;
- (void)clearMaterialImages;
- (void)resetCamera;

@end
