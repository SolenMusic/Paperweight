#import "PackedLibraryWindowController.hpp"

#include <paperweight/pwlib.hpp>

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

@interface PWPackedLibraryRow : NSObject
@property(nonatomic, copy) NSString* uid;
@property(nonatomic, copy) NSString* name;
@property(nonatomic, copy) NSString* storage;
@property(nonatomic, copy) NSString* canonicalSize;
@property(nonatomic, copy) NSString* storedSize;
@property(nonatomic, copy) NSString* checksum;
@end

@implementation PWPackedLibraryRow
@end

namespace {

NSTextField* label(NSString* text)
{
    auto* result = [NSTextField labelWithString:text];
    result.lineBreakMode = NSLineBreakByTruncatingTail;
    return result;
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

NSString* hexChecksum(std::uint64_t checksum)
{
    return [NSString stringWithFormat:@"%016llx", static_cast<unsigned long long>(checksum)];
}

std::optional<std::uint64_t> parseSeed(NSString* text)
{
    const char* utf8 = text.UTF8String;
    if (utf8 == nullptr || *utf8 == '\0') {
        return std::nullopt;
    }
    const char* end = utf8 + std::char_traits<char>::length(utf8);
    std::uint64_t value{};
    const auto result = std::from_chars(utf8, end, value);
    return result.ec == std::errc{} && result.ptr == end
        ? std::optional<std::uint64_t>{value}
        : std::nullopt;
}

} // namespace

@interface PackedLibraryWindowController () <NSTableViewDataSource, NSTableViewDelegate>
@property(nonatomic, copy) PWOpenPackedMaterialHandler openHandler;
@property(nonatomic, strong) NSURL* libraryURL;
@property(nonatomic, strong) NSArray<PWPackedLibraryRow*>* rows;
@property(nonatomic, strong) NSTableView* tableView;
@property(nonatomic, strong) NSTextField* summaryLabel;
@property(nonatomic, strong) NSTextField* seedField;
@property(nonatomic, strong) NSButton* openButton;
@property(nonatomic, readwrite) BOOL validLibrary;
@end

@implementation PackedLibraryWindowController {
    std::vector<std::uint8_t> bytes_;
    std::optional<paperweight::PackedMaterialLibrary> library_;
}

- (instancetype)initWithURL:(NSURL*)url
                 openHandler:(PWOpenPackedMaterialHandler)openHandler
{
    const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    auto* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 860, 480)
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    self = [super initWithWindow:window];
    if (self) {
        _libraryURL = url;
        _openHandler = [openHandler copy];
        _rows = @[];
        window.title = [NSString stringWithFormat:@"%@ — Pack Inspector", url.lastPathComponent];
        window.minSize = NSMakeSize(720, 400);
        [self buildInterface];
        [self readLibrary];
    }
    return self;
}

- (void)buildInterface
{
    auto* content = [[NSView alloc] initWithFrame:NSZeroRect];
    self.window.contentView = content;

    auto* title = label(@"Portable Material Pack");
    title.font = [NSFont systemFontOfSize:18.0 weight:NSFontWeightSemibold];
    auto* path = label(self.libraryURL.path);
    path.textColor = NSColor.secondaryLabelColor;
    self.summaryLabel = label(@"Reading pack…");
    self.summaryLabel.maximumNumberOfLines = 2;

    self.tableView = [[NSTableView alloc] initWithFrame:NSZeroRect];
    self.tableView.delegate = self;
    self.tableView.dataSource = self;
    self.tableView.usesAlternatingRowBackgroundColors = YES;
    self.tableView.allowsEmptySelection = YES;
    const NSArray* columns = @[
        @[@"name", @"Name", @180], @[@"uid", @"UID", @270],
        @[@"storage", @"Storage", @65], @[@"stored", @"Stored", @80],
        @[@"canonical", @"Canonical", @80], @[@"checksum", @"Entry checksum", @140],
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
    scroll.hasHorizontalScroller = YES;
    scroll.borderType = NSBezelBorder;

    auto* seedLabel = label(@"Instantiation seed");
    self.seedField = [NSTextField textFieldWithString:@"18431"];
    [self.seedField.widthAnchor constraintEqualToConstant:150.0].active = YES;
    self.openButton = [NSButton buttonWithTitle:@"Instantiate in Editor"
                                          target:self
                                          action:@selector(openSelected:)];
    self.openButton.enabled = NO;
    auto* controls = [NSStackView stackViewWithViews:@[
        seedLabel, self.seedField, self.openButton,
    ]];
    controls.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    controls.spacing = 8.0;

    for (NSView* view in @[title, path, self.summaryLabel, scroll, controls]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [content addSubview:view];
    }
    [NSLayoutConstraint activateConstraints:@[
        [title.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:20.0],
        [title.topAnchor constraintEqualToAnchor:content.topAnchor constant:18.0],
        [path.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [path.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-20.0],
        [path.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:4.0],
        [self.summaryLabel.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [self.summaryLabel.trailingAnchor constraintEqualToAnchor:path.trailingAnchor],
        [self.summaryLabel.topAnchor constraintEqualToAnchor:path.bottomAnchor constant:8.0],
        [scroll.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [scroll.trailingAnchor constraintEqualToAnchor:path.trailingAnchor],
        [scroll.topAnchor constraintEqualToAnchor:self.summaryLabel.bottomAnchor constant:12.0],
        [scroll.bottomAnchor constraintEqualToAnchor:controls.topAnchor constant:-12.0],
        [controls.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
        [controls.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-18.0],
    ]];
}

- (void)readLibrary
{
    NSError* readError = nil;
    NSData* data = [NSData dataWithContentsOfURL:self.libraryURL
                                        options:NSDataReadingMappedIfSafe
                                          error:&readError];
    if (data == nil) {
        self.summaryLabel.stringValue = readError.localizedDescription != nil
            ? readError.localizedDescription
            : @"The pack could not be read.";
        self.summaryLabel.textColor = NSColor.systemRedColor;
        return;
    }
    if (data.length != 0) {
        const auto* begin = static_cast<const std::uint8_t*>(data.bytes);
        bytes_.assign(begin, begin + data.length);
    } else {
        bytes_.clear();
    }
    auto result = paperweight::readPwlib(bytes_);
    if (const auto* readFailure = std::get_if<paperweight::PwlibError>(&result)) {
        self.summaryLabel.stringValue = [NSString stringWithUTF8String:readFailure->message.c_str()];
        self.summaryLabel.textColor = NSColor.systemRedColor;
        return;
    }
    library_ = std::get<paperweight::PackedMaterialLibrary>(std::move(result));
    auto* rows = [NSMutableArray arrayWithCapacity:library_->entries().size()];
    std::size_t rleCount{};
    std::uint64_t canonicalBytes{};
    std::uint64_t storedBytes{};
    for (const auto& entry : library_->entries()) {
        auto* row = [[PWPackedLibraryRow alloc] init];
        row.uid = [[NSString alloc] initWithBytes:entry.uid.data()
                                           length:entry.uid.size()
                                         encoding:NSUTF8StringEncoding];
        row.name = [[NSString alloc] initWithBytes:entry.name.data()
                                            length:entry.name.size()
                                          encoding:NSUTF8StringEncoding];
        row.storage = entry.storageMode == paperweight::PwlibStorageMode::rle ? @"RLE" : @"Raw";
        row.canonicalSize = byteCount(entry.uncompressedSize);
        row.storedSize = byteCount(entry.storedSize);
        row.checksum = hexChecksum(entry.checksum);
        canonicalBytes += entry.uncompressedSize;
        storedBytes += entry.storedSize;
        if (entry.storageMode == paperweight::PwlibStorageMode::rle) {
            ++rleCount;
        }
        [rows addObject:row];
    }
    self.rows = rows;
    self.validLibrary = YES;
    self.summaryLabel.stringValue = [NSString stringWithFormat:
        @"Version %u; %lu material%@; pack %@; payload %@ stored from %@ canonical; "
         "%zu RLE; library checksum %@.",
        library_->formatVersion(), static_cast<unsigned long>(rows.count),
        rows.count == 1 ? @"" : @"s", byteCount(bytes_.size()),
        byteCount(storedBytes), byteCount(canonicalBytes), rleCount,
        hexChecksum(library_->checksum())];
    [self.tableView reloadData];
    if (rows.count != 0) {
        [self.tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
                   byExtendingSelection:NO];
    }
}

- (void)showPackedLibrary
{
    [self showWindow:nil];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
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
    static_cast<void>(tableView);
    if (rowIndex < 0 || static_cast<NSUInteger>(rowIndex) >= self.rows.count) {
        return nil;
    }
    const auto* row = self.rows[static_cast<NSUInteger>(rowIndex)];
    NSString* value = @"";
    if ([tableColumn.identifier isEqualToString:@"name"]) value = row.name;
    else if ([tableColumn.identifier isEqualToString:@"uid"]) value = row.uid;
    else if ([tableColumn.identifier isEqualToString:@"storage"]) value = row.storage;
    else if ([tableColumn.identifier isEqualToString:@"stored"]) value = row.storedSize;
    else if ([tableColumn.identifier isEqualToString:@"canonical"]) value = row.canonicalSize;
    else if ([tableColumn.identifier isEqualToString:@"checksum"]) value = row.checksum;
    auto* view = label(value);
    view.toolTip = value;
    return view;
}

- (void)tableViewSelectionDidChange:(NSNotification*)notification
{
    static_cast<void>(notification);
    self.openButton.enabled = self.validLibrary && self.tableView.selectedRow >= 0;
}

- (void)openSelected:(id)sender
{
    static_cast<void>(sender);
    const NSInteger index = self.tableView.selectedRow;
    if (!library_ || index < 0 || static_cast<NSUInteger>(index) >= self.rows.count) {
        return;
    }
    const auto seed = parseSeed(self.seedField.stringValue);
    if (!seed) {
        auto* alert = [[NSAlert alloc] init];
        alert.alertStyle = NSAlertStyleCritical;
        alert.messageText = @"The seed is not valid";
        alert.informativeText = @"Enter a whole number from 0 through 18446744073709551615.";
        [alert runModal];
        return;
    }
    const auto& entry = library_->entries()[static_cast<std::size_t>(index)];
    auto result = library_->instantiateByUid(entry.uid, *seed);
    if (const auto* openError = std::get_if<paperweight::PwlibError>(&result)) {
        auto* alert = [[NSAlert alloc] init];
        alert.alertStyle = NSAlertStyleCritical;
        alert.messageText = @"The packed material could not be instantiated";
        alert.informativeText = [NSString stringWithUTF8String:openError->message.c_str()];
        [alert runModal];
        return;
    }
    if (self.openHandler != nil) {
        self.openHandler(std::get<paperweight::Material>(std::move(result)));
    }
}

@end
