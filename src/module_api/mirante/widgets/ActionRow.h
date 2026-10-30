#pragma once

#include <QString>
#include <QUrl>
#include <QWidget>

#include <functional>
#include <memory>

class QMenu;

namespace mirante::widgets {

// The buttons of a native view, which wrap in a narrow cell, with a line for
// the error of the last action.
class ActionRow : public QWidget
{
public:
    explicit ActionRow(QWidget *parent = nullptr);
    ~ActionRow() override;

    // Adds a button. shortLabel replaces label in a narrow cell.
    void addAction(const QString &id, const QString &label, const QString &shortLabel,
                   std::function<void()> handler);
    // Adds a button with a menu. fill is called each time it opens, such as
    // to list what a Jira issue allows now.
    void addMenuAction(const QString &id, const QString &label, const QString &shortLabel,
                       std::function<void(QMenu &)> fill);
    void setActionVisible(const QString &id, bool visible);
    bool isActionVisible(const QString &id) const;
    // Disables the button while its request runs.
    void setBusy(const QString &id, bool busy);

    // The "Web" link at the end of the row, which opens url in the external
    // browser. An empty url hides it.
    void setWebLink(const QUrl &url);
    QUrl webLink() const;

    void showError(const QString &text);
    void clearError();
    QString error() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante::widgets
