#import "MaterialLibraryWindowController.hpp"

#include "ImageBridge.hpp"

#include <paperweight/generator.hpp>
#include <paperweight/material_library.hpp>
#include <paperweight/pmat.hpp>
#include <paperweight/pwlib.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

@interface PWMaterialLibraryRow : NSObject
@property(nonatomic, strong) NSURL* url;
@property(nonatomic, copy) NSString* relativePath;
@property(nonatomic, copy) NSString* displayName;
@property(nonatomic, copy) NSString* category;
@property(nonatomic, copy) NSString* searchText;
@property(nonatomic, copy) NSString* status;
@property(nonatomic, copy) NSString* sourceSize;
@property(nonatomic, copy) NSString* packedSize;
@property(nonatomic, copy) NSString* packedSizeDetail;
@property(nonatomic, strong) NSImage* thumbnail;
@property(nonatomic) BOOL ready;
@end

@implementation PWMaterialLibraryRow
@end

namespace {

constexpr std::uint32_t thumbnailResolution = 96;

NSTextField* label(NSString* text)
{
    auto* result = [NSTextField labelWithString:text];
    result.lineBreakMode = NSLineBreakByTruncatingTail;
    return result;
}

NSString* utf8(const std::string& value)
{
    auto* result = [NSString stringWithUTF8String:value.c_str()];
    return result != nil ? result : @"";
}

NSString* byteCount(std::uint64_t bytes)
{
    constexpr double kibibyte = 1024.0;
    constexpr double mebibyte = kibibyte * 1024.0;
    if (bytes < 1024) {
        return [NSString stringWithFormat:@"%llu B", static_cast<unsigned long long>(bytes)];
    }
    if (static_cast<double>(bytes) < mebibyte) {
        return [NSString stringWithFormat:@"%.1f KiB", static_cast<double>(bytes) / kibibyte];
    }
    return [NSString stringWithFormat:@"%.1f MiB", static_cast<double>(bytes) / mebibyte];
}

std::string slugForName(NSString* name)
{
    const char* value = name.lowercaseString.UTF8String;
    std::string result;
    bool separator = false;
    if (value != nullptr) {
        for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(value);
             *cursor != 0; ++cursor) {
            if (std::isalnum(*cursor) != 0) {
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
    while (!result.empty() && result.back() == '-') {
        result.pop_back();
    }
    return result.empty() ? "material" : result;
}

} // namespace

@interface MaterialLibraryWindowController ()
    <NSWindowDelegate, NSTableViewDataSource, NSTableViewDelegate, NSSearchFieldDelegate>
@property(nonatomic, copy) PWOpenMaterialHandler openMaterialHandler;
@property(nonatomic, copy) PWRelocateMaterialHandler relocationHandler;
@property(nonatomic, copy) PWCanRewriteMaterialHandler canRewriteHandler;
@property(nonatomic, strong) NSURL* workingFolderURL;
@property(nonatomic, strong) NSTextField* folderLabel;
@property(nonatomic, strong) NSTextField* summaryLabel;
@property(nonatomic, strong) NSSearchField* searchField;
@property(nonatomic, strong) NSPopUpButton* categoryPopup;
@property(nonatomic, strong) NSTableView* tableView;
@property(nonatomic, strong) NSArray<PWMaterialLibraryRow*>* allRows;
@property(nonatomic, strong) NSArray<PWMaterialLibraryRow*>* visibleRows;
@property(nonatomic, strong) NSButton* openButton;
@property(nonatomic, strong) NSButton* duplicateButton;
@property(nonatomic, strong) NSButton* renameButton;
@property(nonatomic, strong) NSButton* moveButton;
@property(nonatomic, strong) NSButton* revealButton;
@property(nonatomic, strong) NSButton* exportPackButton;
@end

@implementation MaterialLibraryWindowController {
    dispatch_queue_t thumbnailQueue_;
    std::uint64_t refreshRevision_;
    std::shared_ptr<std::atomic_bool> thumbnailCancellation_;
}

- (instancetype)initWithOpenMaterialHandler:(PWOpenMaterialHandler)handler
                           relocationHandler:(PWRelocateMaterialHandler)relocationHandler
                           canRewriteHandler:(PWCanRewriteMaterialHandler)canRewriteHandler
{
    const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    auto* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1100, 620)
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    self = [super initWithWindow:window];
    if (self) {
        _openMaterialHandler = [handler copy];
        _relocationHandler = [relocationHandler copy];
        _canRewriteHandler = [canRewriteHandler copy];
        _allRows = @[];
        _visibleRows = @[];
        thumbnailQueue_ = dispatch_queue_create(
            "org.solen-music.paperweight.library-thumbnails", DISPATCH_QUEUE_SERIAL);
        window.title = @"Material Library — Paperweight";
        window.minSize = NSMakeSize(900, 460);
        window.delegate = self;
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(applicationDidBecomeActive:)
                                                   name:NSApplicationDidBecomeActiveNotification
                                                 object:nil];
        NSString* remembered = [NSUserDefaults.standardUserDefaults
            stringForKey:@"materialLibraryWorkingFolder"];
        if (remembered.length != 0) {
            _workingFolderURL = [NSURL fileURLWithPath:remembered isDirectory:YES];
        }
        [self buildInterface];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)showMaterialLibrary
{
    [self showWindow:nil];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [self refreshLibrary:nil];
}

- (void)windowDidBecomeKey:(NSNotification*)notification
{
    static_cast<void>(notification);
    [self refreshLibrary:nil];
}

- (void)applicationDidBecomeActive:(NSNotification*)notification
{
    static_cast<void>(notification);
    if (self.window.isVisible) {
        [self refreshLibrary:nil];
    }
}

- (void)windowWillClose:(NSNotification*)notification
{
    static_cast<void>(notification);
    if (thumbnailCancellation_) {
        thumbnailCancellation_->store(true, std::memory_order_relaxed);
    }
}

- (void)buildInterface
{
    auto* content = [[NSView alloc] initWithFrame:NSZeroRect];
    self.window.contentView = content;

    auto* title = label(@"Working Folder");
    title.font = [NSFont systemFontOfSize:18.0 weight:NSFontWeightSemibold];
    self.folderLabel = label(@"No folder chosen");
    self.folderLabel.textColor = NSColor.secondaryLabelColor;
    auto* chooseButton = [NSButton buttonWithTitle:@"Choose Folder…"
                                             target:self
                                             action:@selector(chooseWorkingFolder:)];
    auto* refreshButton = [NSButton buttonWithTitle:@"Refresh"
                                              target:self
                                              action:@selector(refreshLibrary:)];
    auto* folderButtons = [NSStackView stackViewWithViews:@[chooseButton, refreshButton]];
    folderButtons.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    folderButtons.spacing = 8.0;

    self.searchField = [[NSSearchField alloc] initWithFrame:NSZeroRect];
    self.searchField.placeholderString = @"Search names, tags, categories, UIDs or paths";
    self.searchField.delegate = self;
    self.searchField.target = self;
    self.searchField.action = @selector(filterChanged:);
    self.categoryPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.categoryPopup addItemWithTitle:@"All Categories"];
    self.categoryPopup.target = self;
    self.categoryPopup.action = @selector(filterChanged:);
    auto* filters = [NSStackView stackViewWithViews:@[self.searchField, self.categoryPopup]];
    filters.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    filters.spacing = 8.0;

    self.tableView = [[NSTableView alloc] initWithFrame:NSZeroRect];
    self.tableView.delegate = self;
    self.tableView.dataSource = self;
    self.tableView.rowHeight = 54.0;
    self.tableView.usesAlternatingRowBackgroundColors = YES;
    self.tableView.allowsEmptySelection = YES;
    self.tableView.allowsMultipleSelection = YES;
    self.tableView.doubleAction = @selector(openSelected:);
    self.tableView.target = self;
    const NSArray* columns = @[
        @[@"preview", @"", @54], @[@"name", @"Name", @190],
        @[@"category", @"Category", @100], @[@"path", @"Location", @170],
        @[@"source", @"PMAT", @80], @[@"packed", @"Pack payload", @105],
        @[@"status", @"Status", @185],
    ];
    for (NSArray* specification in columns) {
        auto* column = [[NSTableColumn alloc] initWithIdentifier:specification[0]];
        column.title = specification[1];
        column.width = [specification[2] doubleValue];
        column.resizingMask = NSTableColumnUserResizingMask;
        [self.tableView addTableColumn:column];
    }
    auto* scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    scroll.documentView = self.tableView;
    scroll.hasVerticalScroller = YES;
    scroll.borderType = NSBezelBorder;

    auto* newButton = [NSButton buttonWithTitle:@"New Material…"
                                          target:self action:@selector(createMaterial:)];
    self.exportPackButton = [NSButton buttonWithTitle:@"Export Pack…"
                                                target:self action:@selector(exportPack:)];
    self.openButton = [NSButton buttonWithTitle:@"Open"
                                          target:self action:@selector(openSelected:)];
    self.duplicateButton = [NSButton buttonWithTitle:@"Duplicate…"
                                               target:self action:@selector(duplicateSelected:)];
    self.renameButton = [NSButton buttonWithTitle:@"Rename…"
                                            target:self action:@selector(renameSelected:)];
    self.moveButton = [NSButton buttonWithTitle:@"Move…"
                                          target:self action:@selector(moveSelected:)];
    self.revealButton = [NSButton buttonWithTitle:@"Reveal"
                                            target:self action:@selector(revealSelected:)];
    auto* actions = [NSStackView stackViewWithViews:@[
        newButton, self.exportPackButton, self.openButton, self.duplicateButton, self.renameButton,
        self.moveButton, self.revealButton,
    ]];
    actions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    actions.spacing = 8.0;
    self.summaryLabel = label(@"Choose a working folder to begin.");
    self.summaryLabel.textColor = NSColor.secondaryLabelColor;
    self.summaryLabel.maximumNumberOfLines = 2;

    for (NSView* view in @[title, self.folderLabel, folderButtons, filters, scroll, actions,
                           self.summaryLabel]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [content addSubview:view];
    }
    [NSLayoutConstraint activateConstraints:@[
        [title.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:20.0],
        [title.topAnchor constraintEqualToAnchor:content.topAnchor constant:18.0],
        [folderButtons.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-20.0],
        [folderButtons.centerYAnchor constraintEqualToAnchor:title.centerYAnchor],
        [self.folderLabel.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [self.folderLabel.trailingAnchor constraintEqualToAnchor:folderButtons.leadingAnchor constant:-12.0],
        [self.folderLabel.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:4.0],
        [filters.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [filters.trailingAnchor constraintEqualToAnchor:folderButtons.trailingAnchor],
        [filters.topAnchor constraintEqualToAnchor:self.folderLabel.bottomAnchor constant:14.0],
        [self.categoryPopup.widthAnchor constraintEqualToConstant:180.0],
        [scroll.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [scroll.trailingAnchor constraintEqualToAnchor:folderButtons.trailingAnchor],
        [scroll.topAnchor constraintEqualToAnchor:filters.bottomAnchor constant:10.0],
        [scroll.bottomAnchor constraintEqualToAnchor:actions.topAnchor constant:-12.0],
        [actions.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [actions.bottomAnchor constraintEqualToAnchor:self.summaryLabel.topAnchor constant:-8.0],
        [self.summaryLabel.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [self.summaryLabel.trailingAnchor constraintEqualToAnchor:folderButtons.trailingAnchor],
        [self.summaryLabel.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-16.0],
    ]];
    [self updateSelectionButtons];
    [self updateFolderLabel];
}

- (void)updateFolderLabel
{
    self.folderLabel.stringValue = self.workingFolderURL != nil
        ? self.workingFolderURL.path
        : @"No folder chosen";
}

- (void)chooseWorkingFolder:(id)sender
{
    static_cast<void>(sender);
    auto* panel = [NSOpenPanel openPanel];
    panel.title = @"Choose Paperweight Material Library Folder";
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = NO;
    panel.canCreateDirectories = YES;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] != NSModalResponseOK) {
        return;
    }
    self.workingFolderURL = panel.URL.URLByStandardizingPath;
    [NSUserDefaults.standardUserDefaults setObject:self.workingFolderURL.path
                                             forKey:@"materialLibraryWorkingFolder"];
    [self updateFolderLabel];
    [self refreshLibrary:nil];
}

- (NSArray<NSURL*>*)materialFiles
{
    if (self.workingFolderURL == nil) {
        return @[];
    }
    NSDirectoryEnumerator<NSURL*>* enumerator = [NSFileManager.defaultManager
        enumeratorAtURL:self.workingFolderURL
        includingPropertiesForKeys:@[NSURLIsRegularFileKey]
        options:NSDirectoryEnumerationSkipsHiddenFiles
        errorHandler:^BOOL(NSURL* url, NSError* error) {
            static_cast<void>(url);
            static_cast<void>(error);
            return YES;
        }];
    auto* files = [NSMutableArray array];
    for (NSURL* url in enumerator) {
        if ([url.pathExtension caseInsensitiveCompare:@"pmat"] == NSOrderedSame) {
            [files addObject:url];
        }
    }
    [files sortUsingComparator:^NSComparisonResult(NSURL* left, NSURL* right) {
        return [left.path compare:right.path options:NSCaseInsensitiveSearch];
    }];
    return files;
}

- (void)refreshLibrary:(id)sender
{
    static_cast<void>(sender);
    ++refreshRevision_;
    const auto revision = refreshRevision_;
    if (thumbnailCancellation_) {
        thumbnailCancellation_->store(true, std::memory_order_relaxed);
    }
    thumbnailCancellation_ = std::make_shared<std::atomic_bool>(false);
    const auto cancellation = thumbnailCancellation_;
    if (self.workingFolderURL == nil) {
        self.allRows = @[];
        [self filterChanged:nil];
        self.summaryLabel.stringValue = @"Choose a working folder to begin.";
        return;
    }

    NSArray<NSURL*>* urls = [self materialFiles];
    std::vector<paperweight::MaterialLibrarySource> sources;
    sources.reserve(urls.count);
    std::unordered_map<std::string, NSURL*> urlsByPath;
    std::unordered_map<std::string, std::uint64_t> sourceSizes;
    std::uint64_t totalSourceBytes{};
    const std::string root = self.workingFolderURL.path.UTF8String;
    for (NSURL* url in urls) {
        std::string fullPath = url.path.UTF8String;
        std::string relative = fullPath.size() > root.size() + 1
            ? fullPath.substr(root.size() + 1)
            : fullPath;
        NSError* error = nil;
        NSString* contents = [NSString stringWithContentsOfURL:url
                                                       encoding:NSUTF8StringEncoding
                                                          error:&error];
        const char* utf8Contents = contents.UTF8String;
        std::string sourceText = utf8Contents != nullptr ? utf8Contents : "";
        sourceSizes.emplace(relative, sourceText.size());
        totalSourceBytes += sourceText.size();
        sources.push_back({relative, std::move(sourceText)});
        urlsByPath.emplace(relative, url);
    }
    const auto index = paperweight::indexMaterialLibrary(sources);
    std::unordered_map<std::string, std::string> statuses;
    for (const auto& diagnostic : index.diagnostics()) {
        auto& status = statuses[diagnostic.path];
        if (!status.empty()) {
            status += "; ";
        }
        if (diagnostic.line != 0) {
            status += "line " + std::to_string(diagnostic.line) + ": ";
        }
        status += diagnostic.message;
    }

    struct PackedSize {
        std::uint64_t stored{};
        std::uint64_t canonical{};
        paperweight::PwlibStorageMode mode{paperweight::PwlibStorageMode::raw};
    };
    std::unordered_map<std::string, PackedSize> packedSizes;
    std::uint64_t storedPayloadBytes{};
    std::uint64_t canonicalPayloadBytes{};
    std::size_t rleCount{};
    std::optional<std::size_t> completePackBytes;
    if (index.diagnostics().empty() && !sources.empty()) {
        auto packResult = paperweight::packPwlib(sources);
        if (auto* bytes = std::get_if<std::vector<std::uint8_t>>(&packResult)) {
            auto readResult = paperweight::readPwlib(*bytes);
            if (auto* library = std::get_if<paperweight::PackedMaterialLibrary>(&readResult)) {
                completePackBytes = bytes->size();
                for (const auto& entry : library->entries()) {
                    packedSizes.emplace(
                        std::string(entry.uid),
                        PackedSize{entry.storedSize, entry.uncompressedSize, entry.storageMode});
                    storedPayloadBytes += entry.storedSize;
                    canonicalPayloadBytes += entry.uncompressedSize;
                    if (entry.storageMode == paperweight::PwlibStorageMode::rle) {
                        ++rleCount;
                    }
                }
            }
        }
    }

    auto* rows = [NSMutableArray arrayWithCapacity:urls.count];
    std::unordered_map<std::string, PWMaterialLibraryRow*> rowsByPath;
    for (const auto& entry : index.entries()) {
        auto* row = [[PWMaterialLibraryRow alloc] init];
        row.url = urlsByPath[entry.path];
        row.relativePath = utf8(entry.path);
        const auto metadata = entry.material.metadata.value_or(paperweight::MaterialMetadata{});
        row.displayName = metadata.name.empty() ? row.url.lastPathComponent : utf8(metadata.name);
        row.category = metadata.category.empty() ? @"Uncategorised" : utf8(metadata.category);
        row.ready = entry.libraryReady;
        row.status = entry.libraryReady ? @"Ready" : utf8(statuses[entry.path]);
        row.sourceSize = byteCount(sourceSizes[entry.path]);
        if (const auto packed = packedSizes.find(metadata.uid); packed != packedSizes.end()) {
            row.packedSize = [NSString stringWithFormat:@"%@ %@",
                byteCount(packed->second.stored),
                packed->second.mode == paperweight::PwlibStorageMode::rle ? @"RLE" : @"raw"];
            row.packedSizeDetail = [NSString stringWithFormat:
                @"%@ stored from %@ canonical PMAT payload",
                byteCount(packed->second.stored), byteCount(packed->second.canonical)];
        } else {
            row.packedSize = @"—";
            row.packedSizeDetail = entry.libraryReady
                ? @"Fix other library problems to calculate the complete pack."
                : @"This material is not ready for packing.";
        }
        std::string search = entry.path + " " + metadata.uid + " " + metadata.name + " " +
            metadata.category + " " + metadata.description;
        for (const auto& tag : metadata.tags) {
            search += " " + tag;
        }
        row.searchText = utf8(search).lowercaseString;
        [rows addObject:row];
        rowsByPath.emplace(entry.path, row);

        const paperweight::Material material = entry.material;
        __weak MaterialLibraryWindowController* weakSelf = self;
        __weak PWMaterialLibraryRow* weakRow = row;
        dispatch_async(thumbnailQueue_, ^{
            if (cancellation->load(std::memory_order_relaxed)) {
                return;
            }
            auto generated = paperweight::generate(
                {material, thumbnailResolution, thumbnailResolution,
                 paperweight::MaterialOutput::colour, std::nullopt, std::nullopt},
                [cancellation]() {
                    return cancellation->load(std::memory_order_relaxed);
                });
            auto image = std::make_shared<std::optional<paperweight::Image>>();
            if (auto* value = std::get_if<paperweight::Image>(&generated)) {
                *image = std::move(*value);
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                MaterialLibraryWindowController* strongSelf = weakSelf;
                PWMaterialLibraryRow* strongRow = weakRow;
                if (strongSelf == nil || strongRow == nil || revision != strongSelf->refreshRevision_ ||
                    cancellation->load(std::memory_order_relaxed) || !*image) {
                    return;
                }
                auto* representation = paperweight::macos::makeBitmapRepresentation(**image);
                if (representation != nil) {
                    auto* thumbnail = [[NSImage alloc] initWithSize:NSMakeSize(48.0, 48.0)];
                    [thumbnail addRepresentation:representation];
                    strongRow.thumbnail = thumbnail;
                    [strongSelf.tableView reloadDataForRowIndexes:
                        [NSIndexSet indexSetWithIndexesInRange:
                            NSMakeRange(0, strongSelf.visibleRows.count)]
                                                 columnIndexes:[NSIndexSet indexSetWithIndex:0]];
                }
            });
        });
    }
    for (const auto& source : sources) {
        if (rowsByPath.contains(source.path)) {
            continue;
        }
        auto* row = [[PWMaterialLibraryRow alloc] init];
        row.url = urlsByPath[source.path];
        row.relativePath = utf8(source.path);
        row.displayName = row.url.lastPathComponent;
        row.category = @"Invalid";
        row.ready = NO;
        row.status = utf8(statuses[source.path]);
        row.sourceSize = byteCount(sourceSizes[source.path]);
        row.packedSize = @"—";
        row.packedSizeDetail = @"This material is not ready for packing.";
        row.searchText = [NSString stringWithFormat:@"%@ %@", row.relativePath, row.status].lowercaseString;
        [rows addObject:row];
    }
    [rows sortUsingComparator:^NSComparisonResult(PWMaterialLibraryRow* left, PWMaterialLibraryRow* right) {
        return [left.relativePath compare:right.relativePath options:NSCaseInsensitiveSearch];
    }];
    self.allRows = rows;

    NSString* selectedCategory = self.categoryPopup.titleOfSelectedItem;
    auto* categories = [NSMutableSet set];
    for (PWMaterialLibraryRow* row in rows) {
        [categories addObject:row.category];
    }
    NSArray* sortedCategories = [categories.allObjects sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
    [self.categoryPopup removeAllItems];
    [self.categoryPopup addItemWithTitle:@"All Categories"];
    [self.categoryPopup addItemsWithTitles:sortedCategories];
    if ([self.categoryPopup itemWithTitle:selectedCategory] != nil) {
        [self.categoryPopup selectItemWithTitle:selectedCategory];
    }
    [self filterChanged:nil];
    const NSUInteger problemCount = index.diagnostics().size();
    if (completePackBytes) {
        self.summaryLabel.stringValue = [NSString stringWithFormat:
            @"%lu material file%@, %lu problem%@. PMAT total %@; PWLIB total %@ "
             "(%@ stored from %@ canonical payload, %zu RLE).",
            static_cast<unsigned long>(urls.count), urls.count == 1 ? @"" : @"s",
            static_cast<unsigned long>(problemCount), problemCount == 1 ? @"" : @"s",
            byteCount(totalSourceBytes), byteCount(*completePackBytes),
            byteCount(storedPayloadBytes), byteCount(canonicalPayloadBytes), rleCount];
    } else {
        self.summaryLabel.stringValue = [NSString stringWithFormat:
            @"%lu material file%@, %lu problem%@. PMAT total %@; fix library problems to preview PWLIB size.",
            static_cast<unsigned long>(urls.count), urls.count == 1 ? @"" : @"s",
            static_cast<unsigned long>(problemCount), problemCount == 1 ? @"" : @"s",
            byteCount(totalSourceBytes)];
    }
}

- (void)filterChanged:(id)sender
{
    static_cast<void>(sender);
    NSString* query = [self.searchField.stringValue
        stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet].lowercaseString;
    NSString* category = self.categoryPopup.titleOfSelectedItem;
    auto* visible = [NSMutableArray array];
    for (PWMaterialLibraryRow* row in self.allRows) {
        const BOOL queryMatches = query.length == 0 || [row.searchText containsString:query];
        const BOOL categoryMatches = [category isEqualToString:@"All Categories"] ||
            [row.category isEqualToString:category];
        if (queryMatches && categoryMatches) {
            [visible addObject:row];
        }
    }
    self.visibleRows = visible;
    [self.tableView reloadData];
    [self updateSelectionButtons];
}

- (void)controlTextDidChange:(NSNotification*)notification
{
    if (notification.object == self.searchField) {
        [self filterChanged:nil];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView
{
    static_cast<void>(tableView);
    return static_cast<NSInteger>(self.visibleRows.count);
}

- (NSView*)tableView:(NSTableView*)tableView
    viewForTableColumn:(NSTableColumn*)tableColumn
                  row:(NSInteger)rowIndex
{
    static_cast<void>(tableView);
    if (rowIndex < 0 || static_cast<NSUInteger>(rowIndex) >= self.visibleRows.count) {
        return nil;
    }
    PWMaterialLibraryRow* row = self.visibleRows[static_cast<NSUInteger>(rowIndex)];
    if ([tableColumn.identifier isEqualToString:@"preview"]) {
        auto* view = [[NSImageView alloc] initWithFrame:NSZeroRect];
        view.image = row.thumbnail;
        view.imageScaling = NSImageScaleProportionallyUpOrDown;
        return view;
    }
    NSString* value = @"";
    if ([tableColumn.identifier isEqualToString:@"name"]) value = row.displayName;
    else if ([tableColumn.identifier isEqualToString:@"category"]) value = row.category;
    else if ([tableColumn.identifier isEqualToString:@"path"]) value = row.relativePath;
    else if ([tableColumn.identifier isEqualToString:@"source"]) value = row.sourceSize;
    else if ([tableColumn.identifier isEqualToString:@"packed"]) value = row.packedSize;
    else if ([tableColumn.identifier isEqualToString:@"status"]) value = row.status;
    auto* view = label(value);
    if ([tableColumn.identifier isEqualToString:@"status"] && !row.ready) {
        view.textColor = NSColor.systemRedColor;
    }
    view.toolTip = [tableColumn.identifier isEqualToString:@"packed"]
        ? row.packedSizeDetail
        : value;
    return view;
}

- (void)tableViewSelectionDidChange:(NSNotification*)notification
{
    static_cast<void>(notification);
    [self updateSelectionButtons];
}

- (PWMaterialLibraryRow*)selectedRow
{
    const NSInteger index = self.tableView.selectedRow;
    return index >= 0 && static_cast<NSUInteger>(index) < self.visibleRows.count
        ? self.visibleRows[static_cast<NSUInteger>(index)]
        : nil;
}

- (void)updateSelectionButtons
{
    const BOOL singleSelection = self.tableView.selectedRowIndexes.count == 1;
    self.openButton.enabled = singleSelection;
    self.duplicateButton.enabled = singleSelection;
    self.renameButton.enabled = singleSelection;
    self.moveButton.enabled = singleSelection;
    self.revealButton.enabled = singleSelection;
    self.exportPackButton.enabled = self.workingFolderURL != nil && self.allRows.count != 0;
}

- (void)openSelected:(id)sender
{
    static_cast<void>(sender);
    PWMaterialLibraryRow* row = [self selectedRow];
    if (row != nil && self.openMaterialHandler != nil) {
        self.openMaterialHandler(row.url);
    }
}

- (void)revealSelected:(id)sender
{
    static_cast<void>(sender);
    PWMaterialLibraryRow* row = [self selectedRow];
    if (row != nil) {
        [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[row.url]];
    }
}

- (NSString*)promptWithTitle:(NSString*)title
                    message:(NSString*)message
                      value:(NSString*)value
{
    auto* field = [NSTextField textFieldWithString:value != nil ? value : @""];
    [field.widthAnchor constraintEqualToConstant:360.0].active = YES;
    auto* alert = [[NSAlert alloc] init];
    alert.messageText = title;
    alert.informativeText = message;
    alert.accessoryView = field;
    [alert addButtonWithTitle:@"Continue"];
    [alert addButtonWithTitle:@"Cancel"];
    if ([alert runModal] != NSAlertFirstButtonReturn) {
        return nil;
    }
    NSString* result = [field.stringValue stringByTrimmingCharactersInSet:
        NSCharacterSet.whitespaceAndNewlineCharacterSet];
    return result.length == 0 ? nil : result;
}

- (NSURL*)uniqueURLInFolder:(NSURL*)folder name:(NSString*)name
{
    const std::string base = slugForName(name);
    for (NSUInteger suffix = 1; suffix < 10000; ++suffix) {
        std::string filename = base;
        if (suffix > 1) {
            filename += "-" + std::to_string(suffix);
        }
        filename += ".pmat";
        NSURL* candidate = [folder URLByAppendingPathComponent:utf8(filename)];
        if (![NSFileManager.defaultManager fileExistsAtPath:candidate.path]) {
            return candidate;
        }
    }
    return nil;
}

- (BOOL)writeMaterial:(const paperweight::Material&)material toURL:(NSURL*)url
{
    const auto serialised = paperweight::serialisePmat(material);
    if (const auto* error = std::get_if<paperweight::SerialisationError>(&serialised)) {
        [self showOperationError:utf8(error->message)];
        return NO;
    }
    const auto& text = std::get<std::string>(serialised);
    NSString* contents = [[NSString alloc] initWithBytes:text.data()
                                                  length:text.size()
                                                encoding:NSUTF8StringEncoding];
    NSError* error = nil;
    if (contents == nil || ![contents writeToURL:url atomically:YES
                                        encoding:NSUTF8StringEncoding error:&error]) {
        [self showOperationError:error.localizedDescription != nil
            ? error.localizedDescription
            : @"The material could not be written."];
        return NO;
    }
    return YES;
}

- (std::optional<paperweight::Material>)readMaterial:(NSURL*)url
{
    NSError* error = nil;
    NSString* contents = [NSString stringWithContentsOfURL:url
                                                   encoding:NSUTF8StringEncoding error:&error];
    if (contents == nil || contents.UTF8String == nullptr) {
        [self showOperationError:error.localizedDescription != nil
            ? error.localizedDescription
            : @"The material could not be read."];
        return std::nullopt;
    }
    auto parsed = paperweight::parsePmat(contents.UTF8String);
    if (const auto* diagnostic = std::get_if<paperweight::ParseDiagnostic>(&parsed)) {
        [self showOperationError:[NSString stringWithFormat:@"Line %zu: %s",
            diagnostic->line, diagnostic->message.c_str()]];
        return std::nullopt;
    }
    return std::get<paperweight::Material>(std::move(parsed));
}

- (void)showOperationError:(NSString*)message
{
    auto* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = @"The library operation could not be completed";
    alert.informativeText = message;
    [alert runModal];
}

- (void)createMaterial:(id)sender
{
    [NSApp sendAction:@selector(showMaterialWizard:) to:NSApp.delegate from:sender];
}

- (NSArray<PWMaterialLibraryRow*>*)selectedRows
{
    auto* rows = [NSMutableArray array];
    [self.tableView.selectedRowIndexes enumerateIndexesUsingBlock:
        ^(NSUInteger index, BOOL* stop) {
            static_cast<void>(stop);
            if (index < self.visibleRows.count) {
                [rows addObject:self.visibleRows[index]];
            }
        }];
    return rows;
}

- (void)exportPack:(id)sender
{
    static_cast<void>(sender);
    NSArray<PWMaterialLibraryRow*>* rows = self.allRows;
    NSArray<PWMaterialLibraryRow*>* selected = [self selectedRows];
    if (selected.count != 0) {
        auto* scope = [[NSAlert alloc] init];
        scope.messageText = @"Export Portable Material Pack";
        scope.informativeText = [NSString stringWithFormat:
            @"Export the entire working folder, or only the %lu selected material%@?",
            static_cast<unsigned long>(selected.count), selected.count == 1 ? @"" : @"s"];
        [scope addButtonWithTitle:@"Entire Folder"];
        [scope addButtonWithTitle:[NSString stringWithFormat:
            @"Selected (%lu)", static_cast<unsigned long>(selected.count)]];
        [scope addButtonWithTitle:@"Cancel"];
        const auto response = [scope runModal];
        if (response == NSAlertThirdButtonReturn) {
            return;
        }
        if (response == NSAlertSecondButtonReturn) {
            rows = selected;
        }
    }
    if (rows.count == 0) {
        [self showOperationError:@"There are no material files to export."];
        return;
    }

    std::vector<paperweight::MaterialLibrarySource> sources;
    sources.reserve(rows.count);
    const std::string root = self.workingFolderURL.path.UTF8String;
    for (PWMaterialLibraryRow* row in rows) {
        NSError* readError = nil;
        NSString* contents = [NSString stringWithContentsOfURL:row.url
                                                       encoding:NSUTF8StringEncoding
                                                          error:&readError];
        if (contents == nil || contents.UTF8String == nullptr) {
            [self showOperationError:readError.localizedDescription != nil
                ? readError.localizedDescription
                : @"A selected material could not be read."];
            return;
        }
        std::string fullPath = row.url.path.UTF8String;
        std::string relative = fullPath.size() > root.size() + 1
            ? fullPath.substr(root.size() + 1)
            : fullPath;
        sources.push_back({relative, contents.UTF8String});
    }
    const auto packed = paperweight::packPwlib(sources);
    if (const auto* packError = std::get_if<paperweight::PwlibError>(&packed)) {
        [self showOperationError:utf8(packError->message)];
        return;
    }
    const auto& bytes = std::get<std::vector<std::uint8_t>>(packed);

    auto* panel = [NSSavePanel savePanel];
    panel.title = @"Export Portable Material Pack";
    panel.nameFieldStringValue = @"Paperweight-Library.pwlib";
    panel.allowedFileTypes = @[@"pwlib"];
    panel.canCreateDirectories = YES;
    if ([panel runModal] != NSModalResponseOK) {
        return;
    }
    NSData* data = [NSData dataWithBytes:bytes.data() length:bytes.size()];
    NSError* writeError = nil;
    if (![data writeToURL:panel.URL options:NSDataWritingAtomic error:&writeError]) {
        [self showOperationError:writeError.localizedDescription != nil
            ? writeError.localizedDescription
            : @"The portable material pack could not be written."];
        return;
    }

    std::size_t rleCount{};
    const auto opened = paperweight::readPwlib(bytes);
    if (const auto* library = std::get_if<paperweight::PackedMaterialLibrary>(&opened)) {
        rleCount = static_cast<std::size_t>(std::count_if(
            library->entries().begin(), library->entries().end(), [](const auto& entry) {
                return entry.storageMode == paperweight::PwlibStorageMode::rle;
            }));
    }
    auto* success = [[NSAlert alloc] init];
    success.messageText = @"Portable material pack exported";
    success.informativeText = [NSString stringWithFormat:
        @"%lu material%@, %lu bytes. RLE was smaller for %zu entr%@; the remaining entries are raw.",
        static_cast<unsigned long>(rows.count), rows.count == 1 ? @"" : @"s",
        static_cast<unsigned long>(bytes.size()), rleCount, rleCount == 1 ? @"y" : @"ies"];
    [success runModal];
}

- (void)duplicateSelected:(id)sender
{
    static_cast<void>(sender);
    PWMaterialLibraryRow* row = [self selectedRow];
    if (row == nil) return;
    auto material = [self readMaterial:row.url];
    if (!material) return;
    NSString* originalName = material->metadata && !material->metadata->name.empty()
        ? utf8(material->metadata->name) : row.displayName.stringByDeletingPathExtension;
    NSString* name = [self promptWithTitle:@"Duplicate Material"
                                   message:@"The duplicate receives its own stable UID."
                                     value:[originalName stringByAppendingString:@" Copy"]];
    if (name == nil) return;
    if (!material->metadata) material->metadata.emplace();
    material->metadata->uid = NSUUID.UUID.UUIDString.lowercaseString.UTF8String;
    material->metadata->name = name.UTF8String;
    NSURL* folder = row.url.URLByDeletingLastPathComponent;
    NSURL* target = [self uniqueURLInFolder:folder name:name];
    if (target == nil || ![self writeMaterial:*material toURL:target]) return;
    [self refreshLibrary:nil];
}

- (void)renameSelected:(id)sender
{
    static_cast<void>(sender);
    PWMaterialLibraryRow* row = [self selectedRow];
    if (row == nil) return;
    if (self.canRewriteHandler != nil && !self.canRewriteHandler(row.url)) {
        [self showOperationError:
            @"This material is open in the editor. Change its friendly name with Tools > Material Information, then save it."];
        return;
    }
    auto material = [self readMaterial:row.url];
    if (!material) return;
    NSString* name = [self promptWithTitle:@"Rename Material"
                                   message:@"This changes the friendly name; the UID and filename remain stable."
                                     value:row.displayName];
    if (name == nil) return;
    if (!material->metadata) material->metadata.emplace();
    material->metadata->name = name.UTF8String;
    if (material->metadata->uid.empty()) {
        material->metadata->uid = NSUUID.UUID.UUIDString.lowercaseString.UTF8String;
    }
    if (![self writeMaterial:*material toURL:row.url]) return;
    [self refreshLibrary:nil];
}

- (BOOL)urlIsInsideWorkingFolder:(NSURL*)url
{
    NSString* root = self.workingFolderURL.URLByStandardizingPath.path;
    NSString* candidate = url.URLByStandardizingPath.path;
    return [candidate isEqualToString:root] ||
        [candidate hasPrefix:[root stringByAppendingString:@"/"]];
}

- (void)moveSelected:(id)sender
{
    static_cast<void>(sender);
    PWMaterialLibraryRow* row = [self selectedRow];
    if (row == nil) return;
    auto* panel = [NSOpenPanel openPanel];
    panel.title = @"Move Material Within Working Folder";
    panel.directoryURL = self.workingFolderURL;
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = NO;
    panel.canCreateDirectories = YES;
    if ([panel runModal] != NSModalResponseOK) return;
    NSURL* destinationFolder = panel.URL.URLByStandardizingPath;
    if (![self urlIsInsideWorkingFolder:destinationFolder]) {
        [self showOperationError:@"Choose a destination inside the current working folder."];
        return;
    }
    NSURL* destination = [destinationFolder URLByAppendingPathComponent:row.url.lastPathComponent];
    if ([destination isEqual:row.url]) return;
    if ([NSFileManager.defaultManager fileExistsAtPath:destination.path]) {
        [self showOperationError:@"A material with that filename already exists in the destination."];
        return;
    }
    NSError* error = nil;
    if (![NSFileManager.defaultManager moveItemAtURL:row.url toURL:destination error:&error]) {
        [self showOperationError:error.localizedDescription];
        return;
    }
    if (self.relocationHandler != nil) {
        self.relocationHandler(row.url, destination);
    }
    [self refreshLibrary:nil];
}

- (void)noteMaterialSavedAtURL:(NSURL*)url
{
    if (url == nil || self.workingFolderURL == nil || ![self urlIsInsideWorkingFolder:url]) {
        return;
    }
    [self refreshLibrary:nil];
}

@end
