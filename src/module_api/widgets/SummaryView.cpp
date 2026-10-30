#include <mirante/widgets/ActionRow.h>
#include <mirante/widgets/SummaryView.h>

namespace mirante::widgets {

struct SummaryView::Private
{
    ActionRow *actions = nullptr;
};

SummaryView::SummaryView(QWidget *parent)
    : QScrollArea(parent)
    , d(std::make_unique<Private>())
{
    d->actions = new ActionRow(this);
}

SummaryView::~SummaryView() = default;

void SummaryView::setTitle(const QString &title) { Q_UNUSED(title) }

void SummaryView::setStateRow(QWidget *row) { Q_UNUSED(row) }

ActionRow *SummaryView::actions() const { return d->actions; }

void SummaryView::setSection(const QString &key, const QString &title, QWidget *content)
{
    Q_UNUSED(key)
    Q_UNUSED(title)
    Q_UNUSED(content)
}

void SummaryView::setSectionVisible(const QString &key, bool visible)
{
    Q_UNUSED(key)
    Q_UNUSED(visible)
}

bool SummaryView::isSectionVisible(const QString &key) const
{
    Q_UNUSED(key)
    return false;
}

void SummaryView::setMessage(const QString &text) { Q_UNUSED(text) }

} // namespace mirante::widgets
