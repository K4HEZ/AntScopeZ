#ifndef BANDPRESETS_H
#define BANDPRESETS_H

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <itubands.h>

// QML-facing wrapper over core::ItuBands: region/band data for
// SettingsPage's Band section, persisted separately from the desktop app's
// own settings (see the .cpp) since main.cpp reuses the desktop's
// organization/application name.
class BandPresets : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QStringList regions READ regions CONSTANT)
    Q_PROPERTY(QString region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(QStringList bandLabels READ bandLabels NOTIFY bandsChanged)
    // {"fromKHz","toKHz","label"} per band in the current region, unwidened
    // -- SwrChart draws one highlight rectangle per entry.
    Q_PROPERTY(QVariantList bands READ bands NOTIFY bandsChanged)
    Q_PROPERTY(int widenPercent READ widenPercent WRITE setWidenPercent NOTIFY widenPercentChanged)

public:
    explicit BandPresets(QObject* parent = nullptr);

    QStringList regions() const { return m_regions; }
    QString region() const { return m_region; }
    void setRegion(const QString& region);
    QStringList bandLabels() const { return m_bandLabels; }
    QVariantList bands() const { return m_bands; }
    int widenPercent() const { return m_widenPercent; }
    void setWidenPercent(int v);

    // {"fromKHz", "toKHz"} for the given index into bandLabels/bands,
    // padded per side by widenPercent (see ItuBands::widen()). Empty map
    // if bandIndex is out of range.
    Q_INVOKABLE QVariantMap widenedRange(int bandIndex) const;

signals:
    void regionChanged();
    void bandsChanged();
    void widenPercentChanged();

private:
    QMap<QString, QList<BandPreset>> m_allBands;
    QStringList m_regions;
    QString m_region;
    QStringList m_bandLabels;
    QVariantList m_bands;
    int m_widenPercent = 20;
};

#endif // BANDPRESETS_H
