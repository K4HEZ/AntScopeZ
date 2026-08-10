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
extern int g_maxMeasurements; // defined in measurements.cpp
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original measurements.cpp (still in
// measurements.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of Measurements.

void Measurements::saveData(quint32 number, QString path)
{
    if (number >= (quint32)g_maxMeasurements)
        number = g_maxMeasurements-1;

    if(path.indexOf(".asd") >= 0 )
    {
        QFile saveFile(path);

        if (!saveFile.open(QIODevice::WriteOnly))
        {
            qWarning("Couldn't open save file.");
            return;
        }

        QVector <RawData> data;
        if(m_calibration != NULL)
        {
            if(m_calibration->getCalibrationEnabled())
            {
                data = m_measurements.at(number).dataRXCalib;
            }else
            {
                data = m_measurements.at(number).dataRX;
            }
        }else
        {
            data = m_measurements.at(number).dataRX;
        }

        //Dots
        QJsonObject mainObj;
        mainObj["DotsNumber"] = data.length();

        //Measurements
        QJsonArray measurementsArray;
        for(int i = 0; i < data.length(); ++i)
        {
            QJsonObject obj;
            obj["fq"] = data.at(i).fq;
            obj["r"] = data.at(i).r;
            obj["x"] = data.at(i).x;
            measurementsArray.append(obj);
        }
        mainObj["Measurements"] = measurementsArray;

        QJsonDocument saveDoc(mainObj);

        saveFile.write(saveDoc.toJson());
    }
}

void Measurements::loadData(QString path)
{
    if(path.indexOf(".asd") >= 0 )
    {
        QStringList list;
        list = path.split("/");
        if(list.length() == 1)
        {
            list.clear();
            list = path.split("\\");
        }

        QFile loadFile(path);

        if (!loadFile.open(QIODevice::ReadOnly)) {
            g_showMessageBox(NULL, QMessageBox::Information, tr("Error"), tr("Couldn't open saved file."));
            qWarning("Couldn't open saved file.");
            return;
        }

        QByteArray saveData = loadFile.readAll();

        QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
        QJsonObject mainObj = loadDoc.object();

        QJsonArray measureArray = mainObj["Measurements"].toArray();

        int size = measureArray.size();
        if (size < 2) {
            g_showMessageBox(NULL, QMessageBox::Information, tr("Error"), tr("The saved file is too short."));
            qWarning("Couldn't open saved file.");
            return;
        }

        QJsonObject dataObject0 = measureArray.first().toObject();
        RawData data0;
        data0.read(dataObject0);
        double fqMin = data0.fq;
        qint64 fqMinHz = static_cast<qint64>(fqMin * 1000000);
        dataObject0 = measureArray.last().toObject();
        data0.read(dataObject0);
        double fqMax = data0.fq;
        qint64 fqMaxHz = static_cast<qint64>(fqMax * 1000000);
        qint32 dots = size;

        int next = nextPrefix();
        QString nextName = QString("%1> %2").arg(next, 2, 10, QChar('0')).arg(list.last());
        //on_newMeasurement(nextName);
        on_newMeasurement(nextName, fqMinHz, fqMaxHz, dots);

        ProgressDlg* progressDlg = new ProgressDlg();
        progressDlg->setValue(0);
        progressDlg->setProgressData(0, size, 1);
        progressDlg->updateActionInfo(tr("Load measurement"));
        progressDlg->updateStatusInfo(tr("please wait ...."));
        progressDlg->show();
        progressDlg->setWindowModality(Qt::WindowModal);
        QApplication::processEvents();

        for(int i = 0; i < size; ++i)
        {
            QJsonObject dataObject = measureArray[i].toObject();
            RawData data;
            data.read(dataObject);
            on_newData(data);
            if ((i%10) == 0) {
                progressDlg->setValue(i);
                progressDlg->updateStatusInfo(QString(tr("loaded %1 dots, from %2")).arg(i).arg(size));
            }
        }
        progressDlg->hide();
        delete progressDlg;

        emit import_finished(fqMin*1000, fqMax*1000);
    }else
    {
        importData(path);
    }
    on_redrawGraphs();
}

void Measurements::exportData(QString _name, int _type, int _number, bool _applyCable, QString _description)
{
    if (_number < 0 || m_measurements.isEmpty() || (_number >= m_measurements.size()))
        return;

    bool calibr = (m_calibration != nullptr) && (m_calibration->getCalibrationEnabled());
    QVector<RawData> vector;
    if (_applyCable)
    {
        switch(m_farEndMeasurement) {
        case 1:
            vector = calibr ? m_farEndMeasurementsSub.at(_number).dataRXCalib : m_farEndMeasurementsSub.at(_number).dataRX;
            break;
        case 2:
            vector = calibr ? m_farEndMeasurementsAdd.at(_number).dataRXCalib : m_farEndMeasurementsAdd.at(_number).dataRX;
            break;
        default:
            vector = calibr ? m_measurements.at(_number).dataRXCalib : m_measurements.at(_number).dataRX;
            break;
        }
    } else {
        vector = calibr ? m_measurements.at(_number).dataRXCalib : m_measurements.at(_number).dataRX;
    }
    exportData(_name, _type, vector, _description);
}

void Measurements::exportData(QString _name, int _type, QVector<RawData>& vector, QString _description)
{
    int len = vector.length();;
    qInfo() << "Touchstone export:"
            << _name
            << "points:"
            << vector.size();
    if(_name.indexOf(".s1p") >= 0 )
    {
        QFile file(_name);

        if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))//if (!file.open(QFile::ReadWrite))
        {
            qInfo() << "Touchstone open failed:"
                    << _name
                    << file.errorString();
            return;
        }

        QTextStream out(&file);

        out << "! Touchstone file generated by AntScopeZ";
        out << "\n";

        double Rswr = ((m_calibration != NULL) && (m_calibration->getCalibrationEnabled())) ? m_calibration->getZ0() : 50;

        if (_type == 0) // Z, RI
        {
            out << "# MHz Z RI R " << Rswr << "\n";
            out << "! Format: Frequency Z-real Z-imaginary (normalized to " << Rswr << " Ohm)\n";
        }else if (_type == 1) // S, RI
        {
            out << "# MHz S RI R " << Rswr << "\n";
            out << "! Format: Frequency S-real S-imaginary (normalized to " << Rswr << " Ohm)\n";
        }
        else if (_type == 2) // S, MA
        {
            out << "# MHz S MA R " << Rswr << "\n";
            out << "! Format: Frequency S-magnitude S-angle (normalized to " << Rswr << " Ohm, angle in degrees)\n";
        }

        if (!_description.isEmpty())
            out << _description << "\n";

        for (int i = 0; i < len; ++i)
        {
            QString s;

            s = QString("%1").arg(vector.at(i).fq, 0, 'f', 6);		// Fq
            out << s << " ";

            double R = vector.at(i).r;
            double X = vector.at(i).x;

            if (_type == 0) // Z, RI
            {
                if (!qIsNaN(R))
                    s = QString::number(R/Rswr,'g',4);           // R
                else
                    s = "0";
                out << s << " ";
                if (!qIsNaN(X))
                    s = QString::number(X/Rswr,'g',4);           // X
                else
                    s = "0";
                out << s << "\n";
            }
            else
            if (_type == 1) // S, RI
            {
                double Gre = (R*R-Rswr*Rswr+X*X)/((R+Rswr)*(R+Rswr)+X*X);
                double Gim = (2*Rswr*X)/((R+Rswr)*(R+Rswr)+X*X);

                if (!qIsNaN(Gre))
                    s = QString::number(Gre,'g',4);              // Real
                else
                    s = "0";
                out << s << " ";

                if (!qIsNaN(Gim))
                    s = QString::number(Gim,'g',4);              // Imaginary
                else
                    s = "0";
                out << s << "\n";

            }
            else
            if (_type == 2) // S, MA
            {
                double Gre = (R*R-Rswr*Rswr+X*X)/((R+Rswr)*(R+Rswr)+X*X);
                double Gim = (2*Rswr*X)/((R+Rswr)*(R+Rswr)+X*X);

                if (!qIsNaN(Gre))
                    s = QString::number(sqrt(Gre*Gre+Gim*Gim),'g',4);		// Magnitude
                else
                    s = "0";
                out << s << " ";

                if (!qIsNaN(Gim))
                    s = QString::number(atan2(Gim,Gre)/3.1415926*180.0,'g',4);		// Angle
                else
                    s = "0";
                out << s << "\n";
            }
        }
        out.flush();
    }else if(_name.indexOf(".csv") >= 0 )
    {
        QString str;
        QFile file(_name);
        bool result = file.open(QFile::ReadWrite);
        if(result)
        {
            extern bool g_developerMode;
            int rxWidth = g_developerMode ? 6 : 2;
            str = "#Frequency(MHz);R;X";
            file.write( str.toLocal8Bit(), str.length());
            file.write("\r\n", 2);
            for (int i = 0; i < len; ++i)
            {
                str = QString::number(vector.at(i).fq, 'f', 6) +
                "," +//";" +
                QString::number(vector.at(i).r,'f',rxWidth) +
                "," +//";" +
                QString::number(vector.at(i).x,'f',rxWidth);

                file.write( str.toLocal8Bit(), str.length());
                file.write("\r\n", 2);
            }
            file.close();
        }
    }else if(_name.indexOf(".nwl") >= 0 )
    {
        QString str;
        QFile file(_name);
        bool result = file.open(QFile::ReadWrite);
        if(result)
        {
            str = "/\"Freq(MHz)\" \"Rs\" \"Xs\"/";
            file.write( str.toLocal8Bit(), str.length());
            file.write("\r\n", 2);

            for (int i = 0; i < len; ++i)
            {
                str = QString::number(vector.at(i).fq, 'f', 6) +
                " " +//";" +
                QString::number(vector.at(i).r,'f',2) +
                " " +//";" +
                QString::number(vector.at(i).x,'f',2);

                file.write( str.toLocal8Bit(), str.length());
                file.write("\r\n", 2);
            }
            file.close();
        }
    }
}

void Measurements::importData(QString _name, bool /*user_format*/)
{
    QStringList list;
    list = _name.split("/");
    if(list.length() == 1)
    {
        list.clear();
        list = _name.split("\\");
    }
    on_newMeasurement(list.last());

    QFile file(_name);
    bool result = file.open(QFile::ReadOnly);
    if(result)
    {
        QString str = file.readAll();
        double fqMin = DBL_MAX;
        double fqMax = 0;
        QStringList nList = str.split('\n');

        str = nList.takeFirst();
        if (str.at(0) == '#') {
            str.replace('#', ' ');
            on_newUserDataHeader(str.trimmed().split(','));
        }

        while (!nList.isEmpty()) {
            str = nList.takeFirst();
            if (str.isEmpty())
                continue;
            QStringList fields = str.split(',');
            RawData rdata;
            UserData udata;
            bool ok;
            QString field = fields.takeFirst();
            rdata.fq = field.toDouble(&ok);
            if (!ok) {
                qDebug() << "***** ERROR: " << str;
                return;
            }
            udata.fq = rdata.fq;
            fqMin = qMin(fqMin, rdata.fq);
            fqMax = qMax(fqMax, rdata.fq);

            field = fields.takeFirst();
            rdata.r = field.toDouble(&ok);
            if (!ok) {
                qDebug() << "***** ERROR: " << str;
                return;
            }
            field = fields.takeFirst();
            rdata.x = field.toDouble(&ok);
            if (!ok) {
                qDebug() << "***** ERROR: " << str;
                return;
            }
            while (!fields.isEmpty()) {
                udata.values.append(fields.takeFirst().toDouble(&ok));
                if (!ok) {
                    qDebug() << "***** ERROR: " << str;
                    return;
                }
            }
            on_newUserData(rdata, udata);
        }
        emit import_finished(fqMin*1000, fqMax*1000);
        on_redrawGraphs();
    }
}

void Measurements::importData(QString _name)
{
    if(_name.indexOf(".s1p") >= 0 )
    {
        QStringList list;
        list = _name.split("/");
        if(list.length() == 1)
        {
            list.clear();
            list = _name.split("\\");
        }

        QString sPathName = _name;

        if (sPathName.isEmpty())
        {
            return;
        }

        QFile ifs(sPathName);

        if (!ifs.open(QFile::ReadWrite))
        {
            return;
        }
        QTextStream in(&ifs);
        bool bGood = true;

        int iLines=0, iPoints=0;

        double  fqmul = 1000.0; // Default is GHz
        int iUnit = 1; // Default is S
        int iFormat = 1; // Default is MA

        QString line;//char str[1000]; // Whole string
        char strn[5][100]; // Substrings

        double f, param1, param2; // for reading S11 data lines

        double Z0 = 50;

        double fqMin = DBL_MAX;
        double fqMax = 0;
        QList<RawData> rawArray;
        do//while (ifs.isOpen() && (!ifs.eof()))
        {
            line = in.readLine();
            line = line.toUpper();
            iLines++;

            if ( (line.length() > 2) && (line[0] == '#')) // Option line
            {
                line.remove(0,1);
                int ns = sscanf(line.toLocal8Bit(), "%s %s %s %s %s", strn[0], strn[1], strn[2], strn[3], strn[4]);
                for (int i=0; i<ns; i++)
                {
                    // Frequency unit

                    if (!strcmp(strn[i], "GHZ"))
                        fqmul = 1000.0;
                    else
                    if (!strcmp(strn[i], "MHZ"))
                        fqmul = 1.0;
                    else
                    if (!strcmp(strn[i], "KHZ"))
                        fqmul = 0.001;
                    else
                    if (!strcmp(strn[i], "HZ"))
                        fqmul = 0.000001;
                    else

                    // Parameter

                    if (!strcmp(strn[i], "S"))
                        iUnit = 1;
                    else
                    if (!strcmp(strn[i], "Z"))
                        iUnit = 2;
                    else

                    // Format

                    if (!strcmp(strn[i], "MA"))
                        iFormat = 1;
                    else
                    if (!strcmp(strn[i], "RI"))
                        iFormat = 2;
                    else

                    // R n

                    if (!strcmp(strn[i], "R"))
                    {
                        if ( i < (ns-1) )
                        {
                            i++;

//                            setlocale(LC_NUMERIC,"C");
                            Z0 = atof(strn[i]);
//                            setlocale(LC_NUMERIC,"");

                            if ( (Z0<=0) || (Z0>10000) )
                            {
                                //bErr = true;
                                //break;
                                return;
                            }
                        }
                        else
                        {
                            //bErr = true;
                            //break;
                            return;
                        }
                    }
                    else
                    {
                        //bErr = true;
                        //break;
                        return;
                    }
                }

                // Check possible combinations
                if(! (((iUnit == 1) && (iFormat == 1)) // S, MA
                        || ((iUnit == 1) && (iFormat == 2))  // S, RI
                        || ((iUnit == 2) && (iFormat == 2))  // Z, RI
                    ))
                {
                    return;
                }

                continue;
            }

            if ( (strstr(line.toLocal8Bit(), "!") != NULL) || (strstr(line.toLocal8Bit(), ".") == NULL) ) // Comment or void line
                continue;

            // Scan data lines

            if ( sscanf(line.toLocal8Bit(), "%lf %lf %lf", &f, &param1, &param2) != 3)
            {
                return;
            }

            double r = 0,x = 0;

            if ( (iUnit == 1) && (iFormat == 1) ) // S, MA
            {
                    double Gr = param1 * cos(param2/180.0*M_PI);
                    double Gi = param1 * sin(param2/180.0*M_PI);

                    r = (1-Gr*Gr-Gi*Gi)/((1-Gr)*(1-Gr)+Gi*Gi);
                    x = (2*Gi)/((1-Gr)*(1-Gr)+Gi*Gi);
            }
            else
            if ( (iUnit == 1) && (iFormat == 2) ) // S, RI
            {
                    r = (1-param1*param1-param2*param2)/((1-param1)*(1-param1)+param2*param2);
                    x = (2*param2)/((1-param1)*(1-param1)+param2*param2);
            }
            else
            if ( (iUnit == 2) && (iFormat == 2) ) // Z, RI
            {
                    r = param1;
                    x = param2;
            }
            else
            {
                // Bug
            }

            if ( qIsNaN(r) || (r<0) )
            {
                r = 0;
            }
            if ( qIsNaN(x) )
            {
                x = 0;
            }

            RawData data;
            data.fq = f*fqmul;
            data.r =r*(Z0);
            data.x =x*(Z0);
            //on_newData(data);
            rawArray.append(data);
            iPoints++;
            fqMin = qMin(fqMin, data.fq);
            fqMax = qMax(fqMax, data.fq);
        }while (!line.isNull());

        on_newMeasurement(list.last(), static_cast<qint64>(fqMin*1000000), static_cast<qint64>(fqMax*1000000), iPoints);
        foreach (auto data, rawArray) {
            on_newData(data);
        }
        emit import_finished(fqMin*1000, fqMax*1000);

        if (bGood && (iPoints>1) )
        {            
            return;
        }
        else
        {
            return;
        }
    }
    else if(_name.indexOf(".csv") >= 0 )
    {        
        //{ TODO USER_DATE debug
//        if (g_developerMode) {
//            importData(_name, true);
//            return;
//        }
        //}

        QStringList list;
        list = _name.split("/");
        if(list.length() == 1)
        {
            list.clear();
            list = _name.split("\\");
        }
//        on_newMeasurement(list.last());
        QList<RawData> rawArray;
        QFile file(_name);
        bool result = file.open(QFile::ReadOnly);
        if(result)
        {
            QString str = file.readAll();
            double fqMin = DBL_MAX;
            double fqMax = 0;
            QStringList nList = str.split('\n');

            double mul=1.0;
            QString strFQ = nList.at(0);
            if (strFQ.contains("kHz", Qt::CaseInsensitive))
                mul = 0.001;
            else if (strFQ.contains("MHz", Qt::CaseInsensitive))
                mul = 1;
            else if (strFQ.contains("GHz", Qt::CaseInsensitive))
                mul = 1000;
            else if (strFQ.contains("Hz", Qt::CaseInsensitive))
                mul = 0.000001;
            else {
                QStringList dList = strFQ.split(',');
                if(dList.length() == 3)
                {
                    RawData data;
                    data.fq = dList.at(0).toDouble()*mul;
                    data.r = dList.at(1).toDouble();
                    data.x = dList.at(2).toDouble();
                    //on_newData(data);
                    rawArray.append(data);
                    fqMin = qMin(fqMin, data.fq);
                    fqMax = qMax(fqMax, data.fq);
                }

            }
            for(int i = 1; i < nList.length(); ++i)
            {
                QStringList dList = nList.at(i).split(',');
                if(dList.length() == 3)
                {
                    RawData data;
                    data.fq = dList.at(0).toDouble()*mul;
                    data.r = dList.at(1).toDouble();
                    data.x = dList.at(2).toDouble();
                    //on_newData(data);
                    rawArray.append(data);
                    fqMin = qMin(fqMin, data.fq);
                    fqMax = qMax(fqMax, data.fq);
                }
            }
            on_newMeasurement(list.last(), static_cast<qint64>(fqMin*1000000), static_cast<qint64>(fqMax*1000000), rawArray.length());
            foreach (auto data, rawArray) {
                on_newData(data);
            }
            emit import_finished(fqMin*1000, fqMax*1000);
        }
    }else if(_name.indexOf(".nwl") >= 0 )
    {
        QStringList list;
        list = _name.split("/");
        if(list.length() == 1)
        {
            list.clear();
            list = _name.split("\\");
        }
//        on_newMeasurement(list.last());

        QList<RawData> rawArray;
        QFile file(_name);
        bool result = file.open(QFile::ReadOnly);
        if(result)
        {
            QString str = file.readAll();

            double fqMin = DBL_MAX;
            double fqMax = 0;
            QStringList nList = str.split('\n');

            double mul=1.0;
            QString strFQ = nList.at(0);
            if (strFQ.contains("kHz", Qt::CaseInsensitive))
                mul = 0.001;
            else if (strFQ.contains("MHz", Qt::CaseInsensitive))
                mul = 1;
            else if (strFQ.contains("GHz", Qt::CaseInsensitive))
                mul = 1000;
            else if (strFQ.contains("Hz", Qt::CaseInsensitive))
                mul = 0.000001;

            for(int i = 1; i < nList.length(); ++i)
            {
                QStringList dList = nList.at(i).split(' ');
                if(dList.length() ==3)
                {
                    RawData data;
                    data.fq = dList.at(0).toDouble()*mul;
                    data.r = dList.at(1).toDouble();
                    data.x = dList.at(2).toDouble();
                    //on_newData(data);
                    rawArray.append(data);                    fqMin = qMin(fqMin, data.fq);
                    fqMax = qMax(fqMax, data.fq);
                }
            }
            on_newMeasurement(list.last(), static_cast<qint64>(fqMin*1000000), static_cast<qint64>(fqMax*1000000), rawArray.length());
            foreach (auto data, rawArray) {
                on_newData(data);
            }
            emit import_finished(fqMin*1000, fqMax*1000);
        }
    } else {
        g_showMessageBox(nullptr, QMessageBox::Information, tr("Load data"), tr("Oops, this format is not supported!"), QMessageBox::Close);
    }
}

int Measurements::nextPrefix()
{
    int next = 0;
    for (int idx=0; idx<m_measurements.size(); idx++)
    {
        QString existed = m_measurements[idx].name;
        if (existed.indexOf('>') == 2) {
            QString num = existed.left(2);
            bool ok = false;
            int prefix = num.toInt(&ok);
            if (ok) {
                next = qMax(next, prefix);
            }
        }
    }
    next++;
    if (next > 99)
        next = 1;
    return next;
}

