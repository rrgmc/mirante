#pragma once

#include <QScrollArea>
#include <QString>

#include <memory>

namespace mirante::widgets {

class ActionRow;

// The fixed layout of a summary: a title, a line of state, the actions, then
// named sections, in one column that scrolls. See decision 0066.
class SummaryView : public QScrollArea
{
public:
    explicit SummaryView(QWidget *parent = nullptr);
    ~SummaryView() override;

    void setTitle(const QString &title);
    // Pills and text under the title. The view takes the widget.
    void setStateRow(QWidget *row);
    ActionRow *actions() const;

    // Adds a section or replaces its content, keeping the order of first use
    // and the scroll position. The view takes the widget.
    void setSection(const QString &key, const QString &title, QWidget *content);
    void setSectionVisible(const QString &key, bool visible);
    bool isSectionVisible(const QString &key) const;

    // Shown instead of everything else, such as "Loading…". Empty removes it.
    void setMessage(const QString &text);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante::widgets
