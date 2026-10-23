#include "metadata.h"

#include <QVariantMap>

#include <pugixml.hpp>

#include <stdexcept>

namespace {

bool isText(const pugi::xml_node &node)
{
    return node.type() == pugi::node_pcdata || node.type() == pugi::node_cdata;
}

// Text before the first child element (ElementTree's element.text).
QString leadingText(const pugi::xml_node &element)
{
    QString text;
    for (pugi::xml_node n = element.first_child(); n && n.type() != pugi::node_element; n = n.next_sibling()) {
        if (isText(n))
            text += QString::fromUtf8(n.value());
    }
    return text.trimmed();
}

// Text after an element up to its next sibling element (ElementTree's element.tail).
QString tailText(const pugi::xml_node &element)
{
    QString text;
    for (pugi::xml_node n = element.next_sibling(); n && n.type() != pugi::node_element; n = n.next_sibling()) {
        if (isText(n))
            text += QString::fromUtf8(n.value());
    }
    return text.trimmed();
}

void appendRow(QVariantList &rows, const QString &name, const QString &value, int depth)
{
    rows.append(QVariantMap{{QStringLiteral("name"), name},
                            {QStringLiteral("value"), value},
                            {QStringLiteral("depth"), depth}});
}

void appendAttributes(QVariantList &rows, const pugi::xml_node &element, int depth)
{
    for (const pugi::xml_attribute &attribute : element.attributes())
        appendRow(rows, QStringLiteral("@") + QString::fromUtf8(attribute.name()),
                  QString::fromUtf8(attribute.value()), depth);
}

void appendChildren(QVariantList &rows, const pugi::xml_node &parent, int depth);

void appendElement(QVariantList &rows, const pugi::xml_node &element, int depth)
{
    appendRow(rows, QString::fromUtf8(element.name()), leadingText(element), depth);
    appendAttributes(rows, element, depth + 1);
    appendChildren(rows, element, depth + 1);
}

void appendChildren(QVariantList &rows, const pugi::xml_node &parent, int depth)
{
    for (pugi::xml_node child = parent.first_child(); child; child = child.next_sibling()) {
        if (child.type() != pugi::node_element)
            continue;
        appendElement(rows, child, depth);
        const QString tail = tailText(child);
        if (!tail.isEmpty())
            appendRow(rows, QStringLiteral("#text"), tail, depth);
    }
}

} // namespace

QVariantList parseMetadata(const QString &xml)
{
    if (xml.trimmed().isEmpty())
        return {};

    const QByteArray bytes = xml.toUtf8();
    pugi::xml_document document;
    const pugi::xml_parse_result parsed =
        document.load_buffer(bytes.constData(), static_cast<size_t>(bytes.size()),
                             pugi::parse_default | pugi::parse_ws_pcdata, pugi::encoding_utf8);
    if (!parsed)
        throw std::runtime_error(std::string("Invalid stream metadata XML: ") + parsed.description());

    const pugi::xml_node desc = document.document_element().child("desc");
    if (!desc)
        return {};

    QVariantList rows;
    const QString descText = leadingText(desc);
    if (!descText.isEmpty())
        appendRow(rows, QStringLiteral("#text"), descText, 0);
    appendAttributes(rows, desc, 0);
    appendChildren(rows, desc, 0);
    return rows;
}