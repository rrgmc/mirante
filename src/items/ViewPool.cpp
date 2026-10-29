#include "items/ViewPool.h"

namespace mirante {

// Stub from M4 T0. T3 implements it.
struct ViewPool::Private
{
    int limit = 8;
};

ViewPool::ViewPool(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

ViewPool::~ViewPool() = default;

void ViewPool::setLimit(int hiddenViews) { d->limit = hiddenViews; }

int ViewPool::limit() const { return d->limit; }

void ViewPool::add(QObject *, std::function<void()>) {}

void ViewPool::remove(QObject *) {}

void ViewPool::setVisible(QObject *, bool) {}

int ViewPool::hiddenCount() const { return 0; }

} // namespace mirante
