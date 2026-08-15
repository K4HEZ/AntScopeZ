#include "measurements.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"
#include "style.h"

extern bool g_developerMode;
extern QMap<QString, QString> g_mapTabPlotNames;
int g_maxMeasurements = MAX_MEASUREMENTS;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

QVector<QColor> generateColors(int number) {
    const int MAX_COLOR = 360;
    const int MIN_COLOR = 0;
    QVector<QColor> colors;
    double jump = (MAX_COLOR-MIN_COLOR) / (number*1.0);
    for (int i = 0; i < number; i++) {
        // Wrap into [0, MAX_COLOR) -- floating-point rounding of jump*i can
        // land exactly on MAX_COLOR for some `number` values, which is out
        // of QColor::fromHsv()'s valid hue range and logs "QColor::fromHsv:
        // HSV parameters out of range" (issue #35).
        int h = ((int)(MIN_COLOR + (jump*i))) % MAX_COLOR;
        colors.append(QColor::fromHsv(h, 255, 255));
    }
    return colors;
}

QColor getColor(int _index)
{
    static QColor colors[] = {
        QColor(30, 40, 255, 150),
        QColor(30, 255, 40, 150),
        QColor(255, 30, 40, 150),
        QColor(255, 127, 0, 255),
        QColor(255, 40, 255, 150)
    };
    if (_index >=0 && _index < 5)
        return colors[_index];

    // g_maxMeasurements is user-settable down to 1 (Settings: "Max
    // measurements", range 1-15), so g_maxMeasurements-4 can be zero or
    // negative, making `jump` a division by zero or negative. And even at
    // the default of 5 (colorCount=1), any _index beyond 5 -- e.g. the S21
    // pen's getColor(m_currentIndex+1) -- lands exactly on a multiple of
    // MAX_COLOR, which is out of range on its own. Both previously reached
    // QColor::fromHsv() with an invalid hue (issue #35, reported during a
    // screenshot). Clamp the divisor and wrap the hue into [0, MAX_COLOR).
    int colorCount = qMax(g_maxMeasurements-4, 1);
    const int MAX_COLOR = 360;
    const int MIN_COLOR = 0;
    double jump = (MAX_COLOR-MIN_COLOR) / (colorCount*1.0);
    int h = ((int)(MIN_COLOR + (jump*(_index-5)))) % MAX_COLOR;
    if (h < 0)
        h += MAX_COLOR;

    return QColor::fromHsv(h, 255, 255);
}

Measurements::Measurements(QObject *parent) : QObject(parent),
    m_currentIndex(0),
    m_graphHintBox(NULL),
    m_graphBriefHint(NULL),
    m_swrLine(NULL),
    m_swrLine2(NULL),
    m_phaseLine(NULL),
    m_phaseLine2(NULL),
    m_rsLine(NULL),
    m_rpLine(NULL),
    m_rlLine(NULL),
    m_rlLine2(NULL),
    m_s21Line(NULL),
    m_s21Line2(NULL),
    m_tdrLine(NULL),
    m_settings(NULL),
    m_calibration(NULL),
    m_graphHintEnabled(true),
    m_graphBriefHintEnabled(true),
    m_calibrationMode(false),
    m_Z0(50),
    m_dotsNumber(50),
    m_smithTracer(NULL)
{    
    QString path = Settings::setIniFile();
    m_settings = new QSettings(path,QSettings::IniFormat);
    m_settings->beginGroup("Measurements");
    m_graphHintEnabled = m_settings->value("GraphHintEnabled",true).toBool();
    m_graphBriefHintEnabled = m_settings->value("GraphBriefHintEnabled",true).toBool();
    m_settings->endGroup();

    m_settings->beginGroup("Cable");
    m_cableVelFactor = m_settings->value("VelFactor",0.66 ).toDouble();
    m_settings->endGroup();

    m_pdTdrImp =  new double[TDR_MAXARRAY];
    m_pdTdrStep =  new double[TDR_MAXARRAY];
    m_pdTdrZ =  new double[TDR_MAXARRAY];

    // m_graphHintBox/m_graphHintNameLabels/m_graphHintValueLabels used to
    // be a single self-constructed PopUp (floating Qt::Tool window,
    // positioned via setName("Hint")'s persisted x/y, colored per
    // chart-background via changeColorTheme()->setHintColor()) -- now a
    // plain QGroupBox with a QFormLayout of label:value QLabel rows,
    // docked in mainwindow.ui's middle column, handed over by MainWindow
    // via setGraphHintWidgets() once the widgets exist. Nothing to
    // construct or color here anymore; see setGraphHintWidgets() for the
    // equivalent initial-text/visibility setup.

    if(m_graphBriefHint == NULL)
    {
        m_graphBriefHint = new PopUp();
        m_graphBriefHint->setHiding(false);
        //m_graphBriefHint->setPopupText("0\n0");
        m_graphBriefHint->setName("BriefHint");
    }
}

Measurements::~Measurements()
{
    m_settings->beginGroup("Measurements");
    m_settings->setValue("GraphHintEnabled",m_graphHintEnabled);
    m_settings->setValue("GraphBriefHintEnabled",m_graphBriefHintEnabled);
    m_settings->endGroup();

    delete []m_pdTdrImp;
    delete []m_pdTdrStep;
    delete []m_pdTdrZ;

    // m_graphHintBox/m_graphHintNameLabels/m_graphHintValueLabels are owned
    // by mainwindow.ui (MainWindow's own ui_mainwindow.h-generated
    // members), not by Measurements -- nothing to delete here, unlike
    // m_graphBriefHint below (still a Measurements-owned floating PopUp).
    if (m_graphBriefHint)
    {
        delete m_graphBriefHint;
    }
}

void Measurements::setWidgets(CustomPlot * swr,   CustomPlot * phase,
                              CustomPlot * rs,    CustomPlot * rp,
                              CustomPlot * rl,    CustomPlot * tdr,    CustomPlot * s21,
                              QCustomPlot * smith, QTableWidget * table)
{
    QColor color(qRgb(66, 85, 138));
    QBrush br(color);
    m_swrWidget = swr;
    m_phaseWidget = phase;
    m_rsWidget = rs;
    m_rsWidget->legend->setVisible(true);
    m_rsWidget->legend->removeAt(0);
    m_rsWidget->legend->setTextColor(Qt::white);
    m_rsWidget->legend->setBrush(br);
    m_rpWidget = rp;
    m_rpWidget->legend->setVisible(true);
    m_rpWidget->legend->removeAt(0);
    m_rpWidget->legend->setTextColor(Qt::white);
    m_rpWidget->legend->setBrush(br);
    m_rlWidget = rl;
    m_s21Widget = s21;
    //m_s21Widget->legend->setVisible(true);
    //m_s21Widget->legend->removeAt(0);
    //m_s21Widget->legend->setTextColor(Qt::white);
    //m_s21Widget->legend->setBrush(br);
    m_tdrWidget = tdr;
    m_tdrWidget->legend->setVisible(true);
    m_tdrWidget->legend->removeAt(0);
    m_tdrWidget->legend->setTextColor(Qt::white);
    m_tdrWidget->legend->setBrush(br);
    m_smithWidget = smith;
    m_tableWidget = table;
    drawSmithImage();

    if(m_graphBriefHint != NULL)
    {
        m_graphBriefHint->setPenColor(QColor(0,0,0,0));
        m_graphBriefHint->setBackgroundColor(QColor(0,0,0,0));
        //m_graphBriefHint->setTextColor("black");
        setBriefHintColor();
    }
    connect(m_tableWidget, &QTableWidget::cellClicked, [=](int row, int col) {
        if (col == COL_MENU) {
            measurement& mm = m_measurements[row];
            QString prefix;
            QString name = mm.name;
            int pos = name.indexOf("> ");
            if (pos != -1) {
                prefix = name.left(pos+2);
                name = name.mid(pos+2);
            }
            QString style = Style::dialog();
            style += Style::label();
            style += Style::lineEdit();
            style += Style::pushButton();
            QInputDialog dlg;
            dlg.setStyleSheet(style);
            QString text;
            dlg.setLabelText(tr("Measurement name:"));
            dlg.setTextValue(name);
            if (dlg.exec() == QDialog::Accepted) {
                text = dlg.textValue();
            }

            if (!text.isEmpty()) {
                mm.name = prefix + text;

                m_tableWidget->setColumnWidth(COL_NAME, COL_NAME_WD);
                QTableWidgetItem* itm = m_tableWidget->item(row, COL_NAME);
                QFontMetrics fm(itm->font());
                int width = COL_NAME_WD;
                QString elided = fm.elidedText(mm.name, Qt::ElideRight, width);
                m_tableWidget->item(row, COL_NAME)->setText(elided);

                QString str = mm.name + tr("\nDouble-click an item to rescale the chart.\nRight-click an item to change color");
                m_tableWidget->item(row, COL_NAME)->setToolTip(str);
            }
        }
    });

}

// See the comment on m_graphHintBox/m_graphHintLabel's constructor spot
// (above) for why this replaces what used to be self-constructed here.
// Called once from MainWindow, right after ui_mainwindow.h's setupUi() has
// created the actual widgets.
void Measurements::setGraphHintWidgets(QWidget* box, const QList<QLabel*>& nameLabels, const QList<QLabel*>& valueLabels)
{
    m_graphHintBox = box;
    m_graphHintNameLabels = nameLabels;
    m_graphHintValueLabels = valueLabels;
    if (m_graphHintValueLabels.isEmpty())
        return;

    setGraphHintPlaceholder();
    if (m_graphHintBox != nullptr)
        m_graphHintBox->setVisible(m_graphHintEnabled);
}

void Measurements::setGraphHintFields(const QList<QPair<QString, QString>>& fields)
{
    int n = m_graphHintValueLabels.size();
    for (int i = 0; i < n; ++i) {
        bool active = i < fields.size();
        m_graphHintNameLabels[i]->setVisible(active);
        m_graphHintValueLabels[i]->setVisible(active);
        if (active) {
            m_graphHintNameLabels[i]->setText(fields[i].first);
            m_graphHintValueLabels[i]->setText(fields[i].second);
        }
    }
}

// Labels-only, empty-values placeholder -- same idea as the old
// single-QLabel placeholder ("Frequency = \nSWR = \n...") but with an
// explicit Phase row now that it's its own field instead of folded into
// |rho|'s line.
void Measurements::setGraphHintPlaceholder()
{
    setGraphHintFields({
        {tr("Frequency"), QString()},
        {tr("SWR"), QString()},
        {tr("RL"), QString()},
        {tr("Z"), QString()},
        {tr("|Z|"), QString()},
        {tr("|rho|"), QString()},
        {tr("Phase"), QString()},
        {tr("C"), QString()},
        {tr("Zpar"), QString()},
        {tr("Cpar"), QString()},
        {tr("Cable"), QString()},
    });
}

void Measurements::setUserWidget(CustomPlot * user) {
    m_userWidget = user;
    if (m_userWidget != nullptr && m_userWidget->legend != nullptr) {
        QColor color(qRgb(66, 85, 138));
        QBrush br(color);
        m_userWidget->legend->setVisible(true);
        m_userWidget->legend->removeAt(0);
        m_userWidget->legend->setTextColor(Qt::white);
        m_userWidget->legend->setBrush(br);
    }
}

void Measurements::setCalibration(Calibration * _calibration)
{
    m_calibration = _calibration;
}

bool Measurements::getCalibrationEnabled(void)
{
    return ((m_calibration != nullptr) && (m_calibration->getCalibrationEnabled()));
}

void Measurements::deleteRow(int row)
{
    m_tableWidget->removeRow(row);

    int count = m_swrWidget->graphCount();
    if(count)
    {
        int row_ = row+1;
        delete m_measurements[row].smithCurve;
        measurement mm = m_measurements[row];
        m_measurements.removeAt(row);
        m_viewMeasurements.removeAt(row);
        m_farEndMeasurementsAdd.removeAt(row);
        m_farEndMeasurementsSub.removeAt(row);

        m_swrWidget->removeGraph(row_);
        m_phaseWidget->removeGraph(row_);
        m_rsWidget->removeGraph(1+row*3);
        m_rsWidget->removeGraph(1+row*3);
        m_rsWidget->removeGraph(1+row*3);
        m_rpWidget->removeGraph(1+row*3);
        m_rpWidget->removeGraph(1+row*3);
        m_rpWidget->removeGraph(1+row*3);
        m_rlWidget->removeGraph(row_);
        m_s21Widget->removeGraph(1+row*2);
        m_s21Widget->removeGraph(1+row*2);
        m_tdrWidget->removeGraph(1+row*3);
        m_tdrWidget->removeGraph(1+row*3);
        m_tdrWidget->removeGraph(1+row*3);
        if (g_developerMode) {
            int index = getBaseUserGraphIndex(row);
            int cnt = mm.userGraphs.size();
            for (int i=0; i<cnt; i++)
                m_userWidget->removeGraph(index);
        }

        // repair legend
        if (row_ == 1 && count > 2) {
            m_rsWidget->legend->addItem(new QCPPlottableLegendItem(m_rsWidget->legend, m_rsWidget->graph(1)));
            m_rsWidget->legend->addItem(new QCPPlottableLegendItem(m_rsWidget->legend, m_rsWidget->graph(2)));
            m_rsWidget->legend->addItem(new QCPPlottableLegendItem(m_rsWidget->legend, m_rsWidget->graph(3)));

            m_rpWidget->legend->addItem(new QCPPlottableLegendItem(m_rpWidget->legend, m_rpWidget->graph(1)));
            m_rpWidget->legend->addItem(new QCPPlottableLegendItem(m_rpWidget->legend, m_rpWidget->graph(2)));
            m_rpWidget->legend->addItem(new QCPPlottableLegendItem(m_rpWidget->legend, m_rpWidget->graph(3)));

            m_tdrWidget->legend->addItem(new QCPPlottableLegendItem(m_tdrWidget->legend, m_tdrWidget->graph(1)));
            m_tdrWidget->legend->addItem(new QCPPlottableLegendItem(m_tdrWidget->legend, m_tdrWidget->graph(2)));
            m_tdrWidget->legend->addItem(new QCPPlottableLegendItem(m_tdrWidget->legend, m_tdrWidget->graph(3)));

            //m_s21Widget->legend->addItem(new QCPPlottableLegendItem(m_s21Widget->legend, m_s21Widget->graph(1)));
            //m_s21Widget->legend->addItem(new QCPPlottableLegendItem(m_s21Widget->legend, m_s21Widget->graph(2)));
        }
        int selRow = (row >= m_measurements.length()) ? (row-1) : row;
        QModelIndex myIndex = m_tableWidget->model()->index( selRow, 0,
                                                             QModelIndex());
        m_tableWidget->selectionModel()->select(myIndex,
                                    QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }

    m_tableWidget->setRowCount(m_measurements.length());
}

void Measurements::on_newMeasurement(QString name, qint64 from, qint64 to, qint32 dots)
{
    on_newMeasurement(name);

    m_measurements.last().set(from, to, dots);
    m_viewMeasurements.last().set(from, to, dots);
    m_farEndMeasurementsAdd.last().set(from, to, dots);
    m_farEndMeasurementsSub.last().set(from, to, dots);

    double range = (to - from)/2.0;
    double center = from + range;
    QString tips;
    QString fmt;
    if (m_RangeMode)
    {
        fmt = tr("FQ:%1kHz SW:%2kHz Points:%3");
        tips = QString(fmt)
                .arg((long)(center/1000))
                .arg((long)(range/1000))
                .arg(dots);
    } else {
        fmt = tr("Start:%1kHz Stop:%2kHz Points:%3");
        tips = QString(fmt)
                .arg((long)(from/1000))
                .arg((long)(to/1000))
                .arg(dots);
    }

    int row = m_tableWidget->rowCount()-1;
    QTableWidgetItem *item = m_tableWidget->item(row,COL_NAME);

    //item->setToolTip(tips);
    QString str = name + tr("\nDouble-click an item to rescale the chart.\nRight-click an item to change color");
    item->setToolTip(str);
    for (int i=0; i<m_tableWidget->rowCount(); i++)
    {
        QTableWidgetItem *item = m_tableWidget->item(i,COL_NAME);
        QString name = item->text();
        QString str = name + tr("\nDouble-click an item to rescale the chart.\nRight-click an item to change color");
        item->setToolTip(str);
    }
    m_measuringInProgress = true;
}

void Measurements::on_newMeasurement(QString name)
{
    m_interrupted = false;
    while(m_measurements.length() >= g_maxMeasurements)
    {
        deleteRow(0);
    }

    m_measurements.append( measurement());
    m_viewMeasurements.append( measurement());
    m_farEndMeasurementsAdd.append( measurement());
    m_farEndMeasurementsSub.append( measurement());

    QPen pen;
    if(m_swrWidget->graphCount() > 1)
    {
        pen = m_swrWidget->graph()->pen();
        pen.setWidth(3);
        m_swrWidget->graph()->setPen(pen);
        m_phaseWidget->graph()->setPen(pen);
        m_rlWidget->graph()->setPen(pen);
        m_s21Widget->graph()->setPen(pen);
        m_smithWidget->graph()->setPen(pen);
        m_measurements.at(m_measurements.length()-2).smithCurve->setPen(pen);
    }    
    m_swrWidget->addGraph();
    m_swrWidget->graph()->setAntialiasedFill(false);
    m_swrWidget->graph()->setName(name);
    m_phaseWidget->addGraph();

    // Needs tr() attention: these setName() calls are chart legend labels
    // (real user-facing text), left untranslated for now as a judgment
    // call -- most of them (R/X/Z/Rp/Xp/Zp/S21) are standard EE
    // abbreviations, the same category as the already-untranslated
    // "kHz"/"Ohm"/"dB" units elsewhere, and arguably shouldn't change
    // across languages. "Stage" is the one outlier below that's an actual
    // word, not a symbol -- more likely a genuine miss.
    m_rsWidget->setAutoAddPlottableToLegend(m_rsWidget->legend->itemCount() < 3);
    m_rsWidget->addGraph();
    m_rsWidget->graph()->setName("R");
    m_rsWidget->addGraph();
    m_rsWidget->graph()->setName("X");
    m_rsWidget->addGraph();
    m_rsWidget->graph()->setName("|Z|");
    m_rpWidget->setAutoAddPlottableToLegend(m_rpWidget->legend->itemCount() < 3);
    m_rpWidget->addGraph();
    //qobject_cast<CustomPlot*>(m_rpWidget)->addGraph();
    m_rpWidget->graph()->setName("Rp");
    m_rpWidget->addGraph();
    m_rpWidget->graph()->setName("Xp");
    m_rpWidget->addGraph();
    m_rpWidget->graph()->setName("|Zp|");
    m_rlWidget->addGraph();

    //m_s21Widget->setAutoAddPlottableToLegend(m_s21Widget->legend->itemCount() < 3);
    m_s21Widget->addGraph();
    m_s21Widget->graph()->setName("S21");
    m_s21Widget->addGraph();
    m_s21Widget->graph()->setName("Stage"); // Needs tr() attention (see above) -- this one's an actual word, not a symbol
    m_s21Widget->graph()->setValueAxis(m_s21Widget->yAxis2);

    m_tdrWidget->setAutoAddPlottableToLegend(m_tdrWidget->legend->itemCount() < 3);
    m_tdrWidget->addGraph();
    m_tdrWidget->graph()->setName(tr("Impulse response"));
    m_tdrWidget->addGraph();
    m_tdrWidget->graph()->setName(tr("Step response"));
    m_tdrWidget->addGraph();
    m_tdrWidget->graph()->setName(tr("|Z|"));
    m_tdrWidget->graph()->setValueAxis(m_tdrWidget->yAxis2);

    m_measurements.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_viewMeasurements.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_farEndMeasurementsAdd.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_farEndMeasurementsSub.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);

    if(++m_currentIndex >= g_maxMeasurements+1)
    {
        m_currentIndex = 1;
    }
    pen.setColor(getColor(m_currentIndex));
    pen.setWidth(ACTIVE_GRAPH_PEN_WIDTH);

    m_swrWidget->setBackgroundScaled(true);

    m_swrWidget->graph()->setPen(pen);
    m_phaseWidget->graph()->setPen(pen);
    m_rlWidget->graph()->setPen(pen);
    m_smithWidget->graph()->setPen(pen);
    m_measurements.last().smithCurve->setPen(pen);

    int rsGraphCount = m_rsWidget->graphCount();
    int s21GraphCount = m_s21Widget->graphCount();
    int tdrGraphCount = m_tdrWidget->graphCount();

    QPen rpen;
    rpen.setColor(QColor(255, 30, 40, 150));
    rpen.setWidthF(3);
    QPen xpen;
    xpen.setColor(QColor(30, 255, 40, 150));
    xpen.setWidthF(3);
    QPen zpen;
    zpen.setColor(QColor(30, 40, 255, 150));
    zpen.setWidthF(3);

    m_rsWidget->graph(rsGraphCount-3)->setPen(rpen);
    m_rsWidget->graph(rsGraphCount-2)->setPen(xpen);
    m_rsWidget->graph(rsGraphCount-1)->setPen(zpen);

    m_rpWidget->graph(rsGraphCount-3)->setPen(rpen);
    m_rpWidget->graph(rsGraphCount-2)->setPen(xpen);
    m_rpWidget->graph(rsGraphCount-1)->setPen(zpen);

    QPen s21Pen;
    s21Pen.setColor(getColor(m_currentIndex));
    s21Pen.setWidth(ACTIVE_GRAPH_PEN_WIDTH);
    m_s21Widget->graph(s21GraphCount-2)->setPen(s21Pen);
    s21Pen.setColor(getColor(m_currentIndex+1));
    m_s21Widget->graph(s21GraphCount-1)->setPen(s21Pen);

    m_tdrWidget->graph(tdrGraphCount-3)->setPen(zpen);
    m_tdrWidget->graph(tdrGraphCount-2)->setPen(xpen);
    m_tdrWidget->graph(tdrGraphCount-1)->setPen(rpen);

    // name.isEmpty -> singlePoint measurement
    if (!name.isEmpty())
    {
        if(m_graphBriefHintEnabled)
        {
            m_graphBriefHint->show();
        }

        QString nextName = name;
        if (name.indexOf("##") == 0)
        {
            int next = nextPrefix();
            nextName = QString("%1> %2").arg(next, 2, 10, QChar('0')).arg(name.mid(2));
        }
        m_measurements.last().name = nextName;
        m_tableWidget->setRowCount(0);

        QIcon icon;
        icon.addPixmap(QPixmap(":/new/prefix1/pencil.png"), QIcon::Normal, QIcon::Off);

        const int cell_side = 24;
        m_tableWidget->setColumnCount(MEASUREMENTS_TABLE_COLUMNS);
        m_tableWidget->setIconSize(QSize(16, 16));
        m_tableWidget->horizontalHeader()->setSectionResizeMode(COL_VISIBLE, QHeaderView::Fixed);
        // Interactive, not Fixed: the Name column's width was previously
        // locked, so a long measurement name (elided to fit) couldn't be
        // widened to actually read it -- user-draggable now.
        m_tableWidget->horizontalHeader()->setSectionResizeMode(COL_NAME, QHeaderView::Interactive);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(COL_MENU, QHeaderView::Fixed);
        m_tableWidget->horizontalHeader()->resizeSection(COL_VISIBLE, cell_side);
        m_tableWidget->horizontalHeader()->resizeSection(COL_MENU, cell_side);

        m_tableWidget->setRowCount(m_measurements.length());
        for(int i = 0; i < m_measurements.length(); ++i)
        {
            const measurement& mm = m_measurements.at(i);
            QTableWidgetItem *item;

            item = new QTableWidgetItem();
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(mm.visible ? Qt::Checked : Qt::Unchecked);
            m_tableWidget->setItem(i,COL_VISIBLE, item);

            item = new QTableWidgetItem();
            m_tableWidget->setItem(i,COL_NAME, item);
            m_tableWidget->setColumnWidth(COL_NAME, COL_NAME_WD);
            QFontMetrics fm(item->font());
            int width = COL_NAME_WD;
            QString elided = fm.elidedText(mm.name, Qt::ElideRight, width);
            item->setText(elided);

            item = new QTableWidgetItem();
            item->setIcon(icon);
            item->setSizeHint(QSize(cell_side, cell_side));
            m_tableWidget->setItem(i,COL_MENU, item);
        }

        m_tableWidget->reset();
        QModelIndex myIndex = m_tableWidget->model()->index( m_measurements.size()-1, 0, QModelIndex());
        m_tableWidget->selectionModel()->select(myIndex,QItemSelectionModel::Select | QItemSelectionModel::Rows);
        m_tableWidget->scrollToBottom();
    }
}


void Measurements::on_continueMeasurement(qint64 from, qint64 to, qint32 dots)
{
   // Q_UNUSED (from);
  //  Q_UNUSED (to);
  //  Q_UNUSED (dots);

    m_isContinuing = true;
    m_currentPoint = 0;
    m_measurements.last().set(from, to, dots); //vnn_0327

    delete m_measurements.last().smithCurve;
    delete m_viewMeasurements.last().smithCurve;
    delete m_farEndMeasurementsAdd.last().smithCurve;
    delete m_farEndMeasurementsSub.last().smithCurve;
    //m_measurements.last().dataRX.clear();
    //m_measurements.last().dataRXCalib.clear();
    m_viewMeasurements.last().dataRX.clear();
    m_viewMeasurements.last().dataRXCalib.clear();
    m_farEndMeasurementsAdd.last().dataRX.clear();
    m_farEndMeasurementsAdd.last().dataRXCalib.clear();
    m_farEndMeasurementsSub.last().dataRX.clear();
    m_farEndMeasurementsSub.last().dataRXCalib.clear();

    m_measurements.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_viewMeasurements.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_farEndMeasurementsAdd.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_farEndMeasurementsSub.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
}

void Measurements::on_newAnalyzerData(RawData _rawData)
{
    on_newData(_rawData, false);
}

void Measurements::on_newDataRedraw(RawData _rawData)
{
    on_newData(_rawData, true);
}

void Measurements::on_newUserDataHeader(QStringList fields)
{
    m_measurements.last().fieldsUser.clear();
    if (fields.isEmpty())
        return;
    m_measurements.last().fieldsUser.append(fields);
    QVector<QColor> colors = generateColors(fields.size());

    m_userWidget->setAutoAddPlottableToLegend(m_userWidget->legend->itemCount() < fields.size());
    for (int i=0; i<fields.size(); i++) {
        m_measurements.last().userGraphs.append(new QCPDataMap());
        m_viewMeasurements.last().userGraphs.append(new QCPDataMap());
        QCPGraph* grp = m_userWidget->addGraph();
        grp->setName(fields.at(i));
        QColor color = colors.takeFirst();
        QPen pen(color);
        pen.setWidth(3);
        grp->setPen(pen);
    }
}

void Measurements::on_newUserData(RawData _rawData, UserData _userData)
{
    on_newData(_rawData);

    m_measurements.last().dataUser.append(_userData);
    for (int idx=0; idx<_userData.values.size(); idx++) {
        QCPData qcpData;
        qcpData.key = _userData.fq*1000;
        qcpData.value = _userData.values.at(idx);
        QCPDataMap* map = m_measurements.last().userGraphs.at(idx);
        map->insert(qcpData.key,qcpData);
        QCPDataMap* vmap = m_viewMeasurements.last().userGraphs.at(idx);
        vmap->insert(qcpData.key,qcpData);
    }
    QVector <double> x,y;
    x.append(_userData.fq*1000);
    x.append(_userData.fq*1000);
    y.append(m_userWidget->yAxis->getRangeLower());
    y.append(m_userWidget->yAxis->getRangeUpper());
    m_userWidget->graph(0)->setData(x,y);

    on_redrawGraphs(true);
}

double regulate(double val, double limit)
{
    double _val = val;
    if (val < -limit)
        _val = -limit;
    if (val > limit)
        _val = limit;
    return _val;
}

void Measurements::on_newData(RawData _rawData, bool _redraw)
{
    if (m_oneFqMode) {
        GraphData _data;
        GraphData _calibData;
        prepareGraphs(_rawData, _data, _calibData);
        updateOneFqWidget(getCalibrationEnabled() ? _calibData : _data);
        return;
    }

    if(m_calibrationMode)
    {
        if (m_calibration != nullptr) {
            m_calibration->on_newData(_rawData);
        }
        return;
    }

    if (m_measurements.isEmpty()) {
        return;
    }
    // fix popup hint bug
    if (m_isContinuing) {
        if (m_currentPoint < m_measurements.last().dataRX.size()) {
            m_measurements.last().dataRX[m_currentPoint] = _rawData;
        } else {
            m_measurements.last().dataRX.append(_rawData);
        }
    } else {
        m_measurements.last().dataRX.append(_rawData);
    }

    updateTDRProgress(m_measurements.last().dataRX.size());

    double VSWR;
    double RL;
    if(computeSWR(_rawData.fq, m_Z0,_rawData.r,_rawData.x,&VSWR,&RL) != 1)
    {
        if(m_measurements.last().swrGraph.size() > 0)
        {
            VSWR = m_measurements.last().swrGraph.last().value;
            RL = m_measurements.last().rlGraph.last().value;
        }else
        {
            //return;
            VSWR = MAX_SWR;
            RL = 0;
        }
    }
    double maxSwr = m_swrWidget->yAxis->range().upper;
    double maxRs = m_rsWidget->yAxis->range().upper;
    double maxRp = m_rpWidget->yAxis->range().upper;

    QVector <double> x,y;
    double fq = _rawData.fq*1000;

    x.append(fq);
    x.append(fq);
    y.append(MIN_SWR);
    y.append(MAX_SWR);

    QCPData data;
    data.key = fq;
    data.value = regulate(VSWR, MAX_SWR);
    //----------------------------------------------
    //----2025_0326 vnn_0327
    measurement& mm = m_measurements.last();
    double fqDx =( ((mm.qint64To - mm.qint64From)/mm.qint64Dots))/1000;//
    double fqDx_up =fq+ (fqDx*0.9);//
    double fqDx_dn =fq- (fqDx*0.8);//
    //double fqMinLimit = m_measurements.last().qint64From/1000;
    double fqMaxLimit = mm.qint64To/1000;
    //x intervals clear ...new.data...|N-1|fqDx_dn|N|fqDx_up|O|...old.data..
    QCPDataMap *swrmapX;
    swrmapX = &( mm.swrGraph);
    QList <double> swrkeysX = swrmapX->keys();
    int keyId_cur = swrkeysX.length()-1;
    if(keyId_cur>4){
        double keyFq_cur =swrkeysX.at(keyId_cur);
        while((keyId_cur>=0)&&(keyFq_cur>fqDx_dn)){
            if(((keyFq_cur>fqDx_dn)&&(keyFq_cur<fqDx_up))||(keyFq_cur>fqMaxLimit)){
             //---del_rec----
             m_measurements.last().swrGraph.remove(keyFq_cur);

             m_measurements.last().rsrGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rsrGraph.remove(keyFq_cur);
             m_measurements.last().rsxGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rsxGraph.remove(keyFq_cur);
             m_measurements.last().rszGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rszGraph.remove(keyFq_cur);

             m_measurements.last().rprGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rprGraph.remove(keyFq_cur);
             m_measurements.last().rpxGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rpxGraph.remove(keyFq_cur);
             m_measurements.last().rpzGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rpzGraph.remove(keyFq_cur);

             m_measurements.last().rlGraph.remove(keyFq_cur);

             m_measurements.last().phaseGraph.remove(keyFq_cur);
             m_measurements.last().rhoGraph.remove(keyFq_cur);
              //---calibr
             if(m_calibration != NULL)
             {
                 if(m_calibration->getCalibrationPerformed())
                 {
                     m_measurements.last().swrGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().swrGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rsrGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rsrGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rsxGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rsxGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rszGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rszGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rprGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rprGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rpxGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rpxGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rpzGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rpzGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rlGraphCalib.remove(keyFq_cur);

                     m_measurements.last().phaseGraphCalib.remove(keyFq_cur);
                     m_measurements.last().rhoGraphCalib.remove(keyFq_cur);
                 }
              }
            }
            keyId_cur--;
            if(keyId_cur>=0){
             keyFq_cur =swrkeysX.at(keyId_cur);
            }
        }
    }

   //-------------------------------------------//vnn_0326
    m_measurements.last().swrGraph.insert(data.key,data);

    m_swrWidget->graph(0)->setData(x,y);

    y.clear();
    y.append(m_phaseWidget->yAxis->getRangeLower());
    y.append(m_phaseWidget->yAxis->getRangeUpper());
    m_phaseWidget->graph(0)->setData(x,y);

    y.clear();
    y.append(m_rsWidget->yAxis->getRangeLower());
    y.append(m_rsWidget->yAxis->getRangeUpper());
    m_rsWidget->graph(0)->setData(x,y);

    y.clear();
    y.append(m_rpWidget->yAxis->getRangeLower());
    y.append(m_rpWidget->yAxis->getRangeUpper());
    m_rpWidget->graph(0)->setData(x,y);

    y.clear();
    y.append(m_rlWidget->yAxis->getRangeLower());
    y.append(m_rlWidget->yAxis->getRangeUpper());
    m_rlWidget->graph(0)->setData(x,y);

//------------------------------------------------------------------------------
//------------------RXZ---------------------------------------------------------
//------------------------------------------------------------------------------
    double R = _rawData.r;
    double X = _rawData.x;
    double Z = computeZ(R, X);

    //qDebug() << "Measurements::on_newData" << fq << R << X;

    data.value = regulate(R, VALUE_LIMIT);
    m_measurements.last().rsrGraph.insert(data.key,data);
    data.value = regulate(R, maxRs);
    m_viewMeasurements.last().rsrGraph.insert(data.key,data);

    data.value = regulate(X, VALUE_LIMIT);
    m_measurements.last().rsxGraph.insert(data.key,data);
    data.value = regulate(X, maxRs);
    m_viewMeasurements.last().rsxGraph.insert(data.key,data);

    data.value = regulate(Z, VALUE_LIMIT);
    m_measurements.last().rszGraph.insert(data.key,data);
    data.value = regulate(Z, maxRs);
    m_viewMeasurements.last().rszGraph.insert(data.key,data);
//------------------------------------------------------------------------------
//------------------RXZ par-----------------------------------------------------
//------------------------------------------------------------------------------
    if (qIsNaN(R) || (R<0.001) )
    {
        R = 0.01;
    }
    if (qIsNaN(X))
    {
        X = 0;
    }
    double Rpar = R*(1+X*X/R/R);
    double Xpar = X*(1+R*R/X/X);
    double Zpar = computeZ(Rpar, Xpar);

    double rr, xx, zz;
    data.value = regulate(Rpar, VALUE_LIMIT);
    m_measurements.last().rprGraph.insert(data.key,data);

    data.value = regulate(Rpar, maxRp);
    m_viewMeasurements.last().rprGraph.insert(data.key,data);

    data.value = regulate(Xpar, VALUE_LIMIT);
    m_measurements.last().rpxGraph.insert(data.key,data);

    data.value = regulate(Xpar, maxRp);
    m_viewMeasurements.last().rpxGraph.insert(data.key,data);

    data.value = regulate(Zpar, VALUE_LIMIT);
    m_measurements.last().rpzGraph.insert(data.key,data);
    data.value = regulate(Zpar, maxRp);
    m_viewMeasurements.last().rpzGraph.insert(data.key,data);

    data.value = RL;
    m_measurements.last().rlGraph.insert(data.key,data);

//------------------------------------------------------------------------------
//----------------------calc phase----------------------------------------------
//------------------------------------------------------------------------------

    if (qIsNaN(_rawData.r) || (_rawData.r<0.001) )
    {
        _rawData.r = 0.01;
    }
    if (qIsNaN(_rawData.x))
    {
        _rawData.x = 0;
    }
    double Rnorm = _rawData.r/m_Z0;
    double Xnorm = _rawData.x/m_Z0;
    double Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
    double RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
    double RhoImag = 2*Xnorm/Denom;
    double RhoPhase = atan2(RhoImag, RhoReal) / M_PI * 180.0;
    double RhoMod = sqrt(RhoReal*RhoReal+RhoImag*RhoImag);
    data.value = RhoPhase;
    m_measurements.last().phaseGraph.insert(data.key,data);
    data.value = RhoMod;
    m_measurements.last().rhoGraph.insert(data.key,data);
//------------------------------------------------------------------------------
//----------------------calc smith----------------------------------------------
//------------------------------------------------------------------------------
    double pointX,pointY;
    NormRXtoSmithPoint(R/m_Z0, X/m_Z0, pointX, pointY);
    double len = m_measurements.last().dataRX.length();
    m_measurements.last().smithGraph.insert(len, QCPCurveData(len, pointX, pointY));
    len = m_measurements.last().dataRX.length()*2 - 1;
    m_measurements.last().smithGraphView.insert(len, QCPCurveData(len, pointX, pointY));

//------------------------------------------------------------------------------
//----------------------Calc calibration if performed---------------------------
//------------------------------------------------------------------------------
    if(m_calibration != NULL)
    {
        if(m_calibration->getCalibrationPerformed())
        {
            R = _rawData.r;
            X = _rawData.x;

            double Gre = (R*R-m_Z0*m_Z0+X*X)/((R+m_Z0)*(R+m_Z0)+X*X);
            double Gim = (2*m_Z0*X)/((R+m_Z0)*(R+m_Z0)+X*X);

            double GreOut;
            double GimOut;

            double SOR =  1; double SOI = 0; // Ideal model
            double SSR = -1; double SSI = 0;
            double SLR =  0; double SLI = 0;

            double COR, COI; // CalibrationReOpen, CalibrationImOpen
            double CSR, CSI; // CalibrationReShort, CalibrationImShort
            double CLR, CLI; // CalibrationReLoad, CalibrationImLoad
            bool res = m_calibration->interpolateS(_rawData.fq, COR, COI, CSR, CSI, CLR, CLI);
//            COR = 1;
//            COI = 0;
//            CSR = -1;
//            CSI = 0;
//            CLR = 0;
//            CLI = 0;

            if (!res)
            {
                SOR =  1; SOI = 0; // Ideal model
                SSR = -1; SSI = 0;
                SLR =  0; SLI = 0;
            }
            m_calibration->applyCalibration(Gre,Gim,  // Measured
                                            COR,COI,CSR,CSI,CLR,CLI, // Measured parameters of cal standards
                                            SOR,SOI,SSR,SSI,SLR,SLI, // Actual (Ideal) parameters of cal standards
                                            GreOut,GimOut); // Actual
            //-----------vnn_04 _1
            double chek_GreGim=sqrt((GreOut*GreOut)+(GimOut*GimOut));
            //1)   ((GreOut==1)&&(GimOut==0))
            //2)   (chek_GreGim>1)
            if( ((GreOut==1)&&(GimOut==0))||(chek_GreGim>1)){
                if((GreOut==1)&&(GimOut==0)){
                    GreOut= 0.999999992;
                }else{ 
                    double ncosA= GreOut/chek_GreGim;
                    double nsinA= GimOut/chek_GreGim;
                    GreOut=0.999999992*ncosA;
                    GimOut=0.999999992*nsinA;
                }
            }

            double calR = (1-GreOut*GreOut-GimOut*GimOut)/((1-GreOut)*(1-GreOut)+GimOut*GimOut);
            calR *= m_Z0;
            double calX = (2*GimOut)/((1-GreOut)*(1-GreOut)+GimOut*GimOut);
            calX *= m_Z0;
            double calZ = computeZ(calR,calX);

            RawData rawDataCalib = _rawData;
            rawDataCalib.r = calR;
            rawDataCalib.x = calX;

            //m_measurements.last().dataRXCalib.append(rawDataCalib);
            if (m_isContinuing) {
                if (m_currentPoint < m_measurements.last().dataRXCalib.size()) {
                    m_measurements.last().dataRXCalib[m_currentPoint] = rawDataCalib;
                } else {
                    m_measurements.last().dataRXCalib.append(rawDataCalib);
                }
            } else {
                m_measurements.last().dataRXCalib.append(rawDataCalib);
            }

            computeSWR(_rawData.fq, m_Z0, calR, calX,&VSWR,&RL);

            data.value = VSWR;
            if( VSWR > MAX_SWR )
            {
                data.value = MAX_SWR;
            }
            m_measurements.last().swrGraphCalib.insert(data.key,data);
            m_viewMeasurements.last().swrGraphCalib.insert(data.key,data);

            data.value = regulate(calR, VALUE_LIMIT);
            m_measurements.last().rsrGraphCalib.insert(data.key,data);
            data.value = regulate(calR, maxRs);
            m_viewMeasurements.last().rsrGraphCalib.insert(data.key,data);

            data.value = regulate(calX, VALUE_LIMIT);
            m_measurements.last().rsxGraphCalib.insert(data.key,data);
            data.value = regulate(calX, maxRs);
            m_viewMeasurements.last().rsxGraphCalib.insert(data.key,data);

            data.value = regulate(calZ, VALUE_LIMIT);
            m_measurements.last().rszGraphCalib.insert(data.key,data);
            data.value = regulate(calZ, maxRs);
            m_viewMeasurements.last().rszGraphCalib.insert(data.key,data);


            double calRpar = calR*(1+calX*calX/calR/calR);
            double calZpar = computeZ(calRpar, calX);

            data.value = regulate(calRpar, VALUE_LIMIT);
            m_measurements.last().rprGraphCalib.insert(data.key,data);
            data.value = regulate(calRpar, maxRp);
            m_viewMeasurements.last().rprGraphCalib.insert(data.key,data);

            data.value = regulate(calX, VALUE_LIMIT);
            m_measurements.last().rpxGraphCalib.insert(data.key,data);
            data.value = regulate(calX, maxRp);
            m_viewMeasurements.last().rpxGraphCalib.insert(data.key,data);

            data.value = regulate(calZpar, VALUE_LIMIT);
            m_measurements.last().rpzGraphCalib.insert(data.key,data);
            data.value = regulate(calZpar, maxRp);
            m_viewMeasurements.last().rpzGraphCalib.insert(data.key,data);

            data.value = RL;
            m_measurements.last().rlGraphCalib.insert(data.key,data);

            //----------------------calc phase---------------------------
            if (qIsNaN(calR) || (calR<0.001) )
            {
                calR = 0.01;
            }
            if (qIsNaN(calX))
            {
                calX = 0;
            }
            Rnorm = calR/m_Z0;
            Xnorm = calX/m_Z0;

            Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
            RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
            RhoImag = 2*Xnorm/Denom;

            RhoPhase = atan2(RhoImag, RhoReal) / M_PI * 180.0;            
            RhoMod = sqrt(RhoReal*RhoReal+RhoImag*RhoImag);

            QString msg = QString("f=%1, r=%2, x=%3, RhoPhase=%4")
                    .arg(_rawData.fq, 0, 'f', 4, QLatin1Char(' '))
                    .arg(calR, 0, 'f', 4, QLatin1Char(' '))
                    .arg(calX, 0, 'f', 4, QLatin1Char(' '))
                    .arg(RhoPhase, 0, 'f', 4, QLatin1Char(' '));

            data.value = RhoPhase;
            m_measurements.last().phaseGraphCalib.insert(data.key,data);
            data.value = RhoMod;
            m_measurements.last().rhoGraphCalib.insert(data.key,data);
            //----------------------calc phase end---------------------------
            //----------------------calc smith-------------------------------

            double ptX,ptY;
            //NormRXtoSmithPoint(R/m_Z0, X/m_Z0, ptX, ptY);
            NormRXtoSmithPoint(Rnorm, Xnorm, ptX, ptY);
            int len = m_measurements.last().dataRX.length();
            m_measurements.last().smithGraphCalib.insert(len, QCPCurveData(len, ptX, ptY));
            len = m_measurements.last().dataRX.length()*2 - 1;
            m_measurements.last().smithGraphViewCalib.insert(len, QCPCurveData(len, ptX, ptY));
             //----------------------calc smith end---------------------------
        }
    }
    m_currentPoint++;
    if (isTDRMode())
        return;

    //qint64 t1 = QDateTime::currentMSecsSinceEpoch();
    if (!_redraw)
        return;
    on_redrawGraphs(m_measuringInProgress && !m_isContinuing);
}

void Measurements::prepareGraphs(RawData _rawData, GraphData& _data, GraphData& _calibData)
{
    _data.FQ = _rawData.fq;
    _data.R = _rawData.r;
    _data.X = _rawData.x;

    computeSWR(_rawData.fq, m_Z0,_data.R,_data.X,&_data.SWR,&_data.RL);
    _data.Z = computeZ(_data.R,_data.X);

    //------------------RXZ par-----------------------------------------------------
    double R = _rawData.r;
    double X = _rawData.x;
    if (qIsNaN(R) || (R<0.001) )
        R = 0.01;
    if (qIsNaN(X))
        X = 0;
    _data.Rpar = R*(1+X*X/R/R);
    _data.Xpar = X*(1+R*R/X/X);
    _data.Zpar = computeZ(R, X);

    //----------------------calc phase----------------------------------------------
    double Rnorm = R/m_Z0;
    double Xnorm = X/m_Z0;
    double Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
    double RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
    double RhoImag = 2*Xnorm/Denom;
    double RhoPhase = atan2(RhoImag, RhoReal) / M_PI * 180.0;
    double RhoMod = sqrt(RhoReal*RhoReal+RhoImag*RhoImag);
    _data.RhoPhase = RhoPhase;
    _data.RhoMod = RhoMod;

    //----------------------Calc calibration if performed---------------------------
    if(m_calibration != NULL)
    {
        if(m_calibration->getCalibrationPerformed())
        {
            _calibData.FQ = _rawData.fq;
            R = _rawData.r;
            X = _rawData.x;
            double Gre = (R*R-m_Z0*m_Z0+X*X)/((R+m_Z0)*(R+m_Z0)+X*X);
            double Gim = (2*m_Z0*X)/((R+m_Z0)*(R+m_Z0)+X*X);

            double GreOut;
            double GimOut;

            double SOR =  1; double SOI = 0; // Ideal model
            double SSR = -1; double SSI = 0;
            double SLR =  0; double SLI = 0;

            double COR, COI; // CalibrationReOpen, CalibrationImOpen
            double CSR, CSI; // CalibrationReShort, CalibrationImShort
            double CLR, CLI; // CalibrationReLoad, CalibrationImLoad
            bool res = m_calibration->interpolateS(_rawData.fq, COR, COI, CSR, CSI, CLR, CLI);
//            COR = 1;
//            COI = 0;
//            CSR = -1;
//            CSI = 0;
//            CLR = 0;
//            CLI = 0;

            if (!res)
            {
                SOR =  1; SOI = 0; // Ideal model
                SSR = -1; SSI = 0;
                SLR =  0; SLI = 0;
            }
            m_calibration->applyCalibration(Gre,Gim,  // Measured
                                            COR,COI,CSR,CSI,CLR,CLI, // Measured parameters of cal standards
                                            SOR,SOI,SSR,SSI,SLR,SLI, // Actual (Ideal) parameters of cal standards
                                            GreOut,GimOut); // Actual
            //-----------vnn_04 _2
            double chek_GreGim=sqrt((GreOut*GreOut)+(GimOut*GimOut));
            //1)   ((GreOut==1)&&(GimOut==0))
            //2)   (chek_GreGim>1)
            if( ((GreOut==1)&&(GimOut==0))||(chek_GreGim>1)){
                if((GreOut==1)&&(GimOut==0)){
                    GreOut= 0.999999992;
                }else{
                    double ncosA= GreOut/chek_GreGim;
                    double nsinA= GimOut/chek_GreGim;
                    GreOut=0.999999992*ncosA;
                    GimOut=0.999999992*nsinA;
                }
            }

            double calR = (1-GreOut*GreOut-GimOut*GimOut)/((1-GreOut)*(1-GreOut)+GimOut*GimOut);
            calR *= m_Z0;
            double calX = (2*GimOut)/((1-GreOut)*(1-GreOut)+GimOut*GimOut);
            calX *= m_Z0;
            double calZ = computeZ(calR,calX);

            _calibData.R = calR;
            _calibData.X = calX;
            _calibData.Z = calZ;
            computeSWR(_calibData.FQ, m_Z0, calR, calX, &_calibData.SWR, &_calibData.RL);

            double calRpar = calR*(1+calX*calX/calR/calR);
            double calZpar = computeZ(calRpar, calX);

            _calibData.Rpar = calRpar;
            _calibData.Xpar = calX;
            _calibData.Zpar = calZpar;

            if (qIsNaN(calR) || (calR<0.001) )
                calR = 0.01;
            if (qIsNaN(calX))
                calX = 0;
            Rnorm = calR/m_Z0;
            Xnorm = calX/m_Z0;

            Denom = (Rnorm+1)*(Rnorm+1)+Xnorm*Xnorm;
            RhoReal = ((Rnorm-1)*(Rnorm+1)+Xnorm*Xnorm)/Denom;
            RhoImag = 2*Xnorm/Denom;

            RhoPhase = atan2(RhoImag, RhoReal) / M_PI * 180.0;
            RhoMod = sqrt(RhoReal*RhoReal+RhoImag*RhoImag);

            _calibData.RhoPhase = RhoPhase;
            _calibData.RhoMod = RhoMod;
        }
    }
    //----------------------calc smith-------------------------------
    double ptX,ptY;
    NormRXtoSmithPoint(Rnorm, Xnorm, ptX, ptY);
    _data.ptX = ptX;
    _data.ptY = ptY;
}


quint32 Measurements::computeSWR(double freq, double Z0, double R, double X, double *VSWR, double *RL)
{
    Q_UNUSED(freq);

    if (R <= 0)
    {
        R = 0.001;
    }
    double SWR, Gamma;
    double XX = X * X;								// always >= 0
    double denominator = (R + Z0) * (R + Z0) + XX;

    if (denominator == 0)
    {
        return 0;
    }
    Gamma = sqrt(((R - Z0) * (R - Z0) + XX) / denominator);
    if (Gamma == 1.0)
    {
        return 0;
    }
    SWR = (1 + Gamma) / (1 - Gamma);

    if ((SWR > 200) || (Gamma > 0.99))
    {
        SWR = 200;
    } else if (SWR < 1)
    {
        SWR = 1;
    }

    if (VSWR)
    {
        *VSWR = SWR;
    }
    if (RL)
    {
        if (Gamma == 0)
        {
            return 0;
        }
        *RL = -20 * log10(Gamma);
    }
    return 1;
}

double Measurements::computeZ (double R, double X)
{
    return sqrt((R*R) + (X*X));
}

void Measurements::on_newS21Data(S21Data _s21Data)
{
    QVector <double> x,y;
    double fq = _s21Data.fq*1000;

    x.append(fq);
    x.append(fq);
    y.append(m_s21Widget->yAxis->getRangeLower());
    y.append(m_s21Widget->yAxis->getRangeUpper());
    m_s21Widget->graph(0)->setData(x,y);

    QCPData data;
    data.key = fq;
    data.value = _s21Data.s21;

    m_measurements.last().s21Graph.insert(data.key,data);

    data.value = _s21Data.stage;
    m_measurements.last().s21StageGraph.insert(data.key,data);
    on_redrawGraphs(true);
}


void Measurements::on_currentTab(QString name)
{
    m_currentTab = name;
    on_redrawGraphs();
}

void Measurements::setCalibrationMode(bool enabled)
{
    m_calibrationMode = enabled;
}

void Measurements::on_calibrationEnabled(bool enabled)
{
    if(m_swrWidget->graphCount() == 1)
    {
        return;
    }

    int graphsCount = m_swrWidget->graphCount();
    for(int i = 1; i < graphsCount; ++i)
    {
        QCPDataMap swrmap;
        QCPDataMap rszmap;
        QCPDataMap rsxmap;
        QCPDataMap rsrmap;
        QCPDataMap rpzmap;
        QCPDataMap rpxmap;
        QCPDataMap rprmap;
        QCPDataMap rlmap;
        QCPDataMap phasemap;
        QCPCurveDataMap smithmap;
        int j = i-1;
        if(enabled)
        {
            swrmap = m_measurements.at(j).swrGraphCalib;
            phasemap = m_measurements.at(j).phaseGraphCalib;

            rszmap = m_measurements.at(j).rszGraphCalib;
            rsxmap = m_measurements.at(j).rsxGraphCalib;
            rsrmap = m_measurements.at(j).rsrGraphCalib;

            rpzmap = m_measurements.at(j).rpzGraphCalib;
            rpxmap = m_measurements.at(j).rpxGraphCalib;
            rprmap = m_measurements.at(j).rprGraphCalib;

            rlmap = m_measurements.at(j).rlGraphCalib;
            smithmap = m_measurements.at(j).smithGraphCalib;
        }else
        {
            swrmap = m_measurements.at(j).swrGraph;
            phasemap = m_measurements.at(j).phaseGraph;

            rszmap = m_measurements.at(j).rszGraph;
            rsxmap = m_measurements.at(j).rsxGraph;
            rsrmap = m_measurements.at(j).rsrGraph;

            rpzmap = m_measurements.at(j).rpzGraph;
            rpxmap = m_measurements.at(j).rpxGraph;
            rprmap = m_measurements.at(j).rprGraph;

            rlmap = m_measurements.at(j).rlGraph;
            smithmap = m_measurements.at(j).smithGraph;
        }

        m_swrWidget->graph(i)->setData(&swrmap, true);
        m_phaseWidget->graph(i)->setData(&phasemap, true);

        m_rsWidget->graph((i*3))->setData(&rszmap, true);
        m_rsWidget->graph((i*3)-1)->setData(&rsxmap, true);
        m_rsWidget->graph((i*3)-2)->setData(&rsrmap, true);

        m_rpWidget->graph((i*3))->setData(&rpzmap, true);
        m_rpWidget->graph((i*3)-1)->setData(&rpxmap, true);
        m_rpWidget->graph((i*3)-2)->setData(&rprmap, true);

        m_rlWidget->graph(i)->setData(&rlmap, true);
        //m_smithWidget->graph(i)->setData(&smithmap, true);
        m_measurements.at(j).smithCurve->setData(&smithmap, true);
    }
    replot();
    emit calibrationChanged();
}

void Measurements::on_dotsNumberChanged(int number)
{
    m_dotsNumber = number;
}

void Measurements::on_changeMeasureSystemMetric (bool state)
{
    m_measureSystemMetric = state;
    if(m_tdrWidget->graphCount()>2)
    {
        if(m_measureSystemMetric)
        {
            m_tdrWidget->graph(m_tdrWidget->graphCount()-3)->setData(&m_measurements.last().tdrImpGraph, true);
            m_tdrWidget->graph(m_tdrWidget->graphCount()-2)->setData(&m_measurements.last().tdrStepGraph, true);
            m_tdrWidget->graph(m_tdrWidget->graphCount()-1)->setData(&m_measurements.last().tdrZGraph, true);
        }else
        {
            m_tdrWidget->graph(m_tdrWidget->graphCount()-3)->setData(&m_measurements.last().tdrImpGraphFeet, true);
            m_tdrWidget->graph(m_tdrWidget->graphCount()-2)->setData(&m_measurements.last().tdrStepGraphFeet, true);
            m_tdrWidget->graph(m_tdrWidget->graphCount()-1)->setData(&m_measurements.last().tdrZGraphFeet, true);
        }
    }
    if(m_measureSystemMetric)
    {
        m_tdrWidget->xAxis->setLabel(tr("Length, m"));
    }
    else
    {
        m_tdrWidget->xAxis->setLabel(tr("Length, feet"));
    }
}

void Measurements::on_translate()
{
    if (!m_graphHintValueLabels.isEmpty())
    {
        setGraphHintPlaceholder();
    }
    if (m_graphBriefHint != nullptr)
    {
        // See the comment on the other setName("BriefHint") call above.
        m_graphBriefHint->setName("BriefHint");
    }

    if (m_tdrWidget->xAxis != nullptr)
    {
        m_tdrWidget->xAxis->setLabel(m_measureSystemMetric ? tr("Length, m") : tr("Length, feet"));
    }
}


int Measurements::getBaseUserGraphIndex(int row)
{
    int idx = 1;
    for (int i=row-1; i>=0; i--) {
        idx += m_measurements[i].userGraphs.size();
    }
    return idx;
}

void Measurements::on_isRangeChanged(bool _range)
{
    m_RangeMode = _range;

    int len = getMeasurementLength();
    for (int i=0; i<m_tableWidget->rowCount(); i++)
    {
        measurement* mm = getMeasurement(len-i-1);
        qint64 from = mm->qint64From;
        qint64 to = mm->qint64To;
        double range = to-from/2;
        double center = from + range;
        QString fmt;
        QString tips;
        if (m_RangeMode)
        {
            fmt = tr("FQ:%1kHz SW:%2kHz Points:%3");
            tips = QString(fmt)
                    .arg((long)(center/1000))
                    .arg((long)(range/1000))
                    .arg(mm->qint64Dots);
        } else {
            fmt = tr("Start:%1kHz Stop:%2kHz Points:%3");
            tips = QString(fmt)
                    .arg((long)(from/1000))
                    .arg((long)(to/1000))
                    .arg(mm->qint64Dots);
        }
        QTableWidgetItem *item = m_tableWidget->item(i,COL_NAME);
        //item->setToolTip(tips);
        QString name = item->text();
        QString str = name + tr("\nDouble-click an item to rescale the chart.\nRight-click an item to change color");
        item->setToolTip(str);
    }
}

void Measurements::setZ0(double _Z0)
{
    m_Z0 = _Z0;
}

void Measurements::on_impedanceChanged(double _z0)
{
    m_Z0 = _z0;
    qint32 len = getMeasurementLength();

    m_currentIndex -= len;
    if (m_currentIndex < 1)
        m_currentIndex = 0;

    QList<QTableWidgetItem*> selected = m_tableWidget->selectedItems();
    int selectedRow = selected.isEmpty() ? -1 : selected.at(0)->row();

    for (int idx=0; idx<len; idx++) {
        measurement mm = m_measurements.takeFirst();
        QString name = mm.name;

        delete mm.smithCurve;
        delete m_viewMeasurements.takeFirst().smithCurve;
        delete m_farEndMeasurementsAdd.takeFirst().smithCurve;
        delete m_farEndMeasurementsSub.takeFirst().smithCurve;
        m_swrWidget->removeGraph(1);
        m_phaseWidget->removeGraph(1);

        m_rsWidget->removeGraph(1);
        m_rsWidget->removeGraph(1);
        m_rsWidget->removeGraph(1);

        m_rpWidget->removeGraph(1);
        m_rpWidget->removeGraph(1);
        m_rpWidget->removeGraph(1);

        m_s21Widget->removeGraph(1);
        m_s21Widget->removeGraph(1);

        m_rlWidget->removeGraph(1);

        m_tdrWidget->removeGraph(1);
        m_tdrWidget->removeGraph(1);
        m_tdrWidget->removeGraph(1);

        on_newMeasurement(name, mm.qint64From, mm.qint64To, mm.qint64Dots);
        for (int i=0; i<mm.dataRX.size(); i++) {
            on_newData(mm.dataRX.at(i));
        }

        // restore user data
        if (g_developerMode) {
            int count = mm.userGraphs.size();
            for (int i=0; i<count; i++)
                m_userWidget->removeGraph(1);
            on_newUserDataHeader(mm.fieldsUser);
            for (int iu=0; iu<mm.dataUser.size(); iu++) {
                UserData _userData = mm.dataUser.at(iu);
                m_measurements.last().dataUser.append(_userData);
                for (int idx=0; idx<_userData.values.size(); idx++) {
                    QCPData qcpData;
                    qcpData.key = _userData.fq*1000;
                    qcpData.value = _userData.values.at(idx);
                    QCPDataMap* map = m_measurements.last().userGraphs.at(idx);
                    map->insert(qcpData.key,qcpData);
                    QCPDataMap* vmap = m_viewMeasurements.last().userGraphs.at(idx);
                    vmap->insert(qcpData.key,qcpData);
                }
                QVector <double> x,y;
                x.append(_userData.fq*1000);
                x.append(_userData.fq*1000);
                y.append(m_userWidget->yAxis->getRangeLower());
                y.append(m_userWidget->yAxis->getRangeUpper());
                m_userWidget->graph(0)->setData(x,y);
            }
        }
    }
    m_measuringInProgress = false;
    if ( selectedRow != -1) {
        m_tableWidget->selectRow(selectedRow);
        emit selectMeasurement(selectedRow, 0);
    }
}

void Measurements::on_measurementComplete()
{
    m_previousI = 0;
    m_measuringInProgress = false;
    m_isContinuing = false;
}

void Measurements::toggleVisibility(int row, bool _state)
{
    measurement& mm = m_measurements[row];
    mm.visible = _state;
    int count = m_swrWidget->graphCount();
    if (count > 1) {
        m_swrWidget->graph(row+1)->setVisible(_state);
        m_phaseWidget->graph(row+1)->setVisible(_state);
        m_rlWidget->graph(row+1)->setVisible(_state);
        mm.smithCurve->setVisible(_state);

        int row2 = row*2 + 1;
        m_s21Widget->graph(row2+0)->setVisible(_state);
        m_s21Widget->graph(row2+1)->setVisible(_state);

        int row1 = row*3 + 1;
        m_rpWidget->graph(row1+0)->setVisible(_state);
        m_rpWidget->graph(row1+1)->setVisible(_state);
        m_rpWidget->graph(row1+2)->setVisible(_state);

        m_rsWidget->graph(row1+0)->setVisible(_state);
        m_rsWidget->graph(row1+1)->setVisible(_state);
        m_rsWidget->graph(row1+2)->setVisible(_state);

        m_tdrWidget->graph(row1+0)->setVisible(_state);
        m_tdrWidget->graph(row1+1)->setVisible(_state);
        m_tdrWidget->graph(row1+2)->setVisible(_state);
    }
    replot();
}

