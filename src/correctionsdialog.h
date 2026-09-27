#ifndef CORRECTIONSDIALOG_H
#define CORRECTIONSDIALOG_H

#include <QDialog>
#include "measurementdata.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;

// Measurements list > "Corrections...": changes the OSL/cable corrections
// one measurement shows, recomputed from its original points.
class CorrectionsDialog : public QDialog
{
    Q_OBJECT
public:
    // `shown`: what the measurement shows now. `cableModel`: starting cable
    // model (the measurement's own, else Settings > Cable's).
    CorrectionsDialog(const QString& name, const Corrections& shown, const Corrections& builtIn,
                      const RfMath::CableParams& cableModel, bool oslAllowed, const QString& oslWhy,
                      bool canRemoveBuiltIn, bool metric, QWidget* parent = nullptr);

    Corrections corrections() const;
    bool asCopy() const;

private:
    void updateCableFields();

    QCheckBox* m_osl;
    QComboBox* m_cableMode;
    QDoubleSpinBox* m_length;
    QDoubleSpinBox* m_velFactor;
    QLabel* m_cableNote;
    QCheckBox* m_copy;
    RfMath::CableParams m_model;
    bool m_metric;
    bool m_cableLocked;
};

#endif // CORRECTIONSDIALOG_H
