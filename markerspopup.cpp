#include "markerspopup.h"
#include "mainwindow.h"
#include "style.h"


QMap<int, QString> MarkersHeaderColumn::m_mapHeader;

MarkersPopUp::MarkersPopUp(QWidget *parent) : QWidget(parent),
      m_durability(2000),
      m_hiding(true),
      m_x(0),
      m_y(0),
      m_biasX(0),
      m_biasY(0),
      m_mainX(0),
      m_mainY(0),
      m_mainBiasX(0),
      m_mainBiasY(0),
      m_bgColor(0,0,0,180),
      m_penColor(255,255,255,180),
      m_textColor("white")
{
      setWindowFlags(Qt::FramelessWindowHint |        // Disable window decoration
                     Qt::Tool);                       // Don't show it as a separate window
      setAttribute(Qt::WA_TranslucentBackground);     // Make the background transparent
      // WA_ShowWithoutActivating kept this window from ever becoming the active
      // window, which on this window manager also meant it never received mouse
      // input at all -- clicks on "X" (or anywhere in the popup) were silently
      // dropped. Letting it activate normally fixes click delivery; the
      // resulting false "main window lost focus" is handled in
      // MainWindow::event() instead.

      animation.setTargetObject(this);                // Set the animation's target object
      animation.setPropertyName("popupOpacity");      // Set the animated property
      connect(&animation, &QAbstractAnimation::finished, this, &MarkersPopUp::hide); // Connect the
                                                        // animation-finished signal to the hide slot
      QString path = Settings::setIniFile();
      m_settings = new QSettings(path,QSettings::IniFormat);

      m_timer = new QTimer();
      connect(m_timer, &QTimer::timeout, this, &MarkersPopUp::hideAnimation);

      initLayout();
}

void MarkersPopUp::setName(QString name)
{
    m_name = name;
    m_settings->beginGroup(m_name);
    if(m_name == "Markers")
    {
        m_x = m_settings->value("x",861).toInt();
        m_y = m_settings->value("y",127).toInt();
        m_mainX = m_settings->value("mainX",169).toInt();
        m_mainY = m_settings->value("mainY",101).toInt();
        m_mainBiasX = m_settings->value("mainBiasX",692).toInt();
        m_mainBiasY = m_settings->value("mainBiasY",26).toInt();
    }
//    QWidget* widget = parentWidget() != nullptr ? parentWidget() : qApp->activeWindow();
//    QPoint pt = widget->mapToGlobal(widget->rect().center());
//    QScreen* pScreen = QGuiApplication::screenAt(pt);
//    QRect availableScreenSize = pScreen->availableGeometry();
    int widthDesc = MainWindow::m_mainWindow->width();
    int heightDesc = MainWindow::m_mainWindow->height();
    if((m_x > widthDesc - width()) || (m_x < 0))
    {
        m_x = 500;
    }
    if( (m_y > heightDesc - height()) || (m_y < 0))
    {
        m_y = 500;
    }

    m_settings->endGroup();

    setGeometry(m_x,m_y,width(),height());
}

MarkersPopUp::~MarkersPopUp()
{
    m_settings->beginGroup(m_name);
    m_settings->setValue("x",m_x);
    m_settings->setValue("y",m_y);
    m_settings->setValue("mainX",m_mainX);
    m_settings->setValue("mainY",m_mainY);
    m_settings->setValue("mainBiasX",m_mainBiasX);
    m_settings->setValue("mainBiasY",m_mainBiasY);
    QList<int> buttons = getColumns();
    QString header;
    for (int i=0; i<buttons.size(); i++)
        header += QString::number(buttons[i]) + ",";
    header.remove(header.length()-1, 1);
    m_settings->setValue("header", header);
    m_settings->endGroup();

    delete m_settings;
}

void MarkersPopUp::initLayout()
{
      //fillHeaderMap();
      createHeader();
      setLayout(&m_layout);
      updateTable();
}

void MarkersPopUp::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect roundedRect;
    roundedRect.setX(rect().x() + 5);
    roundedRect.setY(rect().y() + 5);
    roundedRect.setWidth(rect().width() - 10);
    roundedRect.setHeight(rect().height() - 10);

    painter.setBrush(QBrush(m_bgColor));
    painter.setPen(m_penColor);

    painter.drawRoundedRect(roundedRect, 5, 5);
}


void MarkersPopUp::on_remove()
{
    QString str = sender()->objectName();
    str.remove(0,2);
    int markerIndex = str.toInt();
    emit removeMarker(markerIndex);
}

QList <QStringList> MarkersPopUp::getPopupList()
{ // print support

    QList <QStringList> retList;
    QStringList tempList;

    // TODO
//    for(int i = 0; i < m_measurementsList.length(); ++i)
//    {
//        tempList.append(m_markersList.at(i));
//        tempList.append(m_measurementsList.at(i));
//        tempList.append(m_fqList.at(i));
//        tempList.append(m_swrList.at(i));
//        tempList.append(m_rlList.at(i));
//        tempList.append(m_zList.at(i));
//        tempList.append(m_phaseList.at(i));
//        retList.append(tempList);
//        tempList.clear();
//    }
    return retList;
}

void MarkersPopUp::show()
{
    if (m_markers == 0 || m_measurements == 0)
        return;

    setWindowOpacity(0.0);

    animation.setDuration(150);
    animation.setStartValue(0.0);
    animation.setEndValue(1.0);

    setGeometry(QCursor::pos().x() - 400,
                QCursor::pos().y() - 100,
                width(),
                height());

    QWidget::show();

    animation.start();
    if(m_hiding)
    {
        m_timer->start(m_durability);
    }
}

void MarkersPopUp::focusShow()
{
    //qDebug() << "MarkersPopUp::focusShow()" << m_menuVisible;
    // Was: also called activateWindow() here, unconditionally -- and this
    // runs synchronously every time a marker is added (Markers::add() calls
    // straight into this, no deferral), forcing MainWindow to lose real WM
    // activation to this popup on every single marker placed. That's the
    // same class of activation race that was making the plot's wheel/drag
    // look "stuck" (see the graphBriefHint fix), just hitting the
    // Start/Delete buttons instead here. show()+raise() alone still makes
    // this window visible and topmost; WA_ShowWithoutActivating is already
    // off (see the constructor comment), so the WM still grants it real
    // activation if the user actually clicks on it -- that's what needed
    // fixing originally, not forcing activation proactively every time it's
    // shown/refreshed.
    QWidget::show();
    raise();
}

void MarkersPopUp::focusHide()
{
    //qDebug() << "MarkersPopUp::focusHide()" << m_menuVisible;
    if (m_menuVisible) {
        setVisible(true);
        return;
    }
    QWidget::hide();
}

void MarkersPopUp::hideAnimation()
{
    m_timer->stop();
    animation.setDuration(1000);
    animation.setStartValue(1.0);
    animation.setEndValue(0.0);
    animation.start();
}

void MarkersPopUp::hide()
{
    if(getPopupOpacity() == 0.0)
    {
        QWidget::hide();
    }
}

void MarkersPopUp::setPopupOpacity(float opacity)
{
    popupOpacity = opacity;

    setWindowOpacity(opacity);
}

float MarkersPopUp::getPopupOpacity() const
{
    return popupOpacity;
}

void MarkersPopUp::mousePressEvent(QMouseEvent * event)
{
    m_biasX = event->pos().x();
    m_biasY = event->pos().y();
}

void MarkersPopUp::mouseMoveEvent(QMouseEvent * )
{
    m_x = QCursor::pos().x() - m_biasX;
    m_y = QCursor::pos().y() - m_biasY;
    setGeometry(m_x,
                m_y,
                width(),
                height());
    m_mainBiasX = m_x - m_mainX;
    m_mainBiasY = m_y - m_mainY;
}

void MarkersPopUp::MainWindowPos(int x, int y)
{
    m_mainX = x;
    m_mainY = y;

    m_x = x + m_mainBiasX;
    m_y = y + m_mainBiasY;
    setGeometry(m_x,
                m_y,
                width(),
                height());
}

void MarkersPopUp::setPosition(int x, int y)
{
    m_x = x;
    m_y = y;
    setGeometry(m_x,
                m_y,
                width(),
                height());
}

void MarkersPopUp::setTextColor(QString color)
{
    m_textColor = color;
}

void MarkersPopUp::on_translate()
{
//    m_removeLabel.setText(tr("Del"));
//    m_numberLabel.setText(tr("Marker"));
//    m_measurementLabel.setText("#");
//    m_fqLabel.setText(tr("Fq"));
//    m_swrLabel.setText(tr("SWR"));
//    m_rlLabel.setText(tr("RL"));
//    m_zLabel.setText(tr("Z"));
//    m_phaseLabel.setText(tr("Phase"));
}


void MarkersPopUp::createMenu(MarkersHeaderColumn &column)
{
    column.menu = new QMenu(column.button);
    column.menu->setStyleSheet(Style::menu());
    QMapIterator<int, QString> it(MarkersHeaderColumn::headerMap());
    while (it.hasNext()) {
        it.next();
        if (it.key() > MarkersHeaderColumn::fieldFQ) {
            QAction* action = column.menu->addAction(it.value());
            action->setData(it.key());
        }
    }
    connect(column.menu, &QMenu::aboutToShow, this, [=](){
        m_menuVisible = true;
        setVisible(true);
    });
    connect(column.menu, &QMenu::aboutToHide, this, [=](){
        m_menuVisible = false;
        extern MainWindow* g_mainWindow;
        if (g_mainWindow != nullptr)
            g_mainWindow->activateWindow();
    });
    connect(column.menu, &QMenu::triggered, this, [this, column](QAction* act) {
       m_menuVisible = true;
       int index = act->data().toInt();
       if (index == MarkersHeaderColumn::fieldInsert) {
            // inset column
           if (m_headerColumns.size() <= MAX_BUTTONS_NUM) {
               QList<int> buttons = getColumns();
               buttons.insert(column.index+1, MarkersHeaderColumn::fieldSWR);
               QString header;
               for (int i=0; i<buttons.size(); i++)
                   header += QString::number(buttons[i]) + ",";
               m_settings->beginGroup("Markers");
               m_settings->setValue("header", header);
               m_settings->endGroup();
               m_settings->sync();
               clearTable();
               createHeader();
               updateMarkers(m_markers, m_measurements, true);
           }
       } else  if (index == MarkersHeaderColumn::fieldRemove) {
            // remove column
           if (m_headerColumns.size() > MIN_BUTTONS_NUM) {
               QList<int> buttons = getColumns();
               buttons.removeAt(column.index);
               QString header;
               for (int i=0; i<buttons.size(); i++)
                   header += QString::number(buttons[i]) + ",";
               m_settings->beginGroup("Markers");
               m_settings->setValue("header", header);
               m_settings->endGroup();
               m_settings->sync();
               clearTable();
               createHeader();
               updateMarkers(m_markers, m_measurements, true);
           }
       } else {
           QString title = act->text();
           ((QToolButton*)(column.button))->setText(title);
           column.button->setProperty("field_type", index);
           QList<int> buttons = getColumns();
           QString header;
           for (int i=0; i<buttons.size(); i++)
               header += QString::number(buttons[i]) + ",";
           m_settings->beginGroup("Markers");
           m_settings->setValue("header", header);
           m_settings->endGroup();
           m_settings->sync();
        }
       setVisible(true);
       m_menuVisible = false;
       emit changeColumns();
    });
}

void MarkersPopUp::createHeader()
{
    QMap<int, QString>& mapHeader = MarkersHeaderColumn::headerMap();
    m_headerColumns.clear();

    m_settings->beginGroup("Markers");
    QString buttons = m_settings->value("header", "0,1,2,3,4,5,6,7,8,9").toString();
    m_settings->endGroup();
    QList<QString> list = buttons.split(',');
    int column = 0;
    foreach (QString key, list) {
        if (key.isEmpty())
            continue;
        MarkersHeaderColumn data;
        int type = key.toInt();
        data.index = column;
        QToolButton* button = new QToolButton(this);        
        data.button = button;
        button->setStyleSheet(Style::toolButton());
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        button->setProperty("field_type", type);
        if (type > MarkersHeaderColumn::fieldFQ) {
            button->setPopupMode(QToolButton::MenuButtonPopup);
            createMenu(data);
            button->setMenu(data.menu);
        }
        button->setText(mapHeader[type]);
        m_layout.addWidget(data.button, 0 ,column++, Qt::AlignHCenter);
        m_headerColumns << data;
    }
}


QList<int> MarkersPopUp::getColumns()
{
    QList<int> list;
    for (int i=0; i<m_headerColumns.size(); i++) {
        list << m_headerColumns[i].button->property("field_type").toInt();
    }
    return list;
}

void MarkersPopUp::updateMarkers(int markers, int measurements, bool force)
{
    // on_measurementComplete() calls this on every scan tick, including
    // during a continuous scan. Rebuilding unconditionally destroys and
    // recreates every "X" remove button each time, which can delete the
    // button between a mouse press and release and silently eat the click.
    // Values are refreshed separately via updateInfo(), so skip the rebuild
    // when the table shape hasn't actually changed. Insert/remove-column
    // passes force=true since m_rows must be rebuilt to match the new
    // header size even though the marker/measurement counts are unchanged.
    if (!force && markers == m_markers && measurements == m_measurements) {
        return;
    }

    clearTable();

    m_markers = markers;
    m_measurements = measurements;

    if (markers == 0) {
        hide();
        return;
    }

    for (int i=0; i<m_headerColumns.size(); i++) {
        m_layout.addWidget(m_headerColumns[i].button, 0, i);
        m_headerColumns[i].button->show();
    }

    int rowCount = m_measurements==0 ? 1 : m_measurements;
    int rowIndex = 1;
    for (int i=0; i<m_markers; i++) {
        QToolButton* button = new QToolButton(this);
        button->setStyleSheet(Style::toolButton());
        QString str = "RM" + QString::number(i);
        button->setObjectName(str);
        button->setMaximumWidth(20);
        button->setText("X");
        connect(button, &QToolButton::clicked, this, &MarkersPopUp::on_remove);
        m_layout.addWidget(button, rowIndex, 0);
        m_removeButtons << button;
        for (int j=0; j<rowCount; j++) {
            QList<QWidget*> row;
            // Column 0 is the delete button, not a data cell. It is tracked in
            // m_removeButtons instead; keep a placeholder so the remaining
            // indices line up with m_headerColumns.
            row << nullptr;
            for (int k=1; k<m_headerColumns.size(); k++) {
                QLabel* label = new QLabel(this);
                label->setStyleSheet(Style::label());
                label->setAlignment(Qt::AlignCenter);
                row << qobject_cast<QLabel*>(label);
                m_layout.addWidget(label, rowIndex, k);
                label->show();
            }
            m_rows << row;
            rowIndex++;
        }
    }

    updateTable();
}

void MarkersPopUp::updateInfo(QList<QList<QVariant>>& info)
{
    if (info.size() != (m_markers*m_measurements))
        return;

    int rowIndex = 0;
    for (int i=0; i<m_markers; i++) {
        for (int j=0; j<m_measurements; j++) {
            QList<QVariant>& rowInfo = info[rowIndex];
            QList<QWidget*>& rowLabel = m_rows[rowIndex];
            for (int k=1; k<m_headerColumns.size(); k++) { // ignore fieldDelete
                if (j != 0 && k == MarkersHeaderColumn::fieldNum)
                    continue;
                QVariant val = rowInfo[k];
                int type = m_headerColumns[k].button->property("field_type").toInt();
                QString str = formatText(type, val);
                QLabel* label = qobject_cast<QLabel*>(rowLabel[k]);
                label->setText(str);
            }
            rowIndex++;
        }
    }
}

void MarkersPopUp::clearTable(void)
{
    QLayoutItem* item = nullptr;
    while((item=m_layout.takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->setVisible(false);
        delete item; // takeAt() hands ownership of the item to us
    }
    for(int i=0; i<m_rows.size(); i++) {
        QList<QWidget*>& row = m_rows[i];
        for (int j=0; j<row.size(); j++) {
            delete row[j];
        }
    }
    m_rows.clear();

    // The delete buttons used to be stored in m_rows via
    // qobject_cast<QLabel*>(button), which is always null for a QToolButton --
    // so they were never freed and every rebuild left another hidden set of
    // them behind, all sharing the same "RM<n>" object names.
    qDeleteAll(m_removeButtons);
    m_removeButtons.clear();
}

void MarkersPopUp::updateTable()
{
    adjustSize();
}

QString MarkersPopUp::formatText(int type, QVariant v)
{
    if (!v.isValid() || v.toDouble() == DBL_MAX)
        return "";

    QString str;
    switch (type) {
    case MarkersHeaderColumn::fieldDelete:
        break;
    case MarkersHeaderColumn::fieldNum:
        str = QString::number(v.toInt());
        break;
    case MarkersHeaderColumn::fieldSerie:
        str = QString::number(v.toInt());
        break;
    case MarkersHeaderColumn::fieldZ:
        str = v.toString();
        break;
    case MarkersHeaderColumn::fieldZpar:
        str = v.toString();
        break;
    default:
        str = QString::number(v.toDouble(),'f', 2);
        break;
    }
    return str;
}

///////////////////////////////////////////////////////
QMap<int, QString>& MarkersHeaderColumn::headerMap()
{
    if (m_mapHeader.isEmpty()) {
        int i = MarkersHeaderColumn::fieldDelete;
        m_mapHeader.insert(i++, "Del");
        m_mapHeader.insert(i++, "Marker");
        m_mapHeader.insert(i++, " # ");
        m_mapHeader.insert(i++, "FQ, kHz");
        m_mapHeader.insert(i++, "SWR");        // SWR - standing wave ratio
        m_mapHeader.insert(i++, "RL, dB");     // RL - return loss
        m_mapHeader.insert(i++, "Phase°");     // phase - phase
        m_mapHeader.insert(i++, "R, Ohm");     // R - resistance (series model)
        m_mapHeader.insert(i++, "X, Ohm");     // X - reactance (series model)
        m_mapHeader.insert(i++, "Z, Ohm");     // Z - impedance
        m_mapHeader.insert(i++, "L, nH");      // L - inductance (series model)
        m_mapHeader.insert(i++, "C, pF");      // C - capacitance (series model)
        m_mapHeader.insert(i++, "rho");        // rho - magnitude
        m_mapHeader.insert(i++, "|Z|, Ohm");   // |Z| - impedance modulus
        m_mapHeader.insert(i++, "R||, Ohm");   // R|| - resistance (parallel model)
        m_mapHeader.insert(i++, "X||, Ohm");   // X|| - reactance (parallel model)
        m_mapHeader.insert(i++, "Z||, Ohm");   // Z|| - impedance (parallel model)
        m_mapHeader.insert(i++, "L||, nH");    // L|| - inductance (parallel model)
        m_mapHeader.insert(i++, "C||, pF");    // C|| - capacitance (parallel model)

        m_mapHeader.insert(MarkersHeaderColumn::fieldInsert, "Insert column");
        m_mapHeader.insert(MarkersHeaderColumn::fieldRemove, "Remove column");
    }
    return m_mapHeader;
}
