#pragma once

#include <mirante/QuickAdd.h>

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace mirante {

class ItemTypeRegistry;
class Model;
class ServerClients;

// The work of quick add, without widgets: it runs the modules' handlers on
// pasted text, matches the proposal to a project, runs the finders, and
// creates the task. QuickAddDialog shows it. See docs/ui.md#quick-add and
// decision 0078. Implemented in QuickAddRun.cpp.
class QuickAddRun : public QObject
{
    Q_OBJECT

public:
    // Without servers, requests see a null server(), as in tests. Everything
    // passed must outlive this object.
    QuickAddRun(Model &model, const ItemTypeRegistry &types, ServerClients *servers, QObject *parent = nullptr);
    ~QuickAddRun() override;

    // How long a handler or finder may take, 30 seconds by default. Then it
    // counts as having found nothing.
    void setTimeout(int ms);

    // One handler's answer.
    struct Proposal
    {
        QString moduleId;           // the handler's module
        QuickAddProposal proposal;
    };

    // Runs every handler on text, in the profile, at once. Stops a run that is
    // still going, and drops its answers. Emits proposed() for each handler
    // that takes the text, as answers arrive, then proposalsDone().
    void propose(const QString &text, const QString &profileId);

    // The projects outside the trash whose moduleSettings.<moduleId>.<key>
    // equals the match's value, in sidebar order. Strings compare without
    // case. Empty without a match.
    static QStringList matchProjects(const Model &model, const QString &moduleId,
                                     const std::optional<ProjectMatch> &match);

    // Runs every finder for the proposal in the project, with the task's
    // title added to its texts. Stops a search that is still going. Emits
    // found() for each finder's new items, then findingDone(). Items whose
    // type and configuration are already offered, by the proposal or an
    // earlier finder, are dropped.
    void findRelated(const Proposal &proposal, const QString &title, const QString &projectId,
                     const QString &profileId);

    // Stops every handler and finder still running. Their answers are dropped.
    void cancel();

    // True while handlers or finders run.
    bool busy() const;

    // Creates an active task of Normal priority in the project, with the
    // items in order, and a dashboard whose one cell holds every item as a
    // tab, the first current. Returns the task's id, or empty if the project
    // does not exist.
    QString create(const QString &projectId, const QString &title, const std::vector<QuickAddItem> &items);

signals:
    void proposed(const mirante::QuickAddRun::Proposal &proposal);
    void proposalsDone();
    void found(const std::vector<mirante::QuickAddItem> &items);
    void findingDone();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
