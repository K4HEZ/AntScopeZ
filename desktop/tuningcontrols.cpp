#include "tuningcontrols.h"
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <cmath>

TuningControls::TuningControls(QWidget* parent) :
    QWidget(parent)
{
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    QLabel* heading = new QLabel(tr("Tuning"), this);
    QFont bold = heading->font();
    bold.setBold(true);
    heading->setFont(bold);

    m_edit = new QLineEdit(this);
    m_edit->setAlignment(Qt::AlignCenter);
    QFont editFont = m_edit->font();
    editFont.setPointSize(10);
    m_edit->setFont(editFont);
    m_edit->installEventFilter(this);

    QToolButton* down = new QToolButton(this);
    down->setArrowType(Qt::DownArrow);
    down->setAutoRepeat(true);
    down->setToolTip(tr("Lower the frequency by one step (Down arrow)"));
    QToolButton* up = new QToolButton(this);
    up->setArrowType(Qt::UpArrow);
    up->setAutoRepeat(true);
    up->setToolTip(tr("Raise the frequency by one step (Up arrow)"));

    m_stepCombo = new QComboBox(this);
    m_stepCombo->addItem(tr("0.1 kHz"), 0.1);
    m_stepCombo->addItem(tr("1 kHz"), 1.0);
    m_stepCombo->addItem(tr("10 kHz"), 10.0);
    m_stepCombo->addItem(tr("100 kHz"), 100.0);
    m_stepCombo->addItem(tr("1 MHz"), 1000.0);
    m_stepCombo->setCurrentIndex(2);

    m_bandCombo = new QComboBox(this);
    m_bandCombo->setToolTip(tr("Jump to the middle of a band"));

    QHBoxLayout* stepRow = new QHBoxLayout();
    stepRow->setSpacing(4);
    stepRow->addWidget(m_stepCombo, 1);
    stepRow->addWidget(down);
    stepRow->addWidget(up);

    m_rate = new QSlider(Qt::Horizontal, this);
    m_rate->setRange(0, 10);
    m_rate->setPageStep(1);
    m_rate->setToolTip(tr("Time between readings: Fast is as quick as the analyzer "
                          "allows; a slower analyzer is never held to this."));
    m_rateLabel = new QLabel(this);
    m_rateLabel->setMinimumWidth(40);
    m_rateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QHBoxLayout* rateRow = new QHBoxLayout();
    rateRow->setSpacing(4);
    rateRow->addWidget(m_rate, 1);
    rateRow->addWidget(m_rateLabel);

    QFormLayout* form = new QFormLayout();
    form->setVerticalSpacing(4);
    form->addRow(tr("Band"), m_bandCombo);
    form->addRow(tr("Frequency, kHz"), m_edit);
    form->addRow(tr("Step"), stepRow);
    form->addRow(tr("Rate"), rateRow);

    m_start = new QPushButton(tr("Start"), this);
    m_stop = new QPushButton(tr("Stop"), this);
    QFont buttonFont = m_start->font();
    buttonFont.setPointSize(10);
    buttonFont.setBold(true);
    for (QPushButton* b : {m_start, m_stop}) {
        b->setFont(buttonFont);
        b->setFixedHeight(36);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    m_start->setToolTip(tr("Start continuous readings at this frequency"));
    m_stop->setToolTip(tr("Stop (Esc)"));
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(m_start);
    buttons->addWidget(m_stop);

    QVBoxLayout* main = new QVBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(4);
    main->addWidget(line);
    main->addWidget(heading);
    main->addLayout(form);
    main->addSpacing(6);
    main->addLayout(buttons);
    main->addStretch(1);

    connect(m_edit, &QLineEdit::editingFinished, this, [this]() {
        bool ok = false;
        double khz = m_edit->text().remove(' ').toDouble(&ok);
        setFrequencyKHz(ok ? khz : m_khz);
    });
    connect(up, &QToolButton::clicked, this, [this]() { step(+1); });
    connect(down, &QToolButton::clicked, this, [this]() { step(-1); });
    connect(m_stepCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        emit stepChanged(stepKHz());
    });
    connect(m_rate, &QSlider::valueChanged, this, [this](int) {
        showRate();
        emit rateChanged(rateSeconds());
    });
    connect(m_bandCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index > 0 && index - 1 < m_bands.size()) {
            const BandPreset& b = m_bands.at(index - 1);
            setFrequencyKHz((b.fromKHz + b.toKHz) / 2);
        }
        m_bandCombo->setCurrentIndex(0);
    });
    connect(m_start, &QPushButton::clicked, this, &TuningControls::startRequested);
    connect(m_stop, &QPushButton::clicked, this, &TuningControls::stopRequested);

    setBands({});
    showFrequency();
    showRate();
    updateButtons();
}

double TuningControls::stepKHz() const
{
    return m_stepCombo->currentData().toDouble();
}

void TuningControls::setStepKHz(double khz)
{
    int i = m_stepCombo->findData(khz);
    if (i >= 0)
        m_stepCombo->setCurrentIndex(i);
}

int TuningControls::rateSeconds() const
{
    return m_rate->value();
}

void TuningControls::setRateSeconds(int seconds)
{
    m_rate->setValue(qBound(0, seconds, 10));
}

void TuningControls::showRate()
{
    m_rateLabel->setText(m_rate->value() == 0 ? tr("Fast") : tr("%1 s").arg(m_rate->value()));
}

void TuningControls::setFrequencyKHz(double khz, bool notify)
{
    khz = qBound(m_minKHz, std::round(khz * 1000.0) / 1000.0, m_maxKHz); // 1 Hz resolution
    bool changed = khz != m_khz;
    m_khz = khz;
    showFrequency();
    if (notify && changed)
        emit frequencyChanged(m_khz);
}

void TuningControls::setFrequencyLimits(double minKHz, double maxKHz)
{
    m_minKHz = minKHz;
    m_maxKHz = maxKHz;
    setFrequencyKHz(m_khz);
}

void TuningControls::setBands(const QList<BandPreset>& bands)
{
    m_bands = bands;
    m_bandCombo->clear();
    m_bandCombo->addItem(tr("Select a band"));
    for (const BandPreset& b : bands) {
        QString center = QString::number((b.fromKHz + b.toKHz) / 2, 'f', 3);
        while (center.endsWith('0'))
            center.chop(1);
        if (center.endsWith('.'))
            center.chop(1);
        QString name = b.label.isEmpty() ? QString::number(b.fromKHz) : b.label;
        m_bandCombo->addItem(tr("%1 (%2 kHz)").arg(name, center));
    }
}

void TuningControls::setRunning(bool running)
{
    m_running = running;
    updateButtons();
}

void TuningControls::setConnected(bool connected)
{
    m_connected = connected;
    updateButtons();
}

void TuningControls::step(int direction)
{
    setFrequencyKHz(m_khz + direction * stepKHz());
}

void TuningControls::showFrequency()
{
    QString s = QString::number(m_khz, 'f', 3);
    while (s.endsWith('0'))
        s.chop(1);
    if (s.endsWith('.'))
        s.chop(1);
    m_edit->setText(s);
}

void TuningControls::updateButtons()
{
    m_start->setEnabled(m_connected && !m_running);
    m_stop->setEnabled(m_running);
}

// Up/Down in the frequency field step it.
bool TuningControls::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_edit && event->type() == QEvent::KeyPress) {
        int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Up || key == Qt::Key_Down) {
            step(key == Qt::Key_Up ? +1 : -1);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
