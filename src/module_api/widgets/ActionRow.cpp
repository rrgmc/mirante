#include <mirante/widgets/ActionRow.h>

#include <QMenu>

namespace mirante::widgets {

struct ActionRow::Private
{
    QUrl webLink;
    QString error;
};

ActionRow::ActionRow(QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
}

ActionRow::~ActionRow() = default;

void ActionRow::addAction(const QString &id, const QString &label, const QString &shortLabel,
                          std::function<void()> handler)
{
    Q_UNUSED(id)
    Q_UNUSED(label)
    Q_UNUSED(shortLabel)
    Q_UNUSED(handler)
}

void ActionRow::addMenuAction(const QString &id, const QString &label, const QString &shortLabel,
                              std::function<void(QMenu &)> fill)
{
    Q_UNUSED(id)
    Q_UNUSED(label)
    Q_UNUSED(shortLabel)
    Q_UNUSED(fill)
}

void ActionRow::setActionVisible(const QString &id, bool visible)
{
    Q_UNUSED(id)
    Q_UNUSED(visible)
}

bool ActionRow::isActionVisible(const QString &id) const
{
    Q_UNUSED(id)
    return false;
}

void ActionRow::setBusy(const QString &id, bool busy)
{
    Q_UNUSED(id)
    Q_UNUSED(busy)
}

void ActionRow::setWebLink(const QUrl &url) { d->webLink = url; }

QUrl ActionRow::webLink() const { return d->webLink; }

void ActionRow::showError(const QString &text) { d->error = text; }

void ActionRow::clearError() { d->error.clear(); }

QString ActionRow::error() const { return d->error; }

} // namespace mirante::widgets
