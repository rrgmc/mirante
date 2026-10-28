#pragma once

#include <QDialog>

#include <functional>
#include <memory>

namespace mirante {

class Model;

// The list of all root dashboards, opened with the button next to the tab bar
// or from its context menu. Closing a tab only hides a root dashboard; this is
// where one is opened again or deleted. Implemented in RootDashboardsDialog.cpp.
class RootDashboardsDialog : public QDialog
{
    Q_OBJECT

public:
    // isOpen says whether a root dashboard has a tab, to mark closed ones.
    RootDashboardsDialog(Model &model, std::function<bool(const QString &id)> isOpen, QWidget *parent = nullptr);
    ~RootDashboardsDialog() override;

    // Asks the user to confirm a deletion. The default shows a message box;
    // tests replace it.
    void setConfirmDelete(std::function<bool(const QString &name)> confirm);

    // The ids in the list, in tab order.
    QStringList ids() const;

    void select(const QString &id);
    void openSelected();
    void deleteSelected();

signals:
    void openRequested(const QString &id);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
