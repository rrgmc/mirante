# Translations

Mirante speaks English, the language of the source, and Brazilian Portuguese. See [decision 0086](../docs/decisions/0086-english-and-brazilian-portuguese.md).

| File | Contents |
|---|---|
| `mirante_pt_BR.ts` | Every text of the app and of all modules, in Brazilian Portuguese |
| `mirante_en.ts` | The English plural forms of the texts that use `%n`, made by `lupdate` |

The build compiles both into the app's resources, under `:/i18n`. Qt's own texts, such as the buttons of standard dialogs, come from Qt's `qtbase` translations.

## Adding or changing a text

1. Write the English text in the source with `tr()` in a class with `Q_OBJECT`, or with `QCoreApplication::translate()`. Module code always uses `QCoreApplication::translate("<module id>", …)`; see [modules](../docs/modules.md#texts).
2. Build a text from parts with `arg()`, such as `tr("%1 was not found.").arg(name)`, never by adding strings, so a translation can put the parts in another order.
3. Run `task translations`. It runs `lupdate`, which adds the new texts to the files, marked unfinished, and removes the texts no longer in the sources.
4. Translate each unfinished text in `mirante_pt_BR.ts`, with Qt Linguist or in a text editor: write the translation and remove `type="unfinished"`. A text with `%n` needs its plural forms in both files.
5. Run `task check`. It fails while a text has no translation, or a translation lost a placeholder, an `&` accelerator or a trailing `…`.

The files keep no source locations, so a change of code does not change them. Ids, stored data, file names and log messages stay in English.

## Terms

The [glossary](../docs/README.md#glossary) terms always take the same Portuguese word. The same list is at the top of `mirante_pt_BR.ts`.

| English | Português |
|---|---|
| Group | Grupo |
| Project | Projeto |
| Folder | Pasta |
| Main folder | Pasta principal |
| Task | Tarefa |
| Item | Item (plural itens) |
| Item type | Tipo de item |
| Module | Módulo |
| Settings screen | Tela de configurações |
| Placement | Posição |
| Dashboard | Painel |
| Task dashboard | Painel da tarefa |
| Root dashboard | Painel raiz |
| Layout tree | Árvore de layout |
| Layout | Layout |
| Split | Divisão |
| Tabs, tab | Abas, aba |
| Cell | Célula |
| Preview slot, preview | Prévia |
| Focus slot, Focus view, Focus | Foco |
| Priority: Urgent, High, Normal, Low | Prioridade: Urgente, Alta, Normal, Baixa |
| Task state: active, done, archived | Ativa, concluída, arquivada |
| Attention | Atenção |
| Server | Servidor |
| Token | Token |
| Profile, default profile | Perfil, perfil padrão |
| Status | Status |
| Status cache | Cache de status |
| Data folder | Pasta de dados |
| Web data folder | Pasta de dados da web |
| Workspace | Espaço de trabalho |
| Sample data | Dados de exemplo |
| Sidebar | Barra lateral |
| Settings | Configurações |
| Device settings | Configurações do dispositivo |
| Trash | Lixeira |
| Lock, unlock (a dashboard) | Travar, destravar |
| Detach, detached window | Destacar, janela destacada |
| Tray | Bandeja |
| Global hotkey | Atalho global |
| Quick add | Adição rápida |
| Quick switcher | Troca rápida |
| Log | Log |
| Keychain | Chaveiro |
| Web login | Login na web |
| Conflict copy | Cópia em conflito |
| Paste, crop | Colar, recortar |
| Merge request, issue, pipeline, job, sprint | Merge request, issue, pipeline, job, sprint (kept in English, as GitLab and Jira do) |
| Label (GitLab) | Etiqueta |
| Assignee, reporter | Responsável, relator |
| Comment | Comentário |

Names stay as they are: Mirante, GitLab, Jira, Claude, Git, Markdown, Qt.
