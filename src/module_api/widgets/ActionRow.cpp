#include <mirante/widgets/ActionRow.h>

#include <mirante/widgets/FlowRow.h>
#include <mirante/widgets/LinkStyle.h>
#include <mirante/widgets/StatusPill.h>

#include <QDesktopServices>
#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace mirante::widgets {

namespace {

// The error line. It takes its colour from the status colours, and follows a
// change of palette.
class ErrorLine : public QLabel
{
public:
    explicit ErrorLine(QWidget *parent)
        : QLabel(parent)
    {
        // The text often comes from a server.
        setTextFormat(Qt::PlainText);
        setWordWrap(true);
        updateColour();
    }

protected:
    void changeEvent(QEvent *event) override
    {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::PaletteChange)
            updateColour();
    }

private:
    void updateColour()
    {
        const QColor colour = statusColours(StatusRole::Bad, palette()).text;
        // Setting the palette sends another change, so stop when it holds.
        if (palette().color(QPalette::WindowText) == colour)
            return;
        QPalette changed = palette();
        changed.setColor(QPalette::WindowText, colour);
        setPalette(changed);
    }
};

// Only pages on the web open from here. An address can come from a server, and
// opening another scheme could start a program.
bool isWebAddress(const QUrl &url)
{
    const QString scheme = url.scheme();
    return scheme == QLatin1String("https") || scheme == QLatin1String("http");
}

} // namespace

struct ActionRow::Private : public QObject
{
    struct Action
    {
        QPushButton *button = nullptr;
        QString label;
        QString shortLabel;
        // The button's width with its full label.
        int fullWidth = 0;
    };

    QWidget *buttons = nullptr;
    FlowRow *flow = nullptr;
    QPushButton *web = nullptr;
    int webWidth = 0;
    ErrorLine *errorLine = nullptr;
    QHash<QString, Action> actions;
    QUrl webLink;

    QPushButton *addButton(const QString &id, const QString &label, const QString &shortLabel)
    {
        Q_ASSERT_X(!actions.contains(id), "ActionRow", "an action id is used twice");
        auto *button = new QPushButton(label, buttons);
        // So that a test or a view can find the button.
        button->setObjectName(id);
        flow->addWidget(button);
        button->show();
        // "Web" stays at the end of the row.
        flow->removeWidget(web);
        flow->addWidget(web);
        actions.insert(id, {button, label, shortLabel.isEmpty() ? label : shortLabel,
                            button->sizeHint().width()});
        updateLabels();
        return button;
    }

    // The short labels apply when the full ones do not fit on one row.
    void updateLabels()
    {
        const int spacing = flow->spacing() >= 0 ? flow->spacing() : 0;
        int needed = web->isHidden() ? 0 : webWidth;
        for (const Action &action : std::as_const(actions)) {
            if (!action.button->isHidden())
                needed += action.fullWidth + spacing;
        }
        const bool narrow = needed > buttons->width();
        for (const Action &action : std::as_const(actions)) {
            const QString &text = narrow ? action.shortLabel : action.label;
            if (action.button->text() != text)
                action.button->setText(text);
        }
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == buttons && event->type() == QEvent::Resize)
            updateLabels();
        return QObject::eventFilter(watched, event);
    }
};

ActionRow::ActionRow(QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    d->buttons = new QWidget(this);
    d->flow = new FlowRow(d->buttons);
    layout->addWidget(d->buttons);
    d->buttons->show();
    d->buttons->installEventFilter(d.get());

    d->web = new QPushButton(tr("Web"), d->buttons);
    d->web->setObjectName(QStringLiteral("web"));
    styleAsLink(d->web);
    d->webWidth = d->web->sizeHint().width();
    d->flow->addWidget(d->web);
    d->web->hide();
    connect(d->web, &QPushButton::clicked, d.get(),
            [this] { QDesktopServices::openUrl(d->webLink); });

    d->errorLine = new ErrorLine(this);
    layout->addWidget(d->errorLine);
    d->errorLine->hide();
}

ActionRow::~ActionRow() = default;

void ActionRow::addAction(const QString &id, const QString &label, const QString &shortLabel,
                          std::function<void()> handler)
{
    QPushButton *button = d->addButton(id, label, shortLabel);
    // A new action makes the error of the last one out of date. Menu actions
    // run from their own menu entries, which clear it through clearError().
    connect(button, &QPushButton::clicked, d.get(), [this, handler = std::move(handler)] {
        clearError();
        if (handler)
            handler();
    });
}

void ActionRow::addLinkAction(const QString &id, const QString &label, const QString &shortLabel,
                              std::function<void()> handler)
{
    addAction(id, label, shortLabel, std::move(handler));
    QPushButton *button = d->actions[id].button;
    styleAsLink(button);
    // The link is narrower than a button with the same label.
    d->actions[id].fullWidth = button->sizeHint().width();
    d->updateLabels();
}

void ActionRow::addMenuAction(const QString &id, const QString &label, const QString &shortLabel,
                              std::function<void(QMenu &)> fill)
{
    QPushButton *button = d->addButton(id, label, shortLabel);
    auto *menu = new QMenu(button);
    connect(menu, &QMenu::aboutToShow, d.get(), [menu, fill = std::move(fill)] {
        menu->clear();
        if (fill)
            fill(*menu);
    });
    button->setMenu(menu);
    // The menu's indicator makes the button wider.
    d->actions[id].fullWidth = button->sizeHint().width();
    d->updateLabels();
}

void ActionRow::setActionVisible(const QString &id, bool visible)
{
    const auto action = d->actions.constFind(id);
    if (action == d->actions.constEnd())
        return;
    action->button->setVisible(visible);
    d->updateLabels();
}

bool ActionRow::isActionVisible(const QString &id) const
{
    const auto action = d->actions.constFind(id);
    return action != d->actions.constEnd() && !action->button->isHidden();
}

void ActionRow::setBusy(const QString &id, bool busy)
{
    const auto action = d->actions.constFind(id);
    if (action != d->actions.constEnd())
        action->button->setEnabled(!busy);
}

void ActionRow::setWebLink(const QUrl &url)
{
    d->webLink = url;
    d->web->setVisible(isWebAddress(url));
    d->updateLabels();
}

QUrl ActionRow::webLink() const { return d->webLink; }

void ActionRow::showError(const QString &text)
{
    d->errorLine->setText(text);
    d->errorLine->setVisible(!text.isEmpty());
}

void ActionRow::clearError() { showError(QString()); }

QString ActionRow::error() const { return d->errorLine->text(); }

} // namespace mirante::widgets
