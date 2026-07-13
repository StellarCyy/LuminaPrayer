#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include "roleact.h"
#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QColor>
#include <QJsonObject>

// =============================================
// Sprite entry: pattern + frame count (+ scale-to-role-size intent)
// =============================================
struct SpriteEntry {
    QString pattern;
    int count = 1;
    bool scale = true;   // scale frames to role_display_size when cached
};

// =============================================
// Form definition: generic string-keyed action sprites for one form.
// Keys are arbitrary action-ids; "<id>_left" / "<id>_right" pairs are
// auto-packed into one directional action "<id>" by SpriteResource.
// =============================================
struct FormDef {
    QMap<QString, SpriteEntry> entries;

    void set(const QString &key, const SpriteEntry &e) { entries.insert(key, e); }
    SpriteEntry entry(const QString &key) const { return entries.value(key); }
    bool has(const QString &key) const { return entries.contains(key); }
};

// =============================================
// Action topology: data-driven runtime state machine (v4)
// =============================================
struct ActionTransitionRule {
    QString trigger;                 // diagnostic name
    int     after_ms = -1;           // literal initial delay (-1 = unset)
    QString after_ms_key;            // BehaviorConfig/BehaviorProfile key override
    int     periodic_ms = -1;        // literal periodic re-check interval (-1 = none)
    QString periodic_ms_key;
    int     chance_percent = 100;    // roll on every fire
    QString chance_percent_key;
    QString to;                      // target state id (empty = stay / behaviors only)
    QStringList behaviors;           // run BEFORE transition; any false vetoes `to`
    QStringList post_behaviors;      // run AFTER the state was applied
    bool    rearm_on_interaction = true;  // restarted by user interaction (rearm)
};

struct ActionStateSpec {
    QString id;
    int     frame_interval_ms = -1;  // per-state frame rate (-1 = global default)
    QString enter_hint;              // hint key (see SpritesProfile::hintPath) or raw path
    QString menu_label;              // non-empty = auto-added to context menu
    QStringList on_enter;            // behaviors run when the state is entered
    QList<ActionTransitionRule> transitions;
};

struct ActionTopologyProfile {
    QMap<QString, ActionStateSpec> states;   // keyed by action-id
};

// =============================================
// Perception bus profile (v4 environment awareness)
// =============================================
struct PerceptionProfile {
    bool enabled            = true;
    int  foreground_poll_ms = 1500;
};

// =============================================
// Voice menu item
// =============================================
struct VoiceMenuItem {
    QString label;
    QString path;
};

// =============================================
// All profile sub-structs
// =============================================
struct WindowProfile {
    int widget_size              = 500;
    int fist_widget_size         = 360;
    int fist_sprite_size         = 100;
};

struct SpritesProfile {
    int role_display_size        = 200;
    QString light_symbol         = QStringLiteral(":/lightsymbol1/lightsymbol0.png");
    QString fist                 = QStringLiteral(":/fist1/fist0.png");
    QString icon                 = QStringLiteral(":/icon/icon.png");

    // Per-form sprite definitions (keyed by CharacterForm enum)
    QMap<CharacterForm, FormDef> forms;

    // Pull (hand-grabbing) sprites per form
    QMap<CharacterForm, QString> pull;

    // Convenience: get form def with fallback to first available
    const FormDef& formDef(CharacterForm f) const {
        auto it = forms.constFind(f);
        if (it != forms.constEnd()) return *it;
        static const FormDef s_empty;
        return forms.isEmpty() ? s_empty : forms.first();
    }
    // Graceful degradation: unknown form falls back to first registered form.
    CharacterForm sanitizeForm(CharacterForm f) const {
        if (forms.contains(f) || forms.isEmpty()) return f;
        return forms.firstKey();
    }
    // Convenience: get pull path with fallback
    QString pullPath(CharacterForm f) const {
        return pull.value(f);
    }

    // M-04: Formerly hard-coded resource paths — now data-driven via character.json
    QString chatbox_bg                = QStringLiteral(":/chatbox1/starchatbox0.png");
    QString hand_sprite               = QStringLiteral(":/hand1/hand0.png");
    QString star_pattern              = QStringLiteral(":/throwstar1/star%1-0.png");
    int     star_variant_count        = 5;

    QString hint_text_ok              = QStringLiteral(":/text/text_ok.png");
    QString hint_text_can_sing        = QStringLiteral(":/text/text_can_sing.png");
    QString hint_text_go_to_sleep     = QStringLiteral(":/text/text_go_to_sleep.png");
    QString hint_text_start_listening = QStringLiteral(":/text/text_start_listening.png");
    QString hint_text_angry           = QStringLiteral(":/text/text_angry1.png");

    // Resolve a topology hint key to a pixmap path. Unknown keys that look
    // like resource/file paths pass through verbatim (custom-state hints).
    QString hintPath(const QString &key) const {
        if (key.isEmpty()) return {};
        if (key == QLatin1String("text_ok"))              return hint_text_ok;
        if (key == QLatin1String("text_can_sing"))        return hint_text_can_sing;
        if (key == QLatin1String("text_go_to_sleep"))     return hint_text_go_to_sleep;
        if (key == QLatin1String("text_start_listening")) return hint_text_start_listening;
        if (key == QLatin1String("text_angry"))           return hint_text_angry;
        if (key.startsWith(QLatin1Char(':')) || key.contains(QLatin1Char('/'))) return key;
        return {};
    }
};

struct AudioProfile {
    QString angry                = QStringLiteral("qrc:/audio/angry.mp3");
    QString humming              = QStringLiteral("qrc:/audio/NormalSinging1.mp3");
    QList<VoiceMenuItem> voice_menu;
};

struct AnimationProfile {
    int frame_interval_ms                = 100;
    int light_intro_start                = 130;
    int light_intro_end                  = 400;
    int light_intro_size_duration_ms     = 1000;
    double light_intro_fade_start_opacity = 0.7;
    int light_intro_fade_duration_ms     = 500;
    int exit_light_start                 = 400;
    int exit_fade_in_duration_ms         = 800;
    int exit_shrink_duration_ms          = 1000;
    int exit_hide_character_threshold    = 300;
    int form_switch_halo_start           = 90;
    int form_switch_halo_end             = 510;
    int form_switch_expand_duration_ms   = 1000;
    double form_switch_flash_opacity     = 0.48;
    int form_switch_flash_duration_ms    = 220;
    int form_switch_shrink_duration_ms   = 1000;
    int fist_halo_end                    = 200;
    int fist_halo_duration_ms            = 600;
    int fist_track_interval_ms           = 16;
    double fist_acceleration             = 4.0;
    double fist_snap_distance            = 30.0;
    int fist_fade_duration_ms            = 200;
    double fist_pullback_factor          = 0.1;
    int fist_track_start_delay_ms        = 200;
    int static_halo_fade_duration_ms     = 500;
};

struct BehaviorProfile {
    double move_speed_px_per_sec         = 160.0;
    int move_min_duration_ms             = 1400;
    int move_max_duration_ms             = 12000;
    int stand_to_move_wait_ms            = 25000;
    int move_to_sleep_wait_ms            = 180000;
    int move_to_sit_wait_ms              = 25000;
    int move_to_playful_wait_ms          = 30000;
    int sit_detection_interval_ms        = 10000;
    int sit_trigger_chance_percent       = 30;
    int sit_mode_duration_ms             = 30000;
    int sit_monitor_interval_ms          = 100;
    int sit_fly_duration_ms              = 1500;
    int playful_detection_interval_ms    = 10000;
    int playful_trigger_chance_percent   = 30;
    int playful_mode_duration_ms         = 30000;
    int playmate_min_spacing_px          = 120;
    double playmate_speed_scale          = 2.0;
    double playmate_accel_scale          = 5.0;
    int playmate_chase_interval_ms       = 16;
    int angry_click_threshold            = 10;
    int angry_duration_ms                = 2000;
    int angry_spawn_offset_px            = 200;
    int click_reset_timeout_ms           = 2000;
    int stand_shake_duration_ms          = 500;
    int stand_shake_offset_px            = 5;
    int stand_shake_min_jumps            = 5;
    int stand_shake_max_jumps            = 9;
    int humming_min_interval_ms          = 45000;
    int humming_max_interval_ms          = 90000;
    int walk_restart_delay_ms            = 500;
    int hint_display_duration_ms         = 2000;
    int clock_display_duration_ms        = 10000;
    double drag_click_threshold_px       = 20.0;
    bool auto_sing_enabled_default       = true;
    CharacterForm character_form_default  = CharacterForm::Solyn;
};

struct BreathEffectProfile {
    bool   enabled              = true;
    int    poll_interval_ms     = 100;
    int    render_fps           = 24;
    double base_speed            = 0.5;
    double cpu_scale_factor      = 0.5;
    double min_alpha_factor      = 0.7;
    int    base_halo_size        = 200;
    double warm_tint_threshold  = 0.8;
    QColor warm_tint_color      = QColor(255, 153, 102);
    double warm_tint_intensity  = 0.3;
    bool   force_swap_rb        = false;
};

struct GomokuProfile {
    int    board_size            = 15;
    int    cell_size             = 50;
    int    board_padding         = 40;
    double border_line_width     = 3.5;
    double inner_line_width      = 1.2;
    QColor line_color            = QColor(218, 165, 32);
    QColor board_bg_color        = QColor(50, 40, 30, 220);
    int    piece_radius          = 20;
    int    particle_count        = 24;
    int    particle_duration_ms  = 250;
    int    win_glow_duration_ms  = 3000;
    int    fade_out_duration_ms  = 500;
    int    ai_move_fly_duration_ms = 400;
    int    ai_think_delay_ms     = 200;
};

struct UIProfile {
    QString clock_font_family    = QStringLiteral("Consolas");
    int clock_font_size          = 30;
    QColor clock_panel_color     = QColor(70, 130, 255, 95);
    int clock_panel_radius       = 18;
    QColor clock_shadow_color    = QColor(15, 25, 60, 180);
    QColor clock_text_color      = QColor(255, 225, 40, 255);
};

struct FoodItemDef {
    QString name;
    QString description;
    int effects[5] = {0, 0, 0, 0, 0};  // [Happiness, Interest, Sanity, Satiety, Affection]
    QString imagePath;  // per-item override; empty = use pattern from FoodMenuProfile
};

struct FoodMenuProfile {
    QString image_pattern = QStringLiteral(":/food1/food%1-0.png");
    QList<FoodItemDef> items;
};

struct StatusSystemProfile {
    bool enabled                = false;
    int  hover_delay_ms         = 3000;
    int  panel_fade_duration_ms = 500;
};

// =============================================
// ProfileManager — Singleton
// Thread-safety contract: all access must be from the GUI thread.
// loadFromFile() and all accessors are GUI-thread-only.
// =============================================
class ProfileManager : public QObject
{
    Q_OBJECT
public:
    static ProfileManager* instance();

    bool loadFromFile(const QString &jsonPath);

    const QString&         name()      const { return m_name; }
    int                    version()   const { return m_version; }
    const WindowProfile&   window()    const { return m_window; }
    const SpritesProfile&  sprites()   const { return m_sprites; }
    const AudioProfile&    audio()     const { return m_audio; }
    const AnimationProfile& animation() const { return m_anim; }
    const BehaviorProfile& behavior()  const { return m_behavior; }
    const BreathEffectProfile& breathEffect() const { return m_breathEffect; }
    const UIProfile&       ui()        const { return m_ui; }
    const StatusSystemProfile& statusSystem() const { return m_statusSystem; }
    const GomokuProfile&   gomoku()    const { return m_gomoku; }
    const FoodMenuProfile& foodMenu()  const { return m_foodMenu; }
    const ActionTopologyProfile& actionTopology() const { return m_actionTopology; }
    const PerceptionProfile& perception() const { return m_perception; }

    // Current loaded file path
    const QString& currentPath() const { return m_currentPath; }

signals:
    void profileReloaded();

private:
    explicit ProfileManager(QObject *parent = nullptr);
    ~ProfileManager() override = default;

    void applyDefaults();
    void parseRoot(const QJsonObject &root);
    void parseMeta(const QJsonObject &obj);
    void parseWindow(const QJsonObject &obj);
    void parseSprites(const QJsonObject &obj);
    void parseFormDef(const QJsonObject &obj, FormDef &form);
    SpriteEntry parseSpriteEntry(const QJsonObject &obj);
    void parseHints(const QJsonObject &obj);
    void parseAudio(const QJsonObject &obj);
    void parseAnimation(const QJsonObject &obj);
    void parseBehavior(const QJsonObject &obj);
    void parseBreathEffect(const QJsonObject &obj);
    void parseUI(const QJsonObject &obj);
    void parseStatusSystem(const QJsonObject &obj);
    void parseGomoku(const QJsonObject &obj);
    void parseFoodMenu(const QJsonObject &obj);
    void parseStateTransitions(const QJsonObject &obj);
    void parsePerception(const QJsonObject &obj);
    void applyDefaultTopology();

    static QColor jsonArrayToColor(const QJsonArray &arr, const QColor &fallback);

    QString          m_currentPath;
    QString          m_name;
    int              m_version = 1;
    WindowProfile    m_window;
    SpritesProfile   m_sprites;
    AudioProfile     m_audio;
    AnimationProfile m_anim;
    BehaviorProfile  m_behavior;
    BreathEffectProfile m_breathEffect;
    UIProfile        m_ui;
    StatusSystemProfile m_statusSystem;
    GomokuProfile    m_gomoku;
    FoodMenuProfile  m_foodMenu;
    ActionTopologyProfile m_actionTopology;
    PerceptionProfile m_perception;
};

#endif // PROFILEMANAGER_H
