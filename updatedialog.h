#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>
#include <QTimer>
#include <qdebug.h>
#include <QMessageBox>

namespace Ui {
class UpdateDialog;
}

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(QWidget *parent = 0);
    ~UpdateDialog();
    void setStatusText(QString text);
    void setMainText(QString text);
    // Ends the dialog in a "here's the result, close when ready" state
    // rather than auto-closing -- used once a firmware download finishes,
    // so the save-path message in statusText stays on screen instead of
    // vanishing (previously this dialog only ever auto-closed at 100%
    // device-flashing progress; there's no flashing step any more).
    void setFinished(QString statusText);
//    void updateByFile();

signals:
    void update();
private slots:
    void on_updateBtn_clicked();
    void on_cancelBtn_clicked();
    void on_errorTimerTick();

public slots:
    void on_percentChanged(qint32 percent);

private:
    Ui::UpdateDialog *ui;

    QTimer *m_errorTimer;
};

#endif // UPDATEDIALOG_H
