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
    // Called after a single/full scan completes (never during Continuous --
    // see the call sites in MainWindow::on_measurementComplete()/
    // on_measurementCompleteNano(), which only reach this on the
    // non-Continuous path). If g_autoMarkerAtLowestSwr is on and a marker
    // slot is free, places an ordinary marker (same create()/setFq()/add()
    // as a user-placed one -- nothing marks it as "auto") at the lowest-SWR
    // point of the trace the user is currently looking at (calibrated/
    // far-end-adjusted per Measurements' own settings, same selection
    // qFactorAt() in MarkerComparisonDialog uses). No-ops silently if no
    // slot is free or there's no SWR data to search.
    void autoPlaceAtLowestSwr();

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
    // Stand-in row body for updateInfo() when there's no measurement yet to
    // read from -- unlike computeMarkerRow(), doesn't touch m_measurements
    // at all, so it's safe to call with zero measurements. Only fieldMarker/
    // fieldFQ are known this early; everything else comes back invalid,
    // which MarkersPopUp::formatText() already renders as blank.
    QList<QVariant> emptyMarkerRow(double fq0, int markerNumber, const QList<int>& columnTypes);

signals:
    // Emitted by add()/on_removeMarker() whenever the marker list itself
    // changes -- distinct from on_measurementComplete(), which fires for a
    // new sweep and routes through changeMarkersHint() just like they do,
    // but on every scan tick rather than on add/remove (so it deliberately
    // doesn't also emit this). MarkerComparisonDialog listens for this so
    // its marker combos stay in sync without needing a fresh scan.
    void markersChanged();

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
