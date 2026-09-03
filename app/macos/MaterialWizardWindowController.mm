#import "MaterialWizardWindowController.hpp"

#include "ImageBridge.hpp"
#include "Material3DPreviewView.hpp"

#include <paperweight/generator.hpp>
#include <paperweight/graph.hpp>
#include <paperweight/material_template.hpp>
#include <paperweight/material_wizard.hpp>
#include <paperweight/pmat.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

NSTextField* wizardLabel(NSString* text)
{
    auto* label = [NSTextField labelWithString:text];
    label.lineBreakMode = NSLineBreakByWordWrapping;
    label.maximumNumberOfLines = 0;
    return label;
}

NSTextField* wizardHeading(NSString* text)
{
    auto* label = wizardLabel(text);
    label.font = [NSFont systemFontOfSize:21.0 weight:NSFontWeightSemibold];
    return label;
}

NSString* stringFromUtf8(std::string_view value)
{
    const std::string owned(value);
    auto* text = [NSString stringWithUTF8String:owned.c_str()];
    return text != nil ? text : @"";
}

NSImage* imageFromPaperweight(const paperweight::Image& source, NSSize displaySize)
{
    auto* representation = paperweight::macos::makeBitmapRepresentation(source);
    if (representation == nil) {
        return nil;
    }
    auto* image = [[NSImage alloc] initWithSize:displaySize];
    [image addRepresentation:representation];
    return image;
}

std::string slugForWizardName(NSString* name)
{
    const char* value = name.lowercaseString.UTF8String;
    std::string result;
    bool separator = false;
    if (value != nullptr) {
        for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(value);
             *cursor != 0; ++cursor) {
            if ((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= '0' && *cursor <= '9')) {
                if (separator && !result.empty()) {
                    result.push_back('-');
                }
                result.push_back(static_cast<char>(*cursor));
                separator = false;
            } else {
                separator = true;
            }
        }
    }
    return result.empty() ? "material" : result;
}

NSString* controlSectionName(paperweight::WizardControlSection section)
{
    switch (section) {
    case paperweight::WizardControlSection::construction:
        return @"Construction";
    case paperweight::WizardControlSection::surface:
        return @"Surface";
    case paperweight::WizardControlSection::wear:
        return @"Wear and character";
    }
    return @"Controls";
}

} // namespace

@interface MaterialWizardWindowController () <NSWindowDelegate, NSTextFieldDelegate>
@property(nonatomic, copy) PWUseWizardMaterialHandler useMaterialHandler;
@property(nonatomic, copy) PWWizardSavedMaterialHandler savedMaterialHandler;
@property(nonatomic, strong) NSSegmentedControl* stepControl;
@property(nonatomic, strong) NSArray<NSView*>* pages;
@property(nonatomic, strong) NSButton* backButton;
@property(nonatomic, strong) NSButton* nextButton;
@property(nonatomic, strong) NSButton* editButton;
@property(nonatomic, strong) NSButton* libraryButton;
@property(nonatomic, strong) NSPopUpButton* familyPopup;
@property(nonatomic, strong) NSTextField* familyDescription;
@property(nonatomic, strong) NSPopUpButton* templatePopup;
@property(nonatomic, strong) NSTextField* templateDescription;
@property(nonatomic, strong) NSPopUpButton* scalePresetPopup;
@property(nonatomic, strong) NSTextField* widthField;
@property(nonatomic, strong) NSTextField* heightField;
@property(nonatomic, strong) NSTextField* reliefDepthField;
@property(nonatomic, strong) NSTextField* normalMultiplierField;
@property(nonatomic, strong) NSTextField* scaleSummary;
@property(nonatomic, strong) NSButton* scaleLockCheckbox;
@property(nonatomic, strong) NSTextField* seedField;
@property(nonatomic, strong) NSButton* seedLockCheckbox;
@property(nonatomic, strong) NSColorWell* lowColourWell;
@property(nonatomic, strong) NSColorWell* highColourWell;
@property(nonatomic, strong) NSButton* colourLockCheckbox;
@property(nonatomic, strong) NSStackView* friendlyControlsStack;
@property(nonatomic, strong) NSMutableArray<NSSlider*>* controlSliders;
@property(nonatomic, strong) NSMutableArray<NSTextField*>* controlValues;
@property(nonatomic, strong) NSMutableArray<NSButton*>* controlLocks;
@property(nonatomic, strong) NSArray<NSButton*>* alternativeButtons;
@property(nonatomic, strong) NSTextField* alternativeSummary;
@property(nonatomic, strong) NSSegmentedControl* previewModeControl;
@property(nonatomic, strong) NSImageView* previewImageView;
@property(nonatomic, strong) Material3DPreviewView* preview3DView;
@property(nonatomic, strong) NSProgressIndicator* previewProgress;
@property(nonatomic, strong) NSTextField* previewStatus;
@end

@implementation MaterialWizardWindowController {
    NSUInteger previewResolution_;
    NSInteger currentStep_;
    paperweight::WizardMaterialFamily selectedFamily_;
    std::vector<const paperweight::ReferenceMaterialTemplate*> familyTemplates_;
    const paperweight::ReferenceMaterialTemplate* activeDescriptor_;
    std::optional<paperweight::MaterialWizardSession> session_;
    std::vector<paperweight::MaterialWizardAlternative> alternatives_;
    NSInteger selectedAlternative_;
    std::optional<paperweight::Material> selectedMaterial_;
    dispatch_queue_t previewQueue_;
    dispatch_queue_t alternativeQueue_;
    std::uint64_t previewRevision_;
    std::uint64_t alternativeRevision_;
    std::shared_ptr<std::atomic_bool> previewCancellation_;
    std::shared_ptr<std::atomic_bool> alternativeCancellation_;
}

- (instancetype)initWithPreviewResolution:(NSUInteger)previewResolution
                        useMaterialHandler:(PWUseWizardMaterialHandler)useMaterialHandler
                      savedMaterialHandler:(PWWizardSavedMaterialHandler)savedMaterialHandler
{
    const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    auto* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1180, 760)
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    self = [super initWithWindow:window];
    if (self) {
        previewResolution_ = std::clamp<NSUInteger>(previewResolution, 64, 1024);
        _useMaterialHandler = [useMaterialHandler copy];
        _savedMaterialHandler = [savedMaterialHandler copy];
        _controlSliders = [NSMutableArray array];
        _controlValues = [NSMutableArray array];
        _controlLocks = [NSMutableArray array];
        currentStep_ = 0;
        selectedFamily_ = paperweight::WizardMaterialFamily::masonry;
        activeDescriptor_ = nullptr;
        selectedAlternative_ = -1;
        previewQueue_ = dispatch_queue_create(
            "org.solen-music.paperweight.wizard-preview", DISPATCH_QUEUE_SERIAL);
        alternativeQueue_ = dispatch_queue_create(
            "org.solen-music.paperweight.wizard-alternatives", DISPATCH_QUEUE_SERIAL);
        window.title = @"New Material — Paperweight";
        window.minSize = NSMakeSize(980, 660);
        window.delegate = self;
        [self buildInterface];
        [self familyChanged:nil];
        [self showStep:0];
    }
    return self;
}

- (void)showMaterialWizard
{
    [self showWindow:nil];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
}

- (void)windowWillClose:(NSNotification*)notification
{
    static_cast<void>(notification);
    if (previewCancellation_) previewCancellation_->store(true, std::memory_order_relaxed);
    if (alternativeCancellation_) alternativeCancellation_->store(true, std::memory_order_relaxed);
}

- (NSStackView*)pageWithViews:(NSArray<NSView*>*)views
{
    auto* page = [NSStackView stackViewWithViews:views];
    page.translatesAutoresizingMaskIntoConstraints = NO;
    page.orientation = NSUserInterfaceLayoutOrientationVertical;
    page.alignment = NSLayoutAttributeLeading;
    page.spacing = 12.0;
    return page;
}

- (void)buildInterface
{
    auto* content = [[NSView alloc] initWithFrame:NSZeroRect];
    self.window.contentView = content;

    auto* leftPanel = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
    leftPanel.material = NSVisualEffectMaterialSidebar;
    leftPanel.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    leftPanel.state = NSVisualEffectStateActive;
    leftPanel.translatesAutoresizingMaskIntoConstraints = NO;
    auto* rightPanel = [[NSView alloc] initWithFrame:NSZeroRect];
    rightPanel.translatesAutoresizingMaskIntoConstraints = NO;
    [content addSubview:leftPanel];
    [content addSubview:rightPanel];
    [NSLayoutConstraint activateConstraints:@[
        [leftPanel.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [leftPanel.topAnchor constraintEqualToAnchor:content.topAnchor],
        [leftPanel.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
        [leftPanel.widthAnchor constraintEqualToConstant:430.0],
        [rightPanel.leadingAnchor constraintEqualToAnchor:leftPanel.trailingAnchor],
        [rightPanel.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
        [rightPanel.topAnchor constraintEqualToAnchor:content.topAnchor],
        [rightPanel.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
    ]];

    self.stepControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.stepControl.segmentCount = 4;
    for (NSInteger index = 0; index < 4; ++index) {
        [self.stepControl setLabel:(@[@"Family", @"Scale", @"Design", @"Choose"])[
            static_cast<NSUInteger>(index)]
                         forSegment:index];
    }
    self.stepControl.selectedSegment = 0;
    self.stepControl.target = self;
    self.stepControl.action = @selector(stepSelected:);
    self.stepControl.accessibilityLabel = @"Material wizard step";

    [self buildFamilyPage];
    [self buildScalePage];
    [self buildDesignPage];
    [self buildAlternativesPage];

    auto* pageContainer = [[NSView alloc] initWithFrame:NSZeroRect];
    pageContainer.translatesAutoresizingMaskIntoConstraints = NO;
    for (NSView* page in self.pages) {
        [pageContainer addSubview:page];
        [NSLayoutConstraint activateConstraints:@[
            [page.leadingAnchor constraintEqualToAnchor:pageContainer.leadingAnchor],
            [page.trailingAnchor constraintEqualToAnchor:pageContainer.trailingAnchor],
            [page.topAnchor constraintEqualToAnchor:pageContainer.topAnchor],
        ]];
    }

    self.backButton = [NSButton buttonWithTitle:@"Back" target:self action:@selector(goBack:)];
    self.nextButton = [NSButton buttonWithTitle:@"Next" target:self action:@selector(goNext:)];
    self.nextButton.keyEquivalent = @"\r";
    self.editButton = [NSButton buttonWithTitle:@"Edit Material"
                                         target:self action:@selector(useInEditor:)];
    self.editButton.bezelStyle = NSBezelStyleTexturedRounded;
    self.libraryButton = [NSButton buttonWithTitle:@"Save to Library…"
                                            target:self action:@selector(saveToLibrary:)];
    auto* navigation = [NSStackView stackViewWithViews:@[
        self.backButton, self.nextButton, self.editButton, self.libraryButton,
    ]];
    navigation.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    navigation.spacing = 8.0;

    for (NSView* view in @[self.stepControl, pageContainer, navigation]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [leftPanel addSubview:view];
    }
    [NSLayoutConstraint activateConstraints:@[
        [self.stepControl.leadingAnchor constraintEqualToAnchor:leftPanel.leadingAnchor constant:20.0],
        [self.stepControl.trailingAnchor constraintEqualToAnchor:leftPanel.trailingAnchor constant:-20.0],
        [self.stepControl.topAnchor constraintEqualToAnchor:leftPanel.topAnchor constant:20.0],
        [pageContainer.leadingAnchor constraintEqualToAnchor:self.stepControl.leadingAnchor],
        [pageContainer.trailingAnchor constraintEqualToAnchor:self.stepControl.trailingAnchor],
        [pageContainer.topAnchor constraintEqualToAnchor:self.stepControl.bottomAnchor constant:22.0],
        [pageContainer.bottomAnchor constraintEqualToAnchor:navigation.topAnchor constant:-16.0],
        [navigation.leadingAnchor constraintEqualToAnchor:self.stepControl.leadingAnchor],
        [navigation.bottomAnchor constraintEqualToAnchor:leftPanel.bottomAnchor constant:-18.0],
    ]];

    [self buildPreviewIn:rightPanel];
}

- (void)buildFamilyPage
{
    self.familyPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    for (const auto& family : paperweight::wizardMaterialFamilies) {
        [self.familyPopup addItemWithTitle:stringFromUtf8(family.displayName)];
    }
    self.familyPopup.target = self;
    self.familyPopup.action = @selector(familyChanged:);
    self.familyPopup.accessibilityLabel = @"Material family";
    self.familyDescription = wizardLabel(@"");
    self.familyDescription.textColor = NSColor.secondaryLabelColor;
    self.templatePopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    self.templatePopup.target = self;
    self.templatePopup.action = @selector(templateChanged:);
    self.templatePopup.accessibilityLabel = @"Starting material template";
    self.templateDescription = wizardLabel(@"");
    self.templateDescription.textColor = NSColor.secondaryLabelColor;
    auto* note = wizardLabel(
        @"Choose what you want to make, then pick a useful starting character. Everything remains editable later.");
    note.textColor = NSColor.secondaryLabelColor;
    auto* page = [self pageWithViews:@[
        wizardHeading(@"What are you making?"), note,
        wizardLabel(@"Material family"), self.familyPopup, self.familyDescription,
        wizardLabel(@"Starting point"), self.templatePopup, self.templateDescription,
    ]];
    self.pages = @[page];
}

- (void)buildScalePage
{
    self.scalePresetPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.scalePresetPopup addItemsWithTitles:@[
        @"Template's natural scale", @"Small detail — 0.5 × 0.5 m",
        @"One metre square — 1 × 1 m", @"Wall section — 2 × 1 m",
        @"Large surface — 4 × 3 m", @"Custom",
    ]];
    self.scalePresetPopup.target = self;
    self.scalePresetPopup.action = @selector(scalePresetChanged:);
    self.scalePresetPopup.accessibilityLabel = @"Physical repeat size preset";
    self.widthField = [NSTextField textFieldWithString:@"1"];
    self.heightField = [NSTextField textFieldWithString:@"1"];
    self.widthField.accessibilityLabel = @"Physical repeat width in metres";
    self.heightField.accessibilityLabel = @"Physical repeat height in metres";
    self.widthField.delegate = self;
    self.heightField.delegate = self;
    self.reliefDepthField = [NSTextField textFieldWithString:@"3"];
    self.normalMultiplierField = [NSTextField textFieldWithString:@"1"];
    self.reliefDepthField.delegate = self;
    self.normalMultiplierField.delegate = self;
    self.reliefDepthField.accessibilityLabel = @"Physical relief depth in millimetres";
    self.normalMultiplierField.accessibilityLabel = @"Artistic normal strength multiplier";
    auto* widthRow = [NSStackView stackViewWithViews:@[
        wizardLabel(@"Width"), self.widthField, wizardLabel(@"metres"),
    ]];
    widthRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    widthRow.alignment = NSLayoutAttributeCenterY;
    widthRow.spacing = 8.0;
    auto* heightRow = [NSStackView stackViewWithViews:@[
        wizardLabel(@"Height"), self.heightField, wizardLabel(@"metres"),
    ]];
    heightRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    heightRow.alignment = NSLayoutAttributeCenterY;
    heightRow.spacing = 8.0;
    auto* reliefRow = [NSStackView stackViewWithViews:@[
        wizardLabel(@"Relief depth"), self.reliefDepthField, wizardLabel(@"millimetres"),
    ]];
    reliefRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    reliefRow.alignment = NSLayoutAttributeCenterY;
    reliefRow.spacing = 8.0;
    auto* normalRow = [NSStackView stackViewWithViews:@[
        wizardLabel(@"Normal multiplier"), self.normalMultiplierField,
    ]];
    normalRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    normalRow.alignment = NSLayoutAttributeCenterY;
    normalRow.spacing = 8.0;
    self.scaleLockCheckbox = [NSButton checkboxWithTitle:@"Keep this physical size in alternatives"
                                                   target:self action:@selector(lockChanged:)];
    self.scaleLockCheckbox.state = NSControlStateValueOn;
    self.scaleSummary = wizardLabel(@"");
    self.scaleSummary.textColor = NSColor.secondaryLabelColor;
    auto* note = wizardLabel(
        @"The repeat size and relief depth describe real dimensions. Normals remain consistent when texture resolution changes; the optional multiplier is purely artistic.");
    note.textColor = NSColor.secondaryLabelColor;
    auto* page = [self pageWithViews:@[
        wizardHeading(@"How large is one repeat?"), note,
        self.scalePresetPopup, widthRow, heightRow, reliefRow, normalRow,
        self.scaleLockCheckbox, self.scaleSummary,
    ]];
    self.pages = [self.pages arrayByAddingObject:page];
}

- (void)buildDesignPage
{
    self.seedField = [NSTextField textFieldWithString:@"18431"];
    self.seedField.delegate = self;
    self.seedField.accessibilityLabel = @"Starting variation seed";
    self.seedLockCheckbox = [NSButton checkboxWithTitle:@"Keep seed"
                                                  target:self action:@selector(lockChanged:)];
    auto* seedRow = [NSStackView stackViewWithViews:@[
        wizardLabel(@"Starting variation"), self.seedField, self.seedLockCheckbox,
    ]];
    seedRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    seedRow.alignment = NSLayoutAttributeCenterY;
    seedRow.spacing = 8.0;
    self.lowColourWell = [[NSColorWell alloc] initWithFrame:NSZeroRect];
    self.highColourWell = [[NSColorWell alloc] initWithFrame:NSZeroRect];
    self.lowColourWell.target = self;
    self.highColourWell.target = self;
    self.lowColourWell.action = @selector(coloursChanged:);
    self.highColourWell.action = @selector(coloursChanged:);
    self.lowColourWell.accessibilityLabel = @"Dark or base colour";
    self.highColourWell.accessibilityLabel = @"Light or accent colour";
    auto* colourRow = [NSStackView stackViewWithViews:@[
        wizardLabel(@"Dark/base colour"), self.lowColourWell,
        wizardLabel(@"Light/accent colour"), self.highColourWell,
    ]];
    colourRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    colourRow.alignment = NSLayoutAttributeCenterY;
    colourRow.spacing = 8.0;
    self.colourLockCheckbox = [NSButton checkboxWithTitle:@"Keep these colours in alternatives"
                                                    target:self action:@selector(lockChanged:)];
    self.colourLockCheckbox.state = NSControlStateValueOn;
    self.friendlyControlsStack = [NSStackView stackViewWithViews:@[]];
    self.friendlyControlsStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.friendlyControlsStack.alignment = NSLayoutAttributeLeading;
    self.friendlyControlsStack.spacing = 7.0;
    auto* note = wizardLabel(
        @"Adjust ordinary template properties. Tick a lock beside anything that should not change when exploring alternatives.");
    note.textColor = NSColor.secondaryLabelColor;
    auto* page = [self pageWithViews:@[
        wizardHeading(@"Shape its character"), note, seedRow, colourRow,
        self.colourLockCheckbox, self.friendlyControlsStack,
    ]];
    self.pages = [self.pages arrayByAddingObject:page];
}

- (void)buildAlternativesPage
{
    auto* generateButton = [NSButton buttonWithTitle:@"Generate Four Alternatives"
                                               target:self action:@selector(generateAlternatives:)];
    auto* buttons = [NSMutableArray array];
    for (NSInteger index = 0; index < 4; ++index) {
        auto* button = [NSButton buttonWithTitle:[NSString stringWithFormat:@"Variation %ld", index + 1]
                                          target:self action:@selector(selectAlternative:)];
        button.tag = index;
        button.buttonType = NSButtonTypePushOnPushOff;
        button.bezelStyle = NSBezelStyleRegularSquare;
        button.imagePosition = NSImageAbove;
        button.imageScaling = NSImageScaleProportionallyUpOrDown;
        button.enabled = NO;
        button.accessibilityLabel = [NSString stringWithFormat:@"Select material variation %ld", index + 1];
        [button.widthAnchor constraintEqualToConstant:176.0].active = YES;
        [button.heightAnchor constraintEqualToConstant:154.0].active = YES;
        [buttons addObject:button];
    }
    self.alternativeButtons = buttons;
    auto* firstRow = [NSStackView stackViewWithViews:@[buttons[0], buttons[1]]];
    firstRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    firstRow.spacing = 8.0;
    auto* secondRow = [NSStackView stackViewWithViews:@[buttons[2], buttons[3]]];
    secondRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    secondRow.spacing = 8.0;
    self.alternativeSummary = wizardLabel(@"Your current design is selected.");
    self.alternativeSummary.textColor = NSColor.secondaryLabelColor;
    auto* note = wizardLabel(
        @"Paperweight varies only unlocked choices. Select any result to inspect it at full preview quality before finishing.");
    note.textColor = NSColor.secondaryLabelColor;
    auto* page = [self pageWithViews:@[
        wizardHeading(@"Choose a variation"), note, generateButton,
        firstRow, secondRow, self.alternativeSummary,
    ]];
    self.pages = [self.pages arrayByAddingObject:page];
}

- (void)buildPreviewIn:(NSView*)rightPanel
{
    auto* heading = wizardHeading(@"Live material preview");
    self.previewModeControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    self.previewModeControl.segmentCount = 2;
    [self.previewModeControl setLabel:@"2D" forSegment:0];
    [self.previewModeControl setLabel:@"3D" forSegment:1];
    self.previewModeControl.selectedSegment = 0;
    self.previewModeControl.target = self;
    self.previewModeControl.action = @selector(previewModeChanged:);
    self.previewModeControl.accessibilityLabel = @"Live preview mode";
    auto* header = [NSStackView stackViewWithViews:@[heading, self.previewModeControl]];
    header.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    header.alignment = NSLayoutAttributeCenterY;
    header.distribution = NSStackViewDistributionFillProportionally;

    auto* previewBox = [[NSBox alloc] initWithFrame:NSZeroRect];
    previewBox.boxType = NSBoxCustom;
    previewBox.fillColor = [NSColor colorWithWhite:0.14 alpha:1.0];
    previewBox.borderWidth = 0.0;
    previewBox.cornerRadius = 10.0;
    self.previewImageView = [[NSImageView alloc] initWithFrame:NSZeroRect];
    self.previewImageView.translatesAutoresizingMaskIntoConstraints = NO;
    self.previewImageView.imageScaling = NSImageScaleProportionallyUpOrDown;
    self.previewImageView.accessibilityLabel = @"Live two-dimensional material preview";
    self.preview3DView = [[Material3DPreviewView alloc] initWithFrame:NSZeroRect];
    self.preview3DView.translatesAutoresizingMaskIntoConstraints = NO;
    self.preview3DView.hidden = YES;
    if (!self.preview3DView.isRendererAvailable) {
        [self.previewModeControl setEnabled:NO forSegment:1];
        self.previewModeControl.toolTip = @"This Mac does not provide a compatible Metal device.";
    }
    [previewBox.contentView addSubview:self.previewImageView];
    [previewBox.contentView addSubview:self.preview3DView];
    for (NSView* view in @[self.previewImageView, self.preview3DView]) {
        [NSLayoutConstraint activateConstraints:@[
            [view.leadingAnchor constraintEqualToAnchor:previewBox.contentView.leadingAnchor],
            [view.trailingAnchor constraintEqualToAnchor:previewBox.contentView.trailingAnchor],
            [view.topAnchor constraintEqualToAnchor:previewBox.contentView.topAnchor],
            [view.bottomAnchor constraintEqualToAnchor:previewBox.contentView.bottomAnchor],
        ]];
    }
    self.previewProgress = [[NSProgressIndicator alloc] initWithFrame:NSZeroRect];
    self.previewProgress.style = NSProgressIndicatorStyleSpinning;
    self.previewProgress.displayedWhenStopped = NO;
    self.previewStatus = wizardLabel(@"Preparing material…");
    self.previewStatus.textColor = NSColor.secondaryLabelColor;
    auto* statusRow = [NSStackView stackViewWithViews:@[self.previewProgress, self.previewStatus]];
    statusRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    statusRow.alignment = NSLayoutAttributeCenterY;
    statusRow.spacing = 8.0;

    for (NSView* view in @[header, previewBox, statusRow]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [rightPanel addSubview:view];
    }
    [NSLayoutConstraint activateConstraints:@[
        [header.leadingAnchor constraintEqualToAnchor:rightPanel.leadingAnchor constant:24.0],
        [header.trailingAnchor constraintEqualToAnchor:rightPanel.trailingAnchor constant:-24.0],
        [header.topAnchor constraintEqualToAnchor:rightPanel.topAnchor constant:20.0],
        [self.previewModeControl.widthAnchor constraintEqualToConstant:180.0],
        [previewBox.leadingAnchor constraintEqualToAnchor:header.leadingAnchor],
        [previewBox.trailingAnchor constraintEqualToAnchor:header.trailingAnchor],
        [previewBox.topAnchor constraintEqualToAnchor:header.bottomAnchor constant:16.0],
        [previewBox.bottomAnchor constraintEqualToAnchor:statusRow.topAnchor constant:-14.0],
        [statusRow.leadingAnchor constraintEqualToAnchor:header.leadingAnchor],
        [statusRow.trailingAnchor constraintEqualToAnchor:header.trailingAnchor],
        [statusRow.bottomAnchor constraintEqualToAnchor:rightPanel.bottomAnchor constant:-18.0],
    ]];
}

- (void)stepSelected:(id)sender
{
    static_cast<void>(sender);
    const NSInteger target = self.stepControl.selectedSegment;
    if (target > currentStep_ && currentStep_ <= 1 && target > 1 &&
        (![self applyScaleFields] || ![self applySurfaceFields])) {
        self.stepControl.selectedSegment = currentStep_;
        return;
    }
    if (target > currentStep_ && currentStep_ <= 2 && target > 2 &&
        ![self applySeedField]) {
        self.stepControl.selectedSegment = currentStep_;
        return;
    }
    [self showStep:target];
}

- (void)showStep:(NSInteger)step
{
    currentStep_ = std::clamp<NSInteger>(step, 0, 3);
    self.stepControl.selectedSegment = currentStep_;
    for (NSInteger index = 0; index < static_cast<NSInteger>(self.pages.count); ++index) {
        self.pages[static_cast<NSUInteger>(index)].hidden = index != currentStep_;
    }
    self.backButton.enabled = currentStep_ > 0;
    self.nextButton.hidden = currentStep_ == 3;
    self.editButton.hidden = currentStep_ != 3;
    self.libraryButton.hidden = currentStep_ != 3;
    self.window.defaultButtonCell = currentStep_ == 3
        ? self.editButton.cell : self.nextButton.cell;
    if (currentStep_ == 3 && alternatives_.empty()) {
        [self generateAlternatives:nil];
    }
    switch (currentStep_) {
    case 0: [self.window makeFirstResponder:self.familyPopup]; break;
    case 1: [self.window makeFirstResponder:self.scalePresetPopup]; break;
    case 2: [self.window makeFirstResponder:self.seedField]; break;
    case 3: [self.window makeFirstResponder:self.alternativeButtons.firstObject]; break;
    }
}

- (void)goBack:(id)sender
{
    static_cast<void>(sender);
    [self showStep:currentStep_ - 1];
}

- (void)goNext:(id)sender
{
    static_cast<void>(sender);
    if (currentStep_ == 1 &&
        (![self applyScaleFields] || ![self applySurfaceFields])) {
        return;
    }
    if (currentStep_ == 2 && ![self applySeedField]) {
        return;
    }
    [self showStep:currentStep_ + 1];
}

- (void)familyChanged:(id)sender
{
    static_cast<void>(sender);
    const NSInteger index = std::clamp<NSInteger>(
        self.familyPopup.indexOfSelectedItem, 0,
        static_cast<NSInteger>(paperweight::wizardMaterialFamilies.size() - 1));
    const auto& family = paperweight::wizardMaterialFamilies[static_cast<std::size_t>(index)];
    selectedFamily_ = family.family;
    self.familyDescription.stringValue = stringFromUtf8(family.description);
    familyTemplates_ = paperweight::wizardTemplatesForFamily(selectedFamily_);
    [self.templatePopup removeAllItems];
    for (const auto* descriptor : familyTemplates_) {
        [self.templatePopup addItemWithTitle:stringFromUtf8(descriptor->displayName)];
    }
    [self.templatePopup selectItemAtIndex:0];
    [self templateChanged:nil];
}

- (void)templateChanged:(id)sender
{
    static_cast<void>(sender);
    const NSInteger index = self.templatePopup.indexOfSelectedItem;
    if (index < 0 || static_cast<std::size_t>(index) >= familyTemplates_.size()) {
        return;
    }
    activeDescriptor_ = familyTemplates_[static_cast<std::size_t>(index)];
    self.templateDescription.stringValue = stringFromUtf8(activeDescriptor_->description);
    const auto resource = stringFromUtf8(activeDescriptor_->recipeResourceName);
    NSURL* url = [NSBundle.mainBundle URLForResource:resource
                                       withExtension:@"pmat"
                                        subdirectory:@"Showcases"];
    NSError* error = nil;
    NSString* text = url == nil ? nil : [NSString stringWithContentsOfURL:url
                                                                  encoding:NSUTF8StringEncoding
                                                                     error:&error];
    if (text == nil || text.UTF8String == nullptr) {
        [self showError:error.localizedDescription != nil
            ? error.localizedDescription : @"The bundled starting material is missing."];
        return;
    }
    const auto parsed = paperweight::parsePmat(text.UTF8String);
    if (const auto* diagnostic = std::get_if<paperweight::ParseDiagnostic>(&parsed)) {
        [self showError:[NSString stringWithFormat:@"Line %zu: %s",
            diagnostic->line, diagnostic->message.c_str()]];
        return;
    }
    const auto sessionResult = paperweight::makeMaterialWizardSession(
        *activeDescriptor_,
        paperweight::makeMaterialRecipe(std::get<paperweight::Material>(parsed)),
        session_ ? session_->baseSeed : 18431);
    if (const auto* wizardError = std::get_if<paperweight::MaterialWizardError>(&sessionResult)) {
        [self showError:stringFromUtf8(wizardError->message)];
        return;
    }
    session_ = std::get<paperweight::MaterialWizardSession>(sessionResult);
    [self refreshSessionControls];
    [self invalidateAlternativesAndPreview];
}

- (void)refreshSessionControls
{
    if (!session_ || activeDescriptor_ == nullptr) return;
    self.widthField.stringValue = [NSString stringWithFormat:@"%.6g", session_->physicalSize.widthMetres];
    self.heightField.stringValue = [NSString stringWithFormat:@"%.6g", session_->physicalSize.heightMetres];
    if (!session_->recipe.reliefDepthMetres) {
        session_->recipe.reliefDepthMetres = 0.003;
    }
    self.reliefDepthField.stringValue = [NSString stringWithFormat:
        @"%.6g", *session_->recipe.reliefDepthMetres * 1000.0];
    self.normalMultiplierField.stringValue = [NSString stringWithFormat:
        @"%.6g", session_->recipe.normalStrength];
    [self.scalePresetPopup selectItemAtIndex:0];
    self.scaleLockCheckbox.state = session_->physicalSizeLocked
        ? NSControlStateValueOn : NSControlStateValueOff;
    self.seedField.stringValue = [NSString stringWithFormat:@"%llu", session_->baseSeed];
    self.seedLockCheckbox.state = session_->seedLocked
        ? NSControlStateValueOn : NSControlStateValueOff;
    self.colourLockCheckbox.state = session_->coloursLocked
        ? NSControlStateValueOn : NSControlStateValueOff;
    self.lowColourWell.color = [NSColor colorWithSRGBRed:session_->lowColour.red / 255.0
                                                   green:session_->lowColour.green / 255.0
                                                    blue:session_->lowColour.blue / 255.0
                                                   alpha:session_->lowColour.alpha / 255.0];
    self.highColourWell.color = [NSColor colorWithSRGBRed:session_->highColour.red / 255.0
                                                    green:session_->highColour.green / 255.0
                                                     blue:session_->highColour.blue / 255.0
                                                    alpha:session_->highColour.alpha / 255.0];
    [self updateScaleSummary];
    [self rebuildFriendlyControls];
}

- (void)rebuildFriendlyControls
{
    for (NSView* view in [self.friendlyControlsStack.arrangedSubviews copy]) {
        [self.friendlyControlsStack removeArrangedSubview:view];
        [view removeFromSuperview];
    }
    [self.controlSliders removeAllObjects];
    [self.controlValues removeAllObjects];
    [self.controlLocks removeAllObjects];
    if (!session_ || activeDescriptor_ == nullptr) return;
    for (const auto section : {paperweight::WizardControlSection::construction,
                               paperweight::WizardControlSection::surface,
                               paperweight::WizardControlSection::wear}) {
        bool addedHeading = false;
        for (std::size_t index = 0; index < session_->controls.size(); ++index) {
            auto& state = session_->controls[index];
            if (state.section != section) continue;
            if (!addedHeading) {
                auto* heading = wizardLabel(controlSectionName(section));
                heading.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold];
                [self.friendlyControlsStack addArrangedSubview:heading];
                addedHeading = true;
            }
            const auto& control = activeDescriptor_->controls[index];
            auto* name = wizardLabel(stringFromUtf8(control.displayName));
            [name.widthAnchor constraintEqualToConstant:112.0].active = YES;
            auto* slider = [NSSlider sliderWithValue:state.value
                                           minValue:control.minimumValue
                                           maxValue:control.maximumValue
                                             target:self
                                             action:@selector(friendlyControlChanged:)];
            slider.continuous = YES;
            slider.tag = static_cast<NSInteger>(index);
            slider.accessibilityLabel = name.stringValue;
            auto* value = wizardLabel(@"");
            value.alignment = NSTextAlignmentRight;
            [value.widthAnchor constraintEqualToConstant:48.0].active = YES;
            value.stringValue = control.step >= 1.0
                ? [NSString stringWithFormat:@"%.0f", state.value]
                : [NSString stringWithFormat:@"%.3g", state.value];
            auto* lock = [NSButton checkboxWithTitle:@"Lock"
                                               target:self action:@selector(controlLockChanged:)];
            lock.tag = static_cast<NSInteger>(index);
            lock.state = state.locked ? NSControlStateValueOn : NSControlStateValueOff;
            lock.accessibilityLabel = [NSString stringWithFormat:@"Lock %@", name.stringValue];
            auto* row = [NSStackView stackViewWithViews:@[name, slider, value, lock]];
            row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
            row.alignment = NSLayoutAttributeCenterY;
            row.spacing = 7.0;
            [row.widthAnchor constraintEqualToConstant:386.0].active = YES;
            [self.friendlyControlsStack addArrangedSubview:row];
            [self.controlSliders addObject:slider];
            [self.controlValues addObject:value];
            [self.controlLocks addObject:lock];
        }
    }
}

- (void)scalePresetChanged:(id)sender
{
    static_cast<void>(sender);
    if (!session_) return;
    switch (self.scalePresetPopup.indexOfSelectedItem) {
    case 0: session_->physicalSize = session_->recipe.physicalSize; break;
    case 1: session_->physicalSize = {0.5, 0.5}; break;
    case 2: session_->physicalSize = {1.0, 1.0}; break;
    case 3: session_->physicalSize = {2.0, 1.0}; break;
    case 4: session_->physicalSize = {4.0, 3.0}; break;
    default: return;
    }
    self.widthField.stringValue = [NSString stringWithFormat:@"%.6g", session_->physicalSize.widthMetres];
    self.heightField.stringValue = [NSString stringWithFormat:@"%.6g", session_->physicalSize.heightMetres];
    [self updateScaleSummary];
    [self invalidateAlternativesAndPreview];
}

- (BOOL)applyScaleFields
{
    if (!session_) return NO;
    const double width = self.widthField.doubleValue;
    const double height = self.heightField.doubleValue;
    if (!std::isfinite(width) || !std::isfinite(height) ||
        width < paperweight::PhysicalLimits::minimumMetres ||
        width > paperweight::PhysicalLimits::maximumMetres ||
        height < paperweight::PhysicalLimits::minimumMetres ||
        height > paperweight::PhysicalLimits::maximumMetres) {
        [self showError:@"Width and height must each be between 0.000001 and 1,000,000 metres."];
        return NO;
    }
    session_->physicalSize = {width, height};
    [self.scalePresetPopup selectItemAtIndex:5];
    [self updateScaleSummary];
    [self invalidateAlternativesAndPreview];
    return YES;
}

- (BOOL)applySurfaceFields
{
    if (!session_) return NO;
    const double reliefMillimetres = self.reliefDepthField.doubleValue;
    const double normalMultiplier = self.normalMultiplierField.doubleValue;
    if (!std::isfinite(reliefMillimetres) || reliefMillimetres < 0.0 ||
        reliefMillimetres > paperweight::MaterialLimits::maximumReliefDepthMetres * 1000.0 ||
        !std::isfinite(normalMultiplier) ||
        normalMultiplier < paperweight::MaterialLimits::minimumNormalStrength ||
        normalMultiplier > paperweight::MaterialLimits::maximumNormalStrength) {
        [self showError:@"Relief depth must be non-negative, and the normal multiplier must be between 0 and 16."];
        return NO;
    }
    session_->recipe.reliefDepthMetres = reliefMillimetres / 1000.0;
    session_->recipe.normalStrength = normalMultiplier;
    [self invalidateAlternativesAndPreview];
    return YES;
}

- (void)updateScaleSummary
{
    if (!session_) return;
    self.scaleSummary.stringValue = [NSString stringWithFormat:
        @"One seamless repeat covers %.3g m × %.3g m. At %lu × %lu preview pixels, each pixel represents about %.3g mm × %.3g mm.",
        session_->physicalSize.widthMetres, session_->physicalSize.heightMetres,
        static_cast<unsigned long>(previewResolution_), static_cast<unsigned long>(previewResolution_),
        session_->physicalSize.widthMetres * 1000.0 / static_cast<double>(previewResolution_),
        session_->physicalSize.heightMetres * 1000.0 / static_cast<double>(previewResolution_)];
}

- (BOOL)applySeedField
{
    if (!session_) return NO;
    const std::string text = self.seedField.stringValue.UTF8String;
    std::uint64_t seed = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), seed);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        [self showError:@"The starting variation must be a non-negative whole number."];
        return NO;
    }
    session_->baseSeed = seed;
    [self invalidateAlternativesAndPreview];
    return YES;
}

- (void)controlTextDidEndEditing:(NSNotification*)notification
{
    if (notification.object == self.widthField || notification.object == self.heightField) {
        [self applyScaleFields];
    } else if (notification.object == self.reliefDepthField ||
               notification.object == self.normalMultiplierField) {
        [self applySurfaceFields];
    } else if (notification.object == self.seedField) {
        [self applySeedField];
    }
}

- (void)friendlyControlChanged:(NSSlider*)sender
{
    if (!session_ || activeDescriptor_ == nullptr || sender.tag < 0 ||
        static_cast<std::size_t>(sender.tag) >= session_->controls.size()) return;
    const auto index = static_cast<std::size_t>(sender.tag);
    const auto& control = activeDescriptor_->controls[index];
    double value = sender.doubleValue;
    if (control.step > 0.0) {
        value = control.minimumValue + std::round(
            (value - control.minimumValue) / control.step) * control.step;
    }
    value = std::clamp(value, control.minimumValue, control.maximumValue);
    sender.doubleValue = value;
    session_->controls[index].value = value;
    self.controlValues[index].stringValue = control.step >= 1.0
        ? [NSString stringWithFormat:@"%.0f", value]
        : [NSString stringWithFormat:@"%.3g", value];
    [self invalidateAlternativesAndPreview];
}

- (void)controlLockChanged:(NSButton*)sender
{
    if (!session_ || sender.tag < 0 ||
        static_cast<std::size_t>(sender.tag) >= session_->controls.size()) return;
    session_->controls[static_cast<std::size_t>(sender.tag)].locked =
        sender.state == NSControlStateValueOn;
    [self clearAlternatives];
}

- (paperweight::Rgba8)rgbaFromWell:(NSColorWell*)well
{
    NSColor* colour = [well.color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    if (colour == nil) colour = well.color;
    return {
        static_cast<std::uint8_t>(std::clamp(std::llround(colour.redComponent * 255.0), 0LL, 255LL)),
        static_cast<std::uint8_t>(std::clamp(std::llround(colour.greenComponent * 255.0), 0LL, 255LL)),
        static_cast<std::uint8_t>(std::clamp(std::llround(colour.blueComponent * 255.0), 0LL, 255LL)),
        static_cast<std::uint8_t>(std::clamp(std::llround(colour.alphaComponent * 255.0), 0LL, 255LL)),
    };
}

- (void)coloursChanged:(id)sender
{
    static_cast<void>(sender);
    if (!session_) return;
    session_->lowColour = [self rgbaFromWell:self.lowColourWell];
    session_->highColour = [self rgbaFromWell:self.highColourWell];
    [self invalidateAlternativesAndPreview];
}

- (void)lockChanged:(id)sender
{
    static_cast<void>(sender);
    if (!session_) return;
    session_->seedLocked = self.seedLockCheckbox.state == NSControlStateValueOn;
    session_->physicalSizeLocked = self.scaleLockCheckbox.state == NSControlStateValueOn;
    session_->coloursLocked = self.colourLockCheckbox.state == NSControlStateValueOn;
    [self clearAlternatives];
}

- (void)clearAlternatives
{
    ++alternativeRevision_;
    if (alternativeCancellation_) alternativeCancellation_->store(true, std::memory_order_relaxed);
    alternatives_.clear();
    selectedAlternative_ = -1;
    for (NSButton* button in self.alternativeButtons) {
        button.image = nil;
        button.enabled = NO;
        button.state = NSControlStateValueOff;
    }
    self.alternativeSummary.stringValue = @"Your current design is selected.";
}

- (void)invalidateAlternativesAndPreview
{
    [self clearAlternatives];
    [self updateSelectedMaterialFromSession];
}

- (void)updateSelectedMaterialFromSession
{
    if (!session_ || activeDescriptor_ == nullptr) return;
    const auto result = paperweight::makeMaterialFromWizard(*session_, *activeDescriptor_);
    if (const auto* error = std::get_if<paperweight::MaterialWizardError>(&result)) {
        selectedMaterial_.reset();
        self.previewStatus.stringValue = stringFromUtf8(error->message);
        self.previewStatus.textColor = NSColor.systemRedColor;
        return;
    }
    selectedMaterial_ = std::get<paperweight::Material>(result);
    [self schedulePreview];
}

- (void)generateAlternatives:(id)sender
{
    static_cast<void>(sender);
    if (!session_ || activeDescriptor_ == nullptr) return;
    if (![self applyScaleFields] || ![self applySurfaceFields] ||
        ![self applySeedField]) return;
    const auto result = paperweight::generateMaterialWizardAlternatives(
        *session_, *activeDescriptor_, 4);
    if (const auto* error = std::get_if<paperweight::MaterialWizardError>(&result)) {
        [self showError:stringFromUtf8(error->message)];
        return;
    }
    [self clearAlternatives];
    alternatives_ = std::get<std::vector<paperweight::MaterialWizardAlternative>>(result);
    const auto revision = ++alternativeRevision_;
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    alternativeCancellation_ = cancellation;
    self.alternativeSummary.stringValue = @"Generating comparison thumbnails…";
    for (std::size_t index = 0; index < alternatives_.size(); ++index) {
        self.alternativeButtons[index].enabled = YES;
        const auto material = alternatives_[index].material;
        __weak MaterialWizardWindowController* weakSelf = self;
        dispatch_async(alternativeQueue_, ^{
            auto generated = paperweight::generate(
                {material, 160, 160, paperweight::MaterialOutput::colour,
                 std::nullopt, std::nullopt},
                [cancellation]() { return cancellation->load(std::memory_order_relaxed); });
            auto image = std::make_shared<std::optional<paperweight::Image>>();
            if (auto* value = std::get_if<paperweight::Image>(&generated)) {
                *image = std::move(*value);
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                MaterialWizardWindowController* strongSelf = weakSelf;
                if (strongSelf == nil || revision != strongSelf->alternativeRevision_ ||
                    cancellation->load(std::memory_order_relaxed) || !*image) return;
                strongSelf.alternativeButtons[index].image =
                    imageFromPaperweight(**image, NSMakeSize(132.0, 112.0));
                if (index + 1 == alternatives_.size()) {
                    strongSelf.alternativeSummary.stringValue =
                        @"Four deterministic alternatives are ready. Your current design remains selected.";
                }
            });
        });
    }
}

- (void)selectAlternative:(NSButton*)sender
{
    if (sender.tag < 0 || static_cast<std::size_t>(sender.tag) >= alternatives_.size()) return;
    selectedAlternative_ = sender.tag;
    selectedMaterial_ = alternatives_[static_cast<std::size_t>(sender.tag)].material;
    for (NSButton* button in self.alternativeButtons) {
        button.state = button == sender ? NSControlStateValueOn : NSControlStateValueOff;
    }
    self.alternativeSummary.stringValue = [NSString stringWithFormat:
        @"Variation %ld selected — seed %llu.", sender.tag + 1, selectedMaterial_->seed];
    [self schedulePreview];
}

- (void)previewModeChanged:(id)sender
{
    static_cast<void>(sender);
    const BOOL threeDimensional = self.previewModeControl.selectedSegment == 1;
    self.previewImageView.hidden = threeDimensional;
    self.preview3DView.hidden = !threeDimensional;
    [self schedulePreview];
}

- (void)schedulePreview
{
    if (!selectedMaterial_) return;
    ++previewRevision_;
    const auto revision = previewRevision_;
    if (previewCancellation_) previewCancellation_->store(true, std::memory_order_relaxed);
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    previewCancellation_ = cancellation;
    const auto material = *selectedMaterial_;
    const BOOL threeDimensional = self.previewModeControl.selectedSegment == 1;
    [self.previewProgress startAnimation:nil];
    self.previewStatus.textColor = NSColor.secondaryLabelColor;
    self.previewStatus.stringValue = threeDimensional
        ? [NSString stringWithFormat:@"Rendering four %lu × %lu maps…",
            static_cast<unsigned long>(previewResolution_), static_cast<unsigned long>(previewResolution_)]
        : [NSString stringWithFormat:@"Rendering %lu × %lu colour preview…",
            static_cast<unsigned long>(previewResolution_), static_cast<unsigned long>(previewResolution_)];
    __weak MaterialWizardWindowController* weakSelf = self;
    dispatch_async(previewQueue_, ^{
        paperweight::GenerationRequest request{
            material,
            static_cast<std::uint32_t>(previewResolution_),
            static_cast<std::uint32_t>(previewResolution_),
            paperweight::MaterialOutput::colour,
            std::nullopt,
            std::nullopt,
        };
        auto compilation = paperweight::compileMaterialGraph(material);
        std::optional<std::string> failure;
        if (auto* graph = std::get_if<paperweight::MaterialGraph>(&compilation)) {
            request.graph = std::move(*graph);
        } else {
            failure = std::get<paperweight::GraphError>(compilation).message;
        }
        std::array<std::optional<paperweight::Image>, 4> images;
        const std::size_t outputCount = threeDimensional ? 4 : 1;
        if (!failure) {
            for (std::size_t index = 0; index < outputCount; ++index) {
                if (cancellation->load(std::memory_order_relaxed)) break;
                request.output = paperweight::materialOutputs[index];
                auto generated = paperweight::generate(
                    request,
                    [cancellation]() { return cancellation->load(std::memory_order_relaxed); });
                if (auto* image = std::get_if<paperweight::Image>(&generated)) {
                    images[index] = std::move(*image);
                } else {
                    failure = std::get<paperweight::GenerationError>(generated).message;
                    break;
                }
            }
        }
        auto sharedImages = std::make_shared<decltype(images)>(std::move(images));
        dispatch_async(dispatch_get_main_queue(), ^{
            MaterialWizardWindowController* strongSelf = weakSelf;
            if (strongSelf == nil || revision != strongSelf->previewRevision_ ||
                cancellation->load(std::memory_order_relaxed)) return;
            [strongSelf.previewProgress stopAnimation:nil];
            if (failure) {
                strongSelf.previewStatus.stringValue = stringFromUtf8(*failure);
                strongSelf.previewStatus.textColor = NSColor.systemRedColor;
                return;
            }
            if (threeDimensional) {
                if (std::all_of(sharedImages->begin(), sharedImages->end(),
                    [](const auto& image) { return image.has_value(); })) {
                    [strongSelf.preview3DView setColourImage:*(*sharedImages)[0]
                                                  heightImage:*(*sharedImages)[1]
                                                  normalImage:*(*sharedImages)[2]
                                               roughnessImage:*(*sharedImages)[3]];
                }
            } else if ((*sharedImages)[0]) {
                strongSelf.previewImageView.image = imageFromPaperweight(
                    *(*sharedImages)[0], NSMakeSize(640.0, 640.0));
            }
            strongSelf.previewStatus.stringValue = [NSString stringWithFormat:
                @"%lu × %lu preview — %.3g × %.3g m seamless repeat",
                static_cast<unsigned long>(previewResolution_),
                static_cast<unsigned long>(previewResolution_),
                material.physicalSize.widthMetres, material.physicalSize.heightMetres];
            strongSelf.previewStatus.textColor = NSColor.secondaryLabelColor;
        });
    });
}

- (void)useInEditor:(id)sender
{
    static_cast<void>(sender);
    if (!selectedMaterial_ || activeDescriptor_ == nullptr || self.useMaterialHandler == nil) return;
    if (self.useMaterialHandler(
            *selectedMaterial_, stringFromUtf8(activeDescriptor_->identifier))) {
        [self close];
    }
}

- (NSURL*)workingFolder
{
    NSString* remembered = [NSUserDefaults.standardUserDefaults
        stringForKey:@"materialLibraryWorkingFolder"];
    if (remembered.length != 0) {
        return [NSURL fileURLWithPath:remembered isDirectory:YES].URLByStandardizingPath;
    }
    auto* panel = [NSOpenPanel openPanel];
    panel.title = @"Choose Paperweight Material Library Folder";
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = NO;
    panel.canCreateDirectories = YES;
    if ([panel runModal] != NSModalResponseOK) return nil;
    [NSUserDefaults.standardUserDefaults setObject:panel.URL.path
                                             forKey:@"materialLibraryWorkingFolder"];
    return panel.URL.URLByStandardizingPath;
}

- (void)saveToLibrary:(id)sender
{
    static_cast<void>(sender);
    if (!selectedMaterial_ || activeDescriptor_ == nullptr) return;
    NSURL* folder = [self workingFolder];
    if (folder == nil) return;
    auto* nameField = [NSTextField textFieldWithString:stringFromUtf8(activeDescriptor_->displayName)];
    auto* categoryField = [NSTextField textFieldWithString:
        stringFromUtf8(paperweight::wizardMaterialFamilies[static_cast<std::size_t>(selectedFamily_)].displayName)];
    auto* tagsField = [NSTextField textFieldWithString:@"wizard-created"];
    auto* grid = [NSGridView gridViewWithViews:@[
        @[wizardLabel(@"Name"), nameField],
        @[wizardLabel(@"Category"), categoryField],
        @[wizardLabel(@"Tags"), tagsField],
    ]];
    [grid.widthAnchor constraintEqualToConstant:440.0].active = YES;
    auto* alert = [[NSAlert alloc] init];
    alert.messageText = @"Save Material to Library";
    alert.informativeText = @"Paperweight will assign a new stable UID and preserve the material as ordinary readable .pmat text.";
    alert.accessoryView = grid;
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Cancel"];
    if ([alert runModal] != NSAlertFirstButtonReturn) return;
    auto trim = [](NSString* value) {
        return [value stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    };
    NSString* name = trim(nameField.stringValue);
    if (name.length == 0) {
        [self showError:@"A library material needs a friendly name."];
        return;
    }
    auto material = *selectedMaterial_;
    paperweight::MaterialMetadata metadata;
    metadata.uid = NSUUID.UUID.UUIDString.lowercaseString.UTF8String;
    metadata.name = name.UTF8String;
    metadata.description = std::string(activeDescriptor_->description);
    metadata.category = trim(categoryField.stringValue).UTF8String;
    for (NSString* component in [tagsField.stringValue componentsSeparatedByString:@","]) {
        NSString* tag = trim(component);
        if (tag.length != 0) metadata.tags.emplace_back(tag.UTF8String);
    }
    material.metadata = std::move(metadata);
    const auto serialised = paperweight::serialisePmat(material);
    if (const auto* error = std::get_if<paperweight::SerialisationError>(&serialised)) {
        [self showError:stringFromUtf8(error->message)];
        return;
    }
    const std::string base = slugForWizardName(name);
    NSURL* target = nil;
    for (NSUInteger suffix = 1; suffix < 10000; ++suffix) {
        std::string filename = base;
        if (suffix > 1) filename += "-" + std::to_string(suffix);
        filename += ".pmat";
        NSURL* candidate = [folder URLByAppendingPathComponent:stringFromUtf8(filename)];
        if (![NSFileManager.defaultManager fileExistsAtPath:candidate.path]) {
            target = candidate;
            break;
        }
    }
    if (target == nil) {
        [self showError:@"Paperweight could not find a collision-free filename."];
        return;
    }
    const auto& source = std::get<std::string>(serialised);
    NSString* text = [[NSString alloc] initWithBytes:source.data()
                                             length:source.size()
                                           encoding:NSUTF8StringEncoding];
    NSError* writeError = nil;
    if (text == nil || ![text writeToURL:target atomically:YES
                                encoding:NSUTF8StringEncoding error:&writeError]) {
        [self showError:writeError.localizedDescription != nil
            ? writeError.localizedDescription : @"The material could not be written."];
        return;
    }
    if (self.savedMaterialHandler != nil) self.savedMaterialHandler(target);
    auto* success = [[NSAlert alloc] init];
    success.messageText = @"Material saved to the working library";
    success.informativeText = target.path;
    [success runModal];
    [self close];
}

- (void)showError:(NSString*)message
{
    auto* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = @"Paperweight could not continue";
    alert.informativeText = message;
    [alert runModal];
}

@end
