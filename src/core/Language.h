#pragma once

#include <QLocale>
#include <QString>
#include <QStringList>

namespace mirante {

// The language the app speaks, from the device setting "language". See
// docs/decisions/0086-english-and-brazilian-portuguese.md. Implemented in
// Language.cpp.
namespace language {

// "en" or "pt_BR". "system" takes "pt_BR" when the first of the system's user
// interface languages, as QLocale::uiLanguages() gives them, is any
// Portuguese, and "en" otherwise. Any other setting counts as "system".
QString choose(const QString &setting, const QStringList &systemLanguages);

// The locale that formats dates and numbers for reading in a chosen language.
QLocale locale(const QString &chosen);

} // namespace language

} // namespace mirante
