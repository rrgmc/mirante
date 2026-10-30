#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace mirante {

class ApiClient;

// Quick add makes a task from pasted text. Handlers read the text and propose
// a task; finders then offer more items for it. Both answer asynchronously,
// through a request object, as a status fetch does through a StatusJob. See
// docs/modules.md#quick-add and decision 0078.

// One item quick add offers. The core adds it as a module's own item dialog
// would: config holds keys of the type's fields, and the item's profile is
// empty, so it follows its group.
struct QuickAddItem
{
    QString typeId;         // such as "gitlab.mr.summary"
    QString name;           // such as "!42"
    QJsonObject config;
};

// A project setting of the handler's module to match, such as
// project = "acme/backend" for the gitlab module. The core picks the project
// whose moduleSettings.<module id>.<key> equals value; strings compare
// without case.
struct ProjectMatch
{
    QString key;
    QJsonValue value;
};

// What a handler makes of the text.
struct QuickAddProposal
{
    QString title;                      // the new task's title
    std::vector<QuickAddItem> items;    // in the order they are offered
    std::optional<ProjectMatch> project;
    // Text the finders search, such as the title, a branch name and a key.
    // The core adds the task's title as the user leaves it.
    QStringList texts;
};

// One run of a handler. The core makes it, passes it to
// QuickAddHandler::propose(), and deletes it after finish().
class QuickAddRequest
{
public:
    virtual ~QuickAddRequest() = default;

    // The pasted text, trimmed.
    virtual QString text() const = 0;

    // A screen of the handler's module's settings, in the profile the quick
    // add dialog shows. Empty if nothing is stored.
    virtual QJsonObject moduleSettings(const QString &screenId) const = 0;

    // The module's server in that profile, or null for a module without a
    // ServerType. Include <mirante/ApiClient.h> to use it.
    virtual ApiClient *server() = 0;

    // Lives as long as the request: the receiver for its requests.
    virtual QObject *owner() = 0;

    // Ends the run. Call it exactly once, with nothing if the text is not for
    // this handler. A run that does not finish in 30 seconds counts as nothing.
    virtual void finish(std::optional<QuickAddProposal> proposal) = 0;
};

// Reads pasted text, such as a merge request's address or a Jira key.
// Registered with ModuleRegistrar::addQuickAddHandler().
class QuickAddHandler
{
public:
    virtual ~QuickAddHandler() = default;

    // Reads request.text() and calls request.finish(). Return quickly, with
    // finish(std::nullopt), for text that is not for this handler, so the
    // server is not asked.
    virtual void propose(QuickAddRequest &request) const = 0;
};

// One run of a finder, after the user chose the project. The core deletes it
// after finish().
class RelatedItemRequest
{
public:
    virtual ~RelatedItemRequest() = default;

    // The proposal's texts, then the task's title.
    virtual QStringList texts() const = 0;

    // The items offered so far, from the handler and other finders. A finder
    // need not leave them out: the core drops an item whose type and
    // configuration are already offered.
    virtual std::vector<QuickAddItem> items() const = 0;

    // A project setting of the finder's module, in the chosen project, or
    // undefined.
    virtual QJsonValue projectSetting(const QString &key) const = 0;

    // As in QuickAddRequest.
    virtual QJsonObject moduleSettings(const QString &screenId) const = 0;
    virtual ApiClient *server() = 0;
    virtual QObject *owner() = 0;

    // Ends the run with the items found, possibly none. Call it exactly once.
    virtual void finish(std::vector<QuickAddItem> items) = 0;
};

// Offers items related to a new task, such as merge requests whose branch
// holds a Jira key. Registered with ModuleRegistrar::addRelatedItemFinder().
class RelatedItemFinder
{
public:
    virtual ~RelatedItemFinder() = default;

    // Searches request.texts() and calls request.finish().
    virtual void find(RelatedItemRequest &request) const = 0;
};

} // namespace mirante
