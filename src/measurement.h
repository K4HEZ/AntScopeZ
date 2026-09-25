#ifndef MEASUREMENT_H
#define MEASUREMENT_H

// One sweep's data (MeasurementData, core) plus its plot-ready QCustomPlot
// containers.

#include <QString>
#include <QStringList>
#include <QVector>
#include <qcustomplot.h>
#include "measurementdata.h"

struct measurement : MeasurementData
{
    bool visible = true; // table checkbox
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
