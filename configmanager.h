#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "roleact.h"
#include <QObject>
#include <QPoint>
#include <QString>

struct BehaviorConfig {
    // All numeric defaults are populated by ConfigManager::seedFromProfile()
    // at load-time from the active ProfileManager profile.
    // The values below are last-resort fallbacks only.

    // Movement
    double move_speed_px_per_sec = 160.0;

    // State transition waits
    int stand_to_move_wait_ms = 25000;
    int move_to_sleep_wait_ms = 180000;
    int move_to_sit_wait_ms = 25000;
    int move_to_playful_wait_ms = 30000;

    // Sitting detection
    int sit_detection_interval_ms = 10000;
    int sit_trigger_chance_percent = 30;
    int sit_mode_duration_ms = 30000;

    // Playful detection
    int playful_detection_interval_ms = 10000;
    int playful_trigger_chance_percent = 30;
    int playful_mode_duration_ms = 30000;

    // Playmate
    int playmate_min_spacing_px = 120;
    double playmate_speed_scale = 2.0;
    double playmate_accel_scale = 5.0;

    // Angry
    int angry_click_threshold = 10;

    // UI hints
    int hint_display_duration_ms = 2000;

    // Audio
    bool auto_sing_enabled = true;

    // Form
    CharacterForm character_form = CharacterForm::Solyn;

    // Breath effect (CPU halo)
    bool breath_effect_enabled = true;

    // Status panel
    bool status_panel_enabled = false;

    // Attribute system
    bool stats_variable = true;
    int  stats_tick_interval_ms = 10000;

    // Head pat
    bool head_pat_from_right = true;
    double head_pat_radius = 300.0;
    double head_pat_start_deg = 45.0;
    double head_pat_end_deg = -3.0;
    int head_pat_squish_ms = 250;

    // Star interaction
    int star_move_speed = 100;
    int max_star_count = 100;
    bool auto_throw_star = false;
    int auto_throw_star_interval_ms = 10000;

    // AI chat
    bool ai_reply_enabled = false;
    QString deepseek_api_key;
    int ai_max_history = 11;
    int ai_timeout_ms = 60000;
    int ai_bubble_duration_ms = 15000;
    int ai_bubble_padding_px  = 50;
    QString ai_system_prompt = QStringLiteral(
        "你是 Lumina，一个活泼的桌面精灵，说话带点傲娇。回答尽量简短可爱，不超过100字。");
};

class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(const QString &filePath, QObject *parent = nullptr);

    void load();
    void save();
    void seedFromProfile();

    BehaviorConfig& config();
    const BehaviorConfig& config() const;

    QPoint windowPos() const;
    void setWindowPos(const QPoint &pos);

private:
    void validate();

    QString m_filePath;
    BehaviorConfig m_config;
    QPoint m_windowPos;
};

#endif // CONFIGMANAGER_H
