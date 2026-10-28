// Stub from task T0. Task T10 implements this file.
#include "layout/SingleViewTracker.h"

namespace mirante {

struct SingleViewTracker::Private
{
};

SingleViewTracker::SingleViewTracker(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

SingleViewTracker::~SingleViewTracker() = default;

QWidget *SingleViewTracker::holder(const QString & /*itemId*/) const { return nullptr; }
void SingleViewTracker::setHolder(const QString & /*itemId*/, QWidget * /*cell*/) {}
void SingleViewTracker::release(QWidget * /*cell*/) {}

} // namespace mirante
