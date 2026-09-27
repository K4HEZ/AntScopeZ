#include "printutils.h"
#include <QPrinterInfo>

QPageSize PrintUtils::defaultPageSize()
{
    QPrinterInfo info = QPrinterInfo::defaultPrinter();
    if (!info.isNull() && info.defaultPageSize().isValid())
        return info.defaultPageSize();
    return QPageSize(QPageSize::Letter);
}
