// Stub from task T0. Task T16 implements this file.
#include "ui/Shortcuts.h"

#include "ui/MainWindow.h"

namespace mirante {

struct Shortcuts::Private
{
};

Shortcuts::Shortcuts(MainWindow *window)
    : QObject(window)
    , d(std::make_unique<Private>())
{
}

Shortcuts::~Shortcuts() = default;

QWidget *Shortcuts::focusedCell() const { return nullptr; }
void Shortcuts::focusCell(QWidget * /*cell*/) {}

} // namespace mirante
