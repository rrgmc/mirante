#pragma once

#include "core/Model.h"
#include "layout/CellFactory.h"
#include "layout/LayoutServices.h"

#include <memory>

namespace mirante {

// The factory for real dashboards. Item references become CellFrames, and in
// a root dashboard, task references become TaskCells.
// Implemented in DashboardCellFactory.cpp.
class DashboardCellFactory : public CellFactory
{
public:
    DashboardCellFactory(const LayoutServices &services, const DashboardRef &dashboard);
    ~DashboardCellFactory() override;

    QWidget *createCell(const std::optional<CellRef> &ref, bool removed, QWidget *parent) override;
    bool canReparent(const std::optional<CellRef> &ref) const override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
