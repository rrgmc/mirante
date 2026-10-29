#include "platform/AttachedDialogs.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QPointer>
#include <QTimer>

namespace mirante::platform {

#if defined(Q_OS_MACOS)
// In AttachedDialogsMac.mm.
void attachChildWindow(QWidget *child, QWidget *parent);
void detachChildWindow(QWidget *child, QWidget *parent);
#endif

namespace {

#if defined(Q_OS_MACOS)
// Attaches a dialog once its native window is on screen, and detaches it when
// it hides. Qt makes the native window during show, so the attachment waits
// for the event loop.
class AttachFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *dialog = qobject_cast<QDialog *>(watched);
        if (!dialog)
            return QObject::eventFilter(watched, event);
        if (event->type() == QEvent::Show) {
            if (QWidget *target = attachTarget(dialog)) {
                QPointer<QDialog> child(dialog);
                QPointer<QWidget> parent(target);
                QTimer::singleShot(0, this, [child, parent] {
                    if (child && parent && child->isVisible()) {
                        attachChildWindow(child, parent);
                        child->setProperty("mirante_attachedTo", QVariant::fromValue<QObject *>(parent));
                    }
                });
            }
        } else if (event->type() == QEvent::Hide) {
            if (auto *parent = qobject_cast<QWidget *>(dialog->property("mirante_attachedTo").value<QObject *>())) {
                detachChildWindow(dialog, parent);
                dialog->setProperty("mirante_attachedTo", QVariant());
            }
        }
        return QObject::eventFilter(watched, event);
    }
};
#endif

} // namespace

QWidget *attachTarget(const QDialog *dialog)
{
#if defined(Q_OS_MACOS)
    if (!dialog)
        return nullptr;
    // exec() marks a dialog modal with WA_ShowModal before it sets the modality.
    const bool modal = dialog->windowModality() != Qt::NonModal || dialog->testAttribute(Qt::WA_ShowModal);
    QWidget *parent = dialog->parentWidget();
    QWidget *owner = parent ? parent->window() : nullptr;
    // A dialog of a dialog stays as it is: its parent dialog is attached already.
    if (!modal || !owner || qobject_cast<QDialog *>(owner) || !owner->isVisible())
        return nullptr;
    return owner;
#else
    Q_UNUSED(dialog);
    return nullptr;
#endif
}

void installAttachedDialogs()
{
#if defined(Q_OS_MACOS)
    qApp->installEventFilter(new AttachFilter(qApp));
#endif
}

} // namespace mirante::platform
