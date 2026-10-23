#pragma once

#include "models.h"

#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QVariant>
#include <QVariantMap>

#include <functional>
#include <optional>
#include <utility>

class ViewerController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *streamModel READ streamModel CONSTANT)
    Q_PROPERTY(QObject *metadataModel READ metadataModel CONSTANT)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY busyChanged)
    Q_PROPERTY(bool loadingMetadata READ loadingMetadata NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString selectedUid READ selectedUid NOTIFY selectedUidChanged)
    Q_PROPERTY(QVariantMap details READ details NOTIFY detailsChanged)
    Q_PROPERTY(QString xml READ xml NOTIFY xmlChanged)

public:
    explicit ViewerController(QObject *parent = nullptr);
    ~ViewerController() override;

    QObject *streamModel() { return &m_streamModel; }
    QObject *metadataModel() { return &m_metadataModel; }
    bool refreshing() const { return m_refreshing; }
    bool loadingMetadata() const { return m_loadingMetadata; }
    QString status() const { return m_status; }
    QString selectedUid() const { return m_selectedUid; }
    QVariantMap details() const { return m_details; }
    QString xml() const { return m_xml; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectStream(const QString &uid);
    Q_INVOKABLE void copyXml();
    Q_INVOKABLE void shutdown();

signals:
    void statusChanged();
    void selectedUidChanged();
    void detailsChanged();
    void xmlChanged();
    void busyChanged();

private:
    enum class WorkerKind { Discovery, Metadata };

    void setStatus(const QString &status);
    void setSelectedUid(const QString &uid);
    void setDetails(const QVariantMap &details);
    void setXml(const QString &xml);
    void startWorker(WorkerKind kind, int requestId, std::function<QVariant()> operation);
    void loadPendingMetadata();
    void onWorkerCompleted(WorkerKind kind, int requestId, const QVariant &result, const QString &error);

    StreamListModel m_streamModel;
    MetadataListModel m_metadataModel;
    QString m_status;
    QString m_selectedUid;
    QVariantMap m_details;
    QString m_xml;
    bool m_refreshing = false;
    bool m_closing = false;
    bool m_loadingMetadata = false;
    bool m_metadataActive = false;
    std::optional<std::pair<int, QString>> m_pendingMetadata;
    int m_metadataRequestId = 0;
    QThreadPool m_threadPool;
};