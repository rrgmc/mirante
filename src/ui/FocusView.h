#pragma once

#include <QStringList>
#include <QWidget>

#include <functional>
#include <memory>

class QMenu;

namespace mirante {

class Model;

// The Focus list of docs/ui.md#focus-view: active tasks of every group, or of
// one group chosen in a combo box, in focusOrder(). Each row shows the task,
// its project and group, and the priority marker. It follows the model's
// changes. The "Needs attention first" toggle arrives in M5. See the M3 plan,
// part 6. Implemented in FocusView.cpp.
class FocusView : public QWidget
{
    Q_OBJECT

public:
    explicit FocusView(const Model &model, QWidget *parent = nullptr);
    ~FocusView() override;

    // The group whose tasks are listed; empty for every group.
    void setGroupFilter(const QString &groupId);
    QString groupFilter() const;

    // The task ids listed, in order. For tests.
    QStringList taskIds() const;

    // Fills the menu a right click on a task opens, with the same commands as
    // the task's menu in the sidebar.
    using MenuFiller = std::function<void(QMenu *menu, const QString &taskId)>;
    void setMenuFiller(MenuFiller filler);

signals:
    // A double click or Enter on a task.
    void taskOpened(const QString &taskId);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
