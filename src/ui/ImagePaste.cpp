#include "ui/ImagePaste.h"

#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/LockState.h"

#include <QImage>

// Stubs written by T0 of the M3 plan. T13 implements them.

namespace mirante {

struct ImagePaste::Private
{
};

ImagePaste::ImagePaste(Model &, Store &, const ItemTypeRegistry &, const LockState &)
    : d(std::make_unique<Private>())
{
}

ImagePaste::~ImagePaste() = default;

ImagePaste::Result ImagePaste::paste(const QImage &, const DashboardRef &, const std::optional<NodePath> &)
{
    return Result::NoImageType;
}

ImagePaste::Result ImagePaste::pasteIntoItem(const QImage &, const QString &, const QString &)
{
    return Result::NoImageType;
}

QString ImagePaste::message(Result) { return {}; }

} // namespace mirante
