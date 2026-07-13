#include "widget.h"
#include "configmanager.h"
#include "spriteresource.h"
#include "platformhal.h"
#include "actionstatemachine.h"
#include "perceptionbus.h"
#include "audiomanager.h"
#include "dragfilter.h"
#include "fistwidget.h"
#include "settingsdialog.h"
#include "playmate.h"
#include "profilemanager.h"
#include "hardwaremanager.h"
#include "effectmanager.h"
#include "gomokuwidget.h"
#include "statusmanager.h"
#include "statuspanel.h"
#include "deepseekchat.h"
#include "chatbubblewidget.h"
#include "foodmenuwidget.h"
#include "headpatwidget.h"
#include "starwidget.h"
#include <QPaintEvent>
#include <QPainter>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QTime>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <QInputDialog>
#include <QPropertyAnimation>
#include <QThread>
#include <cmath>
#include <algorithm>

// Convenience accessors — file-scoped inline functions replace former macros
// to eliminate name-collision risk and improve debuggability.
static inline auto  PM()  { return ProfileManager::instance(); }
static inline const AnimationProfile&   PA()  { return PM()->animation(); }
static inline const BehaviorProfile&    PB()  { return PM()->behavior(); }
static inline const WindowProfile&      PW()  { return PM()->window(); }
static inline const SpritesProfile&     PS()  { return PM()->sprites(); }
static inline const UIProfile&          PU()  { return PM()->ui(); }
static inline const AudioProfile&       PAU() { return PM()->audio(); }

// ==========================================
// Config convenience accessors
// ==========================================
const BehaviorConfig& Widget::cfg() const { return m_config->config(); }
BehaviorConfig& Widget::cfg() { return m_config->config(); }

// ==========================================
// Widget constructor
// ==========================================
Widget::Widget(QWidget *parent)
    : QWidget(parent),
    // Components
    m_config(new ConfigManager(QCoreApplication::applicationDirPath() + "/lumina_config.ini", this)),
    m_sprites(new SpriteResource(this)),
    m_hal(new PlatformHAL(this)),
    m_audio(new AudioManager(this)),
    m_hardware(nullptr),
    m_breathFx(nullptr),
    m_actionMachine(new ActionStateMachine(this)),
    m_perception(nullptr),
    m_perceptionThread(nullptr),
    // State
    cur_role_act(RoleAct::Stand),
    cur_action_id(Act::Id::Stand),
    show_character(true),
    is_form_switching(false),
    form_flash_opacity(0.0),
    current_light_size(0),
    show_light(false),
    current_opacity(1.0),
    current_time_text(QTime::currentTime().toString("HH:mm:ss")),
    show_time_overlay(false),
    bottom_hint_timer(new QTimer(this)),
    current_move_anim(nullptr),
    move_face_right(false),
    allow_sit_try(false),
    // Timers
    frame_timer(new QTimer(this)),
    clock_timer(new QTimer(this)),
    clock_display_timer(new QTimer(this)),
    click_reset_timer(new QTimer(this)),
    stand_shake_timer(new QTimer(this)),
    playful_duration_timer(new QTimer(this)),
    playmate_chase_timer(new QTimer(this)),
    auto_sing_timer(new QTimer(this)),
    // Menu / UI
    menu(new QMenu(this)),
    auto_sing_toggle_action(nullptr),
    // Status system
    m_statusMgr(new StatusManager(this)),
    m_statusPanel(nullptr),
    m_hoverTimer(new QTimer(this)),
    // Interaction
    click_count(0),
    playful_mode_active(false),
    is_stand_shaking(false),
    stand_shake_origin(QPoint()),
    stand_shake_remaining_steps(0),
    playmate_velocity(0.0, 0.0),
    playmate(nullptr),
    // Frame animation
    m_frameIndex(0),
    // Star interaction
    m_starPhysicsTimer(new QTimer(this)),
    m_autoThrowStarTimer(new QTimer(this)),
    // Gomoku
    m_gomokuWidget(nullptr),
    m_gomokuMode(false),
    m_gomokuSuspended(false),
    m_gomokuFlyAnim(nullptr)
{
    this->setFixedSize(PW().widget_size, PW().widget_size);
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->installEventFilter(new DragFilter(this));
    this->setAttribute(Qt::WA_Hover, true);

    // -- Status system hover timer --
    m_hoverTimer->setSingleShot(true);
    connect(m_hoverTimer, &QTimer::timeout, this, [this]() {
        if (!cfg().status_panel_enabled) return;
        if (!m_statusPanel) {
            m_statusPanel = new StatusPanel(m_statusMgr, nullptr);
        }
        QScreen *screen = QGuiApplication::primaryScreen();
        if (!screen) return;
        const QRect geo = geometry();
        m_statusPanel->showAt(QPoint(geo.right(), geo.top()), screen->availableGeometry().size());
        m_statusPanel->fadeIn();
    });

    // -- Live-refresh StatusPanel on any attribute change (food, tick, angry, etc.) --
    connect(m_statusMgr, &StatusManager::statusChanged, this, [this]() {
        if (m_statusPanel && m_statusPanel->isVisible())
            m_statusPanel->update();
    });

    // -- Auto-feed policy: Widget decides what to do when satiety is critical --
    connect(m_statusMgr, &StatusManager::satietyCritical, this, [this]() {
        m_statusMgr->addValue(StatusManager::Satiety, 5);  // "流光" auto-feed
    });

    // -- Breath effect (GPU halo via offscreen FBO) --
    const auto &bfxCfg = PM()->breathEffect();
    if (bfxCfg.enabled) {
        m_hardware = new HardwareManager(bfxCfg.poll_interval_ms, this);
        m_breathFx = new EffectManager(this);
        connect(m_hardware, &HardwareManager::cpuUsageUpdated,
                m_breathFx, &EffectManager::setCpuUsage);
        connect(m_breathFx, &EffectManager::frameReady,
                this, QOverload<>::of(&QWidget::update));
    }

    // -- Frame animation timer --
    connect(frame_timer, &QTimer::timeout, [this](){
        auto paths = m_sprites->actionPaths(cur_action_id, cfg().character_form);
        if (paths.isEmpty()) return;
        this->cur_role_pix = SpriteResource::resolveFramePath(
            paths, m_sprites->isDirectional(cur_action_id), move_face_right, m_frameIndex++);
        this->update();
    });

    // -- Clock timer --
    connect(clock_timer, &QTimer::timeout, this, [this]() {
        current_time_text = QTime::currentTime().toString("HH:mm:ss");
        if (show_time_overlay) update();
    });
    clock_timer->start(1000);

    clock_display_timer->setSingleShot(true);
    connect(clock_display_timer, &QTimer::timeout, this, [this]() {
        show_time_overlay = false;
        update();
    });

    // -- Bottom hint timer --
    bottom_hint_timer->setSingleShot(true);
    connect(bottom_hint_timer, &QTimer::timeout, this, [this]() {
        bottom_hint_transient_pix.clear();
        update();
    });

    // -- Auto-sing timer --
    auto_sing_timer->setSingleShot(true);
    connect(auto_sing_timer, &QTimer::timeout, this, [this]() {
        if (cfg().auto_sing_enabled && cur_role_act != RoleAct::Sleeping) {
            m_audio->playVoice(PAU().humming);
        }
        if (cfg().auto_sing_enabled) {
            scheduleNextHumming();
        }
    });

    // -- Playful duration timer --
    playful_duration_timer->setSingleShot(true);
    connect(playful_duration_timer, &QTimer::timeout, this, [this]() {
        if (playful_mode_active) endPlayfulMode(true);
    });

    // -- Playmate chase timer --
    connect(playmate_chase_timer, &QTimer::timeout, this, &Widget::updatePlaymateChase);

    // (Idle / sleep / sit-detection / playful-detection timers are armed by
    //  the data-driven ActionStateMachine — see setupActionMachine())

    // -- Click reset timer --
    click_reset_timer->setSingleShot(true);
    connect(click_reset_timer, &QTimer::timeout, [this](){
        click_count = 0;
    });

    // -- Stand shake timer --
    stand_shake_timer->setSingleShot(false);
    connect(stand_shake_timer, &QTimer::timeout, this, [this]() {
        if (!is_stand_shaking || cur_role_act != RoleAct::Stand) {
            stand_shake_timer->stop();
            move(stand_shake_origin);
            is_stand_shaking = false;
            stand_shake_remaining_steps = 0;
            return;
        }
        if (stand_shake_remaining_steps <= 0) {
            stand_shake_timer->stop();
            move(stand_shake_origin);
            is_stand_shaking = false;
            return;
        }
        const int shakeOff = PB().stand_shake_offset_px;
        const int dx = QRandomGenerator::global()->bounded(-shakeOff, shakeOff + 1);
        const int dy = QRandomGenerator::global()->bounded(-shakeOff, shakeOff + 1);
        move(stand_shake_origin + QPoint(dx, dy));
        --stand_shake_remaining_steps;
        if (stand_shake_remaining_steps <= 0) {
            stand_shake_timer->stop();
            move(stand_shake_origin);
            is_stand_shaking = false;
        }
    });

    // -- Star physics timer --
    connect(m_starPhysicsTimer, &QTimer::timeout, this, &Widget::updateStarPhysics);

    // -- Auto-throw star timer --
    connect(m_autoThrowStarTimer, &QTimer::timeout, this, [this]() {
        if (m_stars.size() < cfg().max_star_count) {
            releaseStar();
        }
    });

    // -- Initialization sequence --
    setupActionMachine();
    initMenu();
    m_config->load();
    m_statusMgr->setStatsVariable(cfg().stats_variable);
    m_statusMgr->setTickIntervalMs(cfg().stats_tick_interval_ms);

    if (auto_sing_toggle_action) {
        auto_sing_toggle_action->setText(cfg().auto_sing_enabled ? "我想安静一点" : "我想听听你的声音");
    }
    m_sprites->loadAll();
    light_pix.load(PS().light_symbol);

    if (m_breathFx && !light_pix.isNull()) {
        m_breathFx->setHaloTexture(light_pix);
    }

    const QPoint savedPos = m_config->windowPos();
    if (!savedPos.isNull()) {
        QWidget::move(savedPos);
    }

    // -- v4 perception bus (environment awareness, worker thread) --
    if (PM()->perception().enabled) {
        startPerception();
    }

    showActAnimation(RoleAct::Stand);
    if (cfg().auto_sing_enabled) {
        scheduleNextHumming();
    }
    if (cfg().auto_throw_star) {
        m_autoThrowStarTimer->start(cfg().auto_throw_star_interval_ms);
    }
    playLightAnimation();
    resetIdleTimer();
}

Widget::~Widget() {
    // Stop the perception worker thread before members are torn down
    stopPerception();
    // Clean up StatusPanel (parentless top-level widget)
    if (m_statusPanel) {
        m_statusPanel->close();
        delete m_statusPanel;
        m_statusPanel = nullptr;
    }
    // H-2: Clean up GomokuWidget if game is still active
    if (m_gomokuWidget) {
        m_gomokuWidget->close();
        delete m_gomokuWidget;
        m_gomokuWidget = nullptr;
    }
    // Clean up StarWidgets (parentless top-level widgets)
    m_starPhysicsTimer->stop();
    m_autoThrowStarTimer->stop();
    qDeleteAll(m_stars);
    m_stars.clear();
    // M-7: Clean up HeadPatWidget (parentless top-level)
    if (m_headPatWidget)
        delete m_headPatWidget;  // QPointer auto-nulls
    endPlayfulMode(false);
    m_config->setWindowPos(pos());
    m_config->save();
}

// ==========================================
// Behavior: idle / sleep / humming
// ==========================================

void Widget::resetIdleTimer() {
    // Notify attribute system of user interaction (resets interest idle clock)
    m_statusMgr->notifyMouseInteraction();

    if (cur_action_id == Act::Id::Sleeping) {
        enterAction(Act::Id::Stand);
        return;
    }
    // Restart phase 1 of every rearm_on_interaction rule of the active state
    // (legacy: sleep/sit timers in Move, idle timer in Stand).
    allow_sit_try = false;
    m_actionMachine->rearm();
}

void Widget::scheduleNextHumming() {
    const int nextMs = QRandomGenerator::global()->bounded(PB().humming_min_interval_ms, PB().humming_max_interval_ms + 1);
    auto_sing_timer->start(nextMs);
}

void Widget::setAutoSingingEnabled(bool enabled) {
    cfg().auto_sing_enabled = enabled;

    if (auto_sing_toggle_action) {
        auto_sing_toggle_action->setText(cfg().auto_sing_enabled ? "我想安静一点" : "我想听听你的声音");
    }
    showBottomHintTransient(cfg().auto_sing_enabled ? PS().hint_text_can_sing : PS().hint_text_ok,
                            cfg().hint_display_duration_ms);
    if (cfg().auto_sing_enabled) {
        scheduleNextHumming();
    } else {
        auto_sing_timer->stop();
    }
    m_config->save();
}

// ==========================================
// Generic property animation helper (M-2)
// ==========================================

void Widget::animateValue(QVariantAnimation*& slot, double& prop,
                          double target, int durationMs, QEasingCurve::Type curve)
{
    if (slot) {
        slot->stop();
        slot->deleteLater();
    }
    double *propPtr = &prop;
    QVariantAnimation **slotPtr = &slot;
    auto *a = new QVariantAnimation(this);
    slot = a;
    a->setStartValue(prop);
    a->setEndValue(target);
    a->setDuration(durationMs);
    a->setEasingCurve(curve);
    connect(a, &QVariantAnimation::valueChanged, this, [this, propPtr](const QVariant &v) {
        *propPtr = v.toDouble();
        update();
    });
    connect(a, &QVariantAnimation::finished, this, [slotPtr]() {
        *slotPtr = nullptr;
    });
    a->start();
}

// ==========================================
// Interaction: click shake / angry
// ==========================================

void Widget::triggerStandClickShake() {
    if (cur_role_act != RoleAct::Stand || is_stand_shaking) return;
    is_stand_shaking = true;
    stand_shake_origin = pos();
    stand_shake_remaining_steps = QRandomGenerator::global()->bounded(PB().stand_shake_min_jumps, PB().stand_shake_max_jumps + 1);
    const int intervalMs = std::max(1, PB().stand_shake_duration_ms / stand_shake_remaining_steps);
    stand_shake_timer->start(intervalMs);
}

void Widget::onPrimaryLeftClick() {
    if (playful_mode_active) {
        endPlayfulMode(true);
    }
}

void Widget::addClickCount() {
    if (cur_role_act == RoleAct::Angry) return;
    click_count++;
    click_reset_timer->start(PB().click_reset_timeout_ms);
    if (click_count >= cfg().angry_click_threshold) {
        triggerAngryAttack();
        click_count = 0;
        click_reset_timer->stop();
    }
}

void Widget::triggerAngryAttack() {
    // H-5: Suspend gomoku — cancel in-flight fly animation, lock placement signals
    if (m_gomokuMode) {
        m_gomokuSuspended = true;
        if (m_gomokuFlyAnim) {
            m_gomokuFlyAnim->stop();
            m_gomokuFlyAnim->deleteLater();
            m_gomokuFlyAnim = nullptr;
        }
    }

    showActAnimation(RoleAct::Angry);
    m_audio->playVoice(PAU().angry);

    // H-1: Null-safe screen access
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QRect screenRect = screen->geometry();

    const int spawnOff = PB().angry_spawn_offset_px;
    QPoint spawnLeftBottom(spawnOff, screenRect.height() - spawnOff);
    QPoint spawnRightTop(screenRect.width() - spawnOff, spawnOff);

    new FistWidget(spawnLeftBottom, true, this);
    new FistWidget(spawnRightTop, false, this);

    // Recovery is owned by the topology's angry_recover rule
    // (after_ms_key: angry_duration_ms — see setupActionMachine()).
}

// ==========================================
// Playful mode
// ==========================================

void Widget::startPlayfulMode() {
    if (playful_mode_active || cur_role_act != RoleAct::Move) return;

    const QList<QString> movePaths = m_sprites->alternateMovePaths(cfg().character_form);
    if (movePaths.isEmpty()) return;

    if (playmate) {
        playmate->deleteLater();
        playmate = nullptr;
    }

    playmate = new Playmate(movePaths);
    playmate_velocity = QPointF(0.0, 0.0);

    const int spawnOffsetX = move_face_right ? -cfg().playmate_min_spacing_px : cfg().playmate_min_spacing_px;
    const QPoint spawnPos = PlatformHAL::clampToScreen(pos() + QPoint(spawnOffsetX, 0), playmate->size());
    playmate->move(spawnPos);
    playmate->setFacingRight(!move_face_right);
    playmate->show();
    playmate->playEntryAnimation();

    playful_mode_active = true;
    playful_duration_timer->start(cfg().playful_mode_duration_ms);
    playmate_chase_timer->start(PB().playmate_chase_interval_ms);
}

void Widget::endPlayfulMode(bool playmateExitAnimation) {
    playmate_chase_timer->stop();
    playful_duration_timer->stop();
    playful_mode_active = false;
    playmate_velocity = QPointF(0.0, 0.0);

    // (Re-entry detection is owned by the machine's periodic
    //  playful_detection rule, which keeps running while in Move.)

    if (!playmate) return;

    if (playmateExitAnimation) {
        playmate->playExitAnimation();
    } else {
        playmate->close();
        playmate->deleteLater();
    }
    playmate = nullptr;
}

void Widget::updatePlaymateChase() {
    if (!playful_mode_active || !playmate) return;
    if (cur_role_act != RoleAct::Move) {
        endPlayfulMode(true);
        return;
    }

    const double dt = PB().playmate_chase_interval_ms / 1000.0;

    const QPoint clampedMainPos = PlatformHAL::clampToScreen(pos(), size());
    if (clampedMainPos != pos()) move(clampedMainPos);

    QPoint playmatePos = PlatformHAL::clampToScreen(playmate->pos(), playmate->size());
    if (playmatePos != playmate->pos()) playmate->move(playmatePos);

    const QPointF toMain = QPointF(clampedMainPos - playmatePos);
    double distToMain = std::sqrt(toMain.x() * toMain.x() + toMain.y() * toMain.y());

    QPointF dirToMain(0.0, 0.0);
    if (distToMain > 1e-3) {
        dirToMain = QPointF(toMain.x() / distToMain, toMain.y() / distToMain);
    } else {
        dirToMain = QPointF(move_face_right ? -1.0 : 1.0, 0.0);
        distToMain = 1.0;
    }

    const QPointF desiredPoint = QPointF(clampedMainPos) - dirToMain * static_cast<double>(cfg().playmate_min_spacing_px);
    const QPointF toDesired = desiredPoint - QPointF(playmatePos);
    const double desiredDist = std::sqrt(toDesired.x() * toDesired.x() + toDesired.y() * toDesired.y());
    QPointF desiredDir(0.0, 0.0);
    if (desiredDist > 1e-3) {
        desiredDir = QPointF(toDesired.x() / desiredDist, toDesired.y() / desiredDist);
    }

    const double maxSpeed = cfg().move_speed_px_per_sec * cfg().playmate_speed_scale;
    const QPointF desiredVelocity = desiredDir * maxSpeed;
    const double blend = std::min(1.0, cfg().playmate_accel_scale * dt);
    playmate_velocity += (desiredVelocity - playmate_velocity) * blend;

    QPoint nextPos = PlatformHAL::clampToScreen(
        (QPointF(playmatePos) + playmate_velocity * dt).toPoint(), playmate->size());

    QPointF spacingDelta = QPointF(clampedMainPos - nextPos);
    double spacing = std::sqrt(spacingDelta.x() * spacingDelta.x() + spacingDelta.y() * spacingDelta.y());
    if (spacing < cfg().playmate_min_spacing_px) {
        if (spacing < 1e-3) {
            spacingDelta = QPointF(move_face_right ? -1.0 : 1.0, 0.0);
            spacing = 1.0;
        }
        const QPointF spacingDir(spacingDelta.x() / spacing, spacingDelta.y() / spacing);
        nextPos = PlatformHAL::clampToScreen(
            (QPointF(clampedMainPos) - spacingDir * static_cast<double>(cfg().playmate_min_spacing_px)).toPoint(),
            playmate->size());
    }

    if (playmate_velocity.x() > 0.5) {
        playmate->setFacingRight(true);
    } else if (playmate_velocity.x() < -0.5) {
        playmate->setFacingRight(false);
    }

    playmate->move(nextPos);
}

// ==========================================
// Bottom hint
// ==========================================

void Widget::showBottomHintTransient(const QString &pixPath, int durationMs) {
    bottom_hint_transient_pix = pixPath;
    if (!pixPath.isEmpty() && durationMs > 0) {
        bottom_hint_timer->start(durationMs);
    } else {
        bottom_hint_timer->stop();
    }
    update();
}

// ==========================================
// Movement: random walk
// ==========================================

void Widget::startRandomWalk() {
    if (cur_role_act == RoleAct::Sleeping || cur_role_act != RoleAct::Move) return;

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    const QRect screenGeom = screen->availableGeometry();
    const int minX = screenGeom.x();
    const int minY = screenGeom.y();
    const int maxX = std::max(minX, screenGeom.x() + screenGeom.width()  - this->width());
    const int maxY = std::max(minY, screenGeom.y() + screenGeom.height() - this->height());

    // 【核心修复 2】边缘抖动修复: 起始点强制 clamp 到屏幕边界内
    QPoint startPos = PlatformHAL::clampToScreen(this->pos(), this->size());
    if (startPos != this->pos()) this->move(startPos);

    const int targetX = QRandomGenerator::global()->bounded(minX, maxX + 1);
    const int targetY = QRandomGenerator::global()->bounded(minY, maxY + 1);
    QPoint endPos(targetX, targetY);

    // 【核心修复 2 续】wave amplitude margin 感知
    const int minMargin = std::min({
        startPos.x() - minX, maxX - startPos.x(),
        startPos.y() - minY, maxY - startPos.y(),
        endPos.x()   - minX, maxX - endPos.x(),
        endPos.y()   - minY, maxY - endPos.y()
    });

    int dx = endPos.x() - startPos.x();
    int dy = endPos.y() - startPos.y();
    if (dx != 0) move_face_right = dx > 0;
    double distance = std::sqrt(dx*dx + dy*dy);

    int duration = static_cast<int>((distance / cfg().move_speed_px_per_sec) * 1000.0);
    duration = std::max(PB().move_min_duration_ms, std::min(duration, PB().move_max_duration_ms));

    QVariantAnimation *anim = new QVariantAnimation(this);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setDuration(duration);
    anim->setEasingCurve(QEasingCurve::InOutSine);

    this->current_move_anim = anim;
    const bool allowWave = (distance > 50.0 && minMargin > 24);
    const double waveAmplitude = allowWave
        ? std::min({36.0, std::max(20.0, distance / 12.0), static_cast<double>(minMargin - 2)})
        : 0.0;
    const int waveCount = allowWave ? std::max(1, static_cast<int>(distance / 300.0)) : 1;

    connect(anim, &QVariantAnimation::valueChanged, this,
            [this, startPos, endPos, allowWave, waveAmplitude, waveCount](const QVariant &val){
        double t = val.toDouble();
        double linearX = startPos.x() + (endPos.x() - startPos.x()) * t;
        double linearY = startPos.y() + (endPos.y() - startPos.y()) * t;

        if (allowWave) {
            double ddx = endPos.x() - startPos.x();
            double ddy = endPos.y() - startPos.y();
            double len = std::sqrt(ddx*ddx + ddy*ddy);
            double nx = -ddy / len;
            double ny =  ddx / len;
            double wave = waveAmplitude * std::sin(t * waveCount * 2 * 3.1415926);
            linearX += nx * wave;
            linearY += ny * wave;
        }

        // 【核心修复 2 续】每帧输出位置 clamp
        this->move(PlatformHAL::clampToScreen(
            QPoint(static_cast<int>(linearX), static_cast<int>(linearY)), this->size()));
    });

    connect(anim, &QVariantAnimation::finished, this, [this, anim](){
        if (this->current_move_anim == anim) {
            this->current_move_anim = nullptr;
        }
        anim->deleteLater();

        // 决策: 继续走还是去坐窗口
        if (this->cur_role_act == RoleAct::Move) {
            bool tryToSit = allow_sit_try;
            if (tryToSit) {
                allow_sit_try = false;
                findAndSitOnWindow();
            }
            if (this->cur_role_act == RoleAct::Move) {
                QTimer::singleShot(PB().walk_restart_delay_ms, this, &Widget::startRandomWalk);
            }
        }
    });

    anim->start();
}

void Widget::stopWalking() {
    if (current_move_anim) {
        current_move_anim->stop();
        current_move_anim->deleteLater();
        current_move_anim = nullptr;
    }

    if (cur_role_act == RoleAct::Angry) return;
    if (cur_role_act != RoleAct::Stand) {
        showActAnimation(RoleAct::Stand);
    }
}

// ==========================================
// Window sitting (via PlatformHAL)
// ==========================================

void Widget::findAndSitOnWindow() {
    SittableWindow win = m_hal->findSittableWindow(this->winId());
    if (!win.valid) return;

    m_hal->setTargetWindow(win);

    int targetY = win.top - 500;
    int maxX = win.width - this->width();
    int safeRange = std::max(1, maxX - 300);
    int targetX = win.x + QRandomGenerator::global()->bounded(0, safeRange);

    QPropertyAnimation *flyAnim = new QPropertyAnimation(this, "pos");
    flyAnim->setDuration(PB().sit_fly_duration_ms);
    flyAnim->setStartValue(this->pos());
    flyAnim->setEndValue(PlatformHAL::clampToScreen(QPoint(targetX, targetY), this->size()));
    flyAnim->setEasingCurve(QEasingCurve::InOutQuad);

    frame_timer->stop();
    cur_action_id = QRandomGenerator::global()->bounded(2) == 0
                        ? Act::Id::Sitting_1 : Act::Id::Sitting_2;
    cur_role_act = roleActFromId(cur_action_id);

    if (current_move_anim) {
        current_move_anim->stop();
        current_move_anim->deleteLater();
        current_move_anim = nullptr;
    }
    // Transient fly phase: no state rules may fire until touchdown
    m_actionMachine->stopAll();

    connect(flyAnim, &QPropertyAnimation::finished, this, [this, flyAnim](){
        // Touchdown: enter the sitting state properly — sprite plus the
        // topology's sit_monitor / sit_timeout rules.
        enterAction(cur_action_id);
        flyAnim->deleteLater();
    });

    flyAnim->start();
}

// ==========================================
// Light / exit / form-switch animations
// ==========================================

void Widget::playLightAnimation() {
    show_light = true;
    current_light_size = PA().light_intro_start;
    current_opacity = 1.0;

    QVariantAnimation *animSize = new QVariantAnimation(this);
    animSize->setStartValue(PA().light_intro_start);
    animSize->setEndValue(PA().light_intro_end);
    animSize->setDuration(PA().light_intro_size_duration_ms);
    animSize->setEasingCurve(QEasingCurve::OutQuad);
    connect(animSize, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        this->current_light_size = val.toInt();
        this->update();
    });

    QVariantAnimation *animFade = new QVariantAnimation(this);
    animFade->setStartValue(PA().light_intro_fade_start_opacity);
    animFade->setEndValue(0.0);
    animFade->setDuration(PA().light_intro_fade_duration_ms);
    animFade->setEasingCurve(QEasingCurve::Linear);
    connect(animFade, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        this->current_opacity = val.toDouble();
        this->update();
    });

    connect(animSize, &QVariantAnimation::finished, this, [this, animFade](){
        animFade->start();
        sender()->deleteLater();
    });
    connect(animFade, &QVariantAnimation::finished, this, [this](){
        this->show_light = false;
        updateBreathEffectVisibility();
        sender()->deleteLater();
    });
    updateBreathEffectVisibility();
    animSize->start();
}

void Widget::playExitAnimation() {
    showActAnimation(RoleAct::Stand);
    show_light = true;
    show_character = true;
    updateBreathEffectVisibility();
    current_light_size = PA().exit_light_start;
    current_opacity = 0.0;

    QVariantAnimation *animFadeIn = new QVariantAnimation(this);
    animFadeIn->setStartValue(0.0);
    animFadeIn->setEndValue(1.0);
    animFadeIn->setDuration(PA().exit_fade_in_duration_ms);
    animFadeIn->setEasingCurve(QEasingCurve::InQuad);
    connect(animFadeIn, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        this->current_opacity = val.toDouble();
        this->update();
    });

    QVariantAnimation *animShrink = new QVariantAnimation(this);
    animShrink->setStartValue(PA().exit_light_start);
    animShrink->setEndValue(0);
    animShrink->setDuration(PA().exit_shrink_duration_ms);
    animShrink->setEasingCurve(QEasingCurve::InBack);
    connect(animShrink, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        this->current_light_size = val.toInt();
        if (this->current_light_size < PA().exit_hide_character_threshold) {
            this->show_character = false;
        }
        this->update();
    });

    connect(animFadeIn, &QVariantAnimation::finished, this, [this, animShrink](){
        animShrink->start();
        sender()->deleteLater();
    });
    connect(animShrink, &QVariantAnimation::finished, this, [this](){
        updateBreathEffectVisibility();
        this->setVisible(false);
        sender()->deleteLater();
    });
    animFadeIn->start();
}

void Widget::showFromTray() {
    this->show_character = true;
    this->show_light = false;
    this->setVisible(true);
    resetIdleTimer();
    playLightAnimation();
}

// ==========================================
// State machine: enterAction (v4 data-driven)
// ==========================================

void Widget::showActAnimation(RoleAct k) {
    enterAction(actionIdFor(k));
}

void Widget::enterAction(const QString &actionId) {
    // Action lock: during Gomoku, only Stand and Angry are allowed
    if (m_gomokuMode && actionId != Act::Id::Stand && actionId != Act::Id::Angry) return;

    const QString previousId = cur_action_id;
    frame_timer->stop();
    cur_action_id = actionId;
    cur_role_act  = roleActFromId(actionId, RoleAct::Stand);

    // Notify attribute system of state change
    m_statusMgr->setCurrentAct(cur_role_act);
    if (actionId == Act::Id::Angry && previousId != Act::Id::Angry) {
        m_statusMgr->notifyAngry();
    }

    // First frame of the new action (directional actions resolve by facing)
    auto paths = m_sprites->actionPaths(actionId, cfg().character_form);
    if (!paths.isEmpty()) {
        this->cur_role_pix = SpriteResource::resolveFramePath(
            paths, m_sprites->isDirectional(actionId), move_face_right, 0);
        this->update();
    }

    // Data-driven topology: disarms the previous state's rules, runs
    // on_enter behaviors, then arms this state's transition rules.
    // States without a spec (or custom JSON states without rules) simply
    // idle — the old D-1 stop-all-first guarantee now falls out of the
    // machine's disarm-on-enter contract.
    allow_sit_try = false;
    m_actionMachine->enterState(actionId);

    // Per-state presentation from the spec (enter hint + frame rate)
    const ActionStateSpec *spec = m_actionMachine->spec(actionId);
    if (spec && !spec->enter_hint.isEmpty() && previousId != actionId) {
        const QString hintPix = PS().hintPath(spec->enter_hint);
        if (!hintPix.isEmpty())
            showBottomHintTransient(hintPix, cfg().hint_display_duration_ms);
    }

    m_frameIndex = 0;
    const int frameMs = (spec && spec->frame_interval_ms > 0)
                            ? spec->frame_interval_ms
                            : PA().frame_interval_ms;
    frame_timer->start(frameMs);
}

// ==========================================
// v4: machine wiring — duration keys + named behaviors
// ==========================================

void Widget::setupActionMachine() {
    m_actionMachine->setTopology(&PM()->actionTopology());

    // Duration/chance key resolver: user-tunable config first (lumina_config.ini),
    // then profile constants (character.json "behavior").
    m_actionMachine->setDurationResolver([this](const QString &key) -> int {
        const BehaviorConfig &c = cfg();
        if (key == QLatin1String("stand_to_move_wait_ms"))          return c.stand_to_move_wait_ms;
        if (key == QLatin1String("move_to_sleep_wait_ms"))          return c.move_to_sleep_wait_ms;
        if (key == QLatin1String("move_to_sit_wait_ms"))            return c.move_to_sit_wait_ms;
        if (key == QLatin1String("move_to_playful_wait_ms"))        return c.move_to_playful_wait_ms;
        if (key == QLatin1String("sit_detection_interval_ms"))      return c.sit_detection_interval_ms;
        if (key == QLatin1String("sit_trigger_chance_percent"))     return c.sit_trigger_chance_percent;
        if (key == QLatin1String("sit_mode_duration_ms"))           return c.sit_mode_duration_ms;
        if (key == QLatin1String("playful_detection_interval_ms"))  return c.playful_detection_interval_ms;
        if (key == QLatin1String("playful_trigger_chance_percent")) return c.playful_trigger_chance_percent;
        if (key == QLatin1String("playful_mode_duration_ms"))       return c.playful_mode_duration_ms;
        if (key == QLatin1String("hint_display_duration_ms"))       return c.hint_display_duration_ms;
        const BehaviorProfile &b = PB();
        if (key == QLatin1String("sit_monitor_interval_ms"))        return b.sit_monitor_interval_ms;
        if (key == QLatin1String("angry_duration_ms"))              return b.angry_duration_ms;
        if (key == QLatin1String("humming_min_interval_ms"))        return b.humming_min_interval_ms;
        if (key == QLatin1String("humming_max_interval_ms"))        return b.humming_max_interval_ms;
        if (key == QLatin1String("walk_restart_delay_ms"))          return b.walk_restart_delay_ms;
        if (key == QLatin1String("clock_display_duration_ms"))      return b.clock_display_duration_ms;
        return -1;
    });

    // ── Named behaviors, referencable from character.json rules ──
    m_actionMachine->registerBehavior(QStringLiteral("end_playful"), [this]() {
        endPlayfulMode(true);
        return true;
    });
    m_actionMachine->registerBehavior(QStringLiteral("halt_move"), [this]() {
        if (current_move_anim) {
            current_move_anim->stop();
            current_move_anim->deleteLater();
            current_move_anim = nullptr;
        }
        return true;
    });
    m_actionMachine->registerBehavior(QStringLiteral("random_walk"), [this]() {
        startRandomWalk();
        return true;
    });
    m_actionMachine->registerBehavior(QStringLiteral("allow_sit"), [this]() {
        allow_sit_try = true;
        return true;
    });
    m_actionMachine->registerBehavior(QStringLiteral("start_playful"), [this]() {
        if (!playful_mode_active) startPlayfulMode();
        return true;
    });
    m_actionMachine->registerBehavior(QStringLiteral("sit_monitor_check"), [this]() {
        if (!m_hal->isTargetWindowValid() || m_hal->hasTargetWindowMoved()) {
            showActAnimation(RoleAct::Move);
            startRandomWalk();
        }
        return true;
    });
    m_actionMachine->registerBehavior(QStringLiteral("angry_recover"), [this]() {
        // H-5: Resume gomoku if it was suspended, otherwise normal idle
        if (m_gomokuMode && m_gomokuSuspended) {
            resumeGomokuFromAngry();
        } else {
            showActAnimation(RoleAct::Stand);
            resetIdleTimer();
        }
        return true;
    });

    connect(m_actionMachine, &ActionStateMachine::transitionRequested,
            this, &Widget::onMachineTransition);
}

void Widget::onMachineTransition(const QString &to, const QStringList &postBehaviors) {
    // Gomoku action lock mirrors enterAction: a blocked transition must also
    // skip its post-behaviors (prevents stray walks during a match).
    if (m_gomokuMode && to != Act::Id::Stand && to != Act::Id::Angry) return;
    enterAction(to);
    m_actionMachine->runBehaviorList(postBehaviors);
}

// ==========================================
// v4: perception bus lifecycle (worker thread)
// ==========================================

void Widget::startPerception() {
    const int pollMs = PM()->perception().foreground_poll_ms;

    if (m_perception) {
        // Already running: retune the poll interval on the bus thread
        PerceptionBus *bus = m_perception;
        QMetaObject::invokeMethod(bus, [bus, pollMs]() { bus->start(pollMs); },
                                  Qt::QueuedConnection);
        return;
    }

    m_perceptionThread = new QThread(this);
    m_perceptionThread->setObjectName(QStringLiteral("PerceptionBus"));
    m_perception = new PerceptionBus();   // parentless: owned by thread teardown
    m_perception->moveToThread(m_perceptionThread);
    connect(m_perceptionThread, &QThread::finished,
            m_perception, &QObject::deleteLater);

    // Auto-queued: delivered on the GUI thread
    connect(m_perception, &PerceptionBus::foregroundWindowChanged,
            this, [this](const QString &title) { m_perceivedWindowTitle = title; });

    m_perceptionThread->start();
    PerceptionBus *bus = m_perception;
    QMetaObject::invokeMethod(bus, [bus, pollMs]() { bus->start(pollMs); },
                              Qt::QueuedConnection);
}

void Widget::stopPerception() {
    if (!m_perceptionThread) return;
    m_perceptionThread->quit();
    m_perceptionThread->wait(2000);
    m_perceptionThread->deleteLater();
    m_perceptionThread = nullptr;
    m_perception = nullptr;   // deleted via QThread::finished -> deleteLater
    m_perceivedWindowTitle.clear();
}

void Widget::applyCurrentForm() {
    auto paths = m_sprites->actionPaths(cur_action_id, cfg().character_form);
    if (!paths.isEmpty()) {
        cur_role_pix = SpriteResource::resolveFramePath(
            paths, m_sprites->isDirectional(cur_action_id), move_face_right, 0);
        update();
    }
}

void Widget::toggleStandFormWithHalo() {
    if (is_form_switching) return;

    is_form_switching = true;
    show_light = true;
    updateBreathEffectVisibility();
    current_light_size = PA().form_switch_halo_start;
    current_opacity = 1.0;

    QVariantAnimation *expandAnim = new QVariantAnimation(this);
    expandAnim->setStartValue(PA().form_switch_halo_start);
    expandAnim->setEndValue(PA().form_switch_halo_end);
    expandAnim->setDuration(PA().form_switch_expand_duration_ms);
    expandAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(expandAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
        this->current_light_size = val.toInt();
        this->update();
    });

    connect(expandAnim, &QVariantAnimation::finished, this, [this, expandAnim](){
        form_flash_opacity = PA().form_switch_flash_opacity;

        QVariantAnimation *flashAnim = new QVariantAnimation(this);
        flashAnim->setStartValue(PA().form_switch_flash_opacity);
        flashAnim->setEndValue(0.0);
        flashAnim->setDuration(PA().form_switch_flash_duration_ms);
        flashAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(flashAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
            form_flash_opacity = val.toDouble();
            this->update();
        });
        connect(flashAnim, &QVariantAnimation::finished, this, [this, flashAnim](){
            form_flash_opacity = 0.0;
            this->update();
            flashAnim->deleteLater();
        });
        flashAnim->start();

        // Cycle to next form (generic for N forms)
        {
            const auto &forms = PS().forms;
            auto it = forms.constFind(cfg().character_form);
            if (it != forms.constEnd()) {
                ++it;
                if (it == forms.constEnd())
                    it = forms.constBegin();
            }
            cfg().character_form = it.key();
        }
        applyCurrentForm();

        QVariantAnimation *shrinkAnim = new QVariantAnimation(this);
        shrinkAnim->setStartValue(this->current_light_size);
        shrinkAnim->setEndValue(0);
        shrinkAnim->setDuration(PA().form_switch_shrink_duration_ms);
        shrinkAnim->setEasingCurve(QEasingCurve::InCubic);
        connect(shrinkAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val){
            this->current_light_size = val.toInt();
            this->update();
        });
        connect(shrinkAnim, &QVariantAnimation::finished, this, [this, shrinkAnim](){
            this->show_light = false;
            this->current_light_size = 0;
            this->current_opacity = 1.0;
            this->is_form_switching = false;
            updateBreathEffectVisibility();
            this->update();
            shrinkAnim->deleteLater();
        });

        shrinkAnim->start();
        expandAnim->deleteLater();
    });

    expandAnim->start();
}

// ==========================================
// Menu triggered handler
// ==========================================

void Widget::onMenuTriggered(QAction *action) {
    resetIdleTimer();
    if (action->data().isNull()) return;
    int val = action->data().toInt();
    if (val == 99) {
        playExitAnimation();
        return;
    }
    RoleAct selectedAct = static_cast<RoleAct>(val);
    showActAnimation(selectedAct);
    if (selectedAct == RoleAct::Move) {
        startRandomWalk();
    }
}

// ==========================================
// Pull-drag (hand-grabbing)
// ==========================================

// ── Adjustable hand-anchor offset (in the 200x200 display frame) ──
// These define the pixel position of the character's hand in the
// *normal* (facing-left) pull sprite, after scaling from 300x300 to 200x200.
// Increase hand_offset_x → character shifts LEFT  relative to cursor.
// Increase hand_offset_y → character shifts UP    relative to cursor.
// Fine-tune these two values until the cursor sits exactly on the hand.
//These are parameters obtained from multiple tests, please do not modify them casually.
static int hand_offset_x = 45;
static int hand_offset_y = 60;

void Widget::enterPullDrag(const QPoint &mouseGlobal) {
    m_pullDragging = true;
    m_pullFaceRight = false;
    m_lastPullMouseGlobal = mouseGlobal;

    // Stop any ongoing movement / idle behaviour
    stopWalking();

    // Position widget so hand is at cursor (forces initial repaint)
    updatePullDrag(mouseGlobal);
}

void Widget::updatePullDrag(const QPoint &mouseGlobal) {
    if (!m_pullDragging) return;

    // Determine facing direction from movement delta
    const bool prevFace = m_pullFaceRight;
    const int dx = mouseGlobal.x() - m_lastPullMouseGlobal.x();
    if (dx > 2)       m_pullFaceRight = true;
    else if (dx < -2) m_pullFaceRight = false;
    m_lastPullMouseGlobal = mouseGlobal;

    // Compute the hand position in widget-local coords
    const int displaySize = PS().role_display_size;
    const int cx = (width()  - displaySize) / 2;   // character draw origin X
    const int cy = (height() - displaySize) / 2;   // character draw origin Y

    int handLocalX, handLocalY;
    if (m_pullFaceRight) {
        // Mirrored: hand X flips within the display rect
        handLocalX = cx + (displaySize - hand_offset_x);
    } else {
        handLocalX = cx + hand_offset_x;
    }
    handLocalY = cy + hand_offset_y;

    // Move widget so that hand-in-widget aligns with the mouse cursor.
    // move() alone triggers a system-level window repaint.
    move(mouseGlobal - QPoint(handLocalX, handLocalY));

    // Only request an explicit repaint when the sprite actually changes
    // (direction flip). This avoids flooding the event loop with redundant
    // update() calls on every high-frequency mouse move.
    if (prevFace != m_pullFaceRight) {
        update();
    }
}

void Widget::exitPullDrag() {
    if (!m_pullDragging) return;
    m_pullDragging = false;

    // Snap back to Stand animation
    showActAnimation(RoleAct::Stand);
    update();
}

// ==========================================
// Paint
// ==========================================

void Widget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);

    // H-3: Micro-vibration as paint offset (safe with concurrent move animations)
    if (!m_vibrateOffset.isNull())
        painter.translate(m_vibrateOffset);

    int characterBottomY = height() / 2;
    bool hasCharacterRect = false;

    // GPU breath effect — draw BEHIND the character
    if (m_breathFx && m_breathFx->isEffectActive()) {
        const QImage &frame = m_breathFx->currentFrame();
        if (!frame.isNull()) {
            int lx = (width()  - frame.width())  / 2;
            int ly = (height() - frame.height()) / 2;
            painter.drawImage(lx, ly, frame);
        }
    }

    if (m_pullDragging) {
        // Draw the pull (hand-grabbing) sprite — direct pre-cached pixmap swap,
        // no per-frame mirroring or scaling.
        const QString basePath = PS().pullPath(cfg().character_form);
        const QString key = m_pullFaceRight
            ? SpriteResource::mirroredKey(basePath) : basePath;
        if (m_sprites->hasPixmap(key)) {
            const QPixmap &pix = m_sprites->pixmap(key);
            const int cx = (width()  - pix.width())  / 2;
            const int cy = (height() - pix.height()) / 2;
            painter.drawPixmap(cx, cy, pix);
            characterBottomY = cy + pix.height();
            hasCharacterRect = true;
        }
    } else if (show_character) {
        if (m_sprites->hasPixmap(cur_role_pix)) {
            const QPixmap &pix = m_sprites->pixmap(cur_role_pix);
            int cx = (this->width()  - pix.width())  / 2;
            int cy = (this->height() - pix.height()) / 2;

            if (m_squishFactor < 1.0) {
                // Squish: compress vertically, expand horizontally to conserve volume
                const double sx = 1.0 + (1.0 - m_squishFactor) * 0.3;
                const double sy = m_squishFactor;
                const double pcx = cx + pix.width() / 2.0;
                const double pcy = cy + pix.height();  // anchor at feet
                painter.save();
                painter.translate(pcx, pcy);
                painter.scale(sx, sy);
                painter.translate(-pcx, -pcy);
                painter.drawPixmap(cx, cy, pix);
                painter.restore();
            } else {
                painter.drawPixmap(cx, cy, pix);
            }
            // Energy pulse glow overlay during head-pat contact
            if (m_patGlowAlpha > 0.001) {
                painter.save();
                painter.setCompositionMode(QPainter::CompositionMode_Plus);
                QRadialGradient glow(cx + pix.width() / 2.0, cy + pix.height() * 0.3,
                                     pix.height() * 0.6);
                const int a = static_cast<int>(m_patGlowAlpha * 255);
                glow.setColorAt(0.0, QColor(220, 240, 255, a));
                glow.setColorAt(0.5, QColor(180, 220, 255, a / 2));
                glow.setColorAt(1.0, QColor(180, 220, 255, 0));
                painter.setBrush(glow);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF(cx + pix.width() / 2.0, cy + pix.height() * 0.3),
                                    pix.width() * 0.6, pix.height() * 0.5);
                painter.restore();
            }

            characterBottomY = cy + pix.height();
            hasCharacterRect = true;
        }
    }

    if (show_light && !light_pix.isNull()) {
        painter.save();
        painter.setOpacity(current_opacity);
        int lx = (this->width()  - current_light_size) / 2;
        int ly = (this->height() - current_light_size) / 2;
        painter.drawPixmap(QRect(lx, ly, current_light_size, current_light_size), light_pix);
        painter.restore();
    }

    if (form_flash_opacity > 0.001) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, static_cast<int>(255 * form_flash_opacity)));
        painter.drawRect(rect());
        painter.restore();
    }

    const QString hintPath = bottom_hint_transient_pix;
    if (!hintPath.isEmpty() && m_sprites->hasPixmap(hintPath)) {
        const QPixmap &hintPix = m_sprites->pixmap(hintPath);
        if (!hintPix.isNull()) {
            const int tx = (width() - hintPix.width()) / 2;
            int ty = hasCharacterRect ? (characterBottomY + 8) : ((height() - hintPix.height()) / 2);
            ty = std::min(ty, height() - hintPix.height() - 6);
            ty = std::max(0, ty);
            painter.drawPixmap(tx, ty, hintPix);
        }
    }

    if (show_time_overlay) {
        painter.save();

        QFont clockFont(PU().clock_font_family, PU().clock_font_size, QFont::Bold);
        painter.setFont(clockFont);
        QFontMetrics fm(clockFont);

        const int horizontalPadding = 28;
        const int verticalPadding = 20;
        const int textWidth = fm.horizontalAdvance(current_time_text);
        const int textHeight = fm.height();

        QRect panelRect(
            (width()  - (textWidth  + horizontalPadding * 2)) / 2,
            (height() - (textHeight + verticalPadding   * 2)) / 2,
            textWidth  + horizontalPadding * 2,
            textHeight + verticalPadding   * 2
            );

        painter.setPen(Qt::NoPen);
        painter.setBrush(PU().clock_panel_color);
        painter.drawRoundedRect(panelRect, PU().clock_panel_radius, PU().clock_panel_radius);

        const int textX = panelRect.center().x() - textWidth / 2;
        const int textY = panelRect.center().y() + fm.ascent() / 2 - 2;

        painter.setPen(PU().clock_shadow_color);
        painter.drawText(textX + 1, textY + 1, current_time_text);

        painter.setPen(PU().clock_text_color);
        painter.drawText(textX, textY, current_time_text);

        painter.restore();
    }
}

void Widget::contextMenuEvent(QContextMenuEvent *event) {
    Q_UNUSED(event)

    // Stop hover timer before blocking exec() — right-click is not hover
    m_hoverTimer->stop();

    if (m_gomokuMode) {
        QMenu gomokuQuitMenu(this);
        QAction *actQuit = gomokuQuitMenu.addAction("先到这里吧");
        connect(actQuit, &QAction::triggered, this, &Widget::endGomoku);
        gomokuQuitMenu.exec(QCursor::pos());
    } else {
        updateStarMenuState();
        this->menu->exec(QCursor::pos());
    }

    // After menu closes: if mouse is still over the widget, restart hover timer
    if (cfg().status_panel_enabled && rect().contains(mapFromGlobal(QCursor::pos()))) {
        const auto &ss = PM()->statusSystem();
        m_hoverTimer->start(ss.hover_delay_ms);
    }
}

void Widget::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
    if (cfg().status_panel_enabled) {
        const auto &ss = PM()->statusSystem();
        m_hoverTimer->start(ss.hover_delay_ms);
    }
}

void Widget::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    m_hoverTimer->stop();
    if (m_statusPanel && !m_statusPanel->isFadingOut()) {
        m_statusPanel->fadeOut();
    }
}

void Widget::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);

    // Safety reset: if the window loses activation mid-drag (PrtSc, Win+Shift+S,
    // Alt+Tab, etc.), the MouseButtonRelease is never delivered. Force-exit
    // pull-drag to prevent the pet from being stuck in the Pull sprite.
    if (event->type() == QEvent::ActivationChange && !isActiveWindow()) {
        if (m_pullDragging) {
            exitPullDrag();
        }
    }
}

// ==========================================
// Menu initialization
// ==========================================

void Widget::initMenu() {
    QAction *actStand = menu->addAction("先别动");
    actStand->setData(QVariant(static_cast<int>(RoleAct::Stand)));
    QAction *actMove = menu->addAction("自由移动");
    actMove->setData(QVariant(static_cast<int>(RoleAct::Move)));
    QAction *actSleep = menu->addAction("睡一会");
    actSleep->setData(QVariant(static_cast<int>(RoleAct::Sleeping)));

    QAction *actFormSwitch = menu->addAction("能展示你的另一个形态吗？");
    connect(actFormSwitch, &QAction::triggered, this, [this]() {
        toggleStandFormWithHalo();
    });

    QAction *actClock = menu->addAction("今夕何年");
    connect(actClock, &QAction::triggered, this, [this]() {
        current_time_text = QTime::currentTime().toString("HH:mm:ss");
        show_time_overlay = true;
        clock_display_timer->start(PB().clock_display_duration_ms);
        update();
    });

    auto_sing_toggle_action = menu->addAction("我想安静一点");
    connect(auto_sing_toggle_action, &QAction::triggered, this, [this]() {
        setAutoSingingEnabled(!cfg().auto_sing_enabled);
    });

    menu->addSeparator();
    QMenu *talkMenu = menu->addMenu("和她交流");
    QAction *inputAct = talkMenu->addAction("我想对你说...");
    connect(inputAct, &QAction::triggered, this, &Widget::openTalkInput);

    talkMenu->addSeparator();
    const auto &voiceMenu = PAU().voice_menu;
    for (const auto &item : voiceMenu) {
        QAction *act = talkMenu->addAction(item.label);
        act->setData(QVariant(item.path));
    }

    connect(talkMenu, &QMenu::triggered, this, [this](QAction *action){
        QString path = action->data().toString();
        if (!path.isEmpty()) {
            m_audio->playVoice(path);
            if (cur_role_act != RoleAct::Stand) {
                showActAnimation(RoleAct::Stand);
            }
        }
    });

    // ── 互动 submenu (E-1: per-feature builders) ──
    QMenu *interactMenu = menu->addMenu("互动");
    buildStatusPanelAction(interactMenu);
    buildFeedAction(interactMenu);
    buildStarActions(interactMenu);
    buildHeadPatAction(interactMenu);

    buildGomokuMenu(menu);

    menu->addSeparator();
    QAction *actExit = menu->addAction("回到天国");
    actExit->setData(QVariant(99));
    connect(this->menu, &QMenu::triggered, this, &Widget::onMenuTriggered);
}

// ==========================================
// E-1: Per-feature interaction menu builders
// ==========================================

void Widget::buildStatusPanelAction(QMenu *menu) {
    QAction *act = menu->addAction("显示角色状态");
    connect(act, &QAction::triggered, this, [this]() {
        if (!m_statusPanel) {
            m_statusPanel = new StatusPanel(m_statusMgr, nullptr);
        }
        QScreen *screen = QGuiApplication::primaryScreen();
        if (!screen) return;
        const QRect geo = geometry();
        m_statusPanel->showForDuration(
            QPoint(geo.right(), geo.top()),
            screen->availableGeometry().size(),
            20000);
    });
}

void Widget::buildFeedAction(QMenu *menu) {
    QAction *act = menu->addAction("喂食");
    connect(act, &QAction::triggered, this, [this]() {
        if (!m_foodMenu) {
            m_foodMenu = new FoodMenuWidget(this);
            connect(m_foodMenu, &FoodMenuWidget::foodSelected, this, [this](int idx) {
                const auto &food = m_foodMenu->foods().at(idx);
                static const StatusManager::StatusKind kinds[5] = {
                    StatusManager::Happiness, StatusManager::Interest,
                    StatusManager::Sanity,    StatusManager::Satiety,
                    StatusManager::Affection
                };
                for (int k = 0; k < 5; ++k) {
                    if (food.effects[k] != 0)
                        m_statusMgr->addWithOverflow(kinds[k], food.effects[k]);
                }
            });
        }
        m_foodMenu->showCentered();
    });
}

void Widget::buildStarActions(QMenu *menu) {
    m_releaseStarAction = menu->addAction("释放星星");
    connect(m_releaseStarAction, &QAction::triggered, this, [this]() { releaseStar(); });

    m_recallStarAction = menu->addAction("收回星星");
    m_recallStarAction->setEnabled(false);
    connect(m_recallStarAction, &QAction::triggered, this, [this]() { recallStar(); });

    m_recallAllStarsAction = menu->addAction("回收所有星星");
    m_recallAllStarsAction->setEnabled(false);
    connect(m_recallAllStarsAction, &QAction::triggered, this, [this]() { recallAllStars(); });
}

void Widget::buildHeadPatAction(QMenu *menu) {
    m_headPatAction = menu->addAction("摸摸头");
    connect(m_headPatAction, &QAction::triggered, this, [this]() {
        if (m_headPatWidget) return;          // H-1: QPointer — safe even during fade-out
        m_headPatAction->setEnabled(false);

        const QRect geo = geometry();
        const int charCx = geo.center().x();
        const int charTopQuarter = geo.top() + geo.height() / 4;

        // M-3: Geometry from config (editable in lumina_config.ini)
        const double radius   = cfg().head_pat_radius;
        const double startDeg = cfg().head_pat_start_deg;
        const double endDeg   = cfg().head_pat_end_deg;
        const int    squishMs = cfg().head_pat_squish_ms;
        const bool   fromRight = cfg().head_pat_from_right;

        const double endRad = qDegreesToRadians(endDeg);
        const int pivotX = fromRight
            ? charCx + static_cast<int>(radius * qCos(endRad))
            : charCx - static_cast<int>(radius * qCos(endRad));
        const int pivotY = charTopQuarter + static_cast<int>(radius * qSin(endRad));

        m_headPatWidget = new HeadPatWidget(QPoint(pivotX, pivotY), radius,
                                            startDeg, endDeg, nullptr);
        m_headPatWidget->mirrored = fromRight;

        // M-4: Single signal carries position; unused param is harmless
        connect(m_headPatWidget, &HeadPatWidget::contactStartAt, this,
                [this, squishMs](QPoint) {
            animateValue(m_squishAnim, m_squishFactor, 0.85, squishMs, QEasingCurve::OutCubic);
            animateValue(m_patGlowAnim, m_patGlowAlpha, 0.18, 120, QEasingCurve::OutQuad);
            // H-3: Micro-vibration via paint offset (no window move)
            auto *vib = new QVariantAnimation(this);
            vib->setStartValue(0.0);
            vib->setEndValue(1.0);
            vib->setDuration(80);
            connect(vib, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
                const double t = v.toDouble();
                const double d = 2.0 * qSin(t * 4.0 * M_PI) * (1.0 - t);
                m_vibrateOffset = QPointF(d, d * 0.7);
                update();
            });
            connect(vib, &QVariantAnimation::finished, this, [this]() {
                m_vibrateOffset = QPointF(0, 0);
                update();
            });
            vib->start(QAbstractAnimation::DeleteWhenStopped);
        });
        connect(m_headPatWidget, &HeadPatWidget::contactEnd, this,
                [this, squishMs]() {
            animateValue(m_squishAnim, m_squishFactor, 1.0, squishMs, QEasingCurve::InCubic);
            animateValue(m_patGlowAnim, m_patGlowAlpha, 0.0, 200, QEasingCurve::InQuad);
        });
        // M-6: stop()+deleteLater() instead of raw delete on running animations
        connect(m_headPatWidget, &HeadPatWidget::finished, this, [this]() {
            if (m_squishAnim)   { m_squishAnim->stop();   m_squishAnim->deleteLater(); }
            m_squishAnim = nullptr;
            if (m_patGlowAnim) { m_patGlowAnim->stop(); m_patGlowAnim->deleteLater(); }
            m_patGlowAnim = nullptr;
            m_squishFactor = 1.0;
            m_patGlowAlpha = 0.0;
            m_vibrateOffset = QPointF(0, 0);
            update();
            m_statusMgr->addWithOverflow(StatusManager::Affection, 3);
            m_statusMgr->addWithOverflow(StatusManager::Happiness, 3);
        });
        // H-1: Re-enable action only after widget fully destroyed (QPointer auto-nulls)
        connect(m_headPatWidget, &QObject::destroyed, this, [this]() {
            m_headPatAction->setEnabled(true);
        });

        m_headPatWidget->start();
    });
}

void Widget::buildGomokuMenu(QMenu *parentMenu) {
    QMenu *gomokuMenu = parentMenu->addMenu("陪我下五子棋");
    QAction *actFirst  = gomokuMenu->addAction("先手");
    QAction *actSecond = gomokuMenu->addAction("后手");
    QAction *actRandom = gomokuMenu->addAction("随机");
    connect(actFirst,  &QAction::triggered, this, [this]() { startGomoku(true); });
    connect(actSecond, &QAction::triggered, this, [this]() { startGomoku(false); });
    connect(actRandom, &QAction::triggered, this, [this]() {
        startGomoku(QRandomGenerator::global()->bounded(2) == 0);
    });
}

// ==========================================
// Talk input
// ==========================================

void Widget::openTalkInput() {
    // Dismiss status panel — dialog steals focus so leaveEvent may not fire
    m_hoverTimer->stop();
    if (m_statusPanel && !m_statusPanel->isFadingOut()) {
        m_statusPanel->fadeOut();
    }

    showBottomHintTransient(PS().hint_text_start_listening, cfg().hint_display_duration_ms);

    // M-11: Non-modal dialog avoids nested event loop (exec()) that could
    // cause re-entrancy with timers, GL rendering, and Gomoku async AI.
    auto *dlg = new QInputDialog(this);
    dlg->setWindowTitle("和她说话");
    dlg->setLabelText("我在听");
    dlg->setTextValue(QString());
    dlg->setInputMode(QInputDialog::TextInput);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    connect(dlg, &QInputDialog::textValueSelected, this, [this](const QString &text) {
        if (text.isEmpty()) return;
        last_user_input = text;
        if (cur_role_act != RoleAct::Stand)
            showActAnimation(RoleAct::Stand);
        if (cfg().ai_reply_enabled)
            sendAIMessage(text);
    });

    dlg->open();
}

// ==========================================
// AI chat helpers
// ==========================================

void Widget::ensureAIChat() {
    if (!m_deepSeekChat) {
        m_deepSeekChat = new DeepSeekChat(this);
        m_deepSeekChat->setSystemPrompt(cfg().ai_system_prompt);
    }
    if (!m_chatBubble) {
        m_chatBubble = new ChatBubbleWidget(this);

        connect(m_deepSeekChat, &DeepSeekChat::responseReady, this, [this](const QString &reply) {
            if (m_chatBubble) m_chatBubble->setResponseText(reply);
        });
        connect(m_deepSeekChat, &DeepSeekChat::errorOccurred, this, [this](const QString &err) {
            qWarning() << "DeepSeek error:" << err;
            if (!m_chatBubble) return;
            if (err == QStringLiteral("TIMEOUT")) {
                m_chatBubble->setResponseText(QStringLiteral("现实宇宙拒绝了她的回答"));
            } else if (err.startsWith(QStringLiteral("HTTP_"))) {
                m_chatBubble->setResponseText(QStringLiteral("错误码:") + err.mid(5));
            } else {
                m_chatBubble->setResponseText(QStringLiteral("错误码:") + err);
            }
        });
    }
}

void Widget::sendAIMessage(const QString &text) {
    ensureAIChat();

    // Show bubble at character top
    const QRect geo = geometry();
    const QPoint anchor(geo.center().x(), geo.top());
    m_chatBubble->setAutoCloseMs(cfg().ai_bubble_duration_ms);
    m_chatBubble->setContentPadding(cfg().ai_bubble_padding_px);
    m_chatBubble->showAt(anchor);
    m_chatBubble->setLoading();

    // Sync config (user may have updated it in settings)
    m_deepSeekChat->setApiKey(cfg().deepseek_api_key);
    m_deepSeekChat->setMaxHistory(cfg().ai_max_history);
    m_deepSeekChat->setTimeoutMs(cfg().ai_timeout_ms);
    m_deepSeekChat->setSystemPrompt(cfg().ai_system_prompt);

    // v4 perception: inject environment context into the outgoing message
    QString payload = text;
    if (!m_perceivedWindowTitle.isEmpty()) {
        payload = QStringLiteral("[环境感知：用户当前的前台窗口是「%1」]\n%2")
                      .arg(m_perceivedWindowTitle, text);
    }
    m_deepSeekChat->sendMessage(payload);
}

// ==========================================
// Settings dialog (delegates to SettingsDialog)
// ==========================================

// ==========================================
// Breath effect visibility
// ==========================================

void Widget::updateBreathEffectVisibility() {
    if (!m_breathFx) return;
    bool shouldShow = cfg().breath_effect_enabled && show_character && !show_light && !is_form_switching;
    m_breathFx->setEffectActive(shouldShow);
}

void Widget::reloadProfile(const QString &jsonPath) {
    // 1. Stop all active animations / timers
    stopWalking();
    endPlayfulMode(false);
    frame_timer->stop();
    m_actionMachine->stopAll();
    playful_duration_timer->stop();
    playmate_chase_timer->stop();
    auto_sing_timer->stop();

    // 2. Reload profile JSON
    PM()->loadFromFile(jsonPath);

    // 3. Re-seed config defaults from new profile, then re-load user INI overrides
    m_config->seedFromProfile();
    m_config->load();
    m_statusMgr->setStatsVariable(cfg().stats_variable);
    m_statusMgr->setTickIntervalMs(cfg().stats_tick_interval_ms);

    // 4. Rebuild sprite resources
    m_sprites->loadAll();
    light_pix.load(PS().light_symbol);

    if (m_breathFx && !light_pix.isNull()) {
        m_breathFx->setHaloTexture(light_pix);
    }
    if (m_hardware) {
        m_hardware->setPollInterval(PM()->breathEffect().poll_interval_ms);
    }

    // 5. Resize widget if profile changed it
    setFixedSize(PW().widget_size, PW().widget_size);

    // 6. Rebuild menu (voice items may have changed)
    menu->clear();
    auto_sing_toggle_action = nullptr;
    m_releaseStarAction = nullptr;
    m_recallStarAction = nullptr;
    m_recallAllStarsAction = nullptr;
    initMenu();

    if (auto_sing_toggle_action) {
        auto_sing_toggle_action->setText(cfg().auto_sing_enabled ? "我想安静一点" : "我想听听你的声音");
    }

    // 7. Restart perception with the new profile's settings
    if (PM()->perception().enabled) {
        startPerception();
    } else {
        stopPerception();
    }

    // 8. Restart normal operation
    showActAnimation(RoleAct::Stand);
    if (cfg().auto_sing_enabled) {
        scheduleNextHumming();
    }
    playLightAnimation();
    resetIdleTimer();
}

void Widget::openSettingsDialog() {
    // MED-13: Non-modal dialog avoids nested event loop (exec()) that could
    // cause re-entrancy with timers, GL rendering, and Gomoku async AI.
    auto *dlg = new SettingsDialog(cfg(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    connect(dlg, &QDialog::accepted, this, [this, dlg]() {
        BehaviorConfig newCfg = dlg->result();
        // Preserve fields not in the dialog
        newCfg.auto_sing_enabled  = cfg().auto_sing_enabled;
        newCfg.character_form = cfg().character_form;
        m_config->config() = newCfg;

        // Apply breath effect toggle immediately
        updateBreathEffectVisibility();

        // Apply status panel toggle immediately
        if (!cfg().status_panel_enabled) {
            m_hoverTimer->stop();
            if (m_statusPanel) m_statusPanel->fadeOut();
        }

        // Apply stats_variable toggle immediately
        m_statusMgr->setStatsVariable(cfg().stats_variable);
        m_statusMgr->setTickIntervalMs(cfg().stats_tick_interval_ms);

        // Refresh running timers for current state (v4: durations are
        // resolved from config at arm time, so re-arming picks up new values)
        allow_sit_try = false;
        m_actionMachine->refreshAll();
        if (playful_mode_active)
            playful_duration_timer->start(cfg().playful_mode_duration_ms);

        // Apply auto-throw star toggle immediately
        if (cfg().auto_throw_star) {
            m_autoThrowStarTimer->start(cfg().auto_throw_star_interval_ms);
        } else {
            m_autoThrowStarTimer->stop();
        }
        updateStarMenuState();

        m_config->save();
        resetIdleTimer();
    });

    dlg->open();
}

// ==========================================
// Star interaction
// ==========================================

void Widget::releaseStar() {
    if (m_stars.size() >= cfg().max_star_count) return;

    const auto &sp = ProfileManager::instance()->sprites();
    const int idx = QRandomGenerator::global()->bounded(sp.star_variant_count);
    const QString path = QString(sp.star_pattern).arg(idx);
    QPixmap pix(path);
    if (pix.isNull()) return;

    const double angle = QRandomGenerator::global()->generateDouble() * 2.0 * M_PI;
    const double speed = static_cast<double>(cfg().star_move_speed);
    const QPointF velocity(speed * std::cos(angle), speed * std::sin(angle));

    const QPoint startPos = geometry().center()
                            - QPoint(StarWidget::kDisplaySize / 2, StarWidget::kDisplaySize / 2);

    StarWidget *star = new StarWidget(pix, velocity, startPos);
    m_stars.append(star);

    if (!m_starPhysicsTimer->isActive())
        m_starPhysicsTimer->start(16);

    updateStarMenuState();
}

void Widget::recallStar() {
    if (m_stars.isEmpty()) return;
    StarWidget *last = m_stars.takeLast();
    last->close();
    last->deleteLater();   // defer: never destroy a widget inside its own event

    if (m_stars.isEmpty())
        m_starPhysicsTimer->stop();

    updateStarMenuState();
}

void Widget::recallAllStars() {
    if (m_stars.isEmpty()) return;
    m_starPhysicsTimer->stop();
    for (StarWidget *s : m_stars) {
        s->close();
        s->deleteLater();   // defer: never destroy a widget inside its own event
    }
    m_stars.clear();
    updateStarMenuState();
}

void Widget::updateStarMenuState() {
    if (m_releaseStarAction)
        m_releaseStarAction->setEnabled(m_stars.size() < cfg().max_star_count);
    if (m_recallStarAction)
        m_recallStarAction->setEnabled(!m_stars.isEmpty());
    if (m_recallAllStarsAction)
        m_recallAllStarsAction->setEnabled(!m_stars.isEmpty());
}

void Widget::updateStarPhysics() {
    if (m_stars.isEmpty()) return;

    const double dt = 16.0 / 1000.0;

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect sr = screen->availableGeometry();

    // 1. Advance all non-dragged stars
    for (StarWidget *s : m_stars)
        s->advance(dt);

    // 2. Bounce off screen edges
    for (StarWidget *s : m_stars) {
        if (s->isDragging()) continue;
        QPointF p = s->posF();
        QPointF v = s->velocity();
        bool changed = false;

        if (p.x() < sr.left()) {
            p.setX(sr.left()); v.setX(std::abs(v.x())); changed = true;
        } else if (p.x() + s->width() > sr.right()) {
            p.setX(sr.right() - s->width()); v.setX(-std::abs(v.x())); changed = true;
        }
        if (p.y() < sr.top()) {
            p.setY(sr.top()); v.setY(std::abs(v.y())); changed = true;
        } else if (p.y() + s->height() > sr.bottom()) {
            p.setY(sr.bottom() - s->height()); v.setY(-std::abs(v.y())); changed = true;
        }

        if (changed) { s->setPosF(p); s->setVelocity(v); }
    }

    // 3. Star–star elastic collision (equal mass)
    for (int i = 0; i < m_stars.size(); ++i) {
        for (int j = i + 1; j < m_stars.size(); ++j) {
            StarWidget *a = m_stars[i];
            StarWidget *b = m_stars[j];

            const QPointF ca = a->centerF();
            const QPointF cb = b->centerF();
            const QPointF d  = ca - cb;
            const double dist = std::sqrt(d.x() * d.x() + d.y() * d.y());
            const double minDist = a->radius() + b->radius();

            if (dist >= minDist || dist < 1e-6) continue;

            const QPointF n = d / dist;
            const QPointF relVel = a->velocity() - b->velocity();
            const double vnRel = relVel.x() * n.x() + relVel.y() * n.y();

            if (vnRel > 0) continue; // already separating

            const QPointF impulse = n * vnRel;
            if (!a->isDragging()) a->setVelocity(a->velocity() - impulse);
            if (!b->isDragging()) b->setVelocity(b->velocity() + impulse);

            const double overlap = (minDist - dist) / 2.0 + 1.0;
            if (!a->isDragging()) a->setPosF(a->posF() + n * overlap);
            if (!b->isDragging()) b->setPosF(b->posF() - n * overlap);
        }
    }
}

// ==========================================
// Gomoku integration
// ==========================================

void Widget::startGomoku(bool humanFirst) {
    if (m_gomokuMode) return;

    // H-1: Null-safe screen access
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    m_gomokuMode = true;
    m_gomokuSuspended = false;
    m_gomokuFlyAnim = nullptr;
    m_gomokuFlyCallback = nullptr;

    // Freeze attribute system during Gomoku
    m_statusMgr->setFrozen(true);

    // Save current position
    m_preGomokuPos = pos();

    // Stop all behavior timers
    stopWalking();
    m_actionMachine->stopAll();
    playful_duration_timer->stop();
    playmate_chase_timer->stop();
    auto_sing_timer->stop();
    stand_shake_timer->stop();
    click_reset_timer->stop();
    endPlayfulMode(false);

    // Switch to Stand (this re-arms Stand's rules, so disarm again)
    showActAnimation(RoleAct::Stand);
    m_actionMachine->stopAll();
    frame_timer->stop();

    // Move character to top-right corner
    QPoint topRight(screen->availableGeometry().right() - PW().widget_size,
                    screen->availableGeometry().top());
    QWidget::move(topRight);
    m_gomokuFlyTarget = topRight;  // Init bug fix: seed fly target for drag self-heal

    // Create and show the board
    m_gomokuWidget = new GomokuWidget(humanFirst, this);
    connect(m_gomokuWidget, &GomokuWidget::aiMoveReady,  this, &Widget::onGomokuAIMoveReady);
    connect(m_gomokuWidget, &GomokuWidget::aiPlaceDone,  this, &Widget::onGomokuAIPlaceDone);
    connect(m_gomokuWidget, &GomokuWidget::gameFinished,  this, &Widget::onGomokuFinished);
    connect(m_gomokuWidget, &GomokuWidget::boardReady,    this, [this]() { raise(); });

    // Force character above the board at all times
    raise();
}

void Widget::endGomoku() {
    if (!m_gomokuMode) return;

    if (m_gomokuWidget) {
        m_gomokuWidget->quitGame();
        // onGomokuFinished will be called after fade-out
    }
}

void Widget::onGomokuAIMoveReady(QPoint screenPos) {
    if (!m_gomokuMode || !m_gomokuWidget) return;

    // H-5: Block placement signals while angry attack is in progress
    if (m_gomokuSuspended) return;

    // Fly character so that its center is above the board intersection
    const int halfW = PW().widget_size / 2;
    QPoint target(screenPos.x() - halfW, screenPos.y() - halfW - PS().role_display_size / 2);

    const auto &gcfg = PM()->gomoku();
    flyCharacterTo(target, gcfg.ai_move_fly_duration_ms, [this]() {
        if (m_gomokuWidget && !m_gomokuSuspended) {
            m_gomokuWidget->confirmAIPlace();
        }
    });
}

void Widget::onGomokuAIPlaceDone() {
    if (!m_gomokuMode) return;

    // H-1: Null-safe screen access
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    // Fly back to top-right corner
    QPoint topRight(screen->availableGeometry().right() - PW().widget_size,
                    screen->availableGeometry().top());

    const auto &gcfg = PM()->gomoku();
    flyCharacterTo(topRight, gcfg.ai_move_fly_duration_ms, nullptr);
}

void Widget::onGomokuFinished() {
    m_gomokuMode = false;
    m_gomokuSuspended = false;

    // Unfreeze attribute system
    m_statusMgr->setFrozen(false);

    // Cancel any in-flight gomoku animation
    if (m_gomokuFlyAnim) {
        m_gomokuFlyAnim->stop();
        m_gomokuFlyAnim->deleteLater();
        m_gomokuFlyAnim = nullptr;
    }
    m_gomokuFlyCallback = nullptr;

    if (m_gomokuWidget) {
        m_gomokuWidget->deleteLater();
        m_gomokuWidget = nullptr;
    }

    // Restore position and normal behavior
    QWidget::move(m_preGomokuPos);
    showActAnimation(RoleAct::Stand);
    resetIdleTimer();

    if (cfg().auto_sing_enabled) {
        scheduleNextHumming();
    }
}

void Widget::flyCharacterTo(QPoint screenPos, int durationMs, std::function<void()> onDone) {
    // Cancel any previous fly animation
    if (m_gomokuFlyAnim) {
        m_gomokuFlyAnim->stop();
        m_gomokuFlyAnim->deleteLater();
        m_gomokuFlyAnim = nullptr;
    }

    // H-5: Store fly target and callback for drag self-heal
    m_gomokuFlyTarget = screenPos;
    m_gomokuFlyCallback = onDone;

    // Ensure character stays above the board during flight
    raise();

    QPropertyAnimation *anim = new QPropertyAnimation(this, "pos");
    anim->setDuration(durationMs);
    anim->setStartValue(pos());
    anim->setEndValue(screenPos);
    anim->setEasingCurve(QEasingCurve::InOutQuad);

    m_gomokuFlyAnim = anim;

    connect(anim, &QPropertyAnimation::finished, this, [this, anim, onDone]() {
        if (m_gomokuFlyAnim == anim)
            m_gomokuFlyAnim = nullptr;
        raise();
        if (onDone && !m_gomokuSuspended)
            onDone();
        anim->deleteLater();
    });

    anim->start();
}

void Widget::gomokuDragSelfHeal() {
    if (!m_gomokuMode || m_gomokuSuspended) return;

    // If a fly animation was interrupted by dragging, smoothly fly back
    // to the last known target position
    if (!m_gomokuFlyTarget.isNull()) {
        const auto &gcfg = PM()->gomoku();
        flyCharacterTo(m_gomokuFlyTarget, gcfg.ai_move_fly_duration_ms, m_gomokuFlyCallback);
    }
}

void Widget::resumeGomokuFromAngry() {
    m_gomokuSuspended = false;
    showActAnimation(RoleAct::Stand);
    m_actionMachine->stopAll();
    frame_timer->stop();

    // Fly back to top-right corner, then the board will continue naturally
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QPoint topRight(screen->availableGeometry().right() - PW().widget_size,
                    screen->availableGeometry().top());

    const auto &gcfg = PM()->gomoku();
    flyCharacterTo(topRight, gcfg.ai_move_fly_duration_ms, nullptr);
}
