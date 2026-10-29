#pragma once

#include <QDialog>

#include <functional>
#include <memory>

namespace mirante {

class Model;

// Lists the workspace's profiles, Default first, and creates, renames and
// deletes them, and clears a profile's web data on this device. Changes go to
// the model at once. Default cannot be renamed or deleted, but can be
// cleared. A warning says that items in one profile share its logins,
// servers and tokens. Module settings are edited from the Modules menu.
// Opened with File > Profiles….
// See docs/ui.md#profiles. Implemented in ProfilesDialog.cpp.
class ProfilesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProfilesDialog(Model &model, QWidget *parent = nullptr);
    ~ProfilesDialog() override;

    // Asks for a name, with a title and a starting text. Returns an empty
    // string when cancelled. The default shows an input dialog; tests replace it.
    void setAskName(std::function<QString(const QString &title, const QString &text)> ask);
    // Asks the user to confirm a deletion. The default shows a message box.
    void setConfirm(std::function<bool(const QString &question)> confirm);
    // Asks the user to confirm clearing the data of the named profile. The
    // default shows a warning with the warning icon; tests replace it.
    void setConfirmClear(std::function<bool(const QString &profileName)> confirm);

    // The ids in the list, in its order: Default first, then by name.
    QStringList ids() const;
    void select(const QString &id);

    // What the buttons do, on the selected profile. Each asks first.
    void addProfile();
    void renameSelected();
    void deleteSelected();
    void clearSelected();

signals:
    // A profile was deleted from the list. Its web logins on this device go next.
    void profileDeleted(const QString &id);
    // The user chose to clear a profile's web logins and site data on this device.
    void profileClearRequested(const QString &id);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
