#include "core/FolderWatcher.h"

// Stubs written by T0 of the M3 plan. T7 implements them.

namespace mirante {

struct FolderWatcher::Private
{
};

FolderWatcher::FolderWatcher(const QString &, OwnWriteCheck, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

FolderWatcher::~FolderWatcher() = default;

void FolderWatcher::setDelay(int) {}
void FolderWatcher::start() {}
void FolderWatcher::stop() {}

} // namespace mirante
