#include "ui/JsonHighlighter.h"

#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QTextDocument>

namespace mirante {

namespace {

QTextCharFormat coloured(const QColor &colour, bool bold = false)
{
    QTextCharFormat f;
    f.setForeground(colour);
    if (bold)
        f.setFontWeight(QFont::DemiBold);
    return f;
}

} // namespace

JsonHighlighter::JsonHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
    // Light text means a dark background, which needs lighter colours.
    const bool dark = QGuiApplication::palette().color(QPalette::Text).lightness() > 128;
    m_key = coloured(dark ? QColor(0x9c, 0xdc, 0xfe) : QColor(0x00, 0x45, 0x9e), true);
    m_string = coloured(dark ? QColor(0xce, 0x91, 0x78) : QColor(0xa3, 0x15, 0x15));
    m_number = coloured(dark ? QColor(0xb5, 0xce, 0xa8) : QColor(0x09, 0x86, 0x58));
    m_literal = coloured(dark ? QColor(0x56, 0x9c, 0xd6) : QColor(0x00, 0x00, 0xff));
    m_punctuation = coloured(QGuiApplication::palette().color(QPalette::PlaceholderText));
}

void JsonHighlighter::highlightBlock(const QString &text)
{
    // A string, with escapes; then, outside strings, a number, a literal or
    // punctuation. A string followed by a colon is a key.
    static const QRegularExpression token(
        QStringLiteral(R"("(?:[^"\\]|\\.)*"(\s*:)?|-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?|\btrue\b|\bfalse\b|\bnull\b|[{}\[\],:])"));
    for (auto it = token.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        const QString found = match.captured(0);
        const qsizetype start = match.capturedStart(0);
        if (found.startsWith(QLatin1Char('"'))) {
            if (match.hasCaptured(1)) {
                const qsizetype colon = match.capturedStart(1);
                setFormat(int(start), int(colon - start), m_key);
                setFormat(int(match.capturedEnd(1) - 1), 1, m_punctuation);
            } else {
                setFormat(int(start), int(found.size()), m_string);
            }
        } else if (found.front().isDigit() || found.front() == QLatin1Char('-')) {
            setFormat(int(start), int(found.size()), m_number);
        } else if (found.front().isLetter()) {
            setFormat(int(start), int(found.size()), m_literal);
        } else {
            setFormat(int(start), int(found.size()), m_punctuation);
        }
    }
}

} // namespace mirante
