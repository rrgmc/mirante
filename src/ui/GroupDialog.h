#pragma once

#include "core/Entities.h"

#include <QDialog>

#include <memory>

namespace mirante {

// Creates or edits a group: its name. Connections arrive in M5. OK is refused
// while the name is empty. See the M3 plan, part 5. Implemented in GroupDialog.cpp.
class GroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GroupDialog(const Group &group, QWidget *parent = nullptr);
    ~GroupDialog() override;

    // The group with the edited fields; every other field as given.
    Group value() const;

    void setName(const QString &name);    // for tests

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
