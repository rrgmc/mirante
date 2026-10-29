#include "ui/SlotTabBar.h"

#include "ui/RootTabBar.h"

#include <QContextMenuEvent>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QStyleOptionTab>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>

namespace mirante {

namespace {

// A long preview title is elided, so the slots leave room for the root tabs.
constexpr int maxSlotWidth = 280;

QTabBar::ButtonPosition closeSide(const QTabBar *bar)
{
    return QTabBar::ButtonPosition(bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
}

} // namespace

SlotTabBar::SlotTabBar(QWidget *parent)
    : QTabBar(parent)
{
    setObjectName(QStringLiteral("slotTabBar"));
    setTabsClosable(true);
    setDrawBase(false);
    setExpanding(false);
    setUsesScrollButtons(false);
    setElideMode(Qt::ElideRight);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    addTab(QIcon(QStringLiteral(":/icons/focus.svg")), tr("Focus"));
    setTabToolTip(FocusSlot, tr("Focus: what to work on now"));
    addTab(QString());

    // Focus cannot be closed.
    const ButtonPosition side = closeSide(this);
    if (QWidget *close = tabButton(FocusSlot, side)) {
        setTabButton(FocusSlot, side, nullptr);
        close->deleteLater();
    }

    // The pin goes on the side without the close button.
    auto *pin = new QToolButton(this);
    pin->setObjectName(QStringLiteral("pinButton"));
    pin->setIcon(QIcon(QStringLiteral(":/icons/pin.svg")));
    pin->setText(tr("Pin"));
    pin->setToolTip(tr("Pin: keep this open in a tab"));
    pin->setAutoRaise(true);
    pin->setFocusPolicy(Qt::NoFocus);
    connect(pin, &QToolButton::clicked, this, &SlotTabBar::pinRequested);
    setTabButton(PreviewSlot, side == LeftSide ? RightSide : LeftSide, pin);

    connect(this, &QTabBar::tabCloseRequested, this, [this](int index) {
        if (index == PreviewSlot && hasPreview())
            emit previewCloseRequested();
    });

    clearPreview();
    setCurrentSlot(-1);
}

SlotTabBar::~SlotTabBar() = default;

void SlotTabBar::setPreview(const QIcon &icon, const QString &title, const QString &toolTip)
{
    setTabIcon(PreviewSlot, icon);
    setTabText(PreviewSlot, title);
    setTabToolTip(PreviewSlot, toolTip);
    setTabVisible(PreviewSlot, true);
    updateGeometry();
}

void SlotTabBar::clearPreview()
{
    setTabVisible(PreviewSlot, false);
    setTabIcon(PreviewSlot, QIcon());
    setTabText(PreviewSlot, QString());
    setTabToolTip(PreviewSlot, QString());
    updateGeometry();
}

bool SlotTabBar::hasPreview() const { return isTabVisible(PreviewSlot); }

void SlotTabBar::setCurrentSlot(int slot)
{
    if (slot == PreviewSlot && !hasPreview())
        slot = -1;
    if (slot >= 0)
        setCurrentIndex(slot);
    tabbar::setSelectionShown(*this, slot >= 0);
}

int SlotTabBar::currentSlot() const { return tabbar::selectionShown(*this) ? currentIndex() : -1; }

QSize SlotTabBar::tabSizeHint(int index) const
{
    QSize size = QTabBar::tabSizeHint(index);
    size.setWidth(std::min(size.width(), maxSlotWidth));
    return size;
}

void SlotTabBar::paintEvent(QPaintEvent *event)
{
    tabbar::paintTabs(
        *this, event->rect(), [this](QStyleOptionTab *option, int index) { initStyleOption(option, index); },
        PreviewSlot, tabbar::selectionShown(*this));
}

// A left click selects a slot. QTabBar's tabBarClicked() comes for any button.
void SlotTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = tabAt(event->position().toPoint());
        if (index >= 0)
            emit slotClicked(index);
    }
    QTabBar::mousePressEvent(event);
}

// A middle click on the preview closes it, as on a tab.
void SlotTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        if (tabAt(event->position().toPoint()) == PreviewSlot && hasPreview())
            emit previewCloseRequested();
        return;
    }
    QTabBar::mouseReleaseEvent(event);
}

// The wheel would change the current index without a click.
void SlotTabBar::wheelEvent(QWheelEvent *event) { event->ignore(); }

void SlotTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    if (tabAt(event->pos()) != PreviewSlot || !hasPreview())
        return;
    QMenu menu(this);
    QAction *pin = menu.addAction(tr("Pin"));
    QAction *close = menu.addAction(tr("Close preview"));
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == pin)
        emit pinRequested();
    else if (chosen == close)
        emit previewCloseRequested();
}

} // namespace mirante
