#include "core/Language.h"

namespace mirante::language {

namespace {

const QString kEnglish = QStringLiteral("en");
const QString kPortuguese = QStringLiteral("pt_BR");

// "pt", "pt-BR", "pt_PT" and the like, but not a language such as "ptx".
bool isPortuguese(const QString &name)
{
    return name.startsWith(QLatin1String("pt"), Qt::CaseInsensitive)
        && (name.size() == 2 || name.at(2) == QLatin1Char('-') || name.at(2) == QLatin1Char('_'));
}

} // namespace

QString choose(const QString &setting, const QStringList &systemLanguages)
{
    if (setting == kEnglish || setting == kPortuguese)
        return setting;
    return !systemLanguages.isEmpty() && isPortuguese(systemLanguages.first()) ? kPortuguese : kEnglish;
}

QLocale locale(const QString &chosen)
{
    if (chosen == kPortuguese)
        return QLocale(QLocale::Portuguese, QLocale::Brazil);
    return QLocale(QLocale::English, QLocale::UnitedStates);
}

} // namespace mirante::language
