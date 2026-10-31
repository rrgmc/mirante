#include "DocumentFormat.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QStringConverter>
#include <QStringDecoder>

namespace mirante::modules::document {

Field formatField()
{
    Field field;
    field.key = kFormatKey;
    field.label = QCoreApplication::translate("document", "Format");
    field.type = Field::Type::Choice;
    field.defaultValue = kFormatAuto;
    field.choices = {kFormatAuto, kFormatMarkdown, kFormatText, kFormatPdf};
    field.choiceLabels = {QCoreApplication::translate("document", "Automatic"),
                          QCoreApplication::translate("document", "Markdown"),
                          QCoreApplication::translate("document", "Plain text"),
                          QCoreApplication::translate("document", "PDF")};
    return field;
}

Format formatFromName(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix == QLatin1String("md") || suffix == QLatin1String("markdown"))
        return Format::Markdown;
    if (suffix == QLatin1String("txt") || suffix == QLatin1String("log"))
        return Format::Text;
    if (suffix == QLatin1String("pdf"))
        return Format::Pdf;
    return Format::Unknown;
}

Format formatFromContentType(const QString &contentType)
{
    const QString type = contentType.section(QLatin1Char(';'), 0, 0).trimmed().toLower();
    if (type == QLatin1String("text/markdown") || type == QLatin1String("text/x-markdown"))
        return Format::Markdown;
    if (type == QLatin1String("application/pdf"))
        return Format::Pdf;
    if (type.startsWith(QLatin1String("text/")))
        return Format::Text;
    return Format::Unknown;
}

Format chooseFormat(const QString &setting, const QString &fileName, const QString &contentType)
{
    if (setting == kFormatMarkdown)
        return Format::Markdown;
    if (setting == kFormatText)
        return Format::Text;
    if (setting == kFormatPdf)
        return Format::Pdf;
    if (const Format byName = formatFromName(fileName); byName != Format::Unknown)
        return byName;
    return formatFromContentType(contentType);
}

qint64 sizeLimit(Format format)
{
    return format == Format::Pdf ? kReadLimit : 5LL * 1024 * 1024;
}

QString decodeText(const QByteArray &bytes)
{
    const std::optional<QStringConverter::Encoding> bom = QStringConverter::encodingForData(bytes);
    QStringDecoder decoder(bom.value_or(QStringConverter::Utf8), QStringDecoder::Flag::Default);
    return decoder.decode(bytes);
}

} // namespace mirante::modules::document
