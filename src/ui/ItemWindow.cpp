#include "ui/ItemWindow.h"

#include "Version.h"
#include "core/Model.h"
#include "items/ItemTypeRegistry.h"
#include "layout/CellFrame.h"

#include <QKeyEvent>
#include <QVBoxLayout>

namespace mirante {

struct ItemWindow::Private
{
    const Model *model = nullptr;
    QString taskId;
    QString itemId;
};

ItemWindow::ItemWindow(const Model &model, const ItemTypeRegistry &types, const QString &taskId,
                       const QString &itemId, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , d(std::make_unique<Private>())
{
    d->model = &model;
    d->taskId = taskId;
    d->itemId = itemId;
    setAttribute(Qt::WA_DeleteOnClose);

    const Task *task = model.task(taskId);
    const Item *item = model.item(taskId, itemId);
    const ItemType *type = item ? &types.type(item->type) : nullptr;
    const QString name = item ? (item->name.isEmpty() ? type->name() : item->name) : QString();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new CellFrame(CellRef{itemId, QString(), QString()}, item == nullptr, type, name, this));

    setWindowTitle(task ? tr("%1 / %2 - %3").arg(task->title, name, QStringLiteral(MIRANTE_APP_NAME))
                        : QStringLiteral(MIRANTE_APP_NAME));
    if (type)
        setWindowIcon(type->icon());

    // The window shows one item, so it goes when the item goes.
    const auto closeIfGone = [this] {
        if (!d->model->item(d->taskId, d->itemId))
            close();
    };
    connect(&model, &Model::taskChanged, this, [closeIfGone, this](const QString &changed) {
        if (changed == d->taskId)
            closeIfGone();
    });
    connect(&model, &Model::modelReset, this, closeIfGone);
}

ItemWindow::~ItemWindow()
{
    // The model outlives the window, so its signals must not reach d.
    disconnect(d->model, nullptr, this, nullptr);
}

// Escape closes the window. Keys the cell does not use reach the window, so
// this works wherever the focus is inside it.
void ItemWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

QString ItemWindow::taskId() const { return d->taskId; }
QString ItemWindow::itemId() const { return d->itemId; }

} // namespace mirante
