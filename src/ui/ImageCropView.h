#pragma once

#include <QImage>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QWidget>

namespace mirante {

// The maths of ImageCropView, kept apart so tests can check it. A crop is a
// rectangle in image pixels. Its edges lie on pixel boundaries, from 0 to the
// image's width or height.
namespace imagecrop {

// The part of a crop rectangle that the mouse grabs.
enum class Handle { None, Move, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };

// Where an image of the given size is drawn in area: as large as fits, but
// never larger than its own size, keeping its shape, centred.
QRectF fit(const QSize &image, const QRectF &area);

// The pixel boundary nearest to a point of the view, clamped to the image.
// shown is where the image is drawn, as fit() returns it.
QPoint toImage(const QPointF &point, const QRectF &shown, const QSize &image);

// Where a rectangle in image pixels is drawn in the view.
QRectF toView(const QRect &rect, const QRectF &shown, const QSize &image);

// The handle of a crop drawn at rect in the view, under point. Corners come
// before edges, and edges before the inside. grab is how far from a corner or
// an edge the mouse may be, in view pixels.
Handle handleAt(const QRectF &rect, const QPointF &point, qreal grab);

// The rectangle between two pixel boundaries, clamped to the image. It is
// empty if the two points share a column or a row.
QRect spanned(const QPoint &from, const QPoint &to, const QSize &image);

// The crop that dragging a handle of start by delta, in image pixels, gives.
// Move keeps the size and stops at the image's edges. Any other handle moves
// its edges, stops them at the image's edges, and keeps at least one pixel.
// An edge dragged past the opposite edge turns the rectangle over.
QRect dragged(const QRect &start, Handle handle, const QPoint &delta, const QSize &image);

} // namespace imagecrop

// Shows an image scaled to fit, and lets the user draw a crop rectangle on it.
//
// - Dragging outside the crop draws a new one. A click without a drag removes it.
// - Handles on the corners and edges resize the crop. Dragging inside moves it.
// - The crop stays within the image. The area outside it is dimmed.
//
// The crop is in the image's own pixels, whatever the scale it is shown at.
// See docs/ui.md#pasting-images. Implemented in ImageCropView.cpp.
class ImageCropView : public QWidget
{
    Q_OBJECT

public:
    explicit ImageCropView(QWidget *parent = nullptr);

    void setImage(const QImage &image);     // removes the crop
    QImage image() const;

    // The crop in image pixels, or an empty rectangle for none.
    QRect crop() const;
    // Sets the crop, clamped to the image. An empty rectangle, or one that
    // covers the whole image, removes it.
    void setCrop(const QRect &crop);
    void clearCrop();
    // The crop, or the whole image if there is none.
    QRect selection() const;

    // Where the image is drawn in the widget.
    QRectF shownRect() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void cropChanged(const QRect &crop);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF cropInView() const;
    void updateCursor(const QPointF &point);

    QImage m_image;
    QRect m_crop;
    // The drag in progress: the handle, the crop it started with, and the
    // point it started at, in image pixels and in the widget.
    imagecrop::Handle m_handle = imagecrop::Handle::None;
    QRect m_startCrop;
    QPoint m_startPoint;
    QPointF m_pressPoint;
    bool m_dragging = false;
};

} // namespace mirante
