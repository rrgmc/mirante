#include "core/WindowPlacement.h"

#include <QSize>

#include <algorithm>
#include <vector>

namespace mirante::windowplacement {

namespace {

// The area of rect that the screens cover. Screens may overlap, as mirrored
// ones do, so the area is counted on a grid made of the rectangles' edges.
qint64 coveredArea(const QRect &rect, const QList<QRect> &screens)
{
    std::vector<QRect> parts;
    for (const QRect &screen : screens) {
        const QRect part = rect.intersected(screen);
        if (!part.isEmpty())
            parts.push_back(part);
    }
    std::vector<int> xs, ys;
    for (const QRect &part : parts) {
        xs.push_back(part.left());
        xs.push_back(part.left() + part.width());
        ys.push_back(part.top());
        ys.push_back(part.top() + part.height());
    }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());

    qint64 area = 0;
    for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
        for (std::size_t j = 0; j + 1 < ys.size(); ++j) {
            const QRect cell(QPoint(xs[i], ys[j]), QSize(xs[i + 1] - xs[i], ys[j + 1] - ys[j]));
            const bool covered = std::any_of(parts.begin(), parts.end(),
                                             [&](const QRect &part) { return part.contains(cell); });
            if (covered)
                area += qint64(cell.width()) * cell.height();
        }
    }
    return area;
}

qint64 areaOf(const QRect &rect) { return qint64(rect.width()) * rect.height(); }

} // namespace

bool isVisible(const QRect &client, const QList<QRect> &screens)
{
    if (client.isEmpty())
        return false;
    // Only the lower half of the assumed title bar is checked. The real one
    // may be lower, such as 31 pixels on Windows 11 and 28 on macOS, and a
    // window against the top of the screen must keep its place.
    const int strip = titleBarHeight / 2;
    const QRect titleBar(client.left(), client.top() - strip, client.width(), strip);
    if (coveredArea(titleBar, screens) < areaOf(titleBar))
        return false;
    const QRect frame = client.adjusted(0, -titleBarHeight, 0, 0);
    return coveredArea(frame, screens) * 2 >= areaOf(frame);
}

QRect place(const QRect &saved, const QSize &fallbackSize, const QList<QRect> &screens, const QRect &primary)
{
    if (saved.isValid() && isVisible(saved, screens))
        return saved;
    QSize size = saved.isValid() ? saved.size() : fallbackSize;
    if (primary.isEmpty())
        return QRect(QPoint(0, titleBarHeight), size);
    // The title bar must fit on the screen too.
    size = size.boundedTo(QSize(primary.width(), primary.height() - titleBarHeight));
    QRect frame(QPoint(0, 0), QSize(size.width(), size.height() + titleBarHeight));
    frame.moveCenter(primary.center());
    return QRect(frame.left(), frame.top() + titleBarHeight, size.width(), size.height());
}

} // namespace mirante::windowplacement
