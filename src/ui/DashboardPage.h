#pragma once

#include "core/Model.h"
#include "layout/LayoutServices.h"

#include <QJsonObject>
#include <QString>
#include <QWidget>

#include <memory>

class QLabel;
class QToolButton;

namespace mirante {

class DashboardCellFactory;
class FocusView;
class HeaderPill;
class HeaderRow;
class LayoutView;

// One tab's page: a LayoutView of one dashboard, with its factory, or the
// Focus list. A task dashboard's page has a title bar with its own lock,
// because a task dashboard is edited on its own (decision 0020). A root
// dashboard's lock is the one at the end of the tab bar row, or in the
// toolbar of its detached window. Implemented in DashboardPage.cpp.
class DashboardPage : public QWidget
{
public:
    // The Focus page. Its ref is empty and it has no view.
    DashboardPage(FocusView *focusView, QWidget *parent);

    // With an item id, the page shows only that item of the task, read-only.
    DashboardPage(const LayoutServices &services, const DashboardRef &dashboard, QWidget *parent,
                  const QString &item = QString());

    ~DashboardPage() override;

    bool isFocus() const { return focus != nullptr; }
    // A root dashboard's page. The Focus page has an empty ref too, but is not one.
    bool isRoot() const { return ref.isRoot() && !focus; }

    const DashboardRef ref;
    const QString itemId;           // set for an item page
    const std::unique_ptr<DashboardCellFactory> factory;
    FocusView *focus = nullptr;     // set for the Focus page
    LayoutView *view = nullptr;     // null for the Focus page
    // The title bar of task dashboards and items, in pills as in cell
    // headers: the task, the dashboard or the item, and the project.
    HeaderRow *headerRow = nullptr;
    HeaderPill *title = nullptr;    // the task, bold
    HeaderPill *part = nullptr;     // the dashboard, or the item with its type's colour
    HeaderPill *context = nullptr;  // the project, in a grey pill
    QToolButton *lock = nullptr;    // task dashboards only
    // The model's layout of a task dashboard when it was last shown, to tell
    // an edit of this dashboard from other changes to its task.
    QJsonObject shownLayout;
};

} // namespace mirante
