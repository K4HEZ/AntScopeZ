#include "printutils.h"
#include "settings.h"
#include <QPrinterInfo>
#include <QSettings>

QPageSize PrintUtils::defaultPageSize()
{
    QPrinterInfo info = QPrinterInfo::defaultPrinter();
    if (!info.isNull() && info.defaultPageSize().isValid())
        return info.defaultPageSize();
    return QPageSize(QPageSize::Letter);
}

QPageLayout PrintUtils::savedPageLayout(QPageLayout::Orientation orientation)
{
    QSettings settings(Settings::setIniFile(), QSettings::IniFormat);
    settings.beginGroup("Print");
    QPageSize size = defaultPageSize();
    QMarginsF margins(0.5, 0.5, 0.5, 0.5);
    if (settings.contains("pageSizeId")) {
        size = QPageSize(QPageSize::PageSizeId(settings.value("pageSizeId").toInt()));
        orientation = QPageLayout::Orientation(settings.value("pageOrientation").toInt());
        margins = QMarginsF(settings.value("marginLeft", 0.5).toDouble(),
                            settings.value("marginTop", 0.5).toDouble(),
                            settings.value("marginRight", 0.5).toDouble(),
                            settings.value("marginBottom", 0.5).toDouble());
    }
    return QPageLayout(size, orientation, margins, QPageLayout::Inch);
}
