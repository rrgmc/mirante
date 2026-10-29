// The image module: an image with an optional link and memo. See README.md.
#include <mirante/Fields.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>

#include <memory>
#include <vector>

namespace mirante::modules::image {

namespace {

// A type with an id, a name, an icon and its configuration fields. The view arrives later.
class StubItemType : public ItemType
{
public:
    StubItemType(const QString &id, const QString &name)
        : m_id(id)
        , m_name(name)
        , m_icon(QStringLiteral(":/modules/image/icons/image.svg"))
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    QIcon icon() const override { return m_icon; }
    QString shortName() const override { return QCoreApplication::translate("image", "Image"); }

    std::vector<Field> fields() const override
    {
        Field file;
        file.key = QStringLiteral("file");
        file.label = QCoreApplication::translate("image", "Image");
        file.type = Field::Type::Image;
        file.required = true;

        Field link;
        link.key = QStringLiteral("link");
        link.label = QCoreApplication::translate("image", "Link");
        link.type = Field::Type::Url;
        link.placeholder = QStringLiteral("https://");

        Field memo;
        memo.key = QStringLiteral("memo");
        memo.label = QCoreApplication::translate("image", "Memo");
        memo.type = Field::Type::MultiLineText;
        return {file, link, memo};
    }

    // A pasted image fills the file field. See docs/decisions/0021-image-items-as-files.md.
    QString pastedImageField() const override { return QStringLiteral("file"); }

private:
    QString m_id;
    QString m_name;
    QIcon m_icon;
};

} // namespace

void registerModule(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<StubItemType>(
        QStringLiteral("image"), QCoreApplication::translate("image", "Image")));
}

} // namespace mirante::modules::image
