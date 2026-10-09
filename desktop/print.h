#ifndef PRINT_H
#define PRINT_H

#include <QDialog>
#include <QPageSize>
#include <analyzer/analyzerparameters.h>
#include <markers.h>
#include <QSettings>
#include <settings.h>
#include "printmarkers.h"
#include "printreport.h"
#include <QTimer>

namespace Ui {
class Print;
}

class QLabel;
class QPrinter;
class QPrintPreviewWidget;

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
    void setEventsTable(const QStringList& headers, const QList<QStringList>& rows);

protected:
    void resizeEvent(QResizeEvent *e);
    void showEvent(QShowEvent *e);

protected slots:
    void on_lineSlider_valueChanged(int value);
    void on_printBtn_clicked();
    void on_pdfPrintBtn_clicked();
    void on_pngPrintBtn_clicked();
    void on_checkBoxReduceToner_toggled(bool checked);
    void on_pageSetupBtn_clicked();
    void on_textEditComment_textChanged();
    void on_titleEdit_textChanged();

protected:
    Ui::Print *ui;
    QSettings *m_settings;

    // Suggested save path with the given extension (no leading dot) --
    // FileDialog::userDataDir() plus the printout's own title
    // (lineEditHead), sanitized; falls back to a timestamp if the title is
    // empty.
    QString suggestedPath(const QString &ext) const;

    // The page, as it will print: title, chart, table, comment, laid out for
    // the preview printer's page layout (see PrintReport). The preview, Print,
    // Save as .pdf and Save as .png all paint it.
    void buildReport();
    void drawChart(QCPPainter& painter, const QSizeF& size);
    void applyLineWidth();
    void refreshPreview(); // soon, once; many changes in a row cost one repaint
    void updatePageLabel();
    void restorePageLayout();
    void savePageLayout();

    PrintReport m_report;
    QPrinter* m_printer = nullptr; // the preview's; holds the chosen paper, orientation and margins
    QPrintPreviewWidget* m_preview = nullptr;
    QLabel* m_pageLabel = nullptr;
    QTimer m_refreshTimer;

    QVector <double> m_mFqList;
    QVector <QCPCurve*> m_curveList;
    QVector <QCPItemStraightLine*> m_mStraightLineList;
    QVector <QCPItemText*> m_mTextList;
    QVector <QCPCurveDataContainer*> m_curveDataList;
    QVector <QCPItemText*> m_textList;
    // Band-highlight rects/labels drawBands()/addBand() create -- kept so
    // checkBoxReduceToner's toggled() handler can show/hide them live
    // without redoing the whole drawBands() pass.
    QVector <QCPAbstractItem*> m_bandItemList;
    // The Smith chart's shaded disc (the area outside the 2:1 circle).
    QCPCurve* m_smithShade = nullptr;
    void applyTonerSetting();

    bool m_isSmithGraph;
    QString m_graphName;
};

#endif // PRINT_H
