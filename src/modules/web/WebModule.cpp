// The web module: any web page as an item. See README.md.
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>

#include <memory>

namespace mirante::modules::web {

namespace {

// Placeholder type for M2: only an id, a name and an icon. Views arrive in M4.
class StubItemType : public ItemType
{
public:
    StubItemType(const QString &id, const QString &name)
        : m_id(id)
        , m_name(name)
        , m_icon(QStringLiteral(":/modules/web/icons/web.svg"))
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    QIcon icon() const override { return m_icon; }

private:
    QString m_id;
    QString m_name;
    QIcon m_icon;
};

} // namespace

void registerModule(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<StubItemType>(
        QStringLiteral("web.page"), QCoreApplication::translate("web", "Web page")));
}

} // namespace mirante::modules::web
