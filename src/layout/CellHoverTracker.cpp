#include "layout/CellHoverTracker.h"

#include <QEvent>
#include <QWidget>

#include <algorithm>

namespace mirante {

CellHoverTracker::CellHoverTracker(QObject *parent)
    : QObject(parent)
{
}

CellHoverTracker::~CellHoverTracker() = default;

void CellHoverTracker::watch(QWidget *cell, const QString &id)
{
    if (!cell)
        return;
    m_watched.push_back({cell, id});
    cell->installEventFilter(this);
    connect(cell, &QObject::destroyed, this, [this](QObject *gone) {
        m_watched.erase(std::remove_if(m_watched.begin(), m_watched.end(),
                                       [gone](const Watched &watched) { return watched.cell == gone; }),
                        m_watched.end());
        leave(gone);
    });
    if (cell->underMouse())
        enter(cell);
}

QString CellHoverTracker::hoveredId() const { return m_current; }

bool CellHoverTracker::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Enter)
        enter(watched);
    else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide)
        leave(watched);
    return QObject::eventFilter(watched, event);
}

void CellHoverTracker::enter(QObject *cell)
{
    if (std::find(m_hovered.begin(), m_hovered.end(), cell) == m_hovered.end())
        m_hovered.push_back(cell);
    update();
}

void CellHoverTracker::leave(QObject *cell)
{
    m_hovered.erase(std::remove(m_hovered.begin(), m_hovered.end(), cell), m_hovered.end());
    update();
}

void CellHoverTracker::update()
{
    QString id;
    if (!m_hovered.empty()) {
        const auto it = std::find_if(m_watched.begin(), m_watched.end(),
                                     [cell = m_hovered.back()](const Watched &watched) { return watched.cell == cell; });
        if (it != m_watched.end())
            id = it->id;
    }
    if (id == m_current)
        return;
    m_current = id;
    emit hoveredChanged(id);
}

} // namespace mirante
