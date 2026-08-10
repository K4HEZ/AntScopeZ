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

// This whole file was originally wrapped in one big #ifndef NO_MULTITAB
// (mainwindow.cpp lines 6758-6957) spanning every multitab method -- the
// mechanical split put the #endif (below, at what was line 6957) in this
// file but left the #ifndef trailing at the end of mainwindow_tabs.cpp
// (whose last function, createUserTab(), sat right before the guard
// opened). Reopened here instead, matching how mainwindow.h already
// guards the multitab members/methods the same way.
#ifndef NO_MULTITAB

void MainWindow::toMultiTab(int tab_index)
{
    QWidget* tab = ui->tabWidget->widget(tab_index);
    m_multiTabData.tabs << tab->objectName();
    buildMultiTabLayout();
    ui->tabWidget->setTabVisible(tab_index, false);
    ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_multi), true);
    ui->printBtn->setEnabled(false);
//    ui->tabWidget->widget(tab_index)->setVisible(false);
//    ui->tabWidget->widget(ui->tabWidget->indexOf(m_tab_multi))->setVisible(true);
}

void MainWindow::fromMultiTab(int tab_index)  // ???? tab_index ????
{
    QString tab_name = ui->tabWidget->widget(tab_index)->objectName();
    m_multiTabData.tabs.removeOne(tab_name);
    buildMultiTabLayout();

    QWidget* tab = ui->tabWidget->widget(tab_index);
    QString plot_name = g_mapTabPlotNames[tab_name];
    tab->layout()->addWidget(m_mapWidgets[plot_name]);
    ui->tabWidget->setTabVisible(tab_index, true);
    ui->printBtn->setEnabled(false);
    //ui->tabWidget->widget(tab_index)->setVisible(true);

    if (m_multiTabData.tabs.isEmpty()) {
        ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_multi), false);
        ui->printBtn->setEnabled(true);
        //ui->tabWidget->widget(ui->tabWidget->indexOf(m_tab_multi))->setVisible(false);
    }
}

QMenu& MainWindow::menuMultiTab(QMenu &menu)
{
    QMap<QString, QPair<int, QString>> tab_title; // <tab_name, <index, title> >
    for (int i=0; i<ui->tabWidget->count(); i++) {
        QString tab_name = ui->tabWidget->widget(i)->objectName();
        if (tab_name != "tab_multi") {
            if (!g_developerMode && tab_name == "tab_user")
                continue;
            if (tab_name == "tab_s21")
                continue;
            // tab_title also drives the "Close X" label/target below for
            // whatever is already in m_multiTabData.tabs, so tab_tdr must
            // stay in this map even though it can't be offered as a new
            // Join target (see the skip in the loop right below). Excluding
            // it here too used to make tab_title[tab_name] default-construct
            // an empty QPair when tab_tdr was already joined (e.g. via the
            // other "Move chart to the tab Multi" menu, or a stale saved
            // setting), producing a bare "Close" that acted on tab index 0
            // instead of TDR.
            tab_title[tab_name] = QPair<int, QString>(i, ui->tabWidget->tabText(i));
        }
    }
    foreach (const QString& tab_name, tab_title.keys()) {
        if (tab_name.isEmpty())
            continue;
        // TDR is a fundamentally different acquisition mode -- CalcTdr()
        // requires the sweep to start near DC, which a normal band scan
        // never does, so joining it here always fed it degenerate/empty
        // data (this is what produced the "!qIsNaN(value)" crash: it
        // collapsed the TDR y-axis to a zero-size range, which then divides
        // by zero in QCPAxis::coordToPixel()). Don't offer it as a Multi
        // join until TDR gets its own dedicated scan flow (see repo "todo"
        // item #7).
        if (tab_name == "tab_tdr")
            continue;
        if (!m_multiTabData.tabs.contains(tab_name)) {
            QPair<int, QString> pair = tab_title[tab_name];
            QString plot_name = pair.second;
            menu.addAction(tr("Join ") + plot_name, this, [this, pair]() {
                toMultiTab(pair.first);
            });
        }
    }
    menu.addSeparator();
    foreach (const QString& tab_name, m_multiTabData.tabs) {
        if (tab_name.isEmpty())
            continue;
        QPair<int, QString> pair = tab_title[tab_name];
        QString plot_name = pair.second;
        menu.addAction(tr("Close ") + plot_name, this, [this, pair]() {
            fromMultiTab(pair.first);
        });
    }
    if (!m_multiTabData.tabs.isEmpty()) {
        menu.addSeparator();
        menu.addAction(tr("Close all "), this, [this, tab_title]() {
            foreach(const QString& tab_name, m_multiTabData.tabs) {
                QPair<int, QString> pair = tab_title[tab_name];
                fromMultiTab(pair.first);
            }
            ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_multi), false);
            ui->printBtn->setEnabled(true);
            //ui->tabWidget->widget(ui->tabWidget->indexOf(m_tab_multi))->setVisible(false);
        });
    }
    return menu;
}

void MainWindow::buildMultiTabLayout()
{
    int tabs = m_multiTabData.tabs.size();
    if (tabs == 0) {
        return;
    }
//    if (tabs == 4) {
//        return;
//    }

    if (m_tab_multi == nullptr)
        return;

    QLayout* layout = m_tab_multi->layout();
    if (layout != nullptr) {
        QLayoutItem *item;
        while ((item = layout->takeAt(0)) != 0)
            layout->removeItem (item);
    }

    foreach(const QString& tab, m_multiTabData.tabs) {
        QString plot = g_mapTabPlotNames[tab];
        QWidget* widget = m_mapWidgets[plot];
        layout->addWidget(widget);
    }
    if (m_multiTabData.tabs.contains("tab_smith")) {
        QSize sz = size();
        sz += QSize(-2, -2);
        resize(sz);
        resizeWnd();
        m_smithWidget->replot();
        sz += QSize(2, 2);
        QTimer::singleShot(1, this, [this, sz]() {
            resize(sz);
            resizeWnd();
            m_smithWidget->replot();
        });
    }
    m_tab_multi->show();
}

void MainWindow::showMultiTab()
{
    QMenu menu;
    menu.setStyleSheet(Style::menu());
    menuMultiTab(menu);
    if (menu.exec(QCursor::pos()) != nullptr) {
        if (!m_multiTabData.tabs.isEmpty()) {
            ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_multi), true);
            ui->printBtn->setEnabled(false);
            //ui->tabWidget->widget(ui->tabWidget->indexOf(m_tab_multi))->setVisible(true);
            ui->tabWidget->setCurrentWidget(m_tab_multi);
        } else {
            ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_multi), false);
            ui->printBtn->setEnabled(true);
            //ui->tabWidget->widget(ui->tabWidget->indexOf(m_tab_multi))->setVisible(false);
            ui->tabWidget->setCurrentWidget(m_tab_swr);
        }
    }
}

QCustomPlot* MainWindow::plotForTab(const QString& tab)
{
    QString plot_name = g_mapTabPlotNames[tab];
    return m_mapWidgets[plot_name];
}

void MainWindow::restoreMultitab(const QString& tabs)
{
    if (!tabs.isEmpty()) {
        QStringList list = tabs.split(',', Qt::SkipEmptyParts);
        foreach (auto tab_name, list) {
            if (tab_name == "tab_user" && !g_developerMode)
                continue;
            if (tab_name == "tab_s21")
                continue;
            // A stale/corrupted "multiTab" setting could list "tab_multi"
            // itself. menuMultiTab() never offers it as a Join target (it's
            // excluded from tab_title), but restoreMultitab() matched it by
            // objectName like any other tab and self-inserted it into
            // m_multiTabData.tabs -- plotForTab("tab_multi") then has no
            // g_mapTabPlotNames entry and returns nullptr, which crashed
            // Measurements::replot()'s tab_multi loop on startup.
            if (tab_name == "tab_multi")
                continue;
            // Don't restore a previously-saved TDR join either -- see the
            // matching exclusion (and explanation) in menuMultiTab().
            if (tab_name == "tab_tdr")
                continue;
            for (int idx = 0; idx<ui->tabWidget->count(); idx++) {
                if (ui->tabWidget->widget(idx)->objectName() == tab_name) {
                    toMultiTab(idx);
                    break;
                }
            }
        }
    }
}
#endif

