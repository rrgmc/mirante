#include "ui/ImageCropView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace mirante {

namespace imagecrop {

QRectF fit(const QSize &image, const QRectF &area)
{
    if (image.isEmpty() || area.isEmpty())
        return {};
    const qreal scale = std::min({area.width() / image.width(), area.height() / image.height(), 1.0});
    const QSizeF size(image.width() * scale, image.height() * scale);
    return QRectF(area.center() - QPointF(size.width() / 2, size.height() / 2), size);
}

QPoint toImage(const QPointF &point, const QRectF &shown, const QSize &image)
{
    if (shown.isEmpty())
        return {};
    const qreal x = (point.x() - shown.left()) * image.width() / shown.width();
    const qreal y = (point.y() - shown.top()) * image.height() / shown.height();
    return QPoint(std::clamp(int(std::lround(x)), 0, image.width()),
                  std::clamp(int(std::lround(y)), 0, image.height()));
}

QRectF toView(const QRect &rect, const QRectF &shown, const QSize &image)
{
    if (image.isEmpty())
        return {};
    const qreal sx = shown.width() / image.width();
    const qreal sy = shown.height() / image.height();
    return QRectF(shown.left() + rect.x() * sx, shown.top() + rect.y() * sy, rect.width() * sx,
                  rect.height() * sy);
}

Handle handleAt(const QRectF &rect, const QPointF &point, qreal grab)
{
    if (rect.isEmpty())
        return Handle::None;
    const qreal x = point.x();
    const qreal y = point.y();
    if (x < rect.left() - grab || x > rect.right() + grab || y < rect.top() - grab || y > rect.bottom() + grab)
        return Handle::None;

    // The nearer edge in each direction, if the point is close enough to it.
    const qreal toLeft = std::abs(x - rect.left());
    const qreal toRight = std::abs(x - rect.right());
    const qreal toTop = std::abs(y - rect.top());
    const qreal toBottom = std::abs(y - rect.bottom());
    const int horizontal = std::min(toLeft, toRight) > grab ? 0 : (toLeft <= toRight ? -1 : 1);
    const int vertical = std::min(toTop, toBottom) > grab ? 0 : (toTop <= toBottom ? -1 : 1);

    if (horizontal < 0 && vertical < 0)
        return Handle::TopLeft;
    if (horizontal > 0 && vertical < 0)
        return Handle::TopRight;
    if (horizontal < 0 && vertical > 0)
        return Handle::BottomLeft;
    if (horizontal > 0 && vertical > 0)
        return Handle::BottomRight;
    if (horizontal < 0)
        return Handle::Left;
    if (horizontal > 0)
        return Handle::Right;
    if (vertical < 0)
        return Handle::Top;
    if (vertical > 0)
        return Handle::Bottom;
    return rect.contains(point) ? Handle::Move : Handle::None;
}

QRect spanned(const QPoint &from, const QPoint &to, const QSize &image)
{
    const int x1 = std::clamp(from.x(), 0, image.width());
    const int x2 = std::clamp(to.x(), 0, image.width());
    const int y1 = std::clamp(from.y(), 0, image.height());
    const int y2 = std::clamp(to.y(), 0, image.height());
    return QRect(std::min(x1, x2), std::min(y1, y2), std::abs(x2 - x1), std::abs(y2 - y1));
}

namespace {

// Keeps the edges low and high within 0 and limit, in order, at least one apart.
void order(int &low, int &high, int limit)
{
    low = std::clamp(low, 0, limit);
    high = std::clamp(high, 0, limit);
    if (low > high)
        std::swap(low, high);
    if (low == high) {
        if (high < limit)
            ++high;
        else
            --low;
    }
}

} // namespace

QRect dragged(const QRect &start, Handle handle, const QPoint &delta, const QSize &image)
{
    if (handle == Handle::None || image.isEmpty())
        return start;
    if (handle == Handle::Move) {
        const int x = std::clamp(start.x() + delta.x(), 0, std::max(0, image.width() - start.width()));
        const int y = std::clamp(start.y() + delta.y(), 0, std::max(0, image.height() - start.height()));
        return QRect(QPoint(x, y), start.size());
    }

    int left = start.x();
    int right = start.x() + start.width();
    int top = start.y();
    int bottom = start.y() + start.height();
    if (handle == Handle::Left || handle == Handle::TopLeft || handle == Handle::BottomLeft)
        left += delta.x();
    if (handle == Handle::Right || handle == Handle::TopRight || handle == Handle::BottomRight)
        right += delta.x();
    if (handle == Handle::Top || handle == Handle::TopLeft || handle == Handle::TopRight)
        top += delta.y();
    if (handle == Handle::Bottom || handle == Handle::BottomLeft || handle == Handle::BottomRight)
        bottom += delta.y();
    order(left, right, image.width());
    order(top, bottom, image.height());
    return QRect(left, top, right - left, bottom - top);
}

} // namespace imagecrop

namespace {

constexpr int margin = 8;           // room around the image for the handles
constexpr qreal grabDistance = 6.0; // how near a handle the mouse must be
constexpr qreal handleSize = 8.0;
constexpr qreal radius = 5.0;       // as AppStyle's rounded fills

Qt::CursorShape cursorFor(imagecrop::Handle handle)
{
    using imagecrop::Handle;
    switch (handle) {
    case Handle::Move:
        return Qt::SizeAllCursor;
    case Handle::Left:
    case Handle::Right:
        return Qt::SizeHorCursor;
    case Handle::Top:
    case Handle::Bottom:
        return Qt::SizeVerCursor;
    case Handle::TopLeft:
    case Handle::BottomRight:
        return Qt::SizeFDiagCursor;
    case Handle::TopRight:
    case Handle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case Handle::None:
        break;
    }
    return Qt::CrossCursor;
}

} // namespace

ImageCropView::ImageCropView(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
}

void ImageCropView::setImage(const QImage &image)
{
    m_image = image;
    m_dragging = false;
    m_crop = QRect();
    updateGeometry();
    update();
    emit cropChanged(m_crop);
}

QImage ImageCropView::image() const { return m_image; }

QRect ImageCropView::crop() const { return m_crop; }

void ImageCropView::setCrop(const QRect &crop)
{
    QRect clamped = crop.normalized().intersected(m_image.rect());
    if (clamped.isEmpty() || clamped == m_image.rect())
        clamped = QRect();
    if (clamped == m_crop)
        return;
    m_crop = clamped;
    update();
    emit cropChanged(m_crop);
}

void ImageCropView::clearCrop() { setCrop(QRect()); }

QRect ImageCropView::selection() const { return m_crop.isEmpty() ? m_image.rect() : m_crop; }

QRectF ImageCropView::shownRect() const
{
    return imagecrop::fit(m_image.size(), QRectF(rect()).adjusted(margin, margin, -margin, -margin));
}

QSize ImageCropView::sizeHint() const
{
    const QSize size = m_image.size().isEmpty() ? QSize(400, 300) : m_image.size();
    const QSize bounded = size.scaled(QSize(800, 560), Qt::KeepAspectRatio);
    const QSize shown = size.width() > bounded.width() ? bounded : size;
    return shown.expandedTo(QSize(240, 160)) + QSize(2 * margin, 2 * margin);
}

QSize ImageCropView::minimumSizeHint() const { return QSize(160, 120); }

QRectF ImageCropView::cropInView() const
{
    return m_crop.isEmpty() ? QRectF() : imagecrop::toView(m_crop, shownRect(), m_image.size());
}

void ImageCropView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // A quiet backdrop, the text colour mostly transparent, as AppStyle's
    // fills, so it suits light and dark palettes.
    QColor backdrop = palette().color(QPalette::WindowText);
    backdrop.setAlpha(18);
    painter.setPen(Qt::NoPen);
    painter.setBrush(backdrop);
    painter.drawRoundedRect(QRectF(rect()), radius, radius);

    if (m_image.isNull())
        return;
    const QRectF shown = shownRect();
    painter.drawImage(shown, m_image, QRectF(m_image.rect()));
    if (m_crop.isEmpty())
        return;

    // The image outside the crop is dimmed.
    const QRectF crop = cropInView();
    QPainterPath outside;
    outside.addRect(shown);
    QPainterPath inside;
    inside.addRect(crop);
    painter.setBrush(QColor(0, 0, 0, 128));
    painter.drawPath(outside.subtracted(inside));

    // The crop's border and its handles, in the highlight colour.
    const QColor accent = palette().color(QPalette::Highlight);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(accent, 1.5));
    painter.drawRect(crop);
    painter.setPen(QPen(palette().color(QPalette::Base), 1.0));
    painter.setBrush(accent);
    const qreal xs[] = {crop.left(), crop.center().x(), crop.right()};
    const qreal ys[] = {crop.top(), crop.center().y(), crop.bottom()};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (i == 1 && j == 1)
                continue;
            const QRectF box(xs[i] - handleSize / 2, ys[j] - handleSize / 2, handleSize, handleSize);
            painter.drawRoundedRect(box, 2, 2);
        }
    }
}

void ImageCropView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_image.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPointF point = event->position();
    m_handle = imagecrop::handleAt(cropInView(), point, grabDistance);
    m_startCrop = m_crop;
    m_startPoint = imagecrop::toImage(point, shownRect(), m_image.size());
    m_pressPoint = point;
    m_dragging = true;
    if (m_handle == imagecrop::Handle::None)
        clearCrop();
    event->accept();
}

void ImageCropView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF point = event->position();
    if (!m_dragging) {
        updateCursor(point);
        return;
    }
    if (m_handle == imagecrop::Handle::None) {
        setCrop(imagecrop::spanned(m_startPoint, imagecrop::toImage(point, shownRect(), m_image.size()),
                                   m_image.size()));
    } else {
        // The mouse's movement in image pixels, not clamped, so a handle can
        // follow it to the image's edge from anywhere.
        const QRectF shown = shownRect();
        const QPointF moved = point - m_pressPoint;
        const QPoint delta(int(std::lround(moved.x() * m_image.width() / shown.width())),
                           int(std::lround(moved.y() * m_image.height() / shown.height())));
        setCrop(imagecrop::dragged(m_startCrop, m_handle, delta, m_image.size()));
    }
    event->accept();
}

void ImageCropView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragging) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_dragging = false;
    m_handle = imagecrop::Handle::None;
    updateCursor(event->position());
    event->accept();
}

void ImageCropView::updateCursor(const QPointF &point)
{
    setCursor(cursorFor(imagecrop::handleAt(cropInView(), point, grabDistance)));
}

} // namespace mirante
