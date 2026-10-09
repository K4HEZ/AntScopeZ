#ifndef PRINTREPORT_H
#define PRINTREPORT_H

#include <QImage>
#include <QList>
#include <QPageLayout>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

class QCPPainter;
class QCustomPlot;
class QPagedPaintDevice;
class QTextDocument;

// One printed report -- title, chart, table, comment -- laid out for a given
// page, and painted the same way to a printer, a PDF, the print preview or
// an image, so they always agree.
//
// Everything is laid out in "design pixels" (1/96 inch) and the painter is
// scaled to the device's resolution, so any paper size, margins and
// orientation work. Fonts are pixel-sized for the same reason (see
// useDesignFonts()).
//
// Page 1 holds the title and the chart; the table and comment come after,
// on the same page when they fit (the chart shrinks to make room, down to a
// minimum) and otherwise flow onto further pages.
class PrintReport
{
public:
    struct Table
    {
        QStringList headers;
        QList<QStringList> rows;
    };

    // Draws the chart at the painter's origin, `size` design pixels.
    using ChartDrawer = std::function<void(QCPPainter&, const QSizeF& size)>;

    void setTitle(const QString& title) { m_title = title; }
    // aspect: width/height the chart looks best at (1 for a Smith chart).
    void setChart(ChartDrawer drawer, double aspect = 1.6)
    {
        m_chart = std::move(drawer);
        m_chartAspect = aspect;
    }
    void setTable(const Table& table) { m_table = table; }
    void setComment(const QString& comment) { m_comment = comment; }

    // Pages needed on `layout`.
    int pageCount(const QPageLayout& layout) const;
    // All pages, onto a printer, PDF writer or preview printer, using the
    // device's own page layout.
    void paint(QPagedPaintDevice* device) const;
    // Every page as an image of the whole sheet (margins included) at `dpi`.
    QList<QImage> renderImages(const QPageLayout& layout, int dpi) const;

    // Converts every font in the plot to a pixel size (point size * 96/72),
    // so it scales with the geometry instead of with the device's dpi.
    static void useDesignFonts(QCustomPlot* plot);

private:
    struct PageContent
    {
        int rowFrom = 0;
        int rowTo = 0; // table rows [rowFrom, rowTo) go on this page
        double tableTop = 0;
        std::shared_ptr<QTextDocument> table;
    };
    struct Plan
    {
        double width = 0;  // usable page area, design pixels
        double height = 0; // without the footer
        double titleHeight = 0;
        double chartTop = 0;
        double chartHeight = 0;
        QList<PageContent> pages;
        std::shared_ptr<QTextDocument> comment;
        double commentTop = -1; // on commentPage, if it shares a page with the table
        int commentPage = -1;   // first page the comment is on
        int pageCount() const { return int(pages.size()); }
    };
    Plan plan(const QPageLayout& layout) const;
    std::shared_ptr<QTextDocument> tableDocument(double width, int from, int to) const;
    std::shared_ptr<QTextDocument> commentDocument(double width, double pageHeight) const;
    void paintPage(QCPPainter& painter, const Plan& plan, int page) const;

    QString m_title;
    ChartDrawer m_chart;
    double m_chartAspect = 1.6;
    Table m_table;
    QString m_comment;
};

#endif // PRINTREPORT_H
