#include "DocumentView.h"

#include "DocumentFormat.h"
#include "DocumentSource.h"

#include <mirante/ItemContext.h>
#include <mirante/widgets/MarkdownView.h>

#include <QBuffer>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPdfDocument>
#include <QPdfView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace mirante::modules::document {

namespace {

// A PDF, one page under another, fitted to the width. Ctrl with the wheel
// zooms.
class PdfPages : public QPdfView
{
public:
    explicit PdfPages(QWidget *parent)
        : QPdfView(parent)
        , m_document(new QPdfDocument(this))
    {
        setFrameShape(QFrame::NoFrame);
        setPageMode(QPdfView::PageMode::MultiPage);
        setZoomMode(QPdfView::ZoomMode::FitToWidth);
        setDocument(m_document);
    }

    // The document reads from m_buffer, so it goes first.
    ~PdfPages() override
    {
        setDocument(nullptr);
        delete m_document;
    }

    // Shows bytes; false if they are not a PDF that can be read.
    bool setBytes(const QByteArray &bytes)
    {
        const int scroll = verticalScrollBar()->value();
        m_document->close();
        m_bytes = bytes;
        m_buffer.close();
        m_buffer.setData(m_bytes);
        m_buffer.open(QIODevice::ReadOnly);
        m_document->load(&m_buffer);
        if (m_document->status() != QPdfDocument::Status::Ready)
            return false;
        verticalScrollBar()->setValue(scroll);
        return true;
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        if (!(event->modifiers() & Qt::ControlModifier)) {
            QPdfView::wheelEvent(event);
            return;
        }
        const qreal steps = event->angleDelta().y() / 120.0;
        const qreal factor = std::clamp(zoomFactor() * std::pow(1.15, steps), 0.25, 8.0);
        setZoomMode(QPdfView::ZoomMode::Custom);
        setZoomFactor(factor);
        event->accept();
    }

private:
    QPdfDocument *m_document;
    QByteArray m_bytes;
    QBuffer m_buffer;
};

class DocumentView : public QWidget
{
public:
    DocumentView(ItemContext &context, std::unique_ptr<DocumentSource> source, bool openLocalLinks, QWidget *parent)
        : QWidget(parent)
        , m_context(context)
        , m_source(std::move(source))
        , m_openLocalLinks(openLocalLinks)
    {
        m_fetcher = new ResourceFetcher(this);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto *bar = new QHBoxLayout;
        bar->setContentsMargins(4, 2, 4, 2);
        bar->setSpacing(2);
        auto *reload = makeButton(QCoreApplication::translate("document", "Reload"), QStringLiteral("reload"));
        auto *open = makeButton(QCoreApplication::translate("document", "Open"), QStringLiteral("open"));
        open->setToolTip(QCoreApplication::translate("document", "Open the file in the program the system uses for it"));
        bar->addWidget(reload);
        bar->addWidget(open);
        m_problem = new QLabel(this);
        m_problem->setObjectName(QStringLiteral("problem"));
        m_problem->setWordWrap(true);
        m_problem->setForegroundRole(QPalette::PlaceholderText);
        m_problem->hide();
        bar->addWidget(m_problem, 1);
        bar->addStretch(0);
        layout->addLayout(bar);

        m_stack = new QStackedWidget(this);
        m_message = new QLabel(m_stack);
        m_message->setObjectName(QStringLiteral("message"));
        m_message->setAlignment(Qt::AlignCenter);
        m_message->setWordWrap(true);
        m_message->setMargin(12);
        m_message->setForegroundRole(QPalette::PlaceholderText);
        m_stack->addWidget(m_message);
        layout->addWidget(m_stack, 1);

        connect(reload, &QToolButton::clicked, this, [this] { m_source->load(); });
        connect(open, &QToolButton::clicked, this, [this] {
            if (m_source->location().isValid())
                QDesktopServices::openUrl(m_source->location());
        });
        connect(m_source.get(), &DocumentSource::loaded, this,
                [this](const QByteArray &bytes, const QString &fileName, const QString &contentType) {
                    show(bytes, fileName, contentType);
                });
        connect(m_source.get(), &DocumentSource::unchanged, this, [this] { m_problem->hide(); });
        connect(m_source.get(), &DocumentSource::failed, this, [this](const QString &message) { fail(message); });
        connect(m_source.get(), &DocumentSource::changed, this, [this] { m_source->load(); });

        showMessage(QCoreApplication::translate("document", "Loading…"));
        m_source->load();
    }

private:
    QToolButton *makeButton(const QString &text, const QString &name)
    {
        auto *button = new QToolButton(this);
        button->setObjectName(name);
        button->setText(text);
        button->setAutoRaise(true);
        return button;
    }

    void showMessage(const QString &text)
    {
        m_message->setText(text);
        m_stack->setCurrentWidget(m_message);
        m_problem->hide();
        m_shown = false;
    }

    void fail(const QString &message)
    {
        if (m_shown) {
            m_problem->setText(QCoreApplication::translate("document", "Could not reload: %1").arg(message));
            m_problem->show();
        } else {
            showMessage(message);
        }
    }

    void show(const QByteArray &bytes, const QString &fileName, const QString &contentType)
    {
        const Format format
            = chooseFormat(m_context.config().value(kFormatKey).toString(), fileName, contentType);
        if (format == Format::Unknown) {
            showMessage(QCoreApplication::translate("document",
                                                    "This file's format is not known. Choose one in the item's settings."));
            return;
        }
        if (bytes.size() > sizeLimit(format)) {
            showMessage(QCoreApplication::translate("document", "This file is too large to show."));
            return;
        }
        QWidget *page = nullptr;
        switch (format) {
        case Format::Markdown:
            page = showMarkdown(decodeText(bytes));
            break;
        case Format::Text:
            page = showText(decodeText(bytes));
            break;
        case Format::Pdf:
            page = showPdf(bytes);
            break;
        case Format::Unknown:
            break;
        }
        if (!page)
            return;
        m_stack->setCurrentWidget(page);
        m_problem->hide();
        m_shown = true;
    }

    QWidget *showMarkdown(const QString &text)
    {
        if (!m_markdown) {
            m_markdown = new widgets::MarkdownView(m_stack);
            m_markdown->setOpenLocalLinks(m_openLocalLinks);
            m_markdown->setResourceFetcher(
                [this](const QUrl &url, QObject *context, std::function<void(const QByteArray &)> done) {
                    m_fetcher->fetch(url, context, std::move(done));
                });
            m_stack->addWidget(m_markdown);
        }
        m_fetcher->setLocalRoot(m_source->localRoot());
        m_markdown->setMarkdownText(text, m_source->location());
        return m_markdown;
    }

    QWidget *showText(const QString &text)
    {
        if (!m_text) {
            m_text = new QPlainTextEdit(m_stack);
            m_text->setReadOnly(true);
            m_text->setFrameShape(QFrame::NoFrame);
            m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            m_text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
            m_stack->addWidget(m_text);
        }
        if (m_text->toPlainText() != text) {
            const int scroll = m_text->verticalScrollBar()->value();
            m_text->setPlainText(text);
            m_text->verticalScrollBar()->setValue(scroll);
        }
        return m_text;
    }

    QWidget *showPdf(const QByteArray &bytes)
    {
        if (!m_pdf) {
            m_pdf = new PdfPages(m_stack);
            m_stack->addWidget(m_pdf);
        }
        if (!m_pdf->setBytes(bytes)) {
            showMessage(QCoreApplication::translate("document", "The PDF file cannot be read."));
            return nullptr;
        }
        return m_pdf;
    }

    ItemContext &m_context;
    std::unique_ptr<DocumentSource> m_source;
    bool m_openLocalLinks = false;
    ResourceFetcher *m_fetcher = nullptr;
    QLabel *m_problem = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_message = nullptr;
    widgets::MarkdownView *m_markdown = nullptr;
    QPlainTextEdit *m_text = nullptr;
    PdfPages *m_pdf = nullptr;
    bool m_shown = false;
};

} // namespace

QWidget *createDocumentView(ItemContext &context, std::unique_ptr<DocumentSource> source, bool openLocalLinks,
                            QWidget *parent)
{
    return new DocumentView(context, std::move(source), openLocalLinks, parent);
}

QWidget *createChooseFolderView(ItemContext &context, const QString &folderId, QWidget *parent)
{
    auto *view = new QWidget(parent);
    auto *layout = new QVBoxLayout(view);
    layout->addStretch(1);
    auto *label = new QLabel(QCoreApplication::translate("document", "Folder not set on this device"), view);
    label->setAlignment(Qt::AlignCenter);
    label->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(label);
    auto *choose = new QPushButton(QCoreApplication::translate("document", "Choose folder…"), view);
    choose->setObjectName(QStringLiteral("chooseFolder"));
    layout->addWidget(choose, 0, Qt::AlignHCenter);
    layout->addStretch(1);
    QObject::connect(choose, &QPushButton::clicked, view, [&context, folderId] { context.mapFolder(folderId); });
    return view;
}

} // namespace mirante::modules::document
