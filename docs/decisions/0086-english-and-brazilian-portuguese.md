# 0086. The app speaks English and Brazilian Portuguese

- Status: Accepted
- Date: 2026-10-30

## Context

Every text the user sees is in English. The user wants the app in Brazilian Portuguese too, chosen from the system's language, with a way to choose another.

## Decision

- **Languages:** English, the language of the source, and Brazilian Portuguese (`pt_BR`).
- **Choosing:** a device setting, **Language**, in the [settings window](../ui.md#settings), with "System", the default, "English" and "Português (Brasil)". "System" takes Brazilian Portuguese when the system's first user interface language is any Portuguese, and English otherwise. A change applies at the next start, and the settings window says so.
- **Mechanism:** Qt's own. The source keeps English text in `tr()` and `QCoreApplication::translate()`, as now. One translation file, `translations/mirante_pt_BR.ts`, covers the core and every module, and is compiled into the app as a resource with `qt_add_translations()`. Qt's own texts, such as the buttons of standard dialogs, come from Qt's `qtbase` translations, deployed with the app.
- **Translators' terms:** the [glossary](../README.md) terms get one Portuguese word each, listed at the top of the translation file, so the app uses them consistently.
- **What stays in English:** the documents, the log, decision records, ids and stored data. Dates and numbers follow the chosen language's locale where the app formats them for reading.
- **New text** must be translated in the same change. `task translations` runs `lupdate` to bring the file up to date, and `task check` runs it too and fails when a text has no Portuguese translation.

## Alternatives

- **Following the system only.** Simple, but the user asked to choose.
- **Changing the language without a restart.** Every widget would have to rebuild its texts on `LanguageChange`. The app has many dialogs and views made once; a restart is rare and costs little.
- **One translation file per module.** It fits third-party modules later, but now one file is simpler to keep complete. Splitting it is a small change when modules move out.

## Consequences

- The Qt download gains `qttools`, for `lupdate` and `lrelease`, and `qttranslations`.
- Every change that adds a text also adds its translation.
- Tests keep running in English: the test runner sets the language to English.
