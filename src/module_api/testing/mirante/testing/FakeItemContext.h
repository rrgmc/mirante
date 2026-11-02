#pragma once

#include <mirante/ApiClient.h>
#include <mirante/ItemContext.h>

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QWidget>

namespace mirante::testing {

// An ItemContext for module tests: what a view reads, set by the test, and what
// it does, recorded. Without a web engine, a web view is a plain widget with the
// address in its "url" property, and a message view one with the text in its
// "message" property. A test that needs more derives from it.
class FakeItemContext : public ItemContext
{
public:
    struct AddedItem
    {
        QString typeId;
        QString name;
        QJsonObject config;
    };

    QJsonObject itemConfig;
    QJsonObject projectSettings;            // the item's module's project settings
    QMap<QString, QJsonObject> screens;     // module settings, by screen id
    QJsonObject deviceSettings;
    QHash<QString, QString> folders;        // folder id to local path
    QHash<QString, QString> files;          // name in files/ to absolute path
    ApiClient *client = nullptr;
    ItemStatusSource *statusSource = nullptr;

    QList<QUrl> webViews;       // the addresses of createWebView()
    QStringList messages;       // the texts of createMessageView()
    QList<QUrl> pages;          // the addresses of showPage()
    QStringList mapped;         // the ids of mapFolder()
    QStringList shownFolders;   // the paths of showFolder()
    QList<AddedItem> addedItems;

    QJsonObject config() const override { return itemConfig; }
    QJsonValue projectSetting(const QString &key) const override { return projectSettings.value(key); }
    QJsonObject moduleSettings(const QString &screenId) const override { return screens.value(screenId); }
    QString folderPath(const QString &folderId) const override { return folders.value(folderId); }
    void mapFolder(const QString &folderId) override { mapped << folderId; }
    QWidget *createWebView(const QUrl &url, QWidget *parent) override
    {
        webViews << url;
        auto *view = new QWidget(parent);
        view->setProperty("url", url);
        return view;
    }
    QWidget *createMessageView(const QString &text, QWidget *parent) override
    {
        messages << text;
        auto *view = new QWidget(parent);
        view->setProperty("message", text);
        return view;
    }
    void updateConfig(const QJsonObject &config) override { itemConfig = config; }
    void showPage(const QUrl &url) override { pages << url; }
    QString addItemToTask(const QString &typeId, const QString &name, const QJsonObject &config) override
    {
        addedItems.append(AddedItem{typeId, name, config});
        return QStringLiteral("new-item");
    }
    ApiClient *server() override { return client; }
    ItemStatusSource *status() override { return statusSource; }
    QJsonValue deviceSetting(const QString &key) const override { return deviceSettings.value(key); }
    QString filePath(const QString &fileName) const override { return files.value(fileName); }
    void showFolder(const QString &path) override { shownFolders << path; }
};

} // namespace mirante::testing
