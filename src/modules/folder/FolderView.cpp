#include "FolderView.h"

#include <mirante/GitState.h>
#include <mirante/ItemContext.h>
#include <mirante/widgets/ActionRow.h>
#include <mirante/widgets/SummaryView.h>

#include <QCoreApplication>
#include <QDir>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <optional>

namespace mirante::modules::folder {

namespace {

// How often the Git state is read while the view shows.
constexpr int kRefreshMs = 10 * 1000;

const QString kTerminal = QStringLiteral("terminal");
const QString kEditor = QStringLiteral("editor");
const QString kFileManager = QStringLiteral("fileManager");
const QString kChooseFolder = QStringLiteral("chooseFolder");

// A label for a path or a branch name, which wraps and is never read as
// markup.
QLabel *plainLabel(const QString &objectName, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(objectName);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

bool sameState(const GitState &a, const GitState &b)
{
    return a.result == b.result && a.branch == b.branch && a.commit == b.commit && a.hasUpstream == b.hasUpstream
           && a.ahead == b.ahead && a.behind == b.behind && a.staged == b.staged && a.changed == b.changed
           && a.untracked == b.untracked && a.conflicts == b.conflicts && a.message == b.message;
}

// "2 ahead · 3 changed · 1 untracked", without the counts that are zero, or
// "Clean".
QString statusLine(const GitState &state)
{
    QStringList parts;
    const auto add = [&parts](int count, const QString &text) {
        if (count > 0)
            parts << text.arg(count);
    };
    add(state.ahead, QCoreApplication::translate("folder", "%1 ahead"));
    add(state.behind, QCoreApplication::translate("folder", "%1 behind"));
    add(state.staged, QCoreApplication::translate("folder", "%1 staged"));
    add(state.changed, QCoreApplication::translate("folder", "%1 changed"));
    add(state.untracked, QCoreApplication::translate("folder", "%1 untracked"));
    add(state.conflicts,
        state.conflicts == 1 ? QCoreApplication::translate("folder", "%1 conflict")
                             : QCoreApplication::translate("folder", "%1 conflicts"));
    if (parts.isEmpty())
        return QCoreApplication::translate("folder", "Clean");
    return parts.join(QStringLiteral(" ") + QChar(0x00B7) + QStringLiteral(" "));
}

// The folder's name and path as the title and first line, then its branch and
// a line of Git status, and the actions. The error of the last action is the
// line under them.
class FolderView : public widgets::SummaryView
{
public:
    FolderView(ItemContext &context, QWidget *parent)
        : widgets::SummaryView(parent)
        , m_context(context)
    {
        auto *state = new QWidget;
        auto *layout = new QVBoxLayout(state);
        layout->setContentsMargins(0, 0, 0, 0);
        m_path = plainLabel(QStringLiteral("path"), state);
        m_branch = plainLabel(QStringLiteral("branch"), state);
        m_status = plainLabel(QStringLiteral("gitStatus"), state);
        layout->addWidget(m_path);
        layout->addWidget(m_branch);
        layout->addWidget(m_status);
        setStateRow(state);

        actions()->addAction(kTerminal, QCoreApplication::translate("folder", "Terminal"),
                             QCoreApplication::translate("folder", "Terminal"), [this] {
            actions()->showError(m_context.launchTerminal(m_localPath, QString(), {}));
        });
        actions()->addAction(kEditor, QCoreApplication::translate("folder", "Editor"),
                             QCoreApplication::translate("folder", "Editor"), [this] { openEditor(); });
        actions()->addAction(kFileManager, QCoreApplication::translate("folder", "File manager"),
                             QCoreApplication::translate("folder", "Files"),
                             [this] { m_context.showFolder(m_localPath); });
        actions()->addAction(kChooseFolder, QCoreApplication::translate("folder", "Choose folder…"),
                             QCoreApplication::translate("folder", "Folder…"), [this] {
            // A modal picker. The core does not make the view again after it,
            // so the view reads the path again itself.
            m_context.mapFolder(folderId());
            readPath();
            if (isVisible())
                refreshGit();
        });

        m_timer = new QTimer(this);
        m_timer->setObjectName(QStringLiteral("gitRefresh"));
        m_timer->setInterval(kRefreshMs);
        connect(m_timer, &QTimer::timeout, this, [this] { refreshGit(); });

        readPath();
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        widgets::SummaryView::showEvent(event);
        refreshGit();
        m_timer->start();
    }

    void hideEvent(QHideEvent *event) override
    {
        widgets::SummaryView::hideEvent(event);
        m_timer->stop();
    }

private:
    QString folderId() const { return m_context.config().value(QStringLiteral("folder")).toString(); }

    // Reads the local path and draws what does not need Git.
    void readPath()
    {
        const QString path = m_context.folderPath(folderId());
        if (path != m_localPath) {
            m_localPath = path;
            m_state.reset();
        }
        const bool mapped = !path.isEmpty();
        actions()->setActionVisible(kTerminal, mapped);
        actions()->setActionVisible(kEditor, mapped);
        actions()->setActionVisible(kFileManager, mapped);
        actions()->setActionVisible(kChooseFolder, !mapped);

        if (!mapped) {
            setTitle(QString());
            m_path->setText(QCoreApplication::translate("folder", "Folder not set on this device"));
            m_branch->hide();
            m_status->hide();
            return;
        }
        // Module views cannot read the project's folders, so the name is the
        // path's last part. A drive's root has none.
        const QString name = QDir(path).dirName();
        setTitle(name.isEmpty() ? QDir::toNativeSeparators(path) : name);
        m_path->setText(QDir::toNativeSeparators(path));
        drawGit();
    }

    void refreshGit()
    {
        // One read at a time. The answer of a read for an earlier path is
        // dropped.
        if (m_localPath.isEmpty() || m_reading)
            return;
        m_reading = true;
        const QString path = m_localPath;
        m_context.gitStatus(path, this, [this, path](const GitState &state) {
            m_reading = false;
            if (path != m_localPath)
                return;
            // The same state as before draws nothing, so a selection in the
            // text stays.
            if (m_state && sameState(*m_state, state))
                return;
            m_state = state;
            drawGit();
        });
    }

    void drawGit()
    {
        if (!m_state) {
            // Before the first answer, the branch from the files alone.
            const QString head = m_context.gitHead(m_localPath);
            m_branch->setText(head);
            m_branch->setVisible(!head.isEmpty());
            m_status->hide();
            return;
        }

        const GitState &state = *m_state;
        QString branch;
        QString status;
        switch (state.result) {
        case GitState::Result::Ok:
            if (!state.branch.isEmpty())
                branch = state.branch;
            else if (!state.commit.isEmpty())
                branch = QCoreApplication::translate("folder", "Detached at %1").arg(state.commit);
            else
                branch = QCoreApplication::translate("folder", "Detached");
            status = statusLine(state);
            break;
        case GitState::Result::NotARepository:
            status = QCoreApplication::translate("folder", "Not a Git repository");
            break;
        case GitState::Result::GitMissing:
            // The branch comes from the files, without Git.
            branch = m_context.gitHead(m_localPath);
            status = QCoreApplication::translate("folder", "Git was not found");
            break;
        case GitState::Result::Failed:
            branch = m_context.gitHead(m_localPath);
            status = state.message.isEmpty() ? QCoreApplication::translate("folder", "Git failed.") : state.message;
            break;
        }
        m_branch->setText(branch);
        m_branch->setVisible(!branch.isEmpty());
        m_status->setText(status);
        m_status->setVisible(!status.isEmpty());
    }

    // The editor device setting is a command line, such as "code -n": its
    // first part is the program, and the folder's path is its last argument.
    void openEditor()
    {
        QStringList parts = QProcess::splitCommand(m_context.deviceSetting(kEditor).toString().trimmed());
        if (parts.isEmpty())
            parts << QStringLiteral("code");
        const QString program = parts.takeFirst();
        parts << m_localPath;
        actions()->showError(m_context.startProgram(m_localPath, program, parts));
    }

    ItemContext &m_context;
    QLabel *m_path = nullptr;
    QLabel *m_branch = nullptr;
    QLabel *m_status = nullptr;
    QTimer *m_timer = nullptr;
    QString m_localPath;
    std::optional<GitState> m_state;
    bool m_reading = false;
};

} // namespace

QWidget *createFolderView(ItemContext &context, QWidget *parent)
{
    return new FolderView(context, parent);
}

} // namespace mirante::modules::folder
