#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <memory>

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

    QObject *streamModel();
    QObject *metadataModel();
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
    // Terminal notifications for scripted clients (lslviewer_probe) and tests. `ok` is false when
    // the worker reported an error; a successful discovery that found no stream still reports true.
    void discoveryFinished(bool ok);
    void metadataFinished(bool ok);

private:
    void setStatus(const QString &status);
    void setSelectedUid(const QString &uid);
    void setDetails(const QVariantMap &details);
    void setXml(const QString &xml);

    // The list models, the worker pool and the request bookkeeping are implementation
    // details, not part of the QML-facing surface. They live in the .cpp so that this
    // header only needs QtCore value types: rebuilding the controller no longer pulls
    // QThreadPool, models.h or <functional>/<optional> into every consumer.
    struct Impl;
    std::unique_ptr<Impl> d;

    QString m_status;
    QString m_selectedUid;
    QVariantMap m_details;
    QString m_xml;
    bool m_refreshing = false;
    bool m_loadingMetadata = false;
};
