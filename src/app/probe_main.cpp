// lslviewer_probe: drives the product's core without a window and dumps the
// result as JSON, so end-to-end tests can assert on real LSL discovery instead
// of GUI automation.
//
//   lslviewer_probe --stream name:LSLViewer --timeout 10 --out dump.json
//
// Exit codes: 0 = dumped, 2 = timed out, 3 = stream not found, 4 = usage error.

#include "models.h"
#include "viewercontroller.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <cstdio>
#include <functional>

namespace {

bool waitFor(const std::function<bool()> &predicate, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return predicate();
}

QJsonArray streamsJson(const StreamListModel *model)
{
    QJsonArray streams;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row);
        streams.append(QJsonObject{
            {QStringLiteral("uid"), model->data(index, StreamListModel::UidRole).toString()},
            {QStringLiteral("name"), model->data(index, StreamListModel::NameRole).toString()},
            {QStringLiteral("type"), model->data(index, StreamListModel::TypeRole).toString()},
            {QStringLiteral("channel_count"), model->data(index, StreamListModel::ChannelsRole).toInt()},
            {QStringLiteral("nominal_srate"), model->data(index, StreamListModel::RateRole).toDouble()},
            {QStringLiteral("source_id"), model->data(index, StreamListModel::SourceRole).toString()}});
    }
    return streams;
}

QJsonArray rowsJson(const MetadataListModel *model)
{
    QJsonArray rows;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row);
        rows.append(QJsonObject{
            {QStringLiteral("name"), model->data(index, MetadataListModel::NameRole).toString()},
            {QStringLiteral("value"), model->data(index, MetadataListModel::ValueRole).toString()},
            {QStringLiteral("depth"), model->data(index, MetadataListModel::DepthRole).toInt()}});
    }
    return rows;
}

// Queries: "uid:<uid>", "name:<exact name>", or a case-insensitive substring of
// the name (falling back to an exact uid match).
QString findUid(const StreamListModel *model, const QString &query)
{
    const bool byUid = query.startsWith(QStringLiteral("uid:"));
    const bool byName = query.startsWith(QStringLiteral("name:"));
    const QString wanted = (byUid || byName) ? query.section(QLatin1Char(':'), 1).trimmed() : query.trimmed();
    if (wanted.isEmpty())
        return {};

    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row);
        const QString uid = model->data(index, StreamListModel::UidRole).toString();
        const QString name = model->data(index, StreamListModel::NameRole).toString();
        if (byUid && uid == wanted)
            return uid;
        if (byName && name.compare(wanted, Qt::CaseInsensitive) == 0)
            return uid;
        if (!byUid && !byName
            && (name.contains(wanted, Qt::CaseInsensitive) || uid == wanted))
            return uid;
    }
    return {};
}

void writeOutput(const QJsonObject &dump, const QString &path)
{
    const QByteArray json = QJsonDocument(dump).toJson(QJsonDocument::Indented);
    if (path.isEmpty()) {
        std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
        std::fflush(stdout);
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(path));
        return;
    }
    file.write(json);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("lslviewer-probe"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Dump one LSL stream's metadata as JSON for scripted testing."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption streamOption(
        {QStringLiteral("s"), QStringLiteral("stream")},
        QStringLiteral("Stream to select: uid:<uid>, name:<name>, or a name substring."),
        QStringLiteral("query"));
    const QCommandLineOption timeoutOption(
        {QStringLiteral("t"), QStringLiteral("timeout")},
        QStringLiteral("Overall timeout in seconds (default 10)."), QStringLiteral("seconds"),
        QStringLiteral("10"));
    const QCommandLineOption outOption(
        {QStringLiteral("o"), QStringLiteral("out")},
        QStringLiteral("Write JSON to this file instead of stdout."), QStringLiteral("path"));
    parser.addOptions({streamOption, timeoutOption, outOption});
    parser.process(app);

    bool timeoutOk = false;
    const double timeoutSeconds = parser.value(timeoutOption).toDouble(&timeoutOk);
    if (!timeoutOk || timeoutSeconds <= 0) {
        std::fprintf(stderr, "invalid --timeout\n");
        return 4;
    }
    const int timeoutMs = static_cast<int>(timeoutSeconds * 1000);
    const QString query = parser.value(streamOption);

    ViewerController controller;
    bool discoveryDone = false;
    bool discoveryOk = false;
    bool metadataDone = false;
    bool metadataOk = false;
    QObject::connect(&controller, &ViewerController::discoveryFinished,
                     [&](bool ok) { discoveryDone = true; discoveryOk = ok; });
    QObject::connect(&controller, &ViewerController::metadataFinished,
                     [&](bool ok) { metadataDone = true; metadataOk = ok; });

    QElapsedTimer clock;
    clock.start();
    const auto remaining = [&] { return qMax(0, timeoutMs - static_cast<int>(clock.elapsed())); };
    const auto streamModel = qobject_cast<StreamListModel *>(controller.streamModel());
    const auto metadataModel = qobject_cast<MetadataListModel *>(controller.metadataModel());
    if (!streamModel || !metadataModel) {
        std::fprintf(stderr, "controller models unavailable\n");
        return 4;
    }

    bool timedOut = false;
    QString error;
    int exitCode = 0;

    controller.refresh();
    if (!waitFor([&] { return discoveryDone; }, remaining())) {
        timedOut = true;
        exitCode = 2;
    } else if (!discoveryOk) {
        error = controller.status();
        exitCode = 2;
    } else if (streamModel->rowCount() > 0) {
        const QString targetUid = query.isEmpty() ? controller.selectedUid() : findUid(streamModel, query);
        if (targetUid.isEmpty()) {
            error = QStringLiteral("no stream matches \"%1\"").arg(query);
            exitCode = 3;
        } else {
            if (targetUid != controller.selectedUid())
                controller.selectStream(targetUid);
            if (!waitFor([&] { return metadataDone; }, remaining())) {
                timedOut = true;
                exitCode = 2;
            } else if (!metadataOk) {
                error = controller.status();
                exitCode = 2;
            }
        }
    }

    QJsonObject dump;
    dump.insert(QStringLiteral("ok"), exitCode == 0);
    dump.insert(QStringLiteral("timedOut"), timedOut);
    dump.insert(QStringLiteral("error"), error);
    dump.insert(QStringLiteral("status"), controller.status());
    dump.insert(QStringLiteral("selectedUid"), controller.selectedUid());
    dump.insert(QStringLiteral("streamCount"), streamModel->rowCount());
    dump.insert(QStringLiteral("streams"), streamsJson(streamModel));
    dump.insert(QStringLiteral("details"), QJsonObject::fromVariantMap(controller.details()));
    dump.insert(QStringLiteral("rows"), rowsJson(metadataModel));
    dump.insert(QStringLiteral("xml"), controller.xml());
    writeOutput(dump, parser.value(outOption));

    controller.shutdown();
    return exitCode;
}
