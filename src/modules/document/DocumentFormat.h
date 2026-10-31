#pragma once

#include <mirante/Fields.h>

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace mirante::modules::document {

// The formats a document item shows. See docs/item-types.md#documents.
enum class Format { Unknown, Markdown, Text, Pdf };

// The configuration key of the format, and its stored values.
inline const QString kFormatKey = QStringLiteral("format");
inline const QString kFormatAuto = QStringLiteral("auto");
inline const QString kFormatMarkdown = QStringLiteral("markdown");
inline const QString kFormatText = QStringLiteral("text");
inline const QString kFormatPdf = QStringLiteral("pdf");

// The format field: Automatic, Markdown, Plain text or PDF.
Field formatField();

// The format of a file name's extension, or Unknown.
Format formatFromName(const QString &fileName);
// The format of a Content-Type header, such as "text/markdown; charset=utf-8",
// or Unknown. Any text/* type other than Markdown is plain text.
Format formatFromContentType(const QString &contentType);
// The format to show: the setting if it names one, else the extension, else
// the content type. Unknown if none says.
Format chooseFormat(const QString &setting, const QString &fileName, const QString &contentType);

// The largest file of a format that is shown: 5 MB for Markdown and text,
// which are laid out on the main thread, and 50 MB for PDF.
qint64 sizeLimit(Format format);
// The largest file read or downloaded at all.
constexpr qint64 kReadLimit = 50LL * 1024 * 1024;

// The bytes as text: UTF-8, or the encoding of a byte order mark.
QString decodeText(const QByteArray &bytes);

} // namespace mirante::modules::document
