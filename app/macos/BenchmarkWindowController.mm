#import "BenchmarkWindowController.hpp"

#include <paperweight/generator.hpp>
#include <paperweight/graph.hpp>
#include <paperweight/image.hpp>
#include <paperweight/output.hpp>
#include <paperweight/pmat.hpp>
#include <paperweight/version.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <sys/sysctl.h>

@interface PWBenchmarkRow : NSObject

@property(nonatomic, copy) NSString* material;
@property(nonatomic) NSUInteger resolution;
@property(nonatomic) NSUInteger multiWorkers;
@property(nonatomic) double singleMilliseconds;
@property(nonatomic) double multiMilliseconds;
@property(nonatomic) double speedup;
@property(nonatomic) double singleThroughput;
@property(nonatomic) double multiThroughput;
@property(nonatomic) unsigned long long checksum;
@property(nonatomic) BOOL identical;

@end

@implementation PWBenchmarkRow
@end

namespace {

using Clock = std::chrono::steady_clock;

struct Showcase {
    const char* name;
    const char* resource;
};

constexpr std::array<Showcase, 18> showcases{{
    {"Default Noise", "default"},
    {"Brick Wall", "brick-wall"},
    {"Cobblestone", "cobblestone"},
    {"Ember", "ember"},
    {"Cracked Stone", "cracked-stone"},
    {"Weathered Metal", "weathered-metal"},
    {"Mossy Pebbles", "mossy-pebbles"},
    {"Knotty Wood", "knotty-wood"},
    {"Marble Veins", "marble-veins"},
    {"Eroded Terrain", "eroded-terrain"},
    {"Toon Dungeon", "toon-dungeon"},
    {"Painted Metal", "painted-metal"},
    {"Graphic Marble", "graphic-marble"},
    {"Region Stones", "region-stones"},
    {"Castle Flagstone", "castle-flagstone"},
    {"Castle Stone", "castle-stone"},
    {"Cel Castle Stone", "cel-castle-stone"},
    {"Castle Roof", "castle-roof"},
}};

constexpr std::array<std::uint32_t, 5> resolutions{{64, 128, 256, 512, 1024}};

std::uint64_t checksum(std::span<const paperweight::Rgba8> pixels)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto& pixel : pixels) {
        for (const auto channel : {pixel.red, pixel.green, pixel.blue, pixel.alpha}) {
            hash ^= channel;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

NSString* outputDisplayName(paperweight::MaterialOutput output)
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

NSString* architectureName()
{
#if defined(__arm64__)
    return @"arm64";
#elif defined(__x86_64__)
    return @"x86_64";
#else
    return @"unknown";
#endif
}

NSString* machineModel()
{
    std::size_t length = 0;
    if (sysctlbyname("hw.model", nullptr, &length, nullptr, 0) != 0 || length == 0) {
        return @"Unknown Mac";
    }
    std::vector<char> value(length);
    if (sysctlbyname("hw.model", value.data(), &length, nullptr, 0) != 0) {
        return @"Unknown Mac";
    }
    NSString* model = [NSString stringWithUTF8String:value.data()];
    return model != nil ? model : @"Unknown Mac";
}

NSString* csvEscaped(NSString* value)
{
    NSString* escaped = [value stringByReplacingOccurrencesOfString:@"\"" withString:@"\"\""];
    return [NSString stringWithFormat:@"\"%@\"", escaped];
}

NSTextField* labelWithText(NSString* text)
{
    NSTextField* label = [NSTextField labelWithString:text];
    label.selectable = NO;
    return label;
}

NSTableColumn* tableColumn(NSString* identifier, NSString* title, CGFloat width)
{
    NSTableColumn* column = [[NSTableColumn alloc] initWithIdentifier:identifier];
    column.title = title;
    column.width = width;
    column.minWidth = std::min(width, 64.0);
    column.resizingMask = NSTableColumnUserResizingMask;
    return column;
}

} // namespace

@interface BenchmarkWindowController () <NSWindowDelegate, NSTableViewDataSource, NSTableViewDelegate>

@property(nonatomic, strong) NSPopUpButton* outputPopup;
@property(nonatomic, strong) NSButton* startButton;
@property(nonatomic, strong) NSButton* cancelButton;
@property(nonatomic, strong) NSButton* csvCopyButton;
@property(nonatomic, strong) NSButton* saveButton;
@property(nonatomic, strong) NSProgressIndicator* progressIndicator;
@property(nonatomic, strong) NSTextField* statusLabel;
@property(nonatomic, strong) NSTextField* completedSummary;
@property(nonatomic, strong) NSTextField* averageSummary;
@property(nonatomic, strong) NSTextField* bestSummary;
@property(nonatomic, strong) NSTextField* matchSummary;
@property(nonatomic, strong) NSTableView* resultsTable;
@property(nonatomic, strong) NSMutableArray<PWBenchmarkRow*>* rows;
@property(nonatomic, copy) NSString* runMetadata;

@end

@implementation BenchmarkWindowController {
    dispatch_queue_t benchmarkQueue_;
    std::shared_ptr<std::atomic_bool> cancellation_;
    BOOL running_;
    NSUInteger multiWorkerCount_;
    paperweight::MaterialOutput selectedOutput_;
}

- (instancetype)init
{
    const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 980, 640)
                                                   styleMask:style
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    self = [super initWithWindow:window];
    if (self) {
        window.title = @"Performance Benchmark — Paperweight";
        window.minSize = NSMakeSize(820, 520);
        window.delegate = self;
        _rows = [[NSMutableArray alloc] init];
        benchmarkQueue_ = dispatch_queue_create(
            "org.solen-music.paperweight.benchmark",
            DISPATCH_QUEUE_SERIAL);
        [self buildInterface];
    }
    return self;
}

- (void)showBenchmarkWindow
{
    [self showWindow:nil];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
}

- (void)buildInterface
{
    NSView* content = [[NSView alloc] initWithFrame:NSZeroRect];
    content.translatesAutoresizingMaskIntoConstraints = NO;
    self.window.contentView = content;

    NSTextField* title = labelWithText(@"Performance Benchmark");
    title.translatesAutoresizingMaskIntoConstraints = NO;
    title.font = [NSFont systemFontOfSize:24.0 weight:NSFontWeightSemibold];

    NSTextField* explanation = labelWithText(
        @"Runs every bundled material at 64, 128, 256, 512, and 1024 pixels. "
         "Each result compares one worker with the full worker pool and verifies the final pixels exactly.");
    explanation.translatesAutoresizingMaskIntoConstraints = NO;
    explanation.maximumNumberOfLines = 2;
    explanation.lineBreakMode = NSLineBreakByWordWrapping;
    explanation.textColor = NSColor.secondaryLabelColor;

    NSTextField* outputLabel = labelWithText(@"Material output:");
    self.outputPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.outputPopup addItemsWithTitles:@[@"Colour", @"Height", @"Normal", @"Roughness"]];
    self.outputPopup.toolTip = @"A complete run benchmarks one output map at a time.";

    self.startButton = [NSButton buttonWithTitle:@"Start Benchmark"
                                          target:self
                                          action:@selector(startBenchmark:)];
    self.startButton.keyEquivalent = @"\r";
    self.startButton.bezelStyle = NSBezelStyleRounded;

    self.cancelButton = [NSButton buttonWithTitle:@"Cancel"
                                           target:self
                                           action:@selector(cancelBenchmark:)];
    self.cancelButton.enabled = NO;

    self.csvCopyButton = [NSButton buttonWithTitle:@"Copy CSV"
                                            target:self
                                            action:@selector(copyCsv:)];
    self.csvCopyButton.enabled = NO;

    self.saveButton = [NSButton buttonWithTitle:@"Save CSV…"
                                         target:self
                                         action:@selector(saveCsv:)];
    self.saveButton.enabled = NO;

    NSStackView* controls = [NSStackView stackViewWithViews:@[
        outputLabel,
        self.outputPopup,
        self.startButton,
        self.cancelButton,
        self.csvCopyButton,
        self.saveButton,
    ]];
    controls.translatesAutoresizingMaskIntoConstraints = NO;
    controls.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    controls.alignment = NSLayoutAttributeCenterY;
    controls.spacing = 8.0;
    [controls setCustomSpacing:22.0 afterView:self.outputPopup];

    self.progressIndicator = [[NSProgressIndicator alloc] initWithFrame:NSZeroRect];
    self.progressIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    self.progressIndicator.indeterminate = NO;
    self.progressIndicator.minValue = 0.0;
    self.progressIndicator.maxValue = static_cast<double>(showcases.size() * resolutions.size() * 2);
    self.progressIndicator.doubleValue = 0.0;

    self.statusLabel = labelWithText(
        @"Ready. A full run can take several minutes; other heavy applications may affect timings.");
    self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
    self.statusLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;

    self.completedSummary = labelWithText(@"Completed: 0 / 50");
    self.averageSummary = labelWithText(@"Average speed-up: —");
    self.bestSummary = labelWithText(@"Best speed-up: —");
    self.matchSummary = labelWithText(@"Exact matches: 0 / 0");
    for (NSTextField* summary in @[
             self.completedSummary,
             self.averageSummary,
             self.bestSummary,
             self.matchSummary,
         ]) {
        summary.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightMedium];
    }

    NSStackView* summaries = [NSStackView stackViewWithViews:@[
        self.completedSummary,
        self.averageSummary,
        self.bestSummary,
        self.matchSummary,
    ]];
    summaries.translatesAutoresizingMaskIntoConstraints = NO;
    summaries.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    summaries.distribution = NSStackViewDistributionFillEqually;
    summaries.spacing = 16.0;

    self.resultsTable = [[NSTableView alloc] initWithFrame:NSZeroRect];
    self.resultsTable.delegate = self;
    self.resultsTable.dataSource = self;
    self.resultsTable.usesAlternatingRowBackgroundColors = YES;
    self.resultsTable.allowsMultipleSelection = NO;
    self.resultsTable.columnAutoresizingStyle = NSTableViewUniformColumnAutoresizingStyle;
    [self.resultsTable addTableColumn:tableColumn(@"material", @"Material", 170.0)];
    [self.resultsTable addTableColumn:tableColumn(@"resolution", @"Size", 70.0)];
    [self.resultsTable addTableColumn:tableColumn(@"single", @"1 Worker", 100.0)];
    [self.resultsTable addTableColumn:tableColumn(@"multi", @"Multi", 100.0)];
    [self.resultsTable addTableColumn:tableColumn(@"speedup", @"Speed-up", 85.0)];
    [self.resultsTable addTableColumn:tableColumn(@"singleThroughput", @"1 Worker MP/s", 110.0)];
    [self.resultsTable addTableColumn:tableColumn(@"multiThroughput", @"Multi MP/s", 100.0)];
    [self.resultsTable addTableColumn:tableColumn(@"match", @"Exact", 75.0)];

    NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    scroll.hasVerticalScroller = YES;
    scroll.hasHorizontalScroller = YES;
    scroll.autohidesScrollers = YES;
    scroll.borderType = NSBezelBorder;
    scroll.documentView = self.resultsTable;

    [content addSubview:title];
    [content addSubview:explanation];
    [content addSubview:controls];
    [content addSubview:self.progressIndicator];
    [content addSubview:self.statusLabel];
    [content addSubview:summaries];
    [content addSubview:scroll];

    [NSLayoutConstraint activateConstraints:@[
        [title.topAnchor constraintEqualToAnchor:content.topAnchor constant:22.0],
        [title.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:24.0],
        [title.trailingAnchor constraintLessThanOrEqualToAnchor:content.trailingAnchor constant:-24.0],

        [explanation.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:4.0],
        [explanation.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [explanation.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-24.0],

        [controls.topAnchor constraintEqualToAnchor:explanation.bottomAnchor constant:16.0],
        [controls.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [controls.trailingAnchor constraintLessThanOrEqualToAnchor:content.trailingAnchor constant:-24.0],

        [self.progressIndicator.topAnchor constraintEqualToAnchor:controls.bottomAnchor constant:14.0],
        [self.progressIndicator.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [self.progressIndicator.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-24.0],

        [self.statusLabel.topAnchor constraintEqualToAnchor:self.progressIndicator.bottomAnchor constant:5.0],
        [self.statusLabel.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [self.statusLabel.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-24.0],

        [summaries.topAnchor constraintEqualToAnchor:self.statusLabel.bottomAnchor constant:13.0],
        [summaries.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [summaries.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-24.0],

        [scroll.topAnchor constraintEqualToAnchor:summaries.bottomAnchor constant:10.0],
        [scroll.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [scroll.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-24.0],
        [scroll.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-22.0],
    ]];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView
{
    static_cast<void>(tableView);
    return static_cast<NSInteger>(self.rows.count);
}

- (NSView*)tableView:(NSTableView*)tableView
    viewForTableColumn:(NSTableColumn*)tableColumn
                   row:(NSInteger)rowIndex
{
    NSString* identifier = tableColumn.identifier;
    NSTextField* field = [tableView makeViewWithIdentifier:identifier owner:self];
    if (field == nil) {
        field = labelWithText(@"");
        field.identifier = identifier;
        field.lineBreakMode = NSLineBreakByTruncatingTail;
    }

    PWBenchmarkRow* row = self.rows[static_cast<NSUInteger>(rowIndex)];
    if ([identifier isEqualToString:@"material"]) {
        field.stringValue = row.material;
    } else if ([identifier isEqualToString:@"resolution"]) {
        field.stringValue = [NSString stringWithFormat:@"%lu × %lu",
            static_cast<unsigned long>(row.resolution),
            static_cast<unsigned long>(row.resolution)];
    } else if ([identifier isEqualToString:@"single"]) {
        field.stringValue = [NSString stringWithFormat:@"%.2f ms", row.singleMilliseconds];
    } else if ([identifier isEqualToString:@"multi"]) {
        field.stringValue = [NSString stringWithFormat:@"%.2f ms (%lu)",
            row.multiMilliseconds,
            static_cast<unsigned long>(row.multiWorkers)];
    } else if ([identifier isEqualToString:@"speedup"]) {
        field.stringValue = [NSString stringWithFormat:@"%.2f×", row.speedup];
    } else if ([identifier isEqualToString:@"singleThroughput"]) {
        field.stringValue = [NSString stringWithFormat:@"%.2f", row.singleThroughput];
    } else if ([identifier isEqualToString:@"multiThroughput"]) {
        field.stringValue = [NSString stringWithFormat:@"%.2f", row.multiThroughput];
    } else if ([identifier isEqualToString:@"match"]) {
        field.stringValue = row.identical ? @"Yes" : @"NO";
        field.textColor = row.identical ? NSColor.systemGreenColor : NSColor.systemRedColor;
        field.font = [NSFont systemFontOfSize:NSFont.systemFontSize weight:NSFontWeightSemibold];
    }
    return field;
}

- (void)updateSummaries
{
    const NSUInteger count = self.rows.count;
    self.completedSummary.stringValue = [NSString stringWithFormat:@"Completed: %lu / %lu",
        static_cast<unsigned long>(count),
        static_cast<unsigned long>(showcases.size() * resolutions.size())];
    if (count == 0) {
        self.averageSummary.stringValue = @"Average speed-up: —";
        self.bestSummary.stringValue = @"Best speed-up: —";
        self.matchSummary.stringValue = @"Exact matches: 0 / 0";
        return;
    }

    double speedupTotal = 0.0;
    double bestSpeedup = 0.0;
    NSUInteger matches = 0;
    for (PWBenchmarkRow* row in self.rows) {
        speedupTotal += row.speedup;
        bestSpeedup = std::max(bestSpeedup, row.speedup);
        if (row.identical) {
            ++matches;
        }
    }
    self.averageSummary.stringValue = [NSString stringWithFormat:@"Average speed-up: %.2f×",
        speedupTotal / static_cast<double>(count)];
    self.bestSummary.stringValue = [NSString stringWithFormat:@"Best speed-up: %.2f×", bestSpeedup];
    self.matchSummary.stringValue = [NSString stringWithFormat:@"Exact matches: %lu / %lu",
        static_cast<unsigned long>(matches),
        static_cast<unsigned long>(count)];
    self.matchSummary.textColor = matches == count
        ? NSColor.systemGreenColor
        : NSColor.systemRedColor;
}

- (void)setRunControlsEnabled:(BOOL)enabled
{
    self.startButton.enabled = enabled;
    self.outputPopup.enabled = enabled;
    self.cancelButton.enabled = !enabled;
}

- (void)reportError:(NSString*)message
{
    running_ = NO;
    cancellation_.reset();
    [self setRunControlsEnabled:YES];
    self.statusLabel.stringValue = message;
    self.statusLabel.textColor = NSColor.systemRedColor;
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Benchmark could not continue";
    alert.informativeText = message;
    [alert beginSheetModalForWindow:self.window completionHandler:nil];
}

- (void)startBenchmark:(id)sender
{
    static_cast<void>(sender);
    if (running_) {
        return;
    }

    selectedOutput_ = static_cast<paperweight::MaterialOutput>(self.outputPopup.indexOfSelectedItem);
    const NSUInteger processorCount = NSProcessInfo.processInfo.processorCount;
    multiWorkerCount_ = std::min<NSUInteger>(std::max<NSUInteger>(processorCount, 1), 32);
    [self.rows removeAllObjects];
    [self.resultsTable reloadData];
    [self updateSummaries];
    self.progressIndicator.doubleValue = 0.0;
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
    self.csvCopyButton.enabled = NO;
    self.saveButton.enabled = NO;
    running_ = YES;
    [self setRunControlsEnabled:NO];

    NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
    dateFormatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    dateFormatter.dateFormat = @"yyyy-MM-dd'T'HH:mm:ssZZZZZ";
    NSString* memory = [NSByteCountFormatter stringFromByteCount:
        static_cast<long long>(NSProcessInfo.processInfo.physicalMemory)
                                               countStyle:NSByteCountFormatterCountStyleMemory];
    self.runMetadata = [NSString stringWithFormat:
        @"# Paperweight %@ benchmark\n"
         "# Timestamp: %@\n"
         "# Machine: %@\n"
         "# Architecture: %@\n"
         "# macOS: %@\n"
         "# Logical processors: %lu\n"
         "# Memory: %@\n"
         "# Output: %@\n",
        [NSString stringWithUTF8String:paperweight::versionString().data()],
        [dateFormatter stringFromDate:NSDate.date],
        machineModel(),
        architectureName(),
        NSProcessInfo.processInfo.operatingSystemVersionString,
        static_cast<unsigned long>(processorCount),
        memory,
        outputDisplayName(selectedOutput_)];

    std::vector<std::pair<std::string, std::string>> materialFiles;
    materialFiles.reserve(showcases.size());
    for (const auto& showcase : showcases) {
        NSString* resource = [NSString stringWithUTF8String:showcase.resource];
        NSString* path = [NSBundle.mainBundle pathForResource:resource
                                                      ofType:@"pmat"
                                                 inDirectory:@"Showcases"];
        if (path == nil) {
            [self reportError:[NSString stringWithFormat:@"The bundled material %@.pmat is missing.", resource]];
            return;
        }
        materialFiles.emplace_back(showcase.name, path.UTF8String);
    }

    auto cancellation = std::make_shared<std::atomic_bool>(false);
    cancellation_ = cancellation;
    const auto output = selectedOutput_;
    const std::uint32_t multiWorkers = static_cast<std::uint32_t>(multiWorkerCount_);
    __weak BenchmarkWindowController* weakSelf = self;
    dispatch_async(benchmarkQueue_, ^{
        @autoreleasepool {
            for (const auto& [materialName, path] : materialFiles) {
                if (cancellation->load(std::memory_order_relaxed)) {
                    break;
                }

                NSData* data = [NSData dataWithContentsOfFile:
                    [NSString stringWithUTF8String:path.c_str()]];
                if (data == nil) {
                    NSString* message = [NSString stringWithFormat:@"Could not read %s.", path.c_str()];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [weakSelf reportError:message];
                    });
                    return;
                }
                const std::string text(
                    static_cast<const char*>(data.bytes),
                    static_cast<std::size_t>(data.length));
                auto parseResult = paperweight::parsePmat(text);
                const auto* parsedMaterial = std::get_if<paperweight::Material>(&parseResult);
                if (parsedMaterial == nullptr) {
                    NSString* message = [NSString stringWithFormat:@"Could not parse %s.", path.c_str()];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [weakSelf reportError:message];
                    });
                    return;
                }
                paperweight::Material material = *parsedMaterial;
                auto compilation = paperweight::compileMaterialGraph(material);
                const auto* compiledGraph = std::get_if<paperweight::MaterialGraph>(&compilation);
                if (compiledGraph == nullptr) {
                    NSString* message = [NSString stringWithFormat:@"Could not compile %s.", materialName.c_str()];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [weakSelf reportError:message];
                    });
                    return;
                }

                paperweight::GenerationRequest request;
                request.material = material;
                request.output = output;
                request.graph = *compiledGraph;
                for (const auto resolution : resolutions) {
                    if (cancellation->load(std::memory_order_relaxed)) {
                        break;
                    }
                    request.width = resolution;
                    request.height = resolution;
                    request.workerCount = 1;

                    NSString* serialStatus = [NSString stringWithFormat:
                        @"%s — %u × %u — measuring 1 worker…",
                        materialName.c_str(),
                        resolution,
                        resolution];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        weakSelf.statusLabel.stringValue = serialStatus;
                    });

                    const auto serialStart = Clock::now();
                    auto serialResult = paperweight::generate(request, [cancellation]() {
                        return cancellation->load(std::memory_order_relaxed);
                    });
                    const auto serialFinish = Clock::now();
                    if (cancellation->load(std::memory_order_relaxed)) {
                        break;
                    }
                    auto* serialImage = std::get_if<paperweight::Image>(&serialResult);
                    if (serialImage == nullptr) {
                        NSString* message = [NSString stringWithFormat:
                            @"Generation failed for %s at %u × %u (1 worker).",
                            materialName.c_str(), resolution, resolution];
                        dispatch_async(dispatch_get_main_queue(), ^{
                            [weakSelf reportError:message];
                        });
                        return;
                    }
                    dispatch_async(dispatch_get_main_queue(), ^{
                        weakSelf.progressIndicator.doubleValue += 1.0;
                    });

                    request.workerCount = multiWorkers;
                    NSString* parallelStatus = [NSString stringWithFormat:
                        @"%s — %u × %u — measuring %u workers…",
                        materialName.c_str(),
                        resolution,
                        resolution,
                        multiWorkers];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        weakSelf.statusLabel.stringValue = parallelStatus;
                    });

                    const auto parallelStart = Clock::now();
                    auto parallelResult = paperweight::generate(request, [cancellation]() {
                        return cancellation->load(std::memory_order_relaxed);
                    });
                    const auto parallelFinish = Clock::now();
                    if (cancellation->load(std::memory_order_relaxed)) {
                        break;
                    }
                    auto* parallelImage = std::get_if<paperweight::Image>(&parallelResult);
                    if (parallelImage == nullptr) {
                        NSString* message = [NSString stringWithFormat:
                            @"Generation failed for %s at %u × %u (%u workers).",
                            materialName.c_str(), resolution, resolution, multiWorkers];
                        dispatch_async(dispatch_get_main_queue(), ^{
                            [weakSelf reportError:message];
                        });
                        return;
                    }

                    const double serialMilliseconds =
                        std::chrono::duration<double, std::milli>(serialFinish - serialStart).count();
                    const double parallelMilliseconds =
                        std::chrono::duration<double, std::milli>(parallelFinish - parallelStart).count();
                    const double megapixels = static_cast<double>(resolution) * resolution / 1'000'000.0;
                    const double serialThroughput = megapixels / (serialMilliseconds / 1000.0);
                    const double parallelThroughput = megapixels / (parallelMilliseconds / 1000.0);
                    const double speedup = serialMilliseconds / parallelMilliseconds;
                    const bool identical = std::equal(
                        serialImage->pixels().begin(),
                        serialImage->pixels().end(),
                        parallelImage->pixels().begin(),
                        parallelImage->pixels().end());
                    const auto resultChecksum = checksum(serialImage->pixels());
                    NSString* displayName = [NSString stringWithUTF8String:materialName.c_str()];

                    dispatch_async(dispatch_get_main_queue(), ^{
                        BenchmarkWindowController* strongSelf = weakSelf;
                        if (strongSelf == nil) {
                            return;
                        }
                        PWBenchmarkRow* row = [[PWBenchmarkRow alloc] init];
                        row.material = displayName;
                        row.resolution = resolution;
                        row.multiWorkers = multiWorkers;
                        row.singleMilliseconds = serialMilliseconds;
                        row.multiMilliseconds = parallelMilliseconds;
                        row.speedup = speedup;
                        row.singleThroughput = serialThroughput;
                        row.multiThroughput = parallelThroughput;
                        row.checksum = resultChecksum;
                        row.identical = identical;
                        [strongSelf.rows addObject:row];
                        [strongSelf.resultsTable reloadData];
                        [strongSelf.resultsTable scrollRowToVisible:
                            static_cast<NSInteger>(strongSelf.rows.count - 1)];
                        strongSelf.progressIndicator.doubleValue += 1.0;
                        strongSelf.csvCopyButton.enabled = YES;
                        strongSelf.saveButton.enabled = YES;
                        [strongSelf updateSummaries];
                    });
                }
            }

            const bool cancelled = cancellation->load(std::memory_order_relaxed);
            dispatch_async(dispatch_get_main_queue(), ^{
                BenchmarkWindowController* strongSelf = weakSelf;
                if (strongSelf == nil || strongSelf->cancellation_ != cancellation) {
                    return;
                }
                strongSelf->running_ = NO;
                strongSelf->cancellation_.reset();
                [strongSelf setRunControlsEnabled:YES];
                BOOL allMatch = YES;
                for (PWBenchmarkRow* row in strongSelf.rows) {
                    allMatch = allMatch && row.identical;
                }
                if (cancelled) {
                    strongSelf.statusLabel.textColor = NSColor.secondaryLabelColor;
                    strongSelf.statusLabel.stringValue = [NSString stringWithFormat:
                        @"Cancelled after %lu completed comparisons.",
                        static_cast<unsigned long>(strongSelf.rows.count)];
                } else if (allMatch) {
                    strongSelf.statusLabel.textColor = NSColor.systemGreenColor;
                    strongSelf.statusLabel.stringValue = [NSString stringWithFormat:
                        @"Complete: all %lu comparisons are byte-identical using 1 and %lu workers.",
                        static_cast<unsigned long>(strongSelf.rows.count),
                        static_cast<unsigned long>(strongSelf->multiWorkerCount_)];
                } else {
                    strongSelf.statusLabel.textColor = NSColor.systemRedColor;
                    strongSelf.statusLabel.stringValue =
                        @"Complete, but at least one multi-worker result differed. See the Exact column.";
                }
            });
        }
    });
}

- (void)cancelBenchmark:(id)sender
{
    static_cast<void>(sender);
    if (cancellation_) {
        cancellation_->store(true, std::memory_order_relaxed);
        self.cancelButton.enabled = NO;
        self.statusLabel.stringValue = @"Cancelling the current generation…";
    }
}

- (NSString*)csvText
{
    NSMutableString* csv = [NSMutableString stringWithString:
        self.runMetadata != nil ? self.runMetadata : @""];
    [csv appendString:
        @"material,output,resolution,single_workers,multi_workers,single_ms,multi_ms,speedup,"
         "single_megapixels_per_second,multi_megapixels_per_second,checksum,byte_identical\n"];
    NSString* output = outputDisplayName(selectedOutput_);
    for (PWBenchmarkRow* row in self.rows) {
        [csv appendFormat:@"%@,%@,%lu,1,%lu,%.6f,%.6f,%.6f,%.6f,%.6f,%llu,%@\n",
            csvEscaped(row.material),
            csvEscaped(output),
            static_cast<unsigned long>(row.resolution),
            static_cast<unsigned long>(row.multiWorkers),
            row.singleMilliseconds,
            row.multiMilliseconds,
            row.speedup,
            row.singleThroughput,
            row.multiThroughput,
            row.checksum,
            row.identical ? @"true" : @"false"];
    }
    return csv;
}

- (void)copyCsv:(id)sender
{
    static_cast<void>(sender);
    NSPasteboard* pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard clearContents];
    [pasteboard setString:[self csvText] forType:NSPasteboardTypeString];
    self.statusLabel.stringValue = @"Benchmark results copied as CSV.";
}

- (void)saveCsv:(id)sender
{
    static_cast<void>(sender);
    NSSavePanel* panel = [NSSavePanel savePanel];
    panel.title = @"Save Benchmark Results";
    panel.nameFieldStringValue = @"Paperweight-benchmark.csv";
    panel.allowedFileTypes = @[@"csv"];
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) {
            return;
        }
        NSError* error = nil;
        if (![[self csvText] writeToURL:panel.URL
                            atomically:YES
                              encoding:NSUTF8StringEncoding
                                 error:&error]) {
            [self reportError:error.localizedDescription != nil
                    ? error.localizedDescription
                    : @"The CSV file could not be saved."];
            return;
        }
        self.statusLabel.stringValue = @"Benchmark results saved.";
    }];
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    static_cast<void>(sender);
    if (cancellation_) {
        cancellation_->store(true, std::memory_order_relaxed);
    }
    return YES;
}

@end
