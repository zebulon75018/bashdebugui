#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);


    // 📌 Récupération des arguments
    QString scriptPath;
    if (argc > 1) {
        scriptPath = QString::fromLocal8Bit(argv[1]);
    }

    MainWindow window;
    window.show();

    // 📌 Charger automatiquement le script si fourni
    if (!scriptPath.isEmpty()) {
        window.loadScriptFromArgument(scriptPath);
    }


    return app.exec();
}
