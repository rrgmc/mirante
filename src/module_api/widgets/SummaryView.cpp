#include <mirante/widgets/ActionRow.h>
#include <mirante/widgets/SummaryView.h>

#include <QLabel>
#include <QList>
#include <QScrollBar>
#include <QSet>
#include <QVBoxLayout>

namespace mirante::widgets {

namespace {

constexpr int columnMargin = 8;
constexpr int columnSpacing = 8;
constexpr int sectionSpacing = 4;

// A label for text from a server, which wraps and is never read as markup.
QLabel *plainLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}

QLabel *boldLabel(QWidget *parent, qreal scale)
{
    QLabel *label = plainLabel(parent);
    QFont font = label->font();
    font.setBold(true);
    if (font.pointSizeF() > 0)
        font.setPointSizeF(font.pointSizeF() * scale);
    label->setFont(font);
    return label;
}

// Removes a widget that its layout has already dropped. It is deleted later,
// since this can run in answer to a click on a button inside it. It loses its
// parent now, so that nothing finds it among the view's children any more.
void discard(QWidget *widget)
{
    widget->hide();
    widget->setParent(nullptr);
    widget->deleteLater();
}

} // namespace

struct SummaryView::Private
{
    struct Section
    {
        QString key;
        QWidget *frame = nullptr;
        QLabel *title = nullptr;
        QWidget *content = nullptr;
    };

    QLabel *message = nullptr;
    QWidget *body = nullptr;
    QVBoxLayout *bodyLayout = nullptr;
    QLabel *title = nullptr;
    QVBoxLayout *stateLayout = nullptr;
    QWidget *stateRow = nullptr;
    ActionRow *actions = nullptr;
    QList<Section> sections;
    QSet<QString> hiddenSections;

    Section *section(const QString &key)
    {
        for (Section &candidate : sections) {
            if (candidate.key == key)
                return &candidate;
        }
        return nullptr;
    }
};

SummaryView::SummaryView(QWidget *parent)
    : QScrollArea(parent)
    , d(std::make_unique<Private>())
{
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);

    auto *column = new QWidget;
    auto *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(columnMargin, columnMargin, columnMargin, columnMargin);

    d->message = plainLabel(column);
    columnLayout->addWidget(d->message);
    d->message->hide();

    d->body = new QWidget(column);
    d->bodyLayout = new QVBoxLayout(d->body);
    d->bodyLayout->setContentsMargins(0, 0, 0, 0);
    d->bodyLayout->setSpacing(columnSpacing);
    columnLayout->addWidget(d->body);
    columnLayout->addStretch(1);
    d->body->show();

    d->title = boldLabel(d->body, 1.2);
    d->bodyLayout->addWidget(d->title);
    d->title->hide();

    d->stateLayout = new QVBoxLayout;
    d->stateLayout->setContentsMargins(0, 0, 0, 0);
    d->bodyLayout->addLayout(d->stateLayout);

    d->actions = new ActionRow(d->body);
    d->bodyLayout->addWidget(d->actions);
    d->actions->show();

    setWidget(column);
}

SummaryView::~SummaryView() = default;

void SummaryView::setTitle(const QString &title)
{
    d->title->setText(title);
    d->title->setVisible(!title.isEmpty());
}

void SummaryView::setStateRow(QWidget *row)
{
    if (row == d->stateRow)
        return;
    const int position = verticalScrollBar()->value();
    if (d->stateRow) {
        d->stateLayout->removeWidget(d->stateRow);
        discard(d->stateRow);
    }
    d->stateRow = row;
    if (row) {
        d->stateLayout->addWidget(row);
        row->show();
    }
    verticalScrollBar()->setValue(position);
}

ActionRow *SummaryView::actions() const { return d->actions; }

void SummaryView::setSection(const QString &key, const QString &title, QWidget *content)
{
    // Hiding a widget that has the focus moves the focus, and the scroll area
    // then scrolls to the widget that gets it.
    const int position = verticalScrollBar()->value();

    Private::Section *section = d->section(key);
    if (!section) {
        Private::Section added;
        added.key = key;
        added.frame = new QWidget(d->body);
        auto *layout = new QVBoxLayout(added.frame);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(sectionSpacing);
        added.title = boldLabel(added.frame, 1.0);
        layout->addWidget(added.title);
        added.title->show();
        d->bodyLayout->addWidget(added.frame);
        added.frame->setVisible(!d->hiddenSections.contains(key));
        d->sections.append(added);
        section = &d->sections.last();
    }

    section->title->setText(title);
    if (content != section->content) {
        auto *layout = section->frame->layout();
        if (section->content) {
            layout->removeWidget(section->content);
            discard(section->content);
        }
        section->content = content;
        if (content) {
            layout->addWidget(content);
            // At once and not on the next pass of the event loop, or the
            // layout could first run without the content. The column would be
            // shorter for a moment, which would lose the scroll position.
            content->show();
        }
    }

    verticalScrollBar()->setValue(position);
}

void SummaryView::setSectionVisible(const QString &key, bool visible)
{
    if (visible)
        d->hiddenSections.remove(key);
    else
        d->hiddenSections.insert(key);
    if (Private::Section *section = d->section(key))
        section->frame->setVisible(visible);
}

bool SummaryView::isSectionVisible(const QString &key) const
{
    const Private::Section *section = d->section(key);
    return section && !section->frame->isHidden();
}

void SummaryView::setMessage(const QString &text)
{
    d->message->setText(text);
    d->message->setVisible(!text.isEmpty());
    d->body->setVisible(text.isEmpty());
}

} // namespace mirante::widgets
