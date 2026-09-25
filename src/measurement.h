#ifndef MEASUREMENT_H
#define MEASUREMENT_H

// One sweep's data plus its plot-ready QCustomPlot containers. Split out of
// analyzer/analyzerparameters.h so the analyzer layer doesn't need
// QCustomPlot (and with it, Widgets).

#include <QString>
#include <QStringList>
#include <QVector>
#include <qcustomplot.h>
#include <analyzer/analyzerparameters.h>

struct measurement
{
    QString name;
    // Stable identity assigned once at creation (Measurements::
    // nextSerialNumber()) -- covers every measurement, scan or loaded file
    // alike, and is what both the Measurements table's own "#" column and
    // the Markers panel's "#" column show. Replaced an earlier "01>"/"02>"
    // prefix baked into scan names for the same purpose (removed 2026-09-17
    // -- it only ever covered scans, not loaded files, and its own counter
    // could disagree with this one once both existed side by side).
    int serialNumber = 0;
    bool visible = true;
    // True if this measurement's on-disk copy (if any) doesn't match what's
    // in memory -- defaults true (a live scan), set false on load from a
    // file (Measurements::loadData()/importData()) or a successful save
    // (any format -- Export's format buttons all funnel through Measurements::
    // clearDirty()), set true again on rename (Measurements::
    // renameMeasurement()). Not affected by changing a measurement's color
    // (display preference, not data) or by a cancelled save. Shown as a
    // trailing " *" in the Points column (Measurements::pointsCellText()).
    bool dirty = true;
    qint64 qint64From;
    qint64 qint64To;
    qint64 qint64Dots;
    void set(qint64 _qint64From, qint64 _qint64To, qint64 _qint64Dots) {
        qint64From = _qint64From; qint64To = _qint64To; qint64Dots =_qint64Dots;
    }

    QVector <S21Data> dataS21;
    QVector <SParamPoint> dataSParam; // real complex 2-port data, from .s2p import -- see SParamPoint's own comment
    QVector <RawData> dataRX;
    QVector <UserData> dataUser;
    QStringList fieldsUser;
//---------------------------------
    QCPGraphDataContainer swrGraph;
    QCPGraphDataContainer phaseGraph;
    QCPGraphDataContainer rhoGraph;
    QCPGraphDataContainer rsrGraph;
    QCPGraphDataContainer rsxGraph;
    QCPGraphDataContainer rszGraph;
    QCPGraphDataContainer rprGraph;
    QCPGraphDataContainer rpxGraph;
    QCPGraphDataContainer rpzGraph;
    QCPGraphDataContainer rlGraph;
    QCPGraphDataContainer s21Graph;
    QCPGraphDataContainer s21StageGraph;
    // Derived from dataSParam (.s2p import), not from S21Data/s21Graph
    // above -- magnitude in dB, phase in degrees.
    QCPGraphDataContainer s21MagGraph;
    QCPGraphDataContainer s21PhaseGraph;
    QCPGraphDataContainer s12MagGraph;
    QCPGraphDataContainer s12PhaseGraph;
    QCPGraphDataContainer tdrImpGraph;
    QCPGraphDataContainer tdrStepGraph;
    QCPGraphDataContainer tdrZGraph;
    QCPGraphDataContainer tdrImpGraphFeet;
    QCPGraphDataContainer tdrStepGraphFeet;
    QCPGraphDataContainer tdrZGraphFeet;
    QVector<QCPGraphDataContainer*> userGraphs;

    QCPCurve *smithCurve;
    QCPCurveDataContainer smithGraph;
    QCPCurveDataContainer smithGraphView;
//---------------------------------
//---------------------------------
//---------------------------------
    QVector <RawData> dataRXCalib;
    QCPGraphDataContainer swrGraphCalib;
    QCPGraphDataContainer phaseGraphCalib;
    QCPGraphDataContainer rhoGraphCalib;
    QCPGraphDataContainer rsrGraphCalib;
    QCPGraphDataContainer rsxGraphCalib;
    QCPGraphDataContainer rszGraphCalib;
    QCPGraphDataContainer rprGraphCalib;
    QCPGraphDataContainer rpxGraphCalib;
    QCPGraphDataContainer rpzGraphCalib;
    QCPGraphDataContainer rlGraphCalib;
    QCPCurveDataContainer smithGraphCalib;
    QCPCurveDataContainer smithGraphViewCalib;
};

#endif // MEASUREMENT_H
