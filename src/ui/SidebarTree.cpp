#include "ui/SidebarTree.h"

#include "ui/ImagePaste.h"
#include "ui/SidebarDelegate.h"
#include "ui/SidebarModel.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QPainter>
#include <QTimer>

#include <algorithm>

namespace mirante {

namespace {

constexpr int inset = 6;
constexpr qreal radius = 5.0;

// The first local file of a drag that is a file, not a folder. Empty if it
// carries none.
QString localFileOf(const QMimeData *mime)
{
    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile())
            return url.toLocalFile();
    }
    return {};
}

// How much of the group colour is mixed into a project row. A little more than
// the old highlight tint, since some group colours are lighter.
constexpr qreal projectTint = 0.10;

QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * amount),
                            float(a.greenF() + (b.greenF() - a.greenF()) * amount),
                            float(a.blueF() + (b.blueF() - a.blueF()) * amount));
}

} // namespace

SidebarTree::SidebarTree(QWidget *parent)
    : QTreeView(parent)
{
    // The window colour behind the rows, which Sidebar sets to its own grey.
    QPalette palette = this->palette();
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
        palette.setColor(group, QPalette::Base, palette.color(group, QPalette::Window));
    setPalette(palette);
}

void SidebarTree::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // The whole width of the row, indentation included, inset on both sides.
    const QRectF row(inset, option.rect.top() + 1, viewport()->width() - 2 * inset, option.rect.height() - 2);
    const bool selected = selectionModel() && selectionModel()->isSelected(index);
    const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    const auto *delegate = qobject_cast<const SidebarDelegate *>(itemDelegate());

    QColor fill;
    if (selected) {
        fill = option.palette.color(QPalette::Active, QPalette::Highlight);
    } else {
        // Projects get a light tint of their group's colour, so they stand out
        // between groups and tasks and show their group while its heading is
        // scrolled away. See docs/decisions/0046-group-colours.md.
        if (kind == SidebarModel::Kind::Project) {
            QColor tint = index.data(SidebarModel::GroupColourRole).value<QColor>();
            if (!tint.isValid())
                tint = option.palette.color(QPalette::Highlight);
            fill = mix(option.palette.color(QPalette::Base), tint, projectTint);
        }
        // The row of what the hovered dashboard cell shows, or the row an
        // image dragged from another app would be dropped on.
        if ((delegate && delegate->isHoverTinted(index)) || (m_imageDropRow.isValid() && index == m_imageDropRow)) {
            const QColor base = fill.isValid() ? fill : option.palette.color(QPalette::Base);
            fill = mix(base, option.palette.color(QPalette::Highlight), 0.22);
        }
    }
    if (fill.isValid()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(row, radius, radius);
        painter->restore();
    }

    // The view and the delegate draw the selection with a transparent
    // highlight, so only the text changes colour.
    QStyleOptionViewItem rowOption = option;
    for (const auto group : {QPalette::Active, QPalette::Inactive})
        rowOption.palette.setColor(group, QPalette::Highlight, Qt::transparent);
    QTreeView::drawRow(painter, rowOption, index);
}

bool SidebarTree::isOwnDrag(const QMimeData *mime) const
{
    if (!model())
        return false;
    const QStringList formats = model()->mimeTypes();
    return std::any_of(formats.begin(), formats.end(), [mime](const QString &format) { return mime->hasFormat(format); });
}

QModelIndex SidebarTree::imageDropRow(const QPoint &pos) const
{
    // The Trash list takes no drops at all: its top level is not drop enabled.
    if (!model() || !(model()->flags(QModelIndex()) & Qt::ItemIsDropEnabled))
        return {};
    const QModelIndex index = indexAt(pos);
    const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    if (!index.isValid() || (kind != SidebarModel::Kind::Task && kind != SidebarModel::Kind::ItemList))
        return {};
    return index;
}

void SidebarTree::setImageDropRow(const QModelIndex &index)
{
    if (index == m_imageDropRow)
        return;
    m_imageDropRow = index;
    viewport()->update();
}

void SidebarTree::endImageDrag()
{
    m_dragImage = QImage();
    m_dragFile.clear();
    setImageDropRow({});
}

// An image from another app is copied, never moved: a file manager would
// otherwise delete the file once the drop is accepted. The drag is taken on
// entering anywhere, so the move events keep coming, and each move accepts
// or refuses it by the row under the pointer.
void SidebarTree::dragEnterEvent(QDragEnterEvent *event)
{
    endImageDrag();
    if (!isOwnDrag(event->mimeData()) && (event->possibleActions() & Qt::CopyAction)
        && model() && (model()->flags(QModelIndex()) & Qt::ItemIsDropEnabled)) {
        m_dragImage = ImagePaste::imageFromMime(event->mimeData());
        if (m_dragImage.isNull())
            m_dragFile = localFileOf(event->mimeData());
        if (draggingFromOutside()) {
            setImageDropRow(imageDropRow(event->position().toPoint()));
            event->setDropAction(Qt::CopyAction);
            event->accept();
            return;
        }
    }
    QTreeView::dragEnterEvent(event);
}

void SidebarTree::dragMoveEvent(QDragMoveEvent *event)
{
    if (!draggingFromOutside()) {
        QTreeView::dragMoveEvent(event);
        return;
    }
    const QModelIndex row = imageDropRow(event->position().toPoint());
    setImageDropRow(row);
    if (row.isValid()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    } else {
        event->ignore();
    }
}

void SidebarTree::dragLeaveEvent(QDragLeaveEvent *event)
{
    endImageDrag();
    QTreeView::dragLeaveEvent(event);
}

void SidebarTree::dropEvent(QDropEvent *event)
{
    if (!draggingFromOutside()) {
        QTreeView::dropEvent(event);
        return;
    }
    const QImage image = m_dragImage;
    const QString file = m_dragFile;
    const QModelIndex row = imageDropRow(event->position().toPoint());
    endImageDrag();
    if (!row.isValid()) {
        event->ignore();
        return;
    }
    event->setDropAction(Qt::CopyAction);
    event->accept();
    const QString taskId = row.data(SidebarModel::TaskIdRole).toString();
    if (!image.isNull())
        QTimer::singleShot(0, this, [this, taskId, image] { emit imageDropped(taskId, image); });
    else
        QTimer::singleShot(0, this, [this, taskId, file] { emit fileDropped(taskId, file); });
}

} // namespace mirante
