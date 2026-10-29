// Tests JsonHighlighter: keys, strings, numbers, literals and punctuation get
// their own colours, and a colon inside a string is not a key's colon.
#include "ui/JsonHighlighter.h"

#include <QTest>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

using namespace mirante;

class TestJsonHighlighter : public QObject
{
    Q_OBJECT

    // The foreground colour at position pos of the first line.
    static QColor colourAt(const QTextDocument &doc, int pos)
    {
        const QTextBlock block = doc.firstBlock();
        for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
            if (pos >= range.start && pos < range.start + range.length)
                return range.format.foreground().color();
        }
        return {};
    }

private slots:
    void colours()
    {
        QTextDocument doc;
        JsonHighlighter highlighter(&doc);
        doc.setPlainText(QStringLiteral(R"(    "name": "a: b", "order": -2.5e3, "done": true,)"));
        doc.documentLayout();       // gives the blocks their layouts
        highlighter.rehighlight();

        const QColor key = colourAt(doc, 5);            // n of "name"
        const QColor string = colourAt(doc, 13);        // a of "a: b"
        const QColor number = colourAt(doc, 31);        // the point of -2.5e3
        const QColor literal = colourAt(doc, 47);       // u of true
        const QColor punctuation = colourAt(doc, 10);   // the colon after "name"

        for (const QColor &colour : {key, string, number, literal, punctuation})
            QVERIFY(colour.isValid());
        QVERIFY(key != string);
        QVERIFY(string != number);
        QVERIFY(number != literal);
        QVERIFY(literal != punctuation);
        // "order" is a key too, and the colon inside "a: b" stays part of the string.
        QCOMPARE(colourAt(doc, 22), key);               // r of "order"
        QCOMPARE(colourAt(doc, 14), string);            // the colon inside "a: b"
    }
};

QTEST_MAIN(TestJsonHighlighter)
#include "tst_json_highlighter.moc"
