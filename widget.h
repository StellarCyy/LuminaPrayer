#ifndef WIDGET_H
#define WIDGET_H

#include "roleact.h"
#include <QWidget>
#include <QTimer>
#include <QMenu>
#include <QVariantAnimation>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <functional>

class QPaintEvent;
class QContextMenuEvent;
class QPropertyAnimation;
class QThread;
class ConfigManager;
class SpriteResource;
class PlatformHAL;
class ActionStateMachine;
class PerceptionBus;
class AudioManager;
class Playmate;
class HardwareManager;
class EffectManager;
class GomokuWidget;
class StatusPanel;
class StatusManager;
class DeepSeekChat;
class ChatBubbleWidget;
class FoodMenuWidget;
class HeadPatWidget;
class StarWidget;

class Widget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QPoint pos READ pos WRITE move)

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

    // Public API (used by DragFilter, main.cpp)
    void stopWalking();
    void showActAnimation(RoleAct k);          // legacy enum wrapper
    void enterAction(const QString &actionId); // v4 string-keyed state entry
    void showFromTray();
    void startRandomWalk();
    void resetIdleTimer();
    void addClickCount();
    void triggerStandClickShake();
    void onPrimaryLeftClick();
    void openTalkInput();
    void openSettingsDialog();
    void reloadProfile(const QString &jsonPath);

    // Pull-drag (hand-grabbing) API
    void enterPullDrag(const QPoint &mouseGlobal);
    void updatePullDrag(const QPoint &mouseGlobal);
    void exitPullDrag();
    bool isPullDragging() const { return m_pullDragging; }

    // Gomoku
    void startGomoku(bool humanFirst);
    void endGomoku();
    bool isInGomokuMode() const { return m_gomokuMode; }
    bool isGomokuSuspended() const { return m_gomokuSuspended; }
    void gomokuDragSelfHeal();

public slots:
    void onMenuTriggered(QAction *action);

protected:
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    // Initialization
    void initMenu();
    // E-1: Per-feature interaction menu builders (add new features here)
    void buildStatusPanelAction(QMenu *menu);
    void buildFeedAction(QMenu *menu);
    void buildStarActions(QMenu *menu);
    void buildHeadPatAction(QMenu *menu);
    void buildGomokuMenu(QMenu *parentMenu);

    // Animations
    void playLightAnimation();
    void playExitAnimation();
    void toggleStandFormWithHalo();
    void applyCurrentForm();

    // v4 data-driven state machine
    void setupActionMachine();
    void onMachineTransition(const QString &to, const QStringList &postBehaviors);

    // v4 perception bus (worker-thread lifecycle)
    void startPerception();
    void stopPerception();

    // Behavior
    void findAndSitOnWindow();
    void triggerAngryAttack();
    void scheduleNextHumming();
    void setAutoSingingEnabled(bool enabled);
    void showBottomHintTransient(const QString &pixPath, int durationMs);

    // Playful mode
    void startPlayfulMode();
    void endPlayfulMode(bool playmateExitAnimation);
    void updatePlaymateChase();

    void updateBreathEffectVisibility();
    void animateValue(QVariantAnimation*& slot, double& prop,
                      double target, int durationMs, QEasingCurve::Type curve);

    // AI chat helpers (extracted from openTalkInput to reduce inline complexity)
    void ensureAIChat();
    void sendAIMessage(const QString &text);

    // Star interaction
    void releaseStar();
    void recallStar();
    void recallAllStars();
    void updateStarPhysics();
    void updateStarMenuState();

    // Gomoku helpers
    void onGomokuAIMoveReady(QPoint screenPos);
    void onGomokuAIPlaceDone();
    void onGomokuFinished();
    void flyCharacterTo(QPoint screenPos, int durationMs, std::function<void()> onDone);
    void resumeGomokuFromAngry();

    // Convenience config accessor
    inline const struct BehaviorConfig& cfg() const;
    inline struct BehaviorConfig& cfg();

    // -- Components (owned) --
    ConfigManager  *m_config;
    SpriteResource *m_sprites;
    PlatformHAL    *m_hal;
    AudioManager    *m_audio;
    HardwareManager *m_hardware;
    EffectManager   *m_breathFx;
    ActionStateMachine *m_actionMachine;
    PerceptionBus   *m_perception;        // lives on m_perceptionThread
    QThread         *m_perceptionThread;
    QString          m_perceivedWindowTitle;   // GUI-thread cache of bus output

    // -- Character state --
    RoleAct  cur_role_act;      // enum view (legacy consumers; custom ids map to Stand)
    QString  cur_action_id;     // v4 source of truth for the active action
    QString  cur_role_pix;
    bool     show_character;

    // -- Form switch --
    bool     is_form_switching;
    double   form_flash_opacity;

    // -- Halo / light animation --
    QPixmap  light_pix;
    int      current_light_size;
    bool     show_light;
    double   current_opacity;

    // -- Clock overlay --
    QString  current_time_text;
    bool     show_time_overlay;
    QString  bottom_hint_transient_pix;
    QTimer  *bottom_hint_timer;

    // -- Movement --
    QVariantAnimation *current_move_anim;
    bool move_face_right;
    bool allow_sit_try;

    // -- Timers --
    // (state-transition timers live in ActionStateMachine since v4)
    QTimer  *frame_timer;
    QTimer  *clock_timer;
    QTimer  *clock_display_timer;
    QTimer  *click_reset_timer;
    QTimer  *stand_shake_timer;
    QTimer  *playful_duration_timer;
    QTimer  *playmate_chase_timer;
    QTimer  *auto_sing_timer;

    // -- Menu / UI --
    QMenu   *menu;
    QAction *auto_sing_toggle_action;

    // -- Status system --
    StatusManager  *m_statusMgr;
    StatusPanel    *m_statusPanel;
    QTimer         *m_hoverTimer;

    // -- Frame animation --
    int      m_frameIndex;

    // -- Interaction state --
    int      click_count;
    bool     playful_mode_active;
    bool     is_stand_shaking;
    QPoint   stand_shake_origin;
    int      stand_shake_remaining_steps;
    QPointF  playmate_velocity;
    Playmate *playmate;
    QString  last_user_input;

    // -- Pull-drag (hand-grabbing) --
    bool     m_pullDragging       = false;
    bool     m_pullFaceRight      = false;
    QPoint   m_lastPullMouseGlobal;

    // -- AI Chat --
    DeepSeekChat    *m_deepSeekChat  = nullptr;
    ChatBubbleWidget *m_chatBubble   = nullptr;

    // -- Food menu --
    FoodMenuWidget  *m_foodMenu      = nullptr;

    // -- Head pat --
    QAction            *m_headPatAction  = nullptr;
    QPointer<HeadPatWidget> m_headPatWidget;
    QVariantAnimation  *m_squishAnim     = nullptr;
    double              m_squishFactor   = 1.0;   // 1.0 = normal, <1 = squished vertically
    QVariantAnimation  *m_patGlowAnim   = nullptr;
    double              m_patGlowAlpha   = 0.0;   // 0 = no glow, >0 = energy pulse overlay
    QPointF             m_vibrateOffset;           // paint-time offset for micro-vibration

    // -- Star interaction --
    QList<StarWidget*> m_stars;
    QAction *m_releaseStarAction  = nullptr;
    QAction *m_recallStarAction      = nullptr;
    QAction *m_recallAllStarsAction  = nullptr;
    QTimer  *m_starPhysicsTimer;
    QTimer  *m_autoThrowStarTimer;

    // -- Gomoku --
    GomokuWidget *m_gomokuWidget;
    bool     m_gomokuMode;
    bool     m_gomokuSuspended;
    QPoint   m_preGomokuPos;
    QPoint   m_gomokuFlyTarget;
    std::function<void()> m_gomokuFlyCallback;
    QPropertyAnimation *m_gomokuFlyAnim;
};

#endif // WIDGET_H
