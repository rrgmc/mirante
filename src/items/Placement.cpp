#include "items/Placement.h"

namespace mirante {

// Stub from M4 T0. T7 implements it.
struct Placement::Private
{
    PlacementServices services;
    QString taskId;
    QString itemId;
};

Placement::Placement(const PlacementServices &services, const QString &taskId, const QString &itemId, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>(Private{services, taskId, itemId}))
{
}

Placement::~Placement() = default;

QString Placement::taskId() const { return d->taskId; }

QString Placement::itemId() const { return d->itemId; }

QWidget *Placement::view() const { return nullptr; }

QWidget *Placement::headerWidget() const { return nullptr; }

} // namespace mirante
