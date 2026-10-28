#pragma once

#include "core/LayoutTree.h"

#include <QWidget>

#include <memory>
#include <optional>
#include <vector>

namespace mirante {

class CellFactory;
class SingleViewTracker;

// Shows one layout tree: QSplitter for splits, QTabWidget for tabs, and a
// widget from the CellFactory for each cell. See the M2 plan, part 4.
// Implemented in LayoutView.cpp.
//
// The view never changes the model. It reports user changes with signals, and
// its owner passes the new tree back through setTree().
class LayoutView : public QWidget
{
    Q_OBJECT

public:
    // The factory must outlive the view.
    explicit LayoutView(CellFactory *factory, QWidget *parent = nullptr);
    ~LayoutView() override;

    // Shows a tree. If it has the same shape as the current one, only sizes and
    // current tabs change. Otherwise the widgets are rebuilt, and cell widgets
    // are moved to their new place instead of being made again.
    void setTree(const LayoutNode &tree);
    const LayoutNode &tree() const;

    // Edit mode is on while the dashboard is unlocked.
    void setEditMode(bool on);
    bool editMode() const;

    void setSingleViewTracker(SingleViewTracker *tracker);

    QWidget *cellAt(const NodePath &path) const;
    std::optional<NodePath> pathOf(const QWidget *cell) const;
    std::vector<QWidget *> visibleCells() const;    // skips cells in hidden tabs

    // Takes a cell widget out of its place, for maximising, and leaves a gap
    // until reattachCell() puts it back.
    QWidget *detachCell(const NodePath &path);
    void reattachCell(QWidget *cell);

signals:
    // The user resized a split or changed a tab. Sizes are reported 300 ms
    // after the last splitter move.
    void treeEdited(const mirante::LayoutNode &tree);

    void cellMenuRequested(const mirante::NodePath &path, const QPoint &globalPos);
    void tabMenuRequested(const mirante::NodePath &tabsPath, int index, const QPoint &globalPos);
    // The + button of a tab group, shown in edit mode, was clicked.
    void tabAddRequested(const mirante::NodePath &tabsPath);
    void cellDragStarted(const mirante::NodePath &path);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
