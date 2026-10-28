#pragma once

#include <QWidget>

#include <memory>

namespace mirante {

class ItemTypeRegistry;
class Model;

// A top-level window that shows one item on its own, outside any dashboard.
// A double click on an item in the sidebar opens it. Escape closes it, and it
// closes itself when the item is removed. Implemented in ItemWindow.cpp.
class ItemWindow : public QWidget
{
    Q_OBJECT

public:
    ItemWindow(const Model &model, const ItemTypeRegistry &types, const QString &taskId, const QString &itemId,
               QWidget *parent = nullptr);
    ~ItemWindow() override;

    QString taskId() const;
    QString itemId() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
