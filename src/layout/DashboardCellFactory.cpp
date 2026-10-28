// Stub from task T0. Task T10 implements this file.
#include "layout/DashboardCellFactory.h"

#include "layout/CellFrame.h"

namespace mirante {

struct DashboardCellFactory::Private
{
    LayoutServices services;
    DashboardRef dashboard;
};

DashboardCellFactory::DashboardCellFactory(const LayoutServices &services, const DashboardRef &dashboard)
    : d(std::make_unique<Private>(Private{services, dashboard}))
{
}

DashboardCellFactory::~DashboardCellFactory() = default;

QWidget *DashboardCellFactory::createCell(const std::optional<CellRef> &ref, bool removed, QWidget *parent)
{
    return new CellFrame(ref, removed, nullptr, QString(), parent);
}

bool DashboardCellFactory::canReparent(const std::optional<CellRef> & /*ref*/) const { return true; }

} // namespace mirante
