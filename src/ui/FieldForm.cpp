#include "ui/FieldForm.h"

#include "items/FieldValidation.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>
#include <utility>

namespace mirante {

namespace {

const QSize thumbnailSize(96, 72);

} // namespace

struct FieldForm::Private
{
    // One row of the form. Only the pointers that fit the field's type are set.
    struct Row
    {
        Field field;
        QWidget *editor = nullptr;      // what editorFor() returns
        QLineEdit *line = nullptr;
        QPlainTextEdit *text = nullptr;
        QSpinBox *spin = nullptr;
        QCheckBox *check = nullptr;
        QComboBox *combo = nullptr;
        QLabel *thumbnail = nullptr;    // Image
        QLabel *imageName = nullptr;    // Image
        QString image;                  // Image: the file name in files/
        // Set when the user changed the value since setConfig(). config()
        // writes only such rows, so an untouched value is returned unchanged,
        // even one the editor cannot show.
        bool edited = false;
    };

    EditorContext context;
    std::vector<Row> rows;
    QJsonObject config;
    bool loading = false;

    // The value the editor of row shows, or Undefined for an empty one.
    static QJsonValue value(const Row &row);
    void fillCombo(Row &row, const QJsonValue &value) const;
    void show(Row &row, const QJsonValue &value);
};

QJsonValue FieldForm::Private::value(const Row &row)
{
    switch (row.field.type) {
    case Field::Type::Text:
    case Field::Type::Url: {
        const QString text = row.line->text();
        return text.isEmpty() ? QJsonValue(QJsonValue::Undefined) : QJsonValue(text);
    }
    case Field::Type::MultiLineText: {
        const QString text = row.text->toPlainText();
        return text.isEmpty() ? QJsonValue(QJsonValue::Undefined) : QJsonValue(text);
    }
    case Field::Type::Integer:
        return row.spin->value();
    case Field::Type::Bool:
        return row.check->isChecked();
    case Field::Type::Choice:
    case Field::Type::Folder: {
        const QJsonValue data = QJsonValue::fromVariant(row.combo->currentData());
        return data.isNull() ? QJsonValue(QJsonValue::Undefined) : data;
    }
    case Field::Type::Image:
        return row.image.isEmpty() ? QJsonValue(QJsonValue::Undefined) : QJsonValue(row.image);
    }
    return QJsonValue::Undefined;
}

// Lists the choices or folders, with an empty entry when the field is optional
// or has no value, and an entry for a value that is not in the list.
void FieldForm::Private::fillCombo(Row &row, const QJsonValue &value) const
{
    QComboBox *combo = row.combo;
    combo->clear();
    const bool empty = value.isUndefined() || value.isNull()
        || (value.isString() && value.toString().isEmpty());
    if (!row.field.required || empty)
        combo->addItem(QString(), QVariant());

    if (row.field.type == Field::Type::Choice) {
        for (qsizetype i = 0; i < row.field.choices.size(); ++i) {
            const QString &choice = row.field.choices.at(i);
            const QString label = i < row.field.choiceLabels.size() ? row.field.choiceLabels.at(i) : choice;
            combo->addItem(label, choice);
        }
    } else {
        for (const auto &[id, name] : context.folders)
            combo->addItem(name, id);
    }

    if (empty) {
        combo->setCurrentIndex(0);
        return;
    }
    int index = combo->findData(value.toVariant());
    if (index < 0) {
        const QString text = value.isString() ? value.toString() : value.toVariant().toString();
        combo->addItem(row.field.type == Field::Type::Folder ? FieldForm::tr("Unknown folder (%1)").arg(text)
                                                             : FieldForm::tr("Unknown value (%1)").arg(text),
                       value.toVariant());
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index);
}

void FieldForm::Private::show(Row &row, const QJsonValue &value)
{
    switch (row.field.type) {
    case Field::Type::Text:
    case Field::Type::Url:
        row.line->setText(value.toString());
        break;
    case Field::Type::MultiLineText:
        row.text->setPlainText(value.toString());
        break;
    case Field::Type::Integer:
        row.spin->setValue(value.toInt());
        break;
    case Field::Type::Bool:
        row.check->setChecked(value.toBool());
        break;
    case Field::Type::Choice:
    case Field::Type::Folder:
        fillCombo(row, value);
        break;
    case Field::Type::Image:
        // The form cannot read files/, so an existing image shows its name only.
        row.image = value.toString();
        row.thumbnail->clear();
        row.imageName->setText(row.image.isEmpty() ? FieldForm::tr("No image") : row.image);
        break;
    }
}

FieldForm::FieldForm(std::vector<Field> fields, EditorContext context, QWidget *parent)
    : ItemEditor(parent)
    , d(std::make_unique<Private>())
{
    d->context = std::move(context);
    auto *form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);

    d->rows.reserve(fields.size());
    for (Field &field : fields) {
        Private::Row row;
        row.field = std::move(field);
        d->rows.push_back(std::move(row));
    }

    for (std::size_t i = 0; i < d->rows.size(); ++i) {
        Private::Row &row = d->rows[i];
        // Marks the row edited and tells listeners, unless setConfig() is filling it.
        const auto edited = [this, i] {
            if (d->loading)
                return;
            d->rows[i].edited = true;
            emit changed();
        };

        switch (row.field.type) {
        case Field::Type::Text:
        case Field::Type::Url:
            row.line = new QLineEdit(this);
            row.line->setPlaceholderText(row.field.placeholder);
            connect(row.line, &QLineEdit::textChanged, this, edited);
            row.editor = row.line;
            break;
        case Field::Type::MultiLineText:
            row.text = new QPlainTextEdit(this);
            row.text->setPlaceholderText(row.field.placeholder);
            row.text->setTabChangesFocus(true);
            connect(row.text, &QPlainTextEdit::textChanged, this, edited);
            row.editor = row.text;
            break;
        case Field::Type::Integer:
            row.spin = new QSpinBox(this);
            row.spin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            connect(row.spin, &QSpinBox::valueChanged, this, edited);
            row.editor = row.spin;
            break;
        case Field::Type::Bool:
            row.check = new QCheckBox(this);
            connect(row.check, &QCheckBox::toggled, this, edited);
            row.editor = row.check;
            break;
        case Field::Type::Choice:
        case Field::Type::Folder:
            row.combo = new QComboBox(this);
            row.combo->setPlaceholderText(row.field.placeholder);
            connect(row.combo, &QComboBox::currentIndexChanged, this, edited);
            row.editor = row.combo;
            break;
        case Field::Type::Image: {
            auto *box = new QWidget(this);
            auto *layout = new QHBoxLayout(box);
            layout->setContentsMargins(0, 0, 0, 0);
            row.thumbnail = new QLabel(box);
            row.thumbnail->setObjectName(QStringLiteral("imageThumbnail"));
            row.thumbnail->setFixedSize(thumbnailSize);
            row.thumbnail->setAlignment(Qt::AlignCenter);
            row.thumbnail->setFrameShape(QFrame::StyledPanel);
            layout->addWidget(row.thumbnail);

            auto *side = new QVBoxLayout;
            row.imageName = new QLabel(tr("No image"), box);
            row.imageName->setObjectName(QStringLiteral("imageName"));
            side->addWidget(row.imageName);
            auto *paste = new QPushButton(tr("Paste"), box);
            paste->setObjectName(QStringLiteral("pasteImage"));
            auto *choose = new QPushButton(tr("Choose file…"), box);
            choose->setObjectName(QStringLiteral("chooseImageFile"));
            const bool canSave = static_cast<bool>(d->context.saveImage);
            paste->setEnabled(canSave);
            choose->setEnabled(canSave);
            side->addWidget(paste);
            side->addWidget(choose);
            side->addStretch();
            layout->addLayout(side);
            layout->addStretch();

            // Saves the image into files/ and shows it.
            const auto useImage = [this, i, edited](const QImage &image) {
                if (image.isNull() || !d->context.saveImage)
                    return;
                const QString name = d->context.saveImage(image);
                if (name.isEmpty()) {
                    QMessageBox::warning(this, tr("Image"), tr("The image could not be saved."));
                    return;
                }
                Private::Row &target = d->rows[i];
                target.image = name;
                target.imageName->setText(name);
                target.thumbnail->setPixmap(QPixmap::fromImage(
                    image.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                edited();
            };
            connect(paste, &QPushButton::clicked, this, [useImage] {
                useImage(QGuiApplication::clipboard()->image());
            });
            connect(choose, &QPushButton::clicked, this, [this, useImage] {
                QStringList patterns;
                for (const QByteArray &format : QImageReader::supportedImageFormats())
                    patterns << QStringLiteral("*.") + QString::fromLatin1(format);
                const QString path = QFileDialog::getOpenFileName(
                    this, tr("Choose image"), QString(), tr("Images (%1)").arg(patterns.join(QLatin1Char(' '))));
                if (path.isEmpty())
                    return;
                const QImage image(path);
                if (image.isNull()) {
                    QMessageBox::warning(this, tr("Image"), tr("The file is not an image that can be read."));
                    return;
                }
                useImage(image);
            });
            row.editor = box;
            break;
        }
        }
        form->addRow(row.field.label, row.editor);
    }

    setConfig({});
}

FieldForm::~FieldForm() = default;

void FieldForm::setConfig(const QJsonObject &config)
{
    d->config = config;
    d->loading = true;
    for (Private::Row &row : d->rows) {
        row.edited = false;
        d->show(row, config.value(row.field.key));
    }
    d->loading = false;
}

QJsonObject FieldForm::config() const
{
    QJsonObject config = d->config;
    for (const Private::Row &row : d->rows) {
        if (!row.edited)
            continue;
        const QJsonValue value = Private::value(row);
        if (value.isUndefined())
            config.remove(row.field.key);
        else
            config.insert(row.field.key, value);
    }
    return config;
}

QStringList FieldForm::problems() const
{
    std::vector<Field> fields;
    fields.reserve(d->rows.size());
    for (const Private::Row &row : d->rows)
        fields.push_back(row.field);
    QStringList messages;
    for (const FieldProblem &problem : validate(fields, config()))
        messages << problem.message;
    return messages;
}

QWidget *FieldForm::editorFor(const QString &key) const
{
    for (const Private::Row &row : d->rows) {
        if (row.field.key == key)
            return row.editor;
    }
    return nullptr;
}

} // namespace mirante
