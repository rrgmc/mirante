#pragma once

#include <QDialog>

#include <functional>
#include <memory>

namespace mirante {

class Model;

// Lists the workspace's web profiles, Default first, and creates, renames and
// deletes them. Changes go to the model at once. Default cannot be renamed or
// deleted. A warning says that every item in a profile can read the logins of
// all sites in it. Opened with File > Web Profiles…. See
// docs/ui.md#web-profiles. Implemented in WebProfilesDialog.cpp.
class WebProfilesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebProfilesDialog(Model &model, QWidget *parent = nullptr);
    ~WebProfilesDialog() override;

    // Asks for a name, with a title and a starting text. Returns an empty
    // string when cancelled. The default shows an input dialog; tests replace it.
    void setAskName(std::function<QString(const QString &title, const QString &text)> ask);
    // Asks the user to confirm a deletion. The default shows a message box.
    void setConfirm(std::function<bool(const QString &question)> confirm);

    // The ids in the list, in its order: Default first, then by name.
    QStringList ids() const;
    void select(const QString &id);

    // What the buttons do, on the selected profile. Each asks first.
    void addProfile();
    void renameSelected();
    void deleteSelected();

signals:
    // A profile was deleted from the list. Its logins on this device go next.
    void profileDeleted(const QString &id);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
