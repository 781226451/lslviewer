// Contract test for the QML UI that both the product and the Python tooling load.
// It pins the surface qml/Main.qml is allowed to rely on: the context property
// name, its properties and invokables, the list-model role names and the object
// names the Python tests used to assert. A change on either side fails here.

#include "models.h"
#include "viewercontroller.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSet>
#include <QStringList>
#include <QtTest>

namespace {

const QStringList kRequiredObjectNames = {
    QStringLiteral("streamList"),   QStringLiteral("detailTabs"),
    QStringLiteral("metadataView"), QStringLiteral("xmlText"),
    QStringLiteral("refreshButton"),
};

const QStringList kRequiredProperties = {
    QStringLiteral("streamModel"),   QStringLiteral("metadataModel"),
    QStringLiteral("refreshing"),    QStringLiteral("loadingMetadata"),
    QStringLiteral("status"),        QStringLiteral("selectedUid"),
    QStringLiteral("details"),       QStringLiteral("xml"),
};

const QStringList kRequiredMethods = {
    QStringLiteral("refresh()"),      QStringLiteral("selectStream(QString)"),
    QStringLiteral("copyXml()"),      QStringLiteral("shutdown()"),
};

const QStringList kStreamRoles = {QStringLiteral("uid"),   QStringLiteral("name"),
                                  QStringLiteral("streamType"), QStringLiteral("channelCount"),
                                  QStringLiteral("nominalSrate"), QStringLiteral("sourceId")};

const QStringList kMetadataRoles = {QStringLiteral("name"), QStringLiteral("value"),
                                    QStringLiteral("depth")};

QStringList roleNamesOf(const QHash<int, QByteArray> &roles)
{
    QStringList names;
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        names.append(QString::fromLatin1(it.value()));
    names.sort();
    return names;
}

} // namespace

class QmlContractTest : public QObject
{
    Q_OBJECT

public:
    void setQmlPath(const QString &path) { m_qmlPath = path; }

private slots:
    void initTestCase();
    void cleanupTestCase();
    void loadsWithoutWarnings();
    void exposesRequiredObjectNames();
    void controllerExposesRequiredApi();
    void modelsExposeRequiredRoles();

private:
    QString m_qmlPath;
    QQmlApplicationEngine *m_engine = nullptr;
    ViewerController *m_controller = nullptr;
    QObject *m_window = nullptr;
    QStringList m_warnings;
};

void QmlContractTest::initTestCase()
{
    QVERIFY2(!m_qmlPath.isEmpty(), "QML path argument missing");
    QVERIFY2(QFile::exists(m_qmlPath), qPrintable(m_qmlPath));

    m_controller = new ViewerController;
    m_engine = new QQmlApplicationEngine;
    connect(m_engine, &QQmlApplicationEngine::warnings, this,
            [this](const QList<QQmlError> &warnings) {
                for (const QQmlError &warning : warnings)
                    m_warnings.append(warning.toString());
            });
    m_engine->rootContext()->setContextProperty(QStringLiteral("viewer"), m_controller);
    m_engine->load(QUrl::fromLocalFile(m_qmlPath));

    QVERIFY2(!m_engine->rootObjects().isEmpty(),
             qPrintable(m_warnings.join(QLatin1Char('\n'))));
    m_window = m_engine->rootObjects().first();
}

void QmlContractTest::cleanupTestCase()
{
    if (m_controller)
        m_controller->shutdown();
    delete m_engine;
    m_engine = nullptr;
    delete m_controller;
    m_controller = nullptr;
    m_window = nullptr;
}

void QmlContractTest::loadsWithoutWarnings()
{
    // Main.qml kicks off a discovery from Component.onCompleted; let it settle so
    // that any binding error surfaces before the assertion.
    QTest::qWait(500);
    QVERIFY2(m_warnings.isEmpty(), qPrintable(m_warnings.join(QLatin1Char('\n'))));
}

void QmlContractTest::exposesRequiredObjectNames()
{
    QVERIFY(m_window);
    for (const QString &name : kRequiredObjectNames) {
        QVERIFY2(m_window->findChild<QObject *>(name) != nullptr,
                 qPrintable(QStringLiteral("missing QML object: %1").arg(name)));
    }
}

void QmlContractTest::controllerExposesRequiredApi()
{
    const QMetaObject *meta = m_controller->metaObject();
    for (const QString &property : kRequiredProperties) {
        QVERIFY2(meta->indexOfProperty(qPrintable(property)) >= 0,
                 qPrintable(QStringLiteral("missing property: %1").arg(property)));
    }
    for (const QString &method : kRequiredMethods) {
        QVERIFY2(meta->indexOfMethod(qPrintable(method)) >= 0,
                 qPrintable(QStringLiteral("missing method: %1").arg(method)));
    }
}

void QmlContractTest::modelsExposeRequiredRoles()
{
    const auto *streams = qobject_cast<StreamListModel *>(m_controller->streamModel());
    const auto *metadata = qobject_cast<MetadataListModel *>(m_controller->metadataModel());
    QVERIFY(streams);
    QVERIFY(metadata);

    QStringList expectedStreams = kStreamRoles;
    expectedStreams.sort();
    QStringList expectedMetadata = kMetadataRoles;
    expectedMetadata.sort();

    QCOMPARE(roleNamesOf(streams->roleNames()), expectedStreams);
    QCOMPARE(roleNamesOf(metadata->roleNames()), expectedMetadata);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    QGuiApplication app(argc, argv);

    const QString qmlPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QmlContractTest test;
    test.setQmlPath(qmlPath);
    return QTest::qExec(&test, 1, argv);
}

#include "tst_qmlcontract.moc"
