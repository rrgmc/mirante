#include "ui/GroupDialog.h"

#include "core/GroupColours.h"
#include "items/PaintedIconEngine.h"

#include <QApplication>
#include <QButtonGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace mirante {

namespace {

// A colour swatch: a filled circle, with a ring in the text colour around it
// while chosen.
class SwatchIconEngine : public PaintedIconEngine
{
public:
    explicit SwatchIconEngine(const QColor &colour) : m_colour(colour) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode, QIcon::State state) override
    {
        const qreal side = qMin(rect.width(), rect.height());
        const QRectF outer(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0,
                           side, side);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (state == QIcon::On) {
            painter->setPen(QPen(QApplication::palette().color(QPalette::WindowText), 1.5));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(outer.adjusted(1, 1, -1, -1));
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_colour);
        const qreal inset = side * 0.22;
        painter->drawEllipse(outer.adjusted(inset, inset, -inset, -inset));
        painter->restore();
    }

    QIconEngine *clone() const override { return new SwatchIconEngine(m_colour); }

private:
    QColor m_colour;
};

} // namespace

struct GroupDialog::Private
{
    GroupDialog *q = nullptr;
    Group group;

    QLineEdit *name = nullptr;
    QComboBox *profile = nullptr;
    QLabel *message = nullptr;             // why OK was refused

    // The colour. chosenColour is empty until one is chosen in the dialog.
    QString chosenColour;
    QButtonGroup *swatches = nullptr;      // the palette, then the custom swatch
    QToolButton *customSwatch = nullptr;   // a colour outside the palette; hidden without one
    QToolButton *otherColour = nullptr;    // "Other…", opens QColorDialog

    QString shownColour() const
    {
        return chosenColour.isEmpty() ? groupcolours::of(group) : chosenColour;
    }

    QToolButton *swatch(const QString &colour, const QString &objectName)
    {
        auto *button = new QToolButton(q);
        button->setObjectName(objectName);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIconSize(QSize(20, 20));
        button->setIcon(QIcon(new SwatchIconEngine(QColor(colour))));
        button->setToolTip(colour);
        button->setProperty("colour", colour);
        return button;
    }

    // Checks the swatch of the shown colour. A colour outside the palette
    // gets the custom swatch.
    void showColour()
    {
        const QString colour = shownColour();
        const bool custom = !groupcolours::palette().contains(colour);
        if (custom) {
            customSwatch->setIcon(QIcon(new SwatchIconEngine(QColor(colour))));
            customSwatch->setToolTip(colour);
            customSwatch->setProperty("colour", colour);
        }
        customSwatch->setVisible(custom);
        for (QAbstractButton *button : swatches->buttons()) {
            if (button->property("colour").toString() == colour && (button == customSwatch) == custom)
                button->setChecked(true);
        }
    }

    void chooseColour(const QString &colour)
    {
        if (!groupcolours::isValid(colour))
            return;
        chosenColour = colour.toLower();
        showColour();
    }
};

GroupDialog::GroupDialog(const Group &group, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->group = group;
    setWindowTitle(group.name.isEmpty() ? tr("New group") : tr("Edit group"));
    resize(520, 300);

    d->name = new QLineEdit(group.name, this);
    d->name->setObjectName(QStringLiteral("name"));

    // The colour: the palette, a swatch for a colour outside it, and "Other…".
    auto *colourRow = new QHBoxLayout;
    colourRow->setSpacing(2);
    d->swatches = new QButtonGroup(this);
    d->swatches->setExclusive(true);
    for (int i = 0; i < groupcolours::palette().size(); ++i) {
        QToolButton *button = d->swatch(groupcolours::palette().at(i), QStringLiteral("colourSwatch%1").arg(i));
        d->swatches->addButton(button);
        colourRow->addWidget(button);
    }
    d->customSwatch = d->swatch(groupcolours::palette().front(), QStringLiteral("customColour"));
    d->swatches->addButton(d->customSwatch);
    colourRow->addWidget(d->customSwatch);
    d->otherColour = new QToolButton(this);
    d->otherColour->setObjectName(QStringLiteral("otherColour"));
    d->otherColour->setText(tr("Other…"));
    d->otherColour->setAutoRaise(true);
    colourRow->addWidget(d->otherColour);
    colourRow->addStretch(1);
    connect(d->swatches, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton *button) { d->chooseColour(button->property("colour").toString()); });
    connect(d->otherColour, &QToolButton::clicked, this, [this] {
        const QColor colour = QColorDialog::getColor(QColor(d->shownColour()), this, tr("Group colour"));
        if (colour.isValid())
            d->chooseColour(colour.name());
    });
    d->showColour();
    auto *colourHelp = new QLabel(tr("The title bars and cell headers of the group's tasks and items, and its "
                                     "projects in the sidebar, are tinted with this colour."),
                                  this);
    colourHelp->setWordWrap(true);

    // The profile.
    d->profile = new QComboBox(this);
    d->profile->setObjectName(QStringLiteral("profile"));
    auto *profileHelp = new QLabel(tr("Items of this group use this profile for their logins and module "
                                      "settings, unless an item chooses its own. Manage profiles with "
                                      "Profiles… in the File menu."),
                                   this);
    profileHelp->setWordWrap(true);

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *form = new QFormLayout;
    form->addRow(tr("Name:"), d->name);
    form->addRow(tr("Colour:"), colourRow);
    form->addRow(QString(), colourHelp);
    form->addRow(tr("Profile:"), d->profile);
    form->addRow(QString(), profileHelp);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &GroupDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch(1);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    // Until setProfiles() is called, only Default is offered.
    setProfiles({});
    d->name->setFocus();
}

GroupDialog::~GroupDialog() = default;

Group GroupDialog::value() const
{
    Group group = d->group;
    group.name = d->name->text().trimmed();

    // Default is stored as no profile.
    const QString profile = d->profile->currentData().toString();
    group.profile = profile == kDefaultProfile ? QString() : profile;

    if (!d->chosenColour.isEmpty())
        group.colour = d->chosenColour;
    return group;
}

QString GroupDialog::colour() const { return d->shownColour(); }
void GroupDialog::setColour(const QString &colour) { d->chooseColour(colour); }

void GroupDialog::setName(const QString &name) { d->name->setText(name); }

void GroupDialog::setProfiles(const std::vector<Profile> &profiles)
{
    d->profile->clear();
    bool hasDefault = false;
    for (const Profile &profile : profiles) {
        d->profile->addItem(profile.name, profile.id);
        hasDefault = hasDefault || profile.id == kDefaultProfile;
    }
    if (!hasDefault)
        d->profile->insertItem(0, tr("Default"), kDefaultProfile);
    // A profile that is not listed, because it was deleted or has not arrived
    // yet, is kept until another is chosen.
    const QString current = d->group.profile.isEmpty() ? kDefaultProfile : d->group.profile;
    if (d->profile->findData(current) < 0)
        d->profile->addItem(tr("Unknown profile, uses Default"), current);
    d->profile->setCurrentIndex(d->profile->findData(current));
}

void GroupDialog::setProfile(const QString &id)
{
    const int index = d->profile->findData(id);
    if (index >= 0)
        d->profile->setCurrentIndex(index);
}

void GroupDialog::accept()
{
    if (d->name->text().trimmed().isEmpty()) {
        d->message->setText(tr("Enter a name."));
        d->message->show();
        d->name->setFocus();
        return;
    }
    QDialog::accept();
}

} // namespace mirante
