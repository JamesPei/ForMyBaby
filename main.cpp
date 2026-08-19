#include "mainwindow.h"

#include <QApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QPixmap>
#include <QScreen>
#include <QSplashScreen>
#include <QTimer>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "ForMyBaby_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // 启动画面，停留 0.5s 后显示主窗口
    QPixmap pixmap(":/static_resource/FMB_splash.jpeg");
    // 缩放至屏幕的 80%（保持宽高比），避免竖屏大图超出桌面屏幕
    const QSize target = QGuiApplication::primaryScreen()->availableGeometry().size() * 0.8;
    const QPixmap scaledPixmap = pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QSplashScreen splash(scaledPixmap);
    splash.show();
    a.processEvents();

    MainWindow w;
    QTimer::singleShot(1000, &w, [&]() {
        w.show();
        splash.finish(&w);
    });

    return a.exec();
}