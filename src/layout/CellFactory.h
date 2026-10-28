#pragma once

#include "core/LayoutTree.h"

#include <optional>

class QWidget;

namespace mirante {

// Makes the widgets that LayoutView places in cells. A task dashboard and a
// root dashboard use different factories, and tests use a fake one.
class CellFactory
{
public:
    virtual ~CellFactory() = default;

    // ref is nullopt for an empty cell. removed is set for a cell whose
    // reference was deleted.
    virtual QWidget *createCell(const std::optional<CellRef> &ref, bool removed, QWidget *parent) = 0;

    // False if the widget for ref must be made again instead of being moved to
    // another parent. See ItemType::reparentSafe().
    virtual bool canReparent(const std::optional<CellRef> &) const { return true; }
};

} // namespace mirante
