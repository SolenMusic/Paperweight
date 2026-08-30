#import <AppKit/AppKit.h>

#include "ImageBridge.hpp"

#include <paperweight/generator.hpp>
#include <paperweight/hash.hpp>
#include <paperweight/pmat.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <variant>

@interface MaterialPreviewView : NSView

@property(nonatomic, strong) NSImage* materialImage;
@property(nonatomic) NSInteger repeatCount;

- (void)setGeneratedImage:(const paperweight::Image&)image;

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
@property(nonatomic, strong) NSTextField* seedField;
@property(nonatomic, strong) NSSlider* frequencySlider;
@property(nonatomic, strong) NSTextField* frequencyValue;
@property(nonatomic, strong) NSSlider* octavesSlider;
@property(nonatomic, strong) NSTextField* octavesValue;
@property(nonatomic, strong) NSSlider* lacunaritySlider;
@property(nonatomic, strong) NSTextField* lacunarityValue;
@property(nonatomic, strong) NSSlider* gainSlider;
@property(nonatomic, strong) NSTextField* gainValue;
@property(nonatomic, strong) NSSegmentedControl* tilingControl;
@property(nonatomic, strong) NSTextField* statusLabel;
@property(nonatomic, strong) NSURL* currentFileURL;

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

NSBox* makeSeparator()
{
    auto* separator = [[NSBox alloc] initWithFrame:NSZeroRect];
    separator.boxType = NSBoxSeparator;
    return separator;
}

} // namespace

@implementation AppDelegate {
    paperweight::Material material_;
    std::optional<paperweight::Image> generatedImage_;
    bool dirty_;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    static_cast<void>(notification);
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
    return [self confirmDiscardIfNeeded] ? NSTerminateNow : NSTerminateCancel;
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
    auto* exportItem = [fileMenu addItemWithTitle:@"Export PNG…"
                                          action:@selector(exportPng:)
                                   keyEquivalent:@"e"];
    exportItem.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
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
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1060, 700)
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Untitled.pmat — Paperweight";
    self.window.minSize = NSMakeSize(820, 560);
    self.window.delegate = self;

    auto* content = [[NSView alloc] initWithFrame:self.window.contentView.bounds];
    content.translatesAutoresizingMaskIntoConstraints = NO;
    self.window.contentView = content;

    auto* controlsPanel = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
    controlsPanel.translatesAutoresizingMaskIntoConstraints = NO;
    controlsPanel.material = NSVisualEffectMaterialSidebar;
    controlsPanel.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    controlsPanel.state = NSVisualEffectStateActive;

    self.previewView = [[MaterialPreviewView alloc] initWithFrame:NSZeroRect];
    self.previewView.translatesAutoresizingMaskIntoConstraints = NO;

    [content addSubview:controlsPanel];
    [content addSubview:self.previewView];
    [NSLayoutConstraint activateConstraints:@[
        [controlsPanel.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [controlsPanel.topAnchor constraintEqualToAnchor:content.topAnchor],
        [controlsPanel.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
        [controlsPanel.widthAnchor constraintEqualToConstant:310.0],
        [self.previewView.leadingAnchor constraintEqualToAnchor:controlsPanel.trailingAnchor constant:20.0],
        [self.previewView.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-20.0],
        [self.previewView.topAnchor constraintEqualToAnchor:content.topAnchor constant:20.0],
        [self.previewView.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-20.0],
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
        makeSeparator(),
        previewLabel,
        self.tilingControl,
        resetButton,
        self.statusLabel,
    ]];
    controlStack.translatesAutoresizingMaskIntoConstraints = NO;
    controlStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    controlStack.alignment = NSLayoutAttributeLeading;
    controlStack.spacing = 13.0;
    [controlsPanel addSubview:controlStack];
    [NSLayoutConstraint activateConstraints:@[
        [controlStack.leadingAnchor constraintEqualToAnchor:controlsPanel.leadingAnchor constant:20.0],
        [controlStack.trailingAnchor constraintEqualToAnchor:controlsPanel.trailingAnchor constant:-20.0],
        [controlStack.topAnchor constraintEqualToAnchor:controlsPanel.topAnchor constant:24.0],
        [self.tilingControl.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
        [self.statusLabel.widthAnchor constraintEqualToAnchor:controlStack.widthAnchor],
    ]];
}

- (void)parameterChanged:(id)sender
{
    if (sender == self.seedField) {
        const char* text = self.seedField.stringValue.UTF8String;
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(text, &end, 10);
        if (text == end || (end != nullptr && *end != '\0')) {
            self.statusLabel.stringValue = @"Seed must be a non-negative integer.";
            return;
        }
        material_.seed = static_cast<std::uint64_t>(parsed);
    }

    material_.frequency = static_cast<std::uint32_t>(std::llround(self.frequencySlider.doubleValue));
    material_.octaves = static_cast<std::uint32_t>(std::llround(self.octavesSlider.doubleValue));
    material_.lacunarity = static_cast<std::uint32_t>(std::llround(self.lacunaritySlider.doubleValue));
    material_.gain = self.gainSlider.doubleValue;
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
    self.seedField.stringValue = [NSString stringWithFormat:@"%llu", material_.seed];
    self.frequencySlider.doubleValue = material_.frequency;
    self.octavesSlider.doubleValue = material_.octaves;
    self.lacunaritySlider.doubleValue = material_.lacunarity;
    self.gainSlider.doubleValue = material_.gain;
    [self updateControlLabels];
    [self regeneratePreview];
    [self markDirty];
}

- (void)tilingChanged:(id)sender
{
    static_cast<void>(sender);
    self.previewView.repeatCount = self.tilingControl.selectedSegment == 1 ? 3 : 1;
}

- (void)updateControlLabels
{
    self.frequencyValue.stringValue = [NSString stringWithFormat:@"%u", material_.frequency];
    self.octavesValue.stringValue = [NSString stringWithFormat:@"%u", material_.octaves];
    self.lacunarityValue.stringValue = [NSString stringWithFormat:@"%u", material_.lacunarity];
    self.gainValue.stringValue = [NSString stringWithFormat:@"%.2f", material_.gain];
}

- (void)applyMaterialToControls
{
    self.seedField.stringValue = [NSString stringWithFormat:@"%llu", material_.seed];
    self.frequencySlider.doubleValue = material_.frequency;
    self.octavesSlider.doubleValue = material_.octaves;
    self.lacunaritySlider.doubleValue = material_.lacunarity;
    self.gainSlider.doubleValue = material_.gain;
    [self updateControlLabels];
    [self regeneratePreview];
}

- (void)regeneratePreview
{
    const paperweight::GenerationRequest request{material_, 512, 512};
    auto result = paperweight::generate(request);
    if (const auto* image = std::get_if<paperweight::Image>(&result)) {
        [self.previewView setGeneratedImage:*image];
        generatedImage_ = *image;
        self.statusLabel.stringValue = @"512 × 512 RGBA8 — mathematically seamless";
        self.statusLabel.textColor = NSColor.secondaryLabelColor;
        return;
    }

    const auto& error = std::get<paperweight::GenerationError>(result);
    generatedImage_.reset();
    self.statusLabel.stringValue = [NSString stringWithUTF8String:error.message.c_str()];
    self.statusLabel.textColor = NSColor.systemRedColor;
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
    panel.nameFieldStringValue = @"Paperweight-512x512.png";
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
    self.statusLabel.stringValue = @"PNG exported";
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
        auto* delegate = [[AppDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }
    return 0;
}
