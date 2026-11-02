#pragma once

#include <QImage>

class QMimeData;

namespace mirante::widgets {

// The image a drag from another app carries: the first local file whose
// suffix is png, jpg, jpeg, gif, bmp or webp and that QImageReader reads,
// else the drag's image data. A null image if it carries neither. See
// docs/decisions/0085-drop-images-from-other-apps.md.
QImage imageFromMime(const QMimeData *data);

} // namespace mirante::widgets
