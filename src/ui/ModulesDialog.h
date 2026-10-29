#pragma once

#include <QDialog>

#include <memory>

class QTextBrowser;
class QTreeWidget;

namespace mirante {

class ItemTypeRegistry;

// The Modules window, opened from Help > Modules…. It lists the modules of
// this build by name, each with its item types under it, and shows the
// selected module's or type's description in a pane beside the list. It is
// read-only. See docs/ui.md#modules-window and decision 0056. Implemented in
// ModulesDialog.cpp.
class ModulesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModulesDialog(const ItemTypeRegistry &types, QWidget *parent = nullptr);
    ~ModulesDialog() override;

    // The list: one top-level row per module, sorted by name, with a child
    // row per item type, sorted by name. A module row keeps the module id in
    // Qt::UserRole; a type row keeps the type id there.
    QTreeWidget *tree() const;
    // The pane that describes the selected row.
    QTextBrowser *details() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
