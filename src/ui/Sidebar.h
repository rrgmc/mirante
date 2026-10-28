#pragma once

#include <QWidget>

#include <memory>

namespace mirante {

class ItemTypeRegistry;
class Model;

// The sidebar: a tree view on SidebarModel, with a delegate for priority
// markers and dimmed Low entries, two filter buttons and a collapse button.
// See the M2 plan, part 5. Implemented in Sidebar.cpp.
class Sidebar : public QWidget
{
    Q_OBJECT

public:
    Sidebar(const Model &model, const ItemTypeRegistry &types, QWidget *parent = nullptr);
    ~Sidebar() override;

    // Selects and scrolls to a group, project, task or item.
    void reveal(const QString &id);

signals:
    void taskClicked(const QString &taskId);            // opens the preview
    void taskDoubleClicked(const QString &taskId);      // pins it
    void itemClicked(const QString &taskId, const QString &itemId);
    void openInNewRootDashboard(const QString &taskId);
    void collapseRequested();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
