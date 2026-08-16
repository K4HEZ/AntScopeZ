#ifndef MARKERS_H
#define MARKERS_H

#include <QObject>
#include <qcustomplot.h>
#include <popup.h>
#include <markerspopup.h>
#include <QSettings>
#include <analyzer/analyzerparameters.h>
#include <settings.h>
#include <measurements.h>

#define MAX_MARKERS 5

struct marker
{
    double frequency;
    QCPItemStraightLine *swrLine = NULL;//QCPItemTracer *swrTracer;
    QCPItemStraightLine *phaseLine = NULL;
    QCPItemStraightLine *rsLine = NULL;
    QCPItemStraightLine *rpLine = NULL;
    QCPItemStraightLine *rlLine = NULL;
    QCPItemStraightLine *s21Line = NULL;
//    QCPItemStraightLine *smithTracer = NULL;
    QCPItemText *swrLineText = NULL;
    QCPItemText *phaseLineText = NULL;
    QCPItemText *rsLineText = NULL;
    QCPItemText *rpLineText = NULL;
    QCPItemText *rlLineText = NULL;
    QCPItemText *s21LineText = NULL;
//    QCPItemText *smithTracerText = NULL;

    // Deleting a QCPAbstractItem directly is not enough: ~QCPAbstractItem only
    // drops the item from its layer, leaving a dangling pointer in
    // QCustomPlot::mItems. That list is walked by itemAt()/selectedItems() on
    // every plot mouse event, and again by ~QCustomPlot() via clearItems(),
    // which would delete the item a second time. QCustomPlot::removeItem()
    // deregisters and deletes in one step, so always go through it.
    static void removeFromPlot(QCPAbstractItem *item)
    {
        if(item && item->parentPlot())
            item->parentPlot()->removeItem(item);
    }

    void clear()
    {
        removeFromPlot(swrLine);       swrLine = NULL;
        removeFromPlot(phaseLine);     phaseLine = NULL;
        removeFromPlot(rsLine);        rsLine = NULL;
        removeFromPlot(rpLine);        rpLine = NULL;
        removeFromPlot(rlLine);        rlLine = NULL;
        removeFromPlot(s21Line);       s21Line = NULL;
        removeFromPlot(swrLineText);   swrLineText = NULL;
        removeFromPlot(phaseLineText); phaseLineText = NULL;
        removeFromPlot(rsLineText);    rsLineText = NULL;
        removeFromPlot(rpLineText);    rpLineText = NULL;
        removeFromPlot(rlLineText);    rlLineText = NULL;
        removeFromPlot(s21LineText);   s21LineText = NULL;
    }
};

class Markers : public QObject
{
    Q_OBJECT
public:
    explicit Markers(QObject *parent = 0);
    ~Markers();

    void setWidgets(QCustomPlot * swr, QCustomPlot * phase, QCustomPlot * rs, QCustomPlot * rp,
                    QCustomPlot * rl, QCustomPlot * tdr, QCustomPlot * s21, QCustomPlot * smith);
    void setMeasurements(Measurements *m);
    void create(double fq);
    void setFq(double fq);
    void add();
    bool getMarkersHintEnabled(void);
    void saveBmp(QString path);
    QList <QStringList> getMarkersHintList();
    qint32 getMarkersCount();
    marker getMarker( quint32 number);
    void repaint();
    void on_translate();
    void changeColorTheme();
    void changeMarkersHint();
    MarkersPopUp * markersHint() { return m_markersHint; }
    QList<QList<QVariant>> updateInfo(QList<int> _columnTypes);
    // Single marker, most recent measurement -- see definition in
    // markers.cpp for why this exists alongside updateInfo().
    QList<QVariant> valuesForMarkerNumber(int markerNumber, const QList<int>& columnTypes);
    bool markersHintEnabled() { return m_markersHintEnabled; }

private:
    QCustomPlot *m_swrWidget;
    QCustomPlot *m_phaseWidget;
    QCustomPlot *m_rsWidget;
    QCustomPlot *m_rpWidget;
    QCustomPlot *m_rlWidget;
    QCustomPlot *m_tdrWidget;
    QCustomPlot *m_s21Widget;
    QCustomPlot *m_smithWidget;

    QVector <marker*> m_markersList;

    MarkersPopUp * m_markersHint;

    QString m_currentTab;

    bool m_markersHintEnabled;

    QSettings * m_settings;

    Measurements *m_measurements;

    bool m_focus;

    double interpolate(double fq1, double fq2, double fq3, double param1, double param2);
    // Row body shared by updateInfo() (all markers x all measurements) and
    // valuesForMarkerNumber() (one marker, most recent measurement only).
    QList<QVariant> computeMarkerRow(double fq0, int markerNumber, int measurementIndex, const QList<int>& columnTypes);

signals:

public slots:
    void on_focus(bool focus);
    void on_mainWindowPos(int x, int y);
    void on_currentTab(QString name);
    void on_newMeasurement(QString);
    void on_measurementComplete();
    void setMarkersHintEnabled(bool enabled);
    void redraw(void);
    void rescale();
    void on_removeMarker(int number);
};

#endif // MARKERS_H
