#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

class StreamListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles { UidRole = Qt::UserRole + 1, NameRole, TypeRole, ChannelsRole, RateRole, SourceRole };

    explicit StreamListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replace(const QVariantList &streams);
    // Returns an empty map when no stream with the uid exists.
    QVariantMap streamForUid(const QString &uid) const;

private:
    QVariantList m_streams;
};

class MetadataListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles { NameRole = Qt::UserRole + 1, ValueRole, DepthRole };

    explicit MetadataListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replace(const QVariantList &rows);

private:
    QVariantList m_rows;
};