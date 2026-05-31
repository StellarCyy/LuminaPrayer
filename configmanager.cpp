#include "configmanager.h"
#include "profilemanager.h"
#include <QSettings>
#include <algorithm>

// MED-16: Lightweight XOR + Base64 obfuscation for API key in INI file.
// NOT real encryption — purely to avoid plaintext exposure on casual inspection.
static const char kObfKey[] = "LmPr2025";

static QString obfuscateKey(const QString &plain) {
    if (plain.isEmpty()) return {};
    QByteArray raw = plain.toUtf8();
    const int klen = static_cast<int>(sizeof(kObfKey) - 1);
    for (int i = 0; i < raw.size(); ++i)
        raw[i] = raw[i] ^ kObfKey[i % klen];
    return QStringLiteral("obf:") + QString::fromLatin1(raw.toBase64());
}

static QString deobfuscateKey(const QString &stored) {
    if (stored.isEmpty()) return {};
    if (!stored.startsWith(QStringLiteral("obf:"))) return stored; // legacy plaintext
    QByteArray raw = QByteArray::fromBase64(stored.mid(4).toLatin1());
    const int klen = static_cast<int>(sizeof(kObfKey) - 1);
    for (int i = 0; i < raw.size(); ++i)
        raw[i] = raw[i] ^ kObfKey[i % klen];
    return QString::fromUtf8(raw);
}

ConfigManager::ConfigManager(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(filePath) {}

void ConfigManager::seedFromProfile() {
    const BehaviorProfile &bp = ProfileManager::instance()->behavior();
    m_config.move_speed_px_per_sec        = bp.move_speed_px_per_sec;
    m_config.stand_to_move_wait_ms        = bp.stand_to_move_wait_ms;
    m_config.move_to_sleep_wait_ms        = bp.move_to_sleep_wait_ms;
    m_config.move_to_sit_wait_ms          = bp.move_to_sit_wait_ms;
    m_config.move_to_playful_wait_ms      = bp.move_to_playful_wait_ms;
    m_config.sit_detection_interval_ms    = bp.sit_detection_interval_ms;
    m_config.sit_trigger_chance_percent   = bp.sit_trigger_chance_percent;
    m_config.sit_mode_duration_ms         = bp.sit_mode_duration_ms;
    m_config.playful_detection_interval_ms  = bp.playful_detection_interval_ms;
    m_config.playful_trigger_chance_percent = bp.playful_trigger_chance_percent;
    m_config.playful_mode_duration_ms     = bp.playful_mode_duration_ms;
    m_config.playmate_min_spacing_px      = bp.playmate_min_spacing_px;
    m_config.playmate_speed_scale         = bp.playmate_speed_scale;
    m_config.playmate_accel_scale         = bp.playmate_accel_scale;
    m_config.angry_click_threshold        = bp.angry_click_threshold;
    m_config.hint_display_duration_ms     = bp.hint_display_duration_ms;
    m_config.auto_sing_enabled            = bp.auto_sing_enabled_default;
    m_config.character_form               = bp.character_form_default;
    m_config.breath_effect_enabled     = ProfileManager::instance()->breathEffect().enabled;
    m_config.status_panel_enabled      = ProfileManager::instance()->statusSystem().enabled;
}

void ConfigManager::load() {
    // Seed all defaults from the active JSON profile first
    seedFromProfile();

    QSettings s(m_filePath, QSettings::IniFormat);

    // INI values override profile defaults (user tweaks persist across sessions)
    // Backward compat: old key was form/use_alternate (bool)
    if (s.contains("form/character_form")) {
        m_config.character_form = static_cast<CharacterForm>(
            s.value("form/character_form", static_cast<int>(m_config.character_form)).toInt());
    } else if (s.contains("form/use_alternate")) {
        m_config.character_form = s.value("form/use_alternate", false).toBool()
            ? CharacterForm::StarDaughter : CharacterForm::Solyn;
    }
    m_config.stand_to_move_wait_ms     = s.value("behavior/stand_to_move_wait_ms",          m_config.stand_to_move_wait_ms).toInt();
    m_config.move_to_sleep_wait_ms     = s.value("behavior/move_to_sleep_wait_ms",          m_config.move_to_sleep_wait_ms).toInt();
    m_config.move_to_sit_wait_ms       = s.value("behavior/move_to_sit_wait_ms",            m_config.move_to_sit_wait_ms).toInt();
    m_config.move_to_playful_wait_ms   = s.value("behavior/move_to_playful_wait_ms",        m_config.move_to_playful_wait_ms).toInt();
    m_config.sit_detection_interval_ms = s.value("behavior/sit_detection_interval_ms",      m_config.sit_detection_interval_ms).toInt();
    m_config.sit_trigger_chance_percent      = s.value("behavior/sit_trigger_chance_percent",      m_config.sit_trigger_chance_percent).toInt();
    m_config.playful_detection_interval_ms   = s.value("behavior/playful_detection_interval_ms",  m_config.playful_detection_interval_ms).toInt();
    m_config.playful_trigger_chance_percent  = s.value("behavior/playful_trigger_chance_percent", m_config.playful_trigger_chance_percent).toInt();
    m_config.sit_mode_duration_ms      = s.value("behavior/sit_mode_duration_ms",           m_config.sit_mode_duration_ms).toInt();
    m_config.playful_mode_duration_ms  = s.value("behavior/playful_mode_duration_ms",       m_config.playful_mode_duration_ms).toInt();
    m_config.angry_click_threshold     = s.value("behavior/angry_click_threshold",          m_config.angry_click_threshold).toInt();
    m_config.playmate_min_spacing_px   = s.value("behavior/playmate_min_spacing_px",        m_config.playmate_min_spacing_px).toInt();
    m_config.move_speed_px_per_sec     = s.value("behavior/move_speed_px_per_sec",          m_config.move_speed_px_per_sec).toDouble();
    m_config.playmate_speed_scale      = s.value("behavior/playmate_speed_scale",           m_config.playmate_speed_scale).toDouble();
    m_config.playmate_accel_scale      = s.value("behavior/playmate_accel_scale",           m_config.playmate_accel_scale).toDouble();
    m_config.hint_display_duration_ms  = s.value("behavior/hint_display_duration_ms",       m_config.hint_display_duration_ms).toInt();
    m_config.auto_sing_enabled         = s.value("behavior/auto_sing_enabled",              m_config.auto_sing_enabled).toBool();
    m_config.breath_effect_enabled     = s.value("behavior/breath_effect_enabled",          m_config.breath_effect_enabled).toBool();
    m_config.status_panel_enabled      = s.value("behavior/status_panel_enabled",           m_config.status_panel_enabled).toBool();
    m_config.stats_variable              = s.value("behavior/stats_variable",                  m_config.stats_variable).toBool();
    m_config.stats_tick_interval_ms      = s.value("behavior/stats_tick_interval_ms",          m_config.stats_tick_interval_ms).toInt();
    m_config.head_pat_from_right         = s.value("behavior/head_pat_from_right",             m_config.head_pat_from_right).toBool();
    m_config.head_pat_radius              = s.value("behavior/head_pat_radius",                m_config.head_pat_radius).toDouble();
    m_config.head_pat_start_deg           = s.value("behavior/head_pat_start_deg",             m_config.head_pat_start_deg).toDouble();
    m_config.head_pat_end_deg             = s.value("behavior/head_pat_end_deg",               m_config.head_pat_end_deg).toDouble();
    m_config.head_pat_squish_ms           = s.value("behavior/head_pat_squish_ms",             m_config.head_pat_squish_ms).toInt();
    m_config.star_move_speed                = s.value("behavior/star_move_speed",                  m_config.star_move_speed).toInt();
    m_config.max_star_count                 = s.value("behavior/max_star_count",                   m_config.max_star_count).toInt();
    m_config.auto_throw_star                = s.value("behavior/auto_throw_star",                  m_config.auto_throw_star).toBool();
    m_config.auto_throw_star_interval_ms    = s.value("behavior/auto_throw_star_interval_ms",      m_config.auto_throw_star_interval_ms).toInt();
    m_config.ai_reply_enabled              = s.value("behavior/ai_reply_enabled",                m_config.ai_reply_enabled).toBool();
    m_config.deepseek_api_key              = deobfuscateKey(s.value("behavior/deepseek_api_key", QString()).toString());
    m_config.ai_max_history                = s.value("behavior/ai_max_history",                  m_config.ai_max_history).toInt();
    m_config.ai_timeout_ms                 = s.value("behavior/ai_timeout_ms",                   m_config.ai_timeout_ms).toInt();
    m_config.ai_bubble_duration_ms          = s.value("behavior/ai_bubble_duration_ms",          m_config.ai_bubble_duration_ms).toInt();
    m_config.ai_bubble_padding_px           = s.value("behavior/ai_bubble_padding_px",           m_config.ai_bubble_padding_px).toInt();
    m_config.ai_system_prompt               = s.value("behavior/ai_system_prompt",               m_config.ai_system_prompt).toString();

    m_windowPos = s.value("window/pos", QPoint()).toPoint();

    validate();
}

void ConfigManager::save() {
    QSettings s(m_filePath, QSettings::IniFormat);

    s.setValue("form/character_form",                     static_cast<int>(m_config.character_form));
    s.setValue("behavior/stand_to_move_wait_ms",          m_config.stand_to_move_wait_ms);
    s.setValue("behavior/move_to_sleep_wait_ms",          m_config.move_to_sleep_wait_ms);
    s.setValue("behavior/move_to_sit_wait_ms",            m_config.move_to_sit_wait_ms);
    s.setValue("behavior/move_to_playful_wait_ms",        m_config.move_to_playful_wait_ms);
    s.setValue("behavior/sit_detection_interval_ms",      m_config.sit_detection_interval_ms);
    s.setValue("behavior/sit_trigger_chance_percent",     m_config.sit_trigger_chance_percent);
    s.setValue("behavior/playful_detection_interval_ms",  m_config.playful_detection_interval_ms);
    s.setValue("behavior/playful_trigger_chance_percent", m_config.playful_trigger_chance_percent);
    s.setValue("behavior/sit_mode_duration_ms",           m_config.sit_mode_duration_ms);
    s.setValue("behavior/playful_mode_duration_ms",       m_config.playful_mode_duration_ms);
    s.setValue("behavior/angry_click_threshold",          m_config.angry_click_threshold);
    s.setValue("behavior/playmate_min_spacing_px",        m_config.playmate_min_spacing_px);
    s.setValue("behavior/move_speed_px_per_sec",          m_config.move_speed_px_per_sec);
    s.setValue("behavior/playmate_speed_scale",           m_config.playmate_speed_scale);
    s.setValue("behavior/playmate_accel_scale",           m_config.playmate_accel_scale);
    s.setValue("behavior/hint_display_duration_ms",       m_config.hint_display_duration_ms);
    s.setValue("behavior/auto_sing_enabled",              m_config.auto_sing_enabled);
    s.setValue("behavior/breath_effect_enabled",          m_config.breath_effect_enabled);
    s.setValue("behavior/status_panel_enabled",            m_config.status_panel_enabled);
    s.setValue("behavior/stats_variable",                   m_config.stats_variable);
    s.setValue("behavior/stats_tick_interval_ms",          m_config.stats_tick_interval_ms);
    s.setValue("behavior/head_pat_from_right",             m_config.head_pat_from_right);
    s.setValue("behavior/head_pat_radius",                m_config.head_pat_radius);
    s.setValue("behavior/head_pat_start_deg",             m_config.head_pat_start_deg);
    s.setValue("behavior/head_pat_end_deg",               m_config.head_pat_end_deg);
    s.setValue("behavior/head_pat_squish_ms",             m_config.head_pat_squish_ms);
    s.setValue("behavior/star_move_speed",                  m_config.star_move_speed);
    s.setValue("behavior/max_star_count",                   m_config.max_star_count);
    s.setValue("behavior/auto_throw_star",                  m_config.auto_throw_star);
    s.setValue("behavior/auto_throw_star_interval_ms",      m_config.auto_throw_star_interval_ms);
    s.setValue("behavior/ai_reply_enabled",                 m_config.ai_reply_enabled);
    s.setValue("behavior/deepseek_api_key",                obfuscateKey(m_config.deepseek_api_key));
    s.setValue("behavior/ai_max_history",                  m_config.ai_max_history);
    s.setValue("behavior/ai_timeout_ms",                   m_config.ai_timeout_ms);
    s.setValue("behavior/ai_bubble_duration_ms",          m_config.ai_bubble_duration_ms);
    s.setValue("behavior/ai_bubble_padding_px",           m_config.ai_bubble_padding_px);
    s.setValue("behavior/ai_system_prompt",               m_config.ai_system_prompt);
    s.setValue("window/pos",                              m_windowPos);
}

BehaviorConfig& ConfigManager::config() { return m_config; }
const BehaviorConfig& ConfigManager::config() const { return m_config; }

QPoint ConfigManager::windowPos() const { return m_windowPos; }
void ConfigManager::setWindowPos(const QPoint &pos) { m_windowPos = pos; }

void ConfigManager::validate() {
    auto &c = m_config;
    c.stand_to_move_wait_ms          = std::max(1000, c.stand_to_move_wait_ms);
    c.move_to_sleep_wait_ms          = std::max(1000, c.move_to_sleep_wait_ms);
    c.move_to_sit_wait_ms            = std::max(1000, c.move_to_sit_wait_ms);
    c.move_to_playful_wait_ms        = std::max(1000, c.move_to_playful_wait_ms);
    c.sit_detection_interval_ms      = std::max(1000, c.sit_detection_interval_ms);
    c.sit_trigger_chance_percent     = std::clamp(c.sit_trigger_chance_percent, 0, 100);
    c.playful_detection_interval_ms  = std::max(1000, c.playful_detection_interval_ms);
    c.playful_trigger_chance_percent = std::clamp(c.playful_trigger_chance_percent, 0, 100);
    c.sit_mode_duration_ms           = std::max(1000, c.sit_mode_duration_ms);
    c.playful_mode_duration_ms       = std::max(1000, c.playful_mode_duration_ms);
    c.angry_click_threshold          = std::max(1, c.angry_click_threshold);
    c.playmate_min_spacing_px        = std::max(20, c.playmate_min_spacing_px);
    c.move_speed_px_per_sec          = std::max(50.0, c.move_speed_px_per_sec);
    c.playmate_speed_scale           = std::max(0.2, c.playmate_speed_scale);
    c.playmate_accel_scale           = std::max(0.1, c.playmate_accel_scale);
    c.hint_display_duration_ms       = std::max(200, c.hint_display_duration_ms);
    c.head_pat_radius                = std::max(50.0, c.head_pat_radius);
    c.head_pat_squish_ms             = std::clamp(c.head_pat_squish_ms, 50, 1000);
    c.star_move_speed                = std::clamp(c.star_move_speed, 10, 2000);
    c.max_star_count                = std::clamp(c.max_star_count, 1, 50);  // H-03: cap for O(n²) collision
    c.auto_throw_star_interval_ms   = std::max(1000, c.auto_throw_star_interval_ms);
    c.ai_timeout_ms                  = std::clamp(c.ai_timeout_ms, 5000, 300000);
}
