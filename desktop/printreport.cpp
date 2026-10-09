#include "printreport.h"
#include "qcustomplot.h"
#include <QFontMetricsF>
#include <QPagedPaintDevice>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextFrame>
#include <QTextTable>
#include <QtMath>

namespace {

const double kDesignDpi = 96.0;
const double kTitleHeight = 30;
const double kFooterHeight = 18;
const double kGap = 12;
// The ink for all printed text. Near-black rather than plain black on
// purpose: setting a pen the painter already has is not recorded, so a print
// preview (which replays the drawing with the viewer's own pen) would show
// the text in the application's theme color -- white in a dark theme.
const QColor kInk(17, 17, 17);
const double kSlack = 6; // keeps a block that just fits from tipping onto the next page

// Draws a text document in the report's ink, whatever the app's palette is.
void drawDocument(QPainter& painter, QTextDocument* doc, const QRectF& clip = QRectF())
{
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, kInk);
    painter.save();
    if (clip.isValid()) {
        painter.setClipRect(clip);
        context.clip = clip;
    }
    doc->documentLayout()->draw(&painter, context);
    painter.restore();
}

QFont pixelFont(double pixels, bool bold = false)
{
    QFont font;
    font.setPixelSize(qRound(pixels));
    font.setBold(bold);
    return font;
}

QFont toPixelSize(QFont font)
{
    if (font.pixelSize() < 0)
        font.setPixelSize(qRound(font.pointSizeF() * kDesignDpi / 72.0));
    return font;
}

} // namespace

void PrintReport::useDesignFonts(QCustomPlot* plot)
{
    if (plot == nullptr)
        return;
    const auto axes = plot->axisRect()->axes();
    for (QCPAxis* axis : axes) {
        axis->setTickLabelFont(toPixelSize(axis->tickLabelFont()));
        axis->setLabelFont(toPixelSize(axis->labelFont()));
        axis->setSelectedTickLabelFont(toPixelSize(axis->selectedTickLabelFont()));
        axis->setSelectedLabelFont(toPixelSize(axis->selectedLabelFont()));
    }
    if (plot->legend != nullptr)
        plot->legend->setFont(toPixelSize(plot->legend->font()));
    for (int i = 0; i < plot->itemCount(); ++i) {
        if (QCPItemText* text = qobject_cast<QCPItemText*>(plot->item(i)))
            text->setFont(toPixelSize(text->font()));
    }
}

std::shared_ptr<QTextDocument> PrintReport::tableDocument(double width, int from, int to) const
{
    auto doc = std::make_shared<QTextDocument>();
    doc->setDocumentMargin(0);
    doc->setDefaultFont(pixelFont(12));
    QString html = "<table width=\"100%\" border=\"1\" cellspacing=\"0\" cellpadding=\"3\" "
                   "style=\"border-color:#808080;\"><tr>";
    for (const QString& h : m_table.headers)
        html += "<th bgcolor=\"#e6e6e6\">" + h.toHtmlEscaped() + "</th>";
    html += "</tr>";
    for (int r = from; r < to; ++r) {
        html += "<tr>";
        for (const QString& cell : m_table.rows.at(r))
            html += "<td align=\"center\">" + cell.toHtmlEscaped() + "</td>";
        html += "</tr>";
    }
    html += "</table>";
    doc->setHtml(html);
    doc->setTextWidth(width);
    return doc;
}

std::shared_ptr<QTextDocument> PrintReport::commentDocument(double width, double pageHeight) const
{
    auto doc = std::make_shared<QTextDocument>();
    doc->setDocumentMargin(0);
    doc->setDefaultFont(pixelFont(12));
    doc->setPlainText(m_comment);
    doc->setTextWidth(width);
    doc->setPageSize(QSizeF(width, pageHeight));
    return doc;
}

PrintReport::Plan PrintReport::plan(const QPageLayout& layout) const
{
    Plan p;
    const QRectF area = layout.paintRect(QPageLayout::Inch);
    p.width = area.width() * kDesignDpi;
    p.height = area.height() * kDesignDpi - kFooterHeight;
    p.titleHeight = m_title.isEmpty() ? 0 : kTitleHeight;
    p.chartTop = p.titleHeight + (p.titleHeight > 0 ? kGap : 0);

    const int rowCount = int(m_table.rows.size());
    const bool hasComment = !m_comment.trimmed().isEmpty();
    const double below = p.height - p.chartTop; // room under the title

    // Measure the table once: the top of every row, with the header as row 0.
    // (A cell's text block sits a fixed offset inside its row; the offset
    // cancels in the differences.)
    QVector<double> rowHeight; // [0] is the header
    if (rowCount > 0) {
        auto probe = tableDocument(p.width, 0, rowCount);
        auto* table = qobject_cast<QTextTable*>(probe->rootFrame()->childFrames().first());
        QAbstractTextDocumentLayout* lay = probe->documentLayout();
        QVector<double> top;
        for (int r = 0; r <= rowCount; ++r)
            top << lay->blockBoundingRect(table->cellAt(r, 0).firstCursorPosition().block()).top();
        const double bottom = lay->frameBoundingRect(table).bottom();
        // rowHeight[0] is the header, [i + 1] data row i. The last row ends at
        // the table's bottom edge, which sits below the last block's top by
        // as much as the first block sits below the table's top.
        for (int r = 0; r <= rowCount; ++r)
            rowHeight << ((r < rowCount ? top.at(r + 1) : bottom + top.at(0)) - top.at(r));
    }
    const double headerH = rowCount > 0 ? rowHeight.at(0) : 0;
    auto rowsHeight = [&](int from, int to) {
        double h = headerH + 2; // borders
        for (int r = from; r < to; ++r)
            h += rowHeight.at(r + 1);
        return h;
    };

    double commentH = 0;
    if (hasComment)
        commentH = commentDocument(p.width, 1e6)->size().height();

    if (m_chart) {
        const double natural = p.width / m_chartAspect;
        const double minChart = qMin(below, qMax(natural * 0.55, below * 0.30));
        // A square chart (Smith) gains nothing from extra height: it would only
        // sit in the middle of a taller box, far from what follows it.
        const double maxChart = qMin(below, natural * (m_chartAspect <= 1.05 ? 1.0 : 1.4));
        double content = 0;
        if (rowCount > 0)
            content += rowsHeight(0, rowCount) + kGap;
        if (hasComment)
            content += commentH + kGap;
        const double room = below - content - (content > 0 ? kSlack : 0);
        if (content > 0 && room < minChart) {
            // The content doesn't fit under even a small chart: a normal
            // chart, and the content flows from beneath it.
            p.chartHeight = qMin(below, qMax(minChart, natural * 1.1));
        } else {
            p.chartHeight = qBound(minChart, room, maxChart);
        }
    }

    // Pages: the first holds the title and chart, then the table's rows
    // packed into whatever room each page has (header repeated), then the
    // comment.
    PageContent first;
    p.pages << first;
    double used = p.chartTop + (m_chart ? p.chartHeight : 0); // bottom of what's on the last page
    if (rowCount > 0) {
        double tableTop = used + (m_chart ? kGap : 0);
        int index = 0;
        while (index < rowCount) {
            const double avail = p.height - tableTop;
            // A page with only a header and no room for a row is no use.
            if (avail < headerH + rowHeight.at(index + 1) + 2 && (tableTop > 0)) {
                p.pages << PageContent();
                tableTop = 0;
                continue;
            }
            int to = index;
            while (to < rowCount && rowsHeight(index, to + 1) <= avail)
                ++to;
            to = qMax(to, index + 1); // always make progress
            PageContent& page = p.pages.last();
            page.rowFrom = index;
            page.rowTo = to;
            page.tableTop = tableTop;
            page.table = tableDocument(p.width, index, to);
            used = tableTop + rowsHeight(index, to);
            index = to;
            if (index < rowCount) {
                p.pages << PageContent();
                tableTop = 0;
            }
        }
    }
    if (hasComment) {
        const double top = used + (used > 0 ? kGap : 0);
        if (top + commentH + kSlack <= p.height) {
            p.comment = commentDocument(p.width, p.height);
            p.commentTop = top;
            p.commentPage = p.pageCount() - 1;
        } else {
            p.comment = commentDocument(p.width, p.height);
            p.commentTop = -1;
            p.commentPage = p.pageCount();
            const int extra = qMax(1, p.comment->pageCount());
            for (int i = 0; i < extra; ++i)
                p.pages << PageContent();
        }
    }
    return p;
}

void PrintReport::paintPage(QCPPainter& painter, const Plan& plan, int page) const
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(kInk);

    if (page == 0) {
        if (!m_title.isEmpty()) {
            const QFont font = pixelFont(18, true);
            painter.setFont(font);
            const QString text = QFontMetricsF(font).elidedText(m_title, Qt::ElideRight, plan.width);
            painter.drawText(QRectF(0, 0, plan.width, plan.titleHeight), Qt::AlignLeft | Qt::AlignVCenter, text);
        }
        if (m_chart && plan.chartHeight > 0) {
            painter.save();
            painter.translate(0, plan.chartTop);
            painter.setClipRect(QRectF(0, 0, plan.width, plan.chartHeight));
            m_chart(painter, QSizeF(plan.width, plan.chartHeight));
            painter.restore();
        }
    }

    const PageContent& content = plan.pages.at(page);
    if (content.table != nullptr) {
        painter.save();
        painter.translate(0, content.tableTop);
        drawDocument(painter, content.table.get());
        painter.restore();
    }
    if (plan.comment != nullptr && page >= plan.commentPage) {
        painter.save();
        if (plan.commentTop >= 0) {
            painter.translate(0, plan.commentTop);
            drawDocument(painter, plan.comment.get());
        } else {
            const int k = page - plan.commentPage;
            painter.translate(0, -k * plan.height);
            drawDocument(painter, plan.comment.get(), QRectF(0, k * plan.height, plan.width, plan.height));
        }
        painter.restore();
    }

    if (plan.pageCount() > 1) {
        painter.setFont(pixelFont(11));
        painter.setPen(Qt::darkGray);
        painter.drawText(QRectF(0, plan.height, plan.width, kFooterHeight), Qt::AlignRight | Qt::AlignVCenter,
                         QObject::tr("Page %1 of %2").arg(page + 1).arg(plan.pageCount()));
    }
}

int PrintReport::pageCount(const QPageLayout& layout) const
{
    return plan(layout).pageCount();
}

void PrintReport::paint(QPagedPaintDevice* device) const
{
    const Plan p = plan(device->pageLayout());
    QCPPainter painter(device);
    painter.setMode(QCPPainter::pmVectorized);
    painter.setMode(QCPPainter::pmNonCosmetic);
    const double scale = device->logicalDpiX() / kDesignDpi;
    for (int page = 0; page < p.pageCount(); ++page) {
        if (page > 0)
            device->newPage();
        painter.save();
        painter.scale(scale, scale);
        paintPage(painter, p, page);
        painter.restore();
    }
}

QList<QImage> PrintReport::renderImages(const QPageLayout& layout, int dpi) const
{
    const Plan p = plan(layout);
    const QRectF full = layout.fullRect(QPageLayout::Inch);
    const QRectF area = layout.paintRect(QPageLayout::Inch);
    QList<QImage> images;
    for (int page = 0; page < p.pageCount(); ++page) {
        QImage image(qRound(full.width() * dpi), qRound(full.height() * dpi), QImage::Format_RGB32);
        image.setDotsPerMeterX(qRound(dpi / 0.0254));
        image.setDotsPerMeterY(qRound(dpi / 0.0254));
        image.fill(Qt::white);
        QCPPainter painter(&image);
        painter.setMode(QCPPainter::pmNonCosmetic);
        painter.translate(area.x() * dpi, area.y() * dpi);
        painter.scale(dpi / kDesignDpi, dpi / kDesignDpi);
        paintPage(painter, p, page);
        painter.end();
        images << image;
    }
    return images;
}
