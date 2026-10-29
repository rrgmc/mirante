#include "ui/FocusView.h"

#include "core/Model.h"

// Stubs written by T0 of the M3 plan. T6 implements them.

namespace mirante {

struct FocusView::Private
{
    QString groupFilter;
};

FocusView::FocusView(const Model &, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
}

FocusView::~FocusView() = default;

void FocusView::setGroupFilter(const QString &groupId) { d->groupFilter = groupId; }
QString FocusView::groupFilter() const { return d->groupFilter; }
QStringList FocusView::taskIds() const { return {}; }

} // namespace mirante
