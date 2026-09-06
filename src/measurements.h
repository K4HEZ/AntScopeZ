#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include <QObject>
#include <QVector>
#include <QLabel>
#include <math.h>
#include <qdebug.h>
#include <qcustomplot.h>
#include <analyzer/analyzerparameters.h>
#include <popup.h>
#include <QSettings>
#include <calibration.h>
#include <ctime>
#include <complex>
#include <settings.h>
#include <float.h>

#include "ProgressDlg.h"

#include "onefqwidget.h"
#include "onefqbigreadout.h"
#include "CustomPlot.h"

#define MAX_MEASUREMENTS 5

#define TDR_MINPOINTS 200

#define TDR_MAXARRAY 20000
#define TDR_MAXPOINTS 1000

//#define TDR_MAXARRAY 32768
//#define TDR_MAXPOINTS 2000

//#define TDR_MAXARRAY 65536
//#define TDR_MAXPOINTS 4000

// Floor for the TDR scan-setup panel's top-frequency control (kHz, matching
// AnalyzerParameters' own minFq()/maxFq() unit convention) -- a too-narrow
// bandwidth makes calcTdrEstimateRaw()'s 1/(maxfq-minfq) term blow up.
// Placeholder value, not yet tuned against real hardware -- see the
// tdr-scan-rework-plan memory.
#define TDR_MIN_FREQUENCY 5000

// One-sided taper applied to a TDR scan's frequency-domain data before the
// inverse FFT -- full gain at DC, tapering toward the window's minimum at
// the top of the measured band (see tdrWindowCoeff()'s comment in
// measurements_tdr.cpp for why only one edge needs tapering here, unlike a
// typical centered FFT window). Was hardcoded to (an unlabeled) Hamming
// until 2026-08-21; default stays Hamming so existing behavior doesn't
// change until a user picks something else.
enum class TdrWindow { Rectangular, Hamming, Hann, Blackman, Kaiser };


#ifndef SPEEDOFLIGHT
#define SPEEDOFLIGHT 299792458.0
#endif

#ifndef FEETINMETER
#define FEETINMETER 3.2808399
#endif

#ifndef DBL_MAX
#define DBL_MAX 1.797693134862315e+308
#endif

// User-settable now (Settings > General: "Selected/other measurement line
// width"), default to the old hardcoded 5/2 -- see measurements.cpp's
// g_activeGraphPenWidth/g_inactiveGraphPenWidth definitions. Left as
// macros, not a straight rename to the globals, so every existing call
// site (mainwindow.cpp/mainwindow_measurements_io.cpp/measurements.cpp)
// keeps compiling unchanged.
extern int g_activeGraphPenWidth;
extern int g_inactiveGraphPenWidth;
#define ACTIVE_GRAPH_PEN_WIDTH g_activeGraphPenWidth
#define INACTIVE_GRAPH_PEN_WIDTH g_inactiveGraphPenWidth

typedef std::complex <double> Complex;

class Measurements : public QObject
{
    Q_OBJECT
public:
    explicit Measurements(QObject *parent = 0);
    ~Measurements();

    void setWidgets(CustomPlot * swr, CustomPlot * phase, CustomPlot * rs, CustomPlot * rp,
                    CustomPlot * rl, CustomPlot * tdr, CustomPlot * s21, QCustomPlot * smith, QTableWidget *table);
    void setUserWidget(CustomPlot * user);
    // box/nameLabels/valueLabels live in mainwindow.ui's docked middle
    // column (see the UI-overhaul branch) -- this just hands over the
    // pointers Measurements needs to keep them updated, replacing the
    // PopUp this used to self-construct. box is what showHideHints()
    // shows/hides (collapses the whole panel, not just the text);
    // nameLabels/valueLabels are a fixed-size pool of pre-built
    // QFormLayout rows (mainwindow.ui's graphHintName0../graphHintValue0..)
    // -- setGraphHintFields() below fills as many as the current tab
    // needs and hides the rest, rather than formatting one big string
    // into a single QLabel (see the label/value-columns redesign).
    void setGraphHintWidgets(QWidget* box, const QList<QLabel*>& nameLabels, const QList<QLabel*>& valueLabels);
    void setCalibration(Calibration * _calibration);
    bool getCalibrationEnabled(void);
    void deleteRow(int row);
    // S21 tab's legend shows only the currently-selected measurement's 4
    // traces (S21/S12 dB+deg), not every measurement at once -- with
    // several measurements loaded, a legend row per trace per measurement
    // was unreadable (see the 2026-09-04 todo.txt entry this replaces).
    // row is a plain m_measurements/table index (0=oldest), matching
    // on_tableWidget_measurments_cellClicked()'s "row"; pass -1 (or an
    // out-of-range row, e.g. after the last measurement is deleted) to
    // just clear the legend.
    void updateS21Legend(int row);
    void setFarEndMeasurement (qint32 mode) { m_farEndMeasurement=mode; }
    qint32 getFarEndMeasurement (void) {return m_farEndMeasurement;}
    // getMeasurement(number) indexes backwards from the newest entry --
    // new scans are appended (see on_newMeasurement()), so number=0 is the
    // just-completed scan and larger number walks back into history. last()
    // must therefore pass 0, not getMeasurementLength()-1 (which is the
    // OLDEST retained scan once more than one is in history -- see
    // callers that keep their own "mostRecent" index for the far-end Sub/
    // Add case and need the same fix: qFactorAt() in
    // markercomparisondialog.cpp, valuesForMarkerNumber() and
    // autoPlaceAtLowestSwr() in markers.cpp).
    measurement* last() {return (isEmpty() ? nullptr : getMeasurement(0)); }
    measurement* getMeasurement(int number) {return &m_measurements[m_measurements.length()-1 - number];}
    measurement* getMeasurementSub(int number) {return &m_farEndMeasurementsSub[m_farEndMeasurementsSub.length()-1 - number];}
    measurement* getMeasurementAdd(int number) {return &m_farEndMeasurementsAdd[m_farEndMeasurementsAdd.length()-1 - number];}
    measurement* getMeasurementView(int number) {return &m_viewMeasurements[m_measurements.length()-1 - number];}
    qint32 getMeasurementLength(void) {return m_measurements.length();}
    bool isEmpty() { return getMeasurementLength() == 0; }
    bool getGraphHintEnabled(void);
    bool getGraphBriefHintEnabled(void);
    void saveData(quint32 number, QString path);
    void loadData(QString path);
    int  nextPrefix();

    void exportData(QString _name, int _type, int _number, bool _applyCable=false, QString _description=QString());
    // 2-port Touchstone (.s2p) export -- straight from dataSParam (real
    // S11/S21/S12/S22, already Z0-normalized at import time), not derived
    // from dataRX like exportData() above. Silently does nothing if the
    // selected measurement has no 2-port data (see Export::updateDetails(),
    // the only caller, which gates the S2P button on that same check).
    // _type: 0 = S,RI, 1 = S,MA, 2 = S,DB (same three choices exportData()
    // offers for 1-port S-parameter export; no Z type -- see the .cpp for
    // why).
    void exportSParamData(QString _name, int _type, int _number, QString _description=QString());
    void importData(QString _name);
    void importData(QString _name, bool user_format);

    double getZ0(void) const{ return m_Z0;}
    void setZ0(double _Z0);

    int CalcTdr(QVector<RawData> *data);
    void FFT(float real[], float imag[], int length, int Inverse = 0);
    int calcTdrDist(QVector<RawData> *data);

    // Pure function of scan parameters -- no FFT execution, no side effects,
    // no member state read besides what's passed in. Used by CalcTdr() (fed
    // real captured asize/minfq/maxfq) and calcTdrDist() (ditto) so neither
    // duplicates this math anymore, and by the TDR scan-setup panel's live
    // preview via the calcTdrEstimate() overload below, fed *planned*
    // dots/top-frequency before any scan runs -- same formula either way, so
    // the preview and the real post-scan numbers can't drift apart. See the
    // tdr-scan-rework-plan memory for the derivation/verification of both
    // fields below.
    struct TdrEstimate {
        // Today's original TDR-range formula (behavior unchanged), hand-
        // verified 2026-08-20 against the textbook max-unambiguous-range
        // formula (c x VF x (N-1) / (2 x BW)) -- matches closely. Depends on
        // dot count (asize) as well as bandwidth, via the FFT-size rounding
        // below. Same units as the TDR chart's x-axis (m or ft, per `metric`).
        double unambiguousRange = 0;
        // True physical resolving power -- c x VF / (2 x BW) -- depends only
        // on bandwidth and velocity factor, not dot count. NOT the same thing
        // as the chart's own per-point spacing (unambiguousRange / fftSize,
        // see chartStep below), which is ~8x finer due to zero-padding and
        // does not represent genuine resolving power. Same units as
        // unambiguousRange.
        double resolution = 0;
        // The legacy m_tdrResolution formula's raw (pre unit-conversion)
        // value -- kept only so CalcTdr()'s m_tdrResolution member stays
        // bit-for-bit identical to before this refactor. Not otherwise
        // meaningful; use `resolution` above instead.
        double chartStep = 0;
        int fftSize = 0; // iTdrFftSize
    };
    static TdrEstimate calcTdrEstimateRaw(int asize, double minfqMHz, double maxfqMHz,
                                           double velFactor, bool metric);
    // Convenience overload for a live preview before any scan has run --
    // asize/minfq synthesized as dots+1 / 0 (a real device returns
    // dotsNumber+1 points per request -- see AnalyzerPro::startStitchedMeasure()'s
    // comment -- and TDR requires minfq within 0.1MHz of DC, see CalcTdr()'s
    // own "Wrong fq" guard).
    static TdrEstimate calcTdrEstimate(int dots, double topFreqMHz, double velFactor, bool metric);

    // Not QSettings-backed (see tdr-scan-rework-plan memory -- the scan
    // panel's controls are meant to reset to sane defaults each time it
    // opens, not persist). CalcTdr() reads these; the panel calls the
    // setters, then triggers a plain redrawTDR() to re-plot already-
    // captured data with the new window -- no rescan needed.
    TdrWindow tdrWindowType() const { return m_tdrWindowType; }
    void setTdrWindowType(TdrWindow type) { m_tdrWindowType = type; }
    double tdrKaiserBeta() const { return m_tdrKaiserBeta; }
    void setTdrKaiserBeta(double beta) { m_tdrKaiserBeta = beta; }

    // Pure function -- see measurements_tdr.cpp for the full comment on what
    // this computes and why. Public (not just CalcTdr()'s internal detail)
    // so a future window-shape preview (e.g. a small sparkline in the scan
    // panel) can call it directly without re-running a scan.
    static double tdrWindowCoeff(TdrWindow type, double beta, int i, int n);

    int CalcTdr2(QVector <RawData> *data);
    qint16 DTF_FindRadix2Length(qint16 length, int *log2N);
    void FFT2(double *Rdat, double *Idat, int N, int LogN, int Ft_Flag);

    void setCableVelFactor(double value);
    // Current live value (Settings > Cable, propagated via
    // MainWindow::on_settingsParamsChanged()) -- what redrawTDR() actually
    // used to turn the last TDR scan's raw data into the distance-keyed
    // tdrImpGraph/tdrZGraph/etc. QCPGraphDataContainers. findTdrPeak() (and, via that,
    // TdrScanPanel) reads this to rescale those already-computed distances
    // to whatever velocity factor is locally being tried, without
    // re-running the FFT. MainWindow::on_tdrScanRequested() also
    // temporarily overrides this for the duration of one TDR scan -- see
    // its own comment for why that's safe without disturbing feedline-loss
    // compensation elsewhere, which reads this same value.
    double cableVelFactor() const { return m_cableVelFactor; }

    struct TdrPeak {
        bool found = false;
        double distance = 0; // in whatever unit `metric` (findTdrPeak()'s param) asked for
        double amplitude = 0; // signed -- see Markers/CalcTdr's Gre-based reflection amplitude
        // Approximate impedance (Ohms) at the peak's distance -- read
        // straight from tdrZGraph/tdrZGraphFeet (already computed by
        // CalcTdr(), already plotted on the TDR chart, just never
        // surfaced as a number before 2026-08-21). Not rescaled by
        // localVf like distance is -- Z doesn't depend on velocity
        // factor, only on the reflection coefficient at that point.
        double impedanceOhms = 0;
        // True if the peak sits in the last ~5% of this scan's unambiguous
        // range -- the real reflection may be farther out than this scan
        // can resolve. See findTdrPeak()'s own comment.
        bool nearRangeEdge = false;
    };
    // Strongest reflection in the most recent TDR scan's trace, in
    // whatever unit `metric` asks for (ft or m -- same choice redrawTDR()
    // makes for the chart). Distance comes back already rescaled from
    // cableVelFactor() (whatever velocity factor was active when that
    // scan's data was computed) to `localVf` -- see the .cpp for why that
    // rescale is valid without re-running the FFT. Moved here from
    // TDRAnalysisDialog 2026-08-21 when it merged into TdrScanPanel -- this
    // is TDR-trace analysis, not dialog logic, so it belongs on
    // Measurements rather than duplicated/copied into whatever UI reads it.
    TdrPeak findTdrPeak(bool metric, double localVf);
    void setCableResistance(double value);
    void setCableLossConductive(double value);
    void setCableLossDielectric(double value);
    void setCableLossFqMHz(double value);
    void setCableLossUnits(int value);
    void setCableLossAtAnyFq(bool value);
    void setCableLength(double value);
    void setCableFarEndMeasurement(int value);
    void on_translate();
    int getBaseUserGraphIndex(int row);
    void startTDRProgress(QWidget* _parent, int _dots);
    void updateTDRProgress(int _dots);
    void stopTDRProgress();
    void startAutocalibrateProgress(QWidget* _parent, int _dots);
    void updateAutocalibrateProgress(int _dots, QString _msg);
    void stopAutocalibrateProgress();
    bool isOneFqMode() { return m_oneFqMode; }
    void setAutoCalibration(int _mode) { m_autoCalibration=_mode; } // 0-NONE, 1-R,L(old AA-1400), 2-C,L(new AA-230 ZOOM)
    int  getAutoCalibration() { return m_autoCalibration; }
    QPair<double, double> autoCalibrate();
    void interrupt() { m_interrupted = true; }
    bool isTDRMode() { return m_tdrProgressDlg != nullptr; }
    void setContinuous(bool _state) { m_isContinuing = _state; m_currentPoint = 0; }
    ProgressDlg* progressDlg() { return m_autoCalibrateProgressDlg; }
    // resetRange=false skips resetting the X axis's upper bound to the
    // scan's full unambiguous range -- default (true) is every existing
    // caller's behavior, unchanged (a new scan or picking a different
    // measurement row genuinely has no established "area of interest" yet,
    // so resetting there is correct). Added so a window-function change can
    // re-plot in place without undoing whatever the user had zoomed/panned
    // to -- see MainWindow's windowChanged() handler.
    void redrawTDR(int _index=-1, bool resetRange=true);
    void drawSmithImage(void);
    void setSmithBackgroundColor(QColor color);
    void setSmithForegroundColor(QColor color);
    void setBriefHintColor();
    QColor chartBackgroundColor();
    QColor inverseChartBackground();
    double tdrZRange() { return m_tdrZRange; }
    QString currentTab() { return m_currentTab; }
    PopUp* graphBriefHint() { return m_graphBriefHint; }

private:
//    QVector <rawData> m_rawDataVector;
//    QVector < QVector<rawData> > m_rawDataLists;
//    QVector <QString> m_tableNames;

    QString m_currentTab;

    // Running phase-unwrap state for on_newSParamPoint()'s live S21
    // capture -- unlike populateSParamData()'s batch import (which
    // unwraps across a whole already-collected list in one call), a live
    // scan arrives one point at a time, so this state has to persist
    // across calls instead of living on the stack. Reset per measurement
    // in on_newMeasurement().
    bool m_liveS21PhaseHavePrev = false;
    double m_liveS21PhasePrevRaw = 0;
    double m_liveS21PhasePrevUnwrapped = 0;

//    measurement m_measurements[MAX_MEASUREMENTS];
    QList <measurement> m_measurements;
    QList <measurement> m_viewMeasurements;
    QList <measurement> m_farEndMeasurementsAdd;
    QList <measurement> m_farEndMeasurementsSub;

    CustomPlot *m_swrWidget;
    CustomPlot *m_phaseWidget;
    CustomPlot *m_rsWidget;
    CustomPlot *m_rpWidget;
    CustomPlot *m_rlWidget;
    CustomPlot *m_tdrWidget;
    CustomPlot *m_s21Widget;
    QCustomPlot *m_smithWidget;
    QCPCurve *m_smithInnerCircle = nullptr;
    // drawSmithImage()'s arcs/labels are never registered via
    // QCustomPlot::addPlottable()/addItem() (they render fine regardless --
    // QCPLayerable self-registers with its layer independently of that list),
    // so m_smithWidget->plottableCount()/itemCount() can't see them. Keep our
    // own references so setSmithForegroundColor() has something to recolor.
    QList<QCPCurve*> m_smithArcs;
    QList<QCPItemText*> m_smithLabels;
    QTableWidget *m_tableWidget;
    CustomPlot *m_userWidget;

    qint32 m_currentIndex;

    QWidget *m_graphHintBox;
    QList<QLabel*> m_graphHintNameLabels;
    QList<QLabel*> m_graphHintValueLabels;
    PopUp *m_graphBriefHint;

    QCPItemStraightLine *m_swrLine;
    QCPItemStraightLine *m_swrLine2;
    QCPItemStraightLine *m_phaseLine;
    QCPItemStraightLine *m_phaseLine2;
    QCPItemStraightLine *m_rsLine;
    QCPItemStraightLine *m_rpLine;
    QCPItemStraightLine *m_rlLine;
    QCPItemStraightLine *m_rlLine2;
    QCPItemStraightLine *m_s21Line;
    QCPItemStraightLine *m_s21Line2;
    QCPItemStraightLine *m_tdrLine;

    QSettings * m_settings;
    Calibration * m_calibration;

    bool m_graphHintEnabled;
    bool m_graphBriefHintEnabled;

    volatile bool m_calibrationMode;

    double m_Z0;

    double *m_pdTdrImp;
    double *m_pdTdrStep;
    double *m_pdTdrZ;

    double m_tdrResolution;
    double m_tdrRange;
    double m_tdrZRange = 0;
    TdrWindow m_tdrWindowType = TdrWindow::Hamming; // default matches pre-2026-08-21 hardcoded behavior
    double m_tdrKaiserBeta = 6.0; // only used when m_tdrWindowType == Kaiser

    ProgressDlg* m_tdrProgressDlg = nullptr;

    ProgressDlg* m_autoCalibrateProgressDlg = nullptr;
    qint32 m_dotsNumber=0;
    quint32 m_tdrDots=0;

//    bool m_calibrationEnabled;
    bool m_measureSystemMetric;

    double m_cableVelFactor;
    double m_cableResistance;
    double m_cableLossConductive;
    double m_cableLossDielectric;
    double m_cableLossFqMHz;
    qint32 m_cableLossUnits;
    qint32 m_cableLossAtAnyFq;
    double m_cableLength;
    qint32 m_farEndMeasurement;
    QCPItemEllipse * m_smithTracer;

    bool m_focus;

    bool m_oneFqMode = false;
    qint64 m_oneFqStartTime;
    // Both constructed together on entering One-Fq mode and kept alive
    // for the whole session -- toggleOneFqDisplayStyle() only ever
    // hide()s/show()s between them, never destroys/recreates one mid-
    // session. Originally this destroyed and rebuilt whichever style
    // wasn't active on every toggle; that crashed deep in Qt's AT-SPI
    // accessibility bridge the moment the replacement widget's native
    // window was created (confirmed via core dump 2026-08-24, SIGSEGV in
    // QAccessibleWidget::text() during QWidget::create() -- reproduced
    // whether the recreate ran inline or deferred via
    // QTimer::singleShot(0, ...), so it wasn't a re-entrancy problem,
    // just rapid native-window churn). Keeping both alive for the session
    // means a toggle is just a visibility flip on already-created native
    // windows, sidestepping that whole class of bug -- and as a bonus,
    // both stay fed with live data the whole time (see
    // updateOneFqWidget()), so switching styles never shows stale values.
    OneFqWidget* m_oneFqWidget = nullptr;
    OneFqBigReadout* m_oneFqBigReadout = nullptr;
    // Which one is currently visible, ini-persisted (sticky across
    // sessions, see constructor/destructor). Flipped by
    // toggleOneFqDisplayStyle().
    OneFqDisplayStyle m_oneFqDisplayStyle = OneFqDisplayStyle::Detailed;
    // Graph-hint enabled/short-hint-enabled flags, saved once on entering
    // One-Fq mode and restored once on leaving it. Centralized here
    // (rather than on whichever display widget happens to be active, as
    // before OneFqBigReadout existed) so the flags survive a style swap
    // mid-session instead of being re-captured -- already-suppressed --
    // from the outgoing widget.
    QPair<bool, bool> m_oneFqSavedHints{true, true};

    int m_autoCalibration = 0; // 1-R,L(old AA-1400), 2-C,L(new AA-230 ZOOM)
    bool m_interrupted = false;
    bool m_RangeMode = false;
    bool m_measuringInProgress = false;
    bool m_isContinuing = false;
    int m_previousI = 0;
    int m_currentPoint = 0;

    quint32 computeSWR(double freq, double Z0, double R, double X, double *VSWR, double *RL);
    double computeZ (double R, double X);

    // Constructs both display widgets (called once, entering One-Fq mode)
    // and wires their signals. Doesn't show either -- see
    // updateOneFqDisplayVisibility().
    void createOneFqDisplayWidgets(QWidget* parent, int dots);
    // Shows whichever of m_oneFqWidget/m_oneFqBigReadout matches
    // m_oneFqDisplayStyle, hides the other. The entire effect of
    // toggleOneFqDisplayStyle() once both widgets exist.
    void updateOneFqDisplayVisibility();
    // Tears down and nulls both m_oneFqWidget/m_oneFqBigReadout (end of
    // session, not a style toggle -- see updateOneFqDisplayVisibility()
    // for that). Doesn't touch m_oneFqMode/hint-flag state -- see
    // endOneFqMode() for that half.
    void destroyOneFqDisplayWidgets();
    // Pure One-Fq-mode state teardown (restores the saved hint flags,
    // clears m_oneFqMode/m_isContinuing, notifies listeners). Deliberately
    // doesn't touch either display widget's lifecycle -- see
    // hideOneFqWidget() vs. onOneFqBigReadoutClosing() for why "who closes
    // the widget" differs by caller.
    void endOneFqMode();
    // OneFqBigReadout tells us (via its closing() signal) that it's
    // already closing itself (title-bar X, or Esc while it has focus) --
    // ends the whole session and takes down its sibling OneFqWidget, but
    // must not close()/delete OneFqBigReadout itself again here; just
    // drop our now-stale pointer, its own Qt::WA_DeleteOnClose handles
    // the rest.
    void onOneFqBigReadoutClosing();

    // Fills as many of the graph-hint panel's fixed row pool
    // (m_graphHintNameLabels/m_graphHintValueLabels) as fields needs --
    // one label:value pair per row, in order -- and hides any leftover
    // rows beyond that (a hidden row takes no layout space, same as
    // everywhere else this app relies on that QLayout behavior). Replaces
    // building one big formatted string for a single QLabel; each tab's
    // updatePopUp()/on_newCursorSmithPos() branch builds its own field
    // list (different tabs show different quantities) instead.
    void setGraphHintFields(const QList<QPair<QString, QString>>& fields);
    // Labels-only, empty-values placeholder shown before any real cursor
    // position/measurement exists yet -- shared by setGraphHintWidgets()
    // (initial state) and on_translate() (language switch, since these
    // rows aren't covered by ui->retranslateUi()'s usual static-widget
    // handling).
    void setGraphHintPlaceholder();

    void NormRXtoSmithPoint(double Rnorm, double Xnorm, double &x, double &y);
    void calcFarEnd(bool _incrementally=false);
    RawData calcFarEnd(const RawData& data, int idx, bool refreshGraphs=true);
    void prepareGraphs(RawData _rawData, GraphData& data, GraphData& calibData);
    void restrictData(qreal _min, qreal _max, QCPGraphData& _data);
    void redrawSWR(bool _incrementally);
    void redrawPhase(bool _incrementally);
    void redrawRs(bool _incrementally);
    void redrawRp(bool _incrementally);
    void redrawRl(bool _incrementally);
    void redrawS21(bool _incrementally);
    void populateSParamData(const QList<SParamPoint>& points);
    // Converts one Touchstone value pair (in whichever format the file's
    // option line declared -- MA/RI/DB) into a plain complex number.
    // Shared by all four S-parameters on a 2-port data line, and by the
    // existing 1-port S11/Z11 pair -- format decoding is identical
    // regardless of which parameter it's for.
    static std::complex<double> sparamFromFormat(int iFormat, double v1, double v2);
    // std::arg() wraps into (-180, 180] degrees; a real transmission
    // phase can rack up many full turns across a sweep, so this
    // accumulates the shortest-path delta between consecutive points
    // instead (same technique as numpy.unwrap()). Shared by
    // populateSParamData()'s batch import and on_newSParamPoint()'s live
    // capture -- the math is identical, only where the have/prev state
    // lives differs (stack-local for a batch, member fields for live).
    static double unwrapPhaseDeg(double rawDeg, bool& havePrev, double& prevRaw, double& prevUnwrapped);
    // COL_POINTS cell text -- "--" until a scan/import finishes, then the
    // point count tagged with "(s1p)"/"(s2p)" so the Measurements list
    // itself shows which rows actually have 2-port data (dataSParam),
    // without needing a whole extra column for it.
    static QString pointsCellText(const measurement& mm);
    void redrawSmith(bool _incrementally);
    void redrawUser(bool _incrementally);
    void exportData(QString _name, int _type, QVector<RawData>& vector, QString _description=QString());

signals:
    void calibrationChanged();
    void import_finished(double _fqMin_khz, double _fqMax_khz);
    void measurementCanceled();
    void oneFqCanceled();
    void selectMeasurement(int row, int col);
    // First real 2-port point of a live capture -- MainWindow listens for
    // this to reveal the S21 tab the same way on_importFinished() does
    // for a .s2p import, just fired live instead of after the fact.
    void sparamDataStarted();

public slots:
    void on_newAnalyzerData(RawData _rawData);
    void on_newDataRedraw(RawData _rawData);
    void on_newData(RawData _rawData, bool _redraw=false);
    void on_newS21Data(S21Data _s21Data);
    void on_newSParamPoint(SParamPoint sp);
    void on_newUserDataHeader(QStringList);
    void on_newUserData(RawData, UserData);
    void on_newMeasurement(QString name);
    void on_newMeasurement(QString name, qint64 from, qint64 to, qint32 dots);
    void on_newMeasurementOneFq(QWidget*, qint64 fq, qint32 dots);
    void on_continueMeasurement(qint64 fq, qint64 sw, qint32 dots);
    void on_currentTab(QString);
    void on_focus(bool focus);
    void hideGraphBriefHint();
    void showHideHints();
    void on_newCursorFq(double x, int number, int mouseX, int mouseY);
    void on_newCursorSmithPos (double x, double y, int number);
    void updatePopUp(double xPos, int number, int mouseX, int mouseY);
    // Hides the crosshair (m_swrLine/m_phaseLine/etc., drawn in
    // updatePopUp() when the cursor is over a matching data point)
    // together with the brief-params popup, instead of leaving them
    // frozen at their last position once the cursor moves outside the
    // plot's data area. Called from each MainWindow::mouseMove_XXX()'s
    // out-of-range branch. Hides every tab's line unconditionally rather
    // than switching on m_currentTab -- harmless if already hidden, and
    // avoids relying on m_currentTab being perfectly in sync.
    void hideGraphCursor();
    void on_mainWindowPos(int x, int y);
    void setGraphHintEnabled(bool enabled);
    void setGraphBriefHintEnabled(bool enabled);
    void setCalibrationMode(bool enabled);
    void on_calibrationEnabled(bool enabled);
    void on_dotsNumberChanged(int number);
    void on_redrawGraphs(bool _incrementally=false);
    void on_changeMeasureSystemMetric (bool state);
    void replot();
    void showOneFqWidget(QWidget* parent, int _dots);
    void updateOneFqWidget(GraphData& _data);
    void hideOneFqWidget(bool dummy=false);
    // Swaps the active One-Fq display widget for the other style, wired
    // to both OneFqWidget::styleToggleRequested() and
    // OneFqBigReadout::styleToggleRequested() (double-click on either).
    void toggleOneFqDisplayStyle();
    // Wired to MainWindow::mainWindowMinimized() -- keeps OneFqBigReadout
    // (a real, taskbar-visible dialog) minimized/restored in lockstep with
    // the main window, distinct from on_focus()'s hide-on-any-OS-focus-loss
    // behavior for the Tool-popup-style widgets.
    void on_mainWindowMinimized(bool minimized);
    void on_isRangeChanged(bool);
    void on_impedanceChanged(double _z0);
    void on_exportCableSettings(QString _description);

    bool on_measurementComplete(); // returns true if the just-finished (empty) row was deleted
    void toggleVisibility(int row, bool _state);
    // Re-centers the Smith chart cursor dot (m_smithTracer, One Fq/marker-
    // click only) to the 50 ohm center point. Called on a new scan starting
    // and on Clear -- without this it just sat at its last One-Fq/marker
    // position indefinitely (issue #31). No-op if it hasn't been created
    // yet (nothing stale to reset).
    void resetSmithTracer();

    void redrawMultiGraph(bool _incrementally);

};

#endif // MEASUREMENTS_H
