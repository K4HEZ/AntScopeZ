#include "bandpresets.h"

#include <QFile>
#include <QTextStream>

#include "mobilesettings.h"

BandPresets::BandPresets(QObject* parent)
    : QObject(parent)
{
    QFile file(":/itu-regions-defaults.txt");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&file);
        m_allBands = ItuBands::parse(stream.readAll());
    }
    m_regions = m_allBands.keys();

    QString region = mobileSettings().value("band/region").toString();
    if (!m_regions.contains(region))
        region = m_regions.isEmpty() ? QString() : m_regions.first();
    setRegion(region);

    m_widenPercent = qBound(0, mobileSettings().value("band/widenPercent", 20).toInt(), 100);
}

void BandPresets::setRegion(const QString& region)
{
    if (m_region == region)
        return;
    m_region = region;
    mobileSettings().setValue("band/region", region);
    rebuild();
    emit regionChanged();
}

void BandPresets::rebuild()
{
    m_shown.clear();
    m_bandLabels.clear();
    m_bands.clear();
    for (const BandPreset& b : m_allBands.value(m_region)) {
        if (b.toKHz < m_limitMinKHz || b.fromKHz > m_limitMaxKHz)
            continue;
        m_shown << b;
        m_bandLabels << QString("%1 - (%2 - %3 kHz)").arg(b.label)
                            .arg(QString::number(b.fromKHz, 'g', 10))
                            .arg(QString::number(b.toKHz, 'g', 10));
        QVariantMap map;
        map["fromKHz"] = b.fromKHz;
        map["toKHz"] = b.toKHz;
        map["label"] = b.label;
        m_bands << map;
    }
    emit bandsChanged();
}

void BandPresets::setLimitMinKHz(double v)
{
    if (m_limitMinKHz == v)
        return;
    m_limitMinKHz = v;
    rebuild();
    emit limitsChanged();
}

void BandPresets::setLimitMaxKHz(double v)
{
    if (m_limitMaxKHz == v)
        return;
    m_limitMaxKHz = v;
    rebuild();
    emit limitsChanged();
}

void BandPresets::setWidenPercent(int v)
{
    v = qBound(0, v, 100);
    if (m_widenPercent == v)
        return;
    m_widenPercent = v;
    mobileSettings().setValue("band/widenPercent", v);
    emit widenPercentChanged();
}

QVariantMap BandPresets::widenedRange(int bandIndex) const
{
    QVariantMap result;
    if (bandIndex < 0 || bandIndex >= m_shown.size())
        return result;
    const BandPreset& b = m_shown.at(bandIndex);
    double from = 0, to = 0;
    ItuBands::widen(b.fromKHz, b.toKHz, m_widenPercent, from, to);
    result["fromKHz"] = from;
    result["toKHz"] = to;
    return result;
}
