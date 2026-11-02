#include <mirante/widgets/ImageMime.h>

#include <QFileInfo>
#include <QImageReader>
#include <QMimeData>
#include <QStringList>
#include <QUrl>

namespace mirante::widgets {

QImage imageFromMime(const QMimeData *data)
{
    if (!data)
        return {};
    static const QStringList suffixes = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                         QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp")};
    for (const QUrl &url : data->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (!suffixes.contains(QFileInfo(path).suffix().toLower()))
            continue;
        QImageReader reader(path);
        // Photos turned by their EXIF orientation show as taken.
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (!image.isNull())
            return image;
    }
    if (data->hasImage())
        return qvariant_cast<QImage>(data->imageData());
    return {};
}

} // namespace mirante::widgets
