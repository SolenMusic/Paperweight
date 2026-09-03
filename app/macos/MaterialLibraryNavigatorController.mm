#import "MaterialLibraryNavigatorController.hpp"
#import "MaterialLibraryWindowController.hpp"

#include <paperweight/pmat.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

NSString* pwString(const std::string& value)
{
    return [NSString stringWithUTF8String:value.c_str()];
}

struct ScannedMaterial {
    __strong NSURL* url;
    __strong NSString* name;
    __strong NSString* detail;
    __strong NSString* searchText;
};

} // namespace

@interface PWLibraryNavigatorRow : NSObject
@property(nonatomic, strong) NSURL* url;
@property(nonatomic, copy) NSString* name;
@property(nonatomic, copy) NSString* detail;
@property(nonatomic, copy) NSString* searchText;
@end

@implementation PWLibraryNavigatorRow
@end

@interface MaterialLibraryNavigatorController ()
    <NSTableViewDataSource, NSTableViewDelegate, NSSearchFieldDelegate>
@property(nonatomic, copy) PWNavigatorOpenMaterialHandler openMaterialHandler;
@property(nonatomic, copy) PWNavigatorShowLibraryHandler showLibraryHandler;
@property(nonatomic, strong) NSSearchField* searchField;
@property(nonatomic, strong) NSTableView* tableView;
@property(nonatomic, strong) NSTextField* summaryLabel;
@property(nonatomic, strong) NSArray<PWLibraryNavigatorRow*>* allRows;
@property(nonatomic, strong) NSArray<PWLibraryNavigatorRow*>* visibleRows;
@property(nonatomic, strong) NSString* representedFolderPath;
@end

@implementation MaterialLibraryNavigatorController {
    dispatch_queue_t scanQueue_;
    std::uint64_t scanRevision_;
}

- (instancetype)initWithOpenMaterialHandler:(PWNavigatorOpenMaterialHandler)openHandler
                          showLibraryHandler:(PWNavigatorShowLibraryHandler)showLibraryHandler
{
    self = [super initWithNibName:nil bundle:nil];
    if (self) {
        _openMaterialHandler = [openHandler copy];
        _showLibraryHandler = [showLibraryHandler copy];
        _allRows = @[];
        _visibleRows = @[];
        scanQueue_ = dispatch_queue_create(
            "org.solen-music.paperweight.library-navigator", DISPATCH_QUEUE_SERIAL);
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(defaultsDidChange:)
                                                   name:NSUserDefaultsDidChangeNotification
                                                 object:nil];
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(libraryDidChange:)
                                                   name:PWMaterialLibraryDidChangeNotification
                                                 object:nil];
    }
    return self;
}

- (void)libraryDidChange:(NSNotification*)notification
{
    static_cast<void>(notification);
    [self refresh];
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)loadView
{
    auto* background = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
    background.translatesAutoresizingMaskIntoConstraints = NO;
    background.material = NSVisualEffectMaterialSidebar;
    background.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    background.state = NSVisualEffectStateActive;
    self.view = background;

    auto* title = [NSTextField labelWithString:@"Library"];
    title.translatesAutoresizingMaskIntoConstraints = NO;
    title.font = [NSFont systemFontOfSize:15.0 weight:NSFontWeightSemibold];

    self.searchField = [[NSSearchField alloc] initWithFrame:NSZeroRect];
    self.searchField.translatesAutoresizingMaskIntoConstraints = NO;
    self.searchField.placeholderString = @"Search materials";
    self.searchField.delegate = self;

    self.tableView = [[NSTableView alloc] initWithFrame:NSZeroRect];
    self.tableView.headerView = nil;
    self.tableView.rowHeight = 48.0;
    self.tableView.intercellSpacing = NSMakeSize(0.0, 2.0);
    self.tableView.selectionHighlightStyle = NSTableViewSelectionHighlightStyleRegular;
    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    self.tableView.target = self;
    self.tableView.doubleAction = @selector(openSelectedMaterial:);
    auto* column = [[NSTableColumn alloc] initWithIdentifier:@"material"];
    column.resizingMask = NSTableColumnAutoresizingMask;
    [self.tableView addTableColumn:column];

    auto* scrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    scrollView.documentView = self.tableView;
    scrollView.hasVerticalScroller = YES;
    scrollView.drawsBackground = NO;
    scrollView.borderType = NSNoBorder;

    self.summaryLabel = [NSTextField labelWithString:@"Loading library…"];
    self.summaryLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.summaryLabel.font = [NSFont systemFontOfSize:11.0];
    self.summaryLabel.textColor = NSColor.secondaryLabelColor;
    self.summaryLabel.lineBreakMode = NSLineBreakByTruncatingTail;

    auto* overviewButton = [NSButton buttonWithTitle:@"Library Overview"
                                              target:self
                                              action:@selector(showLibrary:)];
    overviewButton.translatesAutoresizingMaskIntoConstraints = NO;
    overviewButton.bezelStyle = NSBezelStyleRounded;

    [background addSubview:title];
    [background addSubview:self.searchField];
    [background addSubview:scrollView];
    [background addSubview:self.summaryLabel];
    [background addSubview:overviewButton];
    [NSLayoutConstraint activateConstraints:@[
        [title.leadingAnchor constraintEqualToAnchor:background.leadingAnchor constant:12.0],
        [title.trailingAnchor constraintEqualToAnchor:background.trailingAnchor constant:-12.0],
        [title.topAnchor constraintEqualToAnchor:background.topAnchor constant:14.0],
        [self.searchField.leadingAnchor constraintEqualToAnchor:background.leadingAnchor constant:8.0],
        [self.searchField.trailingAnchor constraintEqualToAnchor:background.trailingAnchor constant:-8.0],
        [self.searchField.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:10.0],
        [scrollView.leadingAnchor constraintEqualToAnchor:background.leadingAnchor],
        [scrollView.trailingAnchor constraintEqualToAnchor:background.trailingAnchor],
        [scrollView.topAnchor constraintEqualToAnchor:self.searchField.bottomAnchor constant:8.0],
        [scrollView.bottomAnchor constraintEqualToAnchor:self.summaryLabel.topAnchor constant:-8.0],
        [self.summaryLabel.leadingAnchor constraintEqualToAnchor:background.leadingAnchor constant:12.0],
        [self.summaryLabel.trailingAnchor constraintEqualToAnchor:background.trailingAnchor constant:-12.0],
        [self.summaryLabel.bottomAnchor constraintEqualToAnchor:overviewButton.topAnchor constant:-8.0],
        [overviewButton.leadingAnchor constraintEqualToAnchor:background.leadingAnchor constant:10.0],
        [overviewButton.trailingAnchor constraintEqualToAnchor:background.trailingAnchor constant:-10.0],
        [overviewButton.bottomAnchor constraintEqualToAnchor:background.bottomAnchor constant:-10.0],
    ]];

    [self refresh];
}

- (void)defaultsDidChange:(NSNotification*)notification
{
    static_cast<void>(notification);
    NSString* folderPath = [NSUserDefaults.standardUserDefaults
        stringForKey:@"materialLibraryWorkingFolder"];
    if ((folderPath == nil && self.representedFolderPath != nil) ||
        (folderPath != nil && ![folderPath isEqualToString:self.representedFolderPath])) {
        [self refresh];
    }
}

- (void)refresh
{
    if (!self.isViewLoaded) {
        return;
    }
    NSString* folderPath = [NSUserDefaults.standardUserDefaults
        stringForKey:@"materialLibraryWorkingFolder"];
    self.representedFolderPath = folderPath;
    const std::uint64_t revision = ++scanRevision_;
    if (folderPath.length == 0) {
        self.allRows = @[];
        [self applyFilter];
        self.summaryLabel.stringValue = @"Choose a working folder in Library Overview.";
        return;
    }

    self.summaryLabel.stringValue = @"Refreshing…";
    NSString* capturedPath = [folderPath copy];
    __weak MaterialLibraryNavigatorController* weakSelf = self;
    dispatch_async(scanQueue_, ^{
        NSURL* rootURL = [NSURL fileURLWithPath:capturedPath isDirectory:YES];
        NSDirectoryEnumerator<NSURL*>* enumerator = [NSFileManager.defaultManager
            enumeratorAtURL:rootURL
            includingPropertiesForKeys:@[NSURLIsRegularFileKey]
            options:NSDirectoryEnumerationSkipsHiddenFiles
            errorHandler:^BOOL(NSURL* url, NSError* error) {
                static_cast<void>(url);
                static_cast<void>(error);
                return YES;
            }];
        std::vector<ScannedMaterial> scanned;
        for (NSURL* url in enumerator) {
            if ([url.pathExtension caseInsensitiveCompare:@"pmat"] != NSOrderedSame) {
                continue;
            }
            NSError* error = nil;
            NSString* contents = [NSString stringWithContentsOfURL:url
                                                           encoding:NSUTF8StringEncoding
                                                              error:&error];
            NSString* name = url.lastPathComponent.stringByDeletingPathExtension;
            NSString* category = @"Uncategorised";
            if (contents != nil) {
                auto result = paperweight::parsePmat(contents.UTF8String);
                if (const auto* material = std::get_if<paperweight::Material>(&result)) {
                    if (material->metadata.has_value()) {
                        if (!material->metadata->name.empty()) {
                            name = pwString(material->metadata->name);
                        }
                        if (!material->metadata->category.empty()) {
                            category = pwString(material->metadata->category);
                        }
                    }
                } else {
                    category = @"Invalid material";
                }
            } else {
                category = @"Unreadable material";
            }
            NSString* relativePath = [url.path substringFromIndex:
                std::min(url.path.length, capturedPath.length + 1)];
            NSString* detail = [NSString stringWithFormat:@"%@ · %@", category, relativePath];
            NSString* search = [NSString stringWithFormat:@"%@ %@ %@",
                name, category, relativePath].lowercaseString;
            scanned.push_back({url, name, detail, search});
        }
        std::sort(scanned.begin(), scanned.end(), [](const auto& left, const auto& right) {
            return [left.name localizedCaseInsensitiveCompare:right.name] == NSOrderedAscending;
        });

        dispatch_async(dispatch_get_main_queue(), ^{
            MaterialLibraryNavigatorController* strongSelf = weakSelf;
            if (strongSelf == nil || revision != strongSelf->scanRevision_) {
                return;
            }
            auto* rows = [NSMutableArray arrayWithCapacity:scanned.size()];
            for (const auto& entry : scanned) {
                auto* row = [[PWLibraryNavigatorRow alloc] init];
                row.url = entry.url;
                row.name = entry.name;
                row.detail = entry.detail;
                row.searchText = entry.searchText;
                [rows addObject:row];
            }
            strongSelf.allRows = rows;
            [strongSelf applyFilter];
        });
    });
}

- (void)controlTextDidChange:(NSNotification*)notification
{
    if (notification.object == self.searchField) {
        [self applyFilter];
    }
}

- (void)applyFilter
{
    NSString* query = [self.searchField.stringValue
        stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet]
        .lowercaseString;
    if (query.length == 0) {
        self.visibleRows = self.allRows;
    } else {
        auto* rows = [NSMutableArray array];
        for (PWLibraryNavigatorRow* row in self.allRows) {
            if ([row.searchText containsString:query]) {
                [rows addObject:row];
            }
        }
        self.visibleRows = rows;
    }
    [self.tableView reloadData];
    if (self.representedFolderPath.length == 0) {
        return;
    }
    if (query.length == 0) {
        self.summaryLabel.stringValue = [NSString stringWithFormat:@"%lu material%@",
            static_cast<unsigned long>(self.allRows.count), self.allRows.count == 1 ? @"" : @"s"];
    } else {
        self.summaryLabel.stringValue = [NSString stringWithFormat:@"%lu of %lu materials",
            static_cast<unsigned long>(self.visibleRows.count),
            static_cast<unsigned long>(self.allRows.count)];
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
    static_cast<void>(tableColumn);
    if (rowIndex < 0 || static_cast<NSUInteger>(rowIndex) >= self.visibleRows.count) {
        return nil;
    }
    auto* cell = [tableView makeViewWithIdentifier:@"navigatorCell" owner:self];
    NSTextField* nameLabel = nil;
    NSTextField* detailLabel = nil;
    if (cell == nil) {
        cell = [[NSTableCellView alloc] initWithFrame:NSZeroRect];
        cell.identifier = @"navigatorCell";
        nameLabel = [NSTextField labelWithString:@""];
        nameLabel.translatesAutoresizingMaskIntoConstraints = NO;
        nameLabel.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightMedium];
        nameLabel.lineBreakMode = NSLineBreakByTruncatingTail;
        nameLabel.tag = 101;
        detailLabel = [NSTextField labelWithString:@""];
        detailLabel.translatesAutoresizingMaskIntoConstraints = NO;
        detailLabel.font = [NSFont systemFontOfSize:10.0];
        detailLabel.textColor = NSColor.secondaryLabelColor;
        detailLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
        detailLabel.tag = 102;
        [cell addSubview:nameLabel];
        [cell addSubview:detailLabel];
        [NSLayoutConstraint activateConstraints:@[
            [nameLabel.leadingAnchor constraintEqualToAnchor:cell.leadingAnchor constant:9.0],
            [nameLabel.trailingAnchor constraintEqualToAnchor:cell.trailingAnchor constant:-7.0],
            [nameLabel.topAnchor constraintEqualToAnchor:cell.topAnchor constant:6.0],
            [detailLabel.leadingAnchor constraintEqualToAnchor:nameLabel.leadingAnchor],
            [detailLabel.trailingAnchor constraintEqualToAnchor:nameLabel.trailingAnchor],
            [detailLabel.topAnchor constraintEqualToAnchor:nameLabel.bottomAnchor constant:2.0],
        ]];
    } else {
        nameLabel = [cell viewWithTag:101];
        detailLabel = [cell viewWithTag:102];
    }
    PWLibraryNavigatorRow* row = self.visibleRows[static_cast<NSUInteger>(rowIndex)];
    nameLabel.stringValue = row.name;
    detailLabel.stringValue = row.detail;
    cell.toolTip = row.url.path;
    return cell;
}

- (PWLibraryNavigatorRow*)selectedRow
{
    NSInteger index = self.tableView.selectedRow;
    if (index < 0 || static_cast<NSUInteger>(index) >= self.visibleRows.count) {
        return nil;
    }
    return self.visibleRows[static_cast<NSUInteger>(index)];
}

- (void)openSelectedMaterial:(id)sender
{
    static_cast<void>(sender);
    PWLibraryNavigatorRow* row = [self selectedRow];
    if (row != nil && self.openMaterialHandler != nil) {
        self.openMaterialHandler(row.url);
    }
}

- (void)showLibrary:(id)sender
{
    static_cast<void>(sender);
    if (self.showLibraryHandler != nil) {
        self.showLibraryHandler();
    }
}

@end
