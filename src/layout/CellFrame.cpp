#include "layout/CellFrame.h"

#include "items/ItemContextImpl.h"
#include "items/UnknownItemType.h"
#include "layout/CellFrameSingleView.h"
#include "layout/CellStyle.h"
#include "layout/HeaderPill.h"
#include "layout/SingleViewTracker.h"

#include <mirante/ItemType.h>

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace mirante {

namespace {

// The header of a cell. In edit mode a drag past the drag distance calls onDrag
// once per press.
class CellHeader : public QWidget
{
public:
    using QWidget::QWidget;

    std::function<void()> onDrag;
    bool editMode = false;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressPos = event->position().toPoint();
            m_pressed = true;
            m_dragged = false;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (editMode && m_pressed && !m_dragged && (event->buttons() & Qt::LeftButton)
            && (event->position().toPoint() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragged = true;
            if (onDrag)
                onDrag();
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            m_pressed = false;
        QWidget::mouseReleaseEvent(event);
    }

private:
    QPoint m_pressPos;
    bool m_pressed = false;
    bool m_dragged = false;
};

// The same grey label as the placements' placeholders.
QLabel *centredLabel(const QString &text, QWidget *parent) { return createMessageLabel(text, parent); }

} // namespace

struct CellFrame::Private
{
    std::optional<CellRef> ref;
    bool editMode = false;
    bool focused = false;

    CellHeader *header = nullptr;
    QHBoxLayout *headerLayout = nullptr;
    HeaderRow *row = nullptr;
    QPointer<QWidget> headerWidget;     // shown between the row and the menu button; not owned
    QToolButton *maximiseButton = nullptr;
    bool canMaximise = false;           // a live item cell, not an empty or removed one
    QPointer<QWidget> customBody;       // set by setBody()
    HeaderPill *context = nullptr;
    QStackedWidget *body = nullptr;
    QWidget *elsewhere = nullptr;       // the "Shown in another cell" page
    CellFrameSingleView *singleView = nullptr;
};

CellFrame::CellFrame(const std::optional<CellRef> &ref, bool removed, const ItemType *type, const QString &itemName,
                     QWidget *parent)
    : QFrame(parent)
    , d(std::make_unique<Private>())
{
    d->ref = ref;

    // A cell takes the focus when clicked or tabbed to.
    setFocusPolicy(Qt::StrongFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header: type pill, item name, place pill, menu button.
    d->header = new CellHeader(this);
    d->header->setObjectName(QStringLiteral("cellHeader"));
    cellstyle::tintHeader(d->header, cellstyle::itemHeaderStrength);
    d->header->onDrag = [this] { emit dragStarted(); };
    auto *headerLayout = new QHBoxLayout(d->header);
    d->headerLayout = headerLayout;
    headerLayout->setContentsMargins(6, 1, 2, 1);
    headerLayout->setSpacing(4);

    // A pill with the type's icon and short name, the item name, and for an
    // item from another task a grey pill with its project and task. The place
    // gives way first, then the name, then the type's label. See
    // docs/decisions/0033-cell-header-pills.md.
    auto *typePill = new HeaderPill;
    typePill->setObjectName(QStringLiteral("cellType"));
    if (const auto *unknown = dynamic_cast<const UnknownItemType *>(type)) {
        typePill->setFill(HeaderPill::Fill::Neutral);
        typePill->setText(unknown->moduleId());
    } else if (type) {
        typePill->setFill(HeaderPill::Fill::Module, type->id().section(QLatin1Char('.'), 0, 0));
        typePill->setText(type->shortName());
    }
    if (type) {
        typePill->setIcon(type->icon());
        typePill->setToolTip(type->name());
    }

    auto *name = new HeaderPill;
    name->setObjectName(QStringLiteral("cellName"));
    name->setText(itemName);
    name->setToolTip(itemName);

    d->context = new HeaderPill;
    d->context->setObjectName(QStringLiteral("cellContext"));
    d->context->setFill(HeaderPill::Fill::Neutral);
    d->context->setForegroundRole(QPalette::PlaceholderText);

    d->row = new HeaderRow(d->header);
    d->row->setParts({typePill, name, d->context});
    d->row->setShrinkOrder({d->context, name, typePill});
    headerLayout->addWidget(d->row, 1);

    auto *menuButton = new QToolButton(d->header);
    menuButton->setAutoRaise(true);
    menuButton->setText(QString(QChar(0x22EE)));   // vertical ellipsis
    menuButton->setToolTip(tr("Cell menu"));
    connect(menuButton, &QToolButton::clicked, this,
            [this, menuButton] { emit menuRequested(menuButton->mapToGlobal(QPoint(0, menuButton->height()))); });
    headerLayout->addWidget(menuButton);

    // The maximise button, after the menu button. It is hidden until the
    // factory shows it.
    d->canMaximise = ref && ref->isItem() && !removed;
    d->maximiseButton = new QToolButton(d->header);
    d->maximiseButton->setObjectName(QStringLiteral("maximiseButton"));
    d->maximiseButton->setAutoRaise(true);
    d->maximiseButton->setVisible(false);
    connect(d->maximiseButton, &QToolButton::clicked, this, &CellFrame::maximiseRequested);
    headerLayout->addWidget(d->maximiseButton);
    setMaximised(false);

    layout->addWidget(d->header);

    // Body: a placeholder in M2.
    d->body = new QStackedWidget(this);
    d->body->setAutoFillBackground(true);
    d->body->setBackgroundRole(QPalette::Base);
    layout->addWidget(d->body, 1);

    QString text;
    if (removed) {
        text = tr("Removed");
    } else if (!ref) {
        text = tr("Empty");
    } else if (ref->isItem()) {
        text = itemPlaceholderText(type, itemName, ref->item);
    } else {
        text = tr("Task: %1").arg(ref->task);
    }
    d->body->addWidget(centredLabel(text, d->body));

    // A frame of a singleView item may have to give way to another cell.
    if (ref && ref->isItem() && !removed && type && type->singleView()) {
        d->elsewhere = new QWidget(d->body);
        auto *elsewhereLayout = new QVBoxLayout(d->elsewhere);
        elsewhereLayout->addStretch(1);
        elsewhereLayout->addWidget(centredLabel(tr("Shown in another cell"), d->elsewhere));
        auto *showHere = new QPushButton(tr("Show here"), d->elsewhere);
        elsewhereLayout->addWidget(showHere, 0, Qt::AlignHCenter);
        elsewhereLayout->addStretch(1);
        d->body->addWidget(d->elsewhere);

        d->singleView = new CellFrameSingleView(this, ref->item, [this](bool here) {
            d->body->setCurrentIndex(here ? 0 : 1);
        });
        connect(showHere, &QPushButton::clicked, d->singleView, &CellFrameSingleView::claim);
    }

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) { emit menuRequested(mapToGlobal(pos)); });

    setEditMode(false);
}

void CellFrame::setTint(const QColor &colour)
{
    cellstyle::tintHeader(d->header, cellstyle::itemHeaderStrength, colour);
}

CellFrame::~CellFrame()
{
    if (d->singleView)
        d->singleView->release();
    // The body goes while d exists, because a placement in it may still
    // change the header widget.
    delete d->body;
    d->body = nullptr;
}

std::optional<CellRef> CellFrame::ref() const { return d->ref; }

void CellFrame::setContext(const QString &text)
{
    d->context->setText(text);
    d->context->setToolTip(text);
    d->row->refresh();
}

void CellFrame::setEditMode(bool on)
{
    d->editMode = on;
    d->header->editMode = on;
    if (on)
        d->header->setCursor(Qt::OpenHandCursor);
    else
        d->header->unsetCursor();

    setStyleSheet(cellstyle::border(QStringLiteral("mirante--CellFrame"), palette(), on, d->focused));
}

void CellFrame::setCellFocused(bool focused)
{
    if (d->focused == focused)
        return;
    d->focused = focused;
    setEditMode(d->editMode);
}

bool CellFrame::isCellFocused() const { return d->focused; }

void CellFrame::setMessage(const QString &text)
{
    if (auto *label = qobject_cast<QLabel *>(d->body->widget(0)))
        label->setText(text);
}

// CellFrameSingleView

CellFrameSingleView::CellFrameSingleView(QWidget *frame, const QString &itemId, std::function<void(bool)> showHere)
    : QObject(frame)
    , m_frame(frame)
    , m_itemId(itemId)
    , m_showHere(std::move(showHere))
{
    frame->installEventFilter(this);
}

CellFrameSingleView::~CellFrameSingleView() = default;

void CellFrameSingleView::setTracker(SingleViewTracker *tracker)
{
    if (tracker == m_tracker)
        return;
    release();
    m_tracker = tracker;
    if (tracker) {
        m_connection = connect(tracker, &SingleViewTracker::holderChanged, this, [this](const QString &itemId) {
            if (itemId == m_itemId)
                update();
        });
    }
    update();
}

void CellFrameSingleView::claim()
{
    if (m_tracker)
        m_tracker->setHolder(m_itemId, m_frame);
}

void CellFrameSingleView::release()
{
    disconnect(m_connection);
    if (SingleViewTracker *tracker = m_tracker) {
        m_tracker = nullptr;
        if (tracker->holder(m_itemId) == m_frame)
            tracker->release(m_frame);
    }
}

CellFrameSingleView *CellFrameSingleView::of(QWidget *frame)
{
    return frame ? frame->findChild<CellFrameSingleView *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

bool CellFrameSingleView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_frame && event->type() == QEvent::Show)
        update();
    return QObject::eventFilter(watched, event);
}

void CellFrameSingleView::update()
{
    if (!m_tracker) {
        m_showHere(true);
        return;
    }
    QWidget *holder = m_tracker->holder(m_itemId);
    if (holder == m_frame) {
        m_showHere(true);
        return;
    }
    // A shown cell claims the item unless another shown cell holds it.
    if (m_frame->isVisible() && (!holder || !holder->isVisible())) {
        claim();   // calls update() again through holderChanged
        return;
    }
    m_showHere(false);
}

void CellFrame::setBody(QWidget *body)
{
    if (!body || body == d->customBody || !d->body)
        return;
    // The body is page 0 of the stack. The "Shown in another cell" page, if
    // any, stays page 1, and the current page stays current.
    QWidget *old = d->body->widget(0);
    const int current = d->body->currentIndex();
    d->body->insertWidget(0, body);
    if (old) {
        d->body->removeWidget(old);
        delete old;
    }
    d->body->setCurrentIndex(current < 0 ? 0 : current);
    d->customBody = body;
}

QWidget *CellFrame::body() const { return d->customBody; }

void CellFrame::setHeaderWidget(QWidget *widget)
{
    if (widget == d->headerWidget)
        return;
    if (QWidget *old = d->headerWidget) {
        // The widget belongs to someone else, so it leaves the header but is
        // not deleted.
        d->headerLayout->removeWidget(old);
        old->hide();
        if (old->parentWidget() == d->header)
            old->setParent(nullptr);
    }
    d->headerWidget = widget;
    if (widget) {
        // After the header row, before the menu button.
        d->headerLayout->insertWidget(1, widget, 1);
        widget->show();
    }
}

void CellFrame::setMaximiseButtonVisible(bool visible) { d->maximiseButton->setVisible(visible && d->canMaximise); }

void CellFrame::setMaximised(bool maximised)
{
    const QString shortcut = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M).toString(QKeySequence::NativeText);
    d->maximiseButton->setIcon(
        QIcon(maximised ? QStringLiteral(":/icons/restore.svg") : QStringLiteral(":/icons/maximise.svg")));
    d->maximiseButton->setToolTip(maximised ? tr("Restore (%1)").arg(shortcut) : tr("Maximise (%1)").arg(shortcut));
}

// Stubs of T0. Task T7 of the M5 plan implements them.

void CellFrame::setStatus(const CellStatus &status)
{
    Q_UNUSED(status)
}

CellStatus CellFrame::status() const
{
    return {};
}

void CellFrame::setRefreshVisible(bool visible)
{
    Q_UNUSED(visible)
}

} // namespace mirante
