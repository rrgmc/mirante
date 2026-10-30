#include "items/QuickAddRun.h"

#include "core/Model.h"

namespace mirante {

// Stubs until T3 of the M7 plan.

struct QuickAddRun::Private
{
};

QuickAddRun::QuickAddRun(Model &model, const ItemTypeRegistry &types, ServerClients *servers, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    Q_UNUSED(model)
    Q_UNUSED(types)
    Q_UNUSED(servers)
}

QuickAddRun::~QuickAddRun() = default;

void QuickAddRun::setTimeout(int ms) { Q_UNUSED(ms) }

void QuickAddRun::propose(const QString &text, const QString &profileId)
{
    Q_UNUSED(text)
    Q_UNUSED(profileId)
    emit proposalsDone();
}

QStringList QuickAddRun::matchProjects(const Model &model, const QString &moduleId,
                                       const std::optional<ProjectMatch> &match)
{
    Q_UNUSED(model)
    Q_UNUSED(moduleId)
    Q_UNUSED(match)
    return {};
}

void QuickAddRun::findRelated(const Proposal &proposal, const QString &title, const QString &projectId,
                              const QString &profileId)
{
    Q_UNUSED(proposal)
    Q_UNUSED(title)
    Q_UNUSED(projectId)
    Q_UNUSED(profileId)
    emit findingDone();
}

void QuickAddRun::cancel() {}

bool QuickAddRun::busy() const { return false; }

QString QuickAddRun::create(const QString &projectId, const QString &title, const std::vector<QuickAddItem> &items)
{
    Q_UNUSED(projectId)
    Q_UNUSED(title)
    Q_UNUSED(items)
    return {};
}

} // namespace mirante
