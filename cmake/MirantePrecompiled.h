// The precompiled header of Mirante's libraries and tests. See
// mirante_precompile_headers() in MiranteTargets.cmake.
//
// It holds the headers that most files include. A target gets only the Qt
// headers of the Qt modules it links, which Qt announces with its QT_*_LIB
// macros, so a library that links only Qt Core still compiles. A source file
// must still include what it uses: builds with MIRANTE_PCH=OFF check that.

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

#if defined(QT_GUI_LIB)
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#endif

#if defined(QT_WIDGETS_LIB)
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#endif

#if defined(QT_TESTLIB_LIB)
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#endif
