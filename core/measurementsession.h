#ifndef MEASUREMENTSESSION_H
#define MEASUREMENTSESSION_H

#include <QObject>
#include "measurementlist.h"

class AnalyzerPro;
class Calibration;

// UI-free owner of a scan session: listens to an AnalyzerPro and keeps the
// resulting measurements, applying the same MeasurementList rules the
// desktop uses. For front ends without the desktop GUI (mobile, headless,
// remote API). The desktop's Measurements shares the rules, not this object.
class MeasurementSession : public QObject
{
    Q_OBJECT
public:
    explicit MeasurementSession(QObject* parent = nullptr);

    void attach(AnalyzerPro* analyzer);
    void setCalibration(Calibration* calibration) { m_calibration = calibration; }
    void setMaxMeasurements(int count) { m_maxMeasurements = qMax(1, count); }

    const MeasurementList<MeasurementData>& measurements() const { return m_list; }
    void remove(int row);
    void rename(int row, const QString& name);
    void clearDirty(int row);

public slots:
    void onNewMeasurement(QString name, qint64 from, qint64 to, qint32 dots);
    void onContinueMeasurement(qint64 from, qint64 to, qint32 dots);
    void onNewData(RawData raw);
    void onNewS21Data(S21Data data);
    void onNewSParamPoint(SParamPoint sp);
    void onNewUserDataHeader(QStringList fields);
    void onNewUserData(RawData raw, UserData user);
    void onMeasurementComplete();

signals:
    void measurementAdded(int row);
    void measurementRemoved(int row);
    void pointAdded(int row, const RawData& raw);
    void measurementFinished(int row);
    void measurementChanged(int row); // renamed / saved

private:
    bool accepting() const { return m_list.inProgress() && m_list.accepting(); }

    MeasurementList<MeasurementData> m_list;
    AnalyzerPro* m_analyzer = nullptr;
    Calibration* m_calibration = nullptr;
    int m_maxMeasurements = 5;
};

#endif // MEASUREMENTSESSION_H
