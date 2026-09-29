#include "itubands.h"

#include <QStringList>
#include <algorithm>

QMap<QString, QList<BandPreset>> ItuBands::parse(const QString& text)
{
    QMap<QString, QList<BandPreset>> result;
    QList<BandPreset>* current = nullptr;

    const QStringList lines = text.split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith('[')) {
            const int end = line.indexOf(']');
            if (end < 0)
                continue;
            current = &result[line.mid(1, end - 1)];
            continue;
        }
        if (current == nullptr)
            continue;

        const QStringList parts = line.split(',');
        if (parts.size() < 3)
            continue;
        BandPreset b;
        b.fromKHz = parts.at(0).trimmed().toDouble();
        b.toKHz = parts.at(1).trimmed().toDouble();
        b.label = parts.at(2).trimmed();
        current->append(b);
    }
    return result;
}

void ItuBands::widen(double fromKHz, double toKHz, double percent, double& outFromKHz, double& outToKHz)
{
    const double pad = (toKHz - fromKHz) * (percent / 100.0);
    outFromKHz = std::max(0.0, fromKHz - pad);
    outToKHz = toKHz + pad;
}
