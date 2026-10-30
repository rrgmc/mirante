#pragma once

#include <QKeySequence>
#include <QObject>
#include <QString>

#include <memory>

namespace mirante::platform {

// A system-wide keyboard shortcut, through QHotkey. It works on Windows, macOS
// and X11. Wayland has no way to register one, so there it fails with a reason
// that names the desktop's own shortcuts instead.
class GlobalHotkey : public QObject
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    // False where a hotkey cannot be registered, and setShortcut() says so.
    static bool isSupported();

    // Replaces the current registration. An empty sequence only removes it.
    // Returns false and fills error if the sequence cannot be registered.
    bool setShortcut(const QKeySequence &sequence, QString *error = nullptr);

signals:
    void activated();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante::platform
