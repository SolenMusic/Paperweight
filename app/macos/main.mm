#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>

#include "BenchmarkWindowController.hpp"
#include "ImageBridge.hpp"
#include "Material3DPreviewView.hpp"
#include "MaterialLibraryWindowController.hpp"
#include "MaterialWizardWindowController.hpp"
#include "PackedLibraryWindowController.hpp"

#include <paperweight/generator.hpp>
#include <paperweight/hash.hpp>
#include <paperweight/layer.hpp>
#include <paperweight/material_template.hpp>
#include <paperweight/material_wizard.hpp>
#include <paperweight/organic.hpp>
#include <paperweight/pmat.hpp>
#include <paperweight/stylised_lighting.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <limits>
#include <memory>
#include <locale>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <variant>

@interface MaterialPreviewView : NSView

@property(nonatomic, strong) NSImage* materialImage;
@property(nonatomic) NSInteger repeatCount;

- (void)setGeneratedImage:(const paperweight::Image&)image;

@end

@interface FlippedView : NSView
@end

@implementation FlippedView
- (BOOL)isFlipped
{
    return YES;
}
@end

@implementation MaterialPreviewView

- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _repeatCount = 1;
        self.wantsLayer = YES;
        self.layer.cornerRadius = 10.0;
        self.layer.masksToBounds = YES;
    }
    return self;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)setGeneratedImage:(const paperweight::Image&)image
{
    auto* representation = paperweight::macos::makeBitmapRepresentation(image);
    if (representation == nil) {
        self.materialImage = nil;
        [self setNeedsDisplay:YES];
        return;
    }

    auto* displayImage = [[NSImage alloc]
        initWithSize:NSMakeSize(static_cast<CGFloat>(image.width()),
                               static_cast<CGFloat>(image.height()))];
    [displayImage addRepresentation:representation];
    self.materialImage = displayImage;
    [self setNeedsDisplay:YES];
}

- (void)setRepeatCount:(NSInteger)repeatCount
{
    _repeatCount = repeatCount == 3 ? 3 : 1;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];
    [[NSColor colorWithCalibratedWhite:0.075 alpha:1.0] setFill];
    NSRectFill(self.bounds);

    if (self.materialImage == nil) {
        return;
    }

    const CGFloat repeats = static_cast<CGFloat>(self.repeatCount);
    const CGFloat tileSide = std::floor(std::min(NSWidth(self.bounds) / repeats,
                                                 NSHeight(self.bounds) / repeats));
    const CGFloat totalSide = tileSide * repeats;
    const CGFloat originX = std::floor((NSWidth(self.bounds) - totalSide) * 0.5);
    const CGFloat originY = std::floor((NSHeight(self.bounds) - totalSide) * 0.5);

    NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationNone;
    for (NSInteger row = 0; row < self.repeatCount; ++row) {
        for (NSInteger column = 0; column < self.repeatCount; ++column) {
            const NSRect destination = NSMakeRect(
                originX + static_cast<CGFloat>(column) * tileSide,
                originY + static_cast<CGFloat>(row) * tileSide,
                tileSide,
                tileSide);
            [self.materialImage drawInRect:destination
                                 fromRect:NSZeroRect
                                operation:NSCompositingOperationCopy
                                 fraction:1.0
                           respectFlipped:YES
                                    hints:@{NSImageHintInterpolation : @(NSImageInterpolationNone)}];
        }
    }
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>

@property(nonatomic, strong) NSWindow* window;
@property(nonatomic, strong) BenchmarkWindowController* benchmarkWindowController;
@property(nonatomic, strong) MaterialLibraryWindowController* materialLibraryWindowController;
@property(nonatomic, strong) MaterialWizardWindowController* materialWizardWindowController;
@property(nonatomic, strong) PackedLibraryWindowController* packedLibraryWindowController;
@property(nonatomic, strong) NSArray<NSURL*>* pendingOpenURLs;
@property(nonatomic, strong) NSTextField* materialUidField;
@property(nonatomic, strong) NSView* previewContainer;
@property(nonatomic, strong) NSStackView* comparisonStack;
@property(nonatomic, strong) NSStackView* referencePanel;
@property(nonatomic, strong) MaterialPreviewView* referenceImageView;
@property(nonatomic, strong) NSTextField* referenceTitleLabel;
@property(nonatomic, strong) MaterialPreviewView* previewView;
@property(nonatomic, strong) Material3DPreviewView* material3DPreviewView;
@property(nonatomic, strong) NSVisualEffectView* previewLoadingPanel;
@property(nonatomic, strong) NSProgressIndicator* previewProgressIndicator;
@property(nonatomic, strong) NSTextField* previewLoadingLabel;
@property(nonatomic, strong) NSMenuItem* exportMenuItem;
@property(nonatomic, strong) NSTextField* seedField;
@property(nonatomic, strong) NSTextField* materialWidthField;
@property(nonatomic, strong) NSTextField* materialHeightField;
@property(nonatomic, strong) NSTextField* coverageWidthField;
@property(nonatomic, strong) NSTextField* coverageHeightField;
@property(nonatomic, strong) NSSlider* frequencySlider;
@property(nonatomic, strong) NSTextField* frequencyValue;
@property(nonatomic, strong) NSSlider* octavesSlider;
@property(nonatomic, strong) NSTextField* octavesValue;
@property(nonatomic, strong) NSSlider* lacunaritySlider;
@property(nonatomic, strong) NSTextField* lacunarityValue;
@property(nonatomic, strong) NSSlider* gainSlider;
@property(nonatomic, strong) NSTextField* gainValue;
@property(nonatomic, strong) NSColorWell* lowColourWell;
@property(nonatomic, strong) NSColorWell* highColourWell;
@property(nonatomic, strong) NSSlider* normalStrengthSlider;
@property(nonatomic, strong) NSTextField* normalStrengthValue;
@property(nonatomic, strong) NSSlider* roughnessLowSlider;
@property(nonatomic, strong) NSTextField* roughnessLowValue;
@property(nonatomic, strong) NSSlider* roughnessHighSlider;
@property(nonatomic, strong) NSTextField* roughnessHighValue;
@property(nonatomic, strong) NSSegmentedControl* outputControl;
@property(nonatomic, strong) NSButton* bakedPresentationCheckbox;
@property(nonatomic, strong) NSStackView* bakeControls;
@property(nonatomic, strong) NSSlider* bakeAzimuthSlider;
@property(nonatomic, strong) NSTextField* bakeAzimuthValue;
@property(nonatomic, strong) NSSlider* bakeElevationSlider;
@property(nonatomic, strong) NSTextField* bakeElevationValue;
@property(nonatomic, strong) NSSlider* bakeBandsSlider;
@property(nonatomic, strong) NSTextField* bakeBandsValue;
@property(nonatomic, strong) NSSlider* bakeHighlightSlider;
@property(nonatomic, strong) NSTextField* bakeHighlightValue;
@property(nonatomic, strong) NSSlider* bakeAmbientSlider;
@property(nonatomic, strong) NSTextField* bakeAmbientValue;
@property(nonatomic, strong) NSSegmentedControl* tilingControl;
@property(nonatomic, strong) NSSegmentedControl* previewModeControl;
@property(nonatomic, strong) NSPopUpButton* previewResolutionPopup;
@property(nonatomic, strong) NSStackView* twoDPreviewControls;
@property(nonatomic, strong) NSStackView* threeDPreviewControls;
@property(nonatomic, strong) NSPopUpButton* previewShapePopup;
@property(nonatomic, strong) NSPopUpButton* lightPresetPopup;
@property(nonatomic, strong) NSSlider* lightAzimuthSlider;
@property(nonatomic, strong) NSTextField* lightAzimuthValue;
@property(nonatomic, strong) NSSlider* lightElevationSlider;
@property(nonatomic, strong) NSTextField* lightElevationValue;
@property(nonatomic, strong) NSSlider* lightIntensitySlider;
@property(nonatomic, strong) NSTextField* lightIntensityValue;
@property(nonatomic, strong) NSSlider* ambientIntensitySlider;
@property(nonatomic, strong) NSTextField* ambientIntensityValue;
@property(nonatomic, strong) NSSlider* displacementSlider;
@property(nonatomic, strong) NSTextField* displacementValue;
@property(nonatomic, strong) NSSlider* previewNormalSlider;
@property(nonatomic, strong) NSTextField* previewNormalValue;
@property(nonatomic, strong) NSButton* toonLightingCheckbox;
@property(nonatomic, strong) NSSlider* toonBandsSlider;
@property(nonatomic, strong) NSTextField* toonBandsValue;
@property(nonatomic, strong) NSSlider* toonSpecularSlider;
@property(nonatomic, strong) NSTextField* toonSpecularValue;
@property(nonatomic, strong) NSSlider* toonRimSlider;
@property(nonatomic, strong) NSTextField* toonRimValue;
@property(nonatomic, strong) NSSlider* animationPhaseSlider;
@property(nonatomic, strong) NSTextField* animationPhaseValue;
@property(nonatomic, strong) NSSlider* animationSpeedSlider;
@property(nonatomic, strong) NSTextField* animationSpeedValue;
@property(nonatomic, strong) NSButton* animationButton;
@property(nonatomic, strong) NSButton* colourMapCheckbox;
@property(nonatomic, strong) NSButton* heightMapCheckbox;
@property(nonatomic, strong) NSButton* normalMapCheckbox;
@property(nonatomic, strong) NSButton* roughnessMapCheckbox;
@property(nonatomic, strong) NSTimer* animationUiTimer;
@property(nonatomic, strong) NSTextField* statusLabel;
@property(nonatomic, strong) NSURL* currentFileURL;
@property(nonatomic, strong) NSStackView* layerListStack;
@property(nonatomic, strong) NSPopUpButton* addOperationPopup;
@property(nonatomic, strong) NSButton* removeLayerButton;
@property(nonatomic, strong) NSButton* moveLayerUpButton;
@property(nonatomic, strong) NSButton* moveLayerDownButton;
@property(nonatomic, strong) NSTextField* layerTypeLabel;
@property(nonatomic, strong) NSSegmentedControl* layerInspectorTabs;
@property(nonatomic, strong) NSStackView* layerSettingsGroup;
@property(nonatomic, strong) NSStackView* transformSettingsGroup;
@property(nonatomic, strong) NSStackView* maskSettingsGroup;
@property(nonatomic, strong) NSStackView* templateControlsGroup;
@property(nonatomic, strong) NSTextField* activeTemplateLabel;
@property(nonatomic, strong) NSMutableArray<NSSlider*>* templateControlSliders;
@property(nonatomic, strong) NSMutableArray<NSTextField*>* templateControlValues;
@property(nonatomic, strong) NSButton* layerEnabledCheckbox;
@property(nonatomic, strong) NSSegmentedControl* layerCompositeControl;
@property(nonatomic, strong) NSSlider* layerOpacitySlider;
@property(nonatomic, strong) NSTextField* layerOpacityValue;
@property(nonatomic, strong) NSStackView* noiseSeedRow;
@property(nonatomic, strong) NSTextField* noiseSeedOffsetField;
@property(nonatomic, strong) NSStackView* solidColourRow;
@property(nonatomic, strong) NSColorWell* solidColourWell;
@property(nonatomic, strong) NSStackView* levelsLowRow;
@property(nonatomic, strong) NSSlider* levelsLowSlider;
@property(nonatomic, strong) NSTextField* levelsLowValue;
@property(nonatomic, strong) NSStackView* levelsHighRow;
@property(nonatomic, strong) NSSlider* levelsHighSlider;
@property(nonatomic, strong) NSTextField* levelsHighValue;
@property(nonatomic, strong) NSStackView* levelsGammaRow;
@property(nonatomic, strong) NSSlider* levelsGammaSlider;
@property(nonatomic, strong) NSTextField* levelsGammaValue;
@property(nonatomic, strong) NSStackView* thresholdRow;
@property(nonatomic, strong) NSSlider* thresholdSlider;
@property(nonatomic, strong) NSTextField* thresholdValue;
@property(nonatomic, strong) NSStackView* patternCountXRow;
@property(nonatomic, strong) NSTextField* patternCountXLabel;
@property(nonatomic, strong) NSSlider* patternCountXSlider;
@property(nonatomic, strong) NSTextField* patternCountXValue;
@property(nonatomic, strong) NSStackView* patternCountYRow;
@property(nonatomic, strong) NSTextField* patternCountYLabel;
@property(nonatomic, strong) NSSlider* patternCountYSlider;
@property(nonatomic, strong) NSTextField* patternCountYValue;
@property(nonatomic, strong) NSStackView* patternValueOneRow;
@property(nonatomic, strong) NSTextField* patternValueOneLabel;
@property(nonatomic, strong) NSSlider* patternValueOneSlider;
@property(nonatomic, strong) NSTextField* patternValueOneValue;
@property(nonatomic, strong) NSStackView* patternValueTwoRow;
@property(nonatomic, strong) NSTextField* patternValueTwoLabel;
@property(nonatomic, strong) NSSlider* patternValueTwoSlider;
@property(nonatomic, strong) NSTextField* patternValueTwoValue;
@property(nonatomic, strong) NSStackView* patternValueThreeRow;
@property(nonatomic, strong) NSTextField* patternValueThreeLabel;
@property(nonatomic, strong) NSSlider* patternValueThreeSlider;
@property(nonatomic, strong) NSTextField* patternValueThreeValue;
@property(nonatomic, strong) NSStackView* patternValueFourRow;
@property(nonatomic, strong) NSTextField* patternValueFourLabel;
@property(nonatomic, strong) NSSlider* patternValueFourSlider;
@property(nonatomic, strong) NSTextField* patternValueFourValue;
@property(nonatomic, strong) NSStackView* surfaceKindRow;
@property(nonatomic, strong) NSPopUpButton* surfaceKindPopup;
@property(nonatomic, strong) NSStackView* courseFieldRow;
@property(nonatomic, strong) NSPopUpButton* courseFieldPopup;
@property(nonatomic, strong) NSStackView* courseGapRow;
@property(nonatomic, strong) NSSlider* courseGapSlider;
@property(nonatomic, strong) NSTextField* courseGapValue;
@property(nonatomic, strong) NSStackView* courseSoftnessRow;
@property(nonatomic, strong) NSSlider* courseSoftnessSlider;
@property(nonatomic, strong) NSTextField* courseSoftnessValue;
@property(nonatomic, strong) NSStackView* courseOverlapRow;
@property(nonatomic, strong) NSSlider* courseOverlapSlider;
@property(nonatomic, strong) NSTextField* courseOverlapValue;
@property(nonatomic, strong) NSStackView* processingTargetRow;
@property(nonatomic, strong) NSPopUpButton* processingTargetPopup;
@property(nonatomic, strong) NSStackView* filterSensitivityRow;
@property(nonatomic, strong) NSSlider* filterSensitivitySlider;
@property(nonatomic, strong) NSTextField* filterSensitivityValue;
@property(nonatomic, strong) NSStackView* posteriseBandsRow;
@property(nonatomic, strong) NSSlider* posteriseBandsSlider;
@property(nonatomic, strong) NSTextField* posteriseBandsValue;
@property(nonatomic, strong) NSStackView* rampModeRow;
@property(nonatomic, strong) NSPopUpButton* rampModePopup;
@property(nonatomic, strong) NSStackView* colourEntriesGroup;
@property(nonatomic, strong) NSMutableArray<NSStackView*>* colourEntryRows;
@property(nonatomic, strong) NSMutableArray<NSTextField*>* colourEntryLabels;
@property(nonatomic, strong) NSMutableArray<NSSlider*>* colourPositionSliders;
@property(nonatomic, strong) NSMutableArray<NSTextField*>* colourPositionValues;
@property(nonatomic, strong) NSMutableArray<NSColorWell*>* colourEntryWells;
@property(nonatomic, strong) NSButton* addColourEntryButton;
@property(nonatomic, strong) NSButton* removeColourEntryButton;
@property(nonatomic, strong) NSStackView* inkColourRow;
@property(nonatomic, strong) NSColorWell* inkColourWell;
@property(nonatomic, strong) NSStackView* inkRadiusRow;
@property(nonatomic, strong) NSSlider* inkRadiusSlider;
@property(nonatomic, strong) NSTextField* inkRadiusValue;
@property(nonatomic, strong) NSStackView* inkThresholdRow;
@property(nonatomic, strong) NSSlider* inkThresholdSlider;
@property(nonatomic, strong) NSTextField* inkThresholdValue;
@property(nonatomic, strong) NSStackView* inkSoftnessRow;
@property(nonatomic, strong) NSSlider* inkSoftnessSlider;
@property(nonatomic, strong) NSTextField* inkSoftnessValue;
@property(nonatomic, strong) NSStackView* inkStrengthRow;
@property(nonatomic, strong) NSSlider* inkStrengthSlider;
@property(nonatomic, strong) NSTextField* inkStrengthValue;
@property(nonatomic, strong) NSButton* inkInvertedCheckbox;
@property(nonatomic, strong) NSButton* facetedNormalsCheckbox;
@property(nonatomic, strong) NSButton* equalMortarWidthCheckbox;
@property(nonatomic, strong) NSButton* physicalBrickCheckbox;
@property(nonatomic, strong) NSStackView* physicalBrickWidthRow;
@property(nonatomic, strong) NSTextField* physicalBrickWidthField;
@property(nonatomic, strong) NSStackView* physicalBrickHeightRow;
@property(nonatomic, strong) NSTextField* physicalBrickHeightField;
@property(nonatomic, strong) NSStackView* physicalBrickMortarRow;
@property(nonatomic, strong) NSTextField* physicalBrickMortarField;
@property(nonatomic, strong) NSStackView* physicalCourseOverlapRow;
@property(nonatomic, strong) NSTextField* physicalCourseOverlapField;
@property(nonatomic, strong) NSTextField* physicalBrickSummary;
@property(nonatomic, strong) NSStackView* patternDirectionRow;
@property(nonatomic, strong) NSSegmentedControl* patternDirectionControl;
@property(nonatomic, strong) NSStackView* patternSeedRow;
@property(nonatomic, strong) NSTextField* patternSeedOffsetField;
@property(nonatomic, strong) NSSlider* transformScaleXSlider;
@property(nonatomic, strong) NSTextField* transformScaleXValue;
@property(nonatomic, strong) NSSlider* transformScaleYSlider;
@property(nonatomic, strong) NSTextField* transformScaleYValue;
@property(nonatomic, strong) NSSlider* transformOffsetXSlider;
@property(nonatomic, strong) NSTextField* transformOffsetXValue;
@property(nonatomic, strong) NSSlider* transformOffsetYSlider;
@property(nonatomic, strong) NSTextField* transformOffsetYValue;
@property(nonatomic, strong) NSSegmentedControl* transformRotationControl;
@property(nonatomic, strong) NSButton* warpEnabledCheckbox;
@property(nonatomic, strong) NSSlider* warpStrengthSlider;
@property(nonatomic, strong) NSTextField* warpStrengthValue;
@property(nonatomic, strong) NSSlider* warpFrequencySlider;
@property(nonatomic, strong) NSTextField* warpFrequencyValue;
@property(nonatomic, strong) NSTextField* warpSeedOffsetField;
@property(nonatomic, strong) NSButton* maskEnabledCheckbox;
@property(nonatomic, strong) NSButton* maskInvertedCheckbox;
@property(nonatomic, strong) NSTextField* maskSeedOffsetField;
@property(nonatomic, strong) NSSlider* maskLowSlider;
@property(nonatomic, strong) NSTextField* maskLowValue;
@property(nonatomic, strong) NSSlider* maskHighSlider;
@property(nonatomic, strong) NSTextField* maskHighValue;

@end

namespace {

NSTextField* makeLabel(NSString* text)
{
    auto* label = [NSTextField labelWithString:text];
    label.selectable = NO;
    return label;
}

NSStackView* makeSliderRow(
    NSString* title,
    double minimum,
    double maximum,
    double value,
    BOOL integerValues,
    id target)
{
    auto* titleLabel = makeLabel(title);
    [titleLabel.widthAnchor constraintEqualToConstant:78.0].active = YES;

    auto* slider = [NSSlider sliderWithValue:value
                                    minValue:minimum
                                    maxValue:maximum
                                      target:target
                                      action:@selector(parameterChanged:)];
    slider.continuous = YES;
    if (integerValues) {
        slider.numberOfTickMarks = static_cast<NSInteger>(maximum - minimum + 1.0);
        slider.allowsTickMarkValuesOnly = YES;
    }

    auto* valueLabel = makeLabel(@"");
    valueLabel.alignment = NSTextAlignmentRight;
    valueLabel.font = [NSFont monospacedDigitSystemFontOfSize:12.0 weight:NSFontWeightRegular];
    [valueLabel.widthAnchor constraintEqualToConstant:48.0].active = YES;

    auto* row = [NSStackView stackViewWithViews:@[ titleLabel, slider, valueLabel ]];
    row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    row.alignment = NSLayoutAttributeCenterY;
    row.spacing = 8.0;

    return row;
}

NSStackView* makeLayerSliderRow(
    NSString* title,
    double minimum,
    double maximum,
    double value,
    id target)
{
    auto* titleLabel = makeLabel(title);
    [titleLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;

    auto* slider = [NSSlider sliderWithValue:value
                                    minValue:minimum
                                    maxValue:maximum
                                      target:target
                                      action:@selector(layerParameterChanged:)];
    slider.continuous = YES;

    auto* valueLabel = makeLabel(@"");
    valueLabel.alignment = NSTextAlignmentRight;
    valueLabel.font = [NSFont monospacedDigitSystemFontOfSize:12.0 weight:NSFontWeightRegular];
    [valueLabel.widthAnchor constraintEqualToConstant:44.0].active = YES;

    auto* row = [NSStackView stackViewWithViews:@[ titleLabel, slider, valueLabel ]];
    row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    row.alignment = NSLayoutAttributeCenterY;
    row.spacing = 8.0;
    return row;
}

NSStackView* makePreviewSliderRow(
    NSString* title,
    double minimum,
    double maximum,
    double value,
    id target)
{
    auto* titleLabel = makeLabel(title);
    [titleLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    auto* slider = [NSSlider sliderWithValue:value
                                    minValue:minimum
                                    maxValue:maximum
                                      target:target
                                      action:@selector(preview3DParameterChanged:)];
    slider.continuous = YES;
    auto* valueLabel = makeLabel(@"");
    valueLabel.alignment = NSTextAlignmentRight;
    valueLabel.font = [NSFont monospacedDigitSystemFontOfSize:12.0
                                                       weight:NSFontWeightRegular];
    [valueLabel.widthAnchor constraintEqualToConstant:48.0].active = YES;
    auto* row = [NSStackView stackViewWithViews:@[titleLabel, slider, valueLabel]];
    row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    row.alignment = NSLayoutAttributeCenterY;
    row.spacing = 8.0;
    return row;
}

NSBox* makeSeparator()
{
    auto* separator = [[NSBox alloc] initWithFrame:NSZeroRect];
    separator.boxType = NSBoxSeparator;
    return separator;
}

std::optional<double> positiveDecimal(NSTextField* field)
{
    const std::string text = field.stringValue.UTF8String;
    std::istringstream stream{text};
    stream.imbue(std::locale::classic());
    double value = 0.0;
    stream >> std::noskipws >> value;
    if (!stream || stream.peek() != std::char_traits<char>::eof() ||
        !std::isfinite(value) || value <= 0.0) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> nonNegativeDecimal(NSTextField* field)
{
    const std::string text = field.stringValue.UTF8String;
    std::istringstream stream{text};
    stream.imbue(std::locale::classic());
    double value = 0.0;
    stream >> std::noskipws >> value;
    if (!stream || stream.peek() != std::char_traits<char>::eof() ||
        !std::isfinite(value) || value < 0.0) {
        return std::nullopt;
    }
    return value;
}

NSStackView* makeMetreFieldRow(
    NSString* title,
    double value,
    id target,
    SEL action)
{
    auto* label = makeLabel(title);
    [label.widthAnchor constraintEqualToConstant:78.0].active = YES;
    auto* field = [[NSTextField alloc] initWithFrame:NSZeroRect];
    field.stringValue = [NSString stringWithFormat:@"%.6g", value];
    field.target = target;
    field.action = action;
    auto* unit = makeLabel(@"m");
    [unit.widthAnchor constraintEqualToConstant:14.0].active = YES;
    auto* row = [NSStackView stackViewWithViews:@[ label, field, unit ]];
    row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    row.alignment = NSLayoutAttributeCenterY;
    row.spacing = 8.0;
    return row;
}

NSColor* colourFromRgba8(const paperweight::Rgba8& colour)
{
    constexpr CGFloat scale = 1.0 / 255.0;
    return [NSColor colorWithSRGBRed:static_cast<CGFloat>(colour.red) * scale
                               green:static_cast<CGFloat>(colour.green) * scale
                                blue:static_cast<CGFloat>(colour.blue) * scale
                               alpha:static_cast<CGFloat>(colour.alpha) * scale];
}

paperweight::Rgba8 rgba8FromColour(NSColor* colour)
{
    NSColor* converted = [colour colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    if (converted == nil) {
        return {};
    }
    const auto channel = [](CGFloat value) {
        return static_cast<std::uint8_t>(std::llround(std::clamp(value, 0.0, 1.0) * 255.0));
    };
    return {
        channel(converted.redComponent),
        channel(converted.greenComponent),
        channel(converted.blueComponent),
        channel(converted.alphaComponent),
    };
}

NSStackView* makeColourRow(
    NSString* title,
    const paperweight::Rgba8& colour,
    id target)
{
    auto* titleLabel = makeLabel(title);
    [titleLabel.widthAnchor constraintEqualToConstant:78.0].active = YES;

    auto* colourWell = [[NSColorWell alloc] initWithFrame:NSZeroRect];
    colourWell.color = colourFromRgba8(colour);
    colourWell.target = target;
    colourWell.action = @selector(colourChanged:);
    colourWell.accessibilityLabel = title;
    [colourWell.heightAnchor constraintEqualToConstant:28.0].active = YES;

    auto* row = [NSStackView stackViewWithViews:@[ titleLabel, colourWell ]];
    row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    row.alignment = NSLayoutAttributeCenterY;
    row.spacing = 8.0;
    return row;
}

NSString* outputName(paperweight::MaterialOutput output)
{
    switch (output) {
    case paperweight::MaterialOutput::colour:
        return @"Colour";
    case paperweight::MaterialOutput::height:
        return @"Height";
    case paperweight::MaterialOutput::normal:
        return @"Normal";
    case paperweight::MaterialOutput::roughness:
        return @"Roughness";
    }
    return @"Unknown";
}

struct PreviewMapSet {
    paperweight::Image colour;
    paperweight::Image height;
    paperweight::Image normal;
    paperweight::Image roughness;
};

NSString* operationDisplayName(const paperweight::LayerOperation& operation)
{
    switch (operation.index()) {
    case 0:
        return @"Noise";
    case 1:
        return @"Solid Colour";
    case 2:
        return @"Levels";
    case 3:
        return @"Threshold";
    case 4:
        return @"Brick Grid";
    case 5:
        return @"Tile Grid";
    case 6:
        return @"Worley Cells";
    case 7:
        return @"Random Cells";
    case 8:
        return @"Lines";
    case 9:
        return @"Rectangles";
    case 10:
        return @"Circles";
    case 11: {
        const auto kind = std::get<paperweight::SurfacePatternOperation>(operation).kind;
        switch (kind) {
        case paperweight::SurfacePatternKind::ridgedNoise:
            return @"Ridged Noise";
        case paperweight::SurfacePatternKind::bands:
            return @"Bands";
        case paperweight::SurfacePatternKind::rings:
            return @"Rings";
        case paperweight::SurfacePatternKind::scatter:
            return @"Scatter";
        case paperweight::SurfacePatternKind::streaks:
            return @"Streaks";
        }
        return @"Surface Pattern";
    }
    case 12: {
        const auto kind = std::get<paperweight::SurfaceFilterOperation>(operation).kind;
        switch (kind) {
        case paperweight::SurfaceFilterKind::invert:
            return @"Invert Filter";
        case paperweight::SurfaceFilterKind::soften:
            return @"Soften Filter";
        case paperweight::SurfaceFilterKind::expand:
            return @"Expand Filter";
        case paperweight::SurfaceFilterKind::contract:
            return @"Contract Filter";
        case paperweight::SurfaceFilterKind::edge:
            return @"Edge Filter";
        case paperweight::SurfaceFilterKind::slope:
            return @"Slope Filter";
        case paperweight::SurfaceFilterKind::cavity:
            return @"Cavity Filter";
        case paperweight::SurfaceFilterKind::peaks:
            return @"Peaks Filter";
        case paperweight::SurfaceFilterKind::edgeAwareSoften:
            return @"Edge-aware Soften";
        }
        return @"Surface Filter";
    }
    case 13:
        return @"Posterise";
    case 14:
        return @"Colour Ramp";
    case 15:
        return @"Palette";
    case 16:
        return @"Ink Contours";
    case 17:
        return @"Region Field";
    case 18:
        return @"Course Layout";
    case 19:
        return @"Region Surface";
    case 20: {
        const auto kind = std::get<paperweight::ShapePrimitiveOperation>(operation).kind;
        switch (kind) {
        case paperweight::ShapePrimitiveKind::roundedRectangle:
            return @"Rounded Rectangles";
        case paperweight::ShapePrimitiveKind::ellipse:
            return @"Ellipses";
        case paperweight::ShapePrimitiveKind::capsule:
            return @"Capsules";
        case paperweight::ShapePrimitiveKind::diamond:
            return @"Diamonds";
        case paperweight::ShapePrimitiveKind::convexPolygon:
            return @"Convex Polygons";
        }
        return @"Shapes";
    }
    case 21:
        return @"Shape Boolean";
    case 22:
        return @"Seam-safe Lattice";
    case 23:
        return @"Instance Scatter";
    case 24:
        return @"Organic Cells";
    case 25:
        return @"Organic Cracks";
    case 26:
        return @"Leaf Clusters";
    case 27:
        return @"Organic Accumulation";
    default:
        return @"Unknown";
    }
}

std::vector<paperweight::ShapePoint> regularPolygonVertices(std::size_t count)
{
    std::vector<paperweight::ShapePoint> vertices;
    vertices.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const double angle = -std::numbers::pi * 0.5 +
            2.0 * std::numbers::pi * static_cast<double>(index) /
                static_cast<double>(count);
        vertices.push_back({0.48 * std::cos(angle), 0.48 * std::sin(angle)});
    }
    return vertices;
}

paperweight::ProcessingTarget processingTargetAtIndex(NSInteger index)
{
    switch (index) {
    case 1:
        return paperweight::ProcessingTarget::scalar;
    case 2:
        return paperweight::ProcessingTarget::colourAndScalar;
    default:
        return paperweight::ProcessingTarget::colour;
    }
}

NSInteger processingTargetIndex(paperweight::ProcessingTarget target)
{
    switch (target) {
    case paperweight::ProcessingTarget::colour:
        return 0;
    case paperweight::ProcessingTarget::scalar:
        return 1;
    case paperweight::ProcessingTarget::colourAndScalar:
        return 2;
    }
    return 0;
}

paperweight::MaterialLayer* layerAt(paperweight::Material& material, NSInteger index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= material.layers.size()) {
        return nullptr;
    }
    return &material.layers[static_cast<std::size_t>(index)];
}

bool materialUsesDerivedRepeat(const paperweight::Material& material)
{
    return std::any_of(
        material.layers.begin(),
        material.layers.end(),
        [](const paperweight::MaterialLayer& layer) {
            const auto* brick = std::get_if<paperweight::BrickGridOperation>(
                &layer.operation);
            if (brick != nullptr && brick->physicalDimensions.has_value()) {
                return true;
            }
            const auto* course = std::get_if<paperweight::CourseLayoutOperation>(
                &layer.operation);
            return course != nullptr && course->physicalDimensions.has_value();
        });
}

double recommendedPreviewDisplacement(const paperweight::Material& material)
{
    // Normal strength is the existing authored indication of intended surface relief.
    // Keep the display-only displacement bounded so high-detail materials remain legible.
    return std::clamp(material.normalStrength * 0.04, 0.0, 0.12);
}

double resizedCoverageExtent(
    double oldCoverage,
    double oldRepeat,
    double newRepeat)
{
    const double repeats = oldCoverage / oldRepeat;
    const double rounded = std::round(repeats);
    const double tolerance = 1.0e-9 * std::max(1.0, std::abs(repeats));
    if (std::isfinite(repeats) && std::abs(repeats - rounded) <= tolerance &&
        rounded >= 1.0 && rounded <= paperweight::PhysicalLimits::maximumRepeats) {
        return newRepeat * rounded;
    }
    return newRepeat;
}

double textureSpaceMortarMaximum(const paperweight::BrickGridOperation& brick)
{
    return paperweight::LayerLimits::maximumGap /
        static_cast<double>(std::max(brick.columns, brick.rows));
}

} // namespace

@implementation AppDelegate {
    paperweight::Material material_;
    paperweight::MaterialOutput selectedOutput_;
    bool bakedPresentationSelected_;
    const paperweight::ReferenceMaterialTemplate* activeTemplate_;
    std::optional<paperweight::Image> generatedImage_;
    std::shared_ptr<PreviewMapSet> generated3DMaps_;
    dispatch_queue_t previewQueue_;
    dispatch_block_t pendingPreviewBlock_;
    std::shared_ptr<std::atomic_bool> previewCancellation_;
    std::uint64_t previewRevision_;
    bool dirty_;
    NSInteger selectedLayer_;
    NSInteger selectedScatterPopulation_;
    paperweight::PhysicalSize previewCoverage_;
    std::uint32_t previewResolution_;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    static_cast<void>(notification);
    selectedOutput_ = paperweight::MaterialOutput::colour;
    bakedPresentationSelected_ = false;
    activeTemplate_ = nullptr;
    material_.layers.push_back(paperweight::makeNoiseLayer());
    previewCoverage_ = material_.physicalSize;
    const NSInteger savedPreviewResolution =
        [NSUserDefaults.standardUserDefaults integerForKey:@"previewResolution"];
    previewResolution_ = savedPreviewResolution == 64 || savedPreviewResolution == 128 ||
            savedPreviewResolution == 256 || savedPreviewResolution == 512 ||
            savedPreviewResolution == 1024
        ? static_cast<std::uint32_t>(savedPreviewResolution)
        : 512;
    selectedLayer_ = 0;
    previewQueue_ = dispatch_queue_create(
        "org.solen-music.paperweight.preview",
        DISPATCH_QUEUE_SERIAL);
    [self buildMenus];
    [self buildWindow];
    [self updateControlLabels];
    [self regeneratePreview];
    [self updateWindowTitle];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    NSArray<NSURL*>* pendingURLs = self.pendingOpenURLs;
    self.pendingOpenURLs = nil;
    for (NSURL* url in pendingURLs) {
        [self openDocumentAtURL:url];
    }
}

- (void)application:(NSApplication*)sender openFiles:(NSArray<NSString*>*)filenames
{
    auto* urls = [NSMutableArray arrayWithCapacity:filenames.count];
    for (NSString* filename in filenames) {
        [urls addObject:[NSURL fileURLWithPath:filename]];
    }
    if (self.window == nil) {
        self.pendingOpenURLs = urls;
        [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
        return;
    }

    BOOL success = YES;
    for (NSURL* url in urls) {
        success = [self openDocumentAtURL:url] && success;
    }
    [sender replyToOpenOrPrint:success
        ? NSApplicationDelegateReplySuccess
        : NSApplicationDelegateReplyFailure];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
    static_cast<void>(sender);
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    static_cast<void>(sender);
    if (![self confirmDiscardIfNeeded]) {
        return NSTerminateCancel;
    }
    ++previewRevision_;
    if (pendingPreviewBlock_ != nil) {
        dispatch_block_cancel(pendingPreviewBlock_);
        pendingPreviewBlock_ = nil;
    }
    if (previewCancellation_) {
        previewCancellation_->store(true, std::memory_order_relaxed);
    }
    [self.animationUiTimer invalidate];
    self.animationUiTimer = nil;
    return NSTerminateNow;
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    static_cast<void>(sender);
    return [self confirmDiscardIfNeeded];
}

- (void)windowWillClose:(NSNotification*)notification
{
    if (notification.object != self.window) {
        return;
    }

    // The editor owns one reusable window rather than an NSDocument window.
    // Once it is closed, its URL must no longer make the library believe the
    // material is open. The retained window can subsequently be shown again
    // when a material is opened from the library or Finder.
    self.currentFileURL = nil;
    self.window.documentEdited = NO;
    self.window.representedURL = nil;
}

- (void)buildMenus
{
    auto* mainMenu = [[NSMenu alloc] initWithTitle:@""];

    auto* appMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:appMenuItem];
    auto* appMenu = [[NSMenu alloc] initWithTitle:@"Paperweight"];
    [appMenu addItemWithTitle:@"About Paperweight"
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit Paperweight" action:@selector(terminate:) keyEquivalent:@"q"];
    appMenuItem.submenu = appMenu;

    auto* fileMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:fileMenuItem];
    auto* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    auto* newMaterialItem = [fileMenu addItemWithTitle:@"New Material…"
                                                action:@selector(showMaterialWizard:)
                                         keyEquivalent:@"n"];
    newMaterialItem.target = self;
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Open…" action:@selector(openMaterial:) keyEquivalent:@"o"];
    auto* referenceTemplateItem = [[NSMenuItem alloc]
        initWithTitle:@"New from Reference Template"
        action:nil
        keyEquivalent:@""];
    auto* referenceTemplateMenu = [[NSMenu alloc] initWithTitle:@"New from Reference Template"];
    for (const auto& descriptor : paperweight::referenceMaterialTemplates()) {
        auto* title = [NSString stringWithUTF8String:descriptor.displayName.data()];
        auto* identifier = [NSString stringWithUTF8String:descriptor.identifier.data()];
        auto* item = [referenceTemplateMenu addItemWithTitle:title
                                                     action:@selector(openReferenceTemplate:)
                                              keyEquivalent:@""];
        item.target = self;
        item.representedObject = identifier;
    }
    referenceTemplateItem.submenu = referenceTemplateMenu;
    [fileMenu addItem:referenceTemplateItem];
    auto* showcaseItem = [[NSMenuItem alloc] initWithTitle:@"New from Showcase"
                                                    action:nil
                                             keyEquivalent:@""];
    auto* showcaseMenu = [[NSMenu alloc] initWithTitle:@"New from Showcase"];
    const NSArray<NSArray<NSString*>*>* showcases = @[
        @[ @"Cracked Stone", @"cracked-stone" ],
        @[ @"Weathered Metal", @"weathered-metal" ],
        @[ @"Mossy Pebbles", @"mossy-pebbles" ],
        @[ @"Knotty Wood", @"knotty-wood" ],
        @[ @"Marble Veins", @"marble-veins" ],
        @[ @"Eroded Terrain", @"eroded-terrain" ],
        @[ @"Toon Dungeon", @"toon-dungeon" ],
        @[ @"Painted Metal", @"painted-metal" ],
        @[ @"Graphic Marble", @"graphic-marble" ],
        @[ @"Region Stones", @"region-stones" ],
        @[ @"Castle Flagstone", @"castle-flagstone" ],
        @[ @"Castle Stone", @"castle-stone" ],
        @[ @"Cel Castle Stone", @"cel-castle-stone" ],
        @[ @"Castle Roof", @"castle-roof" ],
        @[ @"Cel Forest Rock", @"cel-forest-rock" ],
        @[ @"Sculpted Flagstone", @"sculpted-flagstone" ],
        @[ @"Worn Masonry", @"worn-masonry" ],
        @[ @"Sculpted Roof Slate", @"sculpted-roof-slate" ],
        @[ @"Castle Window", @"castle-window" ],
        @[ @"Detailed Crate", @"detailed-crate" ],
        @[ @"Decorative Fasteners", @"decorative-fasteners" ],
        @[ @"Masonry Corner Variation", @"masonry-corner-variation" ],
        @[ @"Cel Courtyard Gravel", @"cel-courtyard-gravel" ],
        @[ @"Scattered Debris", @"scattered-debris" ],
        @[ @"Foliage Foundation", @"foliage-foundation" ],
        @[ @"Cel Forest Bark", @"cel-forest-bark" ],
        @[ @"Castle Foliage", @"castle-foliage" ],
    ];
    for (NSArray<NSString*>* showcase in showcases) {
        auto* item = [showcaseMenu addItemWithTitle:showcase[0]
                                            action:@selector(openShowcase:)
                                     keyEquivalent:@""];
        item.target = self;
        item.representedObject = showcase[1];
    }
    showcaseItem.submenu = showcaseMenu;
    [fileMenu addItem:showcaseItem];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Open Reference Image…"
                        action:@selector(chooseReferenceImage:)
                 keyEquivalent:@"r"];
    [fileMenu addItemWithTitle:@"Hide Reference Image"
                        action:@selector(clearReferenceImage:)
                 keyEquivalent:@""];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Save" action:@selector(saveMaterial:) keyEquivalent:@"s"];
    auto* saveAsItem = [fileMenu addItemWithTitle:@"Save As…"
                                          action:@selector(saveMaterialAs:)
                                   keyEquivalent:@"s"];
    saveAsItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [fileMenu addItem:[NSMenuItem separatorItem]];
    self.exportMenuItem = [fileMenu addItemWithTitle:@"Export PNG…"
                                              action:@selector(exportPng:)
                                       keyEquivalent:@"e"];
    self.exportMenuItem.keyEquivalentModifierMask =
        NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"w"];
    fileMenuItem.submenu = fileMenu;

    auto* editMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:editMenuItem];
    auto* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
    editMenuItem.submenu = editMenu;

    auto* toolsMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:toolsMenuItem];
    auto* toolsMenu = [[NSMenu alloc] initWithTitle:@"Tools"];
    auto* libraryItem = [toolsMenu addItemWithTitle:@"Material Library…"
                                             action:@selector(showMaterialLibrary:)
                                      keyEquivalent:@"l"];
    libraryItem.target = self;
    libraryItem.keyEquivalentModifierMask =
        NSEventModifierFlagCommand | NSEventModifierFlagOption;
    auto* benchmarkItem = [toolsMenu addItemWithTitle:@"Performance Benchmark…"
                                               action:@selector(showPerformanceBenchmark:)
                                        keyEquivalent:@"b"];
    benchmarkItem.target = self;
    benchmarkItem.keyEquivalentModifierMask =
        NSEventModifierFlagCommand | NSEventModifierFlagOption;
    [toolsMenu addItem:[NSMenuItem separatorItem]];
    auto* materialInformationItem = [toolsMenu addItemWithTitle:@"Material Information…"
                                                          action:@selector(showMaterialInformation:)
                                                   keyEquivalent:@"i"];
    materialInformationItem.target = self;
    materialInformationItem.keyEquivalentModifierMask =
        NSEventModifierFlagCommand | NSEventModifierFlagOption;
    toolsMenuItem.submenu = toolsMenu;

    NSApp.mainMenu = mainMenu;
}

- (void)showMaterialWizard:(id)sender
{
    static_cast<void>(sender);
    if (self.materialWizardWindowController.window.isVisible) {
        [self.materialWizardWindowController showMaterialWizard];
        return;
    }
    __weak AppDelegate* weakSelf = self;
    self.materialWizardWindowController = [[MaterialWizardWindowController alloc]
        initWithPreviewResolution:previewResolution_
        useMaterialHandler:^BOOL(
            const paperweight::Material& material,
            NSString* templateIdentifier) {
            AppDelegate* strongSelf = weakSelf;
            if (strongSelf == nil || ![strongSelf confirmDiscardIfNeeded]) {
                return NO;
            }
            strongSelf->material_ = material;
            strongSelf->selectedLayer_ = 0;
            strongSelf.currentFileURL = nil;
            strongSelf->dirty_ = true;
            const auto* descriptor = templateIdentifier.UTF8String == nullptr
                ? nullptr
                : paperweight::findWizardMaterialTemplate(templateIdentifier.UTF8String);
            [strongSelf setActiveReferenceTemplate:descriptor];
            [strongSelf clearReferenceImage:nil];
            [strongSelf applyMaterialToControls];
            [strongSelf updateWindowTitle];
            strongSelf.statusLabel.stringValue = @"Wizard material is ready for detailed editing.";
            strongSelf.statusLabel.textColor = NSColor.secondaryLabelColor;
            [strongSelf.window makeKeyAndOrderFront:nil];
            return YES;
        }
        savedMaterialHandler:^(NSURL* url) {
            AppDelegate* strongSelf = weakSelf;
            [strongSelf.materialLibraryWindowController noteMaterialSavedAtURL:url];
        }];
    [self.materialWizardWindowController showMaterialWizard];
}

- (void)showMaterialLibrary:(id)sender
{
    static_cast<void>(sender);
    if (self.materialLibraryWindowController == nil) {
        __weak AppDelegate* weakSelf = self;
        self.materialLibraryWindowController = [[MaterialLibraryWindowController alloc]
            initWithOpenMaterialHandler:^(NSURL* url) {
                AppDelegate* strongSelf = weakSelf;
                if (strongSelf != nil) {
                    [strongSelf openMaterialAtURL:url asShowcase:NO];
                }
            }
            relocationHandler:^(NSURL* oldURL, NSURL* newURL) {
                AppDelegate* strongSelf = weakSelf;
                if (strongSelf != nil && [strongSelf.currentFileURL isEqual:oldURL]) {
                    strongSelf.currentFileURL = newURL;
                    [strongSelf updateWindowTitle];
                }
            }
            canRewriteHandler:^BOOL(NSURL* url) {
                AppDelegate* strongSelf = weakSelf;
                return strongSelf == nil || !strongSelf.window.isVisible ||
                    strongSelf.currentFileURL == nil ||
                    ![strongSelf.currentFileURL isEqual:url];
            }];
    }
    [self.materialLibraryWindowController showMaterialLibrary];
}

- (void)assignMaterialUid:(id)sender
{
    static_cast<void>(sender);
    if (self.materialUidField.stringValue.length == 0) {
        self.materialUidField.stringValue = NSUUID.UUID.UUIDString.lowercaseString;
    }
}

- (void)showMaterialInformation:(id)sender
{
    static_cast<void>(sender);
    const paperweight::MaterialMetadata current = material_.metadata.value_or(
        paperweight::MaterialMetadata{});
    auto stringFromUtf8 = [](const std::string& value) {
        return [NSString stringWithUTF8String:value.c_str()];
    };

    auto* nameField = [NSTextField textFieldWithString:stringFromUtf8(current.name)];
    auto* categoryField = [NSTextField textFieldWithString:stringFromUtf8(current.category)];
    auto* descriptionField = [NSTextField textFieldWithString:stringFromUtf8(current.description)];
    std::string joinedTags;
    for (std::size_t index = 0; index < current.tags.size(); ++index) {
        if (index != 0) {
            joinedTags += ", ";
        }
        joinedTags += current.tags[index];
    }
    auto* tagsField = [NSTextField textFieldWithString:stringFromUtf8(joinedTags)];
    self.materialUidField = [NSTextField labelWithString:stringFromUtf8(current.uid)];
    self.materialUidField.selectable = YES;
    self.materialUidField.lineBreakMode = NSLineBreakByTruncatingMiddle;
    auto* assignUidButton = [NSButton buttonWithTitle:@"Assign UID"
                                               target:self
                                               action:@selector(assignMaterialUid:)];
    assignUidButton.enabled = current.uid.empty();
    assignUidButton.toolTip = current.uid.empty()
        ? @"Give this material a stable identity for use in a library."
        : @"The material already has a stable identity.";
    auto* uidRow = [NSStackView stackViewWithViews:@[self.materialUidField, assignUidButton]];
    uidRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    uidRow.distribution = NSStackViewDistributionFillProportionally;
    uidRow.spacing = 8.0;

    auto* grid = [NSGridView gridViewWithViews:@[
        @[makeLabel(@"Name"), nameField],
        @[makeLabel(@"UID"), uidRow],
        @[makeLabel(@"Category"), categoryField],
        @[makeLabel(@"Tags"), tagsField],
        @[makeLabel(@"Description"), descriptionField],
    ]];
    grid.translatesAutoresizingMaskIntoConstraints = NO;
    grid.rowSpacing = 8.0;
    grid.columnSpacing = 10.0;
    grid.xPlacement = NSGridCellPlacementFill;
    [grid.widthAnchor constraintEqualToConstant:520.0].active = YES;

    auto* alert = [[NSAlert alloc] init];
    alert.messageText = @"Material Information";
    alert.informativeText =
        @"Identity and descriptive details do not alter the generated texture.";
    alert.accessoryView = grid;
    [alert addButtonWithTitle:@"Save Information"];
    [alert addButtonWithTitle:@"Cancel"];
    if ([alert runModal] != NSAlertFirstButtonReturn) {
        self.materialUidField = nil;
        return;
    }

    auto trimmed = [](NSString* value) {
        return [value stringByTrimmingCharactersInSet:
            NSCharacterSet.whitespaceAndNewlineCharacterSet];
    };
    paperweight::MaterialMetadata updated;
    updated.uid = trimmed(self.materialUidField.stringValue).UTF8String;
    updated.name = trimmed(nameField.stringValue).UTF8String;
    updated.category = trimmed(categoryField.stringValue).UTF8String;
    updated.description = trimmed(descriptionField.stringValue).UTF8String;
    for (NSString* component in [tagsField.stringValue componentsSeparatedByString:@","]) {
        NSString* tag = trimmed(component);
        if (tag.length != 0) {
            updated.tags.emplace_back(tag.UTF8String);
        }
    }
    self.materialUidField = nil;

    const bool empty = updated.uid.empty() && updated.name.empty() &&
        updated.category.empty() && updated.description.empty() && updated.tags.empty();
    if (!empty) {
        if (const auto error = paperweight::validateMaterialMetadata(updated)) {
            [self showErrorWithTitle:@"The material information is not valid"
                             message:[NSString stringWithUTF8String:error->c_str()]];
            return;
        }
        material_.metadata = std::move(updated);
    } else {
        material_.metadata.reset();
    }
    [self markDirty];
    self.statusLabel.stringValue = @"Material information updated";
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
}

- (void)showPerformanceBenchmark:(id)sender
{
    static_cast<void>(sender);
    if (self.benchmarkWindowController == nil) {
        self.benchmarkWindowController = [[BenchmarkWindowController alloc] init];
    }
    [self.benchmarkWindowController showBenchmarkWindow];
}

- (void)buildWindow
{
    const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1430, 820)
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Untitled.pmat — Paperweight";
    self.window.minSize = NSMakeSize(1120, 720);
    // NSWindow releases itself when closed unless told otherwise. AppDelegate
    // keeps and reuses this window, so allowing that release leaves the strong
    // property pointing at a deallocated object and crashes the next editor
    // action (or application teardown while an auxiliary window is open).
    self.window.releasedWhenClosed = NO;
    self.window.delegate = self;
    NSColorPanel.sharedColorPanel.showsAlpha = YES;

    auto* content = [[NSView alloc] initWithFrame:self.window.contentView.bounds];
    content.translatesAutoresizingMaskIntoConstraints = NO;
    self.window.contentView = content;

    auto* controlsPanel = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
    controlsPanel.translatesAutoresizingMaskIntoConstraints = NO;
    controlsPanel.material = NSVisualEffectMaterialSidebar;
    controlsPanel.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    controlsPanel.state = NSVisualEffectStateActive;

    auto* layersPanel = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
    layersPanel.translatesAutoresizingMaskIntoConstraints = NO;
    layersPanel.material = NSVisualEffectMaterialSidebar;
    layersPanel.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    layersPanel.state = NSVisualEffectStateActive;

    self.previewContainer = [[NSView alloc] initWithFrame:NSZeroRect];
    self.previewContainer.translatesAutoresizingMaskIntoConstraints = NO;
    self.previewView = [[MaterialPreviewView alloc] initWithFrame:NSZeroRect];
    self.previewView.translatesAutoresizingMaskIntoConstraints = NO;
    auto* generatedTitle = makeLabel(@"Generated");
    generatedTitle.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightSemibold];
    generatedTitle.textColor = NSColor.secondaryLabelColor;
    auto* generatedPanel = [NSStackView stackViewWithViews:@[generatedTitle, self.previewView]];
    generatedPanel.translatesAutoresizingMaskIntoConstraints = NO;
    generatedPanel.orientation = NSUserInterfaceLayoutOrientationVertical;
    generatedPanel.alignment = NSLayoutAttributeLeading;
    generatedPanel.spacing = 6.0;

    self.referenceTitleLabel = makeLabel(@"Reference");
    self.referenceTitleLabel.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightSemibold];
    self.referenceTitleLabel.textColor = NSColor.secondaryLabelColor;
    self.referenceImageView = [[MaterialPreviewView alloc] initWithFrame:NSZeroRect];
    self.referenceImageView.translatesAutoresizingMaskIntoConstraints = NO;
    self.referencePanel = [NSStackView stackViewWithViews:@[
        self.referenceTitleLabel, self.referenceImageView,
    ]];
    self.referencePanel.translatesAutoresizingMaskIntoConstraints = NO;
    self.referencePanel.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.referencePanel.alignment = NSLayoutAttributeLeading;
    self.referencePanel.spacing = 6.0;
    self.referencePanel.hidden = YES;

    self.comparisonStack = [NSStackView stackViewWithViews:@[
        generatedPanel, self.referencePanel,
    ]];
    self.comparisonStack.translatesAutoresizingMaskIntoConstraints = NO;
    self.comparisonStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    self.comparisonStack.alignment = NSLayoutAttributeTop;
    self.comparisonStack.distribution = NSStackViewDistributionFillEqually;
    self.comparisonStack.spacing = 14.0;
    self.material3DPreviewView = [[Material3DPreviewView alloc] initWithFrame:NSZeroRect];
    self.material3DPreviewView.translatesAutoresizingMaskIntoConstraints = NO;
    self.material3DPreviewView.hidden = YES;
    self.material3DPreviewView.wantsLayer = YES;
    self.material3DPreviewView.layer.cornerRadius = 10.0;
    self.material3DPreviewView.layer.masksToBounds = YES;

    self.previewLoadingPanel = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
    self.previewLoadingPanel.translatesAutoresizingMaskIntoConstraints = NO;
    self.previewLoadingPanel.material = NSVisualEffectMaterialHUDWindow;
    self.previewLoadingPanel.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    self.previewLoadingPanel.state = NSVisualEffectStateActive;
    self.previewLoadingPanel.wantsLayer = YES;
    self.previewLoadingPanel.layer.cornerRadius = 9.0;
    self.previewLoadingPanel.hidden = YES;

    self.previewProgressIndicator = [[NSProgressIndicator alloc] initWithFrame:NSZeroRect];
    self.previewProgressIndicator.style = NSProgressIndicatorStyleSpinning;
    self.previewProgressIndicator.controlSize = NSControlSizeRegular;
    self.previewProgressIndicator.indeterminate = YES;
    self.previewProgressIndicator.displayedWhenStopped = NO;
    self.previewProgressIndicator.usesThreadedAnimation = YES;
    self.previewProgressIndicator.accessibilityLabel = @"Rendering preview";
    self.previewLoadingLabel = makeLabel(@"Rendering preview…");
    self.previewLoadingLabel.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightMedium];

    auto* previewLoadingStack = [NSStackView stackViewWithViews:@[
        self.previewProgressIndicator,
        self.previewLoadingLabel,
    ]];
    previewLoadingStack.translatesAutoresizingMaskIntoConstraints = NO;
    previewLoadingStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    previewLoadingStack.alignment = NSLayoutAttributeCenterY;
    previewLoadingStack.spacing = 9.0;
    [self.previewLoadingPanel addSubview:previewLoadingStack];
    [self.previewContainer addSubview:self.comparisonStack];
    [self.previewContainer addSubview:self.material3DPreviewView];
    [self.previewContainer addSubview:self.previewLoadingPanel];

    [content addSubview:controlsPanel];
    [content addSubview:layersPanel];
    [content addSubview:self.previewContainer];
    [NSLayoutConstraint activateConstraints:@[
        [controlsPanel.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [controlsPanel.topAnchor constraintEqualToAnchor:content.topAnchor],
        [controlsPanel.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
        [controlsPanel.widthAnchor constraintEqualToConstant:310.0],
        [layersPanel.leadingAnchor constraintEqualToAnchor:controlsPanel.trailingAnchor],
        [layersPanel.topAnchor constraintEqualToAnchor:content.topAnchor],
        [layersPanel.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
        [layersPanel.widthAnchor constraintEqualToConstant:340.0],
        [self.previewContainer.leadingAnchor constraintEqualToAnchor:layersPanel.trailingAnchor constant:20.0],
        [self.previewContainer.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-20.0],
        [self.previewContainer.topAnchor constraintEqualToAnchor:content.topAnchor constant:20.0],
        [self.previewContainer.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-20.0],
        [self.comparisonStack.leadingAnchor constraintEqualToAnchor:self.previewContainer.leadingAnchor],
        [self.comparisonStack.trailingAnchor constraintEqualToAnchor:self.previewContainer.trailingAnchor],
        [self.comparisonStack.topAnchor constraintEqualToAnchor:self.previewContainer.topAnchor],
        [self.comparisonStack.bottomAnchor constraintEqualToAnchor:self.previewContainer.bottomAnchor],
        [self.previewView.widthAnchor constraintEqualToAnchor:generatedPanel.widthAnchor],
        [self.previewView.bottomAnchor constraintEqualToAnchor:generatedPanel.bottomAnchor],
        [self.referenceImageView.widthAnchor constraintEqualToAnchor:self.referencePanel.widthAnchor],
        [self.referenceImageView.bottomAnchor constraintEqualToAnchor:self.referencePanel.bottomAnchor],
        [self.material3DPreviewView.leadingAnchor constraintEqualToAnchor:self.previewContainer.leadingAnchor],
        [self.material3DPreviewView.trailingAnchor constraintEqualToAnchor:self.previewContainer.trailingAnchor],
        [self.material3DPreviewView.topAnchor constraintEqualToAnchor:self.previewContainer.topAnchor],
        [self.material3DPreviewView.bottomAnchor constraintEqualToAnchor:self.previewContainer.bottomAnchor],
        [self.previewLoadingPanel.centerXAnchor constraintEqualToAnchor:self.previewContainer.centerXAnchor],
        [self.previewLoadingPanel.centerYAnchor constraintEqualToAnchor:self.previewContainer.centerYAnchor],
        [previewLoadingStack.leadingAnchor
            constraintEqualToAnchor:self.previewLoadingPanel.leadingAnchor constant:14.0],
        [previewLoadingStack.trailingAnchor
            constraintEqualToAnchor:self.previewLoadingPanel.trailingAnchor constant:-14.0],
        [previewLoadingStack.topAnchor
            constraintEqualToAnchor:self.previewLoadingPanel.topAnchor constant:10.0],
        [previewLoadingStack.bottomAnchor
            constraintEqualToAnchor:self.previewLoadingPanel.bottomAnchor constant:-10.0],
    ]];

    auto* title = makeLabel(@"Procedural Material");
    title.font = [NSFont systemFontOfSize:22.0 weight:NSFontWeightSemibold];
    auto* subtitle = makeLabel(@"Deterministic periodic FBM");
    subtitle.textColor = NSColor.secondaryLabelColor;

    auto* seedLabel = makeLabel(@"Seed");
    [seedLabel.widthAnchor constraintEqualToConstant:78.0].active = YES;
    self.seedField = [[NSTextField alloc] initWithFrame:NSZeroRect];
    self.seedField.stringValue = [NSString stringWithFormat:@"%llu", material_.seed];
    self.seedField.target = self;
    self.seedField.action = @selector(parameterChanged:);
    auto* randomButton = [NSButton buttonWithTitle:@"New" target:self action:@selector(randomiseSeed:)];
    auto* seedRow = [NSStackView stackViewWithViews:@[ seedLabel, self.seedField, randomButton ]];
    seedRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    seedRow.alignment = NSLayoutAttributeCenterY;
    seedRow.spacing = 8.0;

    const auto makePhysicalSizeRow = ^NSStackView*(
        NSString* title,
        paperweight::PhysicalSize value,
        SEL action) {
        auto* label = makeLabel(title);
        [label.widthAnchor constraintEqualToConstant:78.0].active = YES;
        auto* widthField = [[NSTextField alloc] initWithFrame:NSZeroRect];
        widthField.stringValue = [NSString stringWithFormat:@"%.6g", value.widthMetres];
        widthField.target = self;
        widthField.action = action;
        auto* by = makeLabel(@"×");
        auto* heightField = [[NSTextField alloc] initWithFrame:NSZeroRect];
        heightField.stringValue = [NSString stringWithFormat:@"%.6g", value.heightMetres];
        heightField.target = self;
        heightField.action = action;
        auto* unit = makeLabel(@"m");
        auto* row = [NSStackView stackViewWithViews:@[
            label, widthField, by, heightField, unit,
        ]];
        row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
        row.alignment = NSLayoutAttributeCenterY;
        row.spacing = 6.0;
        return row;
    };
    auto* materialSizeRow = makePhysicalSizeRow(
        @"Repeat size",
        material_.physicalSize,
        @selector(materialSizeChanged:));
    self.materialWidthField = static_cast<NSTextField*>(materialSizeRow.views[1]);
    self.materialHeightField = static_cast<NSTextField*>(materialSizeRow.views[3]);
    auto* coverageSizeRow = makePhysicalSizeRow(
        @"Coverage",
        previewCoverage_,
        @selector(coverageChanged:));
    self.coverageWidthField = static_cast<NSTextField*>(coverageSizeRow.views[1]);
    self.coverageHeightField = static_cast<NSTextField*>(coverageSizeRow.views[3]);

    const auto& frequencyMetadata = paperweight::metadataFor(paperweight::MaterialParameter::frequency);
    const auto& octavesMetadata = paperweight::metadataFor(paperweight::MaterialParameter::octaves);
    const auto& lacunarityMetadata = paperweight::metadataFor(paperweight::MaterialParameter::lacunarity);
    const auto& gainMetadata = paperweight::metadataFor(paperweight::MaterialParameter::gain);

    NSStackView* frequencyRow = makeSliderRow(
        [NSString stringWithUTF8String:frequencyMetadata.displayName.data()],
        frequencyMetadata.minimumValue,
        frequencyMetadata.maximumValue,
        material_.frequency,
        frequencyMetadata.integral,
        self);
    self.frequencySlider = static_cast<NSSlider*>(frequencyRow.views[1]);
    self.frequencyValue = static_cast<NSTextField*>(frequencyRow.views[2]);
    NSStackView* octavesRow = makeSliderRow(
        [NSString stringWithUTF8String:octavesMetadata.displayName.data()],
        octavesMetadata.minimumValue,
        octavesMetadata.maximumValue,
        material_.octaves,
        octavesMetadata.integral,
        self);
    self.octavesSlider = static_cast<NSSlider*>(octavesRow.views[1]);
    self.octavesValue = static_cast<NSTextField*>(octavesRow.views[2]);
    NSStackView* lacunarityRow = makeSliderRow(
        [NSString stringWithUTF8String:lacunarityMetadata.displayName.data()],
        lacunarityMetadata.minimumValue,
        lacunarityMetadata.maximumValue,
        material_.lacunarity,
        lacunarityMetadata.integral,
        self);
    self.lacunaritySlider = static_cast<NSSlider*>(lacunarityRow.views[1]);
    self.lacunarityValue = static_cast<NSTextField*>(lacunarityRow.views[2]);
    NSStackView* gainRow = makeSliderRow(
        [NSString stringWithUTF8String:gainMetadata.displayName.data()],
        gainMetadata.minimumValue,
        gainMetadata.maximumValue,
        material_.gain,
        gainMetadata.integral,
        self);
    self.gainSlider = static_cast<NSSlider*>(gainRow.views[1]);
    self.gainValue = static_cast<NSTextField*>(gainRow.views[2]);

    const auto& lowColourMetadata = paperweight::metadataFor(paperweight::MaterialColour::low);
    const auto& highColourMetadata = paperweight::metadataFor(paperweight::MaterialColour::high);
    NSStackView* lowColourRow = makeColourRow(
        [NSString stringWithUTF8String:lowColourMetadata.displayName.data()],
        material_.lowColour,
        self);
    self.lowColourWell = static_cast<NSColorWell*>(lowColourRow.views[1]);
    NSStackView* highColourRow = makeColourRow(
        [NSString stringWithUTF8String:highColourMetadata.displayName.data()],
        material_.highColour,
        self);
    self.highColourWell = static_cast<NSColorWell*>(highColourRow.views[1]);

    const auto& normalStrengthMetadata =
        paperweight::metadataFor(paperweight::MaterialParameter::normalStrength);
    const auto& roughnessLowMetadata =
        paperweight::metadataFor(paperweight::MaterialParameter::roughnessLow);
    const auto& roughnessHighMetadata =
        paperweight::metadataFor(paperweight::MaterialParameter::roughnessHigh);
    NSStackView* normalStrengthRow = makeSliderRow(
        @"Normal",
        normalStrengthMetadata.minimumValue,
        normalStrengthMetadata.maximumValue,
        material_.normalStrength,
        normalStrengthMetadata.integral,
        self);
    self.normalStrengthSlider = static_cast<NSSlider*>(normalStrengthRow.views[1]);
    self.normalStrengthValue = static_cast<NSTextField*>(normalStrengthRow.views[2]);
    NSStackView* roughnessLowRow = makeSliderRow(
        @"Rough low",
        roughnessLowMetadata.minimumValue,
        roughnessLowMetadata.maximumValue,
        material_.roughnessLow,
        roughnessLowMetadata.integral,
        self);
    self.roughnessLowSlider = static_cast<NSSlider*>(roughnessLowRow.views[1]);
    self.roughnessLowValue = static_cast<NSTextField*>(roughnessLowRow.views[2]);
    NSStackView* roughnessHighRow = makeSliderRow(
        @"Rough high",
        roughnessHighMetadata.minimumValue,
        roughnessHighMetadata.maximumValue,
        material_.roughnessHigh,
        roughnessHighMetadata.integral,
        self);
    self.roughnessHighSlider = static_cast<NSSlider*>(roughnessHighRow.views[1]);
    self.roughnessHighValue = static_cast<NSTextField*>(roughnessHighRow.views[2]);

    auto* previewModeLabel = makeLabel(@"Preview mode");
    self.previewModeControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.previewModeControl.segmentCount = 2;
    [self.previewModeControl setLabel:@"2D" forSegment:0];
    [self.previewModeControl setLabel:@"3D" forSegment:1];
    self.previewModeControl.selectedSegment = 0;
    self.previewModeControl.target = self;
    self.previewModeControl.action = @selector(previewModeChanged:);
    self.previewModeControl.accessibilityLabel = @"Preview mode";
    if (!self.material3DPreviewView.isRendererAvailable) {
        [self.previewModeControl setEnabled:NO forSegment:1];
        self.previewModeControl.toolTip = @"This Mac does not provide a compatible Metal device.";
    }

    auto* previewResolutionLabel = makeLabel(@"Texture resolution");
    self.previewResolutionPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.previewResolutionPopup addItemsWithTitles:@[
        @"64 × 64", @"128 × 128", @"256 × 256", @"512 × 512", @"1024 × 1024",
    ]];
    const NSArray<NSNumber*>* previewResolutions = @[@64, @128, @256, @512, @1024];
    [self.previewResolutionPopup selectItemAtIndex:static_cast<NSInteger>(
        [previewResolutions indexOfObject:@(previewResolution_)])];
    self.previewResolutionPopup.target = self;
    self.previewResolutionPopup.action = @selector(previewResolutionChanged:);
    self.previewResolutionPopup.toolTip =
        @"Sets the generated texture size for 2D, baked, and 3D previews.";

    auto* outputLabel = makeLabel(@"Material output");
    self.outputControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.outputControl.segmentCount = 4;
    [self.outputControl setLabel:@"Colour" forSegment:0];
    [self.outputControl setLabel:@"Height" forSegment:1];
    [self.outputControl setLabel:@"Normal" forSegment:2];
    [self.outputControl setLabel:@"Roughness" forSegment:3];
    self.outputControl.selectedSegment = 0;
    self.outputControl.target = self;
    self.outputControl.action = @selector(outputChanged:);
    self.outputControl.accessibilityLabel = @"Material output";
    self.bakedPresentationCheckbox = [NSButton
        checkboxWithTitle:@"Optional baked presentation"
        target:self
        action:@selector(bakedPresentationToggled:)];
    self.bakedPresentationCheckbox.toolTip =
        @"Preview a separate colour image with portable stylised lighting. The unlit material maps are unchanged.";

    auto* previewLabel = makeLabel(@"Preview tiling");
    self.tilingControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.tilingControl.segmentCount = 2;
    [self.tilingControl setLabel:@"1 × 1" forSegment:0];
    [self.tilingControl setLabel:@"3 × 3" forSegment:1];
    self.tilingControl.selectedSegment = 0;
    self.tilingControl.target = self;
    self.tilingControl.action = @selector(tilingChanged:);

    auto* bakeAzimuthRow = makePreviewSliderRow(@"Azimuth", 0.0, 360.0, 315.0, self);
    self.bakeAzimuthSlider = static_cast<NSSlider*>(bakeAzimuthRow.views[1]);
    self.bakeAzimuthValue = static_cast<NSTextField*>(bakeAzimuthRow.views[2]);
    auto* bakeElevationRow = makePreviewSliderRow(@"Elevation", -10.0, 90.0, 45.0, self);
    self.bakeElevationSlider = static_cast<NSSlider*>(bakeElevationRow.views[1]);
    self.bakeElevationValue = static_cast<NSTextField*>(bakeElevationRow.views[2]);
    auto* bakeBandsRow = makePreviewSliderRow(@"Diffuse bands", 2.0, 8.0, 3.0, self);
    self.bakeBandsSlider = static_cast<NSSlider*>(bakeBandsRow.views[1]);
    self.bakeBandsValue = static_cast<NSTextField*>(bakeBandsRow.views[2]);
    self.bakeBandsSlider.numberOfTickMarks = 7;
    self.bakeBandsSlider.allowsTickMarkValuesOnly = YES;
    auto* bakeHighlightRow = makePreviewSliderRow(@"Highlight at", 0.45, 1.0, 0.82, self);
    self.bakeHighlightSlider = static_cast<NSSlider*>(bakeHighlightRow.views[1]);
    self.bakeHighlightValue = static_cast<NSTextField*>(bakeHighlightRow.views[2]);
    auto* bakeAmbientRow = makePreviewSliderRow(@"Ambient", 0.0, 0.8, 0.22, self);
    self.bakeAmbientSlider = static_cast<NSSlider*>(bakeAmbientRow.views[1]);
    self.bakeAmbientValue = static_cast<NSTextField*>(bakeAmbientRow.views[2]);
    for (NSSlider* slider in @[
             self.bakeAzimuthSlider,
             self.bakeElevationSlider,
             self.bakeBandsSlider,
             self.bakeHighlightSlider,
             self.bakeAmbientSlider,
         ]) {
        slider.target = self;
        slider.action = @selector(bakeParameterChanged:);
    }
    self.bakeControls = [NSStackView stackViewWithViews:@[
        makeLabel(@"Baked presentation"),
        bakeAzimuthRow,
        bakeElevationRow,
        bakeBandsRow,
        bakeHighlightRow,
        bakeAmbientRow,
    ]];
    self.bakeControls.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.bakeControls.alignment = NSLayoutAttributeLeading;
    self.bakeControls.spacing = 7.0;
    self.bakeControls.hidden = YES;
    [self updateBakeControlLabels];

    auto* chooseReferenceButton = [NSButton buttonWithTitle:@"Open Reference…"
                                                      target:self
                                                      action:@selector(chooseReferenceImage:)];
    auto* hideReferenceButton = [NSButton buttonWithTitle:@"Hide Reference"
                                                    target:self
                                                    action:@selector(clearReferenceImage:)];
    auto* referenceButtons = [NSStackView stackViewWithViews:@[
        chooseReferenceButton, hideReferenceButton,
    ]];
    referenceButtons.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    referenceButtons.distribution = NSStackViewDistributionFillEqually;
    referenceButtons.spacing = 8.0;

    self.twoDPreviewControls = [NSStackView stackViewWithViews:@[
        outputLabel,
        self.outputControl,
        self.bakedPresentationCheckbox,
        self.bakeControls,
        previewLabel,
        self.tilingControl,
        referenceButtons,
    ]];
    self.twoDPreviewControls.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.twoDPreviewControls.alignment = NSLayoutAttributeLeading;
    self.twoDPreviewControls.spacing = 8.0;

    auto* shapeLabel = makeLabel(@"Inspection shape");
    [shapeLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.previewShapePopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.previewShapePopup addItemsWithTitles:@[@"Plane", @"Sphere", @"Cube", @"Cylinder"]];
    [self.previewShapePopup selectItemAtIndex:1];
    self.previewShapePopup.target = self;
    self.previewShapePopup.action = @selector(previewShapeChanged:);
    auto* shapeRow = [NSStackView stackViewWithViews:@[shapeLabel, self.previewShapePopup]];
    shapeRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    shapeRow.alignment = NSLayoutAttributeCenterY;
    shapeRow.spacing = 8.0;

    auto* lightingLabel = makeLabel(@"Lighting");
    [lightingLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.lightPresetPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.lightPresetPopup addItemsWithTitles:@[@"Studio", @"Raking", @"Top", @"Backlight", @"Custom"]];
    self.lightPresetPopup.target = self;
    self.lightPresetPopup.action = @selector(lightPresetChanged:);
    auto* lightingRow = [NSStackView stackViewWithViews:@[lightingLabel, self.lightPresetPopup]];
    lightingRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    lightingRow.alignment = NSLayoutAttributeCenterY;
    lightingRow.spacing = 8.0;

    auto* lightAzimuthRow = makePreviewSliderRow(@"Azimuth", 0.0, 360.0, 35.0, self);
    self.lightAzimuthSlider = static_cast<NSSlider*>(lightAzimuthRow.views[1]);
    self.lightAzimuthValue = static_cast<NSTextField*>(lightAzimuthRow.views[2]);
    auto* lightElevationRow = makePreviewSliderRow(@"Elevation", -10.0, 90.0, 38.0, self);
    self.lightElevationSlider = static_cast<NSSlider*>(lightElevationRow.views[1]);
    self.lightElevationValue = static_cast<NSTextField*>(lightElevationRow.views[2]);
    auto* lightIntensityRow = makePreviewSliderRow(@"Brightness", 0.2, 2.5, 1.0, self);
    self.lightIntensitySlider = static_cast<NSSlider*>(lightIntensityRow.views[1]);
    self.lightIntensityValue = static_cast<NSTextField*>(lightIntensityRow.views[2]);
    auto* ambientRow = makePreviewSliderRow(@"Ambient", 0.0, 0.8, 0.18, self);
    self.ambientIntensitySlider = static_cast<NSSlider*>(ambientRow.views[1]);
    self.ambientIntensityValue = static_cast<NSTextField*>(ambientRow.views[2]);
    auto* displacementRow = makePreviewSliderRow(@"Height", 0.0, 0.4, 0.04, self);
    self.displacementSlider = static_cast<NSSlider*>(displacementRow.views[1]);
    self.displacementValue = static_cast<NSTextField*>(displacementRow.views[2]);
    auto* previewNormalRow = makePreviewSliderRow(@"Normal", 0.0, 2.0, 1.0, self);
    self.previewNormalSlider = static_cast<NSSlider*>(previewNormalRow.views[1]);
    self.previewNormalValue = static_cast<NSTextField*>(previewNormalRow.views[2]);

    self.toonLightingCheckbox = [NSButton checkboxWithTitle:@"Toon lighting"
                                                      target:self
                                                      action:@selector(preview3DParameterChanged:)];
    self.toonLightingCheckbox.toolTip =
        @"Preview the material with discrete light bands and graphic highlights.";
    auto* toonBandsRow = makePreviewSliderRow(@"Light bands", 2.0, 6.0, 3.0, self);
    self.toonBandsSlider = static_cast<NSSlider*>(toonBandsRow.views[1]);
    self.toonBandsValue = static_cast<NSTextField*>(toonBandsRow.views[2]);
    self.toonBandsSlider.numberOfTickMarks = 5;
    self.toonBandsSlider.allowsTickMarkValuesOnly = YES;
    auto* toonSpecularRow = makePreviewSliderRow(@"Highlight", 0.0, 1.0, 0.55, self);
    self.toonSpecularSlider = static_cast<NSSlider*>(toonSpecularRow.views[1]);
    self.toonSpecularValue = static_cast<NSTextField*>(toonSpecularRow.views[2]);
    auto* toonRimRow = makePreviewSliderRow(@"Rim light", 0.0, 1.0, 0.18, self);
    self.toonRimSlider = static_cast<NSSlider*>(toonRimRow.views[1]);
    self.toonRimValue = static_cast<NSTextField*>(toonRimRow.views[2]);

    self.colourMapCheckbox = [NSButton checkboxWithTitle:@"Colour"
                                                   target:self
                                                   action:@selector(previewMapToggled:)];
    self.heightMapCheckbox = [NSButton checkboxWithTitle:@"Height"
                                                   target:self
                                                   action:@selector(previewMapToggled:)];
    self.normalMapCheckbox = [NSButton checkboxWithTitle:@"Normal"
                                                   target:self
                                                   action:@selector(previewMapToggled:)];
    self.roughnessMapCheckbox = [NSButton checkboxWithTitle:@"Roughness"
                                                      target:self
                                                      action:@selector(previewMapToggled:)];
    for (NSButton* checkbox in @[
             self.colourMapCheckbox,
             self.heightMapCheckbox,
             self.normalMapCheckbox,
             self.roughnessMapCheckbox,
         ]) {
        checkbox.state = NSControlStateValueOn;
    }
    auto* mapsFirstRow = [NSStackView stackViewWithViews:@[
        self.colourMapCheckbox, self.heightMapCheckbox,
    ]];
    mapsFirstRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    mapsFirstRow.distribution = NSStackViewDistributionFillEqually;
    auto* mapsSecondRow = [NSStackView stackViewWithViews:@[
        self.normalMapCheckbox, self.roughnessMapCheckbox,
    ]];
    mapsSecondRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    mapsSecondRow.distribution = NSStackViewDistributionFillEqually;

    auto* animationPhaseRow = makePreviewSliderRow(@"Light phase", 0.0, 1.0, 0.0, self);
    self.animationPhaseSlider = static_cast<NSSlider*>(animationPhaseRow.views[1]);
    self.animationPhaseValue = static_cast<NSTextField*>(animationPhaseRow.views[2]);
    auto* animationSpeedRow = makePreviewSliderRow(@"Speed", 0.02, 1.5, 0.25, self);
    self.animationSpeedSlider = static_cast<NSSlider*>(animationSpeedRow.views[1]);
    self.animationSpeedValue = static_cast<NSTextField*>(animationSpeedRow.views[2]);
    self.animationButton = [NSButton buttonWithTitle:@"Play Light"
                                              target:self
                                              action:@selector(togglePreviewAnimation:)];
    auto* resetCameraButton = [NSButton buttonWithTitle:@"Reset View"
                                                  target:self
                                                  action:@selector(resetPreviewCamera:)];
    auto* animationButtons = [NSStackView stackViewWithViews:@[
        self.animationButton, resetCameraButton,
    ]];
    animationButtons.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    animationButtons.distribution = NSStackViewDistributionFillEqually;
    animationButtons.spacing = 8.0;

    self.threeDPreviewControls = [NSStackView stackViewWithViews:@[
        shapeRow,
        lightingRow,
        lightAzimuthRow,
        lightElevationRow,
        lightIntensityRow,
        ambientRow,
        makeLabel(@"Material maps"),
        mapsFirstRow,
        mapsSecondRow,
        displacementRow,
        previewNormalRow,
        makeLabel(@"Stylised lighting"),
        self.toonLightingCheckbox,
        toonBandsRow,
        toonSpecularRow,
        toonRimRow,
        makeLabel(@"Animated inspection"),
        animationPhaseRow,
        animationSpeedRow,
        animationButtons,
    ]];
    self.threeDPreviewControls.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.threeDPreviewControls.alignment = NSLayoutAttributeLeading;
    self.threeDPreviewControls.spacing = 7.0;
    self.threeDPreviewControls.hidden = YES;
    [self updatePreview3DControlLabels];

    auto* resetButton = [NSButton buttonWithTitle:@"Reset Material"
                                           target:self
                                           action:@selector(resetMaterial:)];
    resetButton.bezelStyle = NSBezelStyleRounded;

    self.statusLabel = makeLabel(@"");
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
    self.statusLabel.maximumNumberOfLines = 3;
    self.statusLabel.lineBreakMode = NSLineBreakByWordWrapping;
    self.statusLabel.usesSingleLineMode = NO;
    self.statusLabel.preferredMaxLayoutWidth = 270.0;

    self.activeTemplateLabel = makeLabel(@"");
    self.activeTemplateLabel.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold];
    self.activeTemplateLabel.maximumNumberOfLines = 2;
    self.activeTemplateLabel.lineBreakMode = NSLineBreakByWordWrapping;
    self.templateControlSliders = [NSMutableArray array];
    self.templateControlValues = [NSMutableArray array];
    self.templateControlsGroup = [NSStackView stackViewWithViews:@[self.activeTemplateLabel]];
    self.templateControlsGroup.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.templateControlsGroup.alignment = NSLayoutAttributeLeading;
    self.templateControlsGroup.spacing = 7.0;
    self.templateControlsGroup.hidden = YES;

    auto* controlStack = [NSStackView stackViewWithViews:@[
        title,
        subtitle,
        self.templateControlsGroup,
        makeSeparator(),
        seedRow,
        materialSizeRow,
        coverageSizeRow,
        frequencyRow,
        octavesRow,
        lacunarityRow,
        gainRow,
        lowColourRow,
        highColourRow,
        normalStrengthRow,
        roughnessLowRow,
        roughnessHighRow,
        makeSeparator(),
        previewModeLabel,
        self.previewModeControl,
        previewResolutionLabel,
        self.previewResolutionPopup,
        self.twoDPreviewControls,
        self.threeDPreviewControls,
        resetButton,
        self.statusLabel,
    ]];
    controlStack.translatesAutoresizingMaskIntoConstraints = NO;
    controlStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    controlStack.alignment = NSLayoutAttributeLeading;
    controlStack.spacing = 10.0;
    auto* controlsDocument = [[FlippedView alloc] initWithFrame:NSZeroRect];
    controlsDocument.translatesAutoresizingMaskIntoConstraints = NO;
    [controlsDocument addSubview:controlStack];
    auto* controlsScrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    controlsScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    controlsScrollView.documentView = controlsDocument;
    controlsScrollView.hasVerticalScroller = YES;
    controlsScrollView.drawsBackground = NO;
    controlsScrollView.borderType = NSNoBorder;
    [controlsPanel addSubview:controlsScrollView];
    [NSLayoutConstraint activateConstraints:@[
        [controlsScrollView.leadingAnchor constraintEqualToAnchor:controlsPanel.leadingAnchor],
        [controlsScrollView.trailingAnchor constraintEqualToAnchor:controlsPanel.trailingAnchor],
        [controlsScrollView.topAnchor constraintEqualToAnchor:controlsPanel.topAnchor],
        [controlsScrollView.bottomAnchor constraintEqualToAnchor:controlsPanel.bottomAnchor],
        [controlsDocument.widthAnchor constraintEqualToAnchor:controlsScrollView.contentView.widthAnchor],
        [controlStack.leadingAnchor constraintEqualToAnchor:controlsDocument.leadingAnchor constant:20.0],
        [controlStack.trailingAnchor constraintEqualToAnchor:controlsDocument.trailingAnchor constant:-20.0],
        [controlStack.topAnchor constraintEqualToAnchor:controlsDocument.topAnchor constant:24.0],
        [controlStack.bottomAnchor constraintEqualToAnchor:controlsDocument.bottomAnchor constant:-24.0],
        [self.previewModeControl.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
        [self.previewResolutionPopup.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
        [self.twoDPreviewControls.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
        [self.threeDPreviewControls.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
        [self.templateControlsGroup.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
        [self.tilingControl.widthAnchor constraintEqualToAnchor:self.twoDPreviewControls.widthAnchor],
        [self.outputControl.widthAnchor constraintEqualToAnchor:self.twoDPreviewControls.widthAnchor],
        [self.bakeControls.widthAnchor constraintEqualToAnchor:self.twoDPreviewControls.widthAnchor],
        [bakeAzimuthRow.widthAnchor constraintEqualToAnchor:self.bakeControls.widthAnchor],
        [bakeElevationRow.widthAnchor constraintEqualToAnchor:self.bakeControls.widthAnchor],
        [bakeBandsRow.widthAnchor constraintEqualToAnchor:self.bakeControls.widthAnchor],
        [bakeHighlightRow.widthAnchor constraintEqualToAnchor:self.bakeControls.widthAnchor],
        [bakeAmbientRow.widthAnchor constraintEqualToAnchor:self.bakeControls.widthAnchor],
        [referenceButtons.widthAnchor constraintEqualToAnchor:self.twoDPreviewControls.widthAnchor],
        [shapeRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [lightingRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [lightAzimuthRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [lightElevationRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [lightIntensityRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [ambientRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [mapsFirstRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [mapsSecondRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [displacementRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [previewNormalRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [toonBandsRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [toonSpecularRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [toonRimRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [animationPhaseRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [animationSpeedRow.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [animationButtons.widthAnchor constraintEqualToAnchor:self.threeDPreviewControls.widthAnchor],
        [self.statusLabel.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
    ]];

    auto* layersTitle = makeLabel(@"Layer Stack");
    layersTitle.font = [NSFont systemFontOfSize:22.0 weight:NSFontWeightSemibold];
    auto* layersSubtitle = makeLabel(@"Evaluated from bottom to top");
    layersSubtitle.textColor = NSColor.secondaryLabelColor;

    self.layerListStack = [[NSStackView alloc] initWithFrame:NSZeroRect];
    self.layerListStack.translatesAutoresizingMaskIntoConstraints = NO;
    self.layerListStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.layerListStack.alignment = NSLayoutAttributeLeading;
    self.layerListStack.spacing = 4.0;

    auto* layerListDocument = [[FlippedView alloc] initWithFrame:NSZeroRect];
    layerListDocument.translatesAutoresizingMaskIntoConstraints = NO;
    [layerListDocument addSubview:self.layerListStack];
    [NSLayoutConstraint activateConstraints:@[
        [self.layerListStack.leadingAnchor constraintEqualToAnchor:layerListDocument.leadingAnchor],
        [self.layerListStack.trailingAnchor constraintEqualToAnchor:layerListDocument.trailingAnchor],
        [self.layerListStack.topAnchor constraintEqualToAnchor:layerListDocument.topAnchor],
        [self.layerListStack.bottomAnchor constraintLessThanOrEqualToAnchor:layerListDocument.bottomAnchor],
        [layerListDocument.widthAnchor constraintEqualToAnchor:self.layerListStack.widthAnchor],
    ]];

    auto* layerScrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    layerScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    layerScrollView.documentView = layerListDocument;
    layerScrollView.hasVerticalScroller = YES;
    layerScrollView.drawsBackground = NO;
    layerScrollView.borderType = NSBezelBorder;
    [layerScrollView.heightAnchor constraintEqualToConstant:210.0].active = YES;

    self.addOperationPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.addOperationPopup addItemsWithTitles:@[
        @"Noise",
        @"Solid Colour",
        @"Levels",
        @"Threshold",
        @"Brick Grid",
        @"Tile Grid",
        @"Worley Cells",
        @"Random Cells",
        @"Lines",
        @"Rectangles",
        @"Circles",
        @"Ridged Noise",
        @"Bands",
        @"Rings",
        @"Scatter",
        @"Streaks",
        @"Surface Filter",
        @"Posterise",
        @"Colour Ramp",
        @"Palette",
        @"Ink Contours",
        @"Region Field",
        @"Course Layout",
        @"Region Surface",
        @"Shape Primitive",
        @"Shape Boolean",
        @"Seam-safe Lattice",
        @"Instance Scatter",
        @"Organic Cells",
        @"Organic Cracks",
        @"Leaf Clusters",
        @"Organic Accumulation",
    ]];
    auto* addLayerButton = [NSButton buttonWithTitle:@"Add"
                                              target:self
                                              action:@selector(addLayer:)];
    auto* addRow = [NSStackView stackViewWithViews:@[ self.addOperationPopup, addLayerButton ]];
    addRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    addRow.spacing = 8.0;
    [self.addOperationPopup.widthAnchor constraintGreaterThanOrEqualToConstant:160.0].active = YES;

    self.removeLayerButton = [NSButton buttonWithTitle:@"Remove"
                                                 target:self
                                                 action:@selector(removeLayer:)];
    self.moveLayerDownButton = [NSButton buttonWithTitle:@"Move Down"
                                                   target:self
                                                   action:@selector(moveLayerDown:)];
    self.moveLayerUpButton = [NSButton buttonWithTitle:@"Move Up"
                                                 target:self
                                                 action:@selector(moveLayerUp:)];
    auto* arrangeRow = [NSStackView stackViewWithViews:@[
        self.removeLayerButton,
        self.moveLayerDownButton,
        self.moveLayerUpButton,
    ]];
    arrangeRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    arrangeRow.distribution = NSStackViewDistributionFillEqually;
    arrangeRow.spacing = 6.0;

    auto* inspectorLabel = makeLabel(@"Selected Layer");
    inspectorLabel.font = [NSFont systemFontOfSize:15.0 weight:NSFontWeightSemibold];
    self.layerTypeLabel = makeLabel(@"");
    self.layerTypeLabel.textColor = NSColor.secondaryLabelColor;
    self.layerEnabledCheckbox = [NSButton checkboxWithTitle:@"Enabled"
                                                    target:self
                                                    action:@selector(layerEnabledChanged:)];

    auto* compositeLabel = makeLabel(@"Composite");
    self.layerCompositeControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.layerCompositeControl.segmentCount = 3;
    [self.layerCompositeControl setLabel:@"Blend" forSegment:0];
    [self.layerCompositeControl setLabel:@"Add" forSegment:1];
    [self.layerCompositeControl setLabel:@"Multiply" forSegment:2];
    self.layerCompositeControl.target = self;
    self.layerCompositeControl.action = @selector(layerParameterChanged:);

    auto* opacityRow = makeLayerSliderRow(@"Opacity", 0.0, 1.0, 1.0, self);
    self.layerOpacitySlider = static_cast<NSSlider*>(opacityRow.views[1]);
    self.layerOpacityValue = static_cast<NSTextField*>(opacityRow.views[2]);

    auto* seedOffsetLabel = makeLabel(@"Seed offset");
    [seedOffsetLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.noiseSeedOffsetField = [[NSTextField alloc] initWithFrame:NSZeroRect];
    self.noiseSeedOffsetField.target = self;
    self.noiseSeedOffsetField.action = @selector(layerParameterChanged:);
    self.noiseSeedRow = [NSStackView stackViewWithViews:@[
        seedOffsetLabel,
        self.noiseSeedOffsetField,
    ]];
    self.noiseSeedRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    self.noiseSeedRow.alignment = NSLayoutAttributeCenterY;
    self.noiseSeedRow.spacing = 8.0;

    self.solidColourRow = makeColourRow(
        @"Colour", paperweight::Rgba8{128, 128, 128, 255}, self);
    self.solidColourWell = static_cast<NSColorWell*>(self.solidColourRow.views[1]);
    self.solidColourWell.action = @selector(layerColourChanged:);

    self.levelsLowRow = makeLayerSliderRow(@"Input low", 0.0, 1.0, 0.0, self);
    self.levelsLowSlider = static_cast<NSSlider*>(self.levelsLowRow.views[1]);
    self.levelsLowValue = static_cast<NSTextField*>(self.levelsLowRow.views[2]);
    self.levelsHighRow = makeLayerSliderRow(@"Input high", 0.0, 1.0, 1.0, self);
    self.levelsHighSlider = static_cast<NSSlider*>(self.levelsHighRow.views[1]);
    self.levelsHighValue = static_cast<NSTextField*>(self.levelsHighRow.views[2]);
    self.levelsGammaRow = makeLayerSliderRow(
        @"Gamma",
        paperweight::LayerLimits::minimumGamma,
        paperweight::LayerLimits::maximumGamma,
        1.0,
        self);
    self.levelsGammaSlider = static_cast<NSSlider*>(self.levelsGammaRow.views[1]);
    self.levelsGammaValue = static_cast<NSTextField*>(self.levelsGammaRow.views[2]);
    self.thresholdRow = makeLayerSliderRow(@"Threshold", 0.0, 1.0, 0.5, self);
    self.thresholdSlider = static_cast<NSSlider*>(self.thresholdRow.views[1]);
    self.thresholdValue = static_cast<NSTextField*>(self.thresholdRow.views[2]);

    self.patternCountXRow = makeLayerSliderRow(
        @"Columns", 1.0, 64.0, 6.0, self);
    self.patternCountXLabel = static_cast<NSTextField*>(self.patternCountXRow.views[0]);
    self.patternCountXSlider = static_cast<NSSlider*>(self.patternCountXRow.views[1]);
    self.patternCountXValue = static_cast<NSTextField*>(self.patternCountXRow.views[2]);
    self.patternCountXSlider.action = @selector(structuralParameterChanged:);

    self.patternCountYRow = makeLayerSliderRow(
        @"Rows", 1.0, 64.0, 6.0, self);
    self.patternCountYLabel = static_cast<NSTextField*>(self.patternCountYRow.views[0]);
    self.patternCountYSlider = static_cast<NSSlider*>(self.patternCountYRow.views[1]);
    self.patternCountYValue = static_cast<NSTextField*>(self.patternCountYRow.views[2]);
    self.patternCountYSlider.action = @selector(structuralParameterChanged:);

    self.patternValueOneRow = makeLayerSliderRow(@"Value", 0.0, 1.0, 0.5, self);
    self.patternValueOneLabel = static_cast<NSTextField*>(self.patternValueOneRow.views[0]);
    self.patternValueOneSlider = static_cast<NSSlider*>(self.patternValueOneRow.views[1]);
    self.patternValueOneValue = static_cast<NSTextField*>(self.patternValueOneRow.views[2]);
    self.patternValueOneSlider.action = @selector(structuralParameterChanged:);

    self.patternValueTwoRow = makeLayerSliderRow(@"Value", 0.0, 1.0, 0.5, self);
    self.patternValueTwoLabel = static_cast<NSTextField*>(self.patternValueTwoRow.views[0]);
    self.patternValueTwoSlider = static_cast<NSSlider*>(self.patternValueTwoRow.views[1]);
    self.patternValueTwoValue = static_cast<NSTextField*>(self.patternValueTwoRow.views[2]);
    self.patternValueTwoSlider.action = @selector(structuralParameterChanged:);

    self.patternValueThreeRow = makeLayerSliderRow(@"Softness", 0.0, 0.25, 0.02, self);
    self.patternValueThreeLabel = static_cast<NSTextField*>(self.patternValueThreeRow.views[0]);
    self.patternValueThreeSlider = static_cast<NSSlider*>(self.patternValueThreeRow.views[1]);
    self.patternValueThreeValue = static_cast<NSTextField*>(self.patternValueThreeRow.views[2]);
    self.patternValueThreeSlider.action = @selector(structuralParameterChanged:);

    self.patternValueFourRow = makeLayerSliderRow(@"Value", 0.0, 1.0, 0.5, self);
    self.patternValueFourLabel = static_cast<NSTextField*>(self.patternValueFourRow.views[0]);
    self.patternValueFourSlider = static_cast<NSSlider*>(self.patternValueFourRow.views[1]);
    self.patternValueFourValue = static_cast<NSTextField*>(self.patternValueFourRow.views[2]);
    self.patternValueFourSlider.action = @selector(structuralParameterChanged:);

    auto* surfaceKindLabel = makeLabel(@"Kind");
    [surfaceKindLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.surfaceKindPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    self.surfaceKindPopup.target = self;
    self.surfaceKindPopup.action = @selector(structuralParameterChanged:);
    self.surfaceKindRow = [NSStackView stackViewWithViews:@[
        surfaceKindLabel,
        self.surfaceKindPopup,
    ]];
    self.surfaceKindRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    self.surfaceKindRow.alignment = NSLayoutAttributeCenterY;
    self.surfaceKindRow.spacing = 8.0;

    auto* courseFieldLabel = makeLabel(@"Output");
    [courseFieldLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.courseFieldPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.courseFieldPopup addItemsWithTitles:@[
        @"Block faces", @"Mortar / gaps", @"Course interiors", @"Overlap",
    ]];
    self.courseFieldPopup.target = self;
    self.courseFieldPopup.action = @selector(structuralParameterChanged:);
    self.courseFieldRow = [NSStackView stackViewWithViews:@[
        courseFieldLabel, self.courseFieldPopup,
    ]];
    self.courseFieldRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    self.courseFieldRow.alignment = NSLayoutAttributeCenterY;
    self.courseFieldRow.spacing = 8.0;

    self.courseGapRow = makeLayerSliderRow(@"Gap width", 0.0, 0.95, 0.08, self);
    self.courseGapSlider = static_cast<NSSlider*>(self.courseGapRow.views[1]);
    self.courseGapValue = static_cast<NSTextField*>(self.courseGapRow.views[2]);
    self.courseGapSlider.action = @selector(structuralParameterChanged:);
    self.courseSoftnessRow = makeLayerSliderRow(@"Softness", 0.0, 0.25, 0.02, self);
    self.courseSoftnessSlider = static_cast<NSSlider*>(self.courseSoftnessRow.views[1]);
    self.courseSoftnessValue = static_cast<NSTextField*>(self.courseSoftnessRow.views[2]);
    self.courseSoftnessSlider.action = @selector(structuralParameterChanged:);
    self.courseOverlapRow = makeLayerSliderRow(@"Overlap", 0.0, 0.95, 0.25, self);
    self.courseOverlapSlider = static_cast<NSSlider*>(self.courseOverlapRow.views[1]);
    self.courseOverlapValue = static_cast<NSTextField*>(self.courseOverlapRow.views[2]);
    self.courseOverlapSlider.action = @selector(structuralParameterChanged:);

    auto* processingTargetLabel = makeLabel(@"Affect");
    [processingTargetLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.processingTargetPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.processingTargetPopup addItemsWithTitles:@[
        @"Colour only", @"Surface only", @"Colour + surface",
    ]];
    self.processingTargetPopup.target = self;
    self.processingTargetPopup.action = @selector(styliseParameterChanged:);
    self.processingTargetRow = [NSStackView stackViewWithViews:@[
        processingTargetLabel, self.processingTargetPopup,
    ]];
    self.processingTargetRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    self.processingTargetRow.alignment = NSLayoutAttributeCenterY;
    self.processingTargetRow.spacing = 8.0;

    self.filterSensitivityRow = makeLayerSliderRow(
        @"Sensitivity", 0.0, 1.0, 0.2, self);
    self.filterSensitivitySlider = static_cast<NSSlider*>(self.filterSensitivityRow.views[1]);
    self.filterSensitivityValue = static_cast<NSTextField*>(self.filterSensitivityRow.views[2]);
    self.filterSensitivitySlider.action = @selector(styliseParameterChanged:);

    self.posteriseBandsRow = makeLayerSliderRow(
        @"Bands", paperweight::LayerLimits::minimumPosteriseBands,
        paperweight::LayerLimits::maximumPosteriseBands, 4.0, self);
    self.posteriseBandsSlider = static_cast<NSSlider*>(self.posteriseBandsRow.views[1]);
    self.posteriseBandsValue = static_cast<NSTextField*>(self.posteriseBandsRow.views[2]);
    self.posteriseBandsSlider.action = @selector(styliseParameterChanged:);
    self.posteriseBandsSlider.numberOfTickMarks =
        paperweight::LayerLimits::maximumPosteriseBands -
        paperweight::LayerLimits::minimumPosteriseBands + 1;
    self.posteriseBandsSlider.allowsTickMarkValuesOnly = YES;

    auto* rampModeLabel = makeLabel(@"Ramp");
    [rampModeLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.rampModePopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.rampModePopup addItemsWithTitles:@[@"Smooth", @"Stepped"]];
    self.rampModePopup.target = self;
    self.rampModePopup.action = @selector(styliseParameterChanged:);
    self.rampModeRow = [NSStackView stackViewWithViews:@[
        rampModeLabel, self.rampModePopup,
    ]];
    self.rampModeRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    self.rampModeRow.alignment = NSLayoutAttributeCenterY;
    self.rampModeRow.spacing = 8.0;

    self.colourEntryRows = [NSMutableArray array];
    self.colourEntryLabels = [NSMutableArray array];
    self.colourPositionSliders = [NSMutableArray array];
    self.colourPositionValues = [NSMutableArray array];
    self.colourEntryWells = [NSMutableArray array];
    self.colourEntriesGroup = [[NSStackView alloc] initWithFrame:NSZeroRect];
    self.colourEntriesGroup.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.colourEntriesGroup.alignment = NSLayoutAttributeLeading;
    self.colourEntriesGroup.spacing = 6.0;
    for (NSInteger index = 0;
         index < static_cast<NSInteger>(paperweight::LayerLimits::maximumColourStops);
         ++index) {
        auto* entryLabel = makeLabel([NSString stringWithFormat:@"Stop %ld", index + 1]);
        [entryLabel.widthAnchor constraintEqualToConstant:52.0].active = YES;
        auto* position = [NSSlider sliderWithValue:index == 0 ? 0.0 : 1.0
                                          minValue:0.0
                                          maxValue:1.0
                                            target:self
                                            action:@selector(styliseParameterChanged:)];
        position.continuous = YES;
        position.tag = index;
        auto* value = makeLabel(index == 0 ? @"0.00" : @"1.00");
        value.alignment = NSTextAlignmentRight;
        value.font = [NSFont monospacedDigitSystemFontOfSize:11.0
                                                       weight:NSFontWeightRegular];
        [value.widthAnchor constraintEqualToConstant:34.0].active = YES;
        auto* well = [[NSColorWell alloc] initWithFrame:NSZeroRect];
        well.target = self;
        well.action = @selector(styliseColourChanged:);
        well.tag = index;
        well.accessibilityLabel = [NSString stringWithFormat:@"Colour %ld", index + 1];
        [well.widthAnchor constraintEqualToConstant:36.0].active = YES;
        [well.heightAnchor constraintEqualToConstant:25.0].active = YES;
        auto* entryRow = [NSStackView stackViewWithViews:@[entryLabel, position, value, well]];
        entryRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
        entryRow.alignment = NSLayoutAttributeCenterY;
        entryRow.spacing = 5.0;
        [self.colourEntriesGroup addArrangedSubview:entryRow];
        [entryRow.widthAnchor constraintEqualToAnchor:self.colourEntriesGroup.widthAnchor].active = YES;
        [self.colourEntryRows addObject:entryRow];
        [self.colourEntryLabels addObject:entryLabel];
        [self.colourPositionSliders addObject:position];
        [self.colourPositionValues addObject:value];
        [self.colourEntryWells addObject:well];
    }
    self.addColourEntryButton = [NSButton buttonWithTitle:@"Add Colour"
                                                    target:self
                                                    action:@selector(addStyliseColour:)];
    self.removeColourEntryButton = [NSButton buttonWithTitle:@"Remove Colour"
                                                       target:self
                                                       action:@selector(removeStyliseColour:)];
    auto* colourEntryButtons = [NSStackView stackViewWithViews:@[
        self.addColourEntryButton, self.removeColourEntryButton,
    ]];
    colourEntryButtons.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    colourEntryButtons.distribution = NSStackViewDistributionFillEqually;
    colourEntryButtons.spacing = 6.0;
    [self.colourEntriesGroup addArrangedSubview:colourEntryButtons];
    [colourEntryButtons.widthAnchor constraintEqualToAnchor:self.colourEntriesGroup.widthAnchor].active = YES;

    self.inkColourRow = makeColourRow(@"Ink colour", {24, 24, 28, 255}, self);
    self.inkColourWell = static_cast<NSColorWell*>(self.inkColourRow.views[1]);
    self.inkColourWell.action = @selector(styliseColourChanged:);
    self.inkRadiusRow = makeLayerSliderRow(@"Radius", 0.0, 0.25, 0.01, self);
    self.inkRadiusSlider = static_cast<NSSlider*>(self.inkRadiusRow.views[1]);
    self.inkRadiusValue = static_cast<NSTextField*>(self.inkRadiusRow.views[2]);
    self.inkRadiusSlider.action = @selector(styliseParameterChanged:);
    self.inkThresholdRow = makeLayerSliderRow(@"Threshold", 0.0, 1.0, 0.12, self);
    self.inkThresholdSlider = static_cast<NSSlider*>(self.inkThresholdRow.views[1]);
    self.inkThresholdValue = static_cast<NSTextField*>(self.inkThresholdRow.views[2]);
    self.inkThresholdSlider.action = @selector(styliseParameterChanged:);
    self.inkSoftnessRow = makeLayerSliderRow(
        @"Softness", paperweight::LayerLimits::minimumContourSoftness,
        paperweight::LayerLimits::maximumContourSoftness, 0.05, self);
    self.inkSoftnessSlider = static_cast<NSSlider*>(self.inkSoftnessRow.views[1]);
    self.inkSoftnessValue = static_cast<NSTextField*>(self.inkSoftnessRow.views[2]);
    self.inkSoftnessSlider.action = @selector(styliseParameterChanged:);
    self.inkStrengthRow = makeLayerSliderRow(@"Strength", 0.0, 1.0, 1.0, self);
    self.inkStrengthSlider = static_cast<NSSlider*>(self.inkStrengthRow.views[1]);
    self.inkStrengthValue = static_cast<NSTextField*>(self.inkStrengthRow.views[2]);
    self.inkStrengthSlider.action = @selector(styliseParameterChanged:);
    self.inkInvertedCheckbox = [NSButton checkboxWithTitle:@"Ink flat regions"
                                                      target:self
                                                      action:@selector(styliseParameterChanged:)];
    self.facetedNormalsCheckbox = [NSButton
        checkboxWithTitle:@"Use deliberate planar normals"
                   target:self
                   action:@selector(structuralParameterChanged:)];
    self.facetedNormalsCheckbox.toolTip =
        @"Preserve the seeded planar faces in the normal map without changing colour, height, or roughness.";

    self.equalMortarWidthCheckbox = [NSButton
        checkboxWithTitle:@"Equal horizontal/vertical mortar"
                   target:self
                   action:@selector(structuralParameterChanged:)];
    self.equalMortarWidthCheckbox.toolTip =
        @"Interpret mortar as one texture-space width on both axes.";
    self.physicalBrickCheckbox = [NSButton
        checkboxWithTitle:@"Size brick in metres"
                   target:self
                   action:@selector(structuralParameterChanged:)];
    self.physicalBrickCheckbox.toolTip =
        @"Keep brick and mortar dimensions consistent in world space.";
    self.physicalBrickWidthRow = makeMetreFieldRow(
        @"Brick width",
        0.24,
        self,
        @selector(structuralParameterChanged:));
    self.physicalBrickWidthField = static_cast<NSTextField*>(
        self.physicalBrickWidthRow.views[1]);
    self.physicalBrickWidthField.toolTip =
        @"Enter the real width of one brick. The seamless repeat is recalculated automatically.";
    self.physicalBrickHeightRow = makeMetreFieldRow(
        @"Brick height",
        0.075,
        self,
        @selector(structuralParameterChanged:));
    self.physicalBrickHeightField = static_cast<NSTextField*>(
        self.physicalBrickHeightRow.views[1]);
    self.physicalBrickHeightField.toolTip =
        @"Enter the real height of one brick. The seamless repeat is recalculated automatically.";
    self.physicalBrickMortarRow = makeMetreFieldRow(
        @"Mortar",
        0.01,
        self,
        @selector(structuralParameterChanged:));
    self.physicalBrickMortarField = static_cast<NSTextField*>(
        self.physicalBrickMortarRow.views[1]);
    self.physicalBrickMortarField.toolTip =
        @"Enter one real mortar width used equally in both directions.";
    self.physicalCourseOverlapRow = makeMetreFieldRow(
        @"Overlap",
        0.04,
        self,
        @selector(structuralParameterChanged:));
    self.physicalCourseOverlapField = static_cast<NSTextField*>(
        self.physicalCourseOverlapRow.views[1]);
    self.physicalCourseOverlapField.toolTip =
        @"Enter the physical overlap depth for slate courses.";
    self.physicalBrickSummary = makeLabel(@"");
    self.physicalBrickSummary.font = [NSFont systemFontOfSize:11.0];
    self.physicalBrickSummary.textColor = NSColor.secondaryLabelColor;
    self.physicalBrickSummary.lineBreakMode = NSLineBreakByWordWrapping;
    self.physicalBrickSummary.maximumNumberOfLines = 2;
    self.physicalBrickSummary.toolTip =
        @"Paperweight calculates the seamless repeat from brick size multiplied by columns and rows.";

    auto* directionLabel = makeLabel(@"Direction");
    [directionLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.patternDirectionControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.patternDirectionControl.segmentCount = 2;
    [self.patternDirectionControl setLabel:@"Vertical" forSegment:0];
    [self.patternDirectionControl setLabel:@"Horizontal" forSegment:1];
    self.patternDirectionControl.selectedSegment = 0;
    self.patternDirectionControl.target = self;
    self.patternDirectionControl.action = @selector(structuralParameterChanged:);
    self.patternDirectionRow = [NSStackView stackViewWithViews:@[
        directionLabel,
        self.patternDirectionControl,
    ]];
    self.patternDirectionRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    self.patternDirectionRow.alignment = NSLayoutAttributeCenterY;
    self.patternDirectionRow.spacing = 8.0;

    auto* patternSeedLabel = makeLabel(@"Seed offset");
    [patternSeedLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.patternSeedOffsetField = [[NSTextField alloc] initWithFrame:NSZeroRect];
    self.patternSeedOffsetField.target = self;
    self.patternSeedOffsetField.action = @selector(structuralParameterChanged:);
    self.patternSeedRow = [NSStackView stackViewWithViews:@[
        patternSeedLabel,
        self.patternSeedOffsetField,
    ]];
    self.patternSeedRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    self.patternSeedRow.alignment = NSLayoutAttributeCenterY;
    self.patternSeedRow.spacing = 8.0;

    self.layerInspectorTabs = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.layerInspectorTabs.segmentCount = 3;
    [self.layerInspectorTabs setLabel:@"Layer" forSegment:0];
    [self.layerInspectorTabs setLabel:@"Transform" forSegment:1];
    [self.layerInspectorTabs setLabel:@"Mask" forSegment:2];
    self.layerInspectorTabs.selectedSegment = 0;
    self.layerInspectorTabs.target = self;
    self.layerInspectorTabs.action = @selector(layerInspectorTabChanged:);

    self.layerSettingsGroup = [NSStackView stackViewWithViews:@[
        self.layerEnabledCheckbox,
        compositeLabel,
        self.layerCompositeControl,
        opacityRow,
        self.noiseSeedRow,
        self.solidColourRow,
        self.levelsLowRow,
        self.levelsHighRow,
        self.levelsGammaRow,
        self.thresholdRow,
        self.physicalBrickCheckbox,
        self.physicalBrickWidthRow,
        self.physicalBrickHeightRow,
        self.physicalBrickMortarRow,
        self.physicalCourseOverlapRow,
        self.patternCountXRow,
        self.patternCountYRow,
        self.physicalBrickSummary,
        self.surfaceKindRow,
        self.courseFieldRow,
        self.patternValueOneRow,
        self.patternValueTwoRow,
        self.patternValueThreeRow,
        self.patternValueFourRow,
        self.courseGapRow,
        self.courseSoftnessRow,
        self.courseOverlapRow,
        self.equalMortarWidthCheckbox,
        self.patternDirectionRow,
        self.patternSeedRow,
        self.processingTargetRow,
        self.filterSensitivityRow,
        self.posteriseBandsRow,
        self.rampModeRow,
        self.colourEntriesGroup,
        self.inkColourRow,
        self.inkRadiusRow,
        self.inkThresholdRow,
        self.inkSoftnessRow,
        self.inkStrengthRow,
        self.inkInvertedCheckbox,
        self.facetedNormalsCheckbox,
    ]];
    self.layerSettingsGroup.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.layerSettingsGroup.alignment = NSLayoutAttributeLeading;
    self.layerSettingsGroup.spacing = 9.0;

    auto* scaleXRow = makeLayerSliderRow(
        @"Scale X",
        paperweight::LayerLimits::minimumScale,
        paperweight::LayerLimits::maximumScale,
        1.0,
        self);
    self.transformScaleXSlider = static_cast<NSSlider*>(scaleXRow.views[1]);
    self.transformScaleXValue = static_cast<NSTextField*>(scaleXRow.views[2]);
    self.transformScaleXSlider.action = @selector(transformParameterChanged:);
    self.transformScaleXSlider.numberOfTickMarks = paperweight::LayerLimits::maximumScale;
    self.transformScaleXSlider.allowsTickMarkValuesOnly = YES;

    auto* scaleYRow = makeLayerSliderRow(
        @"Scale Y",
        paperweight::LayerLimits::minimumScale,
        paperweight::LayerLimits::maximumScale,
        1.0,
        self);
    self.transformScaleYSlider = static_cast<NSSlider*>(scaleYRow.views[1]);
    self.transformScaleYValue = static_cast<NSTextField*>(scaleYRow.views[2]);
    self.transformScaleYSlider.action = @selector(transformParameterChanged:);
    self.transformScaleYSlider.numberOfTickMarks = paperweight::LayerLimits::maximumScale;
    self.transformScaleYSlider.allowsTickMarkValuesOnly = YES;

    auto* offsetXRow = makeLayerSliderRow(@"Offset X", -1.0, 1.0, 0.0, self);
    self.transformOffsetXSlider = static_cast<NSSlider*>(offsetXRow.views[1]);
    self.transformOffsetXValue = static_cast<NSTextField*>(offsetXRow.views[2]);
    self.transformOffsetXSlider.action = @selector(transformParameterChanged:);
    auto* offsetYRow = makeLayerSliderRow(@"Offset Y", -1.0, 1.0, 0.0, self);
    self.transformOffsetYSlider = static_cast<NSSlider*>(offsetYRow.views[1]);
    self.transformOffsetYValue = static_cast<NSTextField*>(offsetYRow.views[2]);
    self.transformOffsetYSlider.action = @selector(transformParameterChanged:);

    auto* rotationLabel = makeLabel(@"Rotation");
    [rotationLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.transformRotationControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.transformRotationControl.segmentCount = 4;
    [self.transformRotationControl setLabel:@"0°" forSegment:0];
    [self.transformRotationControl setLabel:@"90°" forSegment:1];
    [self.transformRotationControl setLabel:@"180°" forSegment:2];
    [self.transformRotationControl setLabel:@"270°" forSegment:3];
    self.transformRotationControl.selectedSegment = 0;
    self.transformRotationControl.target = self;
    self.transformRotationControl.action = @selector(transformParameterChanged:);
    auto* rotationRow = [NSStackView stackViewWithViews:@[
        rotationLabel,
        self.transformRotationControl,
    ]];
    rotationRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    rotationRow.alignment = NSLayoutAttributeCenterY;
    rotationRow.spacing = 8.0;

    self.warpEnabledCheckbox = [NSButton checkboxWithTitle:@"Enable periodic warp"
                                                    target:self
                                                    action:@selector(transformParameterChanged:)];
    auto* warpStrengthRow = makeLayerSliderRow(@"Strength", 0.0, 1.0, 0.0, self);
    self.warpStrengthSlider = static_cast<NSSlider*>(warpStrengthRow.views[1]);
    self.warpStrengthValue = static_cast<NSTextField*>(warpStrengthRow.views[2]);
    self.warpStrengthSlider.action = @selector(transformParameterChanged:);
    auto* warpFrequencyRow = makeLayerSliderRow(
        @"Frequency",
        paperweight::LayerLimits::minimumWarpFrequency,
        paperweight::LayerLimits::maximumWarpFrequency,
        1.0,
        self);
    self.warpFrequencySlider = static_cast<NSSlider*>(warpFrequencyRow.views[1]);
    self.warpFrequencyValue = static_cast<NSTextField*>(warpFrequencyRow.views[2]);
    self.warpFrequencySlider.action = @selector(transformParameterChanged:);
    self.warpFrequencySlider.numberOfTickMarks = paperweight::LayerLimits::maximumWarpFrequency;
    self.warpFrequencySlider.allowsTickMarkValuesOnly = YES;
    auto* warpSeedLabel = makeLabel(@"Warp seed");
    [warpSeedLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.warpSeedOffsetField = [[NSTextField alloc] initWithFrame:NSZeroRect];
    self.warpSeedOffsetField.target = self;
    self.warpSeedOffsetField.action = @selector(transformParameterChanged:);
    auto* warpSeedRow = [NSStackView stackViewWithViews:@[
        warpSeedLabel,
        self.warpSeedOffsetField,
    ]];
    warpSeedRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    warpSeedRow.alignment = NSLayoutAttributeCenterY;
    warpSeedRow.spacing = 8.0;

    self.transformSettingsGroup = [NSStackView stackViewWithViews:@[
        scaleXRow,
        scaleYRow,
        offsetXRow,
        offsetYRow,
        rotationRow,
        makeSeparator(),
        self.warpEnabledCheckbox,
        warpStrengthRow,
        warpFrequencyRow,
        warpSeedRow,
    ]];
    self.transformSettingsGroup.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.transformSettingsGroup.alignment = NSLayoutAttributeLeading;
    self.transformSettingsGroup.spacing = 9.0;

    self.maskEnabledCheckbox = [NSButton checkboxWithTitle:@"Enable noise mask"
                                                    target:self
                                                    action:@selector(maskParameterChanged:)];
    self.maskInvertedCheckbox = [NSButton checkboxWithTitle:@"Invert mask"
                                                     target:self
                                                     action:@selector(maskParameterChanged:)];
    auto* maskSeedLabel = makeLabel(@"Mask seed");
    [maskSeedLabel.widthAnchor constraintEqualToConstant:72.0].active = YES;
    self.maskSeedOffsetField = [[NSTextField alloc] initWithFrame:NSZeroRect];
    self.maskSeedOffsetField.target = self;
    self.maskSeedOffsetField.action = @selector(maskParameterChanged:);
    auto* maskSeedRow = [NSStackView stackViewWithViews:@[
        maskSeedLabel,
        self.maskSeedOffsetField,
    ]];
    maskSeedRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    maskSeedRow.alignment = NSLayoutAttributeCenterY;
    maskSeedRow.spacing = 8.0;
    auto* maskLowRow = makeLayerSliderRow(@"Input low", 0.0, 1.0, 0.0, self);
    self.maskLowSlider = static_cast<NSSlider*>(maskLowRow.views[1]);
    self.maskLowValue = static_cast<NSTextField*>(maskLowRow.views[2]);
    self.maskLowSlider.action = @selector(maskParameterChanged:);
    auto* maskHighRow = makeLayerSliderRow(@"Input high", 0.0, 1.0, 1.0, self);
    self.maskHighSlider = static_cast<NSSlider*>(maskHighRow.views[1]);
    self.maskHighValue = static_cast<NSTextField*>(maskHighRow.views[2]);
    self.maskHighSlider.action = @selector(maskParameterChanged:);
    self.maskSettingsGroup = [NSStackView stackViewWithViews:@[
        self.maskEnabledCheckbox,
        self.maskInvertedCheckbox,
        maskSeedRow,
        maskLowRow,
        maskHighRow,
    ]];
    self.maskSettingsGroup.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.maskSettingsGroup.alignment = NSLayoutAttributeLeading;
    self.maskSettingsGroup.spacing = 9.0;

    auto* layerStack = [NSStackView stackViewWithViews:@[
        layersTitle,
        layersSubtitle,
        makeSeparator(),
        layerScrollView,
        addRow,
        arrangeRow,
        makeSeparator(),
        inspectorLabel,
        self.layerTypeLabel,
        self.layerInspectorTabs,
        self.layerSettingsGroup,
        self.transformSettingsGroup,
        self.maskSettingsGroup,
    ]];
    layerStack.translatesAutoresizingMaskIntoConstraints = NO;
    layerStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    layerStack.alignment = NSLayoutAttributeLeading;
    layerStack.spacing = 9.0;
    [layersPanel addSubview:layerStack];
    [NSLayoutConstraint activateConstraints:@[
        [layerStack.leadingAnchor constraintEqualToAnchor:layersPanel.leadingAnchor constant:18.0],
        [layerStack.trailingAnchor constraintEqualToAnchor:layersPanel.trailingAnchor constant:-18.0],
        [layerStack.topAnchor constraintEqualToAnchor:layersPanel.topAnchor constant:24.0],
        [layerScrollView.widthAnchor constraintEqualToAnchor:layerStack.widthAnchor],
        [addRow.widthAnchor constraintEqualToAnchor:layerStack.widthAnchor],
        [arrangeRow.widthAnchor constraintEqualToAnchor:layerStack.widthAnchor],
        [self.layerInspectorTabs.widthAnchor constraintEqualToAnchor:layerStack.widthAnchor],
        [self.layerSettingsGroup.widthAnchor constraintEqualToAnchor:layerStack.widthAnchor],
        [self.transformSettingsGroup.widthAnchor constraintEqualToAnchor:layerStack.widthAnchor],
        [self.maskSettingsGroup.widthAnchor constraintEqualToAnchor:layerStack.widthAnchor],
        [self.layerCompositeControl.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.physicalBrickWidthRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.physicalBrickHeightRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.physicalBrickMortarRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.physicalCourseOverlapRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.physicalBrickSummary.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternCountXRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternCountYRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternValueOneRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternValueTwoRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternValueThreeRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternValueFourRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.surfaceKindRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.surfaceKindPopup.trailingAnchor constraintEqualToAnchor:self.surfaceKindRow.trailingAnchor],
        [self.courseFieldRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.courseFieldPopup.trailingAnchor constraintEqualToAnchor:self.courseFieldRow.trailingAnchor],
        [self.courseGapRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.courseSoftnessRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.courseOverlapRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.processingTargetRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.processingTargetPopup.trailingAnchor constraintEqualToAnchor:self.processingTargetRow.trailingAnchor],
        [self.filterSensitivityRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.posteriseBandsRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.rampModeRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.rampModePopup.trailingAnchor constraintEqualToAnchor:self.rampModeRow.trailingAnchor],
        [self.colourEntriesGroup.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.inkColourRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.inkRadiusRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.inkThresholdRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.inkSoftnessRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.inkStrengthRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternDirectionRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternSeedRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternDirectionControl.trailingAnchor
            constraintEqualToAnchor:self.patternDirectionRow.trailingAnchor],
        [scaleXRow.widthAnchor constraintEqualToAnchor:self.transformSettingsGroup.widthAnchor],
        [scaleYRow.widthAnchor constraintEqualToAnchor:self.transformSettingsGroup.widthAnchor],
        [offsetXRow.widthAnchor constraintEqualToAnchor:self.transformSettingsGroup.widthAnchor],
        [offsetYRow.widthAnchor constraintEqualToAnchor:self.transformSettingsGroup.widthAnchor],
        [rotationRow.widthAnchor constraintEqualToAnchor:self.transformSettingsGroup.widthAnchor],
        [warpStrengthRow.widthAnchor constraintEqualToAnchor:self.transformSettingsGroup.widthAnchor],
        [warpFrequencyRow.widthAnchor constraintEqualToAnchor:self.transformSettingsGroup.widthAnchor],
        [warpSeedRow.widthAnchor constraintEqualToAnchor:self.transformSettingsGroup.widthAnchor],
        [self.transformRotationControl.trailingAnchor
            constraintEqualToAnchor:rotationRow.trailingAnchor],
        [maskSeedRow.widthAnchor constraintEqualToAnchor:self.maskSettingsGroup.widthAnchor],
        [maskLowRow.widthAnchor constraintEqualToAnchor:self.maskSettingsGroup.widthAnchor],
        [maskHighRow.widthAnchor constraintEqualToAnchor:self.maskSettingsGroup.widthAnchor],
    ]];

    [self rebuildLayerList];
    [self refreshLayerInspector];
}

- (void)rebuildLayerList
{
    for (NSView* view in [self.layerListStack.arrangedSubviews copy]) {
        [self.layerListStack removeArrangedSubview:view];
        [view removeFromSuperview];
    }

    for (std::size_t displayIndex = material_.layers.size();
         displayIndex > 0;
         --displayIndex) {
        const std::size_t index = displayIndex - 1;
        const auto& layer = material_.layers[index];
        auto* enabled = [NSButton checkboxWithTitle:@""
                                            target:self
                                            action:@selector(layerListEnabledChanged:)];
        enabled.tag = static_cast<NSInteger>(index);
        enabled.state = layer.enabled ? NSControlStateValueOn : NSControlStateValueOff;
        enabled.accessibilityLabel = [NSString
            stringWithFormat:@"Enable layer %zu", index + 1];
        [enabled.widthAnchor constraintEqualToConstant:22.0].active = YES;

        NSString* marker = selectedLayer_ == static_cast<NSInteger>(index) ? @"› " : @"  ";
        NSString* name = [NSString
            stringWithFormat:@"%@%zu  %@", marker, index + 1, operationDisplayName(layer.operation)];
        auto* select = [NSButton buttonWithTitle:name
                                          target:self
                                          action:@selector(selectLayer:)];
        select.tag = static_cast<NSInteger>(index);
        select.bezelStyle = NSBezelStyleInline;
        select.alignment = NSTextAlignmentLeft;
        select.font = selectedLayer_ == static_cast<NSInteger>(index)
            ? [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold]
            : [NSFont systemFontOfSize:13.0 weight:NSFontWeightRegular];

        auto* row = [NSStackView stackViewWithViews:@[ enabled, select ]];
        row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
        row.alignment = NSLayoutAttributeCenterY;
        row.spacing = 3.0;
        [self.layerListStack addArrangedSubview:row];
        [row.widthAnchor constraintEqualToAnchor:self.layerListStack.widthAnchor].active = YES;
    }
}

- (void)updateLayerInspectorLiveValueLabels
{
    self.layerOpacityValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.layerOpacitySlider.doubleValue];
    self.levelsLowValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.levelsLowSlider.doubleValue];
    self.levelsHighValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.levelsHighSlider.doubleValue];
    self.levelsGammaValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.levelsGammaSlider.doubleValue];
    self.thresholdValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.thresholdSlider.doubleValue];

    self.patternCountXValue.stringValue = [NSString
        stringWithFormat:@"%.0f", self.patternCountXSlider.doubleValue];
    self.patternCountYValue.stringValue = [NSString
        stringWithFormat:@"%.0f", self.patternCountYSlider.doubleValue];
    self.patternValueOneValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.patternValueOneSlider.doubleValue];
    self.patternValueTwoValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.patternValueTwoSlider.doubleValue];
    self.patternValueThreeValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.patternValueThreeSlider.doubleValue];
    self.patternValueFourValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.patternValueFourSlider.doubleValue];
    self.courseGapValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.courseGapSlider.doubleValue];
    self.courseSoftnessValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.courseSoftnessSlider.doubleValue];
    self.courseOverlapValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.courseOverlapSlider.doubleValue];

    self.transformScaleXValue.stringValue = [NSString
        stringWithFormat:@"%.0f", self.transformScaleXSlider.doubleValue];
    self.transformScaleYValue.stringValue = [NSString
        stringWithFormat:@"%.0f", self.transformScaleYSlider.doubleValue];
    self.transformOffsetXValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.transformOffsetXSlider.doubleValue];
    self.transformOffsetYValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.transformOffsetYSlider.doubleValue];
    self.warpStrengthValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.warpStrengthSlider.doubleValue];
    self.warpFrequencyValue.stringValue = [NSString
        stringWithFormat:@"%.0f", self.warpFrequencySlider.doubleValue];

    self.maskLowValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.maskLowSlider.doubleValue];
    self.maskHighValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.maskHighSlider.doubleValue];
}

- (void)refreshLayerInspector
{
    const bool repeatIsDerived = materialUsesDerivedRepeat(material_);
    self.materialWidthField.editable = !repeatIsDerived;
    self.materialHeightField.editable = !repeatIsDerived;
    NSString* repeatToolTip = repeatIsDerived
        ? @"Calculated automatically from the active physical layout dimensions and counts."
        : @"The physical size of one seamless material repeat.";
    self.materialWidthField.toolTip = repeatToolTip;
    self.materialHeightField.toolTip = repeatToolTip;

    auto* layer = layerAt(material_, selectedLayer_);
    const BOOL hasLayer = layer != nullptr;
    self.removeLayerButton.enabled = material_.layers.size() > 1;
    self.moveLayerDownButton.enabled = hasLayer && selectedLayer_ > 0;
    self.moveLayerUpButton.enabled = hasLayer &&
        static_cast<std::size_t>(selectedLayer_ + 1) < material_.layers.size();
    self.layerEnabledCheckbox.enabled = hasLayer;
    self.layerCompositeControl.enabled = hasLayer;
    self.layerOpacitySlider.enabled = hasLayer;
    self.layerInspectorTabs.enabled = hasLayer;
    if (!hasLayer) {
        self.layerTypeLabel.stringValue = @"No layer selected";
        self.noiseSeedRow.hidden = YES;
        self.solidColourRow.hidden = YES;
        self.levelsLowRow.hidden = YES;
        self.levelsHighRow.hidden = YES;
        self.levelsGammaRow.hidden = YES;
        self.thresholdRow.hidden = YES;
        self.patternCountXRow.hidden = YES;
        self.patternCountYRow.hidden = YES;
        self.patternValueOneRow.hidden = YES;
        self.patternValueTwoRow.hidden = YES;
        self.patternValueThreeRow.hidden = YES;
        self.patternValueFourRow.hidden = YES;
        self.surfaceKindRow.hidden = YES;
        self.courseFieldRow.hidden = YES;
        self.courseGapRow.hidden = YES;
        self.courseSoftnessRow.hidden = YES;
        self.courseOverlapRow.hidden = YES;
        self.equalMortarWidthCheckbox.hidden = YES;
        self.physicalBrickCheckbox.hidden = YES;
        self.physicalBrickWidthRow.hidden = YES;
        self.physicalBrickHeightRow.hidden = YES;
        self.physicalBrickMortarRow.hidden = YES;
        self.physicalCourseOverlapRow.hidden = YES;
        self.physicalBrickSummary.hidden = YES;
        self.patternDirectionRow.hidden = YES;
        self.patternSeedRow.hidden = YES;
        self.processingTargetRow.hidden = YES;
        self.filterSensitivityRow.hidden = YES;
        self.posteriseBandsRow.hidden = YES;
        self.rampModeRow.hidden = YES;
        self.colourEntriesGroup.hidden = YES;
        self.inkColourRow.hidden = YES;
        self.inkRadiusRow.hidden = YES;
        self.inkThresholdRow.hidden = YES;
        self.inkSoftnessRow.hidden = YES;
        self.inkStrengthRow.hidden = YES;
        self.inkInvertedCheckbox.hidden = YES;
        self.facetedNormalsCheckbox.hidden = YES;
        [self updateLayerInspectorTabVisibility];
        return;
    }

    self.layerTypeLabel.stringValue = [NSString
        stringWithFormat:@"%ld. %@", selectedLayer_ + 1, operationDisplayName(layer->operation)];
    self.layerEnabledCheckbox.state = layer->enabled
        ? NSControlStateValueOn
        : NSControlStateValueOff;
    switch (layer->compositeMode) {
    case paperweight::CompositeMode::blend:
        self.layerCompositeControl.selectedSegment = 0;
        break;
    case paperweight::CompositeMode::add:
        self.layerCompositeControl.selectedSegment = 1;
        break;
    case paperweight::CompositeMode::multiply:
        self.layerCompositeControl.selectedSegment = 2;
        break;
    }
    self.layerOpacitySlider.doubleValue = layer->opacity;
    self.layerOpacityValue.stringValue = [NSString stringWithFormat:@"%.2f", layer->opacity];

    const auto* noise = std::get_if<paperweight::NoiseOperation>(&layer->operation);
    const auto* solid = std::get_if<paperweight::SolidColourOperation>(&layer->operation);
    const auto* levels = std::get_if<paperweight::LevelsOperation>(&layer->operation);
    const auto* threshold = std::get_if<paperweight::ThresholdOperation>(&layer->operation);
    const auto* brick = std::get_if<paperweight::BrickGridOperation>(&layer->operation);
    const auto* tile = std::get_if<paperweight::TileGridOperation>(&layer->operation);
    const auto* worley = std::get_if<paperweight::WorleyCellsOperation>(&layer->operation);
    const auto* randomCells = std::get_if<paperweight::RandomCellsOperation>(&layer->operation);
    const auto* lines = std::get_if<paperweight::LinesOperation>(&layer->operation);
    const auto* rectangles = std::get_if<paperweight::RectanglesOperation>(&layer->operation);
    const auto* circles = std::get_if<paperweight::CirclesOperation>(&layer->operation);
    const auto* surface = std::get_if<paperweight::SurfacePatternOperation>(&layer->operation);
    const auto* filter = std::get_if<paperweight::SurfaceFilterOperation>(&layer->operation);
    const auto* posterise = std::get_if<paperweight::PosteriseOperation>(&layer->operation);
    const auto* ramp = std::get_if<paperweight::ColourRampOperation>(&layer->operation);
    const auto* palette = std::get_if<paperweight::PaletteOperation>(&layer->operation);
    const auto* ink = std::get_if<paperweight::InkContourOperation>(&layer->operation);
    const auto* region = std::get_if<paperweight::RegionFieldOperation>(&layer->operation);
    const auto* course = std::get_if<paperweight::CourseLayoutOperation>(&layer->operation);
    const auto* sculpt = std::get_if<paperweight::RegionSurfaceOperation>(&layer->operation);
    const auto* shape = std::get_if<paperweight::ShapePrimitiveOperation>(&layer->operation);
    const auto* shapeBoolean = std::get_if<paperweight::ShapeBooleanOperation>(&layer->operation);
    const auto* editableShape = shape != nullptr
        ? shape
        : shapeBoolean != nullptr ? &shapeBoolean->shape : nullptr;
    const auto* lattice = std::get_if<paperweight::LatticeOperation>(&layer->operation);
    const auto* scatter = std::get_if<paperweight::ScatterOperation>(&layer->operation);
    const auto* organicCells = std::get_if<paperweight::OrganicCellOperation>(&layer->operation);
    const auto* organicCracks = std::get_if<paperweight::OrganicCrackOperation>(&layer->operation);
    const auto* leaves = std::get_if<paperweight::LeafClusterOperation>(&layer->operation);
    const auto* accumulation =
        std::get_if<paperweight::OrganicAccumulationOperation>(&layer->operation);
    self.noiseSeedRow.hidden = noise == nullptr;
    self.solidColourRow.hidden = solid == nullptr;
    self.levelsLowRow.hidden = levels == nullptr;
    self.levelsHighRow.hidden = levels == nullptr;
    self.levelsGammaRow.hidden = levels == nullptr;
    self.thresholdRow.hidden = threshold == nullptr;
    self.patternCountXRow.hidden = YES;
    self.patternCountYRow.hidden = YES;
    self.patternValueOneRow.hidden = YES;
    self.patternValueTwoRow.hidden = YES;
    self.patternValueThreeRow.hidden = YES;
    self.patternValueFourRow.hidden = YES;
    self.surfaceKindRow.hidden = YES;
    self.courseFieldRow.hidden = YES;
    self.courseGapRow.hidden = YES;
    self.courseSoftnessRow.hidden = YES;
    self.courseOverlapRow.hidden = YES;
    self.equalMortarWidthCheckbox.hidden = YES;
    self.physicalBrickCheckbox.hidden = brick == nullptr && course == nullptr;
    self.physicalBrickWidthRow.hidden = YES;
    self.physicalBrickHeightRow.hidden = YES;
    self.physicalBrickMortarRow.hidden = YES;
    self.physicalCourseOverlapRow.hidden = YES;
    self.physicalBrickSummary.hidden = YES;
    self.patternDirectionRow.hidden = YES;
    self.patternSeedRow.hidden = YES;
    self.processingTargetRow.hidden = YES;
    self.filterSensitivityRow.hidden = YES;
    self.posteriseBandsRow.hidden = YES;
    self.rampModeRow.hidden = YES;
    self.colourEntriesGroup.hidden = YES;
    self.inkColourRow.hidden = YES;
    self.inkRadiusRow.hidden = YES;
    self.inkThresholdRow.hidden = YES;
    self.inkSoftnessRow.hidden = YES;
    self.inkStrengthRow.hidden = YES;
    self.inkInvertedCheckbox.hidden = YES;
    self.facetedNormalsCheckbox.hidden = YES;
    self.inkInvertedCheckbox.title = @"Ink flat regions";
    self.facetedNormalsCheckbox.title = @"Use deliberate planar normals";
    self.addColourEntryButton.title = @"Add Colour";
    self.removeColourEntryButton.title = @"Remove Colour";
    static_cast<NSTextField*>(self.processingTargetRow.views[0]).stringValue = @"Affect";
    [self.processingTargetPopup removeAllItems];
    [self.processingTargetPopup addItemsWithTitles:@[
        @"Colour only", @"Surface only", @"Colour + surface",
    ]];
    self.facetedNormalsCheckbox.toolTip =
        @"Preserve the seeded planar faces in the normal map without changing colour, height, or roughness.";

    if (noise != nullptr) {
        self.noiseSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", noise->seedOffset];
    }
    if (solid != nullptr) {
        self.solidColourWell.color = colourFromRgba8(solid->colour);
    }
    if (levels != nullptr) {
        static_cast<NSTextField*>(self.levelsLowRow.views[0]).stringValue = @"Input low";
        static_cast<NSTextField*>(self.levelsHighRow.views[0]).stringValue = @"Input high";
        static_cast<NSTextField*>(self.levelsGammaRow.views[0]).stringValue = @"Gamma";
        self.levelsLowSlider.minValue = 0.0;
        self.levelsLowSlider.maxValue = 1.0;
        self.levelsHighSlider.minValue = 0.0;
        self.levelsHighSlider.maxValue = 1.0;
        self.levelsGammaSlider.minValue = paperweight::LayerLimits::minimumGamma;
        self.levelsGammaSlider.maxValue = paperweight::LayerLimits::maximumGamma;
        self.levelsLowSlider.doubleValue = levels->inputLow;
        self.levelsHighSlider.doubleValue = levels->inputHigh;
        self.levelsGammaSlider.doubleValue = levels->gamma;
        self.levelsLowValue.stringValue = [NSString stringWithFormat:@"%.2f", levels->inputLow];
        self.levelsHighValue.stringValue = [NSString stringWithFormat:@"%.2f", levels->inputHigh];
        self.levelsGammaValue.stringValue = [NSString stringWithFormat:@"%.2f", levels->gamma];
    }
    if (threshold != nullptr) {
        static_cast<NSTextField*>(self.thresholdRow.views[0]).stringValue = @"Threshold";
        self.thresholdSlider.doubleValue = threshold->threshold;
        self.thresholdValue.stringValue = [NSString
            stringWithFormat:@"%.2f", threshold->threshold];
    }

    const auto showCount = [](NSStackView* row,
                              NSTextField* label,
                              NSSlider* slider,
                              NSTextField* valueLabel,
                              NSString* title,
                              std::uint32_t value) {
        row.hidden = NO;
        label.stringValue = title;
        slider.minValue = paperweight::LayerLimits::minimumPatternCount;
        slider.maxValue = paperweight::LayerLimits::maximumPatternCount;
        slider.doubleValue = value;
        valueLabel.stringValue = [NSString stringWithFormat:@"%u", value];
    };
    const auto showValue = [](NSStackView* row,
                              NSTextField* label,
                              NSSlider* slider,
                              NSTextField* valueLabel,
                              NSString* title,
                              double minimum,
                              double maximum,
                              double value) {
        row.hidden = NO;
        label.stringValue = title;
        slider.minValue = minimum;
        slider.maxValue = maximum;
        slider.doubleValue = value;
        valueLabel.stringValue = [NSString stringWithFormat:@"%.2f", value];
    };
    if (brick != nullptr) {
        self.physicalBrickCheckbox.title = @"Size brick in metres";
        static_cast<NSTextField*>(self.physicalBrickWidthRow.views[0]).stringValue =
            @"Brick width";
        static_cast<NSTextField*>(self.physicalBrickHeightRow.views[0]).stringValue =
            @"Brick height";
        static_cast<NSTextField*>(self.physicalBrickMortarRow.views[0]).stringValue =
            @"Mortar";
        const bool physical = brick->physicalDimensions.has_value();
        self.physicalBrickCheckbox.state = physical
            ? NSControlStateValueOn
            : NSControlStateValueOff;
        if (physical) {
            self.physicalBrickWidthRow.hidden = NO;
            self.physicalBrickHeightRow.hidden = NO;
            self.physicalBrickMortarRow.hidden = NO;
            self.physicalBrickSummary.hidden = NO;
            self.physicalBrickWidthField.stringValue = [NSString
                stringWithFormat:@"%.6g", brick->physicalDimensions->widthMetres];
            self.physicalBrickHeightField.stringValue = [NSString
                stringWithFormat:@"%.6g", brick->physicalDimensions->heightMetres];
            self.physicalBrickMortarField.stringValue = [NSString
                stringWithFormat:@"%.6g", brick->physicalDimensions->mortarMetres];
            const auto columns = static_cast<std::uint32_t>(std::clamp(
                std::llround(
                    material_.physicalSize.widthMetres /
                    brick->physicalDimensions->widthMetres),
                static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
                static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
            const auto rows = static_cast<std::uint32_t>(std::clamp(
                std::llround(
                    material_.physicalSize.heightMetres /
                    brick->physicalDimensions->heightMetres),
                static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
                static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
            showCount(self.patternCountXRow, self.patternCountXLabel,
                      self.patternCountXSlider, self.patternCountXValue, @"Columns", columns);
            showCount(self.patternCountYRow, self.patternCountYLabel,
                      self.patternCountYSlider, self.patternCountYValue, @"Rows", rows);
            self.physicalBrickSummary.stringValue = [NSString stringWithFormat:
                @"%u × %u bricks → %.6g × %.6g m seamless repeat",
                columns,
                rows,
                material_.physicalSize.widthMetres,
                material_.physicalSize.heightMetres];
        } else {
            const bool equalWidth =
                brick->mortarSpace == paperweight::BrickMortarSpace::texture;
            showCount(self.patternCountXRow, self.patternCountXLabel,
                      self.patternCountXSlider, self.patternCountXValue, @"Columns", brick->columns);
            showCount(self.patternCountYRow, self.patternCountYLabel,
                      self.patternCountYSlider, self.patternCountYValue, @"Rows", brick->rows);
            showValue(self.patternValueOneRow, self.patternValueOneLabel,
                      self.patternValueOneSlider, self.patternValueOneValue,
                      equalWidth ? @"Mortar width" : @"Mortar",
                      0.0,
                      equalWidth ? textureSpaceMortarMaximum(*brick) : 0.95,
                      brick->mortar);
            self.equalMortarWidthCheckbox.hidden = NO;
            self.equalMortarWidthCheckbox.state = equalWidth
                ? NSControlStateValueOn
                : NSControlStateValueOff;
        }
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Stagger", 0.0, 1.0, brick->stagger);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Softness", 0.0, 0.25, brick->softness);
    } else if (course != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"Masonry", @"Flagstone slabs", @"Slate / shingles",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(course->profile)];
        self.courseFieldRow.hidden = NO;
        [self.courseFieldPopup removeAllItems];
        [self.courseFieldPopup addItemsWithTitles:@[
            @"Block faces", @"Mortar / gaps", @"Course interiors", @"Overlap",
        ]];
        [self.courseFieldPopup selectItemAtIndex:static_cast<NSInteger>(course->field)];
        static_cast<NSTextField*>(self.courseGapRow.views[0]).stringValue = @"Gap width";
        self.courseGapSlider.minValue = 0.0;
        self.courseGapSlider.maxValue = 0.95;
        static_cast<NSTextField*>(self.courseSoftnessRow.views[0]).stringValue = @"Softness";
        self.courseSoftnessSlider.minValue = 0.0;
        self.courseSoftnessSlider.maxValue = 0.25;
        static_cast<NSTextField*>(self.courseOverlapRow.views[0]).stringValue = @"Overlap";
        self.courseOverlapSlider.minValue = 0.0;
        self.courseOverlapSlider.maxValue = 0.95;
        self.physicalBrickCheckbox.title = @"Size layout in metres";
        self.physicalBrickCheckbox.state = course->physicalDimensions
            ? NSControlStateValueOn
            : NSControlStateValueOff;
        static_cast<NSTextField*>(self.physicalBrickWidthRow.views[0]).stringValue =
            @"Block width";
        static_cast<NSTextField*>(self.physicalBrickHeightRow.views[0]).stringValue =
            @"Course height";
        static_cast<NSTextField*>(self.physicalBrickMortarRow.views[0]).stringValue =
            @"Gap width";

        std::uint32_t blocks = course->blocks;
        std::uint32_t courses = course->courses;
        if (course->physicalDimensions) {
            blocks = static_cast<std::uint32_t>(std::clamp(
                std::llround(material_.physicalSize.widthMetres /
                    course->physicalDimensions->blockWidthMetres),
                static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
                static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
            courses = static_cast<std::uint32_t>(std::clamp(
                std::llround(material_.physicalSize.heightMetres /
                    course->physicalDimensions->courseHeightMetres),
                static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
                static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
            self.physicalBrickWidthRow.hidden = NO;
            self.physicalBrickHeightRow.hidden = NO;
            self.physicalBrickMortarRow.hidden = NO;
            self.physicalCourseOverlapRow.hidden =
                course->profile != paperweight::CourseLayoutProfile::slates;
            self.physicalBrickSummary.hidden = NO;
            self.physicalBrickWidthField.stringValue = [NSString
                stringWithFormat:@"%.6g", course->physicalDimensions->blockWidthMetres];
            self.physicalBrickHeightField.stringValue = [NSString
                stringWithFormat:@"%.6g", course->physicalDimensions->courseHeightMetres];
            self.physicalBrickMortarField.stringValue = [NSString
                stringWithFormat:@"%.6g", course->physicalDimensions->gapMetres];
            self.physicalCourseOverlapField.stringValue = [NSString
                stringWithFormat:@"%.6g", course->physicalDimensions->overlapMetres];
            self.physicalBrickSummary.stringValue = [NSString stringWithFormat:
                @"%u blocks × %u courses → %.6g × %.6g m seamless repeat",
                blocks,
                courses,
                material_.physicalSize.widthMetres,
                material_.physicalSize.heightMetres];
        } else {
            self.courseGapRow.hidden = NO;
            self.courseGapSlider.doubleValue = course->gap;
            self.courseGapValue.stringValue = [NSString
                stringWithFormat:@"%.2f", course->gap];
            if (course->profile == paperweight::CourseLayoutProfile::slates) {
                self.courseOverlapRow.hidden = NO;
                self.courseOverlapSlider.doubleValue = course->overlap;
                self.courseOverlapValue.stringValue = [NSString
                    stringWithFormat:@"%.2f", course->overlap];
            }
        }
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue, @"Blocks", blocks);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue, @"Courses", courses);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Width vary", 0.0, 1.0, course->blockVariation);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Height vary", 0.0, 1.0, course->courseVariation);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Stagger", 0.0, 1.0, course->stagger);
        showValue(self.patternValueFourRow, self.patternValueFourLabel,
                  self.patternValueFourSlider, self.patternValueFourValue,
                  @"Crooked", 0.0, 1.0, course->crookedness);
        self.courseSoftnessRow.hidden = NO;
        self.courseSoftnessSlider.doubleValue = course->softness;
        self.courseSoftnessValue.stringValue = [NSString
            stringWithFormat:@"%.2f", course->softness];
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", course->seedOffset];
    } else if (sculpt != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"Rounded bevel", @"Chamfered bevel", @"Hand-cut bevel",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(sculpt->profile)];
        self.courseFieldRow.hidden = NO;
        [self.courseFieldPopup removeAllItems];
        [self.courseFieldPopup addItemsWithTitles:@[
            @"Constructed height",
            @"Cavity mask",
            @"Outer-edge mask",
            @"Exposed-face mask",
            @"Facet mask",
            @"Wear mask",
        ]];
        [self.courseFieldPopup selectItemAtIndex:static_cast<NSInteger>(sculpt->field)];
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue,
                  @"Facets", sculpt->facetCount);
        self.patternCountXSlider.minValue = paperweight::LayerLimits::minimumFacetCount;
        self.patternCountXSlider.maxValue = paperweight::LayerLimits::maximumFacetCount;
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue,
                  @"Chip scale", sculpt->chipScale);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Bevel width", 0.001, 1.0, sculpt->bevelWidth);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Bevel height", 0.0, 1.0, sculpt->bevelHeight);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Facet strength", 0.0, 1.0, sculpt->facetStrength);
        showValue(self.patternValueFourRow, self.patternValueFourLabel,
                  self.patternValueFourSlider, self.patternValueFourValue,
                  @"Centre peak", 0.0, 1.0, sculpt->centrePeak);
        self.courseGapRow.hidden = NO;
        static_cast<NSTextField*>(self.courseGapRow.views[0]).stringValue = @"Slope";
        self.courseGapSlider.minValue = 0.0;
        self.courseGapSlider.maxValue = 1.0;
        self.courseGapSlider.doubleValue = sculpt->slopeStrength;
        self.courseGapValue.stringValue = [NSString stringWithFormat:@"%.2f", sculpt->slopeStrength];
        self.courseSoftnessRow.hidden = NO;
        static_cast<NSTextField*>(self.courseSoftnessRow.views[0]).stringValue = @"Edge chips";
        self.courseSoftnessSlider.minValue = 0.0;
        self.courseSoftnessSlider.maxValue = 1.0;
        self.courseSoftnessSlider.doubleValue = sculpt->chipAmount;
        self.courseSoftnessValue.stringValue = [NSString stringWithFormat:@"%.2f", sculpt->chipAmount];
        self.courseOverlapRow.hidden = NO;
        static_cast<NSTextField*>(self.courseOverlapRow.views[0]).stringValue = @"Edge wear";
        self.courseOverlapSlider.minValue = 0.0;
        self.courseOverlapSlider.maxValue = 1.0;
        self.courseOverlapSlider.doubleValue = sculpt->wearAmount;
        self.courseOverlapValue.stringValue = [NSString stringWithFormat:@"%.2f", sculpt->wearAmount];
        self.filterSensitivityRow.hidden = NO;
        static_cast<NSTextField*>(self.filterSensitivityRow.views[0]).stringValue = @"Erosion";
        self.filterSensitivitySlider.doubleValue = sculpt->erosionAmount;
        self.filterSensitivityValue.stringValue = [NSString stringWithFormat:@"%.2f", sculpt->erosionAmount];
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", sculpt->seedOffset];
        self.processingTargetRow.hidden = NO;
        [self.processingTargetPopup selectItemAtIndex:processingTargetIndex(sculpt->target)];
        self.facetedNormalsCheckbox.hidden = NO;
        self.facetedNormalsCheckbox.state = sculpt->facetedNormals
            ? NSControlStateValueOn
            : NSControlStateValueOff;
    } else if (scatter != nullptr) {
        selectedScatterPopulation_ = std::clamp<NSInteger>(
            selectedScatterPopulation_, 0,
            static_cast<NSInteger>(scatter->populations.size() - 1));
        const auto& population = scatter->populations[
            static_cast<std::size_t>(selectedScatterPopulation_)];

        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"Rounded Rectangle", @"Ellipse", @"Capsule", @"Diamond", @"Convex Polygon",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(scatter->stamp.kind)];
        self.courseFieldRow.hidden = NO;
        [self.courseFieldPopup removeAllItems];
        [self.courseFieldPopup addItemsWithTitles:@[
            @"Material outputs", @"Fill mask", @"Instance random",
            @"Local U", @"Local V", @"Boundary distance",
        ]];
        [self.courseFieldPopup selectItemAtIndex:static_cast<NSInteger>(scatter->field)];
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue,
                  @"Candidates X", scatter->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue,
                  @"Candidates Y", scatter->rows);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Density", 0.0, 1.0, scatter->density);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Jitter", 0.0, 1.0, scatter->jitter);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Minimum gap", 0.0, paperweight::LayerLimits::maximumScatterDistance,
                  scatter->minimumDistance);
        showValue(self.patternValueFourRow, self.patternValueFourLabel,
                  self.patternValueFourSlider, self.patternValueFourValue,
                  @"Max overlap", 0.0, 1.0, scatter->maximumOverlap);
        self.processingTargetRow.hidden = NO;
        static_cast<NSTextField*>(self.processingTargetRow.views[0]).stringValue = @"Overlap";
        [self.processingTargetPopup removeAllItems];
        [self.processingTargetPopup addItemsWithTitles:@[
            @"Forbidden", @"Controlled", @"Unrestricted",
        ]];
        [self.processingTargetPopup selectItemAtIndex:static_cast<NSInteger>(scatter->overlapMode)];
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", scatter->seedOffset];

        self.rampModeRow.hidden = NO;
        static_cast<NSTextField*>(self.rampModeRow.views[0]).stringValue = @"Population";
        [self.rampModePopup removeAllItems];
        for (std::size_t populationIndex = 0;
             populationIndex < scatter->populations.size(); ++populationIndex) {
            [self.rampModePopup addItemWithTitle:[NSString
                stringWithFormat:@"Population %zu", populationIndex + 1]];
        }
        [self.rampModePopup selectItemAtIndex:selectedScatterPopulation_];
        self.courseGapRow.hidden = NO;
        static_cast<NSTextField*>(self.courseGapRow.views[0]).stringValue = @"Minimum scale";
        self.courseGapSlider.minValue = paperweight::LayerLimits::minimumScatterScale;
        self.courseGapSlider.maxValue = paperweight::LayerLimits::maximumScatterScale;
        self.courseGapSlider.doubleValue = population.minimumScale;
        self.courseSoftnessRow.hidden = NO;
        static_cast<NSTextField*>(self.courseSoftnessRow.views[0]).stringValue = @"Maximum scale";
        self.courseSoftnessSlider.minValue = paperweight::LayerLimits::minimumScatterScale;
        self.courseSoftnessSlider.maxValue = paperweight::LayerLimits::maximumScatterScale;
        self.courseSoftnessSlider.doubleValue = population.maximumScale;
        self.courseOverlapRow.hidden = NO;
        static_cast<NSTextField*>(self.courseOverlapRow.views[0]).stringValue = @"Population weight";
        self.courseOverlapSlider.minValue = 0.01;
        self.courseOverlapSlider.maxValue = 4.0;
        self.courseOverlapSlider.doubleValue = population.weight;
        self.filterSensitivityRow.hidden = NO;
        static_cast<NSTextField*>(self.filterSensitivityRow.views[0]).stringValue = @"Stamp size";
        self.filterSensitivitySlider.minValue = 0.001;
        self.filterSensitivitySlider.maxValue = 0.5;
        self.filterSensitivitySlider.doubleValue = scatter->stamp.width;
        self.posteriseBandsRow.hidden = NO;
        static_cast<NSTextField*>(self.posteriseBandsRow.views[0]).stringValue = @"Stamp height";
        self.posteriseBandsSlider.minValue = 0.001;
        self.posteriseBandsSlider.maxValue = 0.5;
        self.posteriseBandsSlider.numberOfTickMarks = 0;
        self.posteriseBandsSlider.allowsTickMarkValuesOnly = NO;
        self.posteriseBandsSlider.doubleValue = scatter->stamp.height;
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%.2f", scatter->stamp.height];
        self.inkRadiusRow.hidden = NO;
        static_cast<NSTextField*>(self.inkRadiusRow.views[0]).stringValue = @"Minimum aspect";
        self.inkRadiusSlider.minValue = paperweight::LayerLimits::minimumScatterAspect;
        self.inkRadiusSlider.maxValue = paperweight::LayerLimits::maximumScatterAspect;
        self.inkRadiusSlider.doubleValue = population.minimumAspect;
        self.inkThresholdRow.hidden = NO;
        static_cast<NSTextField*>(self.inkThresholdRow.views[0]).stringValue = @"Maximum aspect";
        self.inkThresholdSlider.minValue = paperweight::LayerLimits::minimumScatterAspect;
        self.inkThresholdSlider.maxValue = paperweight::LayerLimits::maximumScatterAspect;
        self.inkThresholdSlider.doubleValue = population.maximumAspect;
        self.inkSoftnessRow.hidden = NO;
        static_cast<NSTextField*>(self.inkSoftnessRow.views[0]).stringValue = @"Min rotation °";
        self.inkSoftnessSlider.minValue = -360.0;
        self.inkSoftnessSlider.maxValue = 360.0;
        self.inkSoftnessSlider.doubleValue = population.minimumRotation;
        self.inkStrengthRow.hidden = NO;
        static_cast<NSTextField*>(self.inkStrengthRow.views[0]).stringValue = @"Max rotation °";
        self.inkStrengthSlider.minValue = -360.0;
        self.inkStrengthSlider.maxValue = 360.0;
        self.inkStrengthSlider.doubleValue = population.maximumRotation;

        self.levelsLowRow.hidden = NO;
        static_cast<NSTextField*>(self.levelsLowRow.views[0]).stringValue = @"Minimum height";
        self.levelsLowSlider.doubleValue = population.minimumHeight;
        self.levelsHighRow.hidden = NO;
        static_cast<NSTextField*>(self.levelsHighRow.views[0]).stringValue = @"Maximum height";
        self.levelsHighSlider.doubleValue = population.maximumHeight;
        self.levelsGammaRow.hidden = NO;
        static_cast<NSTextField*>(self.levelsGammaRow.views[0]).stringValue = @"Min roughness";
        self.levelsGammaSlider.minValue = 0.0;
        self.levelsGammaSlider.maxValue = 1.0;
        self.levelsGammaSlider.doubleValue = population.minimumRoughness;
        self.thresholdRow.hidden = NO;
        static_cast<NSTextField*>(self.thresholdRow.views[0]).stringValue = @"Max roughness";
        self.thresholdSlider.doubleValue = population.maximumRoughness;

        self.colourEntriesGroup.hidden = NO;
        const auto colourCount = scatter->populations.size() * 2;
        for (NSUInteger colourIndex = 0; colourIndex < self.colourEntryRows.count; ++colourIndex) {
            NSStackView* row = self.colourEntryRows[colourIndex];
            row.hidden = colourIndex >= colourCount;
            if (colourIndex >= colourCount) continue;
            const auto populationIndex = static_cast<std::size_t>(colourIndex / 2);
            const bool high = colourIndex % 2 != 0;
            self.colourEntryLabels[colourIndex].stringValue = [NSString
                stringWithFormat:@"P%zu %@", populationIndex + 1, high ? @"high" : @"low"];
            self.colourPositionSliders[colourIndex].hidden = YES;
            self.colourPositionValues[colourIndex].hidden = YES;
            const auto& colours = scatter->populations[populationIndex];
            self.colourEntryWells[colourIndex].color = colourFromRgba8(
                high ? colours.highColour : colours.lowColour);
        }
        self.addColourEntryButton.title = @"Add Population";
        self.removeColourEntryButton.title = @"Remove Population";
        self.addColourEntryButton.enabled =
            scatter->populations.size() < paperweight::LayerLimits::maximumScatterPopulations;
        self.removeColourEntryButton.enabled = scatter->populations.size() > 1;
        self.inkInvertedCheckbox.hidden = NO;
        self.inkInvertedCheckbox.title = @"Use density mask";
        self.inkInvertedCheckbox.state = scatter->densityMask.enabled
            ? NSControlStateValueOn : NSControlStateValueOff;
        self.facetedNormalsCheckbox.hidden = NO;
        self.facetedNormalsCheckbox.title = @"Use exclusion mask";
        self.facetedNormalsCheckbox.state = scatter->exclusionMask.enabled
            ? NSControlStateValueOn : NSControlStateValueOff;
        self.facetedNormalsCheckbox.toolTip =
            @"Exclude candidates with a seamless deterministic noise mask.";
        [self updateLayerInspectorLiveValueLabels];
    } else if (organicCells != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"Bark plates", @"Plate boundaries", @"Region variation",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(organicCells->field)];
        self.courseFieldRow.hidden = NO;
        [self.courseFieldPopup removeAllItems];
        [self.courseFieldPopup addItemsWithTitles:@[@"Vertical grain", @"Horizontal grain"]];
        [self.courseFieldPopup selectItemAtIndex:static_cast<NSInteger>(organicCells->direction)];
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue,
                  @"Columns", organicCells->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue,
                  @"Rows", organicCells->rows);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Anisotropy", paperweight::LayerLimits::minimumOrganicAnisotropy,
                  paperweight::LayerLimits::maximumOrganicAnisotropy,
                  organicCells->anisotropy);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Jitter", 0.0, 1.0, organicCells->jitter);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Irregularity", 0.0, 1.0, organicCells->irregularity);
        showValue(self.patternValueFourRow, self.patternValueFourLabel,
                  self.patternValueFourSlider, self.patternValueFourValue,
                  @"Gap", 0.0, 1.0, organicCells->gap);
        self.courseSoftnessRow.hidden = NO;
        static_cast<NSTextField*>(self.courseSoftnessRow.views[0]).stringValue = @"Softness";
        self.courseSoftnessSlider.minValue = 0.0;
        self.courseSoftnessSlider.maxValue = 0.25;
        self.courseSoftnessSlider.doubleValue = organicCells->softness;
        self.courseSoftnessValue.stringValue = [NSString
            stringWithFormat:@"%.3f", organicCells->softness];
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", organicCells->seedOffset];
    } else if (organicCracks != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"All cracks", @"Trunks", @"Branches", @"Hierarchy", @"Distance",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(organicCracks->field)];
        self.courseFieldRow.hidden = NO;
        [self.courseFieldPopup removeAllItems];
        [self.courseFieldPopup addItemsWithTitles:@[@"Vertical", @"Horizontal"]];
        [self.courseFieldPopup selectItemAtIndex:static_cast<NSInteger>(organicCracks->direction)];
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue,
                  @"Root cracks", organicCracks->roots);
        self.patternCountXSlider.maxValue = paperweight::LayerLimits::maximumCrackRoots;
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue,
                  @"Segments", organicCracks->segments);
        self.patternCountYSlider.minValue = 2.0;
        self.patternCountYSlider.maxValue = paperweight::LayerLimits::maximumCrackSegments;
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Branch chance", 0.0, 1.0, organicCracks->branchProbability);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Bend", 0.0, 1.0, organicCracks->bend);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Width", 0.001, 0.25, organicCracks->width);
        showValue(self.patternValueFourRow, self.patternValueFourLabel,
                  self.patternValueFourSlider, self.patternValueFourValue,
                  @"Branch taper", 0.0, 1.0, organicCracks->taper);
        self.posteriseBandsRow.hidden = NO;
        static_cast<NSTextField*>(self.posteriseBandsRow.views[0]).stringValue = @"Branch levels";
        self.posteriseBandsSlider.minValue = 0.0;
        self.posteriseBandsSlider.maxValue = paperweight::LayerLimits::maximumCrackBranchLevels;
        self.posteriseBandsSlider.numberOfTickMarks =
            paperweight::LayerLimits::maximumCrackBranchLevels + 1;
        self.posteriseBandsSlider.allowsTickMarkValuesOnly = YES;
        self.posteriseBandsSlider.doubleValue = organicCracks->branchLevels;
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%u", organicCracks->branchLevels];
        self.courseSoftnessRow.hidden = NO;
        static_cast<NSTextField*>(self.courseSoftnessRow.views[0]).stringValue = @"Softness";
        self.courseSoftnessSlider.minValue = 0.0;
        self.courseSoftnessSlider.maxValue = 0.25;
        self.courseSoftnessSlider.doubleValue = organicCracks->softness;
        self.courseSoftnessValue.stringValue = [NSString
            stringWithFormat:@"%.3f", organicCracks->softness];
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", organicCracks->seedOffset];
    } else if (leaves != nullptr) {
        self.rampModeRow.hidden = NO;
        static_cast<NSTextField*>(self.rampModeRow.views[0]).stringValue = @"Species";
        [self.rampModePopup removeAllItems];
        [self.rampModePopup addItemsWithTitles:@[
            @"Custom", @"Ivy", @"Laurel", @"Oak", @"Ash",
        ]];
        [self.rampModePopup selectItemAtIndex:0];
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"Ovate", @"Lanceolate", @"Cordate", @"Lobed",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(leaves->profile)];
        self.courseFieldRow.hidden = NO;
        [self.courseFieldPopup removeAllItems];
        [self.courseFieldPopup addItemsWithTitles:@[
            @"Material outputs", @"Fill", @"Edge", @"Midrib", @"Veins", @"Leaf variation",
        ]];
        [self.courseFieldPopup selectItemAtIndex:static_cast<NSInteger>(leaves->field)];
        self.processingTargetRow.hidden = NO;
        static_cast<NSTextField*>(self.processingTargetRow.views[0]).stringValue = @"Cluster";
        [self.processingTargetPopup removeAllItems];
        [self.processingTargetPopup addItemsWithTitles:@[
            @"Radial", @"Fan", @"Vine", @"Canopy",
        ]];
        [self.processingTargetPopup selectItemAtIndex:static_cast<NSInteger>(leaves->pattern)];
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue,
                  @"Clusters X", leaves->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue,
                  @"Clusters Y", leaves->rows);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Density", 0.0, 1.0, leaves->density);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Spread", 0.0, paperweight::LayerLimits::maximumLeafExtent,
                  leaves->clusterSpread);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Leaf length", 0.001, paperweight::LayerLimits::maximumLeafExtent,
                  leaves->leafLength);
        showValue(self.patternValueFourRow, self.patternValueFourLabel,
                  self.patternValueFourSlider, self.patternValueFourValue,
                  @"Leaf width", 0.001, paperweight::LayerLimits::maximumLeafExtent,
                  leaves->leafWidth);
        self.posteriseBandsRow.hidden = NO;
        static_cast<NSTextField*>(self.posteriseBandsRow.views[0]).stringValue = @"Leaves / cluster";
        self.posteriseBandsSlider.minValue = 1.0;
        self.posteriseBandsSlider.maxValue = paperweight::LayerLimits::maximumLeavesPerCluster;
        self.posteriseBandsSlider.numberOfTickMarks = 0;
        self.posteriseBandsSlider.allowsTickMarkValuesOnly = NO;
        self.posteriseBandsSlider.doubleValue = leaves->leavesPerCluster;
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%u", leaves->leavesPerCluster];
        self.courseGapRow.hidden = NO;
        static_cast<NSTextField*>(self.courseGapRow.views[0]).stringValue = @"Scale variation";
        self.courseGapSlider.minValue = 0.0;
        self.courseGapSlider.maxValue = 0.9;
        self.courseGapSlider.doubleValue = leaves->scaleVariation;
        self.courseGapValue.stringValue = [NSString stringWithFormat:@"%.2f", leaves->scaleVariation];
        self.courseSoftnessRow.hidden = NO;
        static_cast<NSTextField*>(self.courseSoftnessRow.views[0]).stringValue = @"Rotation variation";
        self.courseSoftnessSlider.minValue = 0.0;
        self.courseSoftnessSlider.maxValue = 360.0;
        self.courseSoftnessSlider.doubleValue = leaves->rotationVariation;
        self.courseSoftnessValue.stringValue = [NSString stringWithFormat:@"%.0f°", leaves->rotationVariation];
        self.courseOverlapRow.hidden = NO;
        static_cast<NSTextField*>(self.courseOverlapRow.views[0]).stringValue = @"Direction";
        self.courseOverlapSlider.minValue = -360.0;
        self.courseOverlapSlider.maxValue = 360.0;
        self.courseOverlapSlider.doubleValue = leaves->directionDegrees;
        self.courseOverlapValue.stringValue = [NSString stringWithFormat:@"%.0f°", leaves->directionDegrees];
        self.filterSensitivityRow.hidden = NO;
        static_cast<NSTextField*>(self.filterSensitivityRow.views[0]).stringValue = @"Edge softness";
        self.filterSensitivitySlider.minValue = 0.0;
        self.filterSensitivitySlider.maxValue = 0.25;
        self.filterSensitivitySlider.doubleValue = leaves->softness;
        self.filterSensitivityValue.stringValue = [NSString stringWithFormat:@"%.3f", leaves->softness];
        self.inkRadiusRow.hidden = NO;
        static_cast<NSTextField*>(self.inkRadiusRow.views[0]).stringValue = @"Silhouette taper";
        self.inkRadiusSlider.minValue = 0.2;
        self.inkRadiusSlider.maxValue = 2.0;
        self.inkRadiusSlider.doubleValue = leaves->taper;
        self.inkRadiusValue.stringValue = [NSString stringWithFormat:@"%.2f", leaves->taper];
        self.inkThresholdRow.hidden = NO;
        static_cast<NSTextField*>(self.inkThresholdRow.views[0]).stringValue = @"Base notch";
        self.inkThresholdSlider.minValue = 0.0;
        self.inkThresholdSlider.maxValue = 1.0;
        self.inkThresholdSlider.doubleValue = leaves->baseNotch;
        self.inkThresholdValue.stringValue = [NSString stringWithFormat:@"%.2f", leaves->baseNotch];
        self.inkSoftnessRow.hidden = NO;
        static_cast<NSTextField*>(self.inkSoftnessRow.views[0]).stringValue = @"Serration";
        self.inkSoftnessSlider.minValue = 0.0;
        self.inkSoftnessSlider.maxValue = 0.8;
        self.inkSoftnessSlider.doubleValue = leaves->serration;
        self.inkSoftnessValue.stringValue = [NSString stringWithFormat:@"%.2f", leaves->serration];
        self.inkStrengthRow.hidden = NO;
        static_cast<NSTextField*>(self.inkStrengthRow.views[0]).stringValue = @"Lobing";
        self.inkStrengthSlider.minValue = 0.0;
        self.inkStrengthSlider.maxValue = 0.8;
        self.inkStrengthSlider.doubleValue = leaves->lobing;
        self.inkStrengthValue.stringValue = [NSString stringWithFormat:@"%.2f", leaves->lobing];
        self.levelsLowRow.hidden = NO;
        static_cast<NSTextField*>(self.levelsLowRow.views[0]).stringValue = @"Minimum height";
        self.levelsLowSlider.doubleValue = leaves->minimumHeight;
        self.levelsHighRow.hidden = NO;
        static_cast<NSTextField*>(self.levelsHighRow.views[0]).stringValue = @"Maximum height";
        self.levelsHighSlider.doubleValue = leaves->maximumHeight;
        self.levelsGammaRow.hidden = NO;
        static_cast<NSTextField*>(self.levelsGammaRow.views[0]).stringValue = @"Min roughness";
        self.levelsGammaSlider.minValue = 0.0;
        self.levelsGammaSlider.maxValue = 1.0;
        self.levelsGammaSlider.doubleValue = leaves->minimumRoughness;
        self.thresholdRow.hidden = NO;
        static_cast<NSTextField*>(self.thresholdRow.views[0]).stringValue = @"Max roughness";
        self.thresholdSlider.doubleValue = leaves->maximumRoughness;
        self.colourEntriesGroup.hidden = NO;
        for (NSUInteger colourIndex = 0; colourIndex < self.colourEntryRows.count; ++colourIndex) {
            self.colourEntryRows[colourIndex].hidden = colourIndex >= 2;
            if (colourIndex < 2) {
                self.colourEntryLabels[colourIndex].stringValue =
                    colourIndex == 0 ? @"Leaf low" : @"Leaf high";
                self.colourPositionSliders[colourIndex].hidden = YES;
                self.colourPositionValues[colourIndex].hidden = YES;
                self.colourEntryWells[colourIndex].color = colourFromRgba8(
                    colourIndex == 0 ? leaves->lowColour : leaves->highColour);
            }
        }
        self.addColourEntryButton.enabled = NO;
        self.removeColourEntryButton.enabled = NO;
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", leaves->seedOffset];
    } else if (accumulation != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[@"Moss", @"Lichen", @"Colour variation"]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(accumulation->kind)];
        self.courseFieldRow.hidden = NO;
        [self.courseFieldPopup removeAllItems];
        [self.courseFieldPopup addItemsWithTitles:@[
            @"Cavities", @"Boundaries", @"Low height", @"Authored mask",
        ]];
        [self.courseFieldPopup selectItemAtIndex:static_cast<NSInteger>(accumulation->source)];
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue,
                  @"Growth scale", accumulation->scale);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Coverage", 0.0, 1.0, accumulation->coverage);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Softness", 0.0, 0.5, accumulation->softness);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Moisture bias", 0.0, 1.0, accumulation->moistureBias);
        showValue(self.patternValueFourRow, self.patternValueFourLabel,
                  self.patternValueFourSlider, self.patternValueFourValue,
                  @"Breakup", 0.0, 1.0, accumulation->breakup);
        self.courseGapRow.hidden = NO;
        static_cast<NSTextField*>(self.courseGapRow.views[0]).stringValue = @"Colour variation";
        self.courseGapSlider.minValue = 0.0;
        self.courseGapSlider.maxValue = 1.0;
        self.courseGapSlider.doubleValue = accumulation->variation;
        self.courseGapValue.stringValue = [NSString stringWithFormat:@"%.2f", accumulation->variation];
        self.processingTargetRow.hidden = NO;
        static_cast<NSTextField*>(self.processingTargetRow.views[0]).stringValue = @"Affect";
        [self.processingTargetPopup removeAllItems];
        [self.processingTargetPopup addItemsWithTitles:@[
            @"Colour only", @"Surface only", @"Colour + surface",
        ]];
        [self.processingTargetPopup selectItemAtIndex:processingTargetIndex(accumulation->target)];
        self.colourEntriesGroup.hidden = NO;
        for (NSUInteger colourIndex = 0; colourIndex < self.colourEntryRows.count; ++colourIndex) {
            self.colourEntryRows[colourIndex].hidden = colourIndex >= 2;
            if (colourIndex < 2) {
                self.colourEntryLabels[colourIndex].stringValue =
                    colourIndex == 0 ? @"Growth low" : @"Growth high";
                self.colourPositionSliders[colourIndex].hidden = YES;
                self.colourPositionValues[colourIndex].hidden = YES;
                self.colourEntryWells[colourIndex].color = colourFromRgba8(
                    colourIndex == 0 ? accumulation->lowColour : accumulation->highColour);
            }
        }
        self.addColourEntryButton.enabled = NO;
        self.removeColourEntryButton.enabled = NO;
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", accumulation->seedOffset];
    } else if (editableShape != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"Rounded Rectangle", @"Ellipse", @"Capsule", @"Diamond", @"Convex Polygon",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:
            static_cast<NSInteger>(editableShape->kind)];
        self.courseFieldRow.hidden = NO;
        [self.courseFieldPopup removeAllItems];
        [self.courseFieldPopup addItemsWithTitles:@[
            @"Filled shape", @"Inset fill", @"Centred outline", @"Inner border",
        ]];
        [self.courseFieldPopup selectItemAtIndex:
            static_cast<NSInteger>(editableShape->field)];
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue,
                  @"Columns", editableShape->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue,
                  @"Rows", editableShape->rows);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Width", 0.001, 1.0, editableShape->width);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Height", 0.001, 1.0, editableShape->height);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Corner", 0.0, 0.5, editableShape->cornerRadius);
        showValue(self.patternValueFourRow, self.patternValueFourLabel,
                  self.patternValueFourSlider, self.patternValueFourValue,
                  @"Rotation °", -360.0, 360.0, editableShape->rotationDegrees);
        self.courseGapRow.hidden = NO;
        static_cast<NSTextField*>(self.courseGapRow.views[0]).stringValue = @"Inset";
        self.courseGapSlider.minValue = 0.0;
        self.courseGapSlider.maxValue = 0.5;
        self.courseGapSlider.doubleValue = editableShape->inset;
        self.courseGapValue.stringValue = [NSString stringWithFormat:@"%.2f", editableShape->inset];
        self.courseSoftnessRow.hidden = NO;
        static_cast<NSTextField*>(self.courseSoftnessRow.views[0]).stringValue = @"Border";
        self.courseSoftnessSlider.minValue = 0.0;
        self.courseSoftnessSlider.maxValue = 0.5;
        self.courseSoftnessSlider.doubleValue = editableShape->borderWidth;
        self.courseSoftnessValue.stringValue = [NSString stringWithFormat:@"%.2f", editableShape->borderWidth];
        self.courseOverlapRow.hidden = NO;
        static_cast<NSTextField*>(self.courseOverlapRow.views[0]).stringValue = @"Stagger";
        self.courseOverlapSlider.minValue = 0.0;
        self.courseOverlapSlider.maxValue = 1.0;
        self.courseOverlapSlider.doubleValue = editableShape->stagger;
        self.courseOverlapValue.stringValue = [NSString stringWithFormat:@"%.2f", editableShape->stagger];
        self.filterSensitivityRow.hidden = NO;
        static_cast<NSTextField*>(self.filterSensitivityRow.views[0]).stringValue = @"Softness";
        self.filterSensitivitySlider.minValue = 0.0;
        self.filterSensitivitySlider.maxValue = 0.25;
        self.filterSensitivitySlider.doubleValue = editableShape->softness;
        self.filterSensitivityValue.stringValue = [NSString stringWithFormat:@"%.2f", editableShape->softness];
        self.inkRadiusRow.hidden = NO;
        static_cast<NSTextField*>(self.inkRadiusRow.views[0]).stringValue = @"Offset X";
        self.inkRadiusSlider.minValue = -0.5;
        self.inkRadiusSlider.maxValue = 0.5;
        self.inkRadiusSlider.doubleValue = editableShape->offsetX;
        self.inkRadiusValue.stringValue = [NSString stringWithFormat:@"%.2f", editableShape->offsetX];
        self.inkThresholdRow.hidden = NO;
        static_cast<NSTextField*>(self.inkThresholdRow.views[0]).stringValue = @"Offset Y";
        self.inkThresholdSlider.minValue = -0.5;
        self.inkThresholdSlider.maxValue = 0.5;
        self.inkThresholdSlider.doubleValue = editableShape->offsetY;
        self.inkThresholdValue.stringValue = [NSString stringWithFormat:@"%.2f", editableShape->offsetY];
        self.posteriseBandsRow.hidden = NO;
        static_cast<NSTextField*>(self.posteriseBandsRow.views[0]).stringValue = @"Vertices";
        self.posteriseBandsSlider.minValue = paperweight::LayerLimits::minimumPolygonVertices;
        self.posteriseBandsSlider.maxValue = paperweight::LayerLimits::maximumPolygonVertices;
        self.posteriseBandsSlider.numberOfTickMarks =
            paperweight::LayerLimits::maximumPolygonVertices -
            paperweight::LayerLimits::minimumPolygonVertices + 1;
        self.posteriseBandsSlider.allowsTickMarkValuesOnly = YES;
        self.posteriseBandsSlider.doubleValue =
            static_cast<double>(editableShape->vertices.size());
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%zu", editableShape->vertices.size()];
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", editableShape->seedOffset];
        if (shapeBoolean != nullptr) {
            self.rampModeRow.hidden = NO;
            static_cast<NSTextField*>(self.rampModeRow.views[0]).stringValue = @"Boolean";
            [self.rampModePopup removeAllItems];
            [self.rampModePopup addItemsWithTitles:@[
                @"Union", @"Intersection", @"Subtraction",
            ]];
            [self.rampModePopup selectItemAtIndex:
                static_cast<NSInteger>(shapeBoolean->mode)];
            self.processingTargetRow.hidden = NO;
            [self.processingTargetPopup selectItemAtIndex:
                processingTargetIndex(shapeBoolean->target)];
        }
    } else if (lattice != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[@"Parallel Lines", @"Diamond Lattice"]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(lattice->kind)];
        self.patternCountXRow.hidden = NO;
        self.patternCountXLabel.stringValue = @"Winding X";
        self.patternCountXSlider.minValue = -paperweight::LayerLimits::maximumLatticeWinding;
        self.patternCountXSlider.maxValue = paperweight::LayerLimits::maximumLatticeWinding;
        self.patternCountXSlider.doubleValue = lattice->windingX;
        self.patternCountXValue.stringValue = [NSString stringWithFormat:@"%d", lattice->windingX];
        self.patternCountYRow.hidden = NO;
        self.patternCountYLabel.stringValue = @"Winding Y";
        self.patternCountYSlider.minValue = -paperweight::LayerLimits::maximumLatticeWinding;
        self.patternCountYSlider.maxValue = paperweight::LayerLimits::maximumLatticeWinding;
        self.patternCountYSlider.doubleValue = lattice->windingY;
        self.patternCountYValue.stringValue = [NSString stringWithFormat:@"%d", lattice->windingY];
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Line width", 0.001, 1.0, lattice->width);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Phase", 0.0, 1.0, lattice->phase);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Softness", 0.0, 0.25, lattice->softness);
    } else if (tile != nullptr) {
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue, @"Columns", tile->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue, @"Rows", tile->rows);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Grout", 0.0, 0.95, tile->grout);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Softness", 0.0, 0.25, tile->softness);
    } else if (worley != nullptr) {
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue, @"Columns", worley->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue, @"Rows", worley->rows);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Jitter", 0.0, 1.0, worley->jitter);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Edge", 0.01, 2.0, worley->edgeWidth);
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", worley->seedOffset];
    } else if (randomCells != nullptr) {
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue, @"Columns", randomCells->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue, @"Rows", randomCells->rows);
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", randomCells->seedOffset];
    } else if (lines != nullptr) {
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue, @"Count", lines->count);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Width", 0.0, 1.0, lines->width);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Softness", 0.0, 0.25, lines->softness);
        self.patternDirectionRow.hidden = NO;
        self.patternDirectionControl.selectedSegment =
            lines->direction == paperweight::LineDirection::horizontal ? 1 : 0;
    } else if (rectangles != nullptr) {
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue, @"Columns", rectangles->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue, @"Rows", rectangles->rows);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Width", 0.0, 1.0, rectangles->width);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Height", 0.0, 1.0, rectangles->height);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Softness", 0.0, 0.25, rectangles->softness);
    } else if (circles != nullptr) {
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue, @"Columns", circles->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue, @"Rows", circles->rows);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Radius", 0.0, 0.5, circles->radius);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Softness", 0.0, 0.25, circles->softness);
    } else if (surface != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"Ridged Noise",
            @"Bands",
            @"Rings",
            @"Scatter",
            @"Streaks",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(surface->kind)];
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue, @"Scale", surface->scale);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Width", 0.001, 1.0, surface->width);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Detail", 0.0, 1.0, surface->detail);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Distortion", 0.0, 1.0, surface->distortion);
        showValue(self.patternValueFourRow, self.patternValueFourLabel,
                  self.patternValueFourSlider, self.patternValueFourValue,
                  @"Variation", 0.0, 1.0, surface->variation);
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", surface->seedOffset];
    } else if (filter != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"Invert",
            @"Soften",
            @"Expand",
            @"Contract",
            @"Edge",
            @"Slope",
            @"Cavity",
            @"Peaks",
            @"Edge-aware Soften",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(filter->kind)];
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Radius", 0.0, 0.25, filter->radius);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Strength", 0.0, 1.0, filter->strength);
        self.processingTargetRow.hidden = NO;
        [self.processingTargetPopup selectItemAtIndex:processingTargetIndex(filter->target)];
        if (filter->kind == paperweight::SurfaceFilterKind::edgeAwareSoften) {
            self.filterSensitivityRow.hidden = NO;
            static_cast<NSTextField*>(self.filterSensitivityRow.views[0]).stringValue = @"Sensitivity";
            self.filterSensitivitySlider.minValue = 0.0;
            self.filterSensitivitySlider.maxValue = 1.0;
            self.filterSensitivitySlider.doubleValue = filter->sensitivity;
            self.filterSensitivityValue.stringValue = [NSString
                stringWithFormat:@"%.2f", filter->sensitivity];
        }
    } else if (region != nullptr) {
        self.surfaceKindRow.hidden = NO;
        [self.surfaceKindPopup removeAllItems];
        [self.surfaceKindPopup addItemsWithTitles:@[
            @"Seeded Random",
            @"Local U",
            @"Local V",
            @"Distance to Centre",
            @"Distance to Boundary",
            @"Course Random",
        ]];
        [self.surfaceKindPopup selectItemAtIndex:static_cast<NSInteger>(region->field)];
        self.patternCountXRow.hidden = NO;
        self.patternCountXLabel.stringValue = @"Channel";
        self.patternCountXSlider.minValue = 0.0;
        self.patternCountXSlider.maxValue = paperweight::LayerLimits::maximumRegionChannel;
        self.patternCountXSlider.doubleValue = region->channel;
        self.patternCountXValue.stringValue = [NSString
            stringWithFormat:@"%u", region->channel];
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Output low", 0.0, 1.0, region->outputLow);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Output high", 0.0, 1.0, region->outputHigh);
        self.patternSeedRow.hidden = NO;
        self.patternSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", region->seedOffset];
        self.processingTargetRow.hidden = NO;
        [self.processingTargetPopup selectItemAtIndex:processingTargetIndex(region->target)];
        self.inkInvertedCheckbox.hidden = NO;
        self.inkInvertedCheckbox.title = @"Invert field";
        self.inkInvertedCheckbox.state = region->inverted
            ? NSControlStateValueOn
            : NSControlStateValueOff;
    } else if (posterise != nullptr) {
        self.posteriseBandsRow.hidden = NO;
        self.processingTargetRow.hidden = NO;
        static_cast<NSTextField*>(self.posteriseBandsRow.views[0]).stringValue = @"Bands";
        self.posteriseBandsSlider.minValue = paperweight::LayerLimits::minimumPosteriseBands;
        self.posteriseBandsSlider.maxValue = paperweight::LayerLimits::maximumPosteriseBands;
        self.posteriseBandsSlider.numberOfTickMarks =
            paperweight::LayerLimits::maximumPosteriseBands -
            paperweight::LayerLimits::minimumPosteriseBands + 1;
        self.posteriseBandsSlider.allowsTickMarkValuesOnly = YES;
        self.posteriseBandsSlider.doubleValue = posterise->bands;
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%u", posterise->bands];
        [self.processingTargetPopup selectItemAtIndex:processingTargetIndex(posterise->target)];
    } else if (ramp != nullptr || palette != nullptr) {
        const BOOL isRamp = ramp != nullptr;
        self.rampModeRow.hidden = !isRamp;
        self.colourEntriesGroup.hidden = NO;
        if (isRamp) {
            static_cast<NSTextField*>(self.rampModeRow.views[0]).stringValue = @"Ramp";
            [self.rampModePopup removeAllItems];
            [self.rampModePopup addItemsWithTitles:@[@"Smooth", @"Stepped"]];
            [self.rampModePopup selectItemAtIndex:
                ramp->mode == paperweight::ColourRampMode::stepped ? 1 : 0];
        }
        const std::size_t count = isRamp ? ramp->stops.size() : palette->colours.size();
        for (NSUInteger index = 0; index < self.colourEntryRows.count; ++index) {
            NSStackView* row = self.colourEntryRows[index];
            row.hidden = index >= count;
            if (index >= count) {
                continue;
            }
            NSTextField* label = self.colourEntryLabels[index];
            NSSlider* position = self.colourPositionSliders[index];
            NSTextField* positionValue = self.colourPositionValues[index];
            NSColorWell* well = self.colourEntryWells[index];
            label.stringValue = [NSString
                stringWithFormat:isRamp ? @"Stop %lu" : @"Colour %lu", index + 1];
            position.hidden = !isRamp;
            positionValue.hidden = !isRamp;
            if (isRamp) {
                position.doubleValue = ramp->stops[index].position;
                position.enabled = index != 0 && index + 1 != count;
                positionValue.stringValue = [NSString
                    stringWithFormat:@"%.2f", ramp->stops[index].position];
                well.color = colourFromRgba8(ramp->stops[index].colour);
            } else {
                well.color = colourFromRgba8(palette->colours[index]);
            }
        }
        self.addColourEntryButton.enabled =
            count < paperweight::LayerLimits::maximumColourStops;
        self.removeColourEntryButton.enabled =
            count > paperweight::LayerLimits::minimumColourStops;
    } else if (ink != nullptr) {
        self.inkColourRow.hidden = NO;
        self.inkRadiusRow.hidden = NO;
        self.inkThresholdRow.hidden = NO;
        self.inkSoftnessRow.hidden = NO;
        self.inkStrengthRow.hidden = NO;
        self.inkInvertedCheckbox.hidden = NO;
        self.inkColourWell.color = colourFromRgba8(ink->colour);
        static_cast<NSTextField*>(self.inkRadiusRow.views[0]).stringValue = @"Radius";
        self.inkRadiusSlider.minValue = 0.0;
        self.inkRadiusSlider.maxValue = 0.25;
        static_cast<NSTextField*>(self.inkThresholdRow.views[0]).stringValue = @"Threshold";
        self.inkThresholdSlider.minValue = 0.0;
        self.inkThresholdSlider.maxValue = 1.0;
        self.inkRadiusSlider.doubleValue = ink->radius;
        self.inkThresholdSlider.doubleValue = ink->threshold;
        self.inkSoftnessSlider.doubleValue = ink->softness;
        self.inkStrengthSlider.doubleValue = ink->strength;
        self.inkRadiusValue.stringValue = [NSString stringWithFormat:@"%.2f", ink->radius];
        self.inkThresholdValue.stringValue = [NSString stringWithFormat:@"%.2f", ink->threshold];
        self.inkSoftnessValue.stringValue = [NSString stringWithFormat:@"%.2f", ink->softness];
        self.inkStrengthValue.stringValue = [NSString stringWithFormat:@"%.2f", ink->strength];
        self.inkInvertedCheckbox.state = ink->inverted
            ? NSControlStateValueOn
            : NSControlStateValueOff;
    }

    const auto& transform = layer->transform;
    self.transformScaleXSlider.doubleValue = transform.scaleX;
    self.transformScaleYSlider.doubleValue = transform.scaleY;
    self.transformOffsetXSlider.doubleValue = transform.offsetX;
    self.transformOffsetYSlider.doubleValue = transform.offsetY;
    self.transformRotationControl.selectedSegment = static_cast<NSInteger>(transform.rotation);
    self.warpEnabledCheckbox.state = transform.warpEnabled
        ? NSControlStateValueOn
        : NSControlStateValueOff;
    self.warpStrengthSlider.doubleValue = transform.warpStrength;
    self.warpFrequencySlider.doubleValue = transform.warpFrequency;
    self.warpSeedOffsetField.stringValue = [NSString
        stringWithFormat:@"%llu", transform.warpSeedOffset];
    self.transformScaleXValue.stringValue = [NSString stringWithFormat:@"%u", transform.scaleX];
    self.transformScaleYValue.stringValue = [NSString stringWithFormat:@"%u", transform.scaleY];
    self.transformOffsetXValue.stringValue = [NSString stringWithFormat:@"%.2f", transform.offsetX];
    self.transformOffsetYValue.stringValue = [NSString stringWithFormat:@"%.2f", transform.offsetY];
    self.warpStrengthValue.stringValue = [NSString stringWithFormat:@"%.2f", transform.warpStrength];
    self.warpFrequencyValue.stringValue = [NSString stringWithFormat:@"%u", transform.warpFrequency];
    self.warpStrengthSlider.enabled = transform.warpEnabled;
    self.warpFrequencySlider.enabled = transform.warpEnabled;
    self.warpSeedOffsetField.enabled = transform.warpEnabled;

    const auto& mask = layer->mask;
    self.maskEnabledCheckbox.state = mask.enabled
        ? NSControlStateValueOn
        : NSControlStateValueOff;
    self.maskInvertedCheckbox.state = mask.inverted
        ? NSControlStateValueOn
        : NSControlStateValueOff;
    self.maskSeedOffsetField.stringValue = [NSString stringWithFormat:@"%llu", mask.seedOffset];
    self.maskLowSlider.doubleValue = mask.inputLow;
    self.maskHighSlider.doubleValue = mask.inputHigh;
    self.maskLowValue.stringValue = [NSString stringWithFormat:@"%.2f", mask.inputLow];
    self.maskHighValue.stringValue = [NSString stringWithFormat:@"%.2f", mask.inputHigh];
    self.maskInvertedCheckbox.enabled = mask.enabled;
    self.maskSeedOffsetField.enabled = mask.enabled;
    self.maskLowSlider.enabled = mask.enabled;
    self.maskHighSlider.enabled = mask.enabled;
    [self updateLayerInspectorTabVisibility];
}

- (void)updateLayerInspectorTabVisibility
{
    const BOOL hasLayer = layerAt(material_, selectedLayer_) != nullptr;
    const NSInteger tab = self.layerInspectorTabs.selectedSegment;
    self.layerSettingsGroup.hidden = !hasLayer || tab != 0;
    self.transformSettingsGroup.hidden = !hasLayer || tab != 1;
    self.maskSettingsGroup.hidden = !hasLayer || tab != 2;
}

- (void)layerInspectorTabChanged:(id)sender
{
    static_cast<void>(sender);
    [self updateLayerInspectorTabVisibility];
}

- (void)selectLayer:(NSButton*)sender
{
    selectedLayer_ = sender.tag;
    [self rebuildLayerList];
    [self refreshLayerInspector];
}

- (void)layerListEnabledChanged:(NSButton*)sender
{
    auto* layer = layerAt(material_, sender.tag);
    if (layer == nullptr) {
        return;
    }
    layer->enabled = sender.state == NSControlStateValueOn;
    selectedLayer_ = sender.tag;
    [self rebuildLayerList];
    [self refreshLayerInspector];
    [self regeneratePreview];
    [self markDirty];
}

- (void)layerEnabledChanged:(NSButton*)sender
{
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }
    layer->enabled = sender.state == NSControlStateValueOn;
    [self rebuildLayerList];
    [self regeneratePreview];
    [self markDirty];
}

- (void)addLayer:(id)sender
{
    static_cast<void>(sender);
    if (material_.layers.size() >= paperweight::LayerLimits::maximumLayers) {
        self.statusLabel.stringValue = @"A material may contain at most 32 layers.";
        self.statusLabel.textColor = NSColor.systemRedColor;
        return;
    }
    switch (self.addOperationPopup.indexOfSelectedItem) {
    case 0:
        material_.layers.push_back(paperweight::makeNoiseLayer(material_.layers.size()));
        break;
    case 1:
        material_.layers.push_back(paperweight::makeSolidColourLayer());
        break;
    case 2:
        material_.layers.push_back(paperweight::makeLevelsLayer());
        break;
    case 3:
        material_.layers.push_back(paperweight::makeThresholdLayer());
        break;
    case 4:
        material_.layers.push_back(paperweight::makeBrickGridLayer());
        break;
    case 5:
        material_.layers.push_back(paperweight::makeTileGridLayer());
        break;
    case 6:
        material_.layers.push_back(paperweight::makeWorleyCellsLayer());
        break;
    case 7:
        material_.layers.push_back(paperweight::makeRandomCellsLayer());
        break;
    case 8:
        material_.layers.push_back(paperweight::makeLinesLayer());
        break;
    case 9:
        material_.layers.push_back(paperweight::makeRectanglesLayer());
        break;
    case 10:
        material_.layers.push_back(paperweight::makeCirclesLayer());
        break;
    case 11:
        material_.layers.push_back(paperweight::makeSurfacePatternLayer(
            paperweight::SurfacePatternKind::ridgedNoise));
        break;
    case 12:
        material_.layers.push_back(paperweight::makeSurfacePatternLayer(
            paperweight::SurfacePatternKind::bands));
        break;
    case 13:
        material_.layers.push_back(paperweight::makeSurfacePatternLayer(
            paperweight::SurfacePatternKind::rings));
        break;
    case 14:
        material_.layers.push_back(paperweight::makeSurfacePatternLayer(
            paperweight::SurfacePatternKind::scatter));
        break;
    case 15:
        material_.layers.push_back(paperweight::makeSurfacePatternLayer(
            paperweight::SurfacePatternKind::streaks));
        break;
    case 16:
        material_.layers.push_back(paperweight::makeSurfaceFilterLayer());
        break;
    case 17:
        material_.layers.push_back(paperweight::makePosteriseLayer());
        break;
    case 18:
        material_.layers.push_back(paperweight::makeColourRampLayer());
        break;
    case 19:
        material_.layers.push_back(paperweight::makePaletteLayer());
        break;
    case 20:
        material_.layers.push_back(paperweight::makeInkContourLayer());
        break;
    case 21:
        material_.layers.push_back(paperweight::makeRegionFieldLayer());
        break;
    case 22:
        material_.layers.push_back(paperweight::makeCourseLayoutLayer());
        break;
    case 23:
        material_.layers.push_back(paperweight::makeRegionSurfaceLayer());
        break;
    case 24:
        material_.layers.push_back(paperweight::makeShapePrimitiveLayer());
        break;
    case 25:
        material_.layers.push_back(paperweight::makeShapeBooleanLayer());
        break;
    case 26:
        material_.layers.push_back(paperweight::makeLatticeLayer());
        break;
    case 27:
        material_.layers.push_back(paperweight::makeScatterLayer());
        selectedScatterPopulation_ = 0;
        break;
    case 28:
        material_.layers.push_back(paperweight::makeOrganicCellLayer());
        break;
    case 29:
        material_.layers.push_back(paperweight::makeOrganicCrackLayer());
        break;
    case 30:
        material_.layers.push_back(paperweight::makeLeafClusterLayer());
        break;
    case 31:
        material_.layers.push_back(paperweight::makeOrganicAccumulationLayer());
        break;
    default:
        return;
    }
    selectedLayer_ = static_cast<NSInteger>(material_.layers.size() - 1);
    [self rebuildLayerList];
    [self refreshLayerInspector];
    [self regeneratePreview];
    [self markDirty];
}

- (void)removeLayer:(id)sender
{
    static_cast<void>(sender);
    if (material_.layers.size() <= 1 || layerAt(material_, selectedLayer_) == nullptr) {
        return;
    }
    material_.layers.erase(material_.layers.begin() + selectedLayer_);
    selectedLayer_ = std::min(
        selectedLayer_, static_cast<NSInteger>(material_.layers.size() - 1));
    [self rebuildLayerList];
    [self refreshLayerInspector];
    [self regeneratePreview];
    [self markDirty];
}

- (void)moveLayerDown:(id)sender
{
    static_cast<void>(sender);
    if (selectedLayer_ <= 0 || layerAt(material_, selectedLayer_) == nullptr) {
        return;
    }
    std::swap(
        material_.layers[static_cast<std::size_t>(selectedLayer_)],
        material_.layers[static_cast<std::size_t>(selectedLayer_ - 1)]);
    --selectedLayer_;
    [self rebuildLayerList];
    [self refreshLayerInspector];
    [self regeneratePreview];
    [self markDirty];
}

- (void)moveLayerUp:(id)sender
{
    static_cast<void>(sender);
    if (selectedLayer_ < 0 ||
        static_cast<std::size_t>(selectedLayer_ + 1) >= material_.layers.size()) {
        return;
    }
    std::swap(
        material_.layers[static_cast<std::size_t>(selectedLayer_)],
        material_.layers[static_cast<std::size_t>(selectedLayer_ + 1)]);
    ++selectedLayer_;
    [self rebuildLayerList];
    [self refreshLayerInspector];
    [self regeneratePreview];
    [self markDirty];
}

- (void)layerColourChanged:(id)sender
{
    static_cast<void>(sender);
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }
    auto* solid = std::get_if<paperweight::SolidColourOperation>(&layer->operation);
    if (solid == nullptr) {
        return;
    }
    solid->colour = rgba8FromColour(self.solidColourWell.color);
    [self regeneratePreview];
    [self markDirty];
}

- (void)styliseColourChanged:(NSColorWell*)sender
{
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }
    if (auto* scatter = std::get_if<paperweight::ScatterOperation>(&layer->operation)) {
        const auto populationIndex = static_cast<std::size_t>(sender.tag / 2);
        if (populationIndex >= scatter->populations.size()) {
            return;
        }
        auto& population = scatter->populations[populationIndex];
        if (sender.tag % 2 == 0) {
            population.lowColour = rgba8FromColour(sender.color);
        } else {
            population.highColour = rgba8FromColour(sender.color);
        }
    } else if (auto* leaves =
                   std::get_if<paperweight::LeafClusterOperation>(&layer->operation)) {
        if (sender.tag == 0) {
            leaves->lowColour = rgba8FromColour(sender.color);
        } else if (sender.tag == 1) {
            leaves->highColour = rgba8FromColour(sender.color);
        } else {
            return;
        }
    } else if (auto* growth =
                   std::get_if<paperweight::OrganicAccumulationOperation>(&layer->operation)) {
        if (sender.tag == 0) {
            growth->lowColour = rgba8FromColour(sender.color);
        } else if (sender.tag == 1) {
            growth->highColour = rgba8FromColour(sender.color);
        } else {
            return;
        }
    } else if (auto* ink = std::get_if<paperweight::InkContourOperation>(&layer->operation)) {
        ink->colour = rgba8FromColour(self.inkColourWell.color);
    } else if (auto* ramp = std::get_if<paperweight::ColourRampOperation>(&layer->operation)) {
        const auto index = static_cast<std::size_t>(sender.tag);
        if (index >= ramp->stops.size()) {
            return;
        }
        ramp->stops[index].colour = rgba8FromColour(sender.color);
    } else if (auto* palette = std::get_if<paperweight::PaletteOperation>(&layer->operation)) {
        const auto index = static_cast<std::size_t>(sender.tag);
        if (index >= palette->colours.size()) {
            return;
        }
        palette->colours[index] = rgba8FromColour(sender.color);
    } else {
        return;
    }
    [self regeneratePreview];
    [self markDirty];
}

- (void)styliseParameterChanged:(id)sender
{
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }
    if (auto* scatter = std::get_if<paperweight::ScatterOperation>(&layer->operation)) {
        if (sender == self.rampModePopup) {
            selectedScatterPopulation_ = self.rampModePopup.indexOfSelectedItem;
            [self refreshLayerInspector];
            return;
        }
        selectedScatterPopulation_ = std::clamp<NSInteger>(
            selectedScatterPopulation_, 0,
            static_cast<NSInteger>(scatter->populations.size() - 1));
        auto& population = scatter->populations[
            static_cast<std::size_t>(selectedScatterPopulation_)];
        if (sender == self.processingTargetPopup) {
            scatter->overlapMode = static_cast<paperweight::ScatterOverlapMode>(
                self.processingTargetPopup.indexOfSelectedItem);
        }
        scatter->stamp.width = self.filterSensitivitySlider.doubleValue;
        scatter->stamp.height = self.posteriseBandsSlider.doubleValue;
        population.minimumAspect = std::min(
            self.inkRadiusSlider.doubleValue,
            self.inkThresholdSlider.doubleValue);
        population.maximumAspect = std::max(
            self.inkRadiusSlider.doubleValue,
            self.inkThresholdSlider.doubleValue);
        population.minimumRotation = std::min(
            self.inkSoftnessSlider.doubleValue,
            self.inkStrengthSlider.doubleValue);
        population.maximumRotation = std::max(
            self.inkSoftnessSlider.doubleValue,
            self.inkStrengthSlider.doubleValue);
        self.inkRadiusSlider.doubleValue = population.minimumAspect;
        self.inkThresholdSlider.doubleValue = population.maximumAspect;
        self.inkSoftnessSlider.doubleValue = population.minimumRotation;
        self.inkStrengthSlider.doubleValue = population.maximumRotation;
        scatter->densityMask.enabled =
            self.inkInvertedCheckbox.state == NSControlStateValueOn;
        scatter->exclusionMask.enabled =
            self.facetedNormalsCheckbox.state == NSControlStateValueOn;
        self.filterSensitivityValue.stringValue = [NSString
            stringWithFormat:@"%.2f", scatter->stamp.width];
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%.2f", scatter->stamp.height];
        self.inkRadiusValue.stringValue = [NSString
            stringWithFormat:@"%.2f", population.minimumAspect];
        self.inkThresholdValue.stringValue = [NSString
            stringWithFormat:@"%.2f", population.maximumAspect];
        self.inkSoftnessValue.stringValue = [NSString
            stringWithFormat:@"%.0f", population.minimumRotation];
        self.inkStrengthValue.stringValue = [NSString
            stringWithFormat:@"%.0f", population.maximumRotation];
    } else if (auto* cracks =
                   std::get_if<paperweight::OrganicCrackOperation>(&layer->operation)) {
        cracks->branchLevels = static_cast<std::uint32_t>(std::clamp(
            std::llround(self.posteriseBandsSlider.doubleValue),
            0LL,
            static_cast<long long>(paperweight::LayerLimits::maximumCrackBranchLevels)));
        self.posteriseBandsSlider.doubleValue = cracks->branchLevels;
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%u", cracks->branchLevels];
    } else if (auto* leaves =
                   std::get_if<paperweight::LeafClusterOperation>(&layer->operation)) {
        if (sender == self.rampModePopup && self.rampModePopup.indexOfSelectedItem > 0) {
            const auto field = leaves->field;
            const auto seedOffset = leaves->seedOffset;
            *leaves = paperweight::leafSpeciesPreset(static_cast<paperweight::LeafSpecies>(
                self.rampModePopup.indexOfSelectedItem - 1));
            leaves->field = field;
            leaves->seedOffset = seedOffset;
            [self refreshLayerInspector];
            [self rebuildLayerList];
        } else {
            if (sender == self.processingTargetPopup) {
                leaves->pattern = static_cast<paperweight::LeafClusterPattern>(
                    self.processingTargetPopup.indexOfSelectedItem);
            }
            leaves->leavesPerCluster = static_cast<std::uint32_t>(std::clamp(
                std::llround(self.posteriseBandsSlider.doubleValue),
                1LL,
                static_cast<long long>(paperweight::LayerLimits::maximumLeavesPerCluster)));
            leaves->softness = self.filterSensitivitySlider.doubleValue;
            leaves->taper = self.inkRadiusSlider.doubleValue;
            leaves->baseNotch = self.inkThresholdSlider.doubleValue;
            leaves->serration = self.inkSoftnessSlider.doubleValue;
            leaves->lobing = self.inkStrengthSlider.doubleValue;
            self.posteriseBandsSlider.doubleValue = leaves->leavesPerCluster;
            self.posteriseBandsValue.stringValue = [NSString
                stringWithFormat:@"%u", leaves->leavesPerCluster];
            self.filterSensitivityValue.stringValue = [NSString
                stringWithFormat:@"%.3f", leaves->softness];
            self.inkRadiusValue.stringValue = [NSString stringWithFormat:@"%.2f", leaves->taper];
            self.inkThresholdValue.stringValue = [NSString stringWithFormat:@"%.2f", leaves->baseNotch];
            self.inkSoftnessValue.stringValue = [NSString stringWithFormat:@"%.2f", leaves->serration];
            self.inkStrengthValue.stringValue = [NSString stringWithFormat:@"%.2f", leaves->lobing];
        }
    } else if (auto* growth =
                   std::get_if<paperweight::OrganicAccumulationOperation>(&layer->operation)) {
        growth->target = processingTargetAtIndex(
            self.processingTargetPopup.indexOfSelectedItem);
    } else if (auto* filter = std::get_if<paperweight::SurfaceFilterOperation>(&layer->operation)) {
        filter->sensitivity = self.filterSensitivitySlider.doubleValue;
        filter->target = processingTargetAtIndex(self.processingTargetPopup.indexOfSelectedItem);
        self.filterSensitivityValue.stringValue = [NSString
            stringWithFormat:@"%.2f", filter->sensitivity];
    } else if (auto* posterise = std::get_if<paperweight::PosteriseOperation>(&layer->operation)) {
        posterise->bands = static_cast<std::uint32_t>(
            std::llround(self.posteriseBandsSlider.doubleValue));
        posterise->target = processingTargetAtIndex(
            self.processingTargetPopup.indexOfSelectedItem);
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%u", posterise->bands];
    } else if (auto* ramp = std::get_if<paperweight::ColourRampOperation>(&layer->operation)) {
        ramp->mode = self.rampModePopup.indexOfSelectedItem == 1
            ? paperweight::ColourRampMode::stepped
            : paperweight::ColourRampMode::linear;
        if ([sender isKindOfClass:NSSlider.class]) {
            auto* slider = static_cast<NSSlider*>(sender);
            const auto index = static_cast<std::size_t>(slider.tag);
            if (index > 0 && index + 1 < ramp->stops.size()) {
                const double minimum = ramp->stops[index - 1].position + 0.01;
                const double maximum = ramp->stops[index + 1].position - 0.01;
                ramp->stops[index].position = std::clamp(
                    slider.doubleValue, minimum, maximum);
                slider.doubleValue = ramp->stops[index].position;
                self.colourPositionValues[index].stringValue = [NSString
                    stringWithFormat:@"%.2f", ramp->stops[index].position];
            }
        }
    } else if (auto* ink = std::get_if<paperweight::InkContourOperation>(&layer->operation)) {
        ink->radius = self.inkRadiusSlider.doubleValue;
        ink->threshold = self.inkThresholdSlider.doubleValue;
        ink->softness = self.inkSoftnessSlider.doubleValue;
        ink->strength = self.inkStrengthSlider.doubleValue;
        ink->inverted = self.inkInvertedCheckbox.state == NSControlStateValueOn;
        self.inkRadiusValue.stringValue = [NSString stringWithFormat:@"%.2f", ink->radius];
        self.inkThresholdValue.stringValue = [NSString stringWithFormat:@"%.2f", ink->threshold];
        self.inkSoftnessValue.stringValue = [NSString stringWithFormat:@"%.2f", ink->softness];
        self.inkStrengthValue.stringValue = [NSString stringWithFormat:@"%.2f", ink->strength];
    } else if (auto* region =
                   std::get_if<paperweight::RegionFieldOperation>(&layer->operation)) {
        region->target = processingTargetAtIndex(
            self.processingTargetPopup.indexOfSelectedItem);
        region->inverted = self.inkInvertedCheckbox.state == NSControlStateValueOn;
    } else if (auto* sculpt =
                   std::get_if<paperweight::RegionSurfaceOperation>(&layer->operation)) {
        sculpt->erosionAmount = self.filterSensitivitySlider.doubleValue;
        sculpt->target = processingTargetAtIndex(
            self.processingTargetPopup.indexOfSelectedItem);
        self.filterSensitivityValue.stringValue = [NSString
            stringWithFormat:@"%.2f", sculpt->erosionAmount];
    } else if (auto* shape =
                   std::get_if<paperweight::ShapePrimitiveOperation>(&layer->operation)) {
        shape->softness = self.filterSensitivitySlider.doubleValue;
        shape->offsetX = self.inkRadiusSlider.doubleValue;
        shape->offsetY = self.inkThresholdSlider.doubleValue;
        if (sender == self.posteriseBandsSlider) {
            const auto vertices = static_cast<std::size_t>(std::llround(
                self.posteriseBandsSlider.doubleValue));
            shape->vertices = regularPolygonVertices(vertices);
        }
        self.filterSensitivityValue.stringValue = [NSString
            stringWithFormat:@"%.2f", shape->softness];
        self.inkRadiusValue.stringValue = [NSString
            stringWithFormat:@"%.2f", shape->offsetX];
        self.inkThresholdValue.stringValue = [NSString
            stringWithFormat:@"%.2f", shape->offsetY];
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%zu", shape->vertices.size()];
    } else if (auto* boolean =
                   std::get_if<paperweight::ShapeBooleanOperation>(&layer->operation)) {
        boolean->shape.softness = self.filterSensitivitySlider.doubleValue;
        boolean->shape.offsetX = self.inkRadiusSlider.doubleValue;
        boolean->shape.offsetY = self.inkThresholdSlider.doubleValue;
        if (sender == self.posteriseBandsSlider) {
            const auto vertices = static_cast<std::size_t>(std::llround(
                self.posteriseBandsSlider.doubleValue));
            boolean->shape.vertices = regularPolygonVertices(vertices);
        }
        boolean->mode = static_cast<paperweight::ShapeBooleanMode>(
            self.rampModePopup.indexOfSelectedItem);
        boolean->target = processingTargetAtIndex(
            self.processingTargetPopup.indexOfSelectedItem);
        self.filterSensitivityValue.stringValue = [NSString
            stringWithFormat:@"%.2f", boolean->shape.softness];
        self.inkRadiusValue.stringValue = [NSString
            stringWithFormat:@"%.2f", boolean->shape.offsetX];
        self.inkThresholdValue.stringValue = [NSString
            stringWithFormat:@"%.2f", boolean->shape.offsetY];
        self.posteriseBandsValue.stringValue = [NSString
            stringWithFormat:@"%zu", boolean->shape.vertices.size()];
    } else {
        return;
    }
    [self regeneratePreview];
    [self markDirty];
}

- (void)addStyliseColour:(id)sender
{
    static_cast<void>(sender);
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }
    if (auto* scatter = std::get_if<paperweight::ScatterOperation>(&layer->operation)) {
        if (scatter->populations.size() >= paperweight::LayerLimits::maximumScatterPopulations) {
            return;
        }
        scatter->populations.push_back(scatter->populations.back());
        selectedScatterPopulation_ = static_cast<NSInteger>(scatter->populations.size() - 1);
    } else if (auto* ramp = std::get_if<paperweight::ColourRampOperation>(&layer->operation)) {
        if (ramp->stops.size() >= paperweight::LayerLimits::maximumColourStops) {
            return;
        }
        const auto right = ramp->stops.end() - 1;
        const auto left = right - 1;
        const auto averageChannel = [](std::uint8_t a, std::uint8_t b) {
            return static_cast<std::uint8_t>((static_cast<unsigned>(a) + b) / 2U);
        };
        const paperweight::Rgba8 colour{
            averageChannel(left->colour.red, right->colour.red),
            averageChannel(left->colour.green, right->colour.green),
            averageChannel(left->colour.blue, right->colour.blue),
            averageChannel(left->colour.alpha, right->colour.alpha),
        };
        ramp->stops.insert(right, {
            (left->position + right->position) * 0.5,
            colour,
        });
    } else if (auto* palette = std::get_if<paperweight::PaletteOperation>(&layer->operation)) {
        if (palette->colours.size() >= paperweight::LayerLimits::maximumColourStops) {
            return;
        }
        const auto& first = palette->colours.front();
        const auto& last = palette->colours.back();
        palette->colours.push_back({
            static_cast<std::uint8_t>((static_cast<unsigned>(first.red) + last.red) / 2U),
            static_cast<std::uint8_t>((static_cast<unsigned>(first.green) + last.green) / 2U),
            static_cast<std::uint8_t>((static_cast<unsigned>(first.blue) + last.blue) / 2U),
            static_cast<std::uint8_t>((static_cast<unsigned>(first.alpha) + last.alpha) / 2U),
        });
    } else {
        return;
    }
    [self refreshLayerInspector];
    [self regeneratePreview];
    [self markDirty];
}

- (void)removeStyliseColour:(id)sender
{
    static_cast<void>(sender);
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }
    if (auto* scatter = std::get_if<paperweight::ScatterOperation>(&layer->operation)) {
        if (scatter->populations.size() <= 1) {
            return;
        }
        const auto index = static_cast<std::size_t>(std::clamp<NSInteger>(
            selectedScatterPopulation_, 0,
            static_cast<NSInteger>(scatter->populations.size() - 1)));
        scatter->populations.erase(
            scatter->populations.begin() +
            static_cast<std::vector<paperweight::ScatterPopulation>::difference_type>(index));
        selectedScatterPopulation_ = std::min<NSInteger>(
            selectedScatterPopulation_,
            static_cast<NSInteger>(scatter->populations.size() - 1));
    } else if (auto* ramp = std::get_if<paperweight::ColourRampOperation>(&layer->operation)) {
        if (ramp->stops.size() <= paperweight::LayerLimits::minimumColourStops) {
            return;
        }
        ramp->stops.erase(ramp->stops.end() - 2);
    } else if (auto* palette = std::get_if<paperweight::PaletteOperation>(&layer->operation)) {
        if (palette->colours.size() <= paperweight::LayerLimits::minimumColourStops) {
            return;
        }
        palette->colours.pop_back();
    } else {
        return;
    }
    [self refreshLayerInspector];
    [self regeneratePreview];
    [self markDirty];
}

- (void)layerParameterChanged:(id)sender
{
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }

    switch (self.layerCompositeControl.selectedSegment) {
    case 1:
        layer->compositeMode = paperweight::CompositeMode::add;
        break;
    case 2:
        layer->compositeMode = paperweight::CompositeMode::multiply;
        break;
    default:
        layer->compositeMode = paperweight::CompositeMode::blend;
        break;
    }
    layer->opacity = self.layerOpacitySlider.doubleValue;

    if (sender == self.noiseSeedOffsetField) {
        const std::string text = self.noiseSeedOffsetField.stringValue.UTF8String;
        std::uint64_t parsed = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
        if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
            self.statusLabel.stringValue = @"A noise seed offset must be a non-negative integer.";
            self.statusLabel.textColor = NSColor.systemRedColor;
            return;
        }
        if (auto* noise = std::get_if<paperweight::NoiseOperation>(&layer->operation)) {
            noise->seedOffset = parsed;
        }
    }

    if (auto* levels = std::get_if<paperweight::LevelsOperation>(&layer->operation)) {
        double low = self.levelsLowSlider.doubleValue;
        double high = self.levelsHighSlider.doubleValue;
        if (low >= high) {
            if (sender == self.levelsLowSlider) {
                low = std::min(low, 0.99);
                high = std::max(high, low + 0.01);
            } else {
                high = std::max(high, 0.01);
                low = std::min(low, high - 0.01);
            }
            self.levelsLowSlider.doubleValue = low;
            self.levelsHighSlider.doubleValue = high;
        }
        levels->inputLow = low;
        levels->inputHigh = high;
        levels->gamma = self.levelsGammaSlider.doubleValue;
    }
    if (auto* threshold = std::get_if<paperweight::ThresholdOperation>(&layer->operation)) {
        threshold->threshold = self.thresholdSlider.doubleValue;
    }
    if (auto* scatter = std::get_if<paperweight::ScatterOperation>(&layer->operation)) {
        selectedScatterPopulation_ = std::clamp<NSInteger>(
            selectedScatterPopulation_, 0,
            static_cast<NSInteger>(scatter->populations.size() - 1));
        auto& population = scatter->populations[
            static_cast<std::size_t>(selectedScatterPopulation_)];
        population.minimumHeight = std::min(
            self.levelsLowSlider.doubleValue,
            self.levelsHighSlider.doubleValue);
        population.maximumHeight = std::max(
            self.levelsLowSlider.doubleValue,
            self.levelsHighSlider.doubleValue);
        population.minimumRoughness = std::min(
            self.levelsGammaSlider.doubleValue,
            self.thresholdSlider.doubleValue);
        population.maximumRoughness = std::max(
            self.levelsGammaSlider.doubleValue,
            self.thresholdSlider.doubleValue);
        self.levelsLowSlider.doubleValue = population.minimumHeight;
        self.levelsHighSlider.doubleValue = population.maximumHeight;
        self.levelsGammaSlider.doubleValue = population.minimumRoughness;
        self.thresholdSlider.doubleValue = population.maximumRoughness;
    } else if (auto* leaves =
                   std::get_if<paperweight::LeafClusterOperation>(&layer->operation)) {
        leaves->minimumHeight = std::min(
            self.levelsLowSlider.doubleValue,
            self.levelsHighSlider.doubleValue);
        leaves->maximumHeight = std::max(
            self.levelsLowSlider.doubleValue,
            self.levelsHighSlider.doubleValue);
        leaves->minimumRoughness = std::min(
            self.levelsGammaSlider.doubleValue,
            self.thresholdSlider.doubleValue);
        leaves->maximumRoughness = std::max(
            self.levelsGammaSlider.doubleValue,
            self.thresholdSlider.doubleValue);
        self.levelsLowSlider.doubleValue = leaves->minimumHeight;
        self.levelsHighSlider.doubleValue = leaves->maximumHeight;
        self.levelsGammaSlider.doubleValue = leaves->minimumRoughness;
        self.thresholdSlider.doubleValue = leaves->maximumRoughness;
    }

    [self updateLayerInspectorLiveValueLabels];
    [self regeneratePreview];
    [self markDirty];
}

- (void)transformParameterChanged:(id)sender
{
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }

    auto& transform = layer->transform;
    transform.scaleX = static_cast<std::uint32_t>(
        std::llround(self.transformScaleXSlider.doubleValue));
    transform.scaleY = static_cast<std::uint32_t>(
        std::llround(self.transformScaleYSlider.doubleValue));
    transform.offsetX = self.transformOffsetXSlider.doubleValue;
    transform.offsetY = self.transformOffsetYSlider.doubleValue;
    transform.rotation = static_cast<paperweight::QuarterTurn>(
        self.transformRotationControl.selectedSegment);
    transform.warpEnabled = self.warpEnabledCheckbox.state == NSControlStateValueOn;
    transform.warpStrength = self.warpStrengthSlider.doubleValue;
    transform.warpFrequency = static_cast<std::uint32_t>(
        std::llround(self.warpFrequencySlider.doubleValue));

    if (sender == self.warpSeedOffsetField) {
        const std::string text = self.warpSeedOffsetField.stringValue.UTF8String;
        std::uint64_t parsed = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
        if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
            self.statusLabel.stringValue = @"A warp seed offset must be a non-negative integer.";
            self.statusLabel.textColor = NSColor.systemRedColor;
            return;
        }
        transform.warpSeedOffset = parsed;
    }

    [self updateLayerInspectorLiveValueLabels];
    if (sender == self.warpEnabledCheckbox) {
        self.warpStrengthSlider.enabled = transform.warpEnabled;
        self.warpFrequencySlider.enabled = transform.warpEnabled;
        self.warpSeedOffsetField.enabled = transform.warpEnabled;
    }
    [self regeneratePreview];
    [self markDirty];
}

- (void)structuralParameterChanged:(id)sender
{
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }

    std::optional<std::uint64_t> parsedSeed;
    if (sender == self.patternSeedOffsetField) {
        const std::string text = self.patternSeedOffsetField.stringValue.UTF8String;
        std::uint64_t parsed = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
        if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
            self.statusLabel.stringValue = @"A seed offset must be a non-negative integer.";
            self.statusLabel.textColor = NSColor.systemRedColor;
            return;
        }
        parsedSeed = parsed;
    }

    const auto countX = static_cast<std::uint32_t>(
        std::clamp(
            std::llround(self.patternCountXSlider.doubleValue),
            static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
            static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
    const auto countY = static_cast<std::uint32_t>(
        std::clamp(
            std::llround(self.patternCountYSlider.doubleValue),
            static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
            static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
    const auto updateShape = [&](paperweight::ShapePrimitiveOperation& shape) {
        if (sender == self.surfaceKindPopup) {
            shape.kind = static_cast<paperweight::ShapePrimitiveKind>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        if (sender == self.courseFieldPopup) {
            shape.field = static_cast<paperweight::ShapeFieldKind>(
                self.courseFieldPopup.indexOfSelectedItem);
        }
        shape.columns = countX;
        shape.rows = countY;
        shape.width = self.patternValueOneSlider.doubleValue;
        shape.height = self.patternValueTwoSlider.doubleValue;
        shape.cornerRadius = self.patternValueThreeSlider.doubleValue;
        shape.rotationDegrees = self.patternValueFourSlider.doubleValue;
        shape.inset = self.courseGapSlider.doubleValue;
        shape.borderWidth = self.courseSoftnessSlider.doubleValue;
        shape.stagger = self.courseOverlapSlider.doubleValue;
        shape.softness = self.filterSensitivitySlider.doubleValue;
        shape.offsetX = self.inkRadiusSlider.doubleValue;
        shape.offsetY = self.inkThresholdSlider.doubleValue;
        if (sender == self.posteriseBandsSlider) {
            const auto vertices = static_cast<std::size_t>(std::llround(
                self.posteriseBandsSlider.doubleValue));
            shape.vertices = regularPolygonVertices(vertices);
        }
        if (parsedSeed) {
            shape.seedOffset = *parsedSeed;
        }
    };
    if (auto* brick = std::get_if<paperweight::BrickGridOperation>(&layer->operation)) {
        const bool wantsPhysical =
            self.physicalBrickCheckbox.state == NSControlStateValueOn;
        if (sender == self.physicalBrickCheckbox) {
            if (wantsPhysical && !brick->physicalDimensions) {
                const double width = material_.physicalSize.widthMetres / brick->columns;
                const double height = material_.physicalSize.heightMetres / brick->rows;
                const double mortar = brick->mortarSpace == paperweight::BrickMortarSpace::texture
                    ? brick->mortar * std::min(
                        material_.physicalSize.widthMetres,
                        material_.physicalSize.heightMetres)
                    : brick->mortar * std::min(width, height);
                brick->physicalDimensions = paperweight::BrickGridOperation::PhysicalDimensions{
                    width,
                    height,
                    mortar,
                };
            } else if (!wantsPhysical && brick->physicalDimensions) {
                const auto physical = *brick->physicalDimensions;
                brick->columns = static_cast<std::uint32_t>(std::clamp(
                    std::llround(material_.physicalSize.widthMetres / physical.widthMetres),
                    static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
                    static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
                brick->rows = static_cast<std::uint32_t>(std::clamp(
                    std::llround(material_.physicalSize.heightMetres / physical.heightMetres),
                    static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
                    static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
                brick->mortar = std::clamp(
                    physical.mortarMetres / std::min(
                        physical.widthMetres,
                        physical.heightMetres),
                    paperweight::LayerLimits::minimumGap,
                    paperweight::LayerLimits::maximumGap);
                brick->mortarSpace = paperweight::BrickMortarSpace::cell;
                brick->physicalDimensions.reset();
            }
        }

        if (sender == self.physicalBrickCheckbox) {
            [self refreshLayerInspector];
        }

        if (brick->physicalDimensions) {
            const auto width = positiveDecimal(self.physicalBrickWidthField);
            const auto height = positiveDecimal(self.physicalBrickHeightField);
            const auto mortar = nonNegativeDecimal(self.physicalBrickMortarField);
            if (!width || !height || !mortar || *mortar >= std::min(*width, *height)) {
                self.statusLabel.stringValue =
                    @"Brick dimensions must be positive metres; mortar may be zero but must be smaller than the brick.";
                self.statusLabel.textColor = NSColor.systemRedColor;
                return;
            }
            brick->physicalDimensions = paperweight::BrickGridOperation::PhysicalDimensions{
                *width,
                *height,
                *mortar,
            };
            brick->columns = countX;
            brick->rows = countY;

            const auto oldRepeat = material_.physicalSize;
            const paperweight::PhysicalSize newRepeat{
                *width * static_cast<double>(countX),
                *height * static_cast<double>(countY),
            };
            previewCoverage_ = {
                resizedCoverageExtent(
                    previewCoverage_.widthMetres,
                    oldRepeat.widthMetres,
                    newRepeat.widthMetres),
                resizedCoverageExtent(
                    previewCoverage_.heightMetres,
                    oldRepeat.heightMetres,
                    newRepeat.heightMetres),
            };
            material_.physicalSize = newRepeat;
            self.materialWidthField.stringValue = [NSString
                stringWithFormat:@"%.6g", newRepeat.widthMetres];
            self.materialHeightField.stringValue = [NSString
                stringWithFormat:@"%.6g", newRepeat.heightMetres];
            self.coverageWidthField.stringValue = [NSString
                stringWithFormat:@"%.6g", previewCoverage_.widthMetres];
            self.coverageHeightField.stringValue = [NSString
                stringWithFormat:@"%.6g", previewCoverage_.heightMetres];
            self.physicalBrickSummary.stringValue = [NSString stringWithFormat:
                @"%u × %u bricks → %.6g × %.6g m seamless repeat",
                countX,
                countY,
                newRepeat.widthMetres,
                newRepeat.heightMetres];
        } else {
            const auto previousMortarSpace = brick->mortarSpace;
            const auto previousMaximumCount = std::max(brick->columns, brick->rows);
            const bool equalWidth =
                self.equalMortarWidthCheckbox.state == NSControlStateValueOn;
            double mortar = sender == self.patternValueOneSlider
                ? self.patternValueOneSlider.doubleValue
                : brick->mortar;
            if (sender == self.equalMortarWidthCheckbox) {
                if (equalWidth && previousMortarSpace == paperweight::BrickMortarSpace::cell) {
                    mortar /= static_cast<double>(previousMaximumCount);
                } else if (!equalWidth &&
                           previousMortarSpace == paperweight::BrickMortarSpace::texture) {
                    mortar = std::min(
                        paperweight::LayerLimits::maximumGap,
                        mortar * static_cast<double>(previousMaximumCount));
                }
            }
            brick->columns = countX;
            brick->rows = countY;
            brick->mortarSpace = equalWidth
                ? paperweight::BrickMortarSpace::texture
                : paperweight::BrickMortarSpace::cell;
            self.patternValueOneSlider.maxValue = equalWidth
                ? textureSpaceMortarMaximum(*brick)
                : paperweight::LayerLimits::maximumGap;
            brick->mortar = std::clamp(
                mortar,
                paperweight::LayerLimits::minimumGap,
                self.patternValueOneSlider.maxValue);
            self.patternValueOneSlider.doubleValue = brick->mortar;
            self.patternValueOneLabel.stringValue = equalWidth ? @"Mortar width" : @"Mortar";
        }
        brick->stagger = self.patternValueTwoSlider.doubleValue;
        brick->softness = self.patternValueThreeSlider.doubleValue;
    } else if (auto* course =
                   std::get_if<paperweight::CourseLayoutOperation>(&layer->operation)) {
        const bool wantsPhysical =
            self.physicalBrickCheckbox.state == NSControlStateValueOn;
        if (sender == self.physicalBrickCheckbox) {
            if (wantsPhysical && !course->physicalDimensions) {
                const double width = material_.physicalSize.widthMetres / course->blocks;
                const double height = material_.physicalSize.heightMetres / course->courses;
                course->physicalDimensions =
                    paperweight::CourseLayoutOperation::PhysicalDimensions{
                        width,
                        height,
                        course->gap * std::min(width, height),
                        course->overlap * height,
                    };
            } else if (!wantsPhysical && course->physicalDimensions) {
                const auto physical = *course->physicalDimensions;
                course->blocks = static_cast<std::uint32_t>(std::clamp(
                    std::llround(material_.physicalSize.widthMetres /
                        physical.blockWidthMetres),
                    static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
                    static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
                course->courses = static_cast<std::uint32_t>(std::clamp(
                    std::llround(material_.physicalSize.heightMetres /
                        physical.courseHeightMetres),
                    static_cast<long long>(paperweight::LayerLimits::minimumPatternCount),
                    static_cast<long long>(paperweight::LayerLimits::maximumPatternCount)));
                course->gap = std::clamp(
                    physical.gapMetres /
                        std::min(physical.blockWidthMetres, physical.courseHeightMetres),
                    paperweight::LayerLimits::minimumGap,
                    paperweight::LayerLimits::maximumGap);
                course->overlap = std::clamp(
                    physical.overlapMetres / physical.courseHeightMetres,
                    paperweight::LayerLimits::minimumLayoutOverlap,
                    paperweight::LayerLimits::maximumLayoutOverlap);
                course->physicalDimensions.reset();
            }
            [self refreshLayerInspector];
        }

        if (course->physicalDimensions) {
            const auto width = positiveDecimal(self.physicalBrickWidthField);
            const auto height = positiveDecimal(self.physicalBrickHeightField);
            const auto gap = nonNegativeDecimal(self.physicalBrickMortarField);
            const auto overlap = nonNegativeDecimal(self.physicalCourseOverlapField);
            if (!width || !height || !gap || !overlap ||
                *gap >= std::min(*width, *height) || *overlap >= *height) {
                self.statusLabel.stringValue =
                    @"Course dimensions must be positive metres; gap and overlap must fit inside the block and course.";
                self.statusLabel.textColor = NSColor.systemRedColor;
                return;
            }
            course->physicalDimensions =
                paperweight::CourseLayoutOperation::PhysicalDimensions{
                    *width,
                    *height,
                    *gap,
                    *overlap,
                };
            course->blocks = countX;
            course->courses = countY;
            const auto oldRepeat = material_.physicalSize;
            const paperweight::PhysicalSize newRepeat{
                *width * static_cast<double>(countX),
                *height * static_cast<double>(countY),
            };
            previewCoverage_ = {
                resizedCoverageExtent(
                    previewCoverage_.widthMetres,
                    oldRepeat.widthMetres,
                    newRepeat.widthMetres),
                resizedCoverageExtent(
                    previewCoverage_.heightMetres,
                    oldRepeat.heightMetres,
                    newRepeat.heightMetres),
            };
            material_.physicalSize = newRepeat;
            self.materialWidthField.stringValue = [NSString
                stringWithFormat:@"%.6g", newRepeat.widthMetres];
            self.materialHeightField.stringValue = [NSString
                stringWithFormat:@"%.6g", newRepeat.heightMetres];
            self.coverageWidthField.stringValue = [NSString
                stringWithFormat:@"%.6g", previewCoverage_.widthMetres];
            self.coverageHeightField.stringValue = [NSString
                stringWithFormat:@"%.6g", previewCoverage_.heightMetres];
            self.physicalBrickSummary.stringValue = [NSString stringWithFormat:
                @"%u blocks × %u courses → %.6g × %.6g m seamless repeat",
                countX,
                countY,
                newRepeat.widthMetres,
                newRepeat.heightMetres];
        } else {
            course->blocks = countX;
            course->courses = countY;
            course->gap = self.courseGapSlider.doubleValue;
            course->overlap = self.courseOverlapSlider.doubleValue;
        }
        if (sender == self.surfaceKindPopup) {
            course->profile = static_cast<paperweight::CourseLayoutProfile>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        if (sender == self.courseFieldPopup) {
            course->field = static_cast<paperweight::CourseLayoutField>(
                self.courseFieldPopup.indexOfSelectedItem);
        }
        course->blockVariation = self.patternValueOneSlider.doubleValue;
        course->courseVariation = self.patternValueTwoSlider.doubleValue;
        course->stagger = self.patternValueThreeSlider.doubleValue;
        course->crookedness = self.patternValueFourSlider.doubleValue;
        course->softness = self.courseSoftnessSlider.doubleValue;
        if (parsedSeed) {
            course->seedOffset = *parsedSeed;
        }
    } else if (auto* sculpt =
                   std::get_if<paperweight::RegionSurfaceOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            sculpt->profile = static_cast<paperweight::BevelProfile>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        if (sender == self.courseFieldPopup) {
            sculpt->field = static_cast<paperweight::RegionSurfaceField>(
                self.courseFieldPopup.indexOfSelectedItem);
        }
        sculpt->facetCount = static_cast<std::uint32_t>(std::clamp(
            countX,
            paperweight::LayerLimits::minimumFacetCount,
            paperweight::LayerLimits::maximumFacetCount));
        sculpt->chipScale = static_cast<std::uint32_t>(std::clamp(
            countY,
            paperweight::LayerLimits::minimumChipScale,
            paperweight::LayerLimits::maximumChipScale));
        sculpt->bevelWidth = self.patternValueOneSlider.doubleValue;
        sculpt->bevelHeight = self.patternValueTwoSlider.doubleValue;
        sculpt->facetStrength = self.patternValueThreeSlider.doubleValue;
        sculpt->centrePeak = self.patternValueFourSlider.doubleValue;
        sculpt->slopeStrength = self.courseGapSlider.doubleValue;
        sculpt->chipAmount = self.courseSoftnessSlider.doubleValue;
        sculpt->wearAmount = self.courseOverlapSlider.doubleValue;
        sculpt->facetedNormals =
            self.facetedNormalsCheckbox.state == NSControlStateValueOn;
        if (parsedSeed) {
            sculpt->seedOffset = *parsedSeed;
        }
    } else if (auto* cells =
                   std::get_if<paperweight::OrganicCellOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            cells->field = static_cast<paperweight::OrganicCellField>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        if (sender == self.courseFieldPopup) {
            cells->direction = static_cast<paperweight::OrganicDirection>(
                self.courseFieldPopup.indexOfSelectedItem);
        }
        cells->columns = countX;
        cells->rows = countY;
        cells->anisotropy = self.patternValueOneSlider.doubleValue;
        cells->jitter = self.patternValueTwoSlider.doubleValue;
        cells->irregularity = self.patternValueThreeSlider.doubleValue;
        cells->gap = self.patternValueFourSlider.doubleValue;
        cells->softness = self.courseSoftnessSlider.doubleValue;
        if (parsedSeed) {
            cells->seedOffset = *parsedSeed;
        }
    } else if (auto* cracks =
                   std::get_if<paperweight::OrganicCrackOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            cracks->field = static_cast<paperweight::OrganicCrackField>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        if (sender == self.courseFieldPopup) {
            cracks->direction = static_cast<paperweight::OrganicDirection>(
                self.courseFieldPopup.indexOfSelectedItem);
        }
        cracks->roots = std::min(countX, paperweight::LayerLimits::maximumCrackRoots);
        cracks->segments = std::clamp<std::uint32_t>(
            countY, 2, paperweight::LayerLimits::maximumCrackSegments);
        cracks->branchProbability = self.patternValueOneSlider.doubleValue;
        cracks->bend = self.patternValueTwoSlider.doubleValue;
        cracks->width = self.patternValueThreeSlider.doubleValue;
        cracks->taper = self.patternValueFourSlider.doubleValue;
        cracks->softness = self.courseSoftnessSlider.doubleValue;
        if (parsedSeed) {
            cracks->seedOffset = *parsedSeed;
        }
    } else if (auto* leaves =
                   std::get_if<paperweight::LeafClusterOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            leaves->profile = static_cast<paperweight::LeafProfile>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        if (sender == self.courseFieldPopup) {
            leaves->field = static_cast<paperweight::LeafField>(
                self.courseFieldPopup.indexOfSelectedItem);
        }
        leaves->columns = countX;
        leaves->rows = countY;
        leaves->density = self.patternValueOneSlider.doubleValue;
        leaves->clusterSpread = self.patternValueTwoSlider.doubleValue;
        leaves->leafLength = self.patternValueThreeSlider.doubleValue;
        leaves->leafWidth = self.patternValueFourSlider.doubleValue;
        leaves->scaleVariation = self.courseGapSlider.doubleValue;
        leaves->rotationVariation = self.courseSoftnessSlider.doubleValue;
        leaves->directionDegrees = self.courseOverlapSlider.doubleValue;
        if (parsedSeed) {
            leaves->seedOffset = *parsedSeed;
        }
    } else if (auto* growth =
                   std::get_if<paperweight::OrganicAccumulationOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            growth->kind = static_cast<paperweight::OrganicAccumulationKind>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        if (sender == self.courseFieldPopup) {
            growth->source = static_cast<paperweight::OrganicAccumulationSource>(
                self.courseFieldPopup.indexOfSelectedItem);
        }
        growth->scale = countX;
        growth->coverage = self.patternValueOneSlider.doubleValue;
        growth->softness = self.patternValueTwoSlider.doubleValue;
        growth->moistureBias = self.patternValueThreeSlider.doubleValue;
        growth->breakup = self.patternValueFourSlider.doubleValue;
        growth->variation = self.courseGapSlider.doubleValue;
        if (parsedSeed) {
            growth->seedOffset = *parsedSeed;
        }
    } else if (auto* scatter =
                   std::get_if<paperweight::ScatterOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            scatter->stamp.kind = static_cast<paperweight::ShapePrimitiveKind>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        if (sender == self.courseFieldPopup) {
            scatter->field = static_cast<paperweight::ScatterField>(
                self.courseFieldPopup.indexOfSelectedItem);
        }
        scatter->columns = countX;
        scatter->rows = countY;
        scatter->density = self.patternValueOneSlider.doubleValue;
        scatter->jitter = self.patternValueTwoSlider.doubleValue;
        scatter->minimumDistance = self.patternValueThreeSlider.doubleValue;
        scatter->maximumOverlap = self.patternValueFourSlider.doubleValue;
        if (parsedSeed) {
            scatter->seedOffset = *parsedSeed;
        }
        selectedScatterPopulation_ = std::clamp<NSInteger>(
            selectedScatterPopulation_, 0,
            static_cast<NSInteger>(scatter->populations.size() - 1));
        auto& population = scatter->populations[
            static_cast<std::size_t>(selectedScatterPopulation_)];
        population.minimumScale = std::min(
            self.courseGapSlider.doubleValue,
            self.courseSoftnessSlider.doubleValue);
        population.maximumScale = std::max(
            self.courseGapSlider.doubleValue,
            self.courseSoftnessSlider.doubleValue);
        self.courseGapSlider.doubleValue = population.minimumScale;
        self.courseSoftnessSlider.doubleValue = population.maximumScale;
        population.weight = self.courseOverlapSlider.doubleValue;
        scatter->exclusionMask.enabled =
            self.facetedNormalsCheckbox.state == NSControlStateValueOn;
    } else if (auto* shape =
                   std::get_if<paperweight::ShapePrimitiveOperation>(&layer->operation)) {
        updateShape(*shape);
    } else if (auto* boolean =
                   std::get_if<paperweight::ShapeBooleanOperation>(&layer->operation)) {
        updateShape(boolean->shape);
    } else if (auto* lattice =
                   std::get_if<paperweight::LatticeOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            lattice->kind = static_cast<paperweight::LatticeKind>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        auto windingX = static_cast<std::int32_t>(
            std::llround(self.patternCountXSlider.doubleValue));
        auto windingY = static_cast<std::int32_t>(
            std::llround(self.patternCountYSlider.doubleValue));
        if (windingX == 0 && windingY == 0) {
            if (sender == self.patternCountXSlider) {
                windingX = 1;
            } else {
                windingY = 1;
            }
        }
        if (lattice->kind == paperweight::LatticeKind::diamonds) {
            if (windingX == 0) {
                windingX = 1;
            }
            if (windingY == 0) {
                windingY = 1;
            }
        }
        lattice->windingX = windingX;
        lattice->windingY = windingY;
        lattice->width = self.patternValueOneSlider.doubleValue;
        lattice->phase = self.patternValueTwoSlider.doubleValue;
        lattice->softness = self.patternValueThreeSlider.doubleValue;
        self.patternCountXSlider.doubleValue = windingX;
        self.patternCountYSlider.doubleValue = windingY;
    } else if (auto* tile = std::get_if<paperweight::TileGridOperation>(&layer->operation)) {
        tile->columns = countX;
        tile->rows = countY;
        tile->grout = self.patternValueOneSlider.doubleValue;
        tile->softness = self.patternValueThreeSlider.doubleValue;
    } else if (auto* worley =
                   std::get_if<paperweight::WorleyCellsOperation>(&layer->operation)) {
        worley->columns = countX;
        worley->rows = countY;
        worley->jitter = self.patternValueOneSlider.doubleValue;
        worley->edgeWidth = self.patternValueTwoSlider.doubleValue;
        if (parsedSeed) {
            worley->seedOffset = *parsedSeed;
        }
    } else if (auto* cells =
                   std::get_if<paperweight::RandomCellsOperation>(&layer->operation)) {
        cells->columns = countX;
        cells->rows = countY;
        if (parsedSeed) {
            cells->seedOffset = *parsedSeed;
        }
    } else if (auto* lines = std::get_if<paperweight::LinesOperation>(&layer->operation)) {
        lines->direction = self.patternDirectionControl.selectedSegment == 1
            ? paperweight::LineDirection::horizontal
            : paperweight::LineDirection::vertical;
        lines->count = countX;
        lines->width = self.patternValueOneSlider.doubleValue;
        lines->softness = self.patternValueThreeSlider.doubleValue;
    } else if (auto* rectangles =
                   std::get_if<paperweight::RectanglesOperation>(&layer->operation)) {
        rectangles->columns = countX;
        rectangles->rows = countY;
        rectangles->width = self.patternValueOneSlider.doubleValue;
        rectangles->height = self.patternValueTwoSlider.doubleValue;
        rectangles->softness = self.patternValueThreeSlider.doubleValue;
    } else if (auto* circles =
                   std::get_if<paperweight::CirclesOperation>(&layer->operation)) {
        circles->columns = countX;
        circles->rows = countY;
        circles->radius = self.patternValueOneSlider.doubleValue;
        circles->softness = self.patternValueThreeSlider.doubleValue;
    } else if (auto* surface =
                   std::get_if<paperweight::SurfacePatternOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            surface->kind = static_cast<paperweight::SurfacePatternKind>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        surface->scale = countX;
        surface->width = self.patternValueOneSlider.doubleValue;
        surface->detail = self.patternValueTwoSlider.doubleValue;
        surface->distortion = self.patternValueThreeSlider.doubleValue;
        surface->variation = self.patternValueFourSlider.doubleValue;
        if (parsedSeed) {
            surface->seedOffset = *parsedSeed;
        }
    } else if (auto* filter =
                   std::get_if<paperweight::SurfaceFilterOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            filter->kind = static_cast<paperweight::SurfaceFilterKind>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        filter->radius = self.patternValueOneSlider.doubleValue;
        filter->strength = self.patternValueTwoSlider.doubleValue;
        filter->target = processingTargetAtIndex(
            self.processingTargetPopup.indexOfSelectedItem);
    } else if (auto* region =
                   std::get_if<paperweight::RegionFieldOperation>(&layer->operation)) {
        if (sender == self.surfaceKindPopup) {
            region->field = static_cast<paperweight::RegionFieldKind>(
                self.surfaceKindPopup.indexOfSelectedItem);
        }
        region->channel = countX;
        region->outputLow = self.patternValueOneSlider.doubleValue;
        region->outputHigh = self.patternValueTwoSlider.doubleValue;
        if (parsedSeed) {
            region->seedOffset = *parsedSeed;
        }
    } else {
        return;
    }

    [self updateLayerInspectorLiveValueLabels];
    if (sender == self.surfaceKindPopup) {
        self.layerTypeLabel.stringValue = [NSString
            stringWithFormat:@"%ld. %@", selectedLayer_ + 1, operationDisplayName(layer->operation)];
        [self rebuildLayerList];
        [self refreshLayerInspector];
    }
    [self regeneratePreview];
    [self markDirty];
}

- (void)maskParameterChanged:(id)sender
{
    auto* layer = layerAt(material_, selectedLayer_);
    if (layer == nullptr) {
        return;
    }

    auto& mask = layer->mask;
    mask.enabled = self.maskEnabledCheckbox.state == NSControlStateValueOn;
    mask.inverted = self.maskInvertedCheckbox.state == NSControlStateValueOn;
    if (sender == self.maskSeedOffsetField) {
        const std::string text = self.maskSeedOffsetField.stringValue.UTF8String;
        std::uint64_t parsed = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
        if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
            self.statusLabel.stringValue = @"A mask seed offset must be a non-negative integer.";
            self.statusLabel.textColor = NSColor.systemRedColor;
            return;
        }
        mask.seedOffset = parsed;
    }

    double low = self.maskLowSlider.doubleValue;
    double high = self.maskHighSlider.doubleValue;
    if (low >= high) {
        if (sender == self.maskLowSlider) {
            low = std::min(low, 0.99);
            high = std::max(high, low + 0.01);
        } else {
            high = std::max(high, 0.01);
            low = std::min(low, high - 0.01);
        }
        self.maskLowSlider.doubleValue = low;
        self.maskHighSlider.doubleValue = high;
    }
    mask.inputLow = low;
    mask.inputHigh = high;

    [self updateLayerInspectorLiveValueLabels];
    if (sender == self.maskEnabledCheckbox) {
        self.maskInvertedCheckbox.enabled = mask.enabled;
        self.maskSeedOffsetField.enabled = mask.enabled;
        self.maskLowSlider.enabled = mask.enabled;
        self.maskHighSlider.enabled = mask.enabled;
    }
    [self regeneratePreview];
    [self markDirty];
}

- (void)materialSizeChanged:(id)sender
{
    static_cast<void>(sender);
    if (materialUsesDerivedRepeat(material_)) {
        self.materialWidthField.stringValue = [NSString
            stringWithFormat:@"%.6g", material_.physicalSize.widthMetres];
        self.materialHeightField.stringValue = [NSString
            stringWithFormat:@"%.6g", material_.physicalSize.heightMetres];
        self.statusLabel.stringValue =
            @"Repeat size is calculated from the physical layout dimensions and counts.";
        self.statusLabel.textColor = NSColor.secondaryLabelColor;
        return;
    }
    const auto width = positiveDecimal(self.materialWidthField);
    const auto height = positiveDecimal(self.materialHeightField);
    if (!width || !height) {
        self.statusLabel.stringValue = @"Material repeat width and height must be positive metre values.";
        self.statusLabel.textColor = NSColor.systemRedColor;
        return;
    }
    const bool coverageFollowedMaterial =
        previewCoverage_ == material_.physicalSize;
    material_.physicalSize = {*width, *height};
    if (coverageFollowedMaterial) {
        previewCoverage_ = material_.physicalSize;
        self.coverageWidthField.stringValue = [NSString stringWithFormat:@"%.6g", *width];
        self.coverageHeightField.stringValue = [NSString stringWithFormat:@"%.6g", *height];
    }
    [self regeneratePreview];
    [self markDirty];
}

- (void)coverageChanged:(id)sender
{
    static_cast<void>(sender);
    const auto width = positiveDecimal(self.coverageWidthField);
    const auto height = positiveDecimal(self.coverageHeightField);
    if (!width || !height) {
        self.statusLabel.stringValue = @"Preview coverage width and height must be positive metre values.";
        self.statusLabel.textColor = NSColor.systemRedColor;
        return;
    }
    previewCoverage_ = {*width, *height};
    [self regeneratePreview];
}

- (void)parameterChanged:(id)sender
{
    if (sender == self.seedField) {
        const std::string text = self.seedField.stringValue.UTF8String;
        std::uint64_t parsed = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
        if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
            self.statusLabel.stringValue = @"Seed must be a non-negative integer.";
            return;
        }
        material_.seed = parsed;
    }

    material_.frequency = static_cast<std::uint32_t>(std::llround(self.frequencySlider.doubleValue));
    material_.octaves = static_cast<std::uint32_t>(std::llround(self.octavesSlider.doubleValue));
    material_.lacunarity = static_cast<std::uint32_t>(std::llround(self.lacunaritySlider.doubleValue));
    material_.gain = self.gainSlider.doubleValue;
    material_.normalStrength = self.normalStrengthSlider.doubleValue;
    material_.roughnessLow = self.roughnessLowSlider.doubleValue;
    material_.roughnessHigh = self.roughnessHighSlider.doubleValue;
    [self updateControlLabels];
    [self regeneratePreview];
    [self markDirty];
}

- (void)randomiseSeed:(id)sender
{
    static_cast<void>(sender);
    material_.seed = paperweight::mixBits(material_.seed);
    self.seedField.stringValue = [NSString stringWithFormat:@"%llu", material_.seed];
    [self regeneratePreview];
    [self markDirty];
}

- (void)resetMaterial:(id)sender
{
    static_cast<void>(sender);
    [self setActiveReferenceTemplate:nullptr];
    material_ = paperweight::Material{};
    material_.layers.push_back(paperweight::makeNoiseLayer());
    previewCoverage_ = material_.physicalSize;
    selectedLayer_ = 0;
    self.seedField.stringValue = [NSString stringWithFormat:@"%llu", material_.seed];
    self.materialWidthField.stringValue = [NSString
        stringWithFormat:@"%.6g", material_.physicalSize.widthMetres];
    self.materialHeightField.stringValue = [NSString
        stringWithFormat:@"%.6g", material_.physicalSize.heightMetres];
    self.coverageWidthField.stringValue = [NSString
        stringWithFormat:@"%.6g", previewCoverage_.widthMetres];
    self.coverageHeightField.stringValue = [NSString
        stringWithFormat:@"%.6g", previewCoverage_.heightMetres];
    self.frequencySlider.doubleValue = material_.frequency;
    self.octavesSlider.doubleValue = material_.octaves;
    self.lacunaritySlider.doubleValue = material_.lacunarity;
    self.gainSlider.doubleValue = material_.gain;
    self.lowColourWell.color = colourFromRgba8(material_.lowColour);
    self.highColourWell.color = colourFromRgba8(material_.highColour);
    self.normalStrengthSlider.doubleValue = material_.normalStrength;
    self.roughnessLowSlider.doubleValue = material_.roughnessLow;
    self.roughnessHighSlider.doubleValue = material_.roughnessHigh;
    self.displacementSlider.doubleValue = recommendedPreviewDisplacement(material_);
    self.material3DPreviewView.displacementStrength = self.displacementSlider.doubleValue;
    [self rebuildLayerList];
    [self refreshLayerInspector];
    [self updateControlLabels];
    [self regeneratePreview];
    [self markDirty];
}

- (void)colourChanged:(id)sender
{
    static_cast<void>(sender);
    material_.lowColour = rgba8FromColour(self.lowColourWell.color);
    material_.highColour = rgba8FromColour(self.highColourWell.color);
    [self regeneratePreview];
    [self markDirty];
}

- (void)tilingChanged:(id)sender
{
    static_cast<void>(sender);
    self.previewView.repeatCount = self.tilingControl.selectedSegment == 1 ? 3 : 1;
}

- (void)outputChanged:(id)sender
{
    static_cast<void>(sender);
    bakedPresentationSelected_ = false;
    self.bakedPresentationCheckbox.state = NSControlStateValueOff;
    switch (self.outputControl.selectedSegment) {
    case 0:
        selectedOutput_ = paperweight::MaterialOutput::colour;
        break;
    case 1:
        selectedOutput_ = paperweight::MaterialOutput::height;
        break;
    case 2:
        selectedOutput_ = paperweight::MaterialOutput::normal;
        break;
    case 3:
        selectedOutput_ = paperweight::MaterialOutput::roughness;
        break;
    default:
        selectedOutput_ = paperweight::MaterialOutput::colour;
        break;
    }
    self.bakeControls.hidden = !bakedPresentationSelected_;
    [self regeneratePreview];
}

- (void)bakedPresentationToggled:(id)sender
{
    static_cast<void>(sender);
    bakedPresentationSelected_ =
        self.bakedPresentationCheckbox.state == NSControlStateValueOn;
    if (bakedPresentationSelected_) {
        selectedOutput_ = paperweight::MaterialOutput::colour;
        self.outputControl.selectedSegment = 0;
    }
    self.bakeControls.hidden = !bakedPresentationSelected_;
    [self regeneratePreview];
}

- (paperweight::StylisedLightingSettings)stylisedLightingSettings
{
    paperweight::StylisedLightingSettings settings;
    settings.lightAzimuthDegrees = self.bakeAzimuthSlider.doubleValue;
    settings.lightElevationDegrees = self.bakeElevationSlider.doubleValue;
    settings.diffuseBands = static_cast<std::uint32_t>(
        std::llround(self.bakeBandsSlider.doubleValue));
    settings.highlightThreshold = self.bakeHighlightSlider.doubleValue;
    settings.ambientContribution = self.bakeAmbientSlider.doubleValue;
    return settings;
}

- (void)updateBakeControlLabels
{
    self.bakeAzimuthValue.stringValue = [NSString stringWithFormat:
        @"%.0f°", self.bakeAzimuthSlider.doubleValue];
    self.bakeElevationValue.stringValue = [NSString stringWithFormat:
        @"%.0f°", self.bakeElevationSlider.doubleValue];
    self.bakeBandsValue.stringValue = [NSString stringWithFormat:
        @"%.0f", self.bakeBandsSlider.doubleValue];
    self.bakeHighlightValue.stringValue = [NSString stringWithFormat:
        @"%.2f", self.bakeHighlightSlider.doubleValue];
    self.bakeAmbientValue.stringValue = [NSString stringWithFormat:
        @"%.2f", self.bakeAmbientSlider.doubleValue];
}

- (void)bakeParameterChanged:(id)sender
{
    static_cast<void>(sender);
    [self updateBakeControlLabels];
    if (bakedPresentationSelected_) {
        [self regeneratePreview];
    }
}

- (NSString*)currentOutputName
{
    return bakedPresentationSelected_ ? @"Baked Presentation" : outputName(selectedOutput_);
}

- (void)previewResolutionChanged:(id)sender
{
    static_cast<void>(sender);
    constexpr std::array<std::uint32_t, 5> resolutions{64, 128, 256, 512, 1024};
    const auto index = static_cast<std::size_t>(self.previewResolutionPopup.indexOfSelectedItem);
    if (index >= resolutions.size()) {
        return;
    }
    previewResolution_ = resolutions[index];
    [NSUserDefaults.standardUserDefaults setInteger:previewResolution_
                                             forKey:@"previewResolution"];
    [self regeneratePreview];
}

- (void)previewModeChanged:(id)sender
{
    static_cast<void>(sender);
    const BOOL threeDimensional = self.previewModeControl.selectedSegment == 1;
    if (threeDimensional && !self.material3DPreviewView.isRendererAvailable) {
        self.previewModeControl.selectedSegment = 0;
        self.statusLabel.stringValue = @"The 3D preview requires a compatible Metal device.";
        self.statusLabel.textColor = NSColor.systemRedColor;
        return;
    }
    self.comparisonStack.hidden = threeDimensional;
    self.material3DPreviewView.hidden = !threeDimensional;
    self.twoDPreviewControls.hidden = threeDimensional;
    self.threeDPreviewControls.hidden = !threeDimensional;
    if (!threeDimensional && self.material3DPreviewView.isAnimationRunning) {
        [self togglePreviewAnimation:nil];
    }
    [self regeneratePreview];
}

- (void)previewShapeChanged:(id)sender
{
    static_cast<void>(sender);
    self.material3DPreviewView.previewShape =
        static_cast<PWPreviewShape>(self.previewShapePopup.indexOfSelectedItem);
}

- (void)lightPresetChanged:(id)sender
{
    static_cast<void>(sender);
    switch (self.lightPresetPopup.indexOfSelectedItem) {
    case 0:
        self.lightAzimuthSlider.doubleValue = 35.0;
        self.lightElevationSlider.doubleValue = 38.0;
        self.lightIntensitySlider.doubleValue = 1.0;
        self.ambientIntensitySlider.doubleValue = 0.18;
        break;
    case 1:
        self.lightAzimuthSlider.doubleValue = 110.0;
        self.lightElevationSlider.doubleValue = 12.0;
        self.lightIntensitySlider.doubleValue = 1.25;
        self.ambientIntensitySlider.doubleValue = 0.10;
        break;
    case 2:
        self.lightAzimuthSlider.doubleValue = 0.0;
        self.lightElevationSlider.doubleValue = 82.0;
        self.lightIntensitySlider.doubleValue = 0.92;
        self.ambientIntensitySlider.doubleValue = 0.22;
        break;
    case 3:
        self.lightAzimuthSlider.doubleValue = 205.0;
        self.lightElevationSlider.doubleValue = 25.0;
        self.lightIntensitySlider.doubleValue = 1.15;
        self.ambientIntensitySlider.doubleValue = 0.12;
        break;
    default:
        return;
    }
    [self applyPreview3DParameters];
}

- (void)preview3DParameterChanged:(id)sender
{
    if (sender == self.lightAzimuthSlider || sender == self.lightElevationSlider ||
        sender == self.lightIntensitySlider || sender == self.ambientIntensitySlider) {
        [self.lightPresetPopup selectItemAtIndex:4];
    }
    [self applyPreview3DParameters];
}

- (void)applyPreview3DParameters
{
    self.material3DPreviewView.lightAzimuthDegrees = self.lightAzimuthSlider.doubleValue;
    self.material3DPreviewView.lightElevationDegrees = self.lightElevationSlider.doubleValue;
    self.material3DPreviewView.lightIntensity = self.lightIntensitySlider.doubleValue;
    self.material3DPreviewView.ambientIntensity = self.ambientIntensitySlider.doubleValue;
    self.material3DPreviewView.displacementStrength = self.displacementSlider.doubleValue;
    self.material3DPreviewView.previewNormalStrength = self.previewNormalSlider.doubleValue;
    self.material3DPreviewView.toonLightingEnabled =
        self.toonLightingCheckbox.state == NSControlStateValueOn;
    self.material3DPreviewView.toonBandCount = self.toonBandsSlider.doubleValue;
    self.material3DPreviewView.toonSpecularThreshold = self.toonSpecularSlider.doubleValue;
    self.material3DPreviewView.toonRimStrength = self.toonRimSlider.doubleValue;
    self.material3DPreviewView.animationPhase = self.animationPhaseSlider.doubleValue;
    self.material3DPreviewView.animationSpeed = self.animationSpeedSlider.doubleValue;
    [self updatePreview3DControlLabels];
}

- (void)updatePreview3DControlLabels
{
    self.lightAzimuthValue.stringValue = [NSString
        stringWithFormat:@"%.0f°", self.lightAzimuthSlider.doubleValue];
    self.lightElevationValue.stringValue = [NSString
        stringWithFormat:@"%.0f°", self.lightElevationSlider.doubleValue];
    self.lightIntensityValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.lightIntensitySlider.doubleValue];
    self.ambientIntensityValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.ambientIntensitySlider.doubleValue];
    self.displacementValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.displacementSlider.doubleValue];
    self.previewNormalValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.previewNormalSlider.doubleValue];
    self.toonBandsValue.stringValue = [NSString
        stringWithFormat:@"%.0f", self.toonBandsSlider.doubleValue];
    self.toonSpecularValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.toonSpecularSlider.doubleValue];
    self.toonRimValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.toonRimSlider.doubleValue];
    const BOOL toonEnabled = self.toonLightingCheckbox.state == NSControlStateValueOn;
    self.toonBandsSlider.enabled = toonEnabled;
    self.toonSpecularSlider.enabled = toonEnabled;
    self.toonRimSlider.enabled = toonEnabled;
    self.animationPhaseValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.animationPhaseSlider.doubleValue];
    self.animationSpeedValue.stringValue = [NSString
        stringWithFormat:@"%.2f×", self.animationSpeedSlider.doubleValue];
}

- (void)previewMapToggled:(id)sender
{
    static_cast<void>(sender);
    self.material3DPreviewView.colourEnabled =
        self.colourMapCheckbox.state == NSControlStateValueOn;
    self.material3DPreviewView.heightEnabled =
        self.heightMapCheckbox.state == NSControlStateValueOn;
    self.material3DPreviewView.normalEnabled =
        self.normalMapCheckbox.state == NSControlStateValueOn;
    self.material3DPreviewView.roughnessEnabled =
        self.roughnessMapCheckbox.state == NSControlStateValueOn;
}

- (void)togglePreviewAnimation:(id)sender
{
    static_cast<void>(sender);
    const BOOL running = !self.material3DPreviewView.isAnimationRunning;
    self.material3DPreviewView.animationRunning = running;
    self.animationButton.title = running ? @"Pause Light" : @"Play Light";
    [self.animationUiTimer invalidate];
    self.animationUiTimer = nil;
    if (running) {
        self.animationUiTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 30.0
                                                                target:self
                                                              selector:@selector(previewAnimationTick:)
                                                              userInfo:nil
                                                               repeats:YES];
    }
}

- (void)previewAnimationTick:(NSTimer*)timer
{
    static_cast<void>(timer);
    self.animationPhaseSlider.doubleValue = self.material3DPreviewView.animationPhase;
    self.animationPhaseValue.stringValue = [NSString
        stringWithFormat:@"%.2f", self.animationPhaseSlider.doubleValue];
}

- (void)resetPreviewCamera:(id)sender
{
    static_cast<void>(sender);
    [self.material3DPreviewView resetCamera];
}

- (void)updateControlLabels
{
    self.frequencyValue.stringValue = [NSString stringWithFormat:@"%u", material_.frequency];
    self.octavesValue.stringValue = [NSString stringWithFormat:@"%u", material_.octaves];
    self.lacunarityValue.stringValue = [NSString stringWithFormat:@"%u", material_.lacunarity];
    self.gainValue.stringValue = [NSString stringWithFormat:@"%.2f", material_.gain];
    self.normalStrengthValue.stringValue =
        [NSString stringWithFormat:@"%.1f", material_.normalStrength];
    self.roughnessLowValue.stringValue =
        [NSString stringWithFormat:@"%.2f", material_.roughnessLow];
    self.roughnessHighValue.stringValue =
        [NSString stringWithFormat:@"%.2f", material_.roughnessHigh];
}

- (void)setActiveReferenceTemplate:(const paperweight::ReferenceMaterialTemplate*)descriptor
{
    activeTemplate_ = descriptor;
    for (NSView* view in [self.templateControlsGroup.arrangedSubviews copy]) {
        [self.templateControlsGroup removeArrangedSubview:view];
        [view removeFromSuperview];
    }
    [self.templateControlSliders removeAllObjects];
    [self.templateControlValues removeAllObjects];

    if (descriptor == nullptr) {
        self.templateControlsGroup.hidden = YES;
        return;
    }

    self.activeTemplateLabel = makeLabel([NSString stringWithFormat:
        @"Material template: %s", descriptor->displayName.data()]);
    self.activeTemplateLabel.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold];
    self.activeTemplateLabel.maximumNumberOfLines = 2;
    [self.templateControlsGroup addArrangedSubview:self.activeTemplateLabel];
    for (std::size_t index = 0; index < descriptor->controls.size(); ++index) {
        const auto& control = descriptor->controls[index];
        auto* row = makePreviewSliderRow(
            [NSString stringWithUTF8String:control.displayName.data()],
            control.minimumValue,
            control.maximumValue,
            control.defaultValue,
            self);
        auto* slider = static_cast<NSSlider*>(row.views[1]);
        auto* value = static_cast<NSTextField*>(row.views[2]);
        slider.target = self;
        slider.action = @selector(templateControlChanged:);
        slider.tag = static_cast<NSInteger>(index);
        if (control.step >= 1.0) {
            value.stringValue = [NSString stringWithFormat:@"%.0f", control.defaultValue];
        } else {
            value.stringValue = [NSString stringWithFormat:@"%.3g", control.defaultValue];
        }
        [self.templateControlSliders addObject:slider];
        [self.templateControlValues addObject:value];
        [self.templateControlsGroup addArrangedSubview:row];
        [row.widthAnchor constraintEqualToAnchor:self.templateControlsGroup.widthAnchor].active = YES;
    }
    self.templateControlsGroup.hidden = NO;
}

- (void)templateControlChanged:(NSSlider*)sender
{
    if (activeTemplate_ == nullptr || sender.tag < 0 ||
        static_cast<std::size_t>(sender.tag) >= activeTemplate_->controls.size()) {
        return;
    }
    const auto& control = activeTemplate_->controls[static_cast<std::size_t>(sender.tag)];
    const double stepped = control.step > 0.0
        ? std::round(sender.doubleValue / control.step) * control.step
        : sender.doubleValue;
    sender.doubleValue = std::clamp(stepped, control.minimumValue, control.maximumValue);
    if (const auto error = paperweight::applyTemplateControl(material_, control, sender.doubleValue)) {
        self.statusLabel.stringValue = [NSString stringWithUTF8String:error->c_str()];
        self.statusLabel.textColor = NSColor.systemRedColor;
        return;
    }
    auto* value = self.templateControlValues[static_cast<NSUInteger>(sender.tag)];
    value.stringValue = control.step >= 1.0
        ? [NSString stringWithFormat:@"%.0f", sender.doubleValue]
        : [NSString stringWithFormat:@"%.3g", sender.doubleValue];
    self.normalStrengthSlider.doubleValue = material_.normalStrength;
    [self rebuildLayerList];
    [self refreshLayerInspector];
    [self updateControlLabels];
    [self regeneratePreview];
    [self markDirty];
}

- (void)applyMaterialToControls
{
    if (material_.layers.empty()) {
        material_.layers.push_back(paperweight::makeNoiseLayer());
    }
    selectedLayer_ = std::clamp(
        selectedLayer_,
        static_cast<NSInteger>(0),
        static_cast<NSInteger>(material_.layers.size() - 1));
    previewCoverage_ = material_.physicalSize;
    self.seedField.stringValue = [NSString stringWithFormat:@"%llu", material_.seed];
    self.materialWidthField.stringValue = [NSString
        stringWithFormat:@"%.6g", material_.physicalSize.widthMetres];
    self.materialHeightField.stringValue = [NSString
        stringWithFormat:@"%.6g", material_.physicalSize.heightMetres];
    self.coverageWidthField.stringValue = [NSString
        stringWithFormat:@"%.6g", previewCoverage_.widthMetres];
    self.coverageHeightField.stringValue = [NSString
        stringWithFormat:@"%.6g", previewCoverage_.heightMetres];
    self.frequencySlider.doubleValue = material_.frequency;
    self.octavesSlider.doubleValue = material_.octaves;
    self.lacunaritySlider.doubleValue = material_.lacunarity;
    self.gainSlider.doubleValue = material_.gain;
    self.lowColourWell.color = colourFromRgba8(material_.lowColour);
    self.highColourWell.color = colourFromRgba8(material_.highColour);
    self.normalStrengthSlider.doubleValue = material_.normalStrength;
    self.roughnessLowSlider.doubleValue = material_.roughnessLow;
    self.roughnessHighSlider.doubleValue = material_.roughnessHigh;
    self.displacementSlider.doubleValue = recommendedPreviewDisplacement(material_);
    self.material3DPreviewView.displacementStrength = self.displacementSlider.doubleValue;
    [self rebuildLayerList];
    [self refreshLayerInspector];
    [self updateControlLabels];
    [self regeneratePreview];
}

- (void)setPreviewLoading:(BOOL)loading
{
    self.previewLoadingPanel.hidden = !loading;
    if (loading) {
        [self.previewProgressIndicator startAnimation:nil];
    } else {
        [self.previewProgressIndicator stopAnimation:nil];
    }
}

- (void)startPreviewGenerationForRevision:(std::uint64_t)revision
{
    if (revision != previewRevision_) {
        return;
    }
    pendingPreviewBlock_ = nil;

    if (self.previewModeControl.selectedSegment == 1) {
        [self start3DPreviewGenerationForRevision:revision];
        return;
    }
    if (bakedPresentationSelected_) {
        [self startBakedPreviewGenerationForRevision:revision];
        return;
    }

    const paperweight::GenerationRequest request{
        material_, previewResolution_, previewResolution_, selectedOutput_, std::nullopt,
        previewCoverage_};
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    previewCancellation_ = cancellation;
    __weak AppDelegate* weakSelf = self;
    dispatch_async(previewQueue_, ^{
        auto graphRequest = request;
        std::size_t graphNodeCount = 0;
        auto compilation = paperweight::compileMaterialGraph(request.material);
        if (auto* graph = std::get_if<paperweight::MaterialGraph>(&compilation)) {
            graphNodeCount = graph->nodes.size();
            graphRequest.graph = std::move(*graph);
        }
        auto result = std::make_shared<paperweight::GenerationResult>(
            paperweight::generate(
                graphRequest,
                [cancellation]() {
                    return cancellation->load(std::memory_order_relaxed);
                }));
        dispatch_async(dispatch_get_main_queue(), ^{
            AppDelegate* strongSelf = weakSelf;
            if (strongSelf == nil || revision != strongSelf->previewRevision_ ||
                cancellation->load(std::memory_order_relaxed)) {
                return;
            }

            strongSelf->previewCancellation_.reset();
            [strongSelf setPreviewLoading:NO];
            if (const auto* image = std::get_if<paperweight::Image>(result.get())) {
                [strongSelf.previewView setGeneratedImage:*image];
                strongSelf->generatedImage_ = *image;
                strongSelf.exportMenuItem.enabled = YES;
                strongSelf.statusLabel.stringValue = [NSString
                    stringWithFormat:@"%u × %u %@ — %.6g × %.6g m — %zu-node graph — seamless",
                                     strongSelf->previewResolution_,
                                     strongSelf->previewResolution_,
                                     outputName(strongSelf->selectedOutput_),
                                     strongSelf->previewCoverage_.widthMetres,
                                     strongSelf->previewCoverage_.heightMetres,
                                     graphNodeCount];
                strongSelf.statusLabel.textColor = NSColor.secondaryLabelColor;
                return;
            }

            const auto& error = std::get<paperweight::GenerationError>(*result);
            strongSelf->generatedImage_.reset();
            strongSelf.exportMenuItem.enabled = NO;
            strongSelf.statusLabel.stringValue =
                [NSString stringWithUTF8String:error.message.c_str()];
            strongSelf.statusLabel.textColor = NSColor.systemRedColor;
        });
    });
}

- (void)startBakedPreviewGenerationForRevision:(std::uint64_t)revision
{
    const paperweight::GenerationRequest request{
        material_,
        previewResolution_,
        previewResolution_,
        paperweight::MaterialOutput::colour,
        std::nullopt,
        previewCoverage_,
    };
    const auto settings = [self stylisedLightingSettings];
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    previewCancellation_ = cancellation;
    __weak AppDelegate* weakSelf = self;
    dispatch_async(previewQueue_, ^{
        auto graphRequest = request;
        std::size_t graphNodeCount = 0;
        std::optional<std::string> failure;
        auto compilation = paperweight::compileMaterialGraph(request.material);
        if (auto* graph = std::get_if<paperweight::MaterialGraph>(&compilation)) {
            graphNodeCount = graph->nodes.size();
            graphRequest.graph = std::move(*graph);
        } else {
            failure = std::get<paperweight::GraphError>(compilation).message;
        }

        std::array<std::optional<paperweight::Image>, 3> sources;
        constexpr std::array sourceOutputs{
            paperweight::MaterialOutput::colour,
            paperweight::MaterialOutput::height,
            paperweight::MaterialOutput::normal,
        };
        if (!failure) {
            for (std::size_t index = 0; index < sourceOutputs.size(); ++index) {
                if (cancellation->load(std::memory_order_relaxed)) {
                    break;
                }
                graphRequest.output = sourceOutputs[index];
                auto generated = paperweight::generate(
                    graphRequest,
                    [cancellation]() {
                        return cancellation->load(std::memory_order_relaxed);
                    });
                if (auto* image = std::get_if<paperweight::Image>(&generated)) {
                    sources[index] = std::move(*image);
                } else {
                    failure = std::get<paperweight::GenerationError>(generated).message;
                    break;
                }
            }
        }

        std::shared_ptr<paperweight::StylisedLightingResult> baked;
        if (!failure && std::all_of(sources.begin(), sources.end(), [](const auto& source) {
                return source.has_value();
            })) {
            baked = std::make_shared<paperweight::StylisedLightingResult>(
                paperweight::bakeStylisedLighting(
                    *sources[0], &*sources[1], &*sources[2], settings));
            if (const auto* error = std::get_if<paperweight::StylisedLightingError>(baked.get())) {
                failure = error->message;
            }
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            AppDelegate* strongSelf = weakSelf;
            if (strongSelf == nil || revision != strongSelf->previewRevision_ ||
                cancellation->load(std::memory_order_relaxed)) {
                return;
            }
            strongSelf->previewCancellation_.reset();
            [strongSelf setPreviewLoading:NO];
            const auto* image = baked
                ? std::get_if<paperweight::Image>(baked.get())
                : nullptr;
            if (image != nullptr) {
                [strongSelf.previewView setGeneratedImage:*image];
                strongSelf->generatedImage_ = *image;
                strongSelf.exportMenuItem.enabled = YES;
                strongSelf.statusLabel.stringValue = [NSString stringWithFormat:
                    @"%u × %u Baked Presentation — separate from unlit colour — %zu-node graph — seamless",
                    strongSelf->previewResolution_,
                    strongSelf->previewResolution_,
                    graphNodeCount];
                strongSelf.statusLabel.textColor = NSColor.secondaryLabelColor;
                return;
            }
            strongSelf->generatedImage_.reset();
            strongSelf.exportMenuItem.enabled = NO;
            strongSelf.statusLabel.stringValue = [NSString stringWithUTF8String:
                failure.value_or("baked presentation did not produce an image").c_str()];
            strongSelf.statusLabel.textColor = NSColor.systemRedColor;
        });
    });
}

- (void)start3DPreviewGenerationForRevision:(std::uint64_t)revision
{
    const paperweight::GenerationRequest request{
        material_,
        previewResolution_,
        previewResolution_,
        paperweight::MaterialOutput::colour,
        std::nullopt,
        previewCoverage_,
    };
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    previewCancellation_ = cancellation;
    __weak AppDelegate* weakSelf = self;
    dispatch_async(previewQueue_, ^{
        auto graphRequest = request;
        std::size_t graphNodeCount = 0;
        std::optional<paperweight::GenerationError> failure;
        auto compilation = paperweight::compileMaterialGraph(request.material);
        if (auto* graph = std::get_if<paperweight::MaterialGraph>(&compilation)) {
            graphNodeCount = graph->nodes.size();
            graphRequest.graph = std::move(*graph);
        } else {
            const auto& error = std::get<paperweight::GraphError>(compilation);
            failure = paperweight::GenerationError{
                paperweight::GenerationErrorCode::invalidGraph,
                error.message,
            };
        }

        std::array<std::optional<paperweight::Image>, 4> images;
        if (!failure) {
            for (std::size_t outputIndex = 0; outputIndex < paperweight::materialOutputs.size(); ++outputIndex) {
                if (cancellation->load(std::memory_order_relaxed)) {
                    break;
                }
                graphRequest.output = paperweight::materialOutputs[outputIndex];
                auto result = paperweight::generate(
                    graphRequest,
                    [cancellation]() {
                        return cancellation->load(std::memory_order_relaxed);
                    });
                if (auto* image = std::get_if<paperweight::Image>(&result)) {
                    images[outputIndex].emplace(std::move(*image));
                    continue;
                }
                failure = std::get<paperweight::GenerationError>(std::move(result));
                break;
            }
        }

        std::shared_ptr<PreviewMapSet> maps;
        if (!failure && std::all_of(images.begin(), images.end(), [](const auto& image) {
                return image.has_value();
            })) {
            maps = std::make_shared<PreviewMapSet>(PreviewMapSet{
                std::move(*images[paperweight::materialOutputIndex(paperweight::MaterialOutput::colour)]),
                std::move(*images[paperweight::materialOutputIndex(paperweight::MaterialOutput::height)]),
                std::move(*images[paperweight::materialOutputIndex(paperweight::MaterialOutput::normal)]),
                std::move(*images[paperweight::materialOutputIndex(paperweight::MaterialOutput::roughness)]),
            });
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            AppDelegate* strongSelf = weakSelf;
            if (strongSelf == nil || revision != strongSelf->previewRevision_ ||
                cancellation->load(std::memory_order_relaxed)) {
                return;
            }
            strongSelf->previewCancellation_.reset();
            [strongSelf setPreviewLoading:NO];
            if (maps) {
                [strongSelf.material3DPreviewView setColourImage:maps->colour
                                                     heightImage:maps->height
                                                     normalImage:maps->normal
                                                  roughnessImage:maps->roughness];
                strongSelf->generated3DMaps_ = maps;
                strongSelf.statusLabel.stringValue = [NSString stringWithFormat:
                    @"Four %u × %u maps — %.6g × %.6g m — %zu-node graph — drag to orbit, scroll to zoom",
                    strongSelf->previewResolution_,
                    strongSelf->previewResolution_,
                    strongSelf->previewCoverage_.widthMetres,
                    strongSelf->previewCoverage_.heightMetres,
                    graphNodeCount];
                strongSelf.statusLabel.textColor = NSColor.secondaryLabelColor;
                return;
            }

            strongSelf->generated3DMaps_.reset();
            const std::string message = failure
                ? failure->message
                : "3D preview generation did not produce a complete map set";
            strongSelf.statusLabel.stringValue = [NSString stringWithUTF8String:message.c_str()];
            strongSelf.statusLabel.textColor = NSColor.systemRedColor;
        });
    });
}

- (void)regeneratePreview
{
    ++previewRevision_;
    const auto revision = previewRevision_;
    if (previewCancellation_) {
        previewCancellation_->store(true, std::memory_order_relaxed);
    }
    if (pendingPreviewBlock_ != nil) {
        dispatch_block_cancel(pendingPreviewBlock_);
    }

    generatedImage_.reset();
    generated3DMaps_.reset();
    self.exportMenuItem.enabled = NO;
    if (self.previewModeControl.selectedSegment == 1) {
        [self.material3DPreviewView clearMaterialImages];
        self.statusLabel.stringValue = [NSString stringWithFormat:
            @"Rendering four %u × %u material maps for 3D…",
            previewResolution_, previewResolution_];
        self.previewLoadingLabel.stringValue = @"Rendering 3D material maps…";
    } else if (bakedPresentationSelected_) {
        self.statusLabel.stringValue = @"Rendering colour, height and normal maps for a baked presentation…";
        self.previewLoadingLabel.stringValue = @"Baking stylised lighting…";
    } else {
        self.statusLabel.stringValue = [NSString stringWithFormat:
            @"Rendering %u × %u preview…", previewResolution_, previewResolution_];
        self.previewLoadingLabel.stringValue = @"Rendering preview…";
    }
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
    [self setPreviewLoading:YES];

    __weak AppDelegate* weakSelf = self;
    pendingPreviewBlock_ = dispatch_block_create(static_cast<dispatch_block_flags_t>(0), ^{
        AppDelegate* strongSelf = weakSelf;
        if (strongSelf != nil && revision == strongSelf->previewRevision_) {
            [strongSelf startPreviewGenerationForRevision:revision];
        }
    });
    constexpr std::int64_t coalescingDelayMilliseconds = 40;
    dispatch_after(
        dispatch_time(
            DISPATCH_TIME_NOW,
            coalescingDelayMilliseconds * static_cast<std::int64_t>(NSEC_PER_MSEC)),
        dispatch_get_main_queue(),
        pendingPreviewBlock_);
}

- (void)markDirty
{
    dirty_ = true;
    [self updateWindowTitle];
}

- (void)updateWindowTitle
{
    NSString* name = self.currentFileURL == nil ? @"Untitled.pmat" : self.currentFileURL.lastPathComponent;
    self.window.title = [NSString stringWithFormat:@"%@ — Paperweight", name];
    self.window.documentEdited = dirty_;
    self.window.representedURL = self.currentFileURL;
}

- (void)showErrorWithTitle:(NSString*)title message:(NSString*)message
{
    auto* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = title;
    alert.informativeText = message;
    [alert runModal];
}

- (BOOL)confirmDiscardIfNeeded
{
    if (!dirty_) {
        return YES;
    }

    auto* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = @"Do you want to save this material?";
    alert.informativeText = @"Your changes will be lost if you don’t save them.";
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert addButtonWithTitle:@"Don’t Save"];
    const NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        return [self saveMaterialWithPanelIfNeeded];
    }
    if (response == NSAlertThirdButtonReturn) {
        dirty_ = false;
        [self updateWindowTitle];
        return YES;
    }
    return NO;
}

- (BOOL)saveMaterialWithPanelIfNeeded
{
    if (self.currentFileURL != nil) {
        return [self saveMaterialToURL:self.currentFileURL];
    }

    auto* panel = [NSSavePanel savePanel];
    panel.title = @"Save Paperweight Material";
    panel.nameFieldStringValue = @"Untitled.pmat";
    panel.allowedFileTypes = @[ @"pmat" ];
    panel.allowsOtherFileTypes = NO;
    panel.canCreateDirectories = YES;
    if ([panel runModal] != NSModalResponseOK) {
        return NO;
    }
    return [self saveMaterialToURL:panel.URL];
}

- (BOOL)saveMaterialToURL:(NSURL*)url
{
    const auto result = paperweight::serialisePmat(material_);
    if (const auto* error = std::get_if<paperweight::SerialisationError>(&result)) {
        [self showErrorWithTitle:@"The material could not be saved"
                         message:[NSString stringWithUTF8String:error->message.c_str()]];
        return NO;
    }

    const auto& text = std::get<std::string>(result);
    auto* contents = [[NSString alloc] initWithBytes:text.data()
                                              length:text.size()
                                            encoding:NSUTF8StringEncoding];
    NSError* error = nil;
    if (contents == nil || ![contents writeToURL:url
                                      atomically:YES
                                        encoding:NSUTF8StringEncoding
                                           error:&error]) {
        NSString* message = error == nil ? @"The material could not be encoded as UTF-8."
                                         : error.localizedDescription;
        [self showErrorWithTitle:@"The material could not be saved" message:message];
        return NO;
    }

    self.currentFileURL = url;
    dirty_ = false;
    [self updateWindowTitle];
    [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:url];
    [self.materialLibraryWindowController noteMaterialSavedAtURL:url];
    self.statusLabel.stringValue = @"Material saved";
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
    return YES;
}

- (void)saveMaterial:(id)sender
{
    static_cast<void>(sender);
    [self saveMaterialWithPanelIfNeeded];
}

- (void)saveMaterialAs:(id)sender
{
    static_cast<void>(sender);
    NSURL* previousURL = self.currentFileURL;
    self.currentFileURL = nil;
    if (![self saveMaterialWithPanelIfNeeded]) {
        self.currentFileURL = previousURL;
        [self updateWindowTitle];
    }
}

- (void)openMaterial:(id)sender
{
    static_cast<void>(sender);
    auto* panel = [NSOpenPanel openPanel];
    panel.title = @"Open Paperweight File";
    panel.allowedFileTypes = @[ @"pmat", @"pwlib" ];
    panel.allowsMultipleSelection = NO;
    panel.canChooseDirectories = NO;
    if ([panel runModal] != NSModalResponseOK) {
        return;
    }

    [self openDocumentAtURL:panel.URL];
}

- (BOOL)openDocumentAtURL:(NSURL*)url
{
    NSString* extension = url.pathExtension.lowercaseString;
    if ([extension isEqualToString:@"pmat"]) {
        return [self openMaterialAtURL:url asShowcase:NO];
    }
    if ([extension isEqualToString:@"pwlib"]) {
        return [self openPackedLibraryAtURL:url];
    }
    [self showErrorWithTitle:@"This file type is not supported"
                     message:@"Paperweight can open .pmat materials and .pwlib material packs."];
    return NO;
}

- (BOOL)openPackedLibraryAtURL:(NSURL*)url
{
    __weak AppDelegate* weakSelf = self;
    self.packedLibraryWindowController = [[PackedLibraryWindowController alloc]
        initWithURL:url
        openHandler:^(paperweight::Material material) {
            AppDelegate* strongSelf = weakSelf;
            if (strongSelf == nil || ![strongSelf confirmDiscardIfNeeded]) {
                return;
            }
            strongSelf->material_ = std::move(material);
            strongSelf->selectedLayer_ = 0;
            strongSelf.currentFileURL = nil;
            strongSelf->dirty_ = true;
            [strongSelf setActiveReferenceTemplate:nullptr];
            [strongSelf clearReferenceImage:nil];
            [strongSelf applyMaterialToControls];
            [strongSelf updateWindowTitle];
            strongSelf.statusLabel.stringValue =
                @"Packed material instantiated as a new editable document.";
            strongSelf.statusLabel.textColor = NSColor.secondaryLabelColor;
            [strongSelf.window makeKeyAndOrderFront:nil];
        }];
    [self.packedLibraryWindowController showPackedLibrary];
    [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:url];
    return YES;
}

- (void)openReferenceTemplate:(NSMenuItem*)sender
{
    NSString* identifier = [sender.representedObject isKindOfClass:NSString.class]
        ? static_cast<NSString*>(sender.representedObject)
        : nil;
    const auto* descriptor = identifier == nil
        ? nullptr
        : paperweight::findReferenceMaterialTemplate(identifier.UTF8String);
    if (descriptor == nullptr) {
        [self showErrorWithTitle:@"The template could not be opened"
                         message:@"Its catalogue entry is missing."];
        return;
    }
    NSURL* url = [NSBundle.mainBundle
        URLForResource:[NSString stringWithUTF8String:descriptor->recipeResourceName.data()]
        withExtension:@"pmat"
        subdirectory:@"Showcases"];
    if (url == nil) {
        [self showErrorWithTitle:@"The template could not be opened"
                         message:@"Its bundled recipe is missing."];
        return;
    }
    NSError* readError = nil;
    auto* contents = [NSString stringWithContentsOfURL:url
                                              encoding:NSUTF8StringEncoding
                                                 error:&readError];
    if (contents == nil || contents.UTF8String == nullptr) {
        [self showErrorWithTitle:@"The template could not be opened"
                         message:readError == nil ? @"Its recipe is not valid UTF-8."
                                                  : readError.localizedDescription];
        return;
    }
    const auto parsed = paperweight::parsePmat(contents.UTF8String);
    const auto* authored = std::get_if<paperweight::Material>(&parsed);
    if (authored == nullptr) {
        const auto& diagnostic = std::get<paperweight::ParseDiagnostic>(parsed);
        [self showErrorWithTitle:@"The template could not be opened"
                         message:[NSString stringWithFormat:@"Line %zu: %s",
                                                           diagnostic.line,
                                                           diagnostic.message.c_str()]];
        return;
    }
    if (![self confirmDiscardIfNeeded]) {
        return;
    }

    const std::uint64_t chosenSeed = material_.seed;
    material_ = paperweight::instantiateMaterial(
        paperweight::makeMaterialRecipe(*authored),
        chosenSeed);
    selectedLayer_ = 0;
    self.currentFileURL = nil;
    dirty_ = true;
    [self setActiveReferenceTemplate:descriptor];
    [self clearReferenceImage:nil];
    [self applyMaterialToControls];
    [self updateWindowTitle];
    self.statusLabel.stringValue = [NSString stringWithFormat:
        @"Template instantiated with seed %llu. Reference file: %s",
        chosenSeed,
        descriptor->referenceFileName.data()];
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
}

- (void)chooseReferenceImage:(id)sender
{
    static_cast<void>(sender);
    auto* panel = [NSOpenPanel openPanel];
    panel.title = @"Choose Reference Material Image";
    panel.allowedFileTypes = @[@"png", @"bmp"];
    panel.allowsMultipleSelection = NO;
    panel.canChooseDirectories = NO;
    if ([panel runModal] != NSModalResponseOK) {
        return;
    }
    auto* image = [[NSImage alloc] initWithContentsOfURL:panel.URL];
    if (image == nil) {
        [self showErrorWithTitle:@"The reference could not be displayed"
                         message:@"Choose a readable PNG or BMP image."];
        return;
    }
    self.referenceImageView.materialImage = image;
    [self.referenceImageView setNeedsDisplay:YES];
    self.referenceTitleLabel.stringValue = [NSString stringWithFormat:
        @"Reference — %@", panel.URL.lastPathComponent];
    self.referencePanel.hidden = NO;
}

- (void)clearReferenceImage:(id)sender
{
    static_cast<void>(sender);
    self.referenceImageView.materialImage = nil;
    [self.referenceImageView setNeedsDisplay:YES];
    self.referenceTitleLabel.stringValue = @"Reference";
    self.referencePanel.hidden = YES;
}

- (void)openShowcase:(NSMenuItem*)sender
{
    NSString* name = [sender.representedObject isKindOfClass:NSString.class]
        ? static_cast<NSString*>(sender.representedObject)
        : nil;
    NSURL* url = name == nil
        ? nil
        : [NSBundle.mainBundle URLForResource:name
                                withExtension:@"pmat"
                                 subdirectory:@"Showcases"];
    if (url == nil) {
        [self showErrorWithTitle:@"The showcase could not be opened"
                         message:@"Its bundled material definition is missing."];
        return;
    }
    [self openMaterialAtURL:url asShowcase:YES];
}

- (BOOL)openMaterialAtURL:(NSURL*)url asShowcase:(BOOL)asShowcase
{

    NSError* readError = nil;
    auto* contents = [NSString stringWithContentsOfURL:url
                                              encoding:NSUTF8StringEncoding
                                                 error:&readError];
    if (contents == nil) {
        [self showErrorWithTitle:@"The material could not be opened"
                         message:readError.localizedDescription];
        return NO;
    }
    const char* utf8 = contents.UTF8String;
    if (utf8 == nullptr) {
        [self showErrorWithTitle:@"The material could not be opened"
                         message:@"The file is not valid UTF-8 text."];
        return NO;
    }
    const auto parsed = paperweight::parsePmat(utf8);
    if (const auto* diagnostic = std::get_if<paperweight::ParseDiagnostic>(&parsed)) {
        auto* message = [NSString stringWithFormat:@"Line %zu, column %zu: %s",
                                                   diagnostic->line,
                                                   diagnostic->column,
                                                   diagnostic->message.c_str()];
        [self showErrorWithTitle:@"This is not a valid .pmat file" message:message];
        return NO;
    }
    if (![self confirmDiscardIfNeeded]) {
        return NO;
    }

    material_ = std::get<paperweight::Material>(parsed);
    selectedLayer_ = 0;
    [self setActiveReferenceTemplate:nullptr];
    self.currentFileURL = asShowcase ? nil : url;
    dirty_ = asShowcase;
    [self applyMaterialToControls];
    [self updateWindowTitle];
    if (!asShowcase) {
        [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:url];
    } else {
        self.statusLabel.stringValue = @"Showcase loaded as a new editable material";
        self.statusLabel.textColor = NSColor.secondaryLabelColor;
    }
    [self.window makeKeyAndOrderFront:nil];
    return YES;
}

- (void)exportPng:(id)sender
{
    static_cast<void>(sender);
    if (!generatedImage_) {
        [self showErrorWithTitle:@"There is no texture to export"
                         message:@"Generate a valid preview before exporting."];
        return;
    }

    auto* panel = [NSSavePanel savePanel];
    panel.title = @"Export Generated Texture";
    panel.nameFieldStringValue = [NSString
        stringWithFormat:@"Paperweight-%@-512x512.png",
                         self.currentOutputName.lowercaseString];
    panel.allowedFileTypes = @[ @"png" ];
    panel.allowsOtherFileTypes = NO;
    panel.canCreateDirectories = YES;
    if ([panel runModal] != NSModalResponseOK) {
        return;
    }

    NSData* data = paperweight::macos::makePngData(*generatedImage_);
    NSError* error = nil;
    if (data == nil || ![data writeToURL:panel.URL options:NSDataWritingAtomic error:&error]) {
        NSString* message = error == nil ? @"The preview could not be encoded as PNG."
                                         : error.localizedDescription;
        [self showErrorWithTitle:@"The PNG could not be exported" message:message];
        return;
    }
    self.statusLabel.stringValue =
        [NSString stringWithFormat:@"%@ PNG exported", self.currentOutputName];
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
}

@end

int main(int argc, const char* argv[])
{
    static_cast<void>(argc);
    static_cast<void>(argv);
    @autoreleasepool {
        auto* application = [NSApplication sharedApplication];
        application.activationPolicy = NSApplicationActivationPolicyRegular;
        __attribute__((objc_precise_lifetime)) AppDelegate* delegate =
            [[AppDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }
    return 0;
}
