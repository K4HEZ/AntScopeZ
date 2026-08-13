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
extern int g_maxMarkers; // see markers.cpp
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

void MainWindow::on_mouseDoubleClick(QMouseEvent* e)
{
    onCreateMarker(e->pos());
}

void MainWindow::onCreateMarker(QAction* action)
{
    onCreateMarker(action->data().toPoint());
}

void MainWindow::onCreateMarker(const QPoint& pos)
{
    //if (m_measurements->isEmpty())
      //  return;
    QCustomPlot* plot = getCurrentPlot();
    if (plot->objectName().contains("smith") || plot->objectName().contains("tdr"))
        return;
    if (m_markers->getMarkersCount() >= g_maxMarkers) {
        Notification::showMessage(tr("Maximum number of markers reached (%1) -- "
                                      "remove one, or raise the limit in Settings.")
                                       .arg(g_maxMarkers),
                                   this);
        return;
    }
    double x = plot->xAxis->pixelToCoord(pos.x());
    m_addingMarker = true;
    m_markers->create(x);
    m_markers->setFq(x);
    m_markers->add();
}

void MainWindow::onCustomContextMenuRequested(const QPoint& pos)
{
    QMenu *menu=new QMenu(this);
    menu->setStyleSheet(Style::menu());
    QCustomPlot* plot = getCurrentPlot();
    if (!plot->objectName().contains("smith") && !plot->objectName().contains("tdr"))
    {
        QAction* action = menu->addAction(tr("Create marker"));
        action->setData(pos);
        connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(onCreateMarker(QAction*)));
    }
    menu->popup(plot->mapToGlobal(pos));
}

// Single source of truth for the measurement points count -- see the
// declaration in mainwindow.h for why this replaced spinBoxPoints's
// implicit valueChanged-signal cascade.
void MainWindow::setDotsNumber(int value)
{
    if (value < 10)
        value = 10;
    if (value > 1000)
        value = 1000;

    ui->lineEdit_points->setText(QString::number(value));

    ui->speedAccuracySlider->blockSignals(true);
    ui->speedAccuracySlider->setValue(value);
    ui->speedAccuracySlider->blockSignals(false);

    m_dotsNumber = value;
    m_measurements->on_dotsNumberChanged(value);
}

void MainWindow::on_lineEdit_points_editingFinished()
{
    setDotsNumber(ui->lineEdit_points->text().toInt());
}

void MainWindow::on_speedAccuracySlider_valueChanged(int value)
{
    setDotsNumber(value);
}

