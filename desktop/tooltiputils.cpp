#include "tooltiputils.h"

void ToolTipUtils::wrapLongToolTips(QWidget *root, int lengthThreshold, int widthPx)
{
    for (QWidget *w : root->findChildren<QWidget*>())
    {
        const QString tip = w->toolTip();
        if (tip.length() > lengthThreshold && !tip.startsWith('<'))
            w->setToolTip(wrapToolTip(tip, lengthThreshold, widthPx));
    }
}

QString ToolTipUtils::wrapToolTip(const QString &plainText, int lengthThreshold, int widthPx)
{
    if (plainText.length() <= lengthThreshold || plainText.startsWith('<'))
        return plainText;

    // A handful of plain-text tooltips (mainwindow.ui's OSL calibration
    // one, for one) already use a literal "\n" for a deliberate line
    // break -- plain HTML collapses a raw newline into insignificant
    // whitespace instead of honoring it, so that break would silently
    // disappear on conversion unless it's turned into a real <br> first.
    const QString escaped = plainText.toHtmlEscaped().replace("\n", "<br>");
    return QString("<table width='%1'><tr><td>%2</td></tr></table>")
               .arg(widthPx)
               .arg(escaped);
}
