// Stub from task T0. Task T13 implements this file.
#include "layout/DragMime.h"

#include <QMimeData>

namespace mirante {

QMimeData *dragMimeData(const DragPayload & /*payload*/) { return new QMimeData; }

std::optional<DragPayload> dragPayload(const QMimeData * /*mime*/) { return std::nullopt; }

} // namespace mirante
