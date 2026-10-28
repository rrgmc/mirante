#pragma once

#include <QTabBar>

#include <memory>

namespace mirante {

// The tab bar of root dashboards. The preview tab has an italic title and a
// pin button. The + button asks for a new root dashboard.
// Implemented in RootTabBar.cpp.
class RootTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit RootTabBar(QWidget *parent = nullptr);
    ~RootTabBar() override;

    // The index of the preview tab, or -1 if there is none.
    void setPreviewIndex(int index);
    int previewIndex() const;

signals:
    void pinRequested(int index);
    void newDashboardRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
