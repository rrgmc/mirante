// Stub from task T0. Task T11 implements this file.
#include "ui/Sidebar.h"

namespace mirante {

struct Sidebar::Private
{
};

Sidebar::Sidebar(const Model & /*model*/, const ItemTypeRegistry & /*types*/, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
}

Sidebar::~Sidebar() = default;

void Sidebar::reveal(const QString & /*id*/) {}

} // namespace mirante
