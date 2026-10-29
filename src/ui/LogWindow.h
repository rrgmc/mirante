#pragma once

#include <QWidget>

#include <memory>

namespace mirante {

class LogBuffer;

// A window that lists the Qt log messages a LogBuffer keeps, as they arrive,
// with a level filter and a text filter. Debug builds open it from
// Help > Show Log. Implemented in LogWindow.cpp.
class LogWindow : public QWidget
{
    Q_OBJECT

public:
    // The buffer must outlive the window.
    explicit LogWindow(LogBuffer &buffer, QWidget *parent = nullptr);
    ~LogWindow() override;

    // The text shown, for tests.
    QString shownText() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
