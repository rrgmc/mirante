#include "ui/LogWindow.h"

#include "core/LogBuffer.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace mirante {

namespace {

// Levels from the least to the most severe, as the filter offers them.
int severity(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return 0;
    case QtInfoMsg:
        return 1;
    case QtWarningMsg:
        return 2;
    case QtCriticalMsg:
        return 3;
    case QtFatalMsg:
        return 4;
    }
    return 0;
}

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("debug");
    case QtInfoMsg:
        return QStringLiteral("info");
    case QtWarningMsg:
        return QStringLiteral("warning");
    case QtCriticalMsg:
        return QStringLiteral("critical");
    case QtFatalMsg:
        return QStringLiteral("fatal");
    }
    return QString();
}

QString format(const LogEntry &entry)
{
    QString line = entry.time.toString(QStringLiteral("HH:mm:ss.zzz")) + QLatin1Char(' ') + levelName(entry.type);
    if (!entry.category.isEmpty() && entry.category != QLatin1String("default"))
        line += QLatin1Char(' ') + entry.category;
    return line + QStringLiteral(": ") + entry.message;
}

} // namespace

struct LogWindow::Private
{
    LogBuffer *buffer = nullptr;
    QComboBox *level = nullptr;
    QLineEdit *filter = nullptr;
    QPlainTextEdit *text = nullptr;

    bool shows(const LogEntry &entry) const
    {
        if (severity(entry.type) < level->currentIndex())
            return false;
        const QString needle = filter->text().trimmed();
        return needle.isEmpty() || format(entry).contains(needle, Qt::CaseInsensitive);
    }

    // Adds a line, and keeps the view at the bottom if it was there.
    void append(const LogEntry &entry)
    {
        if (!shows(entry))
            return;
        QScrollBar *bar = text->verticalScrollBar();
        const bool atBottom = bar->value() == bar->maximum();
        text->appendPlainText(format(entry));
        if (atBottom)
            bar->setValue(bar->maximum());
    }

    void refill()
    {
        text->clear();
        for (const LogEntry &entry : buffer->entries())
            append(entry);
        text->verticalScrollBar()->setValue(text->verticalScrollBar()->maximum());
    }
};

LogWindow::LogWindow(LogBuffer &buffer, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , d(std::make_unique<Private>())
{
    d->buffer = &buffer;
    setWindowTitle(tr("Log"));
    resize(900, 500);

    d->level = new QComboBox(this);
    d->level->setObjectName(QStringLiteral("logLevel"));
    d->level->addItems({tr("Debug and above"), tr("Info and above"), tr("Warnings and above"), tr("Errors only")});

    d->filter = new QLineEdit(this);
    d->filter->setObjectName(QStringLiteral("logFilter"));
    d->filter->setPlaceholderText(tr("Filter, such as js or claude.ai"));
    d->filter->setClearButtonEnabled(true);

    auto *clear = new QPushButton(tr("Clear"), this);
    clear->setObjectName(QStringLiteral("logClear"));

    d->text = new QPlainTextEdit(this);
    d->text->setObjectName(QStringLiteral("logText"));
    d->text->setReadOnly(true);
    d->text->setLineWrapMode(QPlainTextEdit::NoWrap);
    d->text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // The buffer keeps 5000 messages; the view keeps as many.
    d->text->setMaximumBlockCount(5000);

    auto *bar = new QHBoxLayout;
    bar->addWidget(new QLabel(tr("Show:"), this));
    bar->addWidget(d->level);
    bar->addWidget(d->filter, 1);
    bar->addWidget(clear);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(bar);
    layout->addWidget(d->text, 1);

    connect(d->level, &QComboBox::currentIndexChanged, this, [this] { d->refill(); });
    connect(d->filter, &QLineEdit::textChanged, this, [this] { d->refill(); });
    connect(clear, &QPushButton::clicked, &buffer, &LogBuffer::clear);
    connect(&buffer, &LogBuffer::cleared, this, [this] { d->text->clear(); });
    connect(&buffer, &LogBuffer::entryAdded, this, [this](const LogEntry &entry) { d->append(entry); });

    d->refill();
}

LogWindow::~LogWindow() = default;

QString LogWindow::shownText() const { return d->text->toPlainText(); }

} // namespace mirante
