#ifndef PRINT_H
#define PRINT_H

#include <QDialog>
#include <QPageSize>
#include <analyzer/analyzerparameters.h>
#include <markers.h>
#include <QSettings>
#include <settings.h>
#include "printmarkers.h"

namespace Ui {
class Print;
}

class Print : public QDialog
{
    Q_OBJECT

public:
    explicit Print(QWidget *parent = 0);
    ~Print();
    virtual void addMarker(double fq, int number);

    //virtual void setRange(QCPRange x, QCPRange y);
    virtual void setRange(QCustomPlot* plot);
    void setRange_yAxis2(QCPRange range);
    void setLabel(QString xLabel, QString yLabel);
    void setData(QSharedPointer<QCPGraphDataContainer> m, QPen pen, QString name);
    void setSmithData(QSharedPointer<QCPCurveDataContainer> map, QPen pen, QString name);
    void setName(QString name) { m_graphName = name; }
    void drawBands(QStringList* _bands, double y1, double y2);
    virtual void addBand (double x1, double x2, double y1, double y2);
    void addBand (double x1, double x2, double y1, double y2, QCustomPlot* plot);
    void addBand (double x1, double x2, double y1, double y2, QString& name);
    void setHead(QString string);

    void updateTable();
    void drawSmithImage (void);

    virtual void rescale();
    void updateMarkers(int markers, int measurements, QList<QList<QVariant>> info);

protected:
    void resizeEvent(QResizeEvent *e);
    void showEvent(QShowEvent *e);

protected slots:
    void on_lineSlider_valueChanged(int value);
    void on_printBtn_clicked();
    void on_pdfPrintBtn_clicked();
    void on_pngPrintBtn_clicked();
    void on_checkBoxPrintBandHighlighting_toggled(bool checked);

protected:
    Ui::Print *ui;
    QSettings *m_settings;

    // Suggested save path with the given extension (no leading dot) --
    // FileDialog::userDataDir() plus the printout's own title
    // (lineEditHead), sanitized; falls back to a timestamp if the title is
    // empty.
    QString suggestedPath(const QString &ext) const;

    // toPixmap(width,height,...) renders at a fixed canvas size completely
    // independent of the live dialog's own on-screen size/aspect --
    // QCustomPlot::toPixmap() internally does its own temporary
    // setViewport(width,height) + draw() (which correctly resyncs
    // axisRect() for that size), but our own axis *ranges* (set by
    // rescale()'s setScaleRatio() call, see its comment) were computed
    // against whatever axisRect() was on-screen, a different aspect ratio
    // from the fixed export canvas -- round on screen, wrong (in the worst
    // case reported, drastically too-zoomed-in) once actually exported.
    // Sets the same viewport toPixmap() is about to use *before* calling
    // rescale(), so setScaleRatio() computes against the real export
    // dimensions instead; toPixmap() itself restores the original
    // (on-screen) viewport before returning, so a second rescale()
    // afterward resyncs the on-screen display back to it.
    QPixmap smithSafePixmap(int width, int height, double scale);

    QVector <double> m_mFqList;
    QVector <QCPCurve*> m_curveList;
    QVector <QCPItemStraightLine*> m_mStraightLineList;
    QVector <QCPItemText*> m_mTextList;
    QVector <QCPCurveDataContainer*> m_curveDataList;
    QVector <QCPItemText*> m_textList;
    // Band-highlight rects/labels drawBands()/addBand() create -- kept so
    // checkBoxPrintBandHighlighting's toggled() handler can show/hide them
    // live without redoing the whole drawBands() pass.
    QVector <QCPAbstractItem*> m_bandItemList;

    bool m_isSmithGraph;
    QString m_graphName;
};

#endif // PRINT_H
