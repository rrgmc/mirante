#pragma once

#include <QImage>
#include <QPersistentModelIndex>
#include <QTreeView>

namespace mirante {

// The sidebar's tree view. It paints row backgrounds as rounded shapes inset
// from the edges, as macOS sidebars do: the selection in the highlight colour,
// a light tint of the group colour on project rows, and the hover tint of SidebarDelegate. The
// view and the delegate draw no background of their own. The tree has the
// window colour as its background.
//
// Besides the model's own drags, a task row or its Items row takes an image
// dropped from another app, as ImagePaste::imageFromMime() reads it, and
// tints that row while the image is over it. Other rows, and every row of
// the Trash list, refuse it. See
// docs/decisions/0085-drop-images-from-other-apps.md. The same rows take a
// local file that is not an image, which the item types may show. See
// docs/decisions/0104-document-items.md.
// Implemented in SidebarTree.cpp.
class SidebarTree : public QTreeView
{
    Q_OBJECT

public:
    explicit SidebarTree(QWidget *parent = nullptr);

signals:
    // An image was dropped on the row of a task or of its Items list. It is
    // emitted once the drop is over, so the drag's source is not kept
    // waiting while a dialog is open.
    void imageDropped(const QString &taskId, const QImage &image);
    // A local file that is not an image was dropped on the row of a task or
    // of its Items list. Emitted once the drop is over, as imageDropped().
    void fileDropped(const QString &taskId, const QString &path);

protected:
    void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    // Whether mime is one of the model's own drags.
    bool isOwnDrag(const QMimeData *mime) const;
    // The row at pos that takes an image drop: a task or its Items row, not
    // in the Trash list. Otherwise an invalid index.
    QModelIndex imageDropRow(const QPoint &pos) const;
    // Whether an image or a file from another app is being dragged.
    bool draggingFromOutside() const { return !m_dragImage.isNull() || !m_dragFile.isEmpty(); }
    // Sets the row tinted for the dragged image, or none.
    void setImageDropRow(const QModelIndex &index);
    void endImageDrag();

    QImage m_dragImage;     // the image being dragged over the tree, if any
    QString m_dragFile;     // else the local file being dragged, if any
    QPersistentModelIndex m_imageDropRow;
};

} // namespace mirante
