// print_report_demo outdir -- writes PDFs/PNGs of sample reports on several
// papers and orientations, for checking PrintReport's layout by eye.
#include "printreport.h"
#include "qcustomplot.h"
#include <QApplication>
#include <QDir>
#include <QPdfWriter>
#include <QPrinter>
#include <QPrintPreviewWidget>
#include <QtMath>
#include <cstdio>

static PrintReport::Table makeTable(int rows, int cols)
{
    PrintReport::Table t;
    for (int c = 0; c < cols; ++c)
        t.headers << QString("Col %1, unit").arg(c + 1);
    for (int r = 0; r < rows; ++r) {
        QStringList row;
        for (int c = 0; c < cols; ++c)
            row << QString::number((r + 1) * (c + 1) * 1.2345, 'f', 2);
        t.rows << row;
    }
    return t;
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    if (qEnvironmentVariableIsSet("DEMO_DARK")) {
        // The app's dark theme: light text on dark, which a printout must ignore.
        app.setStyle("Fusion");
        QPalette dark;
        dark.setColor(QPalette::Window, QColor(45, 45, 45));
        dark.setColor(QPalette::WindowText, Qt::white);
        dark.setColor(QPalette::Base, QColor(30, 30, 30));
        dark.setColor(QPalette::Text, Qt::white);
        dark.setColor(QPalette::ButtonText, Qt::white);
        app.setPalette(dark);
    }
    const QString out = argc > 1 ? argv[1] : "/tmp/printdemo";
    QDir().mkpath(out);

    QCustomPlot plot;
    plot.addGraph();
    for (int i = 0; i <= 200; ++i) {
        double x = 3500 + i * 3.0;
        plot.graph(0)->addData(x, 1.5 + 4 * qAbs(qSin(i / 25.0)));
    }
    plot.graph(0)->setPen(QPen(Qt::blue, 2));
    plot.xAxis->setLabel("Frequency, kHz");
    plot.yAxis->setLabel("SWR");
    plot.rescaleAxes();
    plot.yAxis->setRange(1, 10);
    PrintReport::useDesignFonts(&plot);

    auto chart = [&plot](QCPPainter& p, const QSizeF& size) {
        plot.toPainter(&p, qRound(size.width()), qRound(size.height()));
    };

    // A plot built the way Print::drawSmithImage() builds the Smith chart:
    // filled curves, text items, axes hidden, equal scaling.
    QCustomPlot smith;
    {
        auto circle = [&smith](double cx, double cy, double r, QColor fill) {
            QCPCurve* c = new QCPCurve(smith.xAxis, smith.yAxis);
            QCPCurveDataContainer d;
            for (int i = 0; i < 360; ++i)
                d.add(QCPCurveData(i, cx + r * qCos(i / 57.02), cy + r * qSin(i / 57.02)));
            c->setData(QSharedPointer<QCPCurveDataContainer>::create(d));
            c->setPen(QPen(Qt::black));
            c->setBrush(QBrush(fill));
            return c;
        };
        circle(0, 0, 6, QColor(0, 0, 255, 20));
        circle(0, 0, 2, QColor(255, 255, 255, 255));
        circle(3, 0, 3, Qt::transparent);
        for (int i = 0; i < plot.itemCount(); ++i) {}
        QCPItemText* t = new QCPItemText(&smith);
        t->position->setCoords(0, 6.5);
        t->setText("1");
        smith.legend->setVisible(true);
        smith.xAxis->setTicks(false);
        smith.yAxis->setTicks(false);
        smith.xAxis->setVisible(false);
        smith.yAxis->setVisible(false);
        smith.xAxis->setRange(-7, 7);
        smith.yAxis->setRange(-7, 7);
        PrintReport::useDesignFonts(&smith);
    }
    auto smithChart = [&smith](QCPPainter& p, const QSizeF& size) {
        const int w = qRound(size.width()), h = qRound(size.height());
        const QRect saved = smith.viewport();
        smith.setViewport(QRect(0, 0, w, h));
        smith.replot(QCustomPlot::rpImmediateRefresh);
        smith.yAxis->setScaleRatio(smith.xAxis, 1.0);
        smith.toPainter(&p, w, h);
        smith.setViewport(saved);
    };

    struct Case { const char* name; QPageSize::PageSizeId size; QPageLayout::Orientation orient; int rows; int cols; const char* comment; };
    const Case cases[] = {
        {"letter-portrait-small", QPageSize::Letter, QPageLayout::Portrait, 3, 8, "A short comment."},
        {"letter-landscape-small", QPageSize::Letter, QPageLayout::Landscape, 3, 8, "A short comment."},
        {"a4-portrait-small", QPageSize::A4, QPageLayout::Portrait, 3, 8, ""},
        {"letter-portrait-big", QPageSize::Letter, QPageLayout::Portrait, 45, 8, "Comment after a long table.\nSecond line."},
        {"a4-landscape-big", QPageSize::A4, QPageLayout::Landscape, 30, 10, ""},
        {"letter-portrait-nochart-table", QPageSize::Letter, QPageLayout::Portrait, 0, 0, "Only a comment, no table."},
        {"letter-portrait-smith-comment", QPageSize::Letter, QPageLayout::Portrait, 0, 0, "A typed comment under the Smith chart."},
        {"letter-portrait-square-comment", QPageSize::Letter, QPageLayout::Portrait, 0, 0, "A typed comment under a square (Smith-like) chart."},
        {"letter-landscape-square-comment", QPageSize::Letter, QPageLayout::Landscape, 0, 0, "A typed comment under a square (Smith-like) chart."},
    };
    for (const Case& c : cases) {
        PrintReport report;
        report.setTitle(QString("Match, 07.10.2026-10:15, SWR graph (%1)").arg(c.name));
        if (QString(c.name).contains("smith"))
            report.setChart(smithChart, 1.0);
        else
            report.setChart(chart, QString(c.name).contains("square") ? 1.0 : 1.7);
        if (c.rows > 0)
            report.setTable(makeTable(c.rows, c.cols));
        report.setComment(c.comment);

        QPdfWriter writer(out + "/" + c.name + ".pdf");
        writer.setResolution(300);
        QPageLayout layout(QPageSize(c.size), c.orient, QMarginsF(0.5, 0.5, 0.5, 0.5), QPageLayout::Inch);
        writer.setPageLayout(layout);
        report.paint(&writer);

        // Through the real preview widget, grabbed off screen.
        {
            QPrinter printer(QPrinter::HighResolution);
            printer.setPageLayout(layout);
            QPrintPreviewWidget preview(&printer);
            QObject::connect(&preview, &QPrintPreviewWidget::paintRequested,
                             [&report](QPrinter* p) { report.paint(p); });
            preview.resize(700, 900);
            preview.show();
            preview.updatePreview();
            preview.fitInView();
            for (int i = 0; i < 20; ++i)
                QApplication::processEvents();
            preview.grab().save(QString("%1/%2-preview.png").arg(out, c.name));
        }

        const QList<QImage> images = report.renderImages(layout, 60);
        for (int i = 0; i < images.size(); ++i)
            images.at(i).save(QString("%1/%2-p%3.png").arg(out, c.name).arg(i + 1));
        std::printf("%-32s %d page(s)\n", c.name, report.pageCount(layout));
    }
    return 0;
}
