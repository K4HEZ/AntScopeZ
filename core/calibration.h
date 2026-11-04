#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <QObject>
#include <analyzer/analyzerparameters.h>
#include <analyzer/analyzerpro.h>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "usermessage.h"
//#include <shlobj.h>

enum {CALIB_NONE = 0, CALIB_OPEN, CALIB_SHORT, CALIB_LOAD, CALIB_TEST};

class CalibData
{
public:
    CalibData()
    {
        //
    }
    ~CalibData()
    {
        //
    }
    void setData(double fq, double re, double im, double r, double x)
    {
        m_fq.append(fq);
        m_re.append(re);
        m_im.append(im);
        m_r.append(r);
        m_x.append(x);
    }
    double getFq(int number)
    {
        return m_fq.at(number);
    }
    double getRe(int number)
    {
        return m_re.at(number);
    }
    double getIm(int number)
    {
        return m_im.at(number);
    }
    double getR(int number)
    {
        return m_r.at(number);
    }
    double getX(int number)
    {
        return m_x.at(number);
    }
    int getSize()
    {
        return m_fq.length();
    }
    bool loadData(QString path, double* Z0)
    {
        clear();

        QString sPathName = path;

        if (sPathName.isEmpty())
        {
            return false;
        }

        QFile ifs(sPathName);

        if (!ifs.open(QFile::ReadWrite))
        {
            return false;
        }
        QTextStream in(&ifs);
        bool bGood = true;

        int iLines=0, iPoints=0;

        double  fqmul = 1000.0; // Default is GHz
        int iUnit = 1; // Default is S
        int iFormat = 1; // Default is MA

        QString line;// Whole string
        char strn[5][100]; // Substrings

        double f, param1, param2; // for reading S11 data lines


        do
        {
            line = in.readLine();
            iLines++;

            if ( (line.length() > 2) && (line[0] == '#')) // Option line
            {
                line.remove(0,1);
                bool bErr = false;

                int ns = sscanf(line.toLocal8Bit(), "%s %s %s %s %s", strn[0], strn[1], strn[2], strn[3], strn[4]);

                for (int i=0; i<ns; i++)
                {
                    // Frequency unit

                    if (!strcmp(strn[i], "GHz"))
                        fqmul = 1000.0;
                    else
                    if (!strcmp(strn[i], "MHz"))
                        fqmul = 1.0;
                    else
                    if (!strcmp(strn[i], "KHz"))
                        fqmul = 0.001;
                    else
                    if (!strcmp(strn[i], "Hz"))
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

                            *Z0 = atof(strn[i]);

                            if ( (*Z0<=0) || (*Z0>10000) )
                            {
                                bErr = true;
                                break;
                            }
                        }
                        else
                        {
                            bErr = true;
                            break;
                        }
                    }
                    else
                    {
                        bErr = true;
                        break;
                    }
                }

                // Check possible combinations

                if ( (iUnit == 1) && (iFormat == 1) ) // S, MA
                {
                }
                else
                if ( (iUnit == 1) && (iFormat == 2) ) // S, RI
                {
                }
                else
                if ( (iUnit == 2) && (iFormat == 2) ) // Z, RI
                {
                }
                else
                {
                    bErr = true;
                }

                if (bErr == true)
                {
                    return false;
                }

                continue;
            }

            if ( (strstr(line.toLocal8Bit(), "!") != NULL) || (strstr(line.toLocal8Bit(), ".") == NULL) ) // Comment or void line
                continue;

            // Scan data lines

            if ( sscanf(line.toLocal8Bit(), "%lf %lf %lf", &f, &param1, &param2) != 3)
            {
                return false;
            }

            double r = 0,x = 0;

            if ( (iUnit == 1) && (iFormat == 1) ) // S, MA
            {
                    // param1=|G|, param2=angle(deg)
                    double Gr = param1 * cos(param2/180.0*M_PI);
                    double Gi = param1 * sin(param2/180.0*M_PI);

                    r = (1-Gr*Gr-Gi*Gi)/((1-Gr)*(1-Gr)+Gi*Gi);
                    x = (2*Gi)/((1-Gr)*(1-Gr)+Gi*Gi);
            }
            else
            if ( (iUnit == 1) && (iFormat == 2) ) // S, RI
            {
                    // param1=Gr, param2=Gi
                    r = (1-param1*param1-param2*param2)/((1-param1)*(1-param1)+param2*param2);
                    x = (2*param2)/((1-param1)*(1-param1)+param2*param2);
            }
            else
            if ( (iUnit == 2) && (iFormat == 2) ) // Z, RI
            {
                    r = param1;
                    x = param2;
            }

            if ( qIsNaN(r) || (r<0) )
                r = 0;
            if ( qIsNaN(x) )
                x = 0;

            double Gre = (r*r-1+x*x)/((r+1)*(r+1)+x*x);
            double Gim = (2*x)/((r+1)*(r+1)+x*x);

            m_fq.append(f*fqmul);
            m_re.append(Gre);
            m_im.append(Gim);
            m_r.append(r*(*Z0));
            m_x.append(x*(*Z0));
            iPoints++;
        }while (!line.isNull());

        if (bGood && (iPoints>1) )
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    bool saveData(QString name, double Z0)
    {
        QFile file(name);
        if(file.exists())
        {
            QFile::remove(name);
        }

        if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))//if (!file.open(QFile::ReadWrite))
        {
            return false;
        }

        QTextStream out(&file);

        out << "! Touchstone file generated by AntScopeZ";

        out << " (calibration data)\n";


        // S, RI
        out << "# MHz S RI R " << QString::number(Z0) << "\n";//Rswr <<
        out << "! Format: Frequency S-real S-imaginary (normalized to " << Z0
            << " Ohm)\n";

        // Data
        for (int i = 0; i < m_fq.length(); ++i)
        {
            QString s;

            s = QString("%1").arg(m_fq.at(i));		// Fq
            out << s << " ";

            // S, RI


            double Gre = m_re.at(i);
            double Gim = m_im.at(i);

            if (!qIsNaN(Gre))
                s = QString("%1").arg(Gre);		// Real
            else
                s = "0";
            out << s << " ";

            if (!qIsNaN(Gim))
                s = QString("%1").arg(Gim);		// Imaginary
            else
                s = "0";
            out << s << "\n";
        }

        out.flush();

        return true;
    }
    void clear()
    {
        m_fq.clear();
        m_re.clear();
        m_im.clear();
        m_r.clear();
        m_x.clear();
    }

private:
    QList<double> m_fq;
    QList<double> m_re;
    QList<double> m_im;
    QList<double> m_r;
    QList<double> m_x;
};

class Calibration : public QObject
{
    Q_OBJECT
public:
    explicit Calibration(QObject *parent = 0);
    ~Calibration();

    bool isAnalyzerConnected() { return !m_serial.isEmpty(); }
    void setSerial(const QString& _serial=QString()) { m_serial = _serial; }
    QString getSerial() { return m_serial; }
    void start(bool force=false);
    bool getCalibrationPerformed(void);
    bool getCalibrationEnabled(void);
    void setAnalyzer(AnalyzerPro *analyzer);//, Measurements *measurements);
    bool isCalibrationPerformed(){return m_OSLCalibrationPerformed;}
    bool interpolateS(double fq, double &reO, double &imO, double &reS, double &imS, double &reL, double &imL);
    void applyCalibration(double MMR, double MMI, // Measured
                          double MOR, double MOI, double MSR, double MSI, double MLR, double MLI, // Measured parameters of cal standards
                          double SOR, double SOI, double SSR, double SSI, double SLR, double SLI, // Actual parameters of cal standards
                          double& MAR, double& MAI);

    QString getCalibrationPath();
    QString getOpenFileName();
    QString getShortFileName();
    QString getLoadFileName();
    bool openFileExists();
    bool shortFileExists();
    bool loadFileExists();
    // Each standard's own file mtime, or an invalid QDateTime if that
    // standard hasn't been captured yet -- drives the OSL tab's per-row
    // "Completed <date/time>" / "Not Performed" status text.
    QDateTime openTimestamp();
    QDateTime shortTimestamp();
    QDateTime loadTimestamp();
    // Shared session parameters from the per-device calibration.ini
    // (written by on_wizardCommit()) -- the Z0/range/points every standard
    // in the current calibration was captured with, not the live/pending
    // Settings values. For display only.
    double calibZ0();
    double calibStartFqMHz();
    double calibStopFqMHz();
    int calibDotsNumber();

    // Test's raw per-point result from whichever on_testCalibration() run
    // is most recent -- never persisted (see on_testCalibration()'s own
    // comment for why): correctness depends on whatever correction
    // happens to be active at the moment it ran (AntScopeZ OSL, a VNA Air
    // selected slot, the analyzer's own on-board calibration, or none),
    // which can silently change later. testResultR(i)/testResultX(i) are
    // the measured values in Ohms; callers plot them against Z0/0
    // themselves.
    int testResultCount();
    double testResultFqMHz(int i);
    double testResultR(int i);
    double testResultX(int i);

    double getZ0 () const {return m_Z0;}
    void setZ0 (double _Z0) {m_Z0 = _Z0;}
    int dotsNumber() { return ((m_dotsNumber < 0) ? 500 : m_dotsNumber); }
    void setDotsNumber(int _dots) { m_dotsNumber = (_dots > 2000) ? 2000 : _dots; }
    void init(const QString& _serial=QString());
    void cancel();

private:
    // Live/committed calibration data, used by interpolateS()/
    // applyCalibration() -- only ever replaced wholesale by
    // on_wizardCommit(), never touched mid-wizard.
    CalibData m_openData;
    CalibData m_shortData;
    CalibData m_loadData;
    // Wizard staging buffers -- each step's capture lands here first;
    // nothing here reaches m_openData/m_shortData/m_loadData or disk until
    // on_wizardCommit(). on_wizardCancel() just discards these.
    CalibData m_wizardOpenData;
    CalibData m_wizardShortData;
    CalibData m_wizardLoadData;
    // Raw per-point result of the wizard's Test step (or the standalone
    // "Test Calibration" button) -- cleared at the start of each
    // on_testCalibration() run, held here only long enough for whichever
    // caller is showing its plot to read it back. Never written to disk.
    CalibData m_testData;

    int m_state;
    int m_dotsCount;

    double m_Z0;

    bool m_OSLCalibrationEnabled;
    bool m_OSLCalibrationPerformed;

    AnalyzerPro *m_analyzer;
    QSettings * m_settings;

    QString m_openCalibFilePath;
    QString m_shortCalibFilePath;
    QString m_loadCalibFilePath;
    // Calibration/<serial>/calibration.ini -- Z0/range/points for the
    // currently-committed calibration, scoped per device so two analyzers
    // used from the same install never cross-contaminate each other's
    // settings (see the old shared-app-ini version of this).
    QString m_calibrationIniPath;

    void clearCalibration(void);
    QString m_calibrationPath;
    int m_dotsNumber;
    QString m_serial;

    // Loaded from m_calibrationIniPath in init(), written by
    // on_wizardCommit() -- see calibZ0()/calibStartFqMHz()/etc. above.
    double m_calibZ0 = 50;
    double m_calibStartFqMHz = 0;
    double m_calibStopFqMHz = 0;
    int m_calibDotsNumber = 0;

signals:
    void progress(int, int);
    void setCalibrationMode(bool);
    void indicatorVisibleChanged(bool visible);
    void userMessage(UserMessageLevel level, const QString& title, const QString& text);
    // Fires once the wizard's Test step's sweep finishes -- the per-point
    // result is ready to read via testResultCount()/testResultR()/etc. and
    // plot. Separate from progress()'s 0-100% capture animation, which
    // only reports sweep progress, not "the data's ready to read."
    void testCaptureFinished();
    // on_wizardCommit() finished writing the three standards + calibration.ini
    // -- the OSL tab re-reads its status display off of this.
    void wizardCommitted();

public slots:
    void on_newData(RawData _rawData);
    // Wizard step triggers -- each captures into its own staging buffer
    // (m_wizardOpenData/etc.), not the live calibration data. Only
    // on_wizardCommit() makes a capture real.
    void on_startCalibrationOpen();
    void on_startCalibrationShort();
    void on_startCalibrationLoad();
    // Quick sanity check: measures whatever standard is connected (meant
    // for a 50/Z0 load) across the whole sweep, so the wizard's own Test
    // page, or Settings OSL tab's standalone "Test Calibration" button,
    // can plot per-point error against reference bands -- no pass/fail,
    // doesn't touch the three saved standards, and never blocks
    // on_wizardCommit(). Verifies whatever correction is actually active
    // right now (AntScopeZ OSL, a VNA Air selected slot, the analyzer's
    // own on-board calibration, or none) -- not specific to OSL. Never
    // persisted: the result would silently go stale the moment settings,
    // the analyzer, or its own active calibration changed, so there's
    // nothing here to save or reload later -- just re-run it.
    void on_testCalibration();
    // Aborts whichever wizard step (Open/Short/Load/Test) is currently in
    // progress and discards all four staging buffers -- the wizard's
    // Cancel button, at any page.
    void on_cancelCalibration();
    // The wizard's Finish button -- the one moment a wizard run actually
    // changes anything: copies the three staging buffers into the live
    // calibration data, writes the .s1p files, and records Z0/range/points
    // into calibration.ini. Until this runs, nothing on disk or in the
    // live calibration data has changed, however far the wizard got. Test
    // itself is never part of this -- it's never persisted (see
    // on_testCalibration()).
    void on_wizardCommit();

    void on_enableOSLCalibration(bool enabled);
    void on_crcError();

};


#endif // CALIBRATION_H
