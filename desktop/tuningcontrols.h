#ifndef TUNINGCONTROLS_H
#define TUNINGCONTROLS_H

#include <QWidget>
#include <QList>
#include "itubands.h"

class QLineEdit;
class QComboBox;
class QPushButton;
class QSlider;
class QLabel;

// Tuning mode's left-column page: frequency entry with step buttons, a
// band quick-pick, and its own Start/Stop.
class TuningControls : public QWidget
{
    Q_OBJECT

public:
    explicit TuningControls(QWidget* parent = nullptr);

    double frequencyKHz() const { return m_khz; }
    double stepKHz() const;
    // 0 = as fast as the device allows, else seconds between readings.
    int rateSeconds() const;
    void setRateSeconds(int seconds);
    bool isRunning() const { return m_running; }
    // notify=false just updates the field (no frequencyChanged()).
    void setFrequencyKHz(double khz, bool notify = true);
    void setStepKHz(double khz);
    void setFrequencyLimits(double minKHz, double maxKHz);
    void setBands(const QList<BandPreset>& bands);
    void setRunning(bool running);
    void setConnected(bool connected);

signals:
    void frequencyChanged(double khz);
    void stepChanged(double khz);
    void rateChanged(int seconds);
    void startRequested();
    void stopRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void step(int direction);
    void showFrequency();
    void updateButtons();
    void showRate();

    QLineEdit* m_edit;
    QComboBox* m_stepCombo;
    QComboBox* m_bandCombo;
    QSlider* m_rate;
    QLabel* m_rateLabel;
    QPushButton* m_start;
    QPushButton* m_stop;
    QList<BandPreset> m_bands;
    double m_khz = 7000;
    double m_minKHz = 0.1;
    double m_maxKHz = 10000000;
    bool m_running = false;
    bool m_connected = false;
};

#endif // TUNINGCONTROLS_H
