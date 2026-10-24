#pragma once

#include <QString>
#include <QVariantList>

// Flattens the <desc> part of an LSL StreamInfo XML into rows {name, value, depth}.
// Throws std::runtime_error on invalid XML.
QVariantList parseMetadata(const QString &xml);