#include "MainWindow.h"
#include "Theme.h"
#include "WatchTypes.h"

#include <QApplication>
#include <QStyleFactory>
#include <QStyleHints>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("FocusLock");
    QApplication::setOrganizationName("FocusLock");
    QApplication::setApplicationVersion("1.0.0");

    // Monitoring continues from the tray after the window closes.
    QApplication::setQuitOnLastWindowClosed(false);

    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark); // dark title bars
    QApplication::setPalette(theme::palette());
    app.setStyleSheet(theme::styleSheet());
    QApplication::setWindowIcon(theme::appIcon());

    qRegisterMetaType<WatchItem>();
    qRegisterMetaType<WatchStatus>();
    qRegisterMetaType<QList<WatchItem>>();
    qRegisterMetaType<QList<WatchStatus>>();
    qRegisterMetaType<QList<quint32>>();

    MainWindow window;
    window.show();
    return app.exec();
}
