#include "items/DashPanel.h"

#include "items/ItemContextImpl.h"
#include "items/ItemTypeRegistry.h"

#include <mirante/Fields.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>
#include <QJsonObject>
#include <QUrl>
#include <QUuid>

#include <memory>
#include <vector>

namespace mirante {

namespace dash {

namespace {

// The panel item type. Its view is a DashView, which only the core's
// ItemContextImpl can make.
class PanelItemType : public ItemType
{
public:
    PanelItemType()
        : m_icon(QStringLiteral(":/icons/panel.svg"))
    {
    }

    QString id() const override { return panelTypeId(); }
    QString name() const override { return QCoreApplication::translate("dash", "Built-in panel"); }
    QIcon icon() const override { return m_icon; }
    QString shortName() const override { return QCoreApplication::translate("dash", "Panel"); }
    QString description() const override
    {
        return QCoreApplication::translate(
            "dash", "A page of the app that shows the status of the task's items, or the active tasks of its group.");
    }

    std::vector<Field> fields() const override
    {
        Field panel;
        panel.key = QStringLiteral("panel");
        panel.label = QCoreApplication::translate("dash", "Panel");
        panel.type = Field::Type::Choice;
        panel.required = true;
        panel.choices = panelIds();
        panel.choiceLabels = {QCoreApplication::translate("dash", "Task summary"),
                              QCoreApplication::translate("dash", "Group overview")};
        panel.defaultValue = panel.choices.first();
        return {panel};
    }

    Rendering rendering() const override { return Rendering::Web; }

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        const QString panel = context.config().value(QStringLiteral("panel")).toString();
        if (!panelIds().contains(panel)) {
            return context.createMessageView(
                QCoreApplication::translate("dash", "Choose the panel in the item's settings."), parent);
        }
        // The core makes every placement's context, so this holds outside
        // tests that make their own.
        if (auto *core = dynamic_cast<ItemContextImpl *>(&context))
            return core->createDashPanel(panel, parent);
        return context.createMessageView(
            QCoreApplication::translate("dash", "Built-in panels are not available here."), parent);
    }

private:
    QIcon m_icon;
};

void registerDash(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QCoreApplication::translate("dash", "Built in"));
    registrar.setModuleDescription(
        QCoreApplication::translate("dash", "Panels that are part of Mirante itself, shown in the app's own pages."));
    registrar.setModuleUuid(QUuid(QStringLiteral("5d0f2b8e-3c71-4a4e-9b1f-6e2d8c0a7f43")));
    registrar.setModuleVersion(QStringLiteral("0.1.0"));
    registrar.setModuleAuthor(QStringLiteral("Rangel Reale"));
    registrar.setModuleHomepage(QUrl(QStringLiteral("https://github.com/rrgmc/mirante/tree/master/resources/panels")));
    registrar.addItemType(std::make_unique<PanelItemType>());
}

} // namespace

QString panelTypeId() { return QStringLiteral("dash.panel"); }

QStringList panelIds() { return {QStringLiteral("task-summary"), QStringLiteral("group-overview")}; }

void registerModule(ItemTypeRegistry &registry) { registry.registerBuiltInModule(QStringLiteral("dash"), &registerDash); }

} // namespace dash

} // namespace mirante
