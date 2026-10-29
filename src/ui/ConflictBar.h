#pragma once

#include "core/Store.h"

#include <QFrame>

#include <functional>
#include <memory>

namespace mirante {

class Model;

// The bar that lists conflict copies made by sync tools, one line each, such
// as "Two versions of task Fix login timeout", with "Keep current", "Use other"
// and "Compare". Compare opens both files side by side, as formatted JSON with
// the differing lines marked, and offers the same two choices. It hides itself
// when there are no conflicts. See docs/sync.md#conflict-copies and the M3
// plan, part 7. Implemented in ConflictBar.cpp.
class ConflictBar : public QFrame
{
    Q_OBJECT

public:
    ConflictBar(Store &store, const Model &model, QWidget *parent = nullptr);
    ~ConflictBar() override;

    // The text of a conflict's line, naming the entity from the model when it
    // can, otherwise the file.
    static QString describe(const Store::Conflict &conflict, const Model &model);

    // Asks the user to confirm "Use other". The default shows a message box.
    void setConfirm(std::function<bool(const QString &question)> confirm);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
