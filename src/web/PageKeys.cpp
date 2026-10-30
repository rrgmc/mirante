#include "web/PageKeys.h"

#include <QChildEvent>
#include <QKeyEvent>
#include <QWidget>

namespace mirante {

PageKeys *PageKeys::install(QWidget *view, Keys keys) { return new PageKeys(view, std::move(keys)); }

PageKeys::PageKeys(QWidget *view, Keys keys)
    : QObject(view)
    , m_keys(std::move(keys))
{
    watch(view);
}

void PageKeys::setKeys(Keys keys) { m_keys = std::move(keys); }

void PageKeys::watch(QObject *object)
{
    if (!object->isWidgetType())
        return;
    object->installEventFilter(this);
    for (QObject *child : object->children())
        watch(child);
}

bool PageKeys::matches(const QList<QKeySequence> &keys, const QKeyCombination &key)
{
    for (const QKeySequence &sequence : keys) {
        if (!sequence.isEmpty() && sequence[0] == key)
            return true;
    }
    return false;
}

QList<QKeySequence> PageKeys::fromStrings(const QStringList &keys)
{
    QList<QKeySequence> sequences;
    for (const QString &key : keys) {
        const QKeySequence sequence = QKeySequence::fromString(key, QKeySequence::PortableText);
        if (!sequence.isEmpty() && sequence[0].key() != Qt::Key_unknown)
            sequences << sequence;
    }
    return sequences;
}

bool PageKeys::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::ChildAdded:
        watch(static_cast<QChildEvent *>(event)->child());
        break;
    case QEvent::ShortcutOverride: {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (m_keys && matches(m_keys(), key->keyCombination())) {
            event->accept();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

} // namespace mirante
