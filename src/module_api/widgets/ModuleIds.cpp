#include <mirante/ModuleIds.h>

namespace mirante {

bool isValidModuleId(const QString &moduleId)
{
    if (moduleId.isEmpty())
        return false;
    for (const QChar c : moduleId) {
        const bool lower = c >= QLatin1Char('a') && c <= QLatin1Char('z');
        const bool digit = c >= QLatin1Char('0') && c <= QLatin1Char('9');
        if (!lower && !digit)
            return false;
    }
    return true;
}

bool idBelongsToModule(const QString &id, const QString &moduleId)
{
    if (moduleId.isEmpty())
        return false;
    if (id == moduleId)
        return true;
    if (!id.startsWith(moduleId + QLatin1Char('.')))
        return false;
    const QString rest = id.mid(moduleId.size() + 1);
    if (rest.isEmpty() || rest.startsWith(QLatin1Char('.')) || rest.endsWith(QLatin1Char('.'))
        || rest.contains(QLatin1String("..")))
        return false;
    return true;
}

bool projectSettingTakes(Field::Type type)
{
    return type != Field::Type::Secret && type != Field::Type::Folder && type != Field::Type::Image;
}

bool deviceSettingTakes(Field::Type type)
{
    return type != Field::Type::Secret && type != Field::Type::Folder && type != Field::Type::Image;
}

} // namespace mirante
