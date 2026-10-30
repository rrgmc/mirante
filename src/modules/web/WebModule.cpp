// The web module: any web page as an item. See README.md.
#include "WebUrls.h"

#include <mirante/Fields.h>
#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>

#include <memory>
#include <vector>

namespace mirante::modules::web {

namespace {

// The web page type: a web view of its address, or a message when the address
// is missing or is not an http or https address.
class PageItemType : public ItemType
{
public:
    PageItemType(const QString &id, const QString &name)
        : m_id(id)
        , m_name(name)
        , m_icon(QStringLiteral(":/modules/web/icons/web.svg"))
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    QIcon icon() const override { return m_icon; }
    QString shortName() const override { return QCoreApplication::translate("web", "Web"); }
    QString description() const override
    {
        return QCoreApplication::translate("web", "The web page at an address, signed in with the item's profile.");
    }

    std::vector<Field> fields() const override
    {
        Field url;
        url.key = QStringLiteral("url");
        url.label = QCoreApplication::translate("web", "Address");
        url.type = Field::Type::Url;
        url.required = true;
        url.placeholder = QStringLiteral("https://");
        return {url};
    }

    Rendering rendering() const override { return Rendering::Web; }

    std::optional<UrlMatch> fromUrl(const QUrl &url, const QString &profileId) const override
    {
        Q_UNUSED(profileId)
        return matchUrl(url);
    }

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        const QUrl url = pageUrl(context.config());
        if (url.isEmpty()) {
            return context.createMessageView(
                QCoreApplication::translate("web", "Set the address in the item's settings."), parent);
        }
        return context.createWebView(url, parent);
    }

private:
    QString m_id;
    QString m_name;
    QIcon m_icon;
};

} // namespace

void registerModule(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Web"));
    registrar.setModuleDescription(QCoreApplication::translate(
        "web", "Any web page as an item."));
    registrar.addItemType(std::make_unique<PageItemType>(
        QStringLiteral("web.page"), QCoreApplication::translate("web", "Web page")));
}

} // namespace mirante::modules::web
