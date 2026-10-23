#include "models.h"

StreamListModel::StreamListModel(QObject *parent) : QAbstractListModel(parent) {}

int StreamListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_streams.size());
}

QVariant StreamListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_streams.size())
        return {};
    const QVariantMap stream = m_streams.at(index.row()).toMap();
    switch (role) {
    case UidRole: return stream.value(QStringLiteral("uid"));
    case NameRole: return stream.value(QStringLiteral("name"));
    case TypeRole: return stream.value(QStringLiteral("type"));
    case ChannelsRole: return stream.value(QStringLiteral("channel_count"));
    case RateRole: return stream.value(QStringLiteral("nominal_srate"));
    case SourceRole: return stream.value(QStringLiteral("source_id"));
    default: return {};
    }
}

QHash<int, QByteArray> StreamListModel::roleNames() const
{
    return {{UidRole, "uid"},          {NameRole, "name"},          {TypeRole, "streamType"},
            {ChannelsRole, "channelCount"}, {RateRole, "nominalSrate"}, {SourceRole, "sourceId"}};
}

void StreamListModel::replace(const QVariantList &streams)
{
    beginResetModel();
    m_streams = streams;
    endResetModel();
}

QVariantMap StreamListModel::streamForUid(const QString &uid) const
{
    for (const QVariant &item : m_streams) {
        QVariantMap stream = item.toMap();
        if (stream.value(QStringLiteral("uid")).toString() == uid)
            return stream;
    }
    return {};
}

MetadataListModel::MetadataListModel(QObject *parent) : QAbstractListModel(parent) {}

int MetadataListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant MetadataListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const QVariantMap row = m_rows.at(index.row()).toMap();
    switch (role) {
    case NameRole: return row.value(QStringLiteral("name"));
    case ValueRole: return row.value(QStringLiteral("value"));
    case DepthRole: return row.value(QStringLiteral("depth"));
    default: return {};
    }
}

QHash<int, QByteArray> MetadataListModel::roleNames() const
{
    return {{NameRole, "name"}, {ValueRole, "value"}, {DepthRole, "depth"}};
}

void MetadataListModel::replace(const QVariantList &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}