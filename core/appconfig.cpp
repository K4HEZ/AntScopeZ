#include "appconfig.h"
#include <QSettings>

AppConfig& AppConfig::get()
{
    static AppConfig instance;
    return instance;
}

void AppConfig::load(QSettings& settings)
{
    settings.beginGroup("Settings");
    analyzerMaxPoints = settings.value("analyzerMaxPoints", analyzerMaxPoints).toInt();
    analyzerTimeoutSec = settings.value("analyzerTimeoutSec", analyzerTimeoutSec).toInt();
    reconnectToDrain = settings.value("reconnectToDrain", reconnectToDrain).toBool();
    useTls = settings.value("useTls", useTls).toBool();
    maxMarkers = settings.value("maxMarkers", maxMarkers).toInt();
    autoMarkerAtLowestSwr = settings.value("autoMarkerAtLowestSwr", autoMarkerAtLowestSwr).toBool();
    bandMarginPercent = qBound(0, settings.value("bandMarginPercent", bandMarginPercent).toInt(), 100);
    settings.endGroup();

    settings.beginGroup("MainWindow");
    measureSystemMetric = settings.value("measureSystemMetric", measureSystemMetric).toBool();
    systemImpedance = settings.value("systemImpedance", systemImpedance).toDouble();
    settings.endGroup();

    settings.beginGroup("Cable");
    cable.velFactor = settings.value("VelFactor", cable.velFactor).toDouble();
    cable.resistance = settings.value("R0", cable.resistance).toDouble();
    cable.lossConductive = settings.value("ConductiveLoss", cable.lossConductive).toDouble();
    cable.lossDielectric = settings.value("DielectricLoss", cable.lossDielectric).toDouble();
    cableLossFqMHz = settings.value("LossFrequencyMHz", cableLossFqMHz).toDouble();
    cable.lossUnits = settings.value("LossUnits", cable.lossUnits).toInt();
    cable.lossAtAnyFq = settings.value("LossAtAnyFrequency", 0).toInt() != 0;
    cable.lengthFeet = settings.value("Length", cable.lengthFeet).toDouble();
    farEndMeasurement = settings.value("FarEndMeasurement", farEndMeasurement).toInt();
    cableName = settings.value("CableName").toString();
    recentCables = settings.value("RecentCables").toStringList();
    cableIsPreset = settings.value("CableIsPreset", cableIsPreset).toBool();
    settings.endGroup();
}

void AppConfig::save(QSettings& settings) const
{
    settings.beginGroup("Settings");
    settings.setValue("analyzerMaxPoints", analyzerMaxPoints);
    settings.setValue("analyzerTimeoutSec", analyzerTimeoutSec);
    settings.setValue("reconnectToDrain", reconnectToDrain);
    settings.setValue("useTls", useTls);
    settings.setValue("maxMarkers", maxMarkers);
    settings.setValue("autoMarkerAtLowestSwr", autoMarkerAtLowestSwr);
    settings.setValue("bandMarginPercent", bandMarginPercent);
    settings.endGroup();

    settings.beginGroup("MainWindow");
    settings.setValue("measureSystemMetric", measureSystemMetric);
    settings.setValue("systemImpedance", systemImpedance);
    settings.endGroup();

    settings.beginGroup("Cable");
    settings.setValue("VelFactor", cable.velFactor);
    settings.setValue("R0", cable.resistance);
    settings.setValue("ConductiveLoss", cable.lossConductive);
    settings.setValue("DielectricLoss", cable.lossDielectric);
    settings.setValue("LossFrequencyMHz", cableLossFqMHz);
    settings.setValue("LossUnits", cable.lossUnits);
    settings.setValue("LossAtAnyFrequency", cable.lossAtAnyFq ? 1 : 0); // int, as before
    settings.setValue("Length", cable.lengthFeet);
    settings.setValue("FarEndMeasurement", farEndMeasurement);
    settings.setValue("CableName", cableName);
    settings.setValue("RecentCables", recentCables);
    settings.setValue("CableIsPreset", cableIsPreset);
    settings.endGroup();
}
