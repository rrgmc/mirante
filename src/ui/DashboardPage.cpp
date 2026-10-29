#include "ui/DashboardPage.h"

#include "layout/CellStyle.h"
#include "layout/DashboardCellFactory.h"
#include "layout/LayoutView.h"
#include "ui/FocusView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace mirante {

DashboardPage::DashboardPage(FocusView *focusView, QWidget *parent)
    : QWidget(parent)
    , focus(focusView)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    focusView->setParent(this);
    layout->addWidget(focusView);
}

DashboardPage::DashboardPage(const LayoutServices &services, const DashboardRef &dashboard, QWidget *parent,
                             const QString &item)
    : QWidget(parent)
    , ref(dashboard)
    , itemId(item)
    , factory(std::make_unique<DashboardCellFactory>(services, dashboard))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    if (!dashboard.isRoot()) {
        auto *header = new QWidget(this);
        header->setObjectName(QStringLiteral("taskDashboardHeader"));
        cellstyle::tintHeader(header, 0.22);
        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(6, 2, 2, 2);
        title = new QLabel(header);
        QFont font = title->font();
        font.setBold(true);
        title->setFont(font);
        title->setMinimumWidth(1);
        headerLayout->addWidget(title);
        // The task's project, dimmed after the title, as in a task cell.
        context = new QLabel(header);
        context->setObjectName(QStringLiteral("taskDashboardContext"));
        context->setTextFormat(Qt::PlainText);
        context->setForegroundRole(QPalette::PlaceholderText);
        context->setMinimumWidth(1);
        context->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        headerLayout->addWidget(context, 1);
        if (item.isEmpty()) {
            lock = new QToolButton(header);
            lock->setObjectName(QStringLiteral("taskLockButton"));
            lock->setAutoRaise(true);
            headerLayout->addWidget(lock);
        }
        layout->addWidget(header);
    }
    view = new LayoutView(factory.get(), this);
    layout->addWidget(view, 1);
}

DashboardPage::~DashboardPage()
{
    // The view goes before the factory it uses.
    delete view;
}

} // namespace mirante
