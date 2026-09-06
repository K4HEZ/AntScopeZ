#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    Ui::AboutDialog *ui;

    // Flag-bar labels (label_2/label_3) track 80% of creditsLabel's width,
    // so they scale with it instead of a hardcoded pixel size.
    void updateFlagLabelWidths();
};

#endif // ABOUTDIALOG_H
