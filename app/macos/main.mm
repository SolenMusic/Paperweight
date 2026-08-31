#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>

#include "ImageBridge.hpp"

#include <paperweight/generator.hpp>
#include <paperweight/hash.hpp>
#include <paperweight/layer.hpp>
#include <paperweight/pmat.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
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
@property(nonatomic, strong) MaterialPreviewView* previewView;
@property(nonatomic, strong) NSVisualEffectView* previewLoadingPanel;
@property(nonatomic, strong) NSProgressIndicator* previewProgressIndicator;
@property(nonatomic, strong) NSTextField* previewLoadingLabel;
@property(nonatomic, strong) NSMenuItem* exportMenuItem;
@property(nonatomic, strong) NSTextField* seedField;
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
@property(nonatomic, strong) NSSegmentedControl* tilingControl;
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

NSBox* makeSeparator()
{
    auto* separator = [[NSBox alloc] initWithFrame:NSZeroRect];
    separator.boxType = NSBoxSeparator;
    return separator;
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
    default:
        return @"Unknown";
    }
}

paperweight::MaterialLayer* layerAt(paperweight::Material& material, NSInteger index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= material.layers.size()) {
        return nullptr;
    }
    return &material.layers[static_cast<std::size_t>(index)];
}

} // namespace

@implementation AppDelegate {
    paperweight::Material material_;
    paperweight::MaterialOutput selectedOutput_;
    std::optional<paperweight::Image> generatedImage_;
    dispatch_queue_t previewQueue_;
    dispatch_block_t pendingPreviewBlock_;
    std::shared_ptr<std::atomic_bool> previewCancellation_;
    std::uint64_t previewRevision_;
    bool dirty_;
    NSInteger selectedLayer_;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    static_cast<void>(notification);
    selectedOutput_ = paperweight::MaterialOutput::colour;
    material_.layers.push_back(paperweight::makeNoiseLayer());
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
    return NSTerminateNow;
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    static_cast<void>(sender);
    return [self confirmDiscardIfNeeded];
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
    [fileMenu addItemWithTitle:@"Open…" action:@selector(openMaterial:) keyEquivalent:@"o"];
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

    NSApp.mainMenu = mainMenu;
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

    self.previewView = [[MaterialPreviewView alloc] initWithFrame:NSZeroRect];
    self.previewView.translatesAutoresizingMaskIntoConstraints = NO;

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
    [self.previewView addSubview:self.previewLoadingPanel];

    [content addSubview:controlsPanel];
    [content addSubview:layersPanel];
    [content addSubview:self.previewView];
    [NSLayoutConstraint activateConstraints:@[
        [controlsPanel.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [controlsPanel.topAnchor constraintEqualToAnchor:content.topAnchor],
        [controlsPanel.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
        [controlsPanel.widthAnchor constraintEqualToConstant:310.0],
        [layersPanel.leadingAnchor constraintEqualToAnchor:controlsPanel.trailingAnchor],
        [layersPanel.topAnchor constraintEqualToAnchor:content.topAnchor],
        [layersPanel.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
        [layersPanel.widthAnchor constraintEqualToConstant:340.0],
        [self.previewView.leadingAnchor constraintEqualToAnchor:layersPanel.trailingAnchor constant:20.0],
        [self.previewView.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-20.0],
        [self.previewView.topAnchor constraintEqualToAnchor:content.topAnchor constant:20.0],
        [self.previewView.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-20.0],
        [self.previewLoadingPanel.centerXAnchor constraintEqualToAnchor:self.previewView.centerXAnchor],
        [self.previewLoadingPanel.centerYAnchor constraintEqualToAnchor:self.previewView.centerYAnchor],
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

    auto* previewLabel = makeLabel(@"Preview tiling");
    self.tilingControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.tilingControl.segmentCount = 2;
    [self.tilingControl setLabel:@"1 × 1" forSegment:0];
    [self.tilingControl setLabel:@"3 × 3" forSegment:1];
    self.tilingControl.selectedSegment = 0;
    self.tilingControl.target = self;
    self.tilingControl.action = @selector(tilingChanged:);

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

    auto* controlStack = [NSStackView stackViewWithViews:@[
        title,
        subtitle,
        makeSeparator(),
        seedRow,
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
        outputLabel,
        self.outputControl,
        previewLabel,
        self.tilingControl,
        resetButton,
        self.statusLabel,
    ]];
    controlStack.translatesAutoresizingMaskIntoConstraints = NO;
    controlStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    controlStack.alignment = NSLayoutAttributeLeading;
    controlStack.spacing = 10.0;
    [controlsPanel addSubview:controlStack];
    [NSLayoutConstraint activateConstraints:@[
        [controlStack.leadingAnchor constraintEqualToAnchor:controlsPanel.leadingAnchor constant:20.0],
        [controlStack.trailingAnchor constraintEqualToAnchor:controlsPanel.trailingAnchor constant:-20.0],
        [controlStack.topAnchor constraintEqualToAnchor:controlsPanel.topAnchor constant:24.0],
        [self.tilingControl.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
        [self.outputControl.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
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

    auto* patternSeedLabel = makeLabel(@"Cell seed");
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
        self.patternCountXRow,
        self.patternCountYRow,
        self.patternValueOneRow,
        self.patternValueTwoRow,
        self.patternValueThreeRow,
        self.patternDirectionRow,
        self.patternSeedRow,
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
        [self.patternCountXRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternCountYRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternValueOneRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternValueTwoRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
        [self.patternValueThreeRow.widthAnchor constraintEqualToAnchor:self.layerSettingsGroup.widthAnchor],
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

    for (std::size_t index = 0; index < material_.layers.size(); ++index) {
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

- (void)refreshLayerInspector
{
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
        self.patternDirectionRow.hidden = YES;
        self.patternSeedRow.hidden = YES;
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
    self.patternDirectionRow.hidden = YES;
    self.patternSeedRow.hidden = YES;

    if (noise != nullptr) {
        self.noiseSeedOffsetField.stringValue = [NSString
            stringWithFormat:@"%llu", noise->seedOffset];
    }
    if (solid != nullptr) {
        self.solidColourWell.color = colourFromRgba8(solid->colour);
    }
    if (levels != nullptr) {
        self.levelsLowSlider.doubleValue = levels->inputLow;
        self.levelsHighSlider.doubleValue = levels->inputHigh;
        self.levelsGammaSlider.doubleValue = levels->gamma;
        self.levelsLowValue.stringValue = [NSString stringWithFormat:@"%.2f", levels->inputLow];
        self.levelsHighValue.stringValue = [NSString stringWithFormat:@"%.2f", levels->inputHigh];
        self.levelsGammaValue.stringValue = [NSString stringWithFormat:@"%.2f", levels->gamma];
    }
    if (threshold != nullptr) {
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
        showCount(self.patternCountXRow, self.patternCountXLabel,
                  self.patternCountXSlider, self.patternCountXValue, @"Columns", brick->columns);
        showCount(self.patternCountYRow, self.patternCountYLabel,
                  self.patternCountYSlider, self.patternCountYValue, @"Rows", brick->rows);
        showValue(self.patternValueOneRow, self.patternValueOneLabel,
                  self.patternValueOneSlider, self.patternValueOneValue,
                  @"Mortar", 0.0, 0.95, brick->mortar);
        showValue(self.patternValueTwoRow, self.patternValueTwoLabel,
                  self.patternValueTwoSlider, self.patternValueTwoValue,
                  @"Stagger", 0.0, 1.0, brick->stagger);
        showValue(self.patternValueThreeRow, self.patternValueThreeLabel,
                  self.patternValueThreeSlider, self.patternValueThreeValue,
                  @"Softness", 0.0, 0.25, brick->softness);
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

    [self refreshLayerInspector];
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

    [self refreshLayerInspector];
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
            self.statusLabel.stringValue = @"A structural seed offset must be a non-negative integer.";
            self.statusLabel.textColor = NSColor.systemRedColor;
            return;
        }
        parsedSeed = parsed;
    }

    const auto countX = static_cast<std::uint32_t>(
        std::llround(self.patternCountXSlider.doubleValue));
    const auto countY = static_cast<std::uint32_t>(
        std::llround(self.patternCountYSlider.doubleValue));
    if (auto* brick = std::get_if<paperweight::BrickGridOperation>(&layer->operation)) {
        brick->columns = countX;
        brick->rows = countY;
        brick->mortar = self.patternValueOneSlider.doubleValue;
        brick->stagger = self.patternValueTwoSlider.doubleValue;
        brick->softness = self.patternValueThreeSlider.doubleValue;
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
    } else {
        return;
    }

    [self refreshLayerInspector];
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

    [self refreshLayerInspector];
    [self regeneratePreview];
    [self markDirty];
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
    material_ = paperweight::Material{};
    material_.layers.push_back(paperweight::makeNoiseLayer());
    selectedLayer_ = 0;
    self.seedField.stringValue = [NSString stringWithFormat:@"%llu", material_.seed];
    self.frequencySlider.doubleValue = material_.frequency;
    self.octavesSlider.doubleValue = material_.octaves;
    self.lacunaritySlider.doubleValue = material_.lacunarity;
    self.gainSlider.doubleValue = material_.gain;
    self.lowColourWell.color = colourFromRgba8(material_.lowColour);
    self.highColourWell.color = colourFromRgba8(material_.highColour);
    self.normalStrengthSlider.doubleValue = material_.normalStrength;
    self.roughnessLowSlider.doubleValue = material_.roughnessLow;
    self.roughnessHighSlider.doubleValue = material_.roughnessHigh;
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
    [self regeneratePreview];
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

- (void)applyMaterialToControls
{
    if (material_.layers.empty()) {
        material_.layers.push_back(paperweight::makeNoiseLayer());
    }
    selectedLayer_ = std::clamp(
        selectedLayer_,
        static_cast<NSInteger>(0),
        static_cast<NSInteger>(material_.layers.size() - 1));
    self.seedField.stringValue = [NSString stringWithFormat:@"%llu", material_.seed];
    self.frequencySlider.doubleValue = material_.frequency;
    self.octavesSlider.doubleValue = material_.octaves;
    self.lacunaritySlider.doubleValue = material_.lacunarity;
    self.gainSlider.doubleValue = material_.gain;
    self.lowColourWell.color = colourFromRgba8(material_.lowColour);
    self.highColourWell.color = colourFromRgba8(material_.highColour);
    self.normalStrengthSlider.doubleValue = material_.normalStrength;
    self.roughnessLowSlider.doubleValue = material_.roughnessLow;
    self.roughnessHighSlider.doubleValue = material_.roughnessHigh;
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

    const paperweight::GenerationRequest request{material_, 512, 512, selectedOutput_};
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    previewCancellation_ = cancellation;
    __weak AppDelegate* weakSelf = self;
    dispatch_async(previewQueue_, ^{
        auto result = std::make_shared<paperweight::GenerationResult>(
            paperweight::generate(
                request,
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
                    stringWithFormat:@"512 × 512 %@ RGBA8 — mathematically seamless",
                                     outputName(strongSelf->selectedOutput_)];
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
    self.exportMenuItem.enabled = NO;
    self.statusLabel.stringValue = @"Rendering 512 × 512 preview…";
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
    panel.title = @"Open Paperweight Material";
    panel.allowedFileTypes = @[ @"pmat" ];
    panel.allowsMultipleSelection = NO;
    panel.canChooseDirectories = NO;
    if ([panel runModal] != NSModalResponseOK) {
        return;
    }

    NSError* readError = nil;
    auto* contents = [NSString stringWithContentsOfURL:panel.URL
                                              encoding:NSUTF8StringEncoding
                                                 error:&readError];
    if (contents == nil) {
        [self showErrorWithTitle:@"The material could not be opened"
                         message:readError.localizedDescription];
        return;
    }
    const char* utf8 = contents.UTF8String;
    if (utf8 == nullptr) {
        [self showErrorWithTitle:@"The material could not be opened"
                         message:@"The file is not valid UTF-8 text."];
        return;
    }
    const auto parsed = paperweight::parsePmat(utf8);
    if (const auto* diagnostic = std::get_if<paperweight::ParseDiagnostic>(&parsed)) {
        auto* message = [NSString stringWithFormat:@"Line %zu, column %zu: %s",
                                                   diagnostic->line,
                                                   diagnostic->column,
                                                   diagnostic->message.c_str()];
        [self showErrorWithTitle:@"This is not a valid .pmat file" message:message];
        return;
    }
    if (![self confirmDiscardIfNeeded]) {
        return;
    }

    material_ = std::get<paperweight::Material>(parsed);
    self.currentFileURL = panel.URL;
    dirty_ = false;
    [self applyMaterialToControls];
    [self updateWindowTitle];
    [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:panel.URL];
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
                         outputName(selectedOutput_).lowercaseString];
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
        [NSString stringWithFormat:@"%@ PNG exported", outputName(selectedOutput_)];
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
