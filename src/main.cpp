#include <aws/core/Aws.h>                   // AWS SDK core initialization/shutdown
#include <QApplication>                     // Qt widget application
#include <QQmlApplicationEngine>            // QML engine to load .qml files
#include <QQmlContext>                      // Access to expose C++ objects to QML
#include "settings.h"                       // Application-wide Settings interface
#include "Database.h"                       // Database access and schema management
#include <QtCharts/QAbstractSeries>         // Register Qt Charts QML module

#ifdef Q_OS_MAC
#include "MacOSMonitoring.h"            // macOS-specific monitoring implementation
#elif defined(Q_OS_WIN)
#include "WindowsMonitoring.h"          // Windows-specific monitoring implementation
#else
#error "Unsupported platform!"
#endif

int main(int argc, char *argv[]) {
    // ---------- AWS SDK Initialization ----------
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    qDebug() << "[AWS] AWS SDK initialized.";

    // ---------- Qt Application Setup ----------
    QApplication app(argc, argv);
    Settings settings;                        // Holds user email/phone/threshold settings
    QQmlApplicationEngine engine;             // Loads and runs the QML UI

// ---------- Platform-Specific Monitoring ----------
// Constructs either MacOSMonitoring or WindowsMonitoring with the shared Settings
#ifdef Q_OS_MAC
    MacOSMonitoring monitoring(&settings);
#elif defined(Q_OS_WIN)
    WindowsMonitoring monitoring(&settings);
#endif

    // Expose C++ objects to QML under known property names
    engine.rootContext()->setContextProperty("Settings", &settings);
    engine.rootContext()->setContextProperty("Monitoring", &monitoring);

    // ---------- Database Singleton Registration ----------
    // Makes Database available in QML as Monitor.Database singleton
    qmlRegisterSingletonType<Database>(
        "Monitor.Database", 1, 0, "Database",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            auto *db = new Database();
            db->createSchema();               // Ensure schema is created
            qDebug() << "[QML] Registered Database singleton.";
            return db;
        }
        );

    // Register the QtCharts QML module (for chart types in QML)
    qmlRegisterModule("QtCharts", 2, 15);

    // Load the main QML UI file from the Qt resource system
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Monitor/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        // If loading failed, shut down AWS and exit with error
        Aws::ShutdownAPI(options);
        return -1;
    }

    // ---------- Connect Monitoring Logs to QML ----------
    // When MonitoringBase emits logMessage, call addLog(message) in the root QML object
    QObject::connect(
        &monitoring, &MonitoringBase::logMessage,
        &engine, [&engine](const QString &message) {
            QObject *root = engine.rootObjects().first();
            if (root) {
                QMetaObject::invokeMethod(
                    root, "addLog",
                    Q_ARG(QVariant, message)
                    );
            }
        }
        );

    // Run the Qt event loop
    int result = app.exec();

    // Cleanly shut down the AWS SDK before exiting
    Aws::ShutdownAPI(options);
    return result;
}
