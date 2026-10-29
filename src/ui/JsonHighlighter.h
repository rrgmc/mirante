#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class QTextDocument;

namespace mirante {

// Colours JSON as the app writes it, one value per line: keys, strings,
// numbers, true, false and null, and punctuation. The colours suit the
// palette's light or dark text. Used by the conflict compare dialog.
// Implemented in JsonHighlighter.cpp.
class JsonHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit JsonHighlighter(QTextDocument *document);

protected:
    void highlightBlock(const QString &text) override;

private:
    QTextCharFormat m_key;
    QTextCharFormat m_string;
    QTextCharFormat m_number;
    QTextCharFormat m_literal;
    QTextCharFormat m_punctuation;
};

} // namespace mirante
