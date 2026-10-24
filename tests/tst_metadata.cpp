// Unit tests for the metadata flattening that both the detail view and the
// scripted probe rely on. Fixtures live in tests/fixtures next to their expected
// output so that adding a case needs no C++ change.

#include "metadata.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

#include <stdexcept>

namespace {

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QJsonArray rowsToJson(const QVariantList &rows)
{
    QJsonArray array;
    for (const QVariant &row : rows) {
        const QVariantMap map = row.toMap();
        array.append(QJsonObject{
            {QStringLiteral("name"), map.value(QStringLiteral("name")).toString()},
            {QStringLiteral("value"), map.value(QStringLiteral("value")).toString()},
            {QStringLiteral("depth"), map.value(QStringLiteral("depth")).toInt()}});
    }
    return array;
}

QString compact(const QJsonArray &array)
{
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QString expectedPathFor(const QString &fixture)
{
    QFileInfo info(fixture);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".expected.json"));
}

} // namespace

class MetadataTest : public QObject
{
    Q_OBJECT

private slots:
    void parse_data();
    void parse();
    void emptyInputYieldsNoRows();
    void rejectsMalformedXml();
};

void MetadataTest::parse_data()
{
    QTest::addColumn<QString>("fixture");

    const QDir dir(QStringLiteral(FIXTURES_DIR));
    const QStringList files = dir.entryList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        const QString path = dir.filePath(file);
        if (!QFile::exists(expectedPathFor(path)))
            continue;
        QTest::newRow(qPrintable(file)) << path;
    }
}

void MetadataTest::parse()
{
    QFETCH(QString, fixture);

    const QString xml = readText(fixture);
    QVERIFY2(!xml.isEmpty(), qPrintable(QStringLiteral("fixture is empty: %1").arg(fixture)));

    const QJsonArray actual = rowsToJson(parseMetadata(xml));
    const QJsonArray expected =
        QJsonDocument::fromJson(readText(expectedPathFor(fixture)).toUtf8()).array();

    QVERIFY2(actual == expected,
             qPrintable(QStringLiteral("\n  actual:   %1\n  expected: %2")
                            .arg(compact(actual), compact(expected))));
}

void MetadataTest::emptyInputYieldsNoRows()
{
    QCOMPARE(parseMetadata(QString()).size(), 0);
    QCOMPARE(parseMetadata(QStringLiteral("   \n\t ")).size(), 0);
}

void MetadataTest::rejectsMalformedXml()
{
    const QString xml = readText(QDir(QStringLiteral(FIXTURES_DIR)).filePath(QStringLiteral("malformed.xml")));
    QVERIFY(!xml.isEmpty());
    QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseMetadata(xml));
}

QTEST_GUILESS_MAIN(MetadataTest)

#include "tst_metadata.moc"
