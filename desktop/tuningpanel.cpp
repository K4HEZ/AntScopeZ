#include "tuningpanel.h"
#include "bandindicator.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QResizeEvent>
#include <QFontMetrics>
#include <cfloat>

static const double kSwrGreenMax = 2.0;
static const double kSwrRedMin = 3.0;

static QString number(double v, int decimals = 2)
{
    return v == DBL_MAX ? QStringLiteral("--") : QString::number(v, 'f', decimals);
}

QColor TuningPanel::swrColor(double swr)
{
    if (swr == DBL_MAX)
        return QColor(128, 128, 128);
    if (swr <= kSwrGreenMax)
        return QColor(0, 178, 90);
    if (swr <= kSwrRedMin)
        return QColor(235, 165, 0);
    return QColor(215, 55, 55);
}

TuningPanel::TuningPanel(QWidget* parent) :
    QWidget(parent)
{
    setObjectName("tab_tuning");

    m_swrLabel = new QLabel(QStringLiteral("--"), this);
    m_swrLabel->setAlignment(Qt::AlignCenter);
    m_swrLabel->setMinimumSize(100, 60);
    m_swrLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QFont bold = m_swrLabel->font();
    bold.setBold(true);
    m_swrLabel->setFont(bold);
    m_swrLabel->installEventFilter(this);

    m_freqLabel = new QLabel(QStringLiteral("--"), this);
    m_freqLabel->setAlignment(Qt::AlignCenter);
    m_freqLabel->setFont(bold);

    QLabel* caption = new QLabel(tr("SWR"), this);
    caption->setAlignment(Qt::AlignCenter);

    QVBoxLayout* readout = new QVBoxLayout();
    readout->setSpacing(0);
    readout->addWidget(m_freqLabel, 0);
    readout->addWidget(m_swrLabel, 1);
    readout->addWidget(caption, 0);

    // Readings: heading and rule like the other left-column sections, then
    // two columns of name/value pairs.
    m_params = new QWidget(this);
    QFrame* line = new QFrame(m_params);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    QLabel* heading = new QLabel(tr("Readings"), m_params);
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    heading->setFont(headingFont);

    const QStringList names = {
        tr("Frequency"), tr("SWR"), tr("RL"), tr("|rho|"), tr("rho phase"), tr("R"),
        tr("X"), tr("|Z|"), tr("Rp"), tr("Xp"), tr("|Zp|"),
    };
    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(2);
    for (int i = 0; i < names.size(); i++) {
        int row = i % 6;
        int col = 2 * (i / 6);
        QLabel* name = new QLabel(names.at(i), m_params);
        QLabel* value = new QLabel(QStringLiteral("--"), m_params);
        grid->addWidget(name, row, col);
        grid->addWidget(value, row, col + 1);
        m_values << value;
    }
    QVBoxLayout* paramsLayout = new QVBoxLayout(m_params);
    paramsLayout->setContentsMargins(0, 0, 0, 0);
    paramsLayout->setSpacing(4);
    paramsLayout->addWidget(line);
    paramsLayout->addWidget(heading);
    paramsLayout->addLayout(grid);

    m_band = new BandIndicator(this);
    connect(m_band, &BandIndicator::frequencyPicked, this, &TuningPanel::frequencyPicked);

    QVBoxLayout* main = new QVBoxLayout(this);
    main->setContentsMargins(11, 11, 11, 11);
    main->setSpacing(10);
    main->addLayout(readout, 1);
    main->addWidget(m_band, 0);

    clear();
}

void TuningPanel::setBands(const QList<BandPreset>& bands)
{
    m_band->setBands(bands);
}

void TuningPanel::setFrequencyKHz(double khz)
{
    m_band->setFrequencyKHz(khz);
}

void TuningPanel::clear()
{
    addData(GraphData());
}

void TuningPanel::addData(const GraphData& d)
{
    QColor c = swrColor(d.SWR);
    m_swrLabel->setText(d.SWR == DBL_MAX ? QStringLiteral("--")
                                         : (d.SWR > 99.9 ? QStringLiteral(">99.9:1") : number(d.SWR) + QStringLiteral(":1")));
    m_swrLabel->setStyleSheet(QString("QLabel { color: %1; }").arg(c.name()));
    QString freq = QStringLiteral("--");
    if (d.FQ != DBL_MAX) {
        // Same format as the frequency selector.
        freq = QString::number(d.FQ * 1000.0, 'f', 3);
        while (freq.endsWith('0'))
            freq.chop(1);
        if (freq.endsWith('.'))
            freq.chop(1);
        freq += QStringLiteral(" kHz");
    }
    m_freqLabel->setText(freq);
    m_freqLabel->setStyleSheet(m_swrLabel->styleSheet());
    m_band->setDotColor(c);
    fitFontToLabel();

    const QStringList values = {
        d.FQ == DBL_MAX ? QStringLiteral("--") : number(d.FQ, 4) + QStringLiteral(" MHz"),
        number(d.SWR),
        d.RL == DBL_MAX ? QStringLiteral("--") : number(d.RL) + QStringLiteral(" dB"),
        number(d.RhoMod, 3),
        d.RhoPhase == DBL_MAX ? QStringLiteral("--") : number(d.RhoPhase) + QStringLiteral(" °"),
        d.R == DBL_MAX ? QStringLiteral("--") : number(d.R) + QStringLiteral(" Ω"),
        d.X == DBL_MAX ? QStringLiteral("--") : number(d.X) + QStringLiteral(" Ω"),
        d.Z == DBL_MAX ? QStringLiteral("--") : number(d.Z) + QStringLiteral(" Ω"),
        d.Rpar == DBL_MAX ? QStringLiteral("--") : number(d.Rpar) + QStringLiteral(" Ω"),
        d.Xpar == DBL_MAX ? QStringLiteral("--") : number(d.Xpar) + QStringLiteral(" Ω"),
        d.Zpar == DBL_MAX ? QStringLiteral("--") : number(d.Zpar) + QStringLiteral(" Ω"),
    };
    for (int i = 0; i < m_values.size() && i < values.size(); i++)
        m_values.at(i)->setText(values.at(i));
}

bool TuningPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_swrLabel && event->type() == QEvent::Resize)
        fitFontToLabel();
    return QWidget::eventFilter(watched, event);
}

// Largest point size at which the text still fits the label.
void TuningPanel::fitFontToLabel()
{
    if (m_fitting)
        return;
    m_fitting = true;
    QString text = m_swrLabel->text();
    QRect target = m_swrLabel->contentsRect().adjusted(10, 10, -10, -10);
    if (text.isEmpty() || target.width() <= 0 || target.height() <= 0) {
        m_fitting = false;
        return;
    }
    QFont font = m_swrLabel->font();
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
    if (best != m_swrLabel->font().pointSize()) {
        font.setPointSize(best);
        m_swrLabel->setFont(font);
    }
    // Digits have no descenders; push the line down so they look centered.
    int top = QFontMetrics(font).descent();
    if (m_swrLabel->contentsMargins().top() != top)
        m_swrLabel->setContentsMargins(0, top, 0, 0);
    m_fitting = false;

    // Frequency readout at a quarter of the SWR size.
    QFont freqFont = m_freqLabel->font();
    freqFont.setPointSize(qMax(8, best / 4));
    if (freqFont.pointSize() != m_freqLabel->font().pointSize())
        m_freqLabel->setFont(freqFont);
}
