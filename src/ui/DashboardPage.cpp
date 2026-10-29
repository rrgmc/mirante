#include "ui/DashboardPage.h"

#include "layout/CellStyle.h"
#include "layout/DashboardCellFactory.h"
#include "layout/HeaderPill.h"
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
        header = new QWidget(this);
        header->setObjectName(QStringLiteral("taskDashboardHeader"));
        // MainWindow tints it again with the task's group colour.
        cellstyle::tintHeader(header, cellstyle::taskHeaderStrength);
        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(6, 3, 2, 3);
        headerLayout->setSpacing(4);
        title = new HeaderPill;
        title->setObjectName(QStringLiteral("taskDashboardTitle"));
        QFont font = title->font();
        font.setBold(true);
        title->setFont(font);
        part = new HeaderPill;
        part->setObjectName(QStringLiteral("taskDashboardPart"));
        // The task's project, in the grey pill of task cells. It gives way
        // first when the bar is narrow, then the dashboard or item.
        context = new HeaderPill;
        context->setObjectName(QStringLiteral("taskDashboardContext"));
        context->setFill(HeaderPill::Fill::Neutral);
        context->setForegroundRole(QPalette::PlaceholderText);
        headerRow = new HeaderRow(header);
        headerRow->setParts({title, part, context});
        headerRow->setShrinkOrder({context, part, title});
        headerLayout->addWidget(headerRow, 1);
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
