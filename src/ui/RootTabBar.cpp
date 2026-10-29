#include "ui/RootTabBar.h"

#include <QContextMenuEvent>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPointer>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QToolButton>

namespace mirante {

struct RootTabBar::Private
{
    int previewIndex = -1;
    QPointer<QToolButton> pin;      // on the preview tab, if there is one

    // The pin goes on the side without the close button.
    static ButtonPosition pinSide(const QTabBar *bar)
    {
        const auto closeSide = ButtonPosition(bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
        return closeSide == LeftSide ? RightSide : LeftSide;
    }

    // The tab that holds the pin button, or -1.
    int pinIndex(const QTabBar *bar) const
    {
        if (!pin)
            return -1;
        const ButtonPosition side = pinSide(bar);
        for (int i = 0; i < bar->count(); ++i) {
            if (bar->tabButton(i, side) == pin)
                return i;
        }
        return -1;
    }
};

RootTabBar::RootTabBar(QWidget *parent)
    : QTabBar(parent)
    , d(std::make_unique<Private>())
{
    setTabsClosable(true);
    setDrawBase(false);
    setExpanding(false);
    setUsesScrollButtons(true);
    setElideMode(Qt::ElideRight);
}

RootTabBar::~RootTabBar()
{
    // The pin's lambda reads d, which goes before the children.
    delete d->pin;
}

void RootTabBar::setPreviewIndex(int index)
{
    if (index < 0 || index >= count())
        index = -1;
    d->previewIndex = index;

    const ButtonPosition side = Private::pinSide(this);
    const int current = d->pinIndex(this);
    if (current == index && (index < 0 || d->pin))
        return;

    // Move the pin: QTabBar deletes the buttons of a removed tab, so a new
    // button is made each time.
    if (current >= 0)
        setTabButton(current, side, nullptr);
    if (d->pin)
        d->pin->deleteLater();
    d->pin = nullptr;

    if (index >= 0) {
        auto *pin = new QToolButton(this);
        pin->setObjectName(QStringLiteral("pinButton"));
        pin->setIcon(QIcon(QStringLiteral(":/icons/pin.svg")));
        pin->setText(tr("Pin"));
        pin->setToolTip(tr("Pin this task"));
        pin->setAutoRaise(true);
        pin->setFocusPolicy(Qt::NoFocus);
        connect(pin, &QToolButton::clicked, this, [this] {
            const int at = d->pinIndex(this);
            if (at >= 0)
                emit pinRequested(at);
        });
        setTabButton(index, side, pin);
        d->pin = pin;
    }
    update();
}

int RootTabBar::previewIndex() const { return d->previewIndex; }

void RootTabBar::paintEvent(QPaintEvent *event)
{
    // Draws the tabs as QTabBar does, but with an italic title for the preview
    // tab. The current tab is drawn last so that it overlaps its neighbours.
    QStylePainter painter(this);
    const QFont normal = font();
    QFont italic = normal;
    italic.setItalic(true);

    auto drawTab = [&](int index) {
        QStyleOptionTab option;
        initStyleOption(&option, index);
        if (!option.rect.intersects(event->rect()))
            return;
        painter.drawControl(QStyle::CE_TabBarTabShape, option);
        painter.setFont(index == d->previewIndex ? italic : normal);
        painter.drawControl(QStyle::CE_TabBarTabLabel, option);
        painter.setFont(normal);
    };

    const int selected = currentIndex();
    for (int i = 0; i < count(); ++i) {
        if (i != selected && isTabVisible(i))
            drawTab(i);
    }
    if (selected >= 0 && isTabVisible(selected))
        drawTab(selected);
}

// A middle click on a tab closes it, as in web browsers.
void RootTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        const int index = tabAt(event->position().toPoint());
        if (index >= 0) {
            emit tabCloseRequested(index);
            return;
        }
    }
    QTabBar::mouseReleaseEvent(event);
}

void RootTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    const int index = tabAt(event->pos());
    QMenu menu(this);
    QAction *add = menu.addAction(tr("New dashboard"));
    QAction *close = index >= 0 ? menu.addAction(tr("Close tab")) : nullptr;
    // Only root dashboard tabs can be detached. Their tab data says so.
    QAction *detach = index >= 0 && tabData(index).toBool() ? menu.addAction(tr("Detach into Window")) : nullptr;
    menu.addSeparator();
    QAction *list = menu.addAction(tr("All dashboards…"));
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == add)
        emit newDashboardRequested();
    else if (chosen && chosen == close)
        emit tabCloseRequested(index);
    else if (chosen && chosen == detach)
        emit detachRequested(index);
    else if (chosen == list)
        emit dashboardListRequested();
}

} // namespace mirante
