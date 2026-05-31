#include "profilemanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QCoreApplication>
#include <QDebug>

// =============================================
// Singleton
// =============================================
ProfileManager* ProfileManager::instance() {
    static ProfileManager s_instance;
    return &s_instance;
}

ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent)
{
    applyDefaults();
}

// =============================================
// Safe defaults — called before any JSON parse
// =============================================
void ProfileManager::applyDefaults() {
    m_name    = QStringLiteral("Solyn");
    m_version = 1;
    m_window   = WindowProfile();
    m_sprites  = SpritesProfile();
    m_audio    = AudioProfile();
    m_anim     = AnimationProfile();
    m_behavior     = BehaviorProfile();
    m_breathEffect = BreathEffectProfile();
    m_ui           = UIProfile();

    // Default Solyn form
    FormDef solynDef;
    solynDef.stand      = { QStringLiteral(":/character1/solyn%d.png"), 1 };
    solynDef.move_left  = { QStringLiteral(":/move1/solynmoveleft%d.png"), 1 };
    solynDef.move_right = { QStringLiteral(":/move1/solynmoveright%d.png"), 1 };
    solynDef.sleeping   = { QStringLiteral(":/sleeping1/solynsleeping%d.png"), 1 };
    solynDef.angry      = { QStringLiteral(":/characterangry1/solynangry%d.png"), 1 };
    solynDef.sitting_1  = { QStringLiteral(":/character2/solyn2-%d.png"), 1 };
    solynDef.sitting_2  = { QStringLiteral(":/character3/solyn3-%d.png"), 1 };
    m_sprites.forms.insert(CharacterForm::Solyn, solynDef);

    // Default StarDaughter form (inherits sitting from Solyn)
    FormDef starDef;
    starDef.stand      = { QStringLiteral(":/character4/stardaughter%d.png"), 1 };
    starDef.move_left  = { QStringLiteral(":/move1/stardaughtermoveleft%d.png"), 1 };
    starDef.move_right = { QStringLiteral(":/move1/stardaughtermoveright%d.png"), 1 };
    starDef.sleeping   = { QStringLiteral(":/sleeping1/stardaughtersleeping%d.png"), 1 };
    starDef.angry      = { QStringLiteral(":/characterangry1/stardaughterangry%d.png"), 1 };
    starDef.sitting_1  = solynDef.sitting_1;
    starDef.sitting_2  = solynDef.sitting_2;
    m_sprites.forms.insert(CharacterForm::StarDaughter, starDef);

    // Default pull sprites per form
    m_sprites.pull.insert(CharacterForm::Solyn,        QStringLiteral(":/pull1/solynpull0.png"));
    m_sprites.pull.insert(CharacterForm::StarDaughter, QStringLiteral(":/pull1/stardaughterpull0.png"));

    // Default food menu (fallback if JSON missing)
    m_foodMenu = FoodMenuProfile();
    m_foodMenu.items = {
        { QStringLiteral("流光"),               QStringLiteral("饱食+5"),              {  0,   0,   0,   5,   0}, {} },
        { QStringLiteral("创始之影"),           QStringLiteral("饱食+10 兴致+3"),      {  0,   3,   0,  10,   0}, {} },
        { QStringLiteral("终末之影"),           QStringLiteral("饱食+10 理智+3"),      {  0,   0,   3,  10,   0}, {} },
        { QStringLiteral("星河之影"),           QStringLiteral("饱食+10 快乐+3"),      {  3,   0,   0,  10,   0}, {} },
        { QStringLiteral("以太相位引擎"),       QStringLiteral("饱食+20 兴致+5 理智+5"),{  0,   5,   5,  20,   0}, {} },
        { QStringLiteral("KFC"),                QStringLiteral("饱食+15 兴致+5 快乐+5"),{  5,   5,   0,  15,   0}, {} },
        { QStringLiteral("方便面"),             QStringLiteral("饱食+3 兴致+1 快乐+1"), {  1,   1,   0,   3,   0}, {} },
        { QStringLiteral("可乐"),               QStringLiteral("饱食+1 兴致+1 快乐+1"), {  1,   1,   0,   1,   0}, {} },
        { QStringLiteral("牛排"),               QStringLiteral("饱食+15 快乐+5 兴致+5 理智+3"),{5, 5, 3, 15, 0}, {} },
        { QStringLiteral("意大利面"),           QStringLiteral("饱食+10 快乐+5 兴致+5"),{  5,   5,   0,  10,   0}, {} },
        { QStringLiteral("黑松露烩饭"),         QStringLiteral("饱食+15 兴致+5 快乐+5"),{  5,   5,   0,  15,   0}, {} },
        { QStringLiteral("法式焦糖布丁"),       QStringLiteral("饱食+5 兴致+10 快乐+10"),{ 10, 10,   0,   5,   0}, {} },
        { QStringLiteral("红烧肉"),             QStringLiteral("饱食+10 快乐+5 兴致+5"),{  5,   5,   0,  10,   0}, {} },
        { QStringLiteral("您的账号已被封禁"),   QStringLiteral("快乐-10 兴致-10 理智-5 亲密-5"),{-10,-10,-5, 0,-5}, {} },
        { QStringLiteral("你不干，有的是人干"), QStringLiteral("快乐-90 兴致-90 理智-90 亲密-90"),{-90,-90,-90,0,-90}, {} },
        { QStringLiteral("电路原理"),           QStringLiteral("饱食+1 快乐-20 兴致-30 理智-10"),{-20,-30,-10, 1, 0}, {} },
        { QStringLiteral("高等数学"),           QStringLiteral("饱食+1 快乐-10 兴致-10 理智-10"),{-10,-10,-10, 1, 0}, {} },
        { QStringLiteral("C语言"),              QStringLiteral("饱食+1 快乐+3 兴致+3 理智+10"),{  3,   3,  10,  1,   0}, {} },
        { QStringLiteral("OS/计组/计网/数据结构"), QStringLiteral("饱食+3 快乐+3 兴致+3 理智+20"),{  3,   3,  20,  3,   0}, {} },
        { QStringLiteral("三体"),               QStringLiteral("饱食+1 快乐+10 兴致+10 理智+10"),{ 10,  10,  10,  1,   0}, {} },
    };

    // Default voice menu
    m_audio.voice_menu = {
        { QStringLiteral("你在吗？"),                                QStringLiteral("qrc:/audio/AreYouHere.mp3") },
        { QStringLiteral("对于你，我很重要吗？"),                      QStringLiteral("qrc:/audio/IImportant.mp3") },
        { QStringLiteral("向她哭诉"),                                QStringLiteral("qrc:/audio/CrytoHer.mp3") },
        { QStringLiteral("对不起，我不太舒服，你不会嫌弃我吧？"),       QStringLiteral("qrc:/audio/SorryUncomfortable.mp3") },
        { QStringLiteral("我以前受过太多的伤害"),                      QStringLiteral("qrc:/audio/UndergotooMuch.mp3") },
        { QStringLiteral("我很烦，很麻烦吗？"),                       QStringLiteral("qrc:/audio/IVeryAnnoyed.mp3") },
        { QStringLiteral("我做出这样的决定，你会讨厌我吗？"),           QStringLiteral("qrc:/audio/DoEverything.mp3") },
        { QStringLiteral("未来会更好吗？"),                           QStringLiteral("qrc:/audio/WillbeBetter.mp3") },
        { QStringLiteral("祝我生日快乐"),                             QStringLiteral("qrc:/audio/birthdaysong.mp3") },
    };
}

// =============================================
// Load from JSON file
// =============================================
bool ProfileManager::loadFromFile(const QString &jsonPath) {
    // Reset to safe defaults first
    applyDefaults();

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[ProfileManager] Cannot open" << jsonPath
                    << "- using built-in defaults.";
        m_currentPath = jsonPath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    // Strip UTF-8 BOM if present — QJsonDocument::fromJson requires pure UTF-8
    if (data.size() >= 3
        && static_cast<unsigned char>(data[0]) == 0xEF
        && static_cast<unsigned char>(data[1]) == 0xBB
        && static_cast<unsigned char>(data[2]) == 0xBF) {
        data.remove(0, 3);
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[ProfileManager] JSON parse error in" << jsonPath
                    << ":" << parseError.errorString()
                    << "- using built-in defaults.";
        m_currentPath = jsonPath;
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "[ProfileManager] Root is not a JSON object - using built-in defaults.";
        m_currentPath = jsonPath;
        return false;
    }

    m_currentPath = jsonPath;
    parseRoot(doc.object());

    qDebug() << "[ProfileManager] Loaded profile:" << m_name
             << "v" << m_version << "from" << jsonPath;

    emit profileReloaded();
    return true;
}

// =============================================
// JSON helpers — every value has an inline fallback
// =============================================

static int safeInt(const QJsonObject &o, const QString &key, int def) {
    if (o.contains(key) && o[key].isDouble()) return o[key].toInt(def);
    return def;
}

static double safeDouble(const QJsonObject &o, const QString &key, double def) {
    if (o.contains(key) && o[key].isDouble()) return o[key].toDouble(def);
    return def;
}

static bool safeBool(const QJsonObject &o, const QString &key, bool def) {
    if (o.contains(key) && o[key].isBool()) return o[key].toBool(def);
    return def;
}

static QString safeString(const QJsonObject &o, const QString &key, const QString &def) {
    if (o.contains(key) && o[key].isString()) return o[key].toString(def);
    return def;
}

// =============================================
// Section parsers
// =============================================

void ProfileManager::parseRoot(const QJsonObject &root) {
    if (root.contains("meta") && root["meta"].isObject())
        parseMeta(root["meta"].toObject());
    if (root.contains("window") && root["window"].isObject())
        parseWindow(root["window"].toObject());
    if (root.contains("sprites") && root["sprites"].isObject())
        parseSprites(root["sprites"].toObject());
    if (root.contains("audio") && root["audio"].isObject())
        parseAudio(root["audio"].toObject());
    if (root.contains("animation") && root["animation"].isObject())
        parseAnimation(root["animation"].toObject());
    if (root.contains("behavior") && root["behavior"].isObject())
        parseBehavior(root["behavior"].toObject());
    if (root.contains("breath_effect") && root["breath_effect"].isObject())
        parseBreathEffect(root["breath_effect"].toObject());
    if (root.contains("ui") && root["ui"].isObject())
        parseUI(root["ui"].toObject());
    if (root.contains("status_system") && root["status_system"].isObject())
        parseStatusSystem(root["status_system"].toObject());
    if (root.contains("gomoku") && root["gomoku"].isObject())
        parseGomoku(root["gomoku"].toObject());
    if (root.contains("food_menu") && root["food_menu"].isObject())
        parseFoodMenu(root["food_menu"].toObject());
}

void ProfileManager::parseMeta(const QJsonObject &obj) {
    m_name    = safeString(obj, "name", m_name);
    m_version = safeInt(obj, "version", m_version);
}

void ProfileManager::parseWindow(const QJsonObject &obj) {
    m_window.widget_size      = safeInt(obj, "widget_size",      m_window.widget_size);
    m_window.fist_widget_size = safeInt(obj, "fist_widget_size", m_window.fist_widget_size);
    m_window.fist_sprite_size = safeInt(obj, "fist_sprite_size", m_window.fist_sprite_size);
}

// JSON form-name → enum mapping (add new forms here)
static const QMap<QString, CharacterForm> kFormNameMap = {
    { QStringLiteral("solyn"),        CharacterForm::Solyn },
    { QStringLiteral("stardaughter"), CharacterForm::StarDaughter },
};

void ProfileManager::parseSprites(const QJsonObject &obj) {
    m_sprites.role_display_size = safeInt(obj, "role_display_size", m_sprites.role_display_size);
    m_sprites.light_symbol      = safeString(obj, "light_symbol", m_sprites.light_symbol);
    m_sprites.fist              = safeString(obj, "fist",         m_sprites.fist);
    m_sprites.icon              = safeString(obj, "icon",         m_sprites.icon);

    // M-04: Data-driven resource paths (formerly hard-coded)
    m_sprites.chatbox_bg        = safeString(obj, "chatbox_bg",        m_sprites.chatbox_bg);
    m_sprites.hand_sprite       = safeString(obj, "hand_sprite",       m_sprites.hand_sprite);
    m_sprites.star_pattern      = safeString(obj, "star_pattern",      m_sprites.star_pattern);
    m_sprites.star_variant_count = qBound(1, safeInt(obj, "star_variant_count", m_sprites.star_variant_count), 50);

    // Parse pull sprites per form
    if (obj.contains("pull") && obj["pull"].isObject()) {
        const QJsonObject pullObj = obj["pull"].toObject();
        for (auto it = pullObj.begin(); it != pullObj.end(); ++it) {
            auto formIt = kFormNameMap.constFind(it.key());
            if (formIt != kFormNameMap.constEnd() && it.value().isString())
                m_sprites.pull[formIt.value()] = it.value().toString();
        }
    }

    // Parse form definitions dynamically via name mapping
    if (obj.contains("forms") && obj["forms"].isObject()) {
        const QJsonObject formsObj = obj["forms"].toObject();
        for (auto it = formsObj.begin(); it != formsObj.end(); ++it) {
            auto formIt = kFormNameMap.constFind(it.key());
            if (formIt != kFormNameMap.constEnd() && it.value().isObject()) {
                // Ensure the map entry exists (may already have defaults)
                FormDef &fd = m_sprites.forms[formIt.value()];
                parseFormDef(it.value().toObject(), fd);
            }
        }
    }

    if (obj.contains("hints") && obj["hints"].isObject())
        parseHints(obj["hints"].toObject());
}

void ProfileManager::parseFormDef(const QJsonObject &obj, FormDef &form) {
    auto tryParse = [&](const QString &key, SpriteEntry &entry) {
        if (obj.contains(key) && obj[key].isObject())
            entry = parseSpriteEntry(obj[key].toObject());
    };
    tryParse("stand",      form.stand);
    tryParse("move_left",  form.move_left);
    tryParse("move_right", form.move_right);
    tryParse("sleeping",   form.sleeping);
    tryParse("angry",      form.angry);
    tryParse("sitting_1",  form.sitting_1);
    tryParse("sitting_2",  form.sitting_2);
}

SpriteEntry ProfileManager::parseSpriteEntry(const QJsonObject &obj) {
    SpriteEntry e;
    e.pattern = safeString(obj, "pattern", QString());
    e.count   = safeInt(obj, "count", 1);
    if (e.count < 1) e.count = 1;
    return e;
}

void ProfileManager::parseHints(const QJsonObject &obj) {
    m_sprites.hint_text_ok              = safeString(obj, "text_ok",              m_sprites.hint_text_ok);
    m_sprites.hint_text_can_sing        = safeString(obj, "text_can_sing",        m_sprites.hint_text_can_sing);
    m_sprites.hint_text_go_to_sleep     = safeString(obj, "text_go_to_sleep",     m_sprites.hint_text_go_to_sleep);
    m_sprites.hint_text_start_listening = safeString(obj, "text_start_listening", m_sprites.hint_text_start_listening);
    m_sprites.hint_text_angry           = safeString(obj, "text_angry",           m_sprites.hint_text_angry);
}

void ProfileManager::parseAudio(const QJsonObject &obj) {
    m_audio.angry   = safeString(obj, "angry",   m_audio.angry);
    m_audio.humming = safeString(obj, "humming", m_audio.humming);

    if (obj.contains("voice_menu") && obj["voice_menu"].isArray()) {
        QList<VoiceMenuItem> items;
        const QJsonArray arr = obj["voice_menu"].toArray();
        for (const QJsonValue &val : arr) {
            if (!val.isObject()) continue;
            QJsonObject item = val.toObject();
            VoiceMenuItem mi;
            mi.label = safeString(item, "label", QString());
            mi.path  = safeString(item, "path",  QString());
            if (!mi.label.isEmpty() && !mi.path.isEmpty())
                items.append(mi);
        }
        if (!items.isEmpty())
            m_audio.voice_menu = items;
    }
}

void ProfileManager::parseAnimation(const QJsonObject &o) {
    auto &a = m_anim;
    a.frame_interval_ms              = safeInt(o, "frame_interval_ms",              a.frame_interval_ms);
    a.light_intro_start              = safeInt(o, "light_intro_start",              a.light_intro_start);
    a.light_intro_end                = safeInt(o, "light_intro_end",                a.light_intro_end);
    a.light_intro_size_duration_ms   = safeInt(o, "light_intro_size_duration_ms",   a.light_intro_size_duration_ms);
    a.light_intro_fade_start_opacity = safeDouble(o, "light_intro_fade_start_opacity", a.light_intro_fade_start_opacity);
    a.light_intro_fade_duration_ms   = safeInt(o, "light_intro_fade_duration_ms",   a.light_intro_fade_duration_ms);
    a.exit_light_start               = safeInt(o, "exit_light_start",               a.exit_light_start);
    a.exit_fade_in_duration_ms       = safeInt(o, "exit_fade_in_duration_ms",       a.exit_fade_in_duration_ms);
    a.exit_shrink_duration_ms        = safeInt(o, "exit_shrink_duration_ms",        a.exit_shrink_duration_ms);
    a.exit_hide_character_threshold  = safeInt(o, "exit_hide_character_threshold",   a.exit_hide_character_threshold);
    a.form_switch_halo_start         = safeInt(o, "form_switch_halo_start",         a.form_switch_halo_start);
    a.form_switch_halo_end           = safeInt(o, "form_switch_halo_end",           a.form_switch_halo_end);
    a.form_switch_expand_duration_ms = safeInt(o, "form_switch_expand_duration_ms", a.form_switch_expand_duration_ms);
    a.form_switch_flash_opacity      = safeDouble(o, "form_switch_flash_opacity",   a.form_switch_flash_opacity);
    a.form_switch_flash_duration_ms  = safeInt(o, "form_switch_flash_duration_ms",  a.form_switch_flash_duration_ms);
    a.form_switch_shrink_duration_ms = safeInt(o, "form_switch_shrink_duration_ms", a.form_switch_shrink_duration_ms);
    a.fist_halo_end                  = safeInt(o, "fist_halo_end",                  a.fist_halo_end);
    a.fist_halo_duration_ms          = safeInt(o, "fist_halo_duration_ms",          a.fist_halo_duration_ms);
    a.fist_track_interval_ms         = safeInt(o, "fist_track_interval_ms",         a.fist_track_interval_ms);
    a.fist_acceleration              = safeDouble(o, "fist_acceleration",            a.fist_acceleration);
    a.fist_snap_distance             = safeDouble(o, "fist_snap_distance",           a.fist_snap_distance);
    a.fist_fade_duration_ms          = safeInt(o, "fist_fade_duration_ms",          a.fist_fade_duration_ms);
    a.fist_pullback_factor           = safeDouble(o, "fist_pullback_factor",        a.fist_pullback_factor);
    a.fist_track_start_delay_ms      = safeInt(o, "fist_track_start_delay_ms",     a.fist_track_start_delay_ms);
    a.static_halo_fade_duration_ms   = safeInt(o, "static_halo_fade_duration_ms",   a.static_halo_fade_duration_ms);

    // M-01: Clamp all timing/size fields to safe ranges
    a.frame_interval_ms              = qBound(16, a.frame_interval_ms, 1000);
    a.light_intro_start              = qBound(0,  a.light_intro_start, 2000);
    a.light_intro_end                = qBound(a.light_intro_start, a.light_intro_end, 2000);
    a.light_intro_size_duration_ms   = qBound(100, a.light_intro_size_duration_ms, 10000);
    a.light_intro_fade_start_opacity = qBound(0.0, a.light_intro_fade_start_opacity, 1.0);
    a.light_intro_fade_duration_ms   = qBound(50,  a.light_intro_fade_duration_ms, 5000);
    a.exit_light_start               = qBound(0,   a.exit_light_start, 2000);
    a.exit_fade_in_duration_ms       = qBound(50,  a.exit_fade_in_duration_ms, 5000);
    a.exit_shrink_duration_ms        = qBound(50,  a.exit_shrink_duration_ms, 5000);
    a.exit_hide_character_threshold  = qBound(0,   a.exit_hide_character_threshold, 2000);
    a.form_switch_halo_start         = qBound(0,   a.form_switch_halo_start, 2000);
    a.form_switch_halo_end           = qBound(a.form_switch_halo_start, a.form_switch_halo_end, 2000);
    a.form_switch_expand_duration_ms = qBound(50,  a.form_switch_expand_duration_ms, 5000);
    a.form_switch_flash_opacity      = qBound(0.0, a.form_switch_flash_opacity, 1.0);
    a.form_switch_flash_duration_ms  = qBound(50,  a.form_switch_flash_duration_ms, 2000);
    a.form_switch_shrink_duration_ms = qBound(50,  a.form_switch_shrink_duration_ms, 5000);
    a.fist_halo_end                  = qBound(10,  a.fist_halo_end, 1000);
    a.fist_halo_duration_ms          = qBound(50,  a.fist_halo_duration_ms, 5000);
    a.fist_track_interval_ms         = qBound(8,   a.fist_track_interval_ms, 200);
    a.fist_acceleration              = qBound(0.1, a.fist_acceleration, 100.0);
    a.fist_snap_distance             = qBound(1.0, a.fist_snap_distance, 500.0);
    a.fist_fade_duration_ms          = qBound(50,  a.fist_fade_duration_ms, 3000);
    a.fist_pullback_factor           = qBound(0.0, a.fist_pullback_factor, 1.0);
    a.fist_track_start_delay_ms      = qBound(0,   a.fist_track_start_delay_ms, 3000);
    a.static_halo_fade_duration_ms   = qBound(50,  a.static_halo_fade_duration_ms, 5000);
}

void ProfileManager::parseBehavior(const QJsonObject &o) {
    auto &b = m_behavior;
    b.move_speed_px_per_sec        = safeDouble(o, "move_speed_px_per_sec",        b.move_speed_px_per_sec);
    b.move_min_duration_ms         = safeInt(o, "move_min_duration_ms",            b.move_min_duration_ms);
    b.move_max_duration_ms         = safeInt(o, "move_max_duration_ms",            b.move_max_duration_ms);
    b.stand_to_move_wait_ms        = safeInt(o, "stand_to_move_wait_ms",           b.stand_to_move_wait_ms);
    b.move_to_sleep_wait_ms        = safeInt(o, "move_to_sleep_wait_ms",           b.move_to_sleep_wait_ms);
    b.move_to_sit_wait_ms          = safeInt(o, "move_to_sit_wait_ms",             b.move_to_sit_wait_ms);
    b.move_to_playful_wait_ms      = safeInt(o, "move_to_playful_wait_ms",         b.move_to_playful_wait_ms);
    b.sit_detection_interval_ms    = safeInt(o, "sit_detection_interval_ms",        b.sit_detection_interval_ms);
    b.sit_trigger_chance_percent   = safeInt(o, "sit_trigger_chance_percent",       b.sit_trigger_chance_percent);
    b.sit_mode_duration_ms         = safeInt(o, "sit_mode_duration_ms",             b.sit_mode_duration_ms);
    b.sit_monitor_interval_ms      = safeInt(o, "sit_monitor_interval_ms",          b.sit_monitor_interval_ms);
    b.sit_fly_duration_ms          = safeInt(o, "sit_fly_duration_ms",              b.sit_fly_duration_ms);
    b.playful_detection_interval_ms  = safeInt(o, "playful_detection_interval_ms",  b.playful_detection_interval_ms);
    b.playful_trigger_chance_percent = safeInt(o, "playful_trigger_chance_percent",  b.playful_trigger_chance_percent);
    b.playful_mode_duration_ms     = safeInt(o, "playful_mode_duration_ms",         b.playful_mode_duration_ms);
    b.playmate_min_spacing_px      = safeInt(o, "playmate_min_spacing_px",          b.playmate_min_spacing_px);
    b.playmate_speed_scale         = safeDouble(o, "playmate_speed_scale",          b.playmate_speed_scale);
    b.playmate_accel_scale         = safeDouble(o, "playmate_accel_scale",          b.playmate_accel_scale);
    b.playmate_chase_interval_ms   = safeInt(o, "playmate_chase_interval_ms",       b.playmate_chase_interval_ms);
    b.angry_click_threshold        = safeInt(o, "angry_click_threshold",            b.angry_click_threshold);
    b.angry_duration_ms            = safeInt(o, "angry_duration_ms",                b.angry_duration_ms);
    b.angry_spawn_offset_px        = safeInt(o, "angry_spawn_offset_px",            b.angry_spawn_offset_px);
    b.click_reset_timeout_ms       = safeInt(o, "click_reset_timeout_ms",           b.click_reset_timeout_ms);
    b.stand_shake_duration_ms      = safeInt(o, "stand_shake_duration_ms",          b.stand_shake_duration_ms);
    b.stand_shake_offset_px        = safeInt(o, "stand_shake_offset_px",            b.stand_shake_offset_px);
    b.stand_shake_min_jumps        = safeInt(o, "stand_shake_min_jumps",            b.stand_shake_min_jumps);
    b.stand_shake_max_jumps        = safeInt(o, "stand_shake_max_jumps",            b.stand_shake_max_jumps);
    b.humming_min_interval_ms      = safeInt(o, "humming_min_interval_ms",          b.humming_min_interval_ms);
    b.humming_max_interval_ms      = safeInt(o, "humming_max_interval_ms",          b.humming_max_interval_ms);
    b.walk_restart_delay_ms        = safeInt(o, "walk_restart_delay_ms",             b.walk_restart_delay_ms);
    b.hint_display_duration_ms     = safeInt(o, "hint_display_duration_ms",          b.hint_display_duration_ms);
    b.clock_display_duration_ms    = safeInt(o, "clock_display_duration_ms",         b.clock_display_duration_ms);
    b.drag_click_threshold_px      = safeDouble(o, "drag_click_threshold_px",        b.drag_click_threshold_px);
    b.auto_sing_enabled_default    = safeBool(o, "auto_sing_enabled",                b.auto_sing_enabled_default);
    b.character_form_default        = static_cast<CharacterForm>(
        safeInt(o, "character_form", static_cast<int>(b.character_form_default)));

    // M-01: Clamp all numeric fields to safe ranges
    b.move_speed_px_per_sec        = qBound(1.0, b.move_speed_px_per_sec, 5000.0);
    b.move_min_duration_ms         = qBound(100, b.move_min_duration_ms, 60000);
    b.move_max_duration_ms         = qBound(b.move_min_duration_ms, b.move_max_duration_ms, 120000);
    b.stand_to_move_wait_ms        = qBound(1000, b.stand_to_move_wait_ms, 600000);
    b.move_to_sleep_wait_ms        = qBound(1000, b.move_to_sleep_wait_ms, 3600000);
    b.move_to_sit_wait_ms          = qBound(1000, b.move_to_sit_wait_ms, 600000);
    b.move_to_playful_wait_ms      = qBound(1000, b.move_to_playful_wait_ms, 600000);
    b.sit_detection_interval_ms    = qBound(500, b.sit_detection_interval_ms, 120000);
    b.sit_trigger_chance_percent   = qBound(0, b.sit_trigger_chance_percent, 100);
    b.sit_mode_duration_ms         = qBound(1000, b.sit_mode_duration_ms, 600000);
    b.sit_monitor_interval_ms      = qBound(50, b.sit_monitor_interval_ms, 5000);
    b.sit_fly_duration_ms          = qBound(100, b.sit_fly_duration_ms, 10000);
    b.playful_detection_interval_ms  = qBound(500, b.playful_detection_interval_ms, 120000);
    b.playful_trigger_chance_percent = qBound(0, b.playful_trigger_chance_percent, 100);
    b.playful_mode_duration_ms     = qBound(1000, b.playful_mode_duration_ms, 600000);
    b.playmate_min_spacing_px      = qBound(20, b.playmate_min_spacing_px, 1000);
    b.playmate_speed_scale         = qBound(0.1, b.playmate_speed_scale, 20.0);
    b.playmate_accel_scale         = qBound(0.1, b.playmate_accel_scale, 50.0);
    b.playmate_chase_interval_ms   = qBound(8, b.playmate_chase_interval_ms, 200);
    b.angry_click_threshold        = qBound(1, b.angry_click_threshold, 100);
    b.angry_duration_ms            = qBound(100, b.angry_duration_ms, 30000);
    b.angry_spawn_offset_px        = qBound(10, b.angry_spawn_offset_px, 2000);
    b.click_reset_timeout_ms       = qBound(100, b.click_reset_timeout_ms, 30000);
    b.stand_shake_duration_ms      = qBound(50, b.stand_shake_duration_ms, 5000);
    b.stand_shake_offset_px        = qBound(1, b.stand_shake_offset_px, 100);
    b.stand_shake_min_jumps        = qBound(1, b.stand_shake_min_jumps, 50);
    b.stand_shake_max_jumps        = qBound(b.stand_shake_min_jumps, b.stand_shake_max_jumps, 100);
    b.humming_min_interval_ms      = qBound(1000, b.humming_min_interval_ms, 600000);
    b.humming_max_interval_ms      = qBound(b.humming_min_interval_ms + 1, b.humming_max_interval_ms, 1200000);
    b.walk_restart_delay_ms        = qBound(0, b.walk_restart_delay_ms, 10000);
    b.hint_display_duration_ms     = qBound(200, b.hint_display_duration_ms, 30000);
    b.clock_display_duration_ms    = qBound(1000, b.clock_display_duration_ms, 120000);
    b.drag_click_threshold_px      = qBound(1.0, b.drag_click_threshold_px, 200.0);
}

void ProfileManager::parseBreathEffect(const QJsonObject &o) {
    auto &b = m_breathEffect;
    b.enabled             = safeBool(o,   "enabled",             b.enabled);
    b.poll_interval_ms    = safeInt(o,    "poll_interval_ms",    b.poll_interval_ms);
    b.render_fps          = safeInt(o,    "render_fps",          b.render_fps);
    b.base_speed          = safeDouble(o, "base_speed",          b.base_speed);
    b.cpu_scale_factor    = safeDouble(o, "cpu_scale_factor",    b.cpu_scale_factor);
    b.min_alpha_factor    = safeDouble(o, "min_alpha_factor",    b.min_alpha_factor);
    b.base_halo_size      = safeInt(o,    "base_halo_size",      b.base_halo_size);
    b.warm_tint_threshold = safeDouble(o, "warm_tint_threshold", b.warm_tint_threshold);
    b.warm_tint_intensity = safeDouble(o, "warm_tint_intensity", b.warm_tint_intensity);

    if (o.contains("warm_tint_color") && o["warm_tint_color"].isArray())
        b.warm_tint_color = jsonArrayToColor(o["warm_tint_color"].toArray(), b.warm_tint_color);
    b.force_swap_rb       = safeBool(o,   "force_swap_rb",       b.force_swap_rb);
}

void ProfileManager::parseUI(const QJsonObject &o) {
    m_ui.clock_font_family  = safeString(o, "clock_font_family", m_ui.clock_font_family);
    m_ui.clock_font_size    = safeInt(o, "clock_font_size",      m_ui.clock_font_size);
    m_ui.clock_panel_radius = safeInt(o, "clock_panel_radius",   m_ui.clock_panel_radius);

    if (o.contains("clock_panel_color") && o["clock_panel_color"].isArray())
        m_ui.clock_panel_color = jsonArrayToColor(o["clock_panel_color"].toArray(), m_ui.clock_panel_color);
    if (o.contains("clock_shadow_color") && o["clock_shadow_color"].isArray())
        m_ui.clock_shadow_color = jsonArrayToColor(o["clock_shadow_color"].toArray(), m_ui.clock_shadow_color);
    if (o.contains("clock_text_color") && o["clock_text_color"].isArray())
        m_ui.clock_text_color = jsonArrayToColor(o["clock_text_color"].toArray(), m_ui.clock_text_color);
}

void ProfileManager::parseStatusSystem(const QJsonObject &o) {
    auto &ss = m_statusSystem;
    ss.enabled                = safeBool(o, "enabled",                ss.enabled);
    ss.hover_delay_ms         = qBound(500, safeInt(o, "hover_delay_ms", ss.hover_delay_ms), 10000);
    ss.panel_fade_duration_ms = qBound(100, safeInt(o, "panel_fade_duration_ms", ss.panel_fade_duration_ms), 3000);
}

void ProfileManager::parseGomoku(const QJsonObject &o) {
    auto &g = m_gomoku;
    g.board_size            = safeInt(o,    "board_size",            g.board_size);
    g.cell_size             = safeInt(o,    "cell_size",             g.cell_size);
    g.board_padding         = safeInt(o,    "board_padding",         g.board_padding);
    g.border_line_width     = safeDouble(o, "border_line_width",     g.border_line_width);
    g.inner_line_width      = safeDouble(o, "inner_line_width",      g.inner_line_width);
    g.piece_radius          = safeInt(o,    "piece_radius",          g.piece_radius);
    g.particle_count        = safeInt(o,    "particle_count",        g.particle_count);
    g.particle_duration_ms  = safeInt(o,    "particle_duration_ms",  g.particle_duration_ms);
    g.win_glow_duration_ms  = safeInt(o,    "win_glow_duration_ms",  g.win_glow_duration_ms);
    g.fade_out_duration_ms  = safeInt(o,    "fade_out_duration_ms",  g.fade_out_duration_ms);
    g.ai_move_fly_duration_ms = safeInt(o,  "ai_move_fly_duration_ms", g.ai_move_fly_duration_ms);
    g.ai_think_delay_ms     = safeInt(o,    "ai_think_delay_ms",     g.ai_think_delay_ms);

    if (o.contains("line_color") && o["line_color"].isArray())
        g.line_color = jsonArrayToColor(o["line_color"].toArray(), g.line_color);
    if (o.contains("board_bg_color") && o["board_bg_color"].isArray())
        g.board_bg_color = jsonArrayToColor(o["board_bg_color"].toArray(), g.board_bg_color);

    // M-2: Clamp all numeric fields to safe ranges (prevent div-by-zero / overflow)
    g.board_size            = qBound(5, g.board_size, 19);
    g.cell_size             = qBound(20, g.cell_size, 120);
    g.board_padding         = qBound(10, g.board_padding, 200);
    g.border_line_width     = qBound(0.5, g.border_line_width, 10.0);
    g.inner_line_width      = qBound(0.2, g.inner_line_width, 5.0);
    g.piece_radius          = qBound(5, g.piece_radius, g.cell_size / 2);
    g.particle_count        = qBound(1, g.particle_count, 200);
    g.particle_duration_ms  = qBound(50, g.particle_duration_ms, 5000);
    g.win_glow_duration_ms  = qBound(100, g.win_glow_duration_ms, 10000);
    g.fade_out_duration_ms  = qBound(100, g.fade_out_duration_ms, 5000);
    g.ai_move_fly_duration_ms = qBound(50, g.ai_move_fly_duration_ms, 3000);
    g.ai_think_delay_ms     = qBound(0, g.ai_think_delay_ms, 5000);
}

void ProfileManager::parseFoodMenu(const QJsonObject &o) {
    m_foodMenu.image_pattern = safeString(o, "image_pattern", m_foodMenu.image_pattern);

    if (o.contains("items") && o["items"].isArray()) {
        const QJsonArray arr = o["items"].toArray();
        if (!arr.isEmpty()) {
            QList<FoodItemDef> items;
            for (const QJsonValue &val : arr) {
                if (!val.isObject()) continue;
                QJsonObject item = val.toObject();
                FoodItemDef fd;
                fd.name        = safeString(item, "name", QString());
                fd.description = safeString(item, "desc", QString());
                fd.imagePath   = safeString(item, "image", QString());

                // Parse effects array [Happiness, Interest, Sanity, Satiety, Affection]
                if (item.contains("effects") && item["effects"].isArray()) {
                    QJsonArray ea = item["effects"].toArray();
                    for (int k = 0; k < 5 && k < ea.size(); ++k)
                        fd.effects[k] = ea[k].toInt(0);
                }

                if (!fd.name.isEmpty())
                    items.append(fd);
            }
            if (!items.isEmpty())
                m_foodMenu.items = items;
        }
    }
}

QColor ProfileManager::jsonArrayToColor(const QJsonArray &arr, const QColor &fallback) {
    if (arr.size() < 3) return fallback;
    int r = arr[0].toInt(0);
    int g = arr[1].toInt(0);
    int b = arr[2].toInt(0);
    int a = (arr.size() >= 4) ? arr[3].toInt(255) : 255;
    return QColor(r, g, b, a);
}
