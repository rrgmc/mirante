#pragma once

#include <QKeySequence>
#include <QList>
#include <QObject>

#include <functional>

class QWidget;

namespace mirante {

// Gives keys to a web page instead of the app's shortcuts. Qt sends a
// ShortcutOverride event to the focused widget before it runs a shortcut;
// accepting it sends the key to the widget. WebEngine accepts it for editing
// keys while a text field of the page has the focus, such as paste, and Ctrl+B
// for bold. This filter accepts it for the keys of the item too. See
// docs/decisions/0088-keys-a-page-keeps.md.
//
// A web view gets its focus proxy, the widget that has the focus, after it is
// made, so the filter also watches the view for new children.
class PageKeys : public QObject
{
public:
    using Keys = std::function<QList<QKeySequence>()>;

    // Installs a filter on view and its children, now and later. keys, which
    // may be empty, is asked on each key, so a change to the item applies at
    // once. The filter is a child of view.
    static PageKeys *install(QWidget *view, Keys keys = {});

    // Changes the item's keys.
    void setKeys(Keys keys);

    // Whether the key of a key event matches the first chord of one of keys.
    static bool matches(const QList<QKeySequence> &keys, const QKeyCombination &key);

    // The keys stored in an item, in QKeySequence::PortableText. Entries that
    // do not parse are left out.
    static QList<QKeySequence> fromStrings(const QStringList &keys);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    PageKeys(QWidget *view, Keys keys);
    void watch(QObject *object);

    Keys m_keys;
};

} // namespace mirante
