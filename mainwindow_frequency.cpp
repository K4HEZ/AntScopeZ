#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "popupindicator.h"
#include "analyzer/customanalyzer.h"
#include "analyzer/nanovna_analyzer.h"
#include "Notification.h"
#include "glwidget.h"
#include "CustomPlot.h"
#include "selectdevicedialog.h"
#include "printmulti.h"
#include "style.h"
#include "filedialog.h"
#include <QWindow>

extern QString appendSpaces(const QString& number);
extern bool g_developerMode; // see main.cpp
extern bool g_usbOnly;
extern int g_maxMeasurements; // see measurements.cpp
extern QMap<QString, QString> g_mapTabPlotNames; // see mainwindow.cpp
extern void setAbsoluteFqMaximum();
extern bool g_bAA55modeNewProtocol;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original mainwindow.cpp (still in
// mainwindow.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of MainWindow.

void MainWindow::applyScanModeLabels(bool isRange)
{
    if (!isRange)
    {
        ui->startLabel->setText(tr("Start"));
        ui->stopLabel->setText(tr("Stop"));
        ui->groupBox_Presets->setTitle(tr("Presets (limits), kHz"));
        ui->tableWidget_presets->horizontalHeaderItem(0)->setText(tr("Start"));
        ui->tableWidget_presets->horizontalHeaderItem(1)->setText(tr("Stop"));
    }
    else
    {
        ui->startLabel->setText(tr("Center"));
        ui->stopLabel->setText(tr("Range (+/-)"));
        ui->groupBox_Presets->setTitle(tr("Presets (center, range), kHz"));
        ui->tableWidget_presets->horizontalHeaderItem(0)->setText(tr("Center"));
        ui->tableWidget_presets->horizontalHeaderItem(1)->setText(tr("Range(+/-)"));
    }
}

void MainWindow::applyScanMode(bool isRange)
{
    if (!isRange)
    {
        double from;
        double to;
        getEnteredFq(from, to);
        AnalyzerParameters::normalizeFq(from, to);

        m_isRange = false;
        applyScanModeLabels(false);
        setFqFrom(from);
        setFqTo(to);
    }
    else
    {
        double start;
        double stop;
        getEnteredFq(start, stop);
        double range = (stop - start)/2;
        double center = start + range;
        AnalyzerParameters::normalizeFqRange(center, range);

        m_isRange = true;
        applyScanModeLabels(true);
        setFqFrom(center);
        setFqTo(range);
    }
    emit isRangeChanged(m_isRange);
}

void MainWindow::on_scanModeCombo_currentIndexChanged(int index)
{
    applyScanMode(index == 1);
}


// Lowest frequency (kHz) this app will let Start/Stop request: 1 Hz.
// ABSOLUTE_MAX_FQ (analyzerparameters.h) is already the matching top end,
// 10,000,000 kHz = 10 GHz -- this is the same device-range limit, applied
// earlier, at the point a value is typed/picked rather than where it's
// finally handed to the device-command layer.
static constexpr double MIN_FQ_KHZ = 0.001;

double MainWindow::clampFqKhz(double khz)
{
    khz = qBound(MIN_FQ_KHZ, khz, (double)ABSOLUTE_MAX_FQ);
    // Round to the nearest 0.001 kHz (1 Hz): finer than that is beyond any
    // analyzer's real resolution, and was the direct trigger for a
    // QCheckedInt "Overflow in operator-" crash further downstream --
    // entering something like "0.000005" kHz into Start/Stop reached
    // integer frequency conversions unclamped. qRound64() (not qRound(),
    // which is only 32-bit) avoids the same class of overflow here, since
    // khz*1000 can be up to 10 billion.
    return qRound64(khz * 1000.0) / 1000.0;
}

QString MainWindow::formatFqKhz(double khz)
{
    QString s = QString::number(khz, 'f', 3);
    if (s.contains('.')) {
        while (s.endsWith('0'))
            s.chop(1);
        if (s.endsWith('.'))
            s.chop(1);
    }
    return s;
}

void MainWindow::setFqFrom(QString from)
{
    from.remove(' ');
    from = formatFqKhz(clampFqKhz(from.toDouble()));
    from = appendSpaces(from);
    ui->lineEdit_fqFrom->setText(from);
}

void MainWindow::setFqFrom(double from)
{
    // No decimal is ever shown below ('f', 0), so the effective floor here
    // is a whole kHz rather than clampFqKhz()'s 1 Hz -- a value in between
    // would just display as "0" anyway. See clampFqKhz() for the actual
    // device-range ceiling.
    from = qBound(1.0, from, (double)ABSOLUTE_MAX_FQ);
    QString sFrom = QString::number(from,'f', 0);
    sFrom = appendSpaces(sFrom);
    ui->lineEdit_fqFrom->setText(sFrom);
}

void MainWindow::setFqTo(QString to)
{
    to.remove(' ');
    to = formatFqKhz(clampFqKhz(to.toDouble()));
    to = appendSpaces(to);
    ui->lineEdit_fqTo->setText(to);
}

void MainWindow::setFqTo(double to)
{
    to = qBound(1.0, to, (double)ABSOLUTE_MAX_FQ);
    QString sTo = QString::number(to,'f', 0);
    sTo = appendSpaces(sTo);
    ui->lineEdit_fqTo->setText(sTo);
}

double MainWindow::getFqFrom(void)
{
    return m_swrWidget->xAxis->range().lower;
}

double MainWindow::getFqTo(void)
{
    return m_swrWidget->xAxis->range().upper;
}


void MainWindow::changeFqFrom(bool _backupValue)
{
    setFqFrom(ui->lineEdit_fqFrom->text());
    double lower = 0;
    double upper = 0;
    if(!m_isRange)
    {
        lower = ui->lineEdit_fqFrom->text().remove(' ').toDouble();
        upper = ui->lineEdit_fqTo->text().remove(' ').toDouble();
        if(lower >= upper)
        {
            upper = lower + 1;
            m_swrWidget->xAxis->setRangeUpper(upper);
            m_phaseWidget->xAxis->setRangeUpper(upper);
            m_rsWidget->xAxis->setRangeUpper(upper);
            m_rpWidget->xAxis->setRangeUpper(upper);
            m_rlWidget->xAxis->setRangeUpper(upper);
            if (g_developerMode)
                m_userWidget->xAxis->setRangeUpper(upper);
        }
        if(lower < 0)
        {
            lower = 0;
        }

        m_swrWidget->xAxis->setRangeLower(lower);
        m_phaseWidget->xAxis->setRangeLower(lower);
        m_rsWidget->xAxis->setRangeLower(lower);
        m_rpWidget->xAxis->setRangeLower(lower);
        m_rlWidget->xAxis->setRangeLower(lower);
        if (g_developerMode)
            m_userWidget->xAxis->setRangeLower(lower);
    }else
    {
        double from = ui->lineEdit_fqFrom->text().remove(' ').toDouble();
        double to = ui->lineEdit_fqTo->text().remove(' ').toDouble();
        lower = from - to;
        upper = from + to;
        if(lower >= upper)
        {
            upper = lower + 1;
            m_swrWidget->xAxis->setRangeUpper(upper);
            m_phaseWidget->xAxis->setRangeUpper(upper);
            m_rsWidget->xAxis->setRangeUpper(upper);
            m_rpWidget->xAxis->setRangeUpper(upper);
            m_rlWidget->xAxis->setRangeUpper(upper);
            if (g_developerMode)
                m_userWidget->xAxis->setRangeUpper(upper);
        }
        if(lower < 0)
        {
            lower = 0;
        }
        m_swrWidget->xAxis->setRangeUpper(lower+ui->lineEdit_fqTo->text().remove(' ').toDouble()*2);
        m_swrWidget->xAxis->setRangeLower(lower);

        m_phaseWidget->xAxis->setRangeUpper(lower+ui->lineEdit_fqTo->text().remove(' ').toDouble()*2);
        m_phaseWidget->xAxis->setRangeLower(lower);

        m_rsWidget->xAxis->setRangeUpper(lower+ui->lineEdit_fqTo->text().remove(' ').toDouble()*2);
        m_rsWidget->xAxis->setRangeLower(lower);

        m_rpWidget->xAxis->setRangeUpper(lower+ui->lineEdit_fqTo->text().remove(' ').toDouble()*2);
        m_rpWidget->xAxis->setRangeLower(lower);

        m_rlWidget->xAxis->setRangeUpper(lower+ui->lineEdit_fqTo->text().remove(' ').toDouble()*2);
        m_rlWidget->xAxis->setRangeLower(lower);
        if (g_developerMode) {
            m_userWidget->xAxis->setRangeUpper(lower+ui->lineEdit_fqTo->text().remove(' ').toDouble()*2);
            m_userWidget->xAxis->setRangeLower(lower);
        }
    }
    if (_backupValue) {
        m_lastEnteredFqFrom = lower;
        m_lastEnteredFqTo = upper;
    }
    updateGraph();
}

void MainWindow::changeFqTo(bool _backupValue)
{
    setFqTo(ui->lineEdit_fqTo->text());
    double lower;
    double upper;
    if(!m_isRange)
    {
        lower = ui->lineEdit_fqFrom->text().remove(' ').toDouble();
        upper = ui->lineEdit_fqTo->text().remove(' ').toDouble();
        if(lower >= upper)
        {
            lower = upper - 1;
            m_swrWidget->xAxis->setRangeLower(lower);
            m_phaseWidget->xAxis->setRangeLower(lower);
            m_rsWidget->xAxis->setRangeLower(lower);
            m_rpWidget->xAxis->setRangeLower(lower);
            m_rlWidget->xAxis->setRangeLower(lower);
            if (g_developerMode)
                m_userWidget->xAxis->setRangeLower(lower);
        }
        m_swrWidget->xAxis->setRangeUpper(upper);
        m_phaseWidget->xAxis->setRangeUpper(upper);
        m_rsWidget->xAxis->setRangeUpper(upper);
        m_rpWidget->xAxis->setRangeUpper(upper);
        m_rlWidget->xAxis->setRangeUpper(upper);
        if (g_developerMode)
            m_userWidget->xAxis->setRangeUpper(upper);
    }else
    {
        lower = ui->lineEdit_fqFrom->text().remove(' ').toDouble() - ui->lineEdit_fqTo->text().remove(' ').toDouble();
        upper = ui->lineEdit_fqFrom->text().remove(' ').toDouble() + ui->lineEdit_fqTo->text().remove(' ').toDouble();
        if(lower >= upper)
        {
            lower = upper - 1;
            m_swrWidget->xAxis->setRangeLower(lower);
            m_phaseWidget->xAxis->setRangeLower(lower);
            m_rsWidget->xAxis->setRangeLower(lower);
            m_rpWidget->xAxis->setRangeLower(lower);
            m_rlWidget->xAxis->setRangeLower(lower);
            if (g_developerMode)
                m_userWidget->xAxis->setRangeLower(lower);
        }
        if(lower < 0)
        {
            lower = 0;
        }
        m_swrWidget->xAxis->setRangeUpper(upper);
        m_swrWidget->xAxis->setRangeLower(lower);

        m_phaseWidget->xAxis->setRangeUpper(upper);
        m_phaseWidget->xAxis->setRangeLower(lower);

        m_rsWidget->xAxis->setRangeUpper(upper);
        m_rsWidget->xAxis->setRangeLower(lower);

        m_rpWidget->xAxis->setRangeUpper(upper);
        m_rpWidget->xAxis->setRangeLower(lower);

        m_rlWidget->xAxis->setRangeUpper(upper);
        m_rlWidget->xAxis->setRangeLower(lower);
        if (g_developerMode) {
            m_userWidget->xAxis->setRangeUpper(upper);
            m_userWidget->xAxis->setRangeLower(lower);
        }
    }
    if (_backupValue) {
        m_lastEnteredFqFrom = lower;
        m_lastEnteredFqTo = upper;
    }
    updateGraph();
    ui->singleStart->setFocus();
}

void MainWindow::on_lineEdit_fqFrom_editingFinished()
{
    changeFqFrom(true);
}

void MainWindow::on_lineEdit_fqTo_editingFinished()
{
    changeFqTo(true);
}

void MainWindow::on_dataChanged(qint64 _center_khz, qint64 _range_khz, qint32 _dots)
{
    double center_khz = (double)_center_khz;
    double range_khz = (double)_range_khz;
    AnalyzerParameters::normalizeFqRange(center_khz, range_khz);
    _center_khz = (qint64)center_khz;
    _range_khz = (qint64)range_khz;

    setDotsNumber(_dots);
    if (m_isRange) {
        ui->lineEdit_fqFrom->setText(QString::number(_center_khz));
        ui->lineEdit_fqTo->setText(QString::number(_range_khz));
    } else {
        QString strFrom = QString::number(_center_khz - _range_khz);
        ui->lineEdit_fqFrom->setText(strFrom);
        QString strTo = QString::number(_center_khz + _range_khz);
        ui->lineEdit_fqTo->setText(strTo);
    }
    changeFqTo();
    changeFqFrom();
    //on_lineEdit_fqTo_editingFinished();
    //on_lineEdit_fqFrom_editingFinished();
}


void MainWindow::onFullRange(bool)
{
    //int model = m_analyzer->getModel();

    AnalyzerParameters* param = AnalyzerParameters::current();
//    qint64 from = param == nullptr ? 100 : param->minFq().toULongLong();
//    qint64 to = param == nullptr ? ABSOLUTE_MAX_FQ : param->maxFq().toULongLong();
    qint64 from = 100;
    qint64 to = ABSOLUTE_MAX_FQ;
    if (param != nullptr) {
        from = param->minFq().toULongLong();
        to = param->maxFq().toULongLong();
    } else if (!m_measurements->isEmpty()) {
        from = m_measurements->last()->qint64From/1000;
        to = m_measurements->last()->qint64To/1000;
    }

    qint64 range = to - from;

    m_lastEnteredFqFrom = from;
    m_lastEnteredFqTo = to;

    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            from = ca->minFq().replace(" ", "").toULongLong();
            to = ca->maxFq().replace(" ", "").toULongLong();
            range = to - from;
        }
    }
    on_dataChanged(from + range/2, range, m_dotsNumber);
}

void MainWindow::getEnteredFq(double& start, double& stop)
{
    if (m_isRange) {
        double center = ui->lineEdit_fqFrom->text().remove(' ').toDouble();
        double range = ui->lineEdit_fqTo->text().remove(' ').toDouble();
        start = center - range;
        stop = center + range;
    } else {
        start = ui->lineEdit_fqFrom->text().remove(' ').toDouble();
        stop = ui->lineEdit_fqTo->text().remove(' ').toDouble();
    }
}

