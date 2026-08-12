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

void MainWindow::setBands(QCustomPlot * widget, double y1, double y2)
{
    addBand(widget,135.7, 137.8, y1, y2);
    addBand(widget,472, 479, y1, y2);
    addBand(widget,1800, 2000, y1, y2);
    addBand(widget,3500, 3800, y1, y2);
    addBand(widget,7000, 7300, y1, y2);
    addBand(widget,10100, 10150, y1, y2);
    addBand(widget,14000, 14350, y1, y2);
    addBand(widget,18068, 18168, y1, y2);
    addBand(widget,21000, 21450, y1, y2);
    addBand(widget,24890, 24990, y1, y2);
    addBand(widget,27075, 27295, y1, y2);
    addBand(widget,28000, 29700, y1, y2);
    addBand(widget,50000, 54000, y1, y2);
    addBand(widget,144000, 148000, y1, y2);
    addBand(widget,220000, 225000, y1, y2);
    addBand(widget,420000, 450000, y1, y2);
    addBand(widget,902000, 928000, y1, y2);
    addBand(widget,1240000, 1300000, y1, y2);
}

void MainWindow::setBands(QCustomPlot * widget, QStringList* bands, double y1, double y2)
{
    if (bands == nullptr)
    {
        setBands(widget, y1, y2);
        return;
    }
    m_settings->beginGroup("Settings");
    bool showName = m_settings->value("show-band-name", false).toBool();
    m_settings->endGroup();
    foreach (QString str, *bands)
    {
        QStringList list = str.split(',');
        if (list.size() == 2 || !showName)
        {
            addBand(widget, list[0].toDouble(), list[1].toDouble(), y1, y2);
        } else if (list.size() == 3) {
            addBand(widget, list[0].toDouble(), list[1].toDouble(), y1, y2, list[2]);
        }
    }
}

void MainWindow::addBand (QCustomPlot * widget, double x1, double x2, double y1, double y2)
{
    QCPItemRect * xRectItem = new QCPItemRect( widget );
    m_itemRectList.append(xRectItem);

    xRectItem->setVisible          (true);
    xRectItem->setPen              (QPen(Qt::transparent));
    xRectItem->setBrush            (QBrush(QColor(50,50,150,50)));

    xRectItem->topLeft->setType(QCPItemPosition::ptPlotCoords);
    xRectItem->topLeft->setAxisRect( widget->xAxis->axisRect() );
    xRectItem->topLeft->setCoords( x1, y2 );

    xRectItem->bottomRight ->setType(QCPItemPosition::ptPlotCoords);
    xRectItem->bottomRight ->setAxisRect( widget->xAxis->axisRect() );
    xRectItem->bottomRight ->setCoords( x2, y1 );
}

void MainWindow::addBand (QCustomPlot * widget, double x1, double x2, double y1, double y2, QString& name)
{
    QCPItemRect * xRectItem = new QCPItemRect( widget );
    m_itemRectList.append(xRectItem);

    xRectItem->setVisible          (true);
    xRectItem->setPen              (QPen(Qt::transparent));
    xRectItem->setBrush            (QBrush(QColor(50,50,150,50)));

    xRectItem->topLeft->setType(QCPItemPosition::ptPlotCoords);
    xRectItem->topLeft->setAxisRect( widget->xAxis->axisRect() );
    xRectItem->topLeft->setCoords( x1, y2 );

    xRectItem->bottomRight ->setType(QCPItemPosition::ptPlotCoords);
    xRectItem->bottomRight ->setAxisRect( widget->xAxis->axisRect() );
    xRectItem->bottomRight ->setCoords( x2, y1 );

    if (!name.isEmpty()) {
        QRectF rr(QPointF(x1, y1), QPointF(x2, y2));
        QPointF pt = rr.center();
        QCPItemText* textItem = new QCPItemText( widget );
        textItem->setColor(QColor(50,50,150,150));
        textItem->setPen(Qt::NoPen);
        textItem->setText(name);
        textItem->position->setCoords(pt.x(), pt.y());
        textItem->setPositionAlignment(Qt::AlignHCenter);
        textItem->setRotation(270);

        m_itemRectList.append(textItem);
    }

}

void MainWindow::createTabs (QString sequence)
{
    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    QStringList tabs = sequence.split(',', Qt::SkipEmptyParts);
    foreach (const QString tab, tabs)
    {
        if (tab == "tab_swr") {
            m_tab_swr = new GLWidget();
            m_tab_swr->setObjectName(QStringLiteral("tab_swr"));
            QHBoxLayout* layout = new QHBoxLayout(m_tab_swr);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("horizontalLayout_1"));
            m_swrWidget = new CustomPlot(1, m_tab_swr);
            qobject_cast<GLWidget*>(m_tab_swr)->setPlotter(m_swrWidget);
            m_swrWidget->setObjectName(QStringLiteral("swr_widget"));
            sizePolicy.setHorizontalStretch(0);
            sizePolicy.setVerticalStretch(2);
            sizePolicy.setHeightForWidth(m_swrWidget->sizePolicy().hasHeightForWidth());
            m_swrWidget->setSizePolicy(sizePolicy);
            layout->addWidget(m_swrWidget);
            ui->tabWidget->addTab(m_tab_swr, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_swr), QApplication::translate("MainWindow", "SWR", 0));
            m_mapWidgets.insert(QStringLiteral("swr_widget"), m_swrWidget);
        }
        if (tab == "tab_phase") {
            m_tab_phase = new GLWidget();
            m_tab_phase->setObjectName(QStringLiteral("tab_phase"));

            QHBoxLayout* layout = new QHBoxLayout(m_tab_phase);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("horizontalLayout_2"));
            m_phaseWidget = new CustomPlot(1, m_tab_phase);
            qobject_cast<GLWidget*>(m_tab_phase)->setPlotter(m_phaseWidget);
            m_phaseWidget->setObjectName(QStringLiteral("phase_widget"));

            sizePolicy.setHorizontalStretch(0);
            sizePolicy.setVerticalStretch(2);
            sizePolicy.setHeightForWidth(m_phaseWidget->sizePolicy().hasHeightForWidth());
            m_phaseWidget->setSizePolicy(sizePolicy);
            layout->addWidget(m_phaseWidget);

            ui->tabWidget->addTab(m_tab_phase, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_phase), QApplication::translate("MainWindow", "Phase", 0));
            m_mapWidgets.insert(QStringLiteral("phase_widget"), m_phaseWidget);
        }
        if (tab == "tab_rs") {
            m_tab_rs = new GLWidget();
            m_tab_rs->setObjectName(QStringLiteral("tab_rs"));

            QHBoxLayout* layout = new QHBoxLayout(m_tab_rs);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("horizontalLayout_3"));
            m_rsWidget = new CustomPlot(3, m_tab_rs);

            qobject_cast<GLWidget*>(m_tab_rs)->setPlotter(m_rsWidget);
            m_rsWidget->setObjectName(QStringLiteral("rs_widget"));

            sizePolicy.setHorizontalStretch(0);
            sizePolicy.setVerticalStretch(2);
            sizePolicy.setHeightForWidth(m_rsWidget->sizePolicy().hasHeightForWidth());
            m_rsWidget->setSizePolicy(sizePolicy);
            layout->addWidget(m_rsWidget);

            ui->tabWidget->addTab(m_tab_rs, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_rs), QApplication::translate("MainWindow", "Z=R+jX", 0));
            m_mapWidgets.insert(QStringLiteral("rs_widget"), m_rsWidget);
        }
        if (tab == "tab_rp") {
            m_tab_rp = new GLWidget();
            m_tab_rp->setObjectName(QStringLiteral("tab_rp"));

            QHBoxLayout* layout = new QHBoxLayout(m_tab_rp);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("horizontalLayout_4"));
            m_rpWidget = new CustomPlot(3, m_tab_rp);
            qobject_cast<GLWidget*>(m_tab_rp)->setPlotter(m_rpWidget);
            m_rpWidget->setObjectName(QStringLiteral("rp_widget"));

            sizePolicy.setHorizontalStretch(0);
            sizePolicy.setVerticalStretch(2);
            sizePolicy.setHeightForWidth(m_rpWidget->sizePolicy().hasHeightForWidth());
            m_rpWidget->setSizePolicy(sizePolicy);
            layout->addWidget(m_rpWidget);

            ui->tabWidget->addTab(m_tab_rp, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_rp), QApplication::translate("MainWindow", "Z=R||+jX", 0));
            m_mapWidgets.insert(QStringLiteral("rp_widget"), m_rpWidget);
        }
        if (tab == "tab_rl") {
            m_tab_rl = new GLWidget();
            m_tab_rl->setObjectName(QStringLiteral("tab_rl"));
            QHBoxLayout* layout = new QHBoxLayout(m_tab_rl);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("horizontalLayout_2"));
            m_rlWidget = new CustomPlot(1, m_tab_rl);
            qobject_cast<GLWidget*>(m_tab_rl)->setPlotter(m_rlWidget);
            m_rlWidget->setObjectName(QStringLiteral("rl_widget"));

            sizePolicy.setHorizontalStretch(0);
            sizePolicy.setVerticalStretch(2);
            sizePolicy.setHeightForWidth(m_rlWidget->sizePolicy().hasHeightForWidth());
            m_rlWidget->setSizePolicy(sizePolicy);
            layout->addWidget(m_rlWidget);
            ui->tabWidget->addTab(m_tab_rl, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_rl), QApplication::translate("MainWindow", "RL", 0));
            m_mapWidgets.insert(QStringLiteral("rl_widget"), m_rlWidget);
        }
        if (tab == "tab_s21") {
            m_tab_s21 = new GLWidget();
            m_tab_s21->setObjectName(QStringLiteral("tab_s21"));

            QHBoxLayout* layout = new QHBoxLayout(m_tab_s21);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("horizontalLayout_s21"));
            m_s21Widget = new CustomPlot(2, m_tab_s21);
            qobject_cast<GLWidget*>(m_tab_s21)->setPlotter(m_s21Widget);
            m_s21Widget->setObjectName(QStringLiteral("s21_widget"));

            sizePolicy.setHorizontalStretch(0);
            sizePolicy.setVerticalStretch(2);
            sizePolicy.setHeightForWidth(m_s21Widget->sizePolicy().hasHeightForWidth());
            m_s21Widget->setSizePolicy(sizePolicy);
            layout->addWidget(m_s21Widget);

            ui->tabWidget->addTab(m_tab_s21, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_s21), QApplication::translate("MainWindow", "S21", 0));
            m_mapWidgets.insert(QStringLiteral("s21_widget"), m_s21Widget);
        }
        if (tab == "tab_tdr") {
            m_tab_tdr = new GLWidget();
            m_tab_tdr->setObjectName(QStringLiteral("tab_tdr"));

            QHBoxLayout* layout = new QHBoxLayout(m_tab_tdr);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("horizontalLayout_6"));
            m_tdrWidget = new CustomPlot(2, m_tab_tdr);
            qobject_cast<GLWidget*>(m_tab_tdr)->setPlotter(m_tdrWidget);
            m_tdrWidget->setObjectName(QStringLiteral("tdr_widget"));

            sizePolicy.setHorizontalStretch(0);
            sizePolicy.setVerticalStretch(2);
            sizePolicy.setHeightForWidth(m_tdrWidget->sizePolicy().hasHeightForWidth());
            m_tdrWidget->setSizePolicy(sizePolicy);
            layout->addWidget(m_tdrWidget);

            ui->tabWidget->addTab(m_tab_tdr, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_tdr), QApplication::translate("MainWindow", "TDR", 0));
            m_mapWidgets.insert(QStringLiteral("tdr_widget"), m_tdrWidget);
        }
        if (tab == "tab_smith") {
            m_tab_smith = new GLWidget();
            m_tab_smith->setObjectName(QStringLiteral("tab_smith"));

            QHBoxLayout* layout = new QHBoxLayout(m_tab_smith);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("horizontalLayout_7"));
            m_smithWidget = new CustomPlot(1, m_tab_smith);
            qobject_cast<GLWidget*>(m_tab_smith)->setPlotter(m_smithWidget);
            m_smithWidget->setObjectName(QStringLiteral("smith_widget"));
            m_smithWidget->mShowHint = false;

            sizePolicy.setHorizontalStretch(0);
            sizePolicy.setVerticalStretch(2);
            sizePolicy.setHeightForWidth(m_smithWidget->sizePolicy().hasHeightForWidth());
            m_smithWidget->setSizePolicy(sizePolicy);
            layout->addWidget(m_smithWidget);

            ui->tabWidget->addTab(m_tab_smith, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_smith), QApplication::translate("MainWindow", "Smith", 0));

            m_smithWidget->xAxis->setTicks(false);
            m_smithWidget->yAxis->setTicks(false);
            m_smithWidget->xAxis->setVisible(false);
            m_smithWidget->yAxis->setVisible(false);
            m_mapWidgets.insert(QStringLiteral("smith_widget"), m_smithWidget);
        }
        if (tab == "tab_user") {
            //createUserTab();
            m_tab_user = new GLWidget();
            m_tab_user->setObjectName(QStringLiteral("tab_user"));

            QHBoxLayout* layout = new QHBoxLayout(m_tab_user);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("horizontalLayout_8"));
            m_userWidget = new CustomPlot(1, m_tab_user);
            qobject_cast<GLWidget*>(m_tab_user)->setPlotter(m_userWidget);
            m_userWidget->setObjectName(QStringLiteral("user_widget"));

            sizePolicy.setHorizontalStretch(0);
            sizePolicy.setVerticalStretch(2);
            sizePolicy.setHeightForWidth(m_userWidget->sizePolicy().hasHeightForWidth());
            m_userWidget->setSizePolicy(sizePolicy);
            layout->addWidget(m_userWidget);

            ui->tabWidget->addTab(m_tab_user, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_user), QApplication::translate("MainWindow", "User defined", 0));
            m_mapWidgets.insert(QStringLiteral("user_widget"), m_userWidget);
        }
#ifndef NO_MULTITAB
        if (tab == "tab_multi") {
            m_tab_multi = new GLWidget();
            m_tab_multi->setObjectName(QStringLiteral("tab_multi"));
            QVBoxLayout* layout = new QVBoxLayout(m_tab_multi);
            layout->setSpacing(6);
            layout->setContentsMargins(11, 11, 11, 11);
            layout->setObjectName(QStringLiteral("layout_multi"));

            ui->tabWidget->addTab(m_tab_multi, QString());
            ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_multi), QApplication::translate("MainWindow", "Multi", 0));
            ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_multi), false);
            ui->actionPrint->setEnabled(true);
            //ui->tabWidget->widget(ui->tabWidget->indexOf(m_tab_multi))->setVisible(false);
        }
#endif
    }

#ifndef NO_MULTITAB
    if (!g_developerMode) {
        ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_user), false);
//        ui->tabWidget->widget(ui->tabWidget->indexOf(m_tab_user))->setVisible(false);
    }
#endif
    // S21 not implemented yet
    ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_s21), false);

    QString path = Settings::setIniFile();
    QSettings set(path, QSettings::IniFormat);
    set.beginGroup("Settings");
    QString strColor = set.value("chart-background", "#ffffff").toString();
    QColor color = QColor::fromString(strColor);
    setChartBackground(color);
    set.endGroup();

    ui->tabWidget->setCurrentIndex(0);

#ifndef NO_MULTITAB
    ui->tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tabWidget->tabBar(), &QTabBar::customContextMenuRequested, [=](const QPoint& point) {
        if (point.isNull())
            return;
        QMenu menu(this);
        menu.setStyleSheet(Style::menu());
        QTabBar* tabBar = ui->tabWidget->tabBar()         ;
        int tabIndex = tabBar->tabAt(point);
        QWidget* tab = ui->tabWidget->widget(tabBar->tabAt(point));
        QString tabName = tab->objectName();
        if (tabName != "tab_multi") {
            if (m_multiTabData.isFull())
                return;
            // TDR can't join Multi -- see the matching exclusion (and
            // explanation) in menuMultiTab(). This is a separate menu
            // (right-click directly on a chart tab, vs. on the Multi tab
            // itself or the "+" button), so it needs its own guard.
            if (tabName == "tab_tdr")
                return;
            menu.addAction(tr("Move chart to the tab Multi"), this, [=]() {
                toMultiTab(tabIndex);
            });
        } else {
            menuMultiTab(menu);
        }
        QString style = Style::menu();
        menu.setStyleSheet(style);
        menu.exec(tabBar->mapToGlobal(point));
    });

    QToolButton *btn = new QToolButton();
    btn->setStyleSheet(Style::toolButton());
    btn->setText("+");
    btn->setToolTip(tr("Add multi-charts"));
    connect(btn, &QAbstractButton::clicked, this, [=]() {
        showMultiTab();
    });
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QToolButton::customContextMenuRequested, [=](const QPoint& point) {
        if (point.isNull())
            return;
        QMenu menu(this);
        QString style = Style::menu();
        menu.setStyleSheet(style);
        menuMultiTab(menu);
        if (menu.exec(btn->mapToGlobal(point)) != nullptr) {
            if (!m_multiTabData.tabs.isEmpty()) {
                ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_multi), true);
                ui->actionPrint->setEnabled(false);
                //ui->tabWidget->widget(ui->tabWidget->indexOf(m_tab_multi))->setVisible(true);
                ui->tabWidget->setCurrentWidget(m_tab_multi);
            } else {
                ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_multi), false);
                ui->actionPrint->setEnabled(true);
                //ui->tabWidget->widget(ui->tabWidget->indexOf(m_tab_multi))->setVisible(false);
                ui->tabWidget->setCurrentWidget(m_tab_swr);
            }
        }
    });
    ui->tabWidget->setCornerWidget(btn, Qt::TopRightCorner);
    // btn is created here at runtime, not in mainwindow.ui, so it can't be
    // placed via <tabstops> -- splice it into the chain setupUi() already
    // established (tabWidget -> scanModeCombo) instead, right after the
    // tab widget. Settings/Export/Import/Print/Screenshot/Data-from-AA
    // used to continue this chain as a row of QPushButtons right under
    // tabWidget; they're QMenuBar actions now (see mainwindow.ui), which
    // aren't QWidgets and don't participate in setTabOrder() at all -- the
    // menu bar has its own Alt-key-driven keyboard access instead.
    setTabOrder(ui->tabWidget, btn);
    setTabOrder(btn, ui->scanModeCombo);
#endif
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    Q_UNUSED(index)
    QString str = ui->tabWidget->currentWidget()->objectName();
    updateGraph();
    emit currentTab (str);    
    QTimer::singleShot(20, m_markers, SLOT(redraw()));
}

QCustomPlot* MainWindow::getCurrentPlot()
{
    QWidget* w = ui->tabWidget->currentWidget();
    QList<QCustomPlot*> children = w->findChildren<QCustomPlot*>();
    if (children.isEmpty())
        return nullptr;
    return children[0];
}

void MainWindow::changeColorTheme(bool _dark)
{
    m_darkColorTheme = _dark;
    Style::setDarkMode(_dark);

    // Native/Fusion rendering off Style::palette() for everything except
    // Single/Continuous/Full's checked (running, green) state, which
    // carries actual functional meaning in its color -- that's state, not
    // decoration, so it applies the same in both themes. The disabled
    // state used to get the same hardcoded-both-themes treatment (a fixed
    // dark gray, rgb(59,59,59)/rgb(119,119,119)) but that only ever read
    // correctly against the Dark canvas -- on Light it painted a dark
    // charcoal island that clashed with everything else. Left native now,
    // same as every other disabled control in the app: Fusion derives a
    // theme-correct disabled look from Style::palette()'s own Button/
    // ButtonText colors (already set below), no per-widget override needed.
    qApp->setStyle(QStyleFactory::create("fusion"));
    qApp->setPalette(Style::palette());

    QString style = "QPushButton:checked{"
            "background-color: rgb(0, 178, 90);}";
    ui->singleStart->setStyleSheet(style);
    ui->continuousStartBtn->setStyleSheet(style);
    ui->fullBtn->setStyleSheet(style);

    if (m_markers != NULL)
        m_markers->changeColorTheme(m_darkColorTheme);

    // Measurements::changeColorTheme() is gone -- it only ever existed to
    // re-color m_graphHint's PopUp, which setStyles() below now handles for
    // free (it's a plain docked, normally-themed widget -- see
    // setGraphHintWidgets()).
    m_measurements->on_redrawGraphs();

    setStyles(); // re-skin the Style::-driven main window chrome for the new theme
}


void MainWindow::setChartBackground(QColor _color)
{
    if (!_color.isValid())
        return;
    QBrush brush(_color);
    QColor inverse(255-_color.red(), 255-_color.green(), 255-_color.blue());
    QPen pen(inverse);
    pen.setWidth(1);

    foreach (QCustomPlot *_plot, m_mapWidgets) {
        _plot->setBackground(brush);
        _plot->xAxis->setTickLabelColor(inverse);
        _plot->xAxis->setLabelColor(inverse);
        _plot->xAxis->setSubTickPen(pen);
        _plot->xAxis->setBasePen(pen);
        _plot->xAxis->setTickPen(pen);

        _plot->yAxis->setTickLabelColor(inverse);
        _plot->yAxis->setLabelColor(inverse);
        _plot->yAxis->setSubTickPen(pen);
        _plot->yAxis->setBasePen(pen);
        _plot->yAxis->setTickPen(pen);

        // yAxis2 (TDR's |Z| scale, S21's Stage scale) was left out here, so it
        // stayed at its default black even after the rest of the axes followed
        // the theme -- unreadable against a dark background.
        _plot->yAxis2->setTickLabelColor(inverse);
        _plot->yAxis2->setLabelColor(inverse);
        _plot->yAxis2->setSubTickPen(pen);
        _plot->yAxis2->setBasePen(pen);
        _plot->yAxis2->setTickPen(pen);
    }

    if (m_measurements != nullptr) {
        m_measurements->setSmithBackgroundColor(_color);
        m_measurements->setSmithForegroundColor(inverse);
        m_smithWidget->replot();
    }
}

void MainWindow::createUserTab()
{
    m_tab_user = new GLWidget();
    m_tab_user->setObjectName(QStringLiteral("tab_user"));

    QHBoxLayout* layout = new QHBoxLayout(m_tab_user);
    layout->setSpacing(6);
    layout->setContentsMargins(11, 11, 11, 11);
    layout->setObjectName(QStringLiteral("horizontalLayout_user"));
    m_userWidget = new CustomPlot(1, m_tab_user);
    qobject_cast<GLWidget*>(m_tab_user)->setPlotter(m_userWidget);
    m_userWidget->setObjectName(QStringLiteral("user_widget"));

    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(2);
    sizePolicy.setHeightForWidth(m_userWidget->sizePolicy().hasHeightForWidth());
    m_userWidget->setSizePolicy(sizePolicy);
    layout->addWidget(m_userWidget);

    ui->tabWidget->addTab(m_tab_user, QString());
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_user), QApplication::translate("MainWindow", "User defined", 0));
    m_mapWidgets.insert(QStringLiteral("user_widget"), m_userWidget);
}

