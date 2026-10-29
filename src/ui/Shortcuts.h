#pragma once

#include <QObject>

#include <memory>

namespace mirante {

class MainWindow;

// The window shortcuts of docs/ui.md#keyboard that M2 covers: focus movement
// between cells, tab switching, maximise and close. It also tracks the focused
// cell through QApplication::focusChanged. Implemented in Shortcuts.cpp.
class Shortcuts : public QObject
{
    Q_OBJECT

public:
    explicit Shortcuts(MainWindow *window);
    ~Shortcuts() override;

    // The cell widget that holds the keyboard focus, or nullptr.
    QWidget *focusedCell() const;

    // Moves the focus to a cell, as if the user clicked it.
    void focusCell(QWidget *cell);

    // Maximises a cell over the content area and focuses it, or restores it
    // if it is the maximised one. The cell need not have the focus. See the
    // M4 plan, part 6.
    void toggleMaximised(QWidget *cell);
    bool isMaximised(const QWidget *cell) const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
