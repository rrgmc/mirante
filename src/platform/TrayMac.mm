// The macOS part of Tray: a native NSStatusItem. Built with ARC. See Tray.h.
#include "platform/Tray.h"

#include <QAction>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QMenu>
#include <QPointer>

#import <AppKit/AppKit.h>

#include <functional>

// Rebuilds the native menu from the QMenu each time it opens, so it follows the
// actions' text and enabled state.
@interface MiranteTrayMenuDelegate : NSObject <NSMenuDelegate>
- (instancetype)initWithMenu:(QMenu *)menu;
@end

@implementation MiranteTrayMenuDelegate {
    QPointer<QMenu> _menu;
}

- (instancetype)initWithMenu:(QMenu *)menu
{
    if ((self = [super init]))
        _menu = menu;
    return self;
}

- (void)menuNeedsUpdate:(NSMenu *)nsMenu
{
    [nsMenu removeAllItems];
    if (!_menu)
        return;
    const QList<QAction *> actions = _menu->actions();
    for (qsizetype i = 0; i < actions.size(); ++i) {
        QAction *action = actions[i];
        if (!action->isVisible())
            continue;
        if (action->isSeparator()) {
            [nsMenu addItem:[NSMenuItem separatorItem]];
            continue;
        }
        // Native menus have no mnemonics.
        const QString title = QString(action->text()).remove(QLatin1Char('&'));
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title.toNSString()
                                                      action:@selector(itemClicked:)
                                               keyEquivalent:@""];
        item.target = self;
        item.tag = i;
        item.enabled = action->isEnabled();
        [nsMenu addItem:item];
    }
}

- (void)itemClicked:(NSMenuItem *)item
{
    if (!_menu)
        return;
    const QList<QAction *> actions = _menu->actions();
    if (item.tag < 0 || item.tag >= actions.size())
        return;
    // Run the action after the menu has finished tracking.
    QMetaObject::invokeMethod(actions[item.tag], &QAction::trigger, Qt::QueuedConnection);
}

@end

@interface MiranteReopenHandler : NSObject
- (instancetype)initWithCallback:(std::function<void()>)callback;
- (void)handleReopen:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)reply;
@end

@implementation MiranteReopenHandler {
    std::function<void()> _callback;
}

- (instancetype)initWithCallback:(std::function<void()>)callback
{
    if ((self = [super init]))
        _callback = std::move(callback);
    return self;
}

- (void)handleReopen:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)reply
{
    _callback();
}

@end

namespace {

// A template image: macOS uses only its alpha and draws it in the menu bar's
// colour.
NSImage *toTemplateImage(const QIcon &icon)
{
    const NSSize size = NSMakeSize(18, 18);
    NSImage *image = [[NSImage alloc] initWithSize:size];
    for (const qreal scale : {1.0, 2.0}) {
        const QImage qimage = icon.pixmap(QSize(18, 18), scale).toImage();
        CGImageRef cgImage = qimage.toCGImage();
        if (!cgImage)
            continue;
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
        CGImageRelease(cgImage);
        rep.size = size;
        [image addRepresentation:rep];
    }
    [image setTemplate:YES];    // "template" is a C++ keyword
    return image;
}

} // namespace

namespace mirante::platform {

struct Tray::Private
{
    NSStatusItem *item = nil;
    NSMenu *menu = nil;
    MiranteTrayMenuDelegate *delegate = nil;
    MiranteReopenHandler *reopenHandler = nil;
};

bool Tray::isAvailable()
{
    // The offscreen platform used by the tests has no menu bar.
    return QGuiApplication::platformName() == QLatin1String("cocoa");
}

Tray::Tray(const QIcon &icon, const QIcon &monochromeIcon, const QString &toolTip, QMenu *menu,
           QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    Q_UNUSED(icon);
    d->delegate = [[MiranteTrayMenuDelegate alloc] initWithMenu:menu];
    d->menu = [[NSMenu alloc] init];
    d->menu.autoenablesItems = NO;
    d->menu.delegate = d->delegate;

    d->item = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
    d->item.button.image = toTemplateImage(monochromeIcon);
    d->item.button.toolTip = toolTip.toNSString();
    d->item.menu = d->menu;

    MiranteReopenHandler *handler = [[MiranteReopenHandler alloc] initWithCallback:[this] { emit reopened(); }];
    d->reopenHandler = handler;
    // AppKit installs its own reopen handler while launching, so replace it
    // afterwards. The tray is made before the event loop starts.
    __block id observer = [NSNotificationCenter.defaultCenter
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *) {
                    [NSNotificationCenter.defaultCenter removeObserver:observer];
                    [NSAppleEventManager.sharedAppleEventManager
                        setEventHandler:handler
                            andSelector:@selector(handleReopen:withReplyEvent:)
                          forEventClass:kCoreEventClass
                             andEventID:kAEReopenApplication];
                }];
}

Tray::~Tray()
{
    [NSAppleEventManager.sharedAppleEventManager removeEventHandlerForEventClass:kCoreEventClass
                                                                      andEventID:kAEReopenApplication];
    [NSStatusBar.systemStatusBar removeStatusItem:d->item];
}

void Tray::setDockIconVisible(bool visible)
{
    // An accessory app has no Dock icon and no menu bar, like a menu bar app.
    const NSApplicationActivationPolicy policy =
        visible ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory;
    if (NSApp.activationPolicy == policy)
        return;
    [NSApp setActivationPolicy:policy];
    // An app that was an accessory does not come to the front by itself.
    if (visible) {
        if (@available(macOS 14, *))
            [NSApp activate];
        else
            [NSApp activateIgnoringOtherApps:YES];
    }
}

} // namespace mirante::platform
