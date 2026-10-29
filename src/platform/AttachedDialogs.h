#pragma once

class QDialog;
class QWidget;

namespace mirante::platform {

// On macOS, a modal dialog of the main window or a detached window is attached
// to that window as a child window while it is shown. It then stays directly
// above its window and comes forward with it, so another app's window cannot
// come between them. On Windows and Linux nothing changes. See
// docs/ui.md#look. Implemented in AttachedDialogs.cpp, with the macOS part in
// AttachedDialogsMac.mm.

// Watches every dialog the application shows. main.cpp calls it once, after
// the QApplication exists.
void installAttachedDialogs();

// The window a dialog is attached to while shown: on macOS, the visible
// window of a modal dialog's parent, unless that window is itself a dialog.
// Null otherwise. For tests.
QWidget *attachTarget(const QDialog *dialog);

} // namespace mirante::platform
