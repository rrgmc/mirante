// The macOS part of AttachedDialogs.cpp: child windows through AppKit.
#import <AppKit/AppKit.h>

#include <QWidget>

namespace mirante::platform {

namespace {

NSWindow *nativeWindow(QWidget *widget)
{
    if (!widget)
        return nil;
    auto *view = reinterpret_cast<NSView *>(widget->window()->winId());
    return view ? view.window : nil;
}

} // namespace

void attachChildWindow(QWidget *child, QWidget *parent)
{
    NSWindow *childWindow = nativeWindow(child);
    NSWindow *parentWindow = nativeWindow(parent);
    if (childWindow && parentWindow && childWindow != parentWindow && childWindow.parentWindow != parentWindow)
        [parentWindow addChildWindow:childWindow ordered:NSWindowAbove];
}

void detachChildWindow(QWidget *child, QWidget *parent)
{
    NSWindow *childWindow = nativeWindow(child);
    NSWindow *parentWindow = nativeWindow(parent);
    if (childWindow && parentWindow && childWindow.parentWindow == parentWindow)
        [parentWindow removeChildWindow:childWindow];
}

} // namespace mirante::platform
