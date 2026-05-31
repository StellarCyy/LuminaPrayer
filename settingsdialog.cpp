#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <algorithm>

SettingsDialog::SettingsDialog(const BehaviorConfig &initial, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("设置参数");
    buildUI(initial);
}

BehaviorConfig SettingsDialog::result() const {
    BehaviorConfig cfg;
    cfg.move_speed_px_per_sec          = m_moveSpeedSpin->value();
    cfg.stand_to_move_wait_ms          = m_standToMoveSpin->value() * 1000;
    cfg.move_to_sleep_wait_ms          = m_moveToSleepSpin->value() * 1000;
    cfg.move_to_sit_wait_ms            = m_moveToSitSpin->value() * 1000;
    cfg.move_to_playful_wait_ms        = m_moveToPlayfulSpin->value() * 1000;
    cfg.sit_detection_interval_ms      = m_sitDetectIntervalSpin->value() * 1000;
    cfg.sit_trigger_chance_percent     = m_sitTriggerChanceSpin->value();
    cfg.playful_detection_interval_ms  = m_playfulDetectIntervalSpin->value() * 1000;
    cfg.playful_trigger_chance_percent = m_playfulTriggerChanceSpin->value();
    cfg.sit_mode_duration_ms           = m_sitDurationSpin->value() * 1000;
    cfg.playful_mode_duration_ms       = m_playfulDurationSpin->value() * 1000;
    cfg.angry_click_threshold          = m_angryClickSpin->value();
    cfg.playmate_min_spacing_px        = m_playmateSpacingSpin->value();
    cfg.playmate_speed_scale           = m_playmateSpeedScaleSpin->value();
    cfg.playmate_accel_scale           = m_playmateAccelScaleSpin->value();
    cfg.hint_display_duration_ms       = m_hintDurationSpin->value() * 1000;
    cfg.breath_effect_enabled          = m_breathEffectCheck->isChecked();
    cfg.head_pat_from_right            = m_headPatFromRightCheck->isChecked();
    cfg.star_move_speed                = m_starSpeedSpin->value();
    cfg.max_star_count                 = m_maxStarCountSpin->value();
    cfg.auto_throw_star                = m_autoThrowStarCheck->isChecked();
    cfg.auto_throw_star_interval_ms    = m_autoThrowStarIntervalSpin->value() * 1000;
    cfg.status_panel_enabled           = m_statusPanelCheck->isChecked();
    cfg.stats_variable                 = m_statsVariableCheck->isChecked();
    cfg.stats_tick_interval_ms         = m_statsTickIntervalSpin->value() * 1000;
    cfg.ai_reply_enabled               = m_aiReplyCheck->isChecked();
    cfg.deepseek_api_key               = m_apiKeyEdit->text().trimmed();
    cfg.ai_max_history                 = m_aiMaxHistorySpin->value();
    cfg.ai_timeout_ms                  = m_aiTimeoutSpin->value() * 1000;
    cfg.ai_bubble_duration_ms          = m_aiBubbleDurationSpin->value() * 1000;
    cfg.ai_bubble_padding_px           = m_aiBubblePaddingSpin->value();
    cfg.ai_system_prompt               = m_aiSystemPromptEdit->toPlainText().trimmed();
    return cfg;
}

void SettingsDialog::buildUI(const BehaviorConfig &cfg) {
    QVBoxLayout *rootLayout = new QVBoxLayout(this);

    m_moveSpeedSpin = new QDoubleSpinBox(this);
    m_moveSpeedSpin->setRange(50.0, 1200.0);
    m_moveSpeedSpin->setDecimals(0);
    m_moveSpeedSpin->setSuffix(" px/s");
    m_moveSpeedSpin->setValue(cfg.move_speed_px_per_sec);

    m_standToMoveSpin = new QSpinBox(this);
    m_standToMoveSpin->setRange(1, 600);
    m_standToMoveSpin->setSuffix(" 秒");
    m_standToMoveSpin->setValue(cfg.stand_to_move_wait_ms / 1000);

    m_moveToSleepSpin = new QSpinBox(this);
    m_moveToSleepSpin->setRange(1, 1800);
    m_moveToSleepSpin->setSuffix(" 秒");
    m_moveToSleepSpin->setValue(cfg.move_to_sleep_wait_ms / 1000);

    m_moveToSitSpin = new QSpinBox(this);
    m_moveToSitSpin->setRange(1, 1800);
    m_moveToSitSpin->setSuffix(" 秒");
    m_moveToSitSpin->setValue(cfg.move_to_sit_wait_ms / 1000);

    m_moveToPlayfulSpin = new QSpinBox(this);
    m_moveToPlayfulSpin->setRange(1, 1800);
    m_moveToPlayfulSpin->setSuffix(" 秒");
    m_moveToPlayfulSpin->setValue(cfg.move_to_playful_wait_ms / 1000);

    m_sitDetectIntervalSpin = new QSpinBox(this);
    m_sitDetectIntervalSpin->setRange(1, 600);
    m_sitDetectIntervalSpin->setSuffix(" 秒");
    m_sitDetectIntervalSpin->setValue(std::max(1, cfg.sit_detection_interval_ms / 1000));

    m_sitTriggerChanceSpin = new QSpinBox(this);
    m_sitTriggerChanceSpin->setRange(0, 100);
    m_sitTriggerChanceSpin->setSuffix(" %");
    m_sitTriggerChanceSpin->setValue(cfg.sit_trigger_chance_percent);

    m_playfulDetectIntervalSpin = new QSpinBox(this);
    m_playfulDetectIntervalSpin->setRange(1, 600);
    m_playfulDetectIntervalSpin->setSuffix(" 秒");
    m_playfulDetectIntervalSpin->setValue(std::max(1, cfg.playful_detection_interval_ms / 1000));

    m_playfulTriggerChanceSpin = new QSpinBox(this);
    m_playfulTriggerChanceSpin->setRange(0, 100);
    m_playfulTriggerChanceSpin->setSuffix(" %");
    m_playfulTriggerChanceSpin->setValue(cfg.playful_trigger_chance_percent);

    m_sitDurationSpin = new QSpinBox(this);
    m_sitDurationSpin->setRange(1, 1800);
    m_sitDurationSpin->setSuffix(" 秒");
    m_sitDurationSpin->setValue(std::max(1, cfg.sit_mode_duration_ms / 1000));

    m_playfulDurationSpin = new QSpinBox(this);
    m_playfulDurationSpin->setRange(1, 1800);
    m_playfulDurationSpin->setSuffix(" 秒");
    m_playfulDurationSpin->setValue(std::max(1, cfg.playful_mode_duration_ms / 1000));

    m_angryClickSpin = new QSpinBox(this);
    m_angryClickSpin->setRange(1, 100);
    m_angryClickSpin->setValue(cfg.angry_click_threshold);

    m_playmateSpacingSpin = new QSpinBox(this);
    m_playmateSpacingSpin->setRange(20, 600);
    m_playmateSpacingSpin->setSuffix(" px");
    m_playmateSpacingSpin->setValue(cfg.playmate_min_spacing_px);

    m_playmateSpeedScaleSpin = new QDoubleSpinBox(this);
    m_playmateSpeedScaleSpin->setRange(0.2, 10.0);
    m_playmateSpeedScaleSpin->setDecimals(2);
    m_playmateSpeedScaleSpin->setSingleStep(0.1);
    m_playmateSpeedScaleSpin->setSuffix(" x");
    m_playmateSpeedScaleSpin->setValue(cfg.playmate_speed_scale);

    m_playmateAccelScaleSpin = new QDoubleSpinBox(this);
    m_playmateAccelScaleSpin->setRange(0.1, 20.0);
    m_playmateAccelScaleSpin->setDecimals(2);
    m_playmateAccelScaleSpin->setSingleStep(0.2);
    m_playmateAccelScaleSpin->setValue(cfg.playmate_accel_scale);

    m_hintDurationSpin = new QSpinBox(this);
    m_hintDurationSpin->setRange(1, 30);
    m_hintDurationSpin->setSuffix(" 秒");
    m_hintDurationSpin->setValue(std::max(1, cfg.hint_display_duration_ms / 1000));

    m_headPatFromRightCheck = new QCheckBox("启用", this);
    m_headPatFromRightCheck->setChecked(cfg.head_pat_from_right);

    m_starSpeedSpin = new QSpinBox(this);
    m_starSpeedSpin->setRange(0, 2000);
    m_starSpeedSpin->setSuffix(" px/s");
    m_starSpeedSpin->setValue(cfg.star_move_speed);

    m_maxStarCountSpin = new QSpinBox(this);
    m_maxStarCountSpin->setRange(1, 500);
    m_maxStarCountSpin->setValue(cfg.max_star_count);

    m_autoThrowStarCheck = new QCheckBox("启用", this);
    m_autoThrowStarCheck->setChecked(cfg.auto_throw_star);

    m_autoThrowStarIntervalSpin = new QSpinBox(this);
    m_autoThrowStarIntervalSpin->setRange(0, 600);
    m_autoThrowStarIntervalSpin->setSuffix(" 秒");
    m_autoThrowStarIntervalSpin->setValue(std::max(1, cfg.auto_throw_star_interval_ms / 1000));

    m_breathEffectCheck = new QCheckBox("启用", this);
    m_breathEffectCheck->setChecked(cfg.breath_effect_enabled);

    m_statusPanelCheck = new QCheckBox("启用", this);
    m_statusPanelCheck->setChecked(cfg.status_panel_enabled);

    m_statsVariableCheck = new QCheckBox("启用", this);
    m_statsVariableCheck->setChecked(cfg.stats_variable);

    m_statsTickIntervalSpin = new QSpinBox(this);
    m_statsTickIntervalSpin->setRange(1, 300);
    m_statsTickIntervalSpin->setSuffix(" 秒");
    m_statsTickIntervalSpin->setValue(std::max(1, cfg.stats_tick_interval_ms / 1000));

    m_aiReplyCheck = new QCheckBox("启用", this);
    m_aiReplyCheck->setChecked(cfg.ai_reply_enabled);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setText(cfg.deepseek_api_key);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(QStringLiteral("sk-..."));

    m_aiMaxHistorySpin = new QSpinBox(this);
    m_aiMaxHistorySpin->setRange(3, 201);
    m_aiMaxHistorySpin->setValue(cfg.ai_max_history);
    m_aiMaxHistorySpin->setToolTip(QStringLiteral("数值越大，token消耗越快"));

    m_aiTimeoutSpin = new QSpinBox(this);
    m_aiTimeoutSpin->setRange(5, 300);
    m_aiTimeoutSpin->setSuffix(" 秒");
    m_aiTimeoutSpin->setValue(std::max(5, cfg.ai_timeout_ms / 1000));
    m_aiTimeoutSpin->setToolTip(QStringLiteral("超过此时间未收到回复则判定超时"));

    m_aiBubbleDurationSpin = new QSpinBox(this);
    m_aiBubbleDurationSpin->setRange(1, 120);
    m_aiBubbleDurationSpin->setSuffix(" 秒");
    m_aiBubbleDurationSpin->setValue(std::max(1, cfg.ai_bubble_duration_ms / 1000));

    m_aiBubblePaddingSpin = new QSpinBox(this);
    m_aiBubblePaddingSpin->setRange(10, 120);
    m_aiBubblePaddingSpin->setSuffix(" px");
    m_aiBubblePaddingSpin->setValue(qBound(10, cfg.ai_bubble_padding_px, 120));

    m_aiSystemPromptEdit = new QPlainTextEdit(this);
    m_aiSystemPromptEdit->setPlainText(cfg.ai_system_prompt);
    m_aiSystemPromptEdit->setMaximumHeight(80);

    // ── Helper: create a collapsible section ──
    auto makeSection = [this](const QString &title,
                              const std::function<void(QFormLayout*)> &populate) -> QWidget*
    {
        QWidget *section = new QWidget(this);
        QVBoxLayout *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(0);

        QPushButton *header = new QPushButton(QStringLiteral("\u25b6  ") + title, section);
        header->setFlat(true);
        header->setStyleSheet(
            QStringLiteral("QPushButton { text-align: left; font-weight: bold; "
                           "padding: 6px 8px; background: palette(midlight); }"
                           "QPushButton:hover { background: palette(mid); }"));
        header->setCursor(Qt::PointingHandCursor);

        QWidget *content = new QWidget(section);
        QFormLayout *form = new QFormLayout(content);
        form->setContentsMargins(12, 4, 4, 4);
        populate(form);
        content->hide();

        sectionLayout->addWidget(header);
        sectionLayout->addWidget(content);

        connect(header, &QPushButton::clicked, this, [header, content, title]() {
            const bool show = !content->isVisible();
            content->setVisible(show);
            header->setText((show ? QStringLiteral("\u25bc  ") : QStringLiteral("\u25b6  ")) + title);
        });

        return section;
    };

    // ── Build collapsible sections ──

    QWidget *s1 = makeSection(QStringLiteral("角色运动"), [&](QFormLayout *f) {
        f->addRow("移动速度", m_moveSpeedSpin);
    });

    QWidget *s2 = makeSection(QStringLiteral("持续时间"), [&](QFormLayout *f) {
        f->addRow("站立→移动 等待", m_standToMoveSpin);
        f->addRow("移动→睡眠 等待", m_moveToSleepSpin);
        f->addRow("移动→坐下 等待", m_moveToSitSpin);
        f->addRow("移动→嬉戏 等待", m_moveToPlayfulSpin);
        f->addRow("坐下 持续时间", m_sitDurationSpin);
    });

    QWidget *s3 = makeSection(QStringLiteral("嬉戏模式"), [&](QFormLayout *f) {
        f->addRow("嬉戏 持续时间", m_playfulDurationSpin);
        f->addRow("伙伴 最小间距", m_playmateSpacingSpin);
        f->addRow("伙伴 速度倍率", m_playmateSpeedScaleSpin);
        f->addRow("伙伴 加速度倍率", m_playmateAccelScaleSpin);
    });

    QWidget *s4 = makeSection(QStringLiteral("触发概率"), [&](QFormLayout *f) {
        f->addRow("坐下 检测间隔", m_sitDetectIntervalSpin);
        f->addRow("坐下 触发概率", m_sitTriggerChanceSpin);
        f->addRow("嬉戏 检测间隔", m_playfulDetectIntervalSpin);
        f->addRow("嬉戏 触发概率", m_playfulTriggerChanceSpin);
    });

    QWidget *s5 = makeSection(QStringLiteral("状态属性"), [&](QFormLayout *f) {
        f->addRow("显示状态面板", m_statusPanelCheck);
        f->addRow("属性随时间变化", m_statsVariableCheck);
        f->addRow("属性刷新间隔", m_statsTickIntervalSpin);
    });

    QWidget *sInteract = makeSection(QStringLiteral("角色互动"), [&](QFormLayout *f) {
        f->addRow("从右边摸头", m_headPatFromRightCheck);
        f->addRow("星星移动速度", m_starSpeedSpin);
        f->addRow("最大星星数量", m_maxStarCountSpin);
        f->addRow("自动扔出星星", m_autoThrowStarCheck);
        f->addRow("扔星星间隔", m_autoThrowStarIntervalSpin);
        const int intervalRow = f->rowCount() - 1;
        f->setRowVisible(intervalRow, cfg.auto_throw_star);
        QFormLayout *formRef = f;
        connect(m_autoThrowStarCheck, &QCheckBox::toggled, this,
                [formRef, intervalRow](bool checked) {
            formRef->setRowVisible(intervalRow, checked);
        });
    });

    QWidget *s6 = makeSection(QStringLiteral("其他"), [&](QFormLayout *f) {
        f->addRow("红温触发连续点击次数", m_angryClickSpin);
        f->addRow("提示文字持续时间", m_hintDurationSpin);
        f->addRow("CPU 光环效果", m_breathEffectCheck);
    });

    QWidget *s7 = makeSection(QStringLiteral("AI 回复"), [&](QFormLayout *f) {
        f->addRow("AI 回复", m_aiReplyCheck);
        f->addRow(new QLabel(QStringLiteral("请输入 DeepSeek 的 API 密钥"), this));
        f->addRow("API Key", m_apiKeyEdit);
        f->addRow("最大对话记忆数（数值越大，token 消耗越快）", m_aiMaxHistorySpin);
        f->addRow("最大回复等待时间", m_aiTimeoutSpin);
        f->addRow("气泡停留时间", m_aiBubbleDurationSpin);
        f->addRow("内容边距", m_aiBubblePaddingSpin);
        f->addRow("角色设定提示词", m_aiSystemPromptEdit);
    });

    // ── Scrollable container ──
    QWidget *scrollContent = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(4, 4, 4, 4);
    contentLayout->setSpacing(2);
    contentLayout->addWidget(s1);
    contentLayout->addWidget(s2);
    contentLayout->addWidget(s3);
    contentLayout->addWidget(s4);
    contentLayout->addWidget(s5);
    contentLayout->addWidget(sInteract);
    contentLayout->addWidget(s6);
    contentLayout->addWidget(s7);
    contentLayout->addStretch(1);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(scrollContent);

    rootLayout->addWidget(scrollArea, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    rootLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(480, 420);
}
