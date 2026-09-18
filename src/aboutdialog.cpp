#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "build-timestamp.h"
#include "updatechecker.h"
#include <QResizeEvent>
#include <QShowEvent>

extern bool g_checkUpdates; // see mainwindow.cpp

AboutDialog::AboutDialog(UpdateChecker *checker, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog),
    m_checker(checker)
{
    ui->setupUi(this);

    // Same "Version:" label/value pair the Settings > Updates tab shows
    // (Settings::setAntScopeVersion()) -- kept visually consistent rather
    // than introducing a second way to display the app version.
    // Version and build timestamp share one label for now.
    ui->versionLabel->setText(QString(ANTSCOPEZ_VER) + "  (" + tr("Build: ") + ANTSCOPEZ_BUILD_TIMESTAMP + ")");

    updateLatestLabel();
    if (m_checker) {
        connect(m_checker, &UpdateChecker::finished, this, &AboutDialog::updateLatestLabel);
    }

    updateFlagLabelWidths();
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::updateLatestLabel()
{
    QString text;
    if (!g_checkUpdates) {
        text = tr("[disabled in settings]");
    } else if (!m_checker || m_checker->state() == UpdateChecker::State::Failed) {
        text = tr("unavailable");
    } else if (m_checker->state() == UpdateChecker::State::Done) {
        text = m_checker->latest();
    } else {
        text = tr("checking...");
    }
    ui->latestLabel->setText(text);
}

void AboutDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updateFlagLabelWidths();
}

void AboutDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // creditsLabel's width isn't final until the layout's actually been
    // activated by showing the dialog -- constructor-time width would be
    // whatever Designer's placeholder geometry happened to be.
    updateFlagLabelWidths();
}

void AboutDialog::updateFlagLabelWidths()
{
    // 80% of creditsLabel's current width, not a hardcoded pixel size --
    // scales with the dialog instead of drifting out of proportion with
    // the credits text next to it.
    int width = qRound(ui->creditsLabel->width() * 0.8);
    ui->label_2->setFixedWidth(width);
    ui->label_3->setFixedWidth(width);
}
