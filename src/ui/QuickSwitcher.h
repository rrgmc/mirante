#pragma once

#include <QFrame>

#include <memory>
#include <vector>

namespace mirante {

class Model;

// One entry the switcher can find.
struct SwitcherEntry
{
    enum class Kind { Group, Project, Task, Item, RootDashboard };
    Kind kind = Kind::Task;
    QString id;
    QString taskId;     // for items: the item's task
    QString label;      // the text matched against the query
    QString detail;     // where it sits, such as "Work / Backend"
};

// The Ctrl+K popup: a line edit over a list ranked by fuzzyMatch().
// Implemented in QuickSwitcher.cpp.
class QuickSwitcher : public QFrame
{
    Q_OBJECT

public:
    QuickSwitcher(const Model &model, QWidget *parent);
    ~QuickSwitcher() override;

    // Clears the query and shows the popup over the parent.
    void popup();

    // Every group, project, task, item and root dashboard in the model.
    static std::vector<SwitcherEntry> candidates(const Model &model);

    // The entries that match the query, best first.
    static std::vector<SwitcherEntry> rank(const std::vector<SwitcherEntry> &entries, const QString &query);

signals:
    void activated(const mirante::SwitcherEntry &entry);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
