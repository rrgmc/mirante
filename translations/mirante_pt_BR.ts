<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<!-- Terms: each glossary term of docs/README.md always takes the same Portuguese
     word. The list comes from translations/README.md; edit it there.
     Group: Grupo
     Project: Projeto
     Folder: Pasta
     Main folder: Pasta principal
     Task: Tarefa
     Item: Item (plural itens)
     Item type: Tipo de item
     Module: Módulo
     Settings screen: Tela de configurações
     Placement: Posição
     Dashboard: Painel
     Task dashboard: Painel da tarefa
     Root dashboard: Painel raiz
     Built-in panel: Quadro interno (never "painel", which is a dashboard)
     Layout tree: Árvore de layout
     Layout: Layout
     Split: Divisão
     Tabs, tab: Abas, aba
     Cell: Célula
     Preview slot, preview: Prévia
     Focus slot, Focus view, Focus: Foco
     Priority: Urgent, High, Normal, Low: Prioridade: Urgente, Alta, Normal, Baixa
     Task state: active, done, archived: Ativa, concluída, arquivada
     Attention: Atenção
     Watch, watched item: Acompanhar, item acompanhado
     Server: Servidor
     Token: Token
     Profile, default profile: Perfil, perfil padrão
     Status: Status
     Status cache: Cache de status
     Data folder: Pasta de dados
     Web data folder: Pasta de dados da web
     Workspace: Espaço de trabalho
     Sample data: Dados de exemplo
     Sidebar: Barra lateral
     Settings: Configurações
     Device settings: Configurações do dispositivo
     Trash: Lixeira
     Lock, unlock (a dashboard): Travar, destravar
     Detach, detached window: Destacar, janela destacada
     Tray: Bandeja
     Global hotkey: Atalho global
     Quick add: Adição rápida
     Quick switcher: Troca rápida
     Log: Log
     Keychain: Chaveiro
     Web login: Login na web
     Conflict copy: Cópia em conflito
     Paste, crop: Colar, recortar
     Merge request, issue, pipeline, job, sprint: Merge request, issue, pipeline, job, sprint (kept in English, as GitLab and Jira do)
     Label (GitLab): Etiqueta
     Assignee, reporter: Responsável, relator
     Comment: Comentário
-->
<TS version="2.1" language="pt_BR" sourcelanguage="en">
<context>
    <name>DashboardCellFactory</name>
    <message>
        <source>%1 › %2</source>
        <translation>%1 › %2</translation>
    </message>
    <message>
        <source>In trash</source>
        <translation>Na lixeira</translation>
    </message>
</context>
<context>
    <name>Edits</name>
    <message>
        <source>Main</source>
        <translation>Principal</translation>
    </message>
</context>
<context>
    <name>FieldValidation</name>
    <message>
        <source>%1 must be text</source>
        <translation>%1 deve ser um texto</translation>
    </message>
    <message>
        <source>%1 must be an address starting with http:// or https://</source>
        <translation>%1 deve ser um endereço que comece com http:// ou https://</translation>
    </message>
    <message>
        <source>%1 must be a whole number</source>
        <translation>%1 deve ser um número inteiro</translation>
    </message>
    <message>
        <source>%1 must be on or off</source>
        <translation>%1 deve ser ligado ou desligado</translation>
    </message>
    <message>
        <source>%1 must be one of the choices offered</source>
        <translation>%1 deve ser uma das opções oferecidas</translation>
    </message>
    <message>
        <source>%1 must be a list of texts</source>
        <translation>%1 deve ser uma lista de textos</translation>
    </message>
    <message>
        <source>%1 is required</source>
        <translation>%1 é obrigatório</translation>
    </message>
</context>
<context>
    <name>FileWriter</name>
    <message>
        <source>Could not create the folder %1</source>
        <translation>Não foi possível criar a pasta %1</translation>
    </message>
</context>
<context>
    <name>GitStatusReader</name>
    <message>
        <source>Git was not found.</source>
        <translation>O Git não foi encontrado.</translation>
    </message>
    <message>
        <source>Git took longer than %1 seconds.</source>
        <translation>O Git demorou mais de %1 segundos.</translation>
    </message>
    <message>
        <source>Git stopped unexpectedly.</source>
        <translation>O Git parou inesperadamente.</translation>
    </message>
    <message>
        <source>Git failed with exit code %1.</source>
        <translation>O Git falhou com o código de saída %1.</translation>
    </message>
</context>
<context>
    <name>ImagePaste</name>
    <message>
        <source>Image</source>
        <translation>Imagem</translation>
    </message>
    <message>
        <source>To add an image, use Paste Image… in a task&apos;s Items menu.</source>
        <translation>Para adicionar uma imagem, use Colar imagem… no menu Itens de uma tarefa.</translation>
    </message>
    <message>
        <source>This build cannot show images.</source>
        <translation>Esta compilação não consegue mostrar imagens.</translation>
    </message>
    <message>
        <source>The image could not be saved.</source>
        <translation>Não foi possível salvar a imagem.</translation>
    </message>
</context>
<context>
    <name>ItemContext</name>
    <message>
        <source>This item type is no longer available: %1.</source>
        <translation>Este tipo de item não está mais disponível: %1.</translation>
    </message>
    <message>
        <source>This item needs the %1 module, which this build does not include.</source>
        <translation>Este item precisa do módulo %1, que esta compilação não inclui.</translation>
    </message>
    <message>
        <source>Type: %1</source>
        <translation>Tipo: %1</translation>
    </message>
    <message>
        <source>unknown</source>
        <translation>desconhecido</translation>
    </message>
    <message>
        <source>Name: %1</source>
        <translation>Nome: %1</translation>
    </message>
    <message>
        <source>Id: %1</source>
        <translation>Id: %1</translation>
    </message>
    <message>
        <source>Web pages are not available here.</source>
        <translation>Páginas web não estão disponíveis aqui.</translation>
    </message>
    <message>
        <source>Choose the folder &quot;%1&quot;</source>
        <translation>Escolha a pasta &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Programs cannot be started here.</source>
        <translation>Não é possível iniciar programas aqui.</translation>
    </message>
</context>
<context>
    <name>Launcher</name>
    <message>
        <source>The terminal %1 could not be started.</source>
        <translation>Não foi possível iniciar o terminal %1.</translation>
    </message>
    <message>
        <source>The terminal %1 was not found.</source>
        <translation>O terminal %1 não foi encontrado.</translation>
    </message>
    <message>
        <source>No terminal was found.</source>
        <translation>Nenhum terminal foi encontrado.</translation>
    </message>
    <message>
        <source>%1 was not found.</source>
        <translation>%1 não foi encontrado.</translation>
    </message>
    <message>
        <source>%1 could not be started.</source>
        <translation>Não foi possível iniciar %1.</translation>
    </message>
</context>
<context>
    <name>Migrations</name>
    <message>
        <source>missing schemaVersion</source>
        <translation>schemaVersion ausente</translation>
    </message>
    <message>
        <source>invalid schemaVersion</source>
        <translation>schemaVersion inválido</translation>
    </message>
    <message>
        <source>written by a newer version of Mirante</source>
        <translation>gravado por uma versão mais nova do Mirante</translation>
    </message>
</context>
<context>
    <name>Presets</name>
    <message>
        <source>Full</source>
        <translation>Inteiro</translation>
    </message>
    <message>
        <source>Two side by side</source>
        <translation>Dois lado a lado</translation>
    </message>
    <message>
        <source>Two stacked</source>
        <translation>Dois empilhados</translation>
    </message>
    <message>
        <source>Grid of four</source>
        <translation>Grade de quatro</translation>
    </message>
    <message>
        <source>One large on top, two below</source>
        <translation>Um grande em cima, dois embaixo</translation>
    </message>
    <message>
        <source>One large on the left, two stacked on the right</source>
        <translation>Um grande à esquerda, dois empilhados à direita</translation>
    </message>
    <message>
        <source>Tabs</source>
        <translation>Abas</translation>
    </message>
</context>
<context>
    <name>Profiles</name>
    <message>
        <source>Default</source>
        <translation>Padrão</translation>
    </message>
    <message>
        <source>%1 (default)</source>
        <translation>%1 (padrão)</translation>
    </message>
</context>
<context>
    <name>Store</name>
    <message>
        <source>not valid JSON: %1</source>
        <translation>não é um JSON válido: %1</translation>
    </message>
    <message>
        <source>not a JSON object</source>
        <translation>não é um objeto JSON</translation>
    </message>
    <message>
        <source>cannot be read: %1</source>
        <translation>não pode ser lido: %1</translation>
    </message>
    <message>
        <source>the folder does not exist</source>
        <translation>a pasta não existe</translation>
    </message>
    <message>
        <source>cannot make %1</source>
        <translation>não foi possível criar %1</translation>
    </message>
    <message>
        <source>cannot copy %1</source>
        <translation>não foi possível copiar %1</translation>
    </message>
    <message>
        <source>the folder is not a workspace</source>
        <translation>a pasta não é um espaço de trabalho</translation>
    </message>
    <message>
        <source>the new folder is the workspace&apos;s folder or inside it</source>
        <translation>a nova pasta é a pasta do espaço de trabalho ou está dentro dela</translation>
    </message>
    <message>
        <source>a file has that name</source>
        <translation>já existe um arquivo com esse nome</translation>
    </message>
    <message>
        <source>the folder is not empty</source>
        <translation>a pasta não está vazia</translation>
    </message>
    <message>
        <source>cannot make the folder that holds it</source>
        <translation>não foi possível criar a pasta que o contém</translation>
    </message>
    <message>
        <source>cannot make the folder</source>
        <translation>não foi possível criar a pasta</translation>
    </message>
    <message>
        <source>%1; the workspace is now in %2</source>
        <translation>%1; o espaço de trabalho agora está em %2</translation>
    </message>
    <message>
        <source>was removed, so this folder is no longer a workspace</source>
        <translation>foi removido, então esta pasta não é mais um espaço de trabalho</translation>
    </message>
    <message>
        <source>could not be saved: %1</source>
        <translation>não pôde ser salvo: %1</translation>
    </message>
    <message>
        <source>the workspace is read-only</source>
        <translation>o espaço de trabalho é somente leitura</translation>
    </message>
    <message>
        <source>could not create the folder %1</source>
        <translation>não foi possível criar a pasta %1</translation>
    </message>
</context>
<context>
    <name>UnknownItemType</name>
    <message>
        <source>Unknown type %1</source>
        <translation>Tipo desconhecido %1</translation>
    </message>
</context>
<context>
    <name>claude</name>
    <message>
        <source>Set the address in the item&apos;s settings.</source>
        <translation>Defina o endereço nas configurações do item.</translation>
    </message>
    <message>
        <source>Address</source>
        <translation>Endereço</translation>
    </message>
    <message>
        <source>Claude sessions on claude.ai.</source>
        <translation>Sessões do Claude no claude.ai.</translation>
    </message>
    <message>
        <source>Claude session (web)</source>
        <translation>Sessão do Claude (web)</translation>
    </message>
    <message>
        <source>Claude</source>
        <translation>Claude</translation>
    </message>
    <message>
        <source>A Claude session on claude.ai, such as a Remote Control or cloud session, in a web view.</source>
        <translation>Uma sessão do Claude no claude.ai, como uma sessão de Remote Control ou na nuvem, em uma visualização web.</translation>
    </message>
    <message>
        <source>Claude session</source>
        <translation>Sessão do Claude</translation>
    </message>
</context>
<context>
    <name>dash</name>
    <message>
        <source>Built-in panel</source>
        <translation>Quadro interno</translation>
    </message>
    <message>
        <source>Panel</source>
        <translation>Quadro</translation>
    </message>
    <message>
        <source>A page of the app that shows the status of the task&apos;s items, or the active tasks of its group.</source>
        <translation>Uma página do app que mostra o status dos itens da tarefa, ou as tarefas ativas do seu grupo.</translation>
    </message>
    <message>
        <source>Task summary</source>
        <translation>Resumo da tarefa</translation>
    </message>
    <message>
        <source>Group overview</source>
        <translation>Visão geral do grupo</translation>
    </message>
    <message>
        <source>Choose the panel in the item&apos;s settings.</source>
        <translation>Escolha o quadro nas configurações do item.</translation>
    </message>
    <message>
        <source>Built-in panels are not available here.</source>
        <translation>Quadros internos não estão disponíveis aqui.</translation>
    </message>
    <message>
        <source>Built in</source>
        <translation>Interno</translation>
    </message>
    <message>
        <source>Panels that are part of Mirante itself, shown in the app&apos;s own pages.</source>
        <translation>Quadros que fazem parte do próprio Mirante, mostrados em páginas do app.</translation>
    </message>
</context>
<context>
    <name>folder</name>
    <message>
        <source>Folder</source>
        <translation>Pasta</translation>
    </message>
    <message>
        <source>One of the project&apos;s folders: its local path, Git branch and status, with shortcuts to a terminal, an editor and the file manager.</source>
        <translation>Uma das pastas do projeto: seu caminho local, branch e status do Git, com atalhos para um terminal, um editor e o gerenciador de arquivos.</translation>
    </message>
    <message>
        <source>A project&apos;s folder as an item, with its Git state and shortcuts to a terminal and an editor.</source>
        <translation>Uma pasta de projeto como item, com seu estado do Git e atalhos para um terminal e um editor.</translation>
    </message>
    <message>
        <source>Folder / terminal</source>
        <translation>Pasta / terminal</translation>
    </message>
    <message>
        <source>Editor command</source>
        <translation>Comando do editor</translation>
    </message>
    <message>
        <source>%1 ahead</source>
        <translation>%1 à frente</translation>
    </message>
    <message>
        <source>%1 behind</source>
        <translation>%1 atrás</translation>
    </message>
    <message>
        <source>%1 staged</source>
        <translation>%1 preparados</translation>
    </message>
    <message>
        <source>%1 changed</source>
        <translation>%1 alterados</translation>
    </message>
    <message>
        <source>%1 untracked</source>
        <translation>%1 não rastreados</translation>
    </message>
    <message>
        <source>%1 conflict</source>
        <translation>%1 conflito</translation>
    </message>
    <message>
        <source>%1 conflicts</source>
        <translation>%1 conflitos</translation>
    </message>
    <message>
        <source>Clean</source>
        <translation>Limpo</translation>
    </message>
    <message>
        <source>Terminal</source>
        <translation>Terminal</translation>
    </message>
    <message>
        <source>Editor</source>
        <translation>Editor</translation>
    </message>
    <message>
        <source>File manager</source>
        <translation>Gerenciador de arquivos</translation>
    </message>
    <message>
        <source>Files</source>
        <translation>Arquivos</translation>
    </message>
    <message>
        <source>Choose folder…</source>
        <translation>Escolher pasta…</translation>
    </message>
    <message>
        <source>Folder…</source>
        <translation>Pasta…</translation>
    </message>
    <message>
        <source>Folder not set on this device</source>
        <translation>Pasta não definida neste dispositivo</translation>
    </message>
    <message>
        <source>Detached at %1</source>
        <translation>Desanexado em %1</translation>
    </message>
    <message>
        <source>Detached</source>
        <translation>Desanexado</translation>
    </message>
    <message>
        <source>Not a Git repository</source>
        <translation>Não é um repositório Git</translation>
    </message>
    <message>
        <source>Git was not found</source>
        <translation>O Git não foi encontrado</translation>
    </message>
    <message>
        <source>Git failed.</source>
        <translation>O Git falhou.</translation>
    </message>
</context>
<context>
    <name>github</name>
    <message>
        <source>This view is not ready yet.</source>
        <translation>Esta visualização ainda não está pronta.</translation>
    </message>
    <message>
        <source>Set the repository in the item&apos;s settings.</source>
        <translation>Defina o repositório nas configurações do item.</translation>
    </message>
    <message>
        <source>Set the owner in the item&apos;s settings.</source>
        <translation>Defina o dono nas configurações do item.</translation>
    </message>
    <message>
        <source>Set the scope in the item&apos;s settings.</source>
        <translation>Defina o escopo nas configurações do item.</translation>
    </message>
    <message numerus="yes">
        <source>%n+ pull request(s)</source>
        <translation>
            <numerusform>%n+ pull request</numerusform>
            <numerusform>%n+ pull requests</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n pull request(s)</source>
        <translation>
            <numerusform>%n pull request</numerusform>
            <numerusform>%n pull requests</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n+ issue(s)</source>
        <translation>
            <numerusform>%n+ issue</numerusform>
            <numerusform>%n+ issues</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n issue(s)</source>
        <translation>
            <numerusform>%n issue</numerusform>
            <numerusform>%n issues</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n+ run(s)</source>
        <translation>
            <numerusform>%n+ execução</numerusform>
            <numerusform>%n+ execuções</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n run(s)</source>
        <translation>
            <numerusform>%n execução</numerusform>
            <numerusform>%n execuções</numerusform>
        </translation>
    </message>
    <message>
        <source>All open</source>
        <translation>Todas abertas</translation>
    </message>
    <message>
        <source>Review requested from me</source>
        <translation>Revisão solicitada a mim</translation>
    </message>
    <message>
        <source>Created by me</source>
        <translation>Criadas por mim</translation>
    </message>
    <message>
        <source>Assigned to me</source>
        <translation>Atribuídas a mim</translation>
    </message>
    <message>
        <source>owner %1</source>
        <translation>dono %1</translation>
    </message>
    <message>
        <source>all repositories</source>
        <translation>todos os repositórios</translation>
    </message>
    <message>
        <source>Started by me</source>
        <translation>Iniciadas por mim</translation>
    </message>
    <message>
        <source>Of the default branch</source>
        <translation>Do branch padrão</translation>
    </message>
    <message>
        <source>Of branch %1</source>
        <translation>Do branch %1</translation>
    </message>
    <message>
        <source>Of pull request #%1</source>
        <translation>Do pull request #%1</translation>
    </message>
    <message>
        <source>All runs</source>
        <translation>Todas as execuções</translation>
    </message>
    <message>
        <source>The repository&apos;s default branch could not be read.</source>
        <translation>Não foi possível ler o branch padrão do repositório.</translation>
    </message>
    <message>
        <source>Set the pull request number in the item&apos;s settings.</source>
        <translation>Defina o número do pull request nas configurações do item.</translation>
    </message>
    <message>
        <source>Set the source in the item&apos;s settings.</source>
        <translation>Defina a origem nas configurações do item.</translation>
    </message>
    <message>
        <source>Set the GitHub base URL in this profile&apos;s Modules &gt; GitHub &gt; General Configuration, in the Profiles menu.</source>
        <translation>Defina a URL base do GitHub em Módulos &gt; GitHub &gt; Configuração geral deste perfil, no menu Perfis.</translation>
    </message>
    <message>
        <source>Set the repository and number in the item&apos;s settings.</source>
        <translation>Defina o repositório e o número nas configurações do item.</translation>
    </message>
    <message>
        <source>Repository</source>
        <translation>Repositório</translation>
    </message>
    <message>
        <source>Pull request number</source>
        <translation>Número do pull request</translation>
    </message>
    <message>
        <source>Rows to fetch</source>
        <translation>Linhas a buscar</translation>
    </message>
    <message>
        <source>Rows per page</source>
        <translation>Linhas por página</translation>
    </message>
    <message>
        <source>Scope</source>
        <translation>Escopo</translation>
    </message>
    <message>
        <source>One repository</source>
        <translation>Um repositório</translation>
    </message>
    <message>
        <source>One owner</source>
        <translation>Um dono</translation>
    </message>
    <message>
        <source>All</source>
        <translation>Todos</translation>
    </message>
    <message>
        <source>Owner</source>
        <translation>Dono</translation>
    </message>
    <message>
        <source>Show</source>
        <translation>Mostrar</translation>
    </message>
    <message>
        <source>Custom filter</source>
        <translation>Filtro personalizado</translation>
    </message>
    <message>
        <source>Branch</source>
        <translation>Branch</translation>
    </message>
    <message>
        <source>Default branch</source>
        <translation>Branch padrão</translation>
    </message>
    <message>
        <source>Workflow file</source>
        <translation>Arquivo do workflow</translation>
    </message>
    <message>
        <source>Branch from</source>
        <translation>Origem do branch</translation>
    </message>
    <message>
        <source>Fixed branch</source>
        <translation>Branch fixo</translation>
    </message>
    <message>
        <source>Current branch of the main folder</source>
        <translation>Branch atual da pasta principal</translation>
    </message>
    <message>
        <source>Branch of a linked pull request</source>
        <translation>Branch de um pull request vinculado</translation>
    </message>
    <message>
        <source>Of a branch</source>
        <translation>De um branch</translation>
    </message>
    <message>
        <source>Of a pull request</source>
        <translation>De um pull request</translation>
    </message>
    <message>
        <source>Issue number</source>
        <translation>Número da issue</translation>
    </message>
    <message>
        <source>Show the checks</source>
        <translation>Mostrar as verificações</translation>
    </message>
    <message>
        <source>Show the reviews</source>
        <translation>Mostrar as revisões</translation>
    </message>
    <message>
        <source>Show the labels</source>
        <translation>Mostrar as etiquetas</translation>
    </message>
    <message>
        <source>GitHub</source>
        <translation>GitHub</translation>
    </message>
    <message>
        <source>Pull requests, Actions runs and issues from GitHub or GitHub Enterprise Server. Each profile sets its server and token in the module&apos;s General Configuration.</source>
        <translation>Pull requests, execuções do Actions e issues do GitHub ou do GitHub Enterprise Server. Cada perfil define seu servidor e token na Configuração geral do módulo.</translation>
    </message>
    <message>
        <source>GitHub PR (summary)</source>
        <translation>GitHub PR (resumo)</translation>
    </message>
    <message>
        <source>PR</source>
        <translation>PR</translation>
    </message>
    <message>
        <source>A summary of one pull request: its state, branches, checks, reviews and people.</source>
        <translation>Um resumo de um pull request: seu estado, branches, verificações, revisões e pessoas.</translation>
    </message>
    <message>
        <source>Show the people</source>
        <translation>Mostrar as pessoas</translation>
    </message>
    <message>
        <source>GitHub PR (page)</source>
        <translation>GitHub PR (página)</translation>
    </message>
    <message>
        <source>The web page of one pull request, on the GitHub server of the item&apos;s profile.</source>
        <translation>A página web de um pull request, no servidor GitHub do perfil do item.</translation>
    </message>
    <message>
        <source>GitHub PR List</source>
        <translation>Lista de PRs do GitHub</translation>
    </message>
    <message>
        <source>PRs</source>
        <translation>PRs</translation>
    </message>
    <message>
        <source>A table of pull requests in a repository, an owner or everywhere, such as those that wait for your review.</source>
        <translation>Uma tabela de pull requests de um repositório, de um dono ou de todos, como os que aguardam a sua revisão.</translation>
    </message>
    <message>
        <source>GitHub Actions Run</source>
        <translation>Execução do GitHub Actions</translation>
    </message>
    <message>
        <source>Run</source>
        <translation>Execução</translation>
    </message>
    <message>
        <source>The Actions runs of a branch&apos;s newest commit, one per workflow, with their jobs. The branch is a fixed one, the main folder&apos;s current branch or a pull request&apos;s branch.</source>
        <translation>As execuções do Actions do commit mais recente de um branch, uma por workflow, com seus jobs. O branch é um branch fixo, o branch atual da pasta principal ou o branch de um pull request.</translation>
    </message>
    <message>
        <source>GitHub Actions Run List</source>
        <translation>Lista de execuções do GitHub Actions</translation>
    </message>
    <message>
        <source>Runs</source>
        <translation>Execuções</translation>
    </message>
    <message>
        <source>A table of a repository&apos;s Actions runs: all of them, those you started, or those of a branch or a pull request.</source>
        <translation>Uma tabela das execuções do Actions de um repositório: todas, as que você iniciou, ou as de um branch ou de um pull request.</translation>
    </message>
    <message>
        <source>GitHub Issue (summary)</source>
        <translation>GitHub Issue (resumo)</translation>
    </message>
    <message>
        <source>Issue</source>
        <translation>Issue</translation>
    </message>
    <message>
        <source>A summary of one issue: its state, assignees, labels, milestone and latest comments.</source>
        <translation>Um resumo de uma issue: seu estado, responsáveis, etiquetas, marco e comentários mais recentes.</translation>
    </message>
    <message>
        <source>Show the details</source>
        <translation>Mostrar os detalhes</translation>
    </message>
    <message>
        <source>Show the comments</source>
        <translation>Mostrar os comentários</translation>
    </message>
    <message>
        <source>GitHub Issue (page)</source>
        <translation>GitHub Issue (página)</translation>
    </message>
    <message>
        <source>The web page of one issue, on the GitHub server of the item&apos;s profile.</source>
        <translation>A página web de uma issue, no servidor GitHub do perfil do item.</translation>
    </message>
    <message>
        <source>GitHub Issue List</source>
        <translation>Lista de issues do GitHub</translation>
    </message>
    <message>
        <source>Issues</source>
        <translation>Issues</translation>
    </message>
    <message>
        <source>A table of issues in a repository, an owner or everywhere, such as those assigned to you.</source>
        <translation>Uma tabela de issues de um repositório, de um dono ou de todos, como as atribuídas a você.</translation>
    </message>
    <message>
        <source>General Configuration</source>
        <translation>Configuração geral</translation>
    </message>
    <message>
        <source>Base URL</source>
        <translation>URL base</translation>
    </message>
    <message>
        <source>Token</source>
        <translation>Token</translation>
    </message>
    <message>
        <source>Kept in this device&apos;s keychain</source>
        <translation>Guardado no chaveiro deste dispositivo</translation>
    </message>
    <message>
        <source>Repositories</source>
        <translation>Repositórios</translation>
    </message>
    <message>
        <source>GitHub repository</source>
        <translation>Repositório do GitHub</translation>
    </message>
    <message>
        <source>Merged</source>
        <translation>Mesclada</translation>
    </message>
    <message>
        <source>Closed</source>
        <translation>Fechada</translation>
    </message>
    <message>
        <source>Draft</source>
        <translation>Rascunho</translation>
    </message>
    <message>
        <source>Open</source>
        <translation>Aberta</translation>
    </message>
    <message>
        <source>checks failed</source>
        <translation>verificações falharam</translation>
    </message>
    <message>
        <source>checks running</source>
        <translation>verificações em andamento</translation>
    </message>
    <message>
        <source>checks passed</source>
        <translation>verificações bem-sucedidas</translation>
    </message>
    <message>
        <source>Run failed</source>
        <translation>Execução falhou</translation>
    </message>
    <message>
        <source>Running</source>
        <translation>Em andamento</translation>
    </message>
    <message>
        <source>Queued</source>
        <translation>Na fila</translation>
    </message>
    <message>
        <source>Waiting</source>
        <translation>Aguardando</translation>
    </message>
    <message>
        <source>Passed</source>
        <translation>Bem-sucedido</translation>
    </message>
    <message>
        <source>Timed out</source>
        <translation>Tempo esgotado</translation>
    </message>
    <message>
        <source>Failed</source>
        <translation>Falhou</translation>
    </message>
    <message>
        <source>Cancelled</source>
        <translation>Cancelada</translation>
    </message>
    <message>
        <source>Skipped</source>
        <translation>Ignorada</translation>
    </message>
    <message>
        <source>Neutral</source>
        <translation>Neutra</translation>
    </message>
    <message>
        <source>Action required</source>
        <translation>Ação necessária</translation>
    </message>
    <message>
        <source>Stale</source>
        <translation>Obsoleta</translation>
    </message>
    <message>
        <source>changes requested</source>
        <translation>alterações solicitadas</translation>
    </message>
    <message numerus="yes">
        <source>%n approval(s)</source>
        <translation>
            <numerusform>%n aprovação</numerusform>
            <numerusform>%n aprovações</numerusform>
        </translation>
    </message>
    <message>
        <source>has conflicts</source>
        <translation>tem conflitos</translation>
    </message>
    <message>
        <source>Checks failed</source>
        <translation>Verificações falharam</translation>
    </message>
    <message>
        <source>Review requested</source>
        <translation>Revisão solicitada</translation>
    </message>
    <message>
        <source>Approved</source>
        <translation>Aprovada</translation>
    </message>
    <message>
        <source>Changes requested</source>
        <translation>Alterações solicitadas</translation>
    </message>
    <message>
        <source>Has conflicts</source>
        <translation>Tem conflitos</translation>
    </message>
    <message>
        <source>assigned to %1</source>
        <translation>atribuída a %1</translation>
    </message>
    <message>
        <source>No runs on %1</source>
        <translation>Nenhuma execução em %1</translation>
    </message>
    <message numerus="yes">
        <source>%1 of %n run(s) failed</source>
        <translation>
            <numerusform>%1 de %n execução falhou</numerusform>
            <numerusform>%1 de %n execuções falharam</numerusform>
        </translation>
    </message>
    <message>
        <source>Set the main folder&apos;s path on this device.</source>
        <translation>Defina o caminho da pasta principal neste dispositivo.</translation>
    </message>
    <message>
        <source>The main folder is not on a branch.</source>
        <translation>A pasta principal não está em um branch.</translation>
    </message>
    <message>
        <source>PRs assigned to me</source>
        <translation>PRs atribuídos a mim</translation>
    </message>
    <message>
        <source>Issues assigned to me</source>
        <translation>Issues atribuídas a mim</translation>
    </message>
    <message>
        <source>PRs to review</source>
        <translation>PRs para revisar</translation>
    </message>
    <message>
        <source>My PRs</source>
        <translation>Meus PRs</translation>
    </message>
    <message>
        <source>My issues</source>
        <translation>Minhas issues</translation>
    </message>
    <message>
        <source>Loading…</source>
        <translation>Carregando…</translation>
    </message>
</context>
<context>
    <name>gitlab</name>
    <message>
        <source>No issues</source>
        <translation>Nenhuma issue</translation>
    </message>
    <message>
        <source>Web</source>
        <translation>Web</translation>
    </message>
    <message>
        <source>Open</source>
        <translation>Aberta</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>Adicionar</translation>
    </message>
    <message>
        <source>Loading…</source>
        <translation>Carregando…</translation>
    </message>
    <message>
        <source>Open page</source>
        <translation>Abrir página</translation>
    </message>
    <message>
        <source>Add to task as item</source>
        <translation>Adicionar à tarefa como item</translation>
    </message>
    <message>
        <source>Copy link</source>
        <translation>Copiar link</translation>
    </message>
    <message>
        <source>No merge requests</source>
        <translation>Nenhuma merge request</translation>
    </message>
    <message>
        <source>Set the GitLab base URL in this profile&apos;s Modules &gt; GitLab &gt; General Configuration, in the Profiles menu.</source>
        <translation>Defina a URL base do GitLab em Módulos &gt; GitLab &gt; Configuração geral deste perfil, no menu Perfis.</translation>
    </message>
    <message>
        <source>Set the project and number in the item&apos;s settings.</source>
        <translation>Defina o projeto e o número nas configurações do item.</translation>
    </message>
    <message>
        <source>Project path</source>
        <translation>Caminho do projeto</translation>
    </message>
    <message>
        <source>Rows to fetch</source>
        <translation>Linhas a buscar</translation>
    </message>
    <message>
        <source>Rows per page</source>
        <translation>Linhas por página</translation>
    </message>
    <message>
        <source>Scope</source>
        <translation>Escopo</translation>
    </message>
    <message>
        <source>One project</source>
        <translation>Um projeto</translation>
    </message>
    <message>
        <source>One group</source>
        <translation>Um grupo</translation>
    </message>
    <message>
        <source>All projects visible to me</source>
        <translation>Todos os projetos visíveis para mim</translation>
    </message>
    <message>
        <source>Group path</source>
        <translation>Caminho do grupo</translation>
    </message>
    <message>
        <source>Show</source>
        <translation>Mostrar</translation>
    </message>
    <message>
        <source>Custom filter</source>
        <translation>Filtro personalizado</translation>
    </message>
    <message>
        <source>Branch</source>
        <translation>Branch</translation>
    </message>
    <message>
        <source>Branch from</source>
        <translation>Origem do branch</translation>
    </message>
    <message>
        <source>A fixed branch</source>
        <translation>Um branch fixo</translation>
    </message>
    <message>
        <source>Current branch of the main folder</source>
        <translation>Branch atual da pasta principal</translation>
    </message>
    <message>
        <source>Branch of a linked merge request</source>
        <translation>Branch de uma merge request vinculada</translation>
    </message>
    <message>
        <source>Merge request number</source>
        <translation>Número da merge request</translation>
    </message>
    <message>
        <source>All pipelines</source>
        <translation>Todos os pipelines</translation>
    </message>
    <message>
        <source>Started by me</source>
        <translation>Iniciados por mim</translation>
    </message>
    <message>
        <source>Of a branch</source>
        <translation>De um branch</translation>
    </message>
    <message>
        <source>Of a merge request</source>
        <translation>De uma merge request</translation>
    </message>
    <message>
        <source>Issue number</source>
        <translation>Número da issue</translation>
    </message>
    <message>
        <source>Show the pipeline</source>
        <translation>Mostrar o pipeline</translation>
    </message>
    <message>
        <source>Show the approvals</source>
        <translation>Mostrar as aprovações</translation>
    </message>
    <message>
        <source>Show the labels</source>
        <translation>Mostrar as etiquetas</translation>
    </message>
    <message>
        <source>Merge requests, pipelines and issues from a GitLab server. Each profile sets its server and token in the module&apos;s General Configuration.</source>
        <translation>Merge requests, pipelines e issues de um servidor GitLab. Cada perfil define seu servidor e token na Configuração geral do módulo.</translation>
    </message>
    <message>
        <source>GitLab MR (summary)</source>
        <translation>GitLab MR (resumo)</translation>
    </message>
    <message>
        <source>MR</source>
        <translation>MR</translation>
    </message>
    <message>
        <source>A summary of one merge request: its state, branches, people, pipeline and approvals.</source>
        <translation>Um resumo de uma merge request: seu estado, branches, pessoas, pipeline e aprovações.</translation>
    </message>
    <message>
        <source>Show the people</source>
        <translation>Mostrar as pessoas</translation>
    </message>
    <message>
        <source>GitLab MR (page)</source>
        <translation>GitLab MR (página)</translation>
    </message>
    <message>
        <source>The web page of one merge request, on the GitLab server of the item&apos;s profile.</source>
        <translation>A página web de uma merge request, no servidor GitLab do perfil do item.</translation>
    </message>
    <message>
        <source>GitLab MR List</source>
        <translation>Lista de MRs do GitLab</translation>
    </message>
    <message>
        <source>MRs</source>
        <translation>MRs</translation>
    </message>
    <message>
        <source>A table of merge requests in a project, a group or everywhere, such as those that wait for your review.</source>
        <translation>Uma tabela de merge requests em um projeto, em um grupo ou em todos os lugares, como as que aguardam sua revisão.</translation>
    </message>
    <message>
        <source>Created by me</source>
        <translation>Criadas por mim</translation>
    </message>
    <message>
        <source>Review requested from me</source>
        <translation>Revisão solicitada a mim</translation>
    </message>
    <message>
        <source>Assigned to me</source>
        <translation>Atribuídas a mim</translation>
    </message>
    <message>
        <source>All open</source>
        <translation>Todas abertas</translation>
    </message>
    <message>
        <source>GitLab CI Pipeline</source>
        <translation>Pipeline de CI do GitLab</translation>
    </message>
    <message>
        <source>CI</source>
        <translation>CI</translation>
    </message>
    <message>
        <source>The latest pipeline of a branch and those started with it, with their stages and jobs. The branch is a fixed one, the main folder&apos;s current branch or a merge request&apos;s branch.</source>
        <translation>O pipeline mais recente de um branch e os iniciados com ele, com seus estágios e jobs. O branch é um branch fixo, o branch atual da pasta principal ou o branch de uma merge request.</translation>
    </message>
    <message>
        <source>GitLab CI Pipeline List</source>
        <translation>Lista de pipelines de CI do GitLab</translation>
    </message>
    <message>
        <source>A table of a project&apos;s pipelines: all of them, those you started, or those of a branch or a merge request.</source>
        <translation>Uma tabela dos pipelines de um projeto: todos, os que você iniciou ou os de um branch ou de uma merge request.</translation>
    </message>
    <message>
        <source>GitLab Issue (summary)</source>
        <translation>GitLab Issue (resumo)</translation>
    </message>
    <message>
        <source>Issue</source>
        <translation>Issue</translation>
    </message>
    <message>
        <source>A summary of one issue: its state, assignees, labels, milestone and latest comments.</source>
        <translation>Um resumo de uma issue: seu estado, responsáveis, etiquetas, marco e comentários mais recentes.</translation>
    </message>
    <message>
        <source>Show the details</source>
        <translation>Mostrar os detalhes</translation>
    </message>
    <message>
        <source>Show the comments</source>
        <translation>Mostrar os comentários</translation>
    </message>
    <message>
        <source>GitLab Issue (page)</source>
        <translation>GitLab Issue (página)</translation>
    </message>
    <message>
        <source>The web page of one issue, on the GitLab server of the item&apos;s profile.</source>
        <translation>A página web de uma issue, no servidor GitLab do perfil do item.</translation>
    </message>
    <message>
        <source>GitLab Issue List</source>
        <translation>Lista de issues do GitLab</translation>
    </message>
    <message>
        <source>Issues</source>
        <translation>Issues</translation>
    </message>
    <message>
        <source>A table of issues in a project, a group or everywhere, such as those assigned to you.</source>
        <translation>Uma tabela de issues em um projeto, em um grupo ou em todos os lugares, como as atribuídas a você.</translation>
    </message>
    <message>
        <source>General Configuration</source>
        <translation>Configuração geral</translation>
    </message>
    <message>
        <source>Base URL</source>
        <translation>URL base</translation>
    </message>
    <message>
        <source>Token</source>
        <translation>Token</translation>
    </message>
    <message>
        <source>Kept in this device&apos;s keychain</source>
        <translation>Guardado no chaveiro deste dispositivo</translation>
    </message>
    <message>
        <source>Project paths</source>
        <translation>Caminhos de projeto</translation>
    </message>
    <message>
        <source>GitLab project path</source>
        <translation>Caminho do projeto no GitLab</translation>
    </message>
    <message>
        <source>Page</source>
        <translation>Página</translation>
    </message>
    <message>
        <source>This item has no server.</source>
        <translation>Este item não tem servidor.</translation>
    </message>
    <message>
        <source>Ref</source>
        <translation>Ref</translation>
    </message>
    <message>
        <source>Title</source>
        <translation>Título</translation>
    </message>
    <message>
        <source>Assignees</source>
        <translation>Responsáveis</translation>
    </message>
    <message>
        <source>Labels</source>
        <translation>Etiquetas</translation>
    </message>
    <message>
        <source>Updated</source>
        <translation>Atualizado</translation>
    </message>
    <message>
        <source>Links</source>
        <translation>Links</translation>
    </message>
    <message>
        <source>Details</source>
        <translation>Detalhes</translation>
    </message>
    <message>
        <source>Comments</source>
        <translation>Comentários</translation>
    </message>
    <message>
        <source>Assignees: %1</source>
        <translation>Responsáveis: %1</translation>
    </message>
    <message>
        <source>none</source>
        <translation>nenhum</translation>
    </message>
    <message>
        <source>Milestone: %1</source>
        <translation>Marco: %1</translation>
    </message>
    <message>
        <source>Due: %1</source>
        <translation>Prazo: %1</translation>
    </message>
    <message>
        <source>The comments could not be read.</source>
        <translation>Não foi possível ler os comentários.</translation>
    </message>
    <message>
        <source>No comments</source>
        <translation>Nenhum comentário</translation>
    </message>
    <message>
        <source>Set the project in the item&apos;s settings.</source>
        <translation>Defina o projeto nas configurações do item.</translation>
    </message>
    <message>
        <source>Set the group in the item&apos;s settings.</source>
        <translation>Defina o grupo nas configurações do item.</translation>
    </message>
    <message>
        <source>Set the scope in the item&apos;s settings.</source>
        <translation>Defina o escopo nas configurações do item.</translation>
    </message>
    <message>
        <source>%1+ merge requests</source>
        <translation>%1+ merge requests</translation>
    </message>
    <message>
        <source>%1+ issues</source>
        <translation>%1+ issues</translation>
    </message>
    <message>
        <source>1 merge request</source>
        <translation>1 merge request</translation>
    </message>
    <message>
        <source>1 issue</source>
        <translation>1 issue</translation>
    </message>
    <message>
        <source>%1 merge requests</source>
        <translation>%1 merge requests</translation>
    </message>
    <message>
        <source>%1 issues</source>
        <translation>%1 issues</translation>
    </message>
    <message>
        <source>%1+ pipelines</source>
        <translation>%1+ pipelines</translation>
    </message>
    <message>
        <source>1 pipeline</source>
        <translation>1 pipeline</translation>
    </message>
    <message>
        <source>%1 pipelines</source>
        <translation>%1 pipelines</translation>
    </message>
    <message>
        <source>group %1</source>
        <translation>grupo %1</translation>
    </message>
    <message>
        <source>all projects</source>
        <translation>todos os projetos</translation>
    </message>
    <message>
        <source>Of branch %1</source>
        <translation>Do branch %1</translation>
    </message>
    <message>
        <source>Of merge request !%1</source>
        <translation>Da merge request !%1</translation>
    </message>
    <message>
        <source>Set the merge request number in the item&apos;s settings.</source>
        <translation>Defina o número da merge request nas configurações do item.</translation>
    </message>
    <message>
        <source>Set the source in the item&apos;s settings.</source>
        <translation>Defina a origem nas configurações do item.</translation>
    </message>
    <message>
        <source>Author</source>
        <translation>Autor</translation>
    </message>
    <message>
        <source>Pipeline</source>
        <translation>Pipeline</translation>
    </message>
    <message>
        <source>Approvals</source>
        <translation>Aprovações</translation>
    </message>
    <message>
        <source>Retry pipeline</source>
        <translation>Repetir pipeline</translation>
    </message>
    <message>
        <source>Retry</source>
        <translation>Repetir</translation>
    </message>
    <message>
        <source>People</source>
        <translation>Pessoas</translation>
    </message>
    <message>
        <source>Draft</source>
        <translation>Rascunho</translation>
    </message>
    <message>
        <source>No pipeline</source>
        <translation>Nenhum pipeline</translation>
    </message>
    <message>
        <source>Log</source>
        <translation>Log</translation>
    </message>
    <message>
        <source>%1 of %2 approvals</source>
        <translation>%1 de %2 aprovações</translation>
    </message>
    <message>
        <source>1 approval</source>
        <translation>1 aprovação</translation>
    </message>
    <message>
        <source>%1 approvals</source>
        <translation>%1 aprovações</translation>
    </message>
    <message>
        <source>No approvals</source>
        <translation>Nenhuma aprovação</translation>
    </message>
    <message>
        <source>Approved by %1</source>
        <translation>Aprovada por %1</translation>
    </message>
    <message>
        <source>Author: %1</source>
        <translation>Autor: %1</translation>
    </message>
    <message>
        <source>Reviewers: %1</source>
        <translation>Revisores: %1</translation>
    </message>
    <message>
        <source>GitLab</source>
        <translation>GitLab</translation>
    </message>
    <message>
        <source>No pipelines</source>
        <translation>Nenhum pipeline</translation>
    </message>
    <message>
        <source>Id</source>
        <translation>Id</translation>
    </message>
    <message>
        <source>Status</source>
        <translation>Status</translation>
    </message>
    <message>
        <source>Started</source>
        <translation>Iniciado</translation>
    </message>
    <message>
        <source>Cancel pipeline</source>
        <translation>Cancelar pipeline</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Cancelar</translation>
    </message>
    <message>
        <source>Trigger pipeline</source>
        <translation>Disparar pipeline</translation>
    </message>
    <message>
        <source>Trigger</source>
        <translation>Disparar</translation>
    </message>
    <message>
        <source>Merge request</source>
        <translation>Merge request</translation>
    </message>
    <message>
        <source>Previous</source>
        <translation>Anterior</translation>
    </message>
    <message>
        <source>Next</source>
        <translation>Próximo</translation>
    </message>
    <message>
        <source>Pipeline #%1</source>
        <translation>Pipeline #%1</translation>
    </message>
    <message>
        <source>The jobs could not be read.</source>
        <translation>Não foi possível ler os jobs.</translation>
    </message>
    <message>
        <source>Jobs</source>
        <translation>Jobs</translation>
    </message>
    <message>
        <source>No jobs</source>
        <translation>Nenhum job</translation>
    </message>
    <message>
        <source>Play</source>
        <translation>Executar</translation>
    </message>
    <message>
        <source>Cancel pipeline #%1?</source>
        <translation>Cancelar o pipeline #%1?</translation>
    </message>
    <message>
        <source>Start a new pipeline on %1?</source>
        <translation>Iniciar um novo pipeline em %1?</translation>
    </message>
    <message>
        <source>passed</source>
        <translation>bem-sucedido</translation>
    </message>
    <message>
        <source>Merged</source>
        <translation>Mesclada</translation>
    </message>
    <message>
        <source>Closed</source>
        <translation>Fechada</translation>
    </message>
    <message>
        <source>Locked</source>
        <translation>Bloqueada</translation>
    </message>
    <message>
        <source>Pipeline failed</source>
        <translation>Pipeline falhou</translation>
    </message>
    <message>
        <source>draft</source>
        <translation>rascunho</translation>
    </message>
    <message>
        <source>pipeline %1</source>
        <translation>pipeline %1</translation>
    </message>
    <message>
        <source>has conflicts</source>
        <translation>tem conflitos</translation>
    </message>
    <message>
        <source>Review requested</source>
        <translation>Revisão solicitada</translation>
    </message>
    <message>
        <source>Approved</source>
        <translation>Aprovada</translation>
    </message>
    <message>
        <source>Has conflicts</source>
        <translation>Tem conflitos</translation>
    </message>
    <message>
        <source>assigned to %1</source>
        <translation>atribuída a %1</translation>
    </message>
    <message>
        <source>No pipeline on %1</source>
        <translation>Nenhum pipeline em %1</translation>
    </message>
    <message>
        <source>1 failed job</source>
        <translation>1 job com falha</translation>
    </message>
    <message>
        <source>%1 failed jobs</source>
        <translation>%1 jobs com falha</translation>
    </message>
    <message>
        <source>Set the main folder&apos;s path on this device.</source>
        <translation>Defina o caminho da pasta principal neste dispositivo.</translation>
    </message>
    <message>
        <source>The main folder is not on a branch.</source>
        <translation>A pasta principal não está em um branch.</translation>
    </message>
    <message>
        <source>%1 s</source>
        <translation>%1 s</translation>
    </message>
    <message>
        <source>%1 min %2 s</source>
        <translation>%1 min %2 s</translation>
    </message>
    <message>
        <source>%1 h %2 min</source>
        <translation>%1 h %2 min</translation>
    </message>
    <message>
        <source>Passed</source>
        <translation>Bem-sucedido</translation>
    </message>
    <message>
        <source>The token cannot do this. It needs the api scope.</source>
        <translation>O token não pode fazer isso. Ele precisa do escopo api.</translation>
    </message>
    <message>
        <source>The action failed.</source>
        <translation>A ação falhou.</translation>
    </message>
    <message>
        <source>Failed, allowed</source>
        <translation>Falhou, permitido</translation>
    </message>
    <message>
        <source>No labels</source>
        <translation>Nenhuma etiqueta</translation>
    </message>
    <message>
        <source>Open</source>
        <comment>link that opens a row</comment>
        <translation>Abrir</translation>
    </message>
</context>
<context>
    <name>image</name>
    <message>
        <source>Image</source>
        <translation>Imagem</translation>
    </message>
    <message>
        <source>An image, such as a screenshot, scaled to fit the cell, with an optional memo under it. A click opens its link, if it has one.</source>
        <translation>Uma imagem, como uma captura de tela, ajustada à célula, com um memorando opcional abaixo. Um clique abre o link dela, se houver.</translation>
    </message>
    <message>
        <source>Link</source>
        <translation>Link</translation>
    </message>
    <message>
        <source>Memo</source>
        <translation>Memorando</translation>
    </message>
    <message>
        <source>Images, such as screenshots or diagrams, with an optional link and memo.</source>
        <translation>Imagens, como capturas de tela ou diagramas, com link e memorando opcionais.</translation>
    </message>
    <message>
        <source>The image file is missing.</source>
        <translation>O arquivo de imagem está faltando.</translation>
    </message>
    <message>
        <source>The image file cannot be read.</source>
        <translation>Não é possível ler o arquivo de imagem.</translation>
    </message>
</context>
<context>
    <name>jira</name>
    <message>
        <source>%1+ issues</source>
        <translation>%1+ issues</translation>
    </message>
    <message>
        <source>1 issue</source>
        <translation>1 issue</translation>
    </message>
    <message>
        <source>%1 issues</source>
        <translation>%1 issues</translation>
    </message>
    <message>
        <source>Current sprint</source>
        <translation>Sprint atual</translation>
    </message>
    <message>
        <source>Current sprint of %1</source>
        <translation>Sprint atual de %1</translation>
    </message>
    <message>
        <source>Recently updated</source>
        <translation>Atualizadas recentemente</translation>
    </message>
    <message>
        <source>Recently updated in %1</source>
        <translation>Atualizadas recentemente em %1</translation>
    </message>
    <message>
        <source>My open issues</source>
        <translation>Minhas issues abertas</translation>
    </message>
    <message>
        <source>Set the JQL query in the item&apos;s settings.</source>
        <translation>Defina a consulta JQL nas configurações do item.</translation>
    </message>
    <message>
        <source>Set the Jira project key in the project&apos;s settings.</source>
        <translation>Defina a chave do projeto do Jira nas configurações do projeto.</translation>
    </message>
    <message>
        <source>Set the Jira base URL in this profile&apos;s Modules &gt; Jira &gt; General Configuration, in the Profiles menu.</source>
        <translation>Defina a URL base do Jira em Módulos &gt; Jira &gt; Configuração geral deste perfil, no menu Perfis.</translation>
    </message>
    <message>
        <source>Set the issue key in the item&apos;s settings.</source>
        <translation>Defina a chave da issue nas configurações do item.</translation>
    </message>
    <message>
        <source>Issue key</source>
        <translation>Chave da issue</translation>
    </message>
    <message>
        <source>Show the details</source>
        <translation>Mostrar os detalhes</translation>
    </message>
    <message>
        <source>Show the description</source>
        <translation>Mostrar a descrição</translation>
    </message>
    <message>
        <source>Show the comments</source>
        <translation>Mostrar os comentários</translation>
    </message>
    <message>
        <source>Show the sub-tasks and links</source>
        <translation>Mostrar as subtarefas e links</translation>
    </message>
    <message>
        <source>Show</source>
        <translation>Mostrar</translation>
    </message>
    <message>
        <source>Current sprint of this project</source>
        <translation>Sprint atual deste projeto</translation>
    </message>
    <message>
        <source>Recently updated in this project</source>
        <translation>Atualizadas recentemente neste projeto</translation>
    </message>
    <message>
        <source>The JQL query below</source>
        <translation>A consulta JQL abaixo</translation>
    </message>
    <message>
        <source>JQL query</source>
        <translation>Consulta JQL</translation>
    </message>
    <message>
        <source>Rows to fetch</source>
        <translation>Linhas a buscar</translation>
    </message>
    <message>
        <source>Rows per page</source>
        <translation>Linhas por página</translation>
    </message>
    <message>
        <source>Jira project key</source>
        <translation>Chave do projeto do Jira</translation>
    </message>
    <message>
        <source>Issues and lists of issues from a Jira site. Each profile sets its site and token in the module&apos;s General Configuration.</source>
        <translation>Issues e listas de issues de um site do Jira. Cada perfil define seu site e token na Configuração geral do módulo.</translation>
    </message>
    <message>
        <source>Jira Issue (summary)</source>
        <translation>Issue do Jira (resumo)</translation>
    </message>
    <message>
        <source>Jira</source>
        <translation>Jira</translation>
    </message>
    <message>
        <source>A summary of one issue: its status, people, sprint, description and latest comments.</source>
        <translation>Um resumo de uma issue: status, pessoas, sprint, descrição e comentários mais recentes.</translation>
    </message>
    <message>
        <source>Jira Issue (page)</source>
        <translation>Issue do Jira (página)</translation>
    </message>
    <message>
        <source>The web page of one issue, on the Jira site of the item&apos;s profile.</source>
        <translation>A página web de uma issue, no site do Jira do perfil do item.</translation>
    </message>
    <message>
        <source>Jira List</source>
        <translation>Lista do Jira</translation>
    </message>
    <message>
        <source>A table of issues from a preset, such as your open issues or the current sprint, or from a JQL query.</source>
        <translation>Uma tabela de issues de uma predefinição, como suas issues abertas ou a sprint atual, ou de uma consulta JQL.</translation>
    </message>
    <message>
        <source>Set the email for Jira Cloud in the profile&apos;s Jira settings.</source>
        <translation>Defina o email do Jira Cloud nas configurações do Jira do perfil.</translation>
    </message>
    <message>
        <source>General Configuration</source>
        <translation>Configuração geral</translation>
    </message>
    <message>
        <source>Base URL</source>
        <translation>URL base</translation>
    </message>
    <message>
        <source>Deployment</source>
        <translation>Implantação</translation>
    </message>
    <message>
        <source>Automatic</source>
        <translation>Automática</translation>
    </message>
    <message>
        <source>Jira Cloud</source>
        <translation>Jira Cloud</translation>
    </message>
    <message>
        <source>Jira Data Center</source>
        <translation>Jira Data Center</translation>
    </message>
    <message>
        <source>Email</source>
        <translation>Email</translation>
    </message>
    <message>
        <source>Your Atlassian account&apos;s email, for Jira Cloud</source>
        <translation>O email da sua conta Atlassian, para o Jira Cloud</translation>
    </message>
    <message>
        <source>Token</source>
        <translation>Token</translation>
    </message>
    <message>
        <source>Kept in this device&apos;s keychain</source>
        <translation>Guardado no chaveiro deste dispositivo</translation>
    </message>
    <message>
        <source>Extra fields</source>
        <translation>Campos extras</translation>
    </message>
    <message>
        <source>Field names, one per line, such as Team</source>
        <translation>Nomes de campos, um por linha, como Team</translation>
    </message>
    <message>
        <source>unassigned</source>
        <translation>sem responsável</translation>
    </message>
    <message>
        <source>assigned to %1</source>
        <translation>atribuída a %1</translation>
    </message>
    <message>
        <source>Status changed to %1</source>
        <translation>Status alterado para %1</translation>
    </message>
    <message>
        <source>Jira search</source>
        <translation>Pesquisa do Jira</translation>
    </message>
    <message>
        <source>Sub-task</source>
        <translation>Subtarefa</translation>
    </message>
    <message>
        <source>Open page</source>
        <translation>Abrir página</translation>
    </message>
    <message>
        <source>Page</source>
        <translation>Página</translation>
    </message>
    <message>
        <source>Loading…</source>
        <translation>Carregando…</translation>
    </message>
    <message>
        <source>Details</source>
        <translation>Detalhes</translation>
    </message>
    <message>
        <source>Comments</source>
        <translation>Comentários</translation>
    </message>
    <message>
        <source>Sub-tasks and links</source>
        <translation>Subtarefas e links</translation>
    </message>
    <message>
        <source>Priority</source>
        <translation>Prioridade</translation>
    </message>
    <message>
        <source>Assignee</source>
        <translation>Responsável</translation>
    </message>
    <message>
        <source>Unassigned</source>
        <translation>Sem responsável</translation>
    </message>
    <message>
        <source>Reporter</source>
        <translation>Relator</translation>
    </message>
    <message>
        <source>Sprint</source>
        <translation>Sprint</translation>
    </message>
    <message>
        <source>Description</source>
        <translation>Descrição</translation>
    </message>
    <message>
        <source>No description.</source>
        <translation>Sem descrição.</translation>
    </message>
    <message>
        <source>No comments.</source>
        <translation>Sem comentários.</translation>
    </message>
    <message>
        <source>No sub-tasks or links.</source>
        <translation>Sem subtarefas ou links.</translation>
    </message>
    <message>
        <source>%1 more</source>
        <translation>mais %1</translation>
    </message>
    <message>
        <source>Web</source>
        <translation>Web</translation>
    </message>
    <message>
        <source>Open</source>
        <translation>Abrir</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>Adicionar</translation>
    </message>
    <message>
        <source>Key</source>
        <translation>Chave</translation>
    </message>
    <message>
        <source>Summary</source>
        <translation>Resumo</translation>
    </message>
    <message>
        <source>Status</source>
        <translation>Status</translation>
    </message>
    <message>
        <source>Updated</source>
        <translation>Atualizada</translation>
    </message>
    <message>
        <source>Links</source>
        <translation>Links</translation>
    </message>
    <message>
        <source>Add to task as item</source>
        <translation>Adicionar à tarefa como item</translation>
    </message>
    <message>
        <source>Copy link</source>
        <translation>Copiar link</translation>
    </message>
</context>
<context>
    <name>main</name>
    <message>
        <source>Mirante is already running with the web data folder %1, but it does not answer. Close it, or wait for it, and start Mirante again.</source>
        <translation>O Mirante já está em execução com a pasta de dados da web %1, mas não responde. Feche-o, ou aguarde, e inicie o Mirante novamente.</translation>
    </message>
    <message>
        <source>Mirante cannot use the web data folder %1.</source>
        <translation>O Mirante não pode usar a pasta de dados da web %1.</translation>
    </message>
</context>
<context>
    <name>mirante::CellFrame</name>
    <message>
        <source>Refresh</source>
        <translation>Atualizar</translation>
    </message>
    <message>
        <source>Cell menu</source>
        <translation>Menu da célula</translation>
    </message>
    <message>
        <source>Open Alone</source>
        <translation>Abrir sozinho</translation>
    </message>
    <message>
        <source>Open this item on its own</source>
        <translation>Abrir este item sozinho</translation>
    </message>
    <message>
        <source>Removed</source>
        <translation>Removido</translation>
    </message>
    <message>
        <source>Empty</source>
        <translation>Vazia</translation>
    </message>
    <message>
        <source>Task: %1</source>
        <translation>Tarefa: %1</translation>
    </message>
    <message>
        <source>Shown in another cell</source>
        <translation>Mostrado em outra célula</translation>
    </message>
    <message>
        <source>Show here</source>
        <translation>Mostrar aqui</translation>
    </message>
    <message>
        <source>Restore (%1)</source>
        <translation>Restaurar (%1)</translation>
    </message>
    <message>
        <source>Maximise (%1)</source>
        <translation>Maximizar (%1)</translation>
    </message>
</context>
<context>
    <name>mirante::ConflictBar</name>
    <message>
        <source>the workspace file</source>
        <translation>o arquivo do espaço de trabalho</translation>
    </message>
    <message>
        <source>image %1</source>
        <translation>a imagem %1</translation>
    </message>
    <message>
        <source>group %1</source>
        <translation>o grupo %1</translation>
    </message>
    <message>
        <source>project %1</source>
        <translation>o projeto %1</translation>
    </message>
    <message>
        <source>task %1</source>
        <translation>a tarefa %1</translation>
    </message>
    <message>
        <source>dashboard %1</source>
        <translation>o painel %1</translation>
    </message>
    <message>
        <source>Keep current</source>
        <translation>Manter atual</translation>
    </message>
    <message>
        <source>Delete the other version</source>
        <translation>Excluir a outra versão</translation>
    </message>
    <message>
        <source>Use other</source>
        <translation>Usar a outra</translation>
    </message>
    <message>
        <source>Replace the current version with the other one</source>
        <translation>Substituir a versão atual pela outra</translation>
    </message>
    <message>
        <source>Compare</source>
        <translation>Comparar</translation>
    </message>
    <message>
        <source>Show both versions side by side</source>
        <translation>Mostrar as duas versões lado a lado</translation>
    </message>
    <message>
        <source>Replace %1 with the other version? The current version is deleted.</source>
        <translation>Substituir %1 pela outra versão? A versão atual é excluída.</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>Fechar</translation>
    </message>
    <message>
        <source>Use other version</source>
        <translation>Usar a outra versão</translation>
    </message>
    <message>
        <source>Two versions of %1</source>
        <translation>Duas versões de %1</translation>
    </message>
</context>
<context>
    <name>mirante::DashBridge</name>
    <message>
        <source>Urgent</source>
        <translation>Urgente</translation>
    </message>
    <message>
        <source>High</source>
        <translation>Alta</translation>
    </message>
    <message>
        <source>Normal</source>
        <translation>Normal</translation>
    </message>
    <message>
        <source>Low</source>
        <translation>Baixa</translation>
    </message>
    <message numerus="yes">
        <source>%n item(s) need attention</source>
        <translation>
            <numerusform>%n item precisa de atenção</numerusform>
            <numerusform>%n itens precisam de atenção</numerusform>
        </translation>
    </message>
    <message>
        <source>No items in this task report a status.</source>
        <translation>Nenhum item desta tarefa informa um status.</translation>
    </message>
    <message>
        <source>No active tasks in this group.</source>
        <translation>Nenhuma tarefa ativa neste grupo.</translation>
    </message>
    <message>
        <source>Not fetched yet</source>
        <translation>Ainda não buscado</translation>
    </message>
    <message>
        <source>Closed</source>
        <translation>Fechado</translation>
    </message>
    <message>
        <source>This task</source>
        <translation>Esta tarefa</translation>
    </message>
    <message>
        <source>This panel&apos;s task no longer exists.</source>
        <translation>A tarefa deste quadro não existe mais.</translation>
    </message>
</context>
<context>
    <name>mirante::DashboardDialog</name>
    <message>
        <source>New dashboard</source>
        <translation>Novo painel</translation>
    </message>
    <message>
        <source>Rename dashboard</source>
        <translation>Renomear painel</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Nome:</translation>
    </message>
    <message>
        <source>Layout:</source>
        <translation>Layout:</translation>
    </message>
    <message>
        <source>Enter a name.</source>
        <translation>Digite um nome.</translation>
    </message>
</context>
<context>
    <name>mirante::DetachedWindow</name>
    <message>
        <source>Editing</source>
        <translation>Editando</translation>
    </message>
    <message>
        <source>Locked</source>
        <translation>Travado</translation>
    </message>
    <message>
        <source>Lock this dashboard (%1)</source>
        <translation>Travar este painel (%1)</translation>
    </message>
    <message>
        <source>Unlock this dashboard for editing (%1)</source>
        <translation>Destravar este painel para edição (%1)</translation>
    </message>
    <message>
        <source>Put Back</source>
        <translation>Devolver</translation>
    </message>
    <message>
        <source>Put this dashboard back in the main window</source>
        <translation>Devolver este painel à janela principal</translation>
    </message>
</context>
<context>
    <name>mirante::EntityActions</name>
    <message>
        <source>Urgent</source>
        <translation>Urgente</translation>
    </message>
    <message>
        <source>High</source>
        <translation>Alta</translation>
    </message>
    <message>
        <source>Normal</source>
        <translation>Normal</translation>
    </message>
    <message>
        <source>Low</source>
        <translation>Baixa</translation>
    </message>
    <message>
        <source>Active</source>
        <translation>Ativa</translation>
    </message>
    <message>
        <source>Done</source>
        <translation>Concluída</translation>
    </message>
    <message>
        <source>Archived</source>
        <translation>Arquivada</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>Excluir</translation>
    </message>
    <message>
        <source>1 project</source>
        <translation>1 projeto</translation>
    </message>
    <message>
        <source>%1 projects</source>
        <translation>%1 projetos</translation>
    </message>
    <message>
        <source>1 task</source>
        <translation>1 tarefa</translation>
    </message>
    <message>
        <source>%1 tasks</source>
        <translation>%1 tarefas</translation>
    </message>
    <message>
        <source>%1 and %2</source>
        <translation>%1 e %2</translation>
    </message>
    <message>
        <source>Permanently delete %1 in the trash?</source>
        <translation>Excluir permanentemente %1 na lixeira?</translation>
    </message>
    <message>
        <source>Delete group %1?</source>
        <translation>Excluir o grupo %1?</translation>
    </message>
    <message>
        <source>Delete group %1 and its %2?</source>
        <translation>Excluir o grupo %1, com %2?</translation>
    </message>
    <message>
        <source>Delete group %1, its %2 and %3?</source>
        <translation>Excluir o grupo %1, com %2 e %3?</translation>
    </message>
    <message>
        <source>Delete project %1?</source>
        <translation>Excluir o projeto %1?</translation>
    </message>
    <message>
        <source>Delete project %1 and its %2?</source>
        <translation>Excluir o projeto %1, com %2?</translation>
    </message>
    <message>
        <source>Delete task %1?</source>
        <translation>Excluir a tarefa %1?</translation>
    </message>
    <message>
        <source>Delete item %1 from task %2?</source>
        <translation>Excluir o item %1 da tarefa %2?</translation>
    </message>
    <message>
        <source>Delete dashboard %1?</source>
        <translation>Excluir o painel %1?</translation>
    </message>
    <message>
        <source>Priority</source>
        <translation>Prioridade</translation>
    </message>
    <message>
        <source>State</source>
        <translation>Estado</translation>
    </message>
    <message>
        <source>Paste Image…</source>
        <translation>Colar imagem…</translation>
    </message>
    <message>
        <source>New project…</source>
        <translation>Novo projeto…</translation>
    </message>
    <message>
        <source>New group…</source>
        <translation>Novo grupo…</translation>
    </message>
    <message>
        <source>Edit group…</source>
        <translation>Editar grupo…</translation>
    </message>
    <message>
        <source>Delete group…</source>
        <translation>Excluir grupo…</translation>
    </message>
    <message>
        <source>New task…</source>
        <translation>Nova tarefa…</translation>
    </message>
    <message>
        <source>Edit project…</source>
        <translation>Editar projeto…</translation>
    </message>
    <message>
        <source>Delete project…</source>
        <translation>Excluir projeto…</translation>
    </message>
    <message>
        <source>New item…</source>
        <translation>Novo item…</translation>
    </message>
    <message>
        <source>Paste URL…</source>
        <translation>Colar URL…</translation>
    </message>
    <message>
        <source>Add Items from Link…</source>
        <translation>Adicionar itens de link…</translation>
    </message>
    <message>
        <source>New dashboard…</source>
        <translation>Novo painel…</translation>
    </message>
    <message>
        <source>Edit task…</source>
        <translation>Editar tarefa…</translation>
    </message>
    <message>
        <source>Delete task…</source>
        <translation>Excluir tarefa…</translation>
    </message>
    <message>
        <source>Edit item…</source>
        <translation>Editar item…</translation>
    </message>
    <message>
        <source>Delete item…</source>
        <translation>Excluir item…</translation>
    </message>
    <message>
        <source>Rename dashboard…</source>
        <translation>Renomear painel…</translation>
    </message>
    <message>
        <source>Set as default</source>
        <translation>Definir como padrão</translation>
    </message>
    <message>
        <source>Delete dashboard…</source>
        <translation>Excluir painel…</translation>
    </message>
    <message>
        <source>Restore</source>
        <translation>Restaurar</translation>
    </message>
    <message>
        <source>Delete permanently…</source>
        <translation>Excluir permanentemente…</translation>
    </message>
    <message>
        <source>Watch</source>
        <translation>Acompanhar</translation>
    </message>
    <message>
        <source>Count on the tray icon, and notify</source>
        <translation>Contar no ícone da bandeja e notificar</translation>
    </message>
</context>
<context>
    <name>mirante::FieldForm</name>
    <message>
        <source>Removed when you press OK. Type to set a new one.</source>
        <translation>Removido quando você clicar em OK. Digite para definir um novo.</translation>
    </message>
    <message>
        <source>Set on this device. Type to replace it.</source>
        <translation>Definido neste dispositivo. Digite para substituí-lo.</translation>
    </message>
    <message>
        <source>Not set on this device.</source>
        <translation>Não definido neste dispositivo.</translation>
    </message>
    <message>
        <source>Unknown folder (%1)</source>
        <translation>Pasta desconhecida (%1)</translation>
    </message>
    <message>
        <source>Unknown value (%1)</source>
        <translation>Valor desconhecido (%1)</translation>
    </message>
    <message>
        <source>No image</source>
        <translation>Sem imagem</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>Remover</translation>
    </message>
    <message>
        <source>Remove the token from this device when you press OK.</source>
        <translation>Remover o token deste dispositivo ao clicar em OK.</translation>
    </message>
    <message>
        <source>One entry per line.</source>
        <translation>Uma entrada por linha.</translation>
    </message>
    <message>
        <source>Paste</source>
        <translation>Colar</translation>
    </message>
    <message>
        <source>Choose file…</source>
        <translation>Escolher arquivo…</translation>
    </message>
    <message>
        <source>Image</source>
        <translation>Imagem</translation>
    </message>
    <message>
        <source>The image could not be saved.</source>
        <translation>Não foi possível salvar a imagem.</translation>
    </message>
    <message>
        <source>Choose image</source>
        <translation>Escolher imagem</translation>
    </message>
    <message>
        <source>Images (%1)</source>
        <translation>Imagens (%1)</translation>
    </message>
    <message>
        <source>The file is not an image that can be read.</source>
        <translation>O arquivo não é uma imagem legível.</translation>
    </message>
</context>
<context>
    <name>mirante::FocusView</name>
    <message>
        <source>All groups</source>
        <translation>Todos os grupos</translation>
    </message>
    <message>
        <source>Needs attention first</source>
        <translation>Atenção primeiro</translation>
    </message>
    <message>
        <source>List the tasks with an attention badge at the top</source>
        <translation>Listar no topo as tarefas com indicador de atenção</translation>
    </message>
</context>
<context>
    <name>mirante::GroupDialog</name>
    <message>
        <source>New group</source>
        <translation>Novo grupo</translation>
    </message>
    <message>
        <source>Edit group</source>
        <translation>Editar grupo</translation>
    </message>
    <message>
        <source>Other…</source>
        <translation>Outra…</translation>
    </message>
    <message>
        <source>Group colour</source>
        <translation>Cor do grupo</translation>
    </message>
    <message>
        <source>The title bars and cell headers of the group&apos;s tasks and items, and its projects in the sidebar, are tinted with this colour.</source>
        <translation>As barras de título e os cabeçalhos de célula das tarefas e itens do grupo, e seus projetos na barra lateral, são tingidos com esta cor.</translation>
    </message>
    <message>
        <source>Items of this group use this profile, unless an item chooses its own.</source>
        <translation>Os itens deste grupo usam este perfil, a menos que o item escolha o seu.</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Nome:</translation>
    </message>
    <message>
        <source>Colour:</source>
        <translation>Cor:</translation>
    </message>
    <message>
        <source>Profile:</source>
        <translation>Perfil:</translation>
    </message>
    <message>
        <source>Default</source>
        <translation>Padrão</translation>
    </message>
    <message>
        <source>The default profile (%1)</source>
        <translation>O perfil padrão (%1)</translation>
    </message>
    <message>
        <source>Unknown profile, uses the default</source>
        <translation>Perfil desconhecido, usa o padrão</translation>
    </message>
    <message>
        <source>Create a new profile for this group</source>
        <translation>Criar um novo perfil para este grupo</translation>
    </message>
    <message>
        <source>Profile name:</source>
        <translation>Nome do perfil:</translation>
    </message>
    <message>
        <source>Enter a name.</source>
        <translation>Informe um nome.</translation>
    </message>
    <message>
        <source>Enter a name for the new profile.</source>
        <translation>Informe um nome para o novo perfil.</translation>
    </message>
    <message>
        <source>A profile called &quot;%1&quot; already exists.</source>
        <translation>Já existe um perfil chamado &quot;%1&quot;.</translation>
    </message>
</context>
<context>
    <name>mirante::ImagePasteDialog</name>
    <message>
        <source>Paste Image</source>
        <translation>Colar imagem</translation>
    </message>
    <message>
        <source>Drag on the image to crop it.</source>
        <translation>Arraste sobre a imagem para recortá-la.</translation>
    </message>
    <message>
        <source>Whole Image</source>
        <translation>Imagem inteira</translation>
    </message>
    <message>
        <source>Paste</source>
        <translation>Colar</translation>
    </message>
    <message>
        <source>Crop: %1 × %2 of %3 × %4 pixels</source>
        <translation>Recorte: %1 × %2 de %3 × %4 pixels</translation>
    </message>
    <message>
        <source>%1 × %2 pixels</source>
        <translation>%1 × %2 pixels</translation>
    </message>
</context>
<context>
    <name>mirante::ItemDialog</name>
    <message>
        <source>This build has no item types.</source>
        <translation>Esta versão não tem tipos de item.</translation>
    </message>
    <message>
        <source>This is not a web address, so every type is shown.</source>
        <translation>Isto não é um endereço web, então todos os tipos são mostrados.</translation>
    </message>
    <message>
        <source>No item type can show this link.</source>
        <translation>Nenhum tipo de item pode mostrar este link.</translation>
    </message>
    <message>
        <source>These types can show the link, best first. Double-click one, or select it and choose Next.</source>
        <translation>Estes tipos podem mostrar o link, do melhor para o pior. Clique duas vezes em um, ou selecione-o e escolha Avançar.</translation>
    </message>
    <message>
        <source>Double-click a type, or select it and choose Next.</source>
        <translation>Clique duas vezes em um tipo, ou selecione-o e escolha Avançar.</translation>
    </message>
    <message>
        <source>Paste a link to create an item from it:</source>
        <translation>Cole um link para criar um item a partir dele:</translation>
    </message>
    <message>
        <source>Choose the type of the new item.</source>
        <translation>Escolha o tipo do novo item.</translation>
    </message>
    <message>
        <source>New item</source>
        <translation>Novo item</translation>
    </message>
    <message>
        <source>Edit item</source>
        <translation>Editar item</translation>
    </message>
    <message>
        <source>Type:</source>
        <translation>Tipo:</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Nome:</translation>
    </message>
    <message>
        <source>The item uses the logins and module settings of its profile. It can read the logins and data of every site in the profile.</source>
        <translation>O item usa os logins e as configurações de módulo do seu perfil. Ele pode ler os logins e os dados de todos os sites do perfil.</translation>
    </message>
    <message>
        <source>Profile:</source>
        <translation>Perfil:</translation>
    </message>
    <message>
        <source>This item type is no longer available, so only the name can be changed. The rest of the item is kept.</source>
        <translation>Este tipo de item não está mais disponível, então só o nome pode ser alterado. O resto do item é mantido.</translation>
    </message>
    <message>
        <source>The module of this item type is not available, so only the name can be changed. The rest of the item is kept.</source>
        <translation>O módulo deste tipo de item não está disponível, então só o nome pode ser alterado. O resto do item é mantido.</translation>
    </message>
    <message>
        <source>Next</source>
        <translation>Avançar</translation>
    </message>
    <message>
        <source>Back</source>
        <translation>Voltar</translation>
    </message>
    <message>
        <source>Default</source>
        <translation>Padrão</translation>
    </message>
    <message>
        <source>%1 (from the group)</source>
        <translation>%1 (do grupo)</translation>
    </message>
    <message>
        <source>Unknown profile, uses the group&apos;s</source>
        <translation>Perfil desconhecido, usa o do grupo</translation>
    </message>
    <message>
        <source>Enter a name.</source>
        <translation>Informe um nome.</translation>
    </message>
    <message>
        <source>Count on the tray icon, and notify</source>
        <translation>Contar no ícone da bandeja e notificar</translation>
    </message>
    <message>
        <source>A watched item that needs attention counts on the tray icon, and a failed pipeline sends a desktop notification. The sidebar shows every item&apos;s attention either way.</source>
        <translation>Um item acompanhado que precisa de atenção conta no ícone da bandeja, e um pipeline com falha envia uma notificação na área de trabalho. A barra lateral mostra a atenção de todos os itens de qualquer forma.</translation>
    </message>
    <message>
        <source>Watch:</source>
        <translation>Acompanhar:</translation>
    </message>
    <message>
        <source>While the page has the keyboard focus, these keys go to the page instead of running Mirante&apos;s shortcuts.</source>
        <translation>Enquanto a página tem o foco do teclado, estas teclas vão para a página em vez de executar os atalhos do Mirante.</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>Adicionar</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>Remover</translation>
    </message>
    <message>
        <source>Keys for the page:</source>
        <translation>Teclas da página:</translation>
    </message>
</context>
<context>
    <name>mirante::LayoutEditor</name>
    <message>
        <source>Remove</source>
        <translation>Remover</translation>
    </message>
    <message>
        <source>Tab</source>
        <translation>Aba</translation>
    </message>
    <message>
        <source>Restore</source>
        <translation>Restaurar</translation>
    </message>
    <message>
        <source>Maximise</source>
        <translation>Maximizar</translation>
    </message>
    <message>
        <source>Open Alone</source>
        <translation>Abrir sozinho</translation>
    </message>
    <message>
        <source>Edit Item…</source>
        <translation>Editar item…</translation>
    </message>
    <message>
        <source>Use Current Address</source>
        <translation>Usar endereço atual</translation>
    </message>
    <message>
        <source>Edit task dashboard</source>
        <translation>Editar painel da tarefa</translation>
    </message>
    <message>
        <source>Split right</source>
        <translation>Dividir à direita</translation>
    </message>
    <message>
        <source>Split down</source>
        <translation>Dividir para baixo</translation>
    </message>
    <message>
        <source>Wrap in tabs</source>
        <translation>Envolver em abas</translation>
    </message>
    <message>
        <source>Add tab</source>
        <translation>Adicionar aba</translation>
    </message>
    <message>
        <source>Clear cell</source>
        <translation>Limpar célula</translation>
    </message>
    <message>
        <source>Clear this cell? It stays in the layout, empty.</source>
        <translation>Limpar esta célula? Ela permanece no layout, vazia.</translation>
    </message>
    <message>
        <source>Remove cell</source>
        <translation>Remover célula</translation>
    </message>
    <message>
        <source>Remove this cell from the dashboard?</source>
        <translation>Remover esta célula do painel?</translation>
    </message>
    <message>
        <source>Rename…</source>
        <translation>Renomear…</translation>
    </message>
    <message>
        <source>Rename tab</source>
        <translation>Renomear aba</translation>
    </message>
    <message>
        <source>Title:</source>
        <translation>Título:</translation>
    </message>
    <message>
        <source>Delete tab</source>
        <translation>Excluir aba</translation>
    </message>
    <message>
        <source>Delete the tab &quot;%1&quot; and remove its content from the dashboard?</source>
        <translation>Excluir a aba &quot;%1&quot; e remover seu conteúdo do painel?</translation>
    </message>
    <message>
        <source>untitled</source>
        <translation>sem título</translation>
    </message>
    <message>
        <source>Move left</source>
        <translation>Mover para a esquerda</translation>
    </message>
    <message>
        <source>Move right</source>
        <translation>Mover para a direita</translation>
    </message>
    <message>
        <source>Ungroup</source>
        <translation>Desagrupar</translation>
    </message>
    <message>
        <source>New tab</source>
        <translation>Nova aba</translation>
    </message>
    <message>
        <source>Watch</source>
        <translation>Acompanhar</translation>
    </message>
    <message>
        <source>Count on the tray icon, and notify</source>
        <translation>Contar no ícone da bandeja e notificar</translation>
    </message>
</context>
<context>
    <name>mirante::LayoutView</name>
    <message>
        <source>Add tab</source>
        <translation>Adicionar aba</translation>
    </message>
</context>
<context>
    <name>mirante::LogWindow</name>
    <message>
        <source>Log</source>
        <translation>Log</translation>
    </message>
    <message>
        <source>Debug and above</source>
        <translation>Depuração e acima</translation>
    </message>
    <message>
        <source>Info and above</source>
        <translation>Informação e acima</translation>
    </message>
    <message>
        <source>Warnings and above</source>
        <translation>Avisos e acima</translation>
    </message>
    <message>
        <source>Errors only</source>
        <translation>Somente erros</translation>
    </message>
    <message>
        <source>Filter, such as js or claude.ai</source>
        <translation>Filtro, como js ou claude.ai</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>Limpar</translation>
    </message>
    <message>
        <source>Show:</source>
        <translation>Mostrar:</translation>
    </message>
</context>
<context>
    <name>mirante::LoginDialog</name>
    <message>
        <source>Sign in</source>
        <translation>Entrar</translation>
    </message>
</context>
<context>
    <name>mirante::MainWindow</name>
    <message>
        <source>Editing</source>
        <translation>Editando</translation>
    </message>
    <message>
        <source>Locked</source>
        <translation>Travado</translation>
    </message>
    <message>
        <source>Lock this dashboard (%1)</source>
        <translation>Travar este painel (%1)</translation>
    </message>
    <message>
        <source>Unlock this dashboard for editing (%1)</source>
        <translation>Destravar este painel para edição (%1)</translation>
    </message>
    <message>
        <source>Focus</source>
        <translation>Foco</translation>
    </message>
    <message>
        <source>%1 / %2</source>
        <translation>%1 / %2</translation>
    </message>
    <message>
        <source>Changes to the sample data are not saved. Use File &gt; New Workspace… to keep your own data.</source>
        <translation>As alterações nos dados de exemplo não são salvas. Use Arquivo &gt; Novo espaço de trabalho… para manter seus próprios dados.</translation>
    </message>
    <message>
        <source>Some files could not be loaded or saved:</source>
        <translation>Alguns arquivos não puderam ser carregados ou salvos:</translation>
    </message>
    <message>
        <source>%1: %2</source>
        <translation>%1: %2</translation>
    </message>
    <message>
        <source>Could not use the keychain: %1</source>
        <translation>Não foi possível usar o chaveiro: %1</translation>
    </message>
    <message>
        <source>%1 in the profile %2 refused the token.</source>
        <translation>%1 no perfil %2 recusou o token.</translation>
    </message>
    <message>
        <source>Set Token…</source>
        <translation>Definir token…</translation>
    </message>
    <message>
        <source>New group, project or task</source>
        <translation>Novo grupo, projeto ou tarefa</translation>
    </message>
    <message>
        <source>New &amp;Group…</source>
        <translation>Novo &amp;grupo…</translation>
    </message>
    <message>
        <source>New &amp;Project…</source>
        <translation>Novo &amp;projeto…</translation>
    </message>
    <message>
        <source>New &amp;Task…</source>
        <translation>Nova &amp;tarefa…</translation>
    </message>
    <message>
        <source>New Task from Link…</source>
        <translation>Nova tarefa a partir de link…</translation>
    </message>
    <message>
        <source>Make a task from a link or a key (%1)</source>
        <translation>Criar uma tarefa a partir de um link ou chave (%1)</translation>
    </message>
    <message>
        <source>Sample data</source>
        <translation>Dados de exemplo</translation>
    </message>
    <message>
        <source>%1 - %2</source>
        <translation>%1 - %2</translation>
    </message>
    <message>
        <source>Open Workspace</source>
        <translation>Abrir espaço de trabalho</translation>
    </message>
    <message>
        <source>%1 is not a Mirante workspace: it has no %2. Make it a workspace?</source>
        <translation>%1 não é um espaço de trabalho do Mirante: não tem %2. Torná-lo um espaço de trabalho?</translation>
    </message>
    <message>
        <source>New Workspace</source>
        <translation>Novo espaço de trabalho</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Nome:</translation>
    </message>
    <message>
        <source>Rename Workspace</source>
        <translation>Renomear espaço de trabalho</translation>
    </message>
    <message>
        <source>The sample data cannot be renamed. Open a workspace folder first.</source>
        <translation>Os dados de exemplo não podem ser renomeados. Abra primeiro uma pasta de espaço de trabalho.</translation>
    </message>
    <message>
        <source>Move Workspace</source>
        <translation>Mover espaço de trabalho</translation>
    </message>
    <message>
        <source>The sample data cannot be moved. Open a workspace folder first.</source>
        <translation>Os dados de exemplo não podem ser movidos. Abra primeiro uma pasta de espaço de trabalho.</translation>
    </message>
    <message>
        <source>Move Workspace: choose its new place</source>
        <translation>Mover espaço de trabalho: escolha o novo local</translation>
    </message>
    <message>
        <source>Move the workspace from %1 to %2?</source>
        <translation>Mover o espaço de trabalho de %1 para %2?</translation>
    </message>
    <message>
        <source>&amp;Profiles</source>
        <translation>&amp;Perfis</translation>
    </message>
    <message>
        <source>&amp;Manage Profiles…</source>
        <translation>&amp;Gerenciar perfis…</translation>
    </message>
    <message>
        <source>Set as &amp;Default</source>
        <translation>Definir como &amp;padrão</translation>
    </message>
    <message>
        <source>&amp;Settings…</source>
        <translation>&amp;Configurações…</translation>
    </message>
    <message>
        <source>&amp;Modules</source>
        <translation>&amp;Módulos</translation>
    </message>
    <message>
        <source>No module settings</source>
        <translation>Sem configurações de módulo</translation>
    </message>
    <message>
        <source>&amp;File</source>
        <translation>&amp;Arquivo</translation>
    </message>
    <message>
        <source>&amp;New Workspace…</source>
        <translation>&amp;Novo espaço de trabalho…</translation>
    </message>
    <message>
        <source>New Workspace: choose its folder</source>
        <translation>Novo espaço de trabalho: escolha a pasta</translation>
    </message>
    <message>
        <source>&amp;Open Workspace…</source>
        <translation>&amp;Abrir espaço de trabalho…</translation>
    </message>
    <message>
        <source>Mirante workspace (%1)</source>
        <translation>Espaço de trabalho do Mirante (%1)</translation>
    </message>
    <message>
        <source>Open &amp;Recent</source>
        <translation>Abrir &amp;recentes</translation>
    </message>
    <message>
        <source>No recent workspaces</source>
        <translation>Nenhum espaço de trabalho recente</translation>
    </message>
    <message>
        <source>Clear Recent</source>
        <translation>Limpar recentes</translation>
    </message>
    <message>
        <source>Open &amp;Sample Data</source>
        <translation>Abrir &amp;dados de exemplo</translation>
    </message>
    <message>
        <source>New Task from &amp;Link…</source>
        <translation>Nova tarefa a partir de &amp;link…</translation>
    </message>
    <message>
        <source>&amp;Quit</source>
        <translation>&amp;Sair</translation>
    </message>
    <message>
        <source>&amp;View</source>
        <translation>&amp;Exibir</translation>
    </message>
    <message>
        <source>&amp;Sidebar</source>
        <translation>&amp;Barra lateral</translation>
    </message>
    <message>
        <source>F&amp;ocus Sidebar</source>
        <translation>&amp;Focar barra lateral</translation>
    </message>
    <message>
        <source>&amp;Expand All Tasks</source>
        <translation>&amp;Expandir todas as tarefas</translation>
    </message>
    <message>
        <source>&amp;Collapse All Tasks</source>
        <translation>&amp;Recolher todas as tarefas</translation>
    </message>
    <message>
        <source>&amp;Full Screen</source>
        <translation>Tela &amp;cheia</translation>
    </message>
    <message>
        <source>&amp;Go To…</source>
        <translation>&amp;Ir para…</translation>
    </message>
    <message>
        <source>&amp;All Dashboards…</source>
        <translation>&amp;Todos os painéis…</translation>
    </message>
    <message>
        <source>&amp;New Dashboard</source>
        <translation>&amp;Novo painel</translation>
    </message>
    <message>
        <source>&amp;Tools</source>
        <translation>&amp;Ferramentas</translation>
    </message>
    <message>
        <source>&amp;Help</source>
        <translation>A&amp;juda</translation>
    </message>
    <message>
        <source>&amp;Modules…</source>
        <translation>&amp;Módulos…</translation>
    </message>
    <message>
        <source>&amp;About Mirante</source>
        <translation>&amp;Sobre o Mirante</translation>
    </message>
    <message>
        <source>About &amp;Qt</source>
        <translation>Sobre o &amp;Qt</translation>
    </message>
    <message>
        <source>Show &amp;Log</source>
        <translation>Mostrar &amp;log</translation>
    </message>
    <message>
        <source>Show Mirante</source>
        <translation>Mostrar o Mirante</translation>
    </message>
    <message>
        <source>Settings…</source>
        <translation>Configurações…</translation>
    </message>
    <message>
        <source>Quit</source>
        <translation>Sair</translation>
    </message>
    <message>
        <source>Hide this warning</source>
        <translation>Ocultar este aviso</translation>
    </message>
    <message>
        <source>New root dashboard</source>
        <translation>Novo painel raiz</translation>
    </message>
    <message>
        <source>Show sidebar</source>
        <translation>Mostrar barra lateral</translation>
    </message>
    <message>
        <source>All dashboards</source>
        <translation>Todos os painéis</translation>
    </message>
    <message>
        <source>All root dashboards, including closed ones</source>
        <translation>Todos os painéis raiz, incluindo os fechados</translation>
    </message>
    <message>
        <source>The workspace folder does not exist.</source>
        <translation>A pasta do espaço de trabalho não existe.</translation>
    </message>
    <message>
        <source>not a Mirante workspace: it has no %1</source>
        <translation>não é um espaço de trabalho do Mirante: não tem %1</translation>
    </message>
    <message numerus="yes">
        <source>not opened: a newer version of Mirante wrote %n of its files, such as %1. Update Mirante to open it.</source>
        <translation>
            <numerusform>não aberto: uma versão mais nova do Mirante gravou um de seus arquivos, %1. Atualize o Mirante para abri-lo.</numerusform>
            <numerusform>não aberto: uma versão mais nova do Mirante gravou %n de seus arquivos, como %1. Atualize o Mirante para abri-lo.</numerusform>
        </translation>
    </message>
    <message>
        <source>cannot be made a workspace: %1</source>
        <translation>não pode ser transformado em espaço de trabalho: %1</translation>
    </message>
    <message>
        <source>no name</source>
        <translation>sem nome</translation>
    </message>
    <message>
        <source>Main</source>
        <translation>Principal</translation>
    </message>
    <message>
        <source>cannot be moved: %1</source>
        <translation>não pode ser movido: %1</translation>
    </message>
    <message>
        <source>the workspace was moved, but this copy of its old folder could not be removed entirely; delete it when no program uses it</source>
        <translation>o espaço de trabalho foi movido, mas esta cópia da pasta antiga não pôde ser removida por completo; exclua-a quando nenhum programa a estiver usando</translation>
    </message>
    <message>
        <source>About %1</source>
        <translation>Sobre o %1</translation>
    </message>
    <message>
        <source>&lt;h3&gt;%1 %2&lt;/h3&gt;&lt;p&gt;%3&lt;/p&gt;&lt;p&gt;%4 &amp;lt;&lt;a href=&quot;mailto:%5&quot;&gt;%5&lt;/a&gt;&amp;gt;&lt;/p&gt;&lt;p&gt;&lt;a href=&quot;%6&quot;&gt;%6&lt;/a&gt;&lt;/p&gt;&lt;p&gt;Released under the MIT License.&lt;/p&gt;</source>
        <translation>&lt;h3&gt;%1 %2&lt;/h3&gt;&lt;p&gt;%3&lt;/p&gt;&lt;p&gt;%4 &amp;lt;&lt;a href=&quot;mailto:%5&quot;&gt;%5&lt;/a&gt;&amp;gt;&lt;/p&gt;&lt;p&gt;&lt;a href=&quot;%6&quot;&gt;%6&lt;/a&gt;&lt;/p&gt;&lt;p&gt;Distribuído sob a Licença MIT.&lt;/p&gt;</translation>
    </message>
    <message>
        <source>&lt;p&gt;Qt %1, Qt WebEngine %2, Chromium %3&lt;/p&gt;</source>
        <translation>&lt;p&gt;Qt %1, Qt WebEngine %2, Chromium %3&lt;/p&gt;</translation>
    </message>
    <message>
        <source>New dashboard</source>
        <translation>Novo painel</translation>
    </message>
    <message numerus="yes">
        <source>%1: %n task(s) need attention</source>
        <translation>
            <numerusform>%1: %n tarefa precisa de atenção</numerusform>
            <numerusform>%1: %n tarefas precisam de atenção</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n watched item(s) need attention</source>
        <translation>
            <numerusform>%n item acompanhado precisa de atenção</numerusform>
            <numerusform>%n itens acompanhados precisam de atenção</numerusform>
        </translation>
    </message>
    <message>
        <source>&amp;Workspace</source>
        <translation>&amp;Espaço de trabalho</translation>
    </message>
    <message>
        <source>Re&amp;name…</source>
        <translation>Re&amp;nomear…</translation>
    </message>
    <message>
        <source>&amp;Move…</source>
        <translation>&amp;Mover…</translation>
    </message>
    <message>
        <source>Show &amp;Folder</source>
        <translation>Mostrar &amp;pasta</translation>
    </message>
    <message>
        <source>Re&amp;load</source>
        <translation>Re&amp;carregar</translation>
    </message>
</context>
<context>
    <name>mirante::ModuleSettingsDialog</name>
    <message>
        <source>%1: %2 %3</source>
        <translation>%1: %2 %3</translation>
    </message>
    <message>
        <source>These settings belong to the profile &quot;%1&quot;. Items that use it read them. They are saved in the workspace and shared with it; tokens stay on this device.</source>
        <translation>Estas configurações pertencem ao perfil &quot;%1&quot;. Os itens que o usam as leem. Elas são salvas no espaço de trabalho e compartilhadas com ele; os tokens ficam neste dispositivo.</translation>
    </message>
</context>
<context>
    <name>mirante::ModulesDialog</name>
    <message>
        <source>No settings.</source>
        <translation>Sem configurações.</translation>
    </message>
    <message>
        <source>Settings:</source>
        <translation>Configurações:</translation>
    </message>
    <message>
        <source>Each profile has its own. Edit them in Profiles &amp;gt; &lt;i&gt;profile&lt;/i&gt; &amp;gt; Modules &amp;gt; %1.</source>
        <translation>Cada perfil tem as suas. Edite-as em Perfis &amp;gt; &lt;i&gt;perfil&lt;/i&gt; &amp;gt; Módulos &amp;gt; %1.</translation>
    </message>
    <message>
        <source>Item types</source>
        <translation>Tipos de item</translation>
    </message>
    <message>
        <source>Module:</source>
        <translation>Módulo:</translation>
    </message>
    <message>
        <source>Modules</source>
        <translation>Módulos</translation>
    </message>
    <message>
        <source>The modules built into this copy of Mirante, and the item types each one adds.</source>
        <translation>Os módulos incluídos nesta cópia do Mirante e os tipos de item que cada um adiciona.</translation>
    </message>
    <message>
        <source>This build has no modules.</source>
        <translation>Esta versão não tem módulos.</translation>
    </message>
    <message>
        <source>Version:</source>
        <translation>Versão:</translation>
    </message>
    <message>
        <source>Author:</source>
        <translation>Autor:</translation>
    </message>
    <message>
        <source>Homepage:</source>
        <translation>Página:</translation>
    </message>
    <message>
        <source>UUID:</source>
        <translation>UUID:</translation>
    </message>
</context>
<context>
    <name>mirante::PageArea</name>
    <message>
        <source>%1 wants to show notifications.</source>
        <translation>%1 quer mostrar notificações.</translation>
    </message>
    <message>
        <source>%1 wants to read and write the clipboard.</source>
        <translation>%1 quer ler e escrever na área de transferência.</translation>
    </message>
    <message>
        <source>Allow</source>
        <translation>Permitir</translation>
    </message>
    <message>
        <source>Deny</source>
        <translation>Negar</translation>
    </message>
    <message>
        <source>Reload</source>
        <translation>Recarregar</translation>
    </message>
    <message>
        <source>Error %1</source>
        <translation>Erro %1</translation>
    </message>
    <message>
        <source>Could not load %1
%2</source>
        <translation>Não foi possível carregar %1
%2</translation>
    </message>
    <message>
        <source>The page %1 stopped.</source>
        <translation>A página %1 parou.</translation>
    </message>
</context>
<context>
    <name>mirante::Placement</name>
    <message>
        <source>just now</source>
        <translation>agora mesmo</translation>
    </message>
    <message>
        <source>%1 min ago</source>
        <translation>há %1 min</translation>
    </message>
    <message>
        <source>%1 h ago</source>
        <translation>há %1 h</translation>
    </message>
    <message numerus="yes">
        <source>%n day(s) ago</source>
        <translation>
            <numerusform>há %n dia</numerusform>
            <numerusform>há %n dias</numerusform>
        </translation>
    </message>
    <message>
        <source>Back to summary</source>
        <translation>Voltar ao resumo</translation>
    </message>
    <message>
        <source>Removed</source>
        <translation>Removido</translation>
    </message>
    <message>
        <source>%1
Profile: %2</source>
        <translation>%1
Perfil: %2</translation>
    </message>
    <message>
        <source>Token not set</source>
        <translation>Token não definido</translation>
    </message>
    <message>
        <source>Token refused</source>
        <translation>Token recusado</translation>
    </message>
    <message>
        <source>No server set</source>
        <translation>Nenhum servidor definido</translation>
    </message>
    <message>
        <source>Not found</source>
        <translation>Não encontrado</translation>
    </message>
    <message>
        <source>%1
The data is from %2.</source>
        <translation>%1
Os dados são de %2.</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>Erro</translation>
    </message>
    <message>
        <source>Clearing this profile&apos;s data…</source>
        <translation>Limpando os dados deste perfil…</translation>
    </message>
    <message>
        <source>This page was unloaded to save memory.</source>
        <translation>Esta página foi descarregada para economizar memória.</translation>
    </message>
    <message>
        <source>Reload</source>
        <translation>Recarregar</translation>
    </message>
</context>
<context>
    <name>mirante::ProfileManager</name>
    <message>
        <source>Save download</source>
        <translation>Salvar download</translation>
    </message>
</context>
<context>
    <name>mirante::ProfileSettingsDialog</name>
    <message>
        <source>Open</source>
        <translation>Abrir</translation>
    </message>
    <message>
        <source>Show the folder in the file manager</source>
        <translation>Mostrar a pasta no gerenciador de arquivos</translation>
    </message>
    <message>
        <source>Made when a page of this profile first loads</source>
        <translation>Criada quando uma página deste perfil carrega pela primeira vez</translation>
    </message>
    <message>
        <source>Profile Settings: %1</source>
        <translation>Configurações do perfil: %1</translation>
    </message>
    <message>
        <source>Storage</source>
        <translation>Armazenamento</translation>
    </message>
    <message>
        <source>Kept on this device only, for this workspace. Other devices keep their own.</source>
        <translation>Mantido apenas neste dispositivo, para este espaço de trabalho. Outros dispositivos mantêm os seus.</translation>
    </message>
    <message>
        <source>Web data</source>
        <translation>Dados da web</translation>
    </message>
    <message>
        <source>Cache</source>
        <translation>Cache</translation>
    </message>
    <message>
        <source>Tokens are in the system keychain, in entries whose names start with this.</source>
        <translation>Os tokens ficam no chaveiro do sistema, em entradas cujos nomes começam com isto.</translation>
    </message>
    <message>
        <source>Tokens</source>
        <translation>Tokens</translation>
    </message>
</context>
<context>
    <name>mirante::ProfilesDialog</name>
    <message>
        <source>Enter a name.</source>
        <translation>Digite um nome.</translation>
    </message>
    <message>
        <source>A profile called &quot;%1&quot; already exists.</source>
        <translation>Já existe um perfil chamado &quot;%1&quot;.</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Nome:</translation>
    </message>
    <message>
        <source>Delete profile</source>
        <translation>Excluir perfil</translation>
    </message>
    <message>
        <source>Clear profile data</source>
        <translation>Limpar dados do perfil</translation>
    </message>
    <message>
        <source>Clear all web data of the profile &quot;%1&quot;?</source>
        <translation>Limpar todos os dados da web do perfil &quot;%1&quot;?</translation>
    </message>
    <message>
        <source>Its logins, cookies, cache and site data on this device are deleted, so every site in it signs out. Pages in this profile reload. Its module settings stay. This cannot be undone.</source>
        <translation>Os logins, cookies, cache e dados de sites dele neste dispositivo são excluídos, então todos os sites nele saem da conta. As páginas deste perfil são recarregadas. As configurações de módulo dele são mantidas. Isto não pode ser desfeito.</translation>
    </message>
    <message>
        <source>Clear Data</source>
        <translation>Limpar dados</translation>
    </message>
    <message>
        <source>Profiles</source>
        <translation>Perfis</translation>
    </message>
    <message>
        <source>A profile holds web logins and module settings, such as the GitLab or Jira server and its token. Each group chooses a profile, and each item can choose its own. Those that choose none use the default profile. The list and the settings are shared with the workspace; logins and tokens stay on this device. A profile&apos;s module settings are in the Profiles menu.</source>
        <translation>Um perfil guarda logins na web e configurações de módulo, como o servidor do GitLab ou do Jira e seu token. Cada grupo escolhe um perfil, e cada item pode escolher o seu. Os que não escolhem nenhum usam o perfil padrão. A lista e as configurações são compartilhadas com o espaço de trabalho; logins e tokens ficam neste dispositivo. As configurações de módulo de um perfil ficam no menu Perfis.</translation>
    </message>
    <message>
        <source>Items in the same profile share its logins, servers and tokens. Every item that uses a profile can read the logins and data of all sites in that profile, and call its servers with its tokens. Use separate profiles for things you do not trust together.</source>
        <translation>Itens no mesmo perfil compartilham seus logins, servidores e tokens. Todo item que usa um perfil pode ler os logins e os dados de todos os sites desse perfil e chamar seus servidores com seus tokens. Use perfis separados para coisas em que você não confia juntas.</translation>
    </message>
    <message>
        <source>New profile…</source>
        <translation>Novo perfil…</translation>
    </message>
    <message>
        <source>Rename…</source>
        <translation>Renomear…</translation>
    </message>
    <message>
        <source>Delete…</source>
        <translation>Excluir…</translation>
    </message>
    <message>
        <source>Set as Default</source>
        <translation>Definir como padrão</translation>
    </message>
    <message>
        <source>Use this profile for groups and items that choose none</source>
        <translation>Usar este perfil para grupos e itens que não escolhem nenhum</translation>
    </message>
    <message>
        <source>Clear Data…</source>
        <translation>Limpar dados…</translation>
    </message>
    <message>
        <source>Sign out of every site in this profile, on this device</source>
        <translation>Sair de todos os sites deste perfil, neste dispositivo</translation>
    </message>
    <message>
        <source>New profile</source>
        <translation>Novo perfil</translation>
    </message>
    <message>
        <source>Rename profile</source>
        <translation>Renomear perfil</translation>
    </message>
    <message>
        <source>Delete the profile &quot;%1&quot;? Its module settings are deleted, and its logins, site data and tokens on this device.</source>
        <translation>Excluir o perfil &quot;%1&quot;? As configurações de módulo dele são excluídas, assim como seus logins, dados de sites e tokens neste dispositivo.</translation>
    </message>
    <message>
        <source>%1 and %2</source>
        <translation>%1 e %2</translation>
    </message>
    <message>
        <source>1 group</source>
        <translation>1 grupo</translation>
    </message>
    <message>
        <source>%1 groups</source>
        <translation>%1 grupos</translation>
    </message>
    <message>
        <source>1 item</source>
        <translation>1 item</translation>
    </message>
    <message>
        <source>%1 items</source>
        <translation>%1 itens</translation>
    </message>
    <message>
        <source>It is used by %1. A group then uses the default profile, and an item its group&apos;s profile.</source>
        <translation>Ele é usado por %1. Um grupo passa a usar o perfil padrão, e um item o perfil do seu grupo.</translation>
    </message>
    <message>
        <source>It is the default profile; &quot;%1&quot; becomes the default.</source>
        <translation>Ele é o perfil padrão; &quot;%1&quot; passa a ser o padrão.</translation>
    </message>
</context>
<context>
    <name>mirante::ProjectDialog</name>
    <message>
        <source>Folder not set on this device</source>
        <translation>Pasta não definida neste dispositivo</translation>
    </message>
    <message>
        <source>Choose…</source>
        <translation>Escolher…</translation>
    </message>
    <message>
        <source>Choose the folder &quot;%1&quot;</source>
        <translation>Escolha a pasta &quot;%1&quot;</translation>
    </message>
    <message>
        <source>New project</source>
        <translation>Novo projeto</translation>
    </message>
    <message>
        <source>Edit project</source>
        <translation>Editar projeto</translation>
    </message>
    <message>
        <source>(unknown group)</source>
        <translation>(grupo desconhecido)</translation>
    </message>
    <message>
        <source>Urgent</source>
        <translation>Urgente</translation>
    </message>
    <message>
        <source>High</source>
        <translation>Alta</translation>
    </message>
    <message>
        <source>Normal</source>
        <translation>Normal</translation>
    </message>
    <message>
        <source>Low</source>
        <translation>Baixa</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Nome</translation>
    </message>
    <message>
        <source>Remote URL</source>
        <translation>URL remota</translation>
    </message>
    <message>
        <source>Main</source>
        <translation>Principal</translation>
    </message>
    <message>
        <source>Path on this device</source>
        <translation>Caminho neste dispositivo</translation>
    </message>
    <message>
        <source>Add folder</source>
        <translation>Adicionar pasta</translation>
    </message>
    <message>
        <source>Remove folder</source>
        <translation>Remover pasta</translation>
    </message>
    <message>
        <source>Name:</source>
        <translation>Nome:</translation>
    </message>
    <message>
        <source>Group:</source>
        <translation>Grupo:</translation>
    </message>
    <message>
        <source>Priority:</source>
        <translation>Prioridade:</translation>
    </message>
    <message>
        <source>Folders:</source>
        <translation>Pastas:</translation>
    </message>
    <message>
        <source>Add settings</source>
        <translation>Adicionar configurações</translation>
    </message>
    <message>
        <source>Enter a name.</source>
        <translation>Digite um nome.</translation>
    </message>
    <message>
        <source>Enter a name for every folder.</source>
        <translation>Digite um nome para cada pasta.</translation>
    </message>
    <message>
        <source>%1: %2</source>
        <translation>%1: %2</translation>
    </message>
</context>
<context>
    <name>mirante::QuickAddDialog</name>
    <message>
        <source>Reading…</source>
        <translation>Lendo…</translation>
    </message>
    <message>
        <source>Mirante does not recognise this text.</source>
        <translation>O Mirante não reconhece este texto.</translation>
    </message>
    <message>
        <source>Looking for related items…</source>
        <translation>Procurando itens relacionados…</translation>
    </message>
    <message>
        <source>Several projects match</source>
        <translation>Vários projetos correspondem</translation>
    </message>
    <message>
        <source>No project matches %1</source>
        <translation>Nenhum projeto corresponde a %1</translation>
    </message>
    <message>
        <source>Related</source>
        <translation>Relacionados</translation>
    </message>
    <message>
        <source>Add Items from Link: %1</source>
        <translation>Adicionar itens do link: %1</translation>
    </message>
    <message>
        <source>New Task from Link</source>
        <translation>Nova tarefa a partir de link</translation>
    </message>
    <message>
        <source>Paste a link or a key</source>
        <translation>Cole um link ou uma chave</translation>
    </message>
    <message>
        <source>Choose a project</source>
        <translation>Escolha um projeto</translation>
    </message>
    <message>
        <source>Create a project first.</source>
        <translation>Crie um projeto primeiro.</translation>
    </message>
    <message>
        <source>Text</source>
        <translation>Texto</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>Perfil</translation>
    </message>
    <message>
        <source>Read as</source>
        <translation>Ler como</translation>
    </message>
    <message>
        <source>Title</source>
        <translation>Título</translation>
    </message>
    <message>
        <source>Project</source>
        <translation>Projeto</translation>
    </message>
    <message>
        <source>Items</source>
        <translation>Itens</translation>
    </message>
    <message>
        <source>Add Items</source>
        <translation>Adicionar itens</translation>
    </message>
    <message>
        <source>Create Task</source>
        <translation>Criar tarefa</translation>
    </message>
</context>
<context>
    <name>mirante::QuickSwitcher</name>
    <message>
        <source>Group</source>
        <translation>Grupo</translation>
    </message>
    <message>
        <source>Project</source>
        <translation>Projeto</translation>
    </message>
    <message>
        <source>Task</source>
        <translation>Tarefa</translation>
    </message>
    <message>
        <source>Item</source>
        <translation>Item</translation>
    </message>
    <message>
        <source>Dashboard</source>
        <translation>Painel</translation>
    </message>
    <message>
        <source>Go to a group, project, task, item or dashboard…</source>
        <translation>Ir para um grupo, projeto, tarefa, item ou painel…</translation>
    </message>
</context>
<context>
    <name>mirante::RootDashboardsDialog</name>
    <message>
        <source>%1 (closed)</source>
        <translation>%1 (fechado)</translation>
    </message>
    <message>
        <source>Delete dashboard</source>
        <translation>Excluir painel</translation>
    </message>
    <message>
        <source>Delete the root dashboard &quot;%1&quot;? The tasks and items it shows are kept.</source>
        <translation>Excluir o painel raiz &quot;%1&quot;? As tarefas e os itens que ele mostra são mantidos.</translation>
    </message>
    <message>
        <source>Root dashboards</source>
        <translation>Painéis raiz</translation>
    </message>
    <message>
        <source>Open</source>
        <translation>Abrir</translation>
    </message>
    <message>
        <source>Delete…</source>
        <translation>Excluir…</translation>
    </message>
</context>
<context>
    <name>mirante::RootTabBar</name>
    <message>
        <source>New dashboard</source>
        <translation>Novo painel</translation>
    </message>
    <message>
        <source>Close tab</source>
        <translation>Fechar aba</translation>
    </message>
    <message>
        <source>Rename dashboard…</source>
        <translation>Renomear painel…</translation>
    </message>
    <message>
        <source>Detach into Window</source>
        <translation>Destacar em janela</translation>
    </message>
    <message>
        <source>All dashboards…</source>
        <translation>Todos os painéis…</translation>
    </message>
</context>
<context>
    <name>mirante::ServerClients</name>
    <message>
        <source>No server set</source>
        <translation>Nenhum servidor definido</translation>
    </message>
    <message>
        <source>Token refused</source>
        <translation>Token recusado</translation>
    </message>
    <message>
        <source>Token not set on this device</source>
        <translation>Token não definido neste dispositivo</translation>
    </message>
    <message>
        <source>The server did not answer in time</source>
        <translation>O servidor não respondeu a tempo</translation>
    </message>
    <message>
        <source>Too many requests (429)</source>
        <translation>Requisições demais (429)</translation>
    </message>
    <message>
        <source>Token refused (%1)</source>
        <translation>Token recusado (%1)</translation>
    </message>
    <message>
        <source>Not allowed (403)</source>
        <translation>Não permitido (403)</translation>
    </message>
    <message>
        <source>Not found (404)</source>
        <translation>Não encontrado (404)</translation>
    </message>
    <message>
        <source>Server error (%1)</source>
        <translation>Erro do servidor (%1)</translation>
    </message>
    <message>
        <source>Rate limited (403)</source>
        <translation>Limite de requisições atingido (403)</translation>
    </message>
</context>
<context>
    <name>mirante::SettingsDialog</name>
    <message>
        <source>Settings</source>
        <translation>Configurações</translation>
    </message>
    <message>
        <source>These settings apply to this device only. They are not saved in the workspace.</source>
        <translation>Estas configurações valem apenas para este dispositivo. Elas não são salvas no espaço de trabalho.</translation>
    </message>
    <message>
        <source>Sidebar</source>
        <translation>Barra lateral</translation>
    </message>
    <message>
        <source>Highlight the sidebar row of the cell under the mouse</source>
        <translation>Destacar a linha da barra lateral da célula sob o mouse</translation>
    </message>
    <message>
        <source>While the mouse is over a dashboard cell, the sidebar row of the task or item it shows gets a soft tint.</source>
        <translation>Enquanto o mouse está sobre uma célula do painel, a linha da barra lateral da tarefa ou do item que ela mostra recebe um tom suave.</translation>
    </message>
    <message>
        <source>Status</source>
        <translation>Status</translation>
    </message>
    <message>
        <source> min</source>
        <translation> min</translation>
    </message>
    <message>
        <source>How often Mirante asks servers, such as GitLab and Jira, for the status of items.</source>
        <translation>Com que frequência o Mirante consulta os servidores, como GitLab e Jira, sobre o status dos itens.</translation>
    </message>
    <message>
        <source>Check servers every</source>
        <translation>Verificar servidores a cada</translation>
    </message>
    <message>
        <source>Window</source>
        <translation>Janela</translation>
    </message>
    <message>
        <source>Shows or hides Mirante from any application.</source>
        <translation>Mostra ou oculta o Mirante a partir de qualquer aplicativo.</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>Limpar</translation>
    </message>
    <message>
        <source>Global hotkey</source>
        <translation>Atalho global</translation>
    </message>
    <message>
        <source>Closing the window keeps Mirante running in the tray</source>
        <translation>Fechar a janela mantém o Mirante em execução na bandeja</translation>
    </message>
    <message>
        <source>Terminal</source>
        <translation>Terminal</translation>
    </message>
    <message>
        <source>The system&apos;s terminal, or such as: wezterm start --cwd {dir} -- {command}</source>
        <translation>O terminal do sistema, ou algo como: wezterm start --cwd {dir} -- {command}</translation>
    </message>
    <message>
        <source>The command that opens a terminal. {dir} is the folder to start in. {command} is the program to run, with its arguments. Empty uses the system&apos;s terminal.</source>
        <translation>O comando que abre um terminal. {dir} é a pasta onde começar. {command} é o programa a executar, com seus argumentos. Vazio usa o terminal do sistema.</translation>
    </message>
    <message>
        <source>Terminal command</source>
        <translation>Comando do terminal</translation>
    </message>
    <message>
        <source>Modules</source>
        <translation>Módulos</translation>
    </message>
    <message>
        <source>General</source>
        <translation>Geral</translation>
    </message>
    <message>
        <source>System</source>
        <translation>Sistema</translation>
    </message>
    <message>
        <source>System uses Brazilian Portuguese when the system&apos;s language is Portuguese, and English otherwise.</source>
        <translation>&quot;Sistema&quot; usa o português do Brasil quando o idioma do sistema é português, e o inglês nos demais casos.</translation>
    </message>
    <message>
        <source>Applies at the next start.</source>
        <translation>Vale a partir da próxima inicialização.</translation>
    </message>
    <message>
        <source>Language</source>
        <translation>Idioma</translation>
    </message>
</context>
<context>
    <name>mirante::Sidebar</name>
    <message>
        <source>The workspace. Double-click to rename it.</source>
        <translation>O espaço de trabalho. Clique duas vezes para renomeá-lo.</translation>
    </message>
    <message>
        <source>Hide sidebar</source>
        <translation>Ocultar barra lateral</translation>
    </message>
    <message>
        <source>Expand all tasks</source>
        <translation>Expandir todas as tarefas</translation>
    </message>
    <message>
        <source>Collapse all tasks</source>
        <translation>Recolher todas as tarefas</translation>
    </message>
    <message>
        <source>Priorities shown</source>
        <translation>Prioridades exibidas</translation>
    </message>
    <message>
        <source>Choose which priorities the tree shows</source>
        <translation>Escolha quais prioridades a árvore exibe</translation>
    </message>
    <message>
        <source>Urgent</source>
        <translation>Urgente</translation>
    </message>
    <message>
        <source>High</source>
        <translation>Alta</translation>
    </message>
    <message>
        <source>Normal</source>
        <translation>Normal</translation>
    </message>
    <message>
        <source>Low</source>
        <translation>Baixa</translation>
    </message>
    <message>
        <source>Projects</source>
        <translation>Projetos</translation>
    </message>
    <message>
        <source>Active</source>
        <translation>Ativas</translation>
    </message>
    <message>
        <source>Done</source>
        <translation>Concluídas</translation>
    </message>
    <message>
        <source>Trash</source>
        <translation>Lixeira</translation>
    </message>
    <message>
        <source>Active tasks, under every project, including projects without any</source>
        <translation>Tarefas ativas, em todos os projetos, inclusive nos que não têm nenhuma</translation>
    </message>
    <message>
        <source>Active tasks, only under projects that have some</source>
        <translation>Tarefas ativas, apenas nos projetos que têm alguma</translation>
    </message>
    <message>
        <source>Done and archived tasks</source>
        <translation>Tarefas concluídas e arquivadas</translation>
    </message>
    <message>
        <source>Deleted projects and tasks, which can be restored</source>
        <translation>Projetos e tarefas excluídos, que podem ser restaurados</translation>
    </message>
    <message>
        <source>Empty Trash…</source>
        <translation>Esvaziar lixeira…</translation>
    </message>
    <message>
        <source>Delete everything in the trash permanently</source>
        <translation>Excluir permanentemente tudo o que está na lixeira</translation>
    </message>
    <message>
        <source>Open in new root dashboard</source>
        <translation>Abrir em novo painel raiz</translation>
    </message>
</context>
<context>
    <name>mirante::SidebarModel</name>
    <message>
        <source>Urgent</source>
        <translation>Urgente</translation>
    </message>
    <message>
        <source>High</source>
        <translation>Alta</translation>
    </message>
    <message>
        <source>Normal</source>
        <translation>Normal</translation>
    </message>
    <message>
        <source>Low</source>
        <translation>Baixa</translation>
    </message>
    <message>
        <source>Items that need attention: %1</source>
        <translation>Itens que precisam de atenção: %1</translation>
    </message>
    <message>
        <source>Priority: %1</source>
        <translation>Prioridade: %1</translation>
    </message>
    <message>
        <source>Items</source>
        <translation>Itens</translation>
    </message>
    <message>
        <source>Drag an item onto a dashboard to show it there</source>
        <translation>Arraste um item para um painel para exibi-lo lá</translation>
    </message>
    <message>
        <source>Dashboards</source>
        <translation>Painéis</translation>
    </message>
    <message>
        <source>Drag a dashboard onto a root dashboard to show the task there</source>
        <translation>Arraste um painel para um painel raiz para exibir a tarefa lá</translation>
    </message>
    <message>
        <source>Task dashboard, opened by default</source>
        <translation>Painel da tarefa, aberto por padrão</translation>
    </message>
    <message>
        <source>Task dashboard</source>
        <translation>Painel da tarefa</translation>
    </message>
</context>
<context>
    <name>mirante::SlotTabBar</name>
    <message>
        <source>Focus</source>
        <translation>Foco</translation>
    </message>
    <message>
        <source>Focus: what to work on now</source>
        <translation>Foco: no que trabalhar agora</translation>
    </message>
    <message>
        <source>Close preview</source>
        <translation>Fechar prévia</translation>
    </message>
</context>
<context>
    <name>mirante::StatusCache</name>
    <message>
        <source>No server is set for this module.</source>
        <translation>Nenhum servidor está definido para este módulo.</translation>
    </message>
    <message>
        <source>The server did not answer in %1 seconds.</source>
        <translation>O servidor não respondeu em %1 segundos.</translation>
    </message>
</context>
<context>
    <name>mirante::TaskCell</name>
    <message>
        <source>In trash</source>
        <translation>Na lixeira</translation>
    </message>
    <message>
        <source>Removed</source>
        <translation>Removida</translation>
    </message>
    <message>
        <source>Removed task</source>
        <translation>Tarefa removida</translation>
    </message>
    <message>
        <source>Restore</source>
        <translation>Restaurar</translation>
    </message>
    <message>
        <source>Maximise</source>
        <translation>Maximizar</translation>
    </message>
    <message>
        <source>Open Alone</source>
        <translation>Abrir sozinho</translation>
    </message>
    <message>
        <source>Edit task dashboard</source>
        <translation>Editar painel da tarefa</translation>
    </message>
    <message>
        <source>Open this task dashboard on its own to edit it (or double-click the cell)</source>
        <translation>Abrir este painel da tarefa sozinho para editá-lo (ou clique duas vezes na célula)</translation>
    </message>
</context>
<context>
    <name>mirante::TaskDialog</name>
    <message>
        <source>New task</source>
        <translation>Nova tarefa</translation>
    </message>
    <message>
        <source>Edit task</source>
        <translation>Editar tarefa</translation>
    </message>
    <message>
        <source>(unknown project)</source>
        <translation>(projeto desconhecido)</translation>
    </message>
    <message>
        <source>Urgent</source>
        <translation>Urgente</translation>
    </message>
    <message>
        <source>High</source>
        <translation>Alta</translation>
    </message>
    <message>
        <source>Normal</source>
        <translation>Normal</translation>
    </message>
    <message>
        <source>Low</source>
        <translation>Baixa</translation>
    </message>
    <message>
        <source>Active</source>
        <translation>Ativa</translation>
    </message>
    <message>
        <source>Done</source>
        <translation>Concluída</translation>
    </message>
    <message>
        <source>Archived</source>
        <translation>Arquivada</translation>
    </message>
    <message>
        <source>Title:</source>
        <translation>Título:</translation>
    </message>
    <message>
        <source>Project:</source>
        <translation>Projeto:</translation>
    </message>
    <message>
        <source>Priority:</source>
        <translation>Prioridade:</translation>
    </message>
    <message>
        <source>State:</source>
        <translation>Estado:</translation>
    </message>
    <message>
        <source>Enter a title.</source>
        <translation>Informe um título.</translation>
    </message>
</context>
<context>
    <name>mirante::WebPanel</name>
    <message>
        <source>Back</source>
        <translation>Voltar</translation>
    </message>
    <message>
        <source>Reload</source>
        <translation>Recarregar</translation>
    </message>
    <message>
        <source>Open in browser</source>
        <translation>Abrir no navegador</translation>
    </message>
    <message>
        <source>Open this page in the external browser</source>
        <translation>Abrir esta página no navegador externo</translation>
    </message>
</context>
<context>
    <name>mirante::platform::GlobalHotkey</name>
    <message>
        <source>this desktop does not let applications register one. Bind a desktop shortcut to Mirante instead.</source>
        <translation>este ambiente de trabalho não permite que aplicativos registrem um. Associe um atalho do ambiente de trabalho ao Mirante.</translation>
    </message>
    <message>
        <source>another application may already use it.</source>
        <translation>outro aplicativo pode já estar usando-o.</translation>
    </message>
    <message>
        <source>The global hotkey %1 could not be registered: %2</source>
        <translation>O atalho global %1 não pôde ser registrado: %2</translation>
    </message>
</context>
<context>
    <name>mirante::widgets::ActionRow</name>
    <message>
        <source>Web</source>
        <translation>Web</translation>
    </message>
</context>
<context>
    <name>mirante::widgets::CommentDialog</name>
    <message>
        <source>Send</source>
        <translation>Enviar</translation>
    </message>
</context>
<context>
    <name>mirante::widgets::ItemListView</name>
    <message>
        <source>%1-%2 of %3</source>
        <translation>%1-%2 de %3</translation>
    </message>
    <message>
        <source>All</source>
        <translation>Todos</translation>
    </message>
    <message>
        <source>Previous</source>
        <translation>Anterior</translation>
    </message>
    <message>
        <source>Next</source>
        <translation>Próxima</translation>
    </message>
</context>
<context>
    <name>notes</name>
    <message>
        <source>Notes</source>
        <translation>Notas</translation>
    </message>
    <message>
        <source>A Markdown note with a preview. Its text is kept in the task&apos;s file.</source>
        <translation>Uma nota em Markdown com prévia. O texto fica guardado no arquivo da tarefa.</translation>
    </message>
    <message>
        <source>Markdown notes, kept in the task&apos;s file, so they sync with the workspace.</source>
        <translation>Notas em Markdown, guardadas no arquivo da tarefa, para que sincronizem com o espaço de trabalho.</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>Editar</translation>
    </message>
    <message>
        <source>Preview</source>
        <translation>Prévia</translation>
    </message>
    <message>
        <source>Switch between Edit and Preview (%1)</source>
        <translation>Alternar entre Editar e Prévia (%1)</translation>
    </message>
</context>
<context>
    <name>web</name>
    <message>
        <source>Web</source>
        <translation>Web</translation>
    </message>
    <message>
        <source>The web page at an address, signed in with the item&apos;s profile.</source>
        <translation>A página web em um endereço, com login feito pelo perfil do item.</translation>
    </message>
    <message>
        <source>Address</source>
        <translation>Endereço</translation>
    </message>
    <message>
        <source>Set the address in the item&apos;s settings.</source>
        <translation>Defina o endereço nas configurações do item.</translation>
    </message>
    <message>
        <source>Any web page as an item.</source>
        <translation>Qualquer página web como item.</translation>
    </message>
    <message>
        <source>Web page</source>
        <translation>Página web</translation>
    </message>
</context>
</TS>
