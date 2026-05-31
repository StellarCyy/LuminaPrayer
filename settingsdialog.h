#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "configmanager.h"
#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const BehaviorConfig &initial, QWidget *parent = nullptr);
    BehaviorConfig result() const;

private:
    void buildUI(const BehaviorConfig &cfg);

    QDoubleSpinBox *m_moveSpeedSpin;
    QSpinBox *m_standToMoveSpin;
    QSpinBox *m_moveToSleepSpin;
    QSpinBox *m_moveToSitSpin;
    QSpinBox *m_moveToPlayfulSpin;
    QSpinBox *m_sitDetectIntervalSpin;
    QSpinBox *m_sitTriggerChanceSpin;
    QSpinBox *m_playfulDetectIntervalSpin;
    QSpinBox *m_playfulTriggerChanceSpin;
    QSpinBox *m_sitDurationSpin;
    QSpinBox *m_playfulDurationSpin;
    QSpinBox *m_angryClickSpin;
    QSpinBox *m_playmateSpacingSpin;
    QDoubleSpinBox *m_playmateSpeedScaleSpin;
    QDoubleSpinBox *m_playmateAccelScaleSpin;
    QSpinBox *m_hintDurationSpin;
    QCheckBox *m_breathEffectCheck;
    QCheckBox *m_statusPanelCheck;
    QCheckBox *m_statsVariableCheck;
    QSpinBox  *m_statsTickIntervalSpin;
    QCheckBox *m_headPatFromRightCheck;
    QSpinBox  *m_starSpeedSpin;
    QSpinBox  *m_maxStarCountSpin;
    QCheckBox *m_autoThrowStarCheck;
    QSpinBox  *m_autoThrowStarIntervalSpin;
    QCheckBox *m_aiReplyCheck;
    QLineEdit *m_apiKeyEdit;
    QSpinBox  *m_aiMaxHistorySpin;
    QSpinBox  *m_aiTimeoutSpin;
    QSpinBox  *m_aiBubbleDurationSpin;
    QSpinBox  *m_aiBubblePaddingSpin;
    QPlainTextEdit *m_aiSystemPromptEdit;
};

#endif // SETTINGSDIALOG_H
