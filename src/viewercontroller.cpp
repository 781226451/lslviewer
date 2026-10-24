#include "viewercontroller.h"

#include "metadata.h"
#include "models.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

#include <lsl_cpp.h>

#include <algorithm>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

QString fromStd(const std::string &value)
{
    return QString::fromStdString(value);
}

QString channelFormatName(lsl::channel_format_t format)
{
    switch (static_cast<int>(format)) {
    case 0: return QStringLiteral("undefined");
    case 1: return QStringLiteral("float32");
    case 2: return QStringLiteral("double64");
    case 3: return QStringLiteral("string");
    case 4: return QStringLiteral("int32");
    case 5: return QStringLiteral("int16");
    case 6: return QStringLiteral("int8");
    case 7: return QStringLiteral("int64");
    default: return QString::number(static_cast<int>(format));
    }
}

QVariantMap summaryFor(const lsl::stream_info &info)
{
    return {{QStringLiteral("uid"), fromStd(info.uid())},
            {QStringLiteral("name"), fromStd(info.name())},
            {QStringLiteral("type"), fromStd(info.type())},
            {QStringLiteral("channel_count"), info.channel_count()},
            {QStringLiteral("nominal_srate"), info.nominal_srate()},
            {QStringLiteral("source_id"), fromStd(info.source_id())}};
}

QVariant discoverStreams()
{
    QVariantList result;
    for (const lsl::stream_info &info : lsl::resolve_streams(1.0))
        result.append(summaryFor(info));
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap left = a.toMap();
        const QVariantMap right = b.toMap();
        const QString leftName = left.value(QStringLiteral("name")).toString().toCaseFolded();
        const QString rightName = right.value(QStringLiteral("name")).toString().toCaseFolded();
        if (leftName != rightName)
            return leftName < rightName;
        return left.value(QStringLiteral("uid")).toString() < right.value(QStringLiteral("uid")).toString();
    });
    return result;
}

QVariant readStreamMetadata(const QString &uid)
{
    const std::vector<lsl::stream_info> matches = lsl::resolve_stream("uid", uid.toStdString(), 1, 1.0);
    if (matches.empty())
        throw std::runtime_error("读取 metadata 前流已离线");

    lsl::stream_inlet inlet(matches.front(), 1, 0, false);
    lsl::stream_info fullInfo = inlet.info(2.0);
    if (fromStd(fullInfo.uid()) != uid)
        throw std::runtime_error("读取过程中流的 UID 发生变化");

    const QString xml = fromStd(fullInfo.as_xml());
    QVariantMap details = summaryFor(fullInfo);
    details.insert(QStringLiteral("hostname"), fromStd(fullInfo.hostname()));
    details.insert(QStringLiteral("version"), fullInfo.version());
    details.insert(QStringLiteral("created_at"), fullInfo.created_at());
    details.insert(QStringLiteral("channel_format"), channelFormatName(fullInfo.channel_format()));
    const QVariantList rows = parseMetadata(xml);
    inlet.close_stream();
    return QVariantMap{{QStringLiteral("details"), details},
                       {QStringLiteral("xml"), xml},
                       {QStringLiteral("rows"), rows}};
}

} // namespace

// State that must not leak into the header: the list models, the worker pool and the
// request bookkeeping. Qt signals are public member functions, so Impl can emit them
// through the owning controller without widening the public API.
struct ViewerController::Impl
{
    enum class WorkerKind { Discovery, Metadata };

    explicit Impl(ViewerController *owner)
        : owner(owner), streamModel(owner), metadataModel(owner)
    {
        threadPool.setMaxThreadCount(2);
    }

    void startWorker(WorkerKind kind, int requestId, std::function<QVariant()> operation);
    void loadPendingMetadata();
    void onWorkerCompleted(WorkerKind kind, int requestId, const QVariant &result,
                           const QString &error);

    ViewerController *owner;
    StreamListModel streamModel;
    MetadataListModel metadataModel;
    QThreadPool threadPool;

    bool closing = false;
    bool metadataActive = false;
    bool hasPendingMetadata = false;
    int metadataRequestId = 0;
    int pendingRequestId = 0;
    QString pendingUid;
};

void ViewerController::Impl::startWorker(WorkerKind kind, int requestId,
                                         std::function<QVariant()> operation)
{
    QPointer<ViewerController> self(owner);
    // The request runs on the pool; the result is delivered back to the controller's
    // thread so that model updates and signal emission stay single threaded.
    threadPool.start(QRunnable::create([self, kind, requestId, operation = std::move(operation)] {
        QVariant result;
        QString error;
        try {
            result = operation();
        } catch (const std::exception &exc) {
            error = QString::fromUtf8(exc.what());
        } catch (...) {
            error = QStringLiteral("未知错误");
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, kind, requestId, result, error] {
            if (self)
                self->d->onWorkerCompleted(kind, requestId, result, error);
        }, Qt::QueuedConnection);
    }));
}

void ViewerController::Impl::loadPendingMetadata()
{
    if (metadataActive || !hasPendingMetadata || closing)
        return;
    const int requestId = pendingRequestId;
    const QString uid = pendingUid;
    hasPendingMetadata = false;
    metadataActive = true;
    startWorker(WorkerKind::Metadata, requestId, [uid] { return readStreamMetadata(uid); });
}

void ViewerController::Impl::onWorkerCompleted(WorkerKind kind, int requestId,
                                               const QVariant &result, const QString &error)
{
    if (closing)
        return;

    if (kind == WorkerKind::Discovery) {
        owner->m_refreshing = false;
        owner->busyChanged();
        if (!error.isEmpty()) {
            owner->setStatus(
                QStringLiteral("发现失败：%1。请检查 LSL 运行库和网络后重试。").arg(error));
            owner->discoveryFinished(false);
            return;
        }

        const QVariantList streams = result.toList();
        streamModel.replace(streams);
        if (streams.isEmpty()) {
            ++metadataRequestId;
            hasPendingMetadata = false;
            owner->m_loadingMetadata = false;
            owner->busyChanged();
            owner->setSelectedUid(QString());
            owner->setDetails({});
            owner->setXml(QString());
            metadataModel.replace({});
            owner->setStatus(QStringLiteral("未发现 LSL 流。请启动发布端，或检查网络后刷新。"));
            owner->discoveryFinished(true);
            return;
        }

        QString selectedUid = owner->m_selectedUid;
        const bool selectionMissing =
            selectedUid.isEmpty() || streamModel.streamForUid(selectedUid).isEmpty();
        if (selectionMissing)
            selectedUid = streams.front().toMap().value(QStringLiteral("uid")).toString();
        if (selectionMissing || (owner->m_xml.isEmpty() && !owner->m_loadingMetadata))
            owner->selectStream(selectedUid);
        else if (!owner->m_loadingMetadata)
            owner->setStatus(QStringLiteral("发现 %1 个 LSL 流。").arg(streams.size()));
        owner->discoveryFinished(true);
        return;
    }

    metadataActive = false;
    loadPendingMetadata();
    if (requestId != metadataRequestId)
        return;
    owner->m_loadingMetadata = false;
    owner->busyChanged();
    if (!error.isEmpty()) {
        metadataModel.replace({});
        owner->setXml(QString());
        owner->setStatus(
            QStringLiteral("Metadata 读取失败：%1。流可能已离线，请重新读取。").arg(error));
        owner->metadataFinished(false);
        return;
    }

    const QVariantMap metadata = result.toMap();
    const QVariantList rows = metadata.value(QStringLiteral("rows")).toList();
    owner->setDetails(metadata.value(QStringLiteral("details")).toMap());
    metadataModel.replace(rows);
    owner->setXml(metadata.value(QStringLiteral("xml")).toString());
    owner->setStatus(rows.isEmpty() ? QStringLiteral("已加载完整信息；该流没有 desc metadata。")
                                    : QStringLiteral("Metadata 已加载。"));
    owner->metadataFinished(true);
}

ViewerController::ViewerController(QObject *parent)
    : QObject(parent),
      d(std::make_unique<Impl>(this)),
      m_status(QStringLiteral("准备搜索 LSL 流…"))
{
}

ViewerController::~ViewerController()
{
    shutdown();
}

QObject *ViewerController::streamModel()
{
    return &d->streamModel;
}

QObject *ViewerController::metadataModel()
{
    return &d->metadataModel;
}

void ViewerController::setStatus(const QString &status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void ViewerController::setSelectedUid(const QString &uid)
{
    if (m_selectedUid != uid) {
        m_selectedUid = uid;
        emit selectedUidChanged();
    }
}

void ViewerController::setDetails(const QVariantMap &details)
{
    m_details = details;
    emit detailsChanged();
}

void ViewerController::setXml(const QString &xml)
{
    m_xml = xml;
    emit xmlChanged();
}

void ViewerController::refresh()
{
    if (d->closing || m_refreshing)
        return;
    m_refreshing = true;
    emit busyChanged();
    setStatus(QStringLiteral("正在发现 LSL 流…"));
    d->startWorker(Impl::WorkerKind::Discovery, 0, discoverStreams);
}

void ViewerController::selectStream(const QString &uid)
{
    if (d->closing)
        return;
    const QVariantMap stream = d->streamModel.streamForUid(uid);
    if (stream.isEmpty()) {
        setStatus(QStringLiteral("所选流已不可用，请刷新后重试。"));
        return;
    }
    if (uid == m_selectedUid && m_loadingMetadata)
        return;

    setSelectedUid(uid);
    setDetails(stream);
    setXml(QString());
    d->metadataModel.replace({});
    const int requestId = ++d->metadataRequestId;
    m_loadingMetadata = true;
    emit busyChanged();
    setStatus(QStringLiteral("正在读取 %1 的 metadata…")
                  .arg(stream.value(QStringLiteral("name")).toString()));
    // Keep only the latest selection while a bounded network request finishes.
    d->hasPendingMetadata = true;
    d->pendingRequestId = requestId;
    d->pendingUid = uid;
    d->loadPendingMetadata();
}

void ViewerController::copyXml()
{
    if (!m_xml.isEmpty()) {
        QGuiApplication::clipboard()->setText(m_xml);
        setStatus(QStringLiteral("完整 XML 已复制到剪贴板。"));
    }
}

void ViewerController::shutdown()
{
    d->closing = true;
    d->hasPendingMetadata = false;
    d->threadPool.clear();
    d->threadPool.waitForDone();
}
