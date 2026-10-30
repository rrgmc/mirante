#include "NotesView.h"

#include <mirante/ItemContext.h>

#include <QButtonGroup>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonObject>
#include <QKeySequence>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace mirante::modules::notes {

namespace {

const QString kText = QStringLiteral("text");

// How long the text waits after the last change before it is saved.
constexpr int kSaveDelayMs = 800;

QString trNotes(const char *text)
{
    return QCoreApplication::translate("notes", text);
}

// The editor. It tells the view when it loses the keyboard's focus.
class Editor : public QPlainTextEdit
{
public:
    std::function<void()> focusLost;

    explicit Editor(QWidget *parent)
        : QPlainTextEdit(parent)
    {
        setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        setFrameShape(QFrame::NoFrame);
        setTabChangesFocus(false);
    }

protected:
    void focusOutEvent(QFocusEvent *event) override
    {
        QPlainTextEdit::focusOutEvent(event);
        if (focusLost)
            focusLost();
    }
};

// The preview. It loads nothing, so a note cannot call out or read a local
// file, and it opens web and mail links in the external programs.
class Preview : public QTextBrowser
{
public:
    explicit Preview(QWidget *parent)
        : QTextBrowser(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setOpenLinks(false);
        setOpenExternalLinks(false);
        connect(this, &QTextBrowser::anchorClicked, this, [](const QUrl &link) {
            const QString scheme = link.scheme();
            if (scheme == QLatin1String("http") || scheme == QLatin1String("https") || scheme == QLatin1String("mailto"))
                QDesktopServices::openUrl(link);
        });
    }

    // Never answers with nothing, because the document would then read a file
    // or network path in the Markdown itself.
    QVariant loadResource(int type, const QUrl &name) override
    {
        Q_UNUSED(name)
        if (type == QTextDocument::ImageResource) {
            QImage blank(1, 1, QImage::Format_ARGB32);
            blank.fill(Qt::transparent);
            return blank;
        }
        return QString(QLatin1Char(' '));
    }
};

class NotesView : public QWidget
{
public:
    NotesView(ItemContext &context, QWidget *parent)
        : QWidget(parent)
        , m_context(context)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // The thin bar with the two modes.
        auto *bar = new QHBoxLayout;
        bar->setContentsMargins(4, 2, 4, 2);
        bar->setSpacing(2);
        m_editButton = makeModeButton(trNotes("Edit"));
        m_previewButton = makeModeButton(trNotes("Preview"));
        auto *modes = new QButtonGroup(this);
        modes->setExclusive(true);
        modes->addButton(m_editButton);
        modes->addButton(m_previewButton);
        bar->addWidget(m_editButton);
        bar->addWidget(m_previewButton);
        bar->addStretch(1);
        layout->addLayout(bar);

        m_stack = new QStackedWidget(this);
        m_editor = new Editor(m_stack);
        m_preview = new Preview(m_stack);
        m_stack->addWidget(m_editor);
        m_stack->addWidget(m_preview);
        layout->addWidget(m_stack, 1);

        m_saveTimer.setSingleShot(true);
        m_saveTimer.setInterval(kSaveDelayMs);
        connect(&m_saveTimer, &QTimer::timeout, this, [this] { save(); });

        const QString text = m_context.config().value(kText).toString();
        m_editor->setPlainText(text);
        connect(m_editor, &QPlainTextEdit::textChanged, this, [this] {
            if (m_loading)
                return;
            m_unsaved = true;
            m_saveTimer.start();
        });
        m_editor->focusLost = [this] { save(); };

        connect(m_editButton, &QToolButton::clicked, this, [this] { showMode(false); });
        connect(m_previewButton, &QToolButton::clicked, this, [this] { showMode(true); });

        // Only while the keyboard's focus is in this view, so two notes on
        // screen each keep their own.
        auto *toggle = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this);
        toggle->setContext(Qt::WidgetWithChildrenShortcut);
        connect(toggle, &QShortcut::activated, this, [this] {
            showMode(m_stack->currentWidget() == m_editor);
            focusMode();
        });

        showMode(!text.isEmpty());

        m_context.onConfigChanged(this, [this](const QJsonObject &config) { takeChange(config); });
    }

    ~NotesView() override
    {
        // Hiding the view as it goes moves the focus away from the editor,
        // after this object's members are gone.
        m_editor->focusLost = nullptr;
        m_closing = true;
        if (m_unsaved)
            save();
    }

private:
    QToolButton *makeModeButton(const QString &text)
    {
        auto *button = new QToolButton(this);
        button->setText(text);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolTip(trNotes("Switch between Edit and Preview (%1)")
                               .arg(QKeySequence(Qt::CTRL | Qt::Key_E).toString(QKeySequence::NativeText)));
        return button;
    }

    void showMode(bool preview)
    {
        if (preview) {
            save();
            m_preview->setMarkdown(m_editor->toPlainText());
            m_stack->setCurrentWidget(m_preview);
        } else {
            m_stack->setCurrentWidget(m_editor);
        }
        m_editButton->setChecked(!preview);
        m_previewButton->setChecked(preview);
    }

    // Keeps the keyboard's focus in the view after a switch, so Ctrl+E
    // switches back.
    void focusMode() { m_stack->currentWidget()->setFocus(Qt::ShortcutFocusReason); }

    // Writes the text into the configuration, keeping its other keys. Empty
    // text removes the key.
    void save()
    {
        m_saveTimer.stop();
        if (!m_unsaved)
            return;
        m_unsaved = false;
        const QJsonObject stored = m_context.config();
        QJsonObject config = stored;
        const QString text = m_editor->toPlainText();
        if (text.isEmpty())
            config.remove(kText);
        else
            config.insert(kText, text);
        if (config != stored)
            m_context.updateConfig(config);
    }

    // A change of the text here or on another device. An unsaved change wins,
    // and is saved later over it.
    void takeChange(const QJsonObject &config)
    {
        if (m_closing || m_unsaved)
            return;
        const QString text = config.value(kText).toString();
        if (text == m_editor->toPlainText())
            return;
        m_loading = true;
        m_editor->setPlainText(text);
        m_loading = false;
        if (m_stack->currentWidget() == m_preview)
            m_preview->setMarkdown(text);
    }

    ItemContext &m_context;
    QToolButton *m_editButton = nullptr;
    QToolButton *m_previewButton = nullptr;
    QStackedWidget *m_stack = nullptr;
    Editor *m_editor = nullptr;
    Preview *m_preview = nullptr;
    QTimer m_saveTimer;
    bool m_unsaved = false;
    bool m_loading = false;
    bool m_closing = false;
};

} // namespace

QWidget *createNotesView(ItemContext &context, QWidget *parent)
{
    return new NotesView(context, parent);
}

} // namespace mirante::modules::notes
