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
    settings.endGroup();
}

void AppConfig::save(QSettings& settings) const
{
    settings.beginGroup("Settings");
    settings.setValue("analyzerMaxPoints", analyzerMaxPoints);
    settings.setValue("analyzerTimeoutSec", analyzerTimeoutSec);
    settings.setValue("reconnectToDrain", reconnectToDrain);
    settings.setValue("useTls", useTls);
    settings.endGroup();
}
