#include "ui/ConflictBar.h"

#include "core/Model.h"

// Stubs written by T0 of the M3 plan. T14 implements them.

namespace mirante {

struct ConflictBar::Private
{
};

ConflictBar::ConflictBar(Store &, const Model &, QWidget *parent)
    : QFrame(parent)
    , d(std::make_unique<Private>())
{
    hide();
}

ConflictBar::~ConflictBar() = default;

QString ConflictBar::describe(const Store::Conflict &conflict, const Model &) { return conflict.copy; }

void ConflictBar::setConfirm(std::function<bool(const QString &)>) {}

} // namespace mirante
