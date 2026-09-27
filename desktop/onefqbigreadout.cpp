#include "onefqbigreadout.h"

#include <QVBoxLayout>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QCloseEvent>

OneFqBigReadout::OneFqBigReadout(QWidget* parent) :
    QDialog(parent)
{
    setWindowTitle(tr("SWR"));
    setSizeGripEnabled(true);

    m_valueLabel = new QLabel(this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    QFont valueFont = m_valueLabel->font();
    valueFont.setBold(true);
    m_valueLabel->setFont(valueFont);

    m_captionLabel = new QLabel(tr("SWR"), this);
    m_captionLabel->setAlignment(Qt::AlignCenter);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_valueLabel, 1);
    layout->addWidget(m_captionLabel, 0);
    setLayout(layout);

    resize(300, 220);
    updateText();
}

void OneFqBigReadout::addData(const GraphData& data)
{
    // Same running-mean approach as OneFqWidget::addValue(), applied to
    // just the one field this style cares about.
    if (data.SWR != DBL_MAX)
        m_swr = (m_swr == DBL_MAX ? data.SWR : (m_swr + data.SWR) / 2.0);
    updateText();
}

void OneFqBigReadout::updateText()
{
    m_valueLabel->setText(m_swr == DBL_MAX ? QStringLiteral("--")
                                            : QString::number(m_swr, 'f', 2) + QStringLiteral(":1"));
    fitFontToLabel();
}

void OneFqBigReadout::fitFontToLabel()
{
    // No auto-fit-text pattern exists elsewhere in this codebase to
    // mirror -- binary-search the point size against QFontMetrics'
    // measured bounds until it's the largest size that still fits the
    // label's available space, so the digits stretch to fill on resize.
    QString text = m_valueLabel->text();
    if (text.isEmpty())
        return;

    QRect target = m_valueLabel->contentsRect().adjusted(10, 10, -10, -10);
    if (target.width() <= 0 || target.height() <= 0)
        return;

    QFont font = m_valueLabel->font();
    int lo = 1, hi = 500, best = lo;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        font.setPointSize(mid);
        QRect bounds = QFontMetrics(font).boundingRect(text);
        if (bounds.width() <= target.width() && bounds.height() <= target.height()) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    font.setPointSize(best);
    m_valueLabel->setFont(font);
}

void OneFqBigReadout::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    fitFontToLabel();
}

void OneFqBigReadout::mouseDoubleClickEvent(QMouseEvent*)
{
    emit styleToggleRequested();
}

void OneFqBigReadout::closeEvent(QCloseEvent* event)
{
    emit closing();
    QDialog::closeEvent(event);
}

void OneFqBigReadout::reject()
{
    emit closing();
    QDialog::reject();
}
