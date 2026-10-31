#include "web/PageFreezer.h"

#include <QWebEnginePage>
#include <QWebEngineView>
#include <QWidget>

#include <utility>

namespace mirante {

namespace {

using State = QWebEnginePage::LifecycleState;

// Frozen when Chromium recommends Frozen or Discarded. Discarded is never
// used, because a discarded page loads again and loses its state.
void apply(QWebEnginePage *page)
{
    if (page->recommendedState() != State::Active && page->lifecycleState() == State::Active)
        page->setLifecycleState(State::Frozen);
}

} // namespace

PageFreezer::PageFreezer(QObject *parent)
    : QObject(parent)
{
}

PageFreezer::~PageFreezer() = default;

void PageFreezer::freeze(const QList<QWidget *> &windows)
{
    if (!m_watch)
        m_watch = new QObject(this);
    for (QWidget *window : windows) {
        if (!window)
            continue;
        const QList<QWebEngineView *> views = window->findChildren<QWebEngineView *>();
        for (QWebEngineView *view : views) {
            if (QWebEnginePage *page = view->page())
                follow(page);
        }
    }
}

void PageFreezer::follow(QWebEnginePage *page)
{
    for (const QPointer<QWebEnginePage> &known : std::as_const(m_pages)) {
        if (known == page)
            return;
    }
    m_pages << page;
    // The recommendation changes when the page stops loading or playing sound.
    connect(page, &QWebEnginePage::recommendedStateChanged, m_watch, [page] { apply(page); });
    apply(page);
}

void PageFreezer::thaw()
{
    delete m_watch;
    m_watch = nullptr;
    const QList<QPointer<QWebEnginePage>> pages = std::exchange(m_pages, {});
    for (const QPointer<QWebEnginePage> &page : pages) {
        if (page && page->lifecycleState() != State::Active)
            page->setLifecycleState(State::Active);
    }
}

} // namespace mirante
