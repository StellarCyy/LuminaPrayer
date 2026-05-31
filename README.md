# LuminaPrayer 项目文档

> 最后更新：2026-04-27
> 构建环境：Qt 6.8.3 + MSVC2022 (x64) + CMake + Ninja

---

## 1. 项目定位

LuminaPrayer 是一个基于 Qt6 Widgets 的**桌面宠物应用**。角色以透明置顶窗口（`WA_TranslucentBackground + FramelessWindowHint`）驻留桌面，支持动作状态机、语音互动、AI 对话、养成属性、五子棋、GPU 呼吸特效等功能。

---

## 2. 功能总览

### 2.1 角色状态与动画
- 六种动作状态：`Stand`、`Move`、`Sleeping`、`Angry`、`Sitting_1`、`Sitting_2`（枚举定义于 `roleact.h`）
- **双形态**：Primary / Alternate 可独立配置每种动作的精灵图，菜单切换时播放光晕过渡动画
- 帧动画由 `frame_timer`（默认 100ms）驱动，`SpriteResource` 统一缓存

### 2.2 自动行为
- **随机行走**：正弦曲线轨迹 + 屏幕边缘 clamp
- **窗口停靠**：自动检测前台窗口顶部，飞行降落并播放坐下动画
- **嬉戏模式**：追逐型伙伴角色（Playmate），概率触发，定时自动退出
- **空闲转睡眠**：长时间空闲后自动切换为 `Sleeping`

### 2.3 交互
- **拖拽/点击**：`DragFilter` 事件过滤器区分拖拽与点击（20px 阈值）
- **Stand 点击抖动**：点击 Stand 状态角色时短暂跳动
- **手抓拖拽**：长按切换为 Pull 精灵，可拖拽角色到任意位置
- **连续点击红温**：达到阈值后触发 `Angry`，播放拳头追踪攻击特效 + 音效

### 2.4 菜单系统
- 右键菜单：动作切换、形态切换、时钟叠层、哼歌开关
- **和她交流** 子菜单：预设语音 + "我想对你说..."（AI 对话入口）
- **互动** 子菜单：显示角色状态、喂食、五子棋

### 2.5 音频
- `AudioManager` 封装 `QMediaPlayer`/`QAudioOutput`，带错误恢复（player 重建）
- 自动哼歌：非 `Sleeping` 状态按随机间隔播放，菜单可切换

### 2.6 AI 对话
- `DeepSeekChat`：DeepSeek API 异步调用，滑动窗口历史（默认 11 条），带 system prompt
- `ChatBubbleWidget`：气泡窗口展示回复，自适应字号，发光效果，自动关闭
- API Key 在 INI 文件中以 XOR+Base64 混淆存储（`obf:` 前缀），向后兼容明文

### 2.7 养成属性系统
- 详见下方 §6

### 2.8 喂食系统
- `FoodMenuWidget`：卡片式食物选择界面，滚动布局，点击后属性增减
- 食物定义在 `character.json → food_menu.items`，每项含 5 维属性效果

### 2.9 五子棋
- `GomokuEngine`：纯逻辑 15×15 棋盘，四方向胜负检测，AI 威胁评分
- `GomokuWidget`：金丝棋盘 UI，粒子汇聚特效、胜利发光、淡出动画
- AI 计算通过 `QtConcurrent::run` 在工作线程执行，完成后角色飞行至落子位置
- 对局期间属性系统冻结，行为定时器全部暂停

### 2.10 GPU 呼吸特效
- `EffectManager`（`QObject`）：Offscreen FBO + GLSL 着色器
- 呼吸频率关联 CPU 使用率（`HardwareManager` 通过 `GetSystemTimes` 轮询）
- 渲染结果通过 `QImage` 回传至 `Widget::paintEvent` 合成

### 2.11 角色下方文字提示
- 各场景切换时短暂显示对应提示图（`text_ok.png`、`text_can_sing.png`、`text_go_to_sleep.png` 等）

---

## 3. 架构与模块依赖

```
main.cpp                    应用入口 + 系统托盘 + 结构化文件日志
  │
  └── Widget (widget.h/cpp)  顶层协调器: 状态机、定时器调度、绘制、菜单
        ├── ConfigManager      运行时配置读写 (lumina_config.ini)
        ├── ProfileManager     角色配置文件解析 (character.json) — 单例
        ├── SpriteResource     精灵资源加载 + 像素缓存 + 双形态 action map
        ├── PlatformHAL        平台抽象层: Windows 窗口检测/监控 (隔离 #ifdef)
        ├── DragFilter         鼠标拖拽 + 点击检测事件过滤器
        ├── AudioManager       音频播放封装 (QMediaPlayer + 错误恢复)
        ├── HardwareManager    CPU 使用率轮询 (GetSystemTimes)
        ├── EffectManager      GPU 呼吸特效 (offscreen FBO + GLSL)
        ├── StatusManager      养成属性系统 (5维属性 + tick + overflow)
        ├── StatusPanel        属性面板 UI (hover 显示)
        ├── DeepSeekChat       AI 对话 (DeepSeek API + 滑动窗口历史)
        ├── ChatBubbleWidget   对话气泡 UI
        ├── FoodMenuWidget     喂食卡片 UI
        ├── GomokuWidget       五子棋 UI + 特效
        │     └── GomokuEngine  纯棋盘逻辑 + AI
        ├── FistWidget         红温拳头攻击特效 (+ StaticHalo)
        ├── Playmate           嬉戏模式伙伴角色
        └── SettingsDialog     参数设置对话框

fontutils.h                 共享 CJK 衬线字体解析工具
roleact.h                   共享状态枚举 RoleAct
shaders/                    GLSL 着色器 (breathe.vert / breathe.frag)
```

---

## 4. 完整源文件地图

| 文件 | 职责 | 关键类/结构体 |
|------|------|---------------|
| `main.cpp` | 应用入口、系统托盘、结构化日志 (`luminaLogHandler`) | — |
| `roleact.h` | `RoleAct` 枚举 (Stand/Move/Sleeping/Angry/Sitting_1/Sitting_2) | `Act::RoleAct` |
| `fontutils.h` | CJK 衬线字体解析，带 fallback 链 | `resolveCJKSerifFont()` |
| `profilemanager.h/cpp` | 从 `character.json` 加载角色配置，单例 | `ProfileManager`, 所有 `*Profile` 结构体 |
| `configmanager.h/cpp` | 运行时配置 (INI)，种子来自 ProfileManager | `ConfigManager`, `BehaviorConfig` |
| `spriteresource.h/cpp` | 双形态精灵加载，QPixmap 缓存 | `SpriteResource` |
| `platformhal.h/cpp` | Windows 窗口检测、屏幕 clamp | `PlatformHAL`, `SittableWindow` |
| `dragfilter.h/cpp` | 事件过滤器：拖拽/点击/抖动 | `DragFilter` |
| `audiomanager.h/cpp` | QMediaPlayer 封装，错误恢复 | `AudioManager` |
| `hardwaremanager.h/cpp` | CPU 使用率轮询 (GetSystemTimes) | `HardwareManager` |
| `effectmanager.h/cpp` | Offscreen FBO + GLSL 呼吸光效 | `EffectManager` |
| `statusmanager.h/cpp` | 5 维属性 tick 系统，overflow 逻辑 | `StatusManager` |
| `statuspanel.h/cpp` | 属性面板绘制 (hover 显示/fadeout) | `StatusPanel` |
| `deepseekchat.h/cpp` | DeepSeek API 调用，滑动窗口历史 | `DeepSeekChat` |
| `chatbubblewidget.h/cpp` | 气泡窗口，自适应字号，发光效果 | `ChatBubbleWidget` |
| `foodmenuwidget.h/cpp` | 喂食卡片 UI (FoodGridWidget + FoodMenuWidget) | `FoodMenuWidget`, `FoodItem` |
| `gomokuengine.h/cpp` | 15×15 棋盘逻辑 + AI 威胁评分 | `GomokuEngine` |
| `gomokuwidget.h/cpp` | 五子棋 UI，粒子/胜利/淡出特效 | `GomokuWidget` |
| `fistwidget.h/cpp` | 红温拳头追踪 + 光晕 | `FistWidget`, `StaticHalo` |
| `playmate.h/cpp` | 嬉戏伙伴角色动画 | `Playmate` |
| `settingsdialog.h/cpp` | 参数设置对话框 | `SettingsDialog` |
| `widget.h/cpp` | **主窗口**：状态机、所有行为调度、绘制、菜单 | `Widget` |
| `shaders/breathe.vert` | 顶点着色器：直通位置+纹理坐标 | — |
| `shaders/breathe.frag` | 片段着色器：呼吸 alpha + 暖色叠加 + R↔B swap | — |

### 资源文件

| 文件 | 内容 |
|------|------|
| `imgs.qrc` | 所有图片资源（角色、特效、食物卡、提示文字等） |
| `audio.qrc` | 所有音频资源（语音、哼歌、愤怒音效） |
| `shaders.qrc` | GLSL 着色器文件 |
| `character.json` | 角色配置（精灵、动画、行为、UI、五子棋等全部参数） |
| `lumina_config.ini` | 运行时持久化配置（用户调整的参数） |

---

## 5. 双层配置系统

### 5.1 character.json — 角色 Profile（只读）

由 `ProfileManager`（单例）在启动时加载，包含所有设计时参数。每个 section 对应一个 `*Profile` 结构体：

| JSON Section | C++ 结构体 | 说明 |
|---|---|---|
| `meta` | — | 角色名、版本号 |
| `window` | `WindowProfile` | 窗口尺寸、拳头尺寸 |
| `sprites` | `SpritesProfile` | 精灵路径（双形态）、提示图路径 |
| `sprites.forms.primary/alternate` | `FormDef` | 每种形态的每种动作精灵 pattern + count |
| `audio` | `AudioProfile` | 音效路径 + 语音菜单项 |
| `animation` | `AnimationProfile` | 所有动画时间/尺寸参数 (>25 项) |
| `behavior` | `BehaviorProfile` | 行为逻辑参数 (>30 项) |
| `breath_effect` | `BreathEffectProfile` | GPU 呼吸特效参数 |
| `ui` | `UIProfile` | 时钟叠层字体/颜色 |
| `status_system` | `StatusSystemProfile` | 属性面板开关、延迟 |
| `gomoku` | `GomokuProfile` | 五子棋棋盘/特效参数 |
| `food_menu` | `FoodMenuProfile` | 食物列表 + 图片 pattern |

**所有数值类参数在 parse 阶段均有 `qBound` 范围校验**，防止 JSON 注入无效值。

每个精灵条目的格式：
```json
{ "pattern": ":/character1/solyn%d.png", "count": 1 }
```
- `pattern`：资源路径模板，`%d` 替换为帧索引 (0-based)
- `count`：帧数量

### 5.2 lumina_config.ini — 运行时配置（读写）

由 `ConfigManager` 管理。启动时先从 `ProfileManager` **种子**（`seedFromProfile()`），再用 INI 值覆盖，最后 `validate()` 范围校验。

当前完整持久化项：

```ini
[form]
use_alternate=false

[behavior]
stand_to_move_wait_ms=25000
move_to_sleep_wait_ms=180000
move_to_sit_wait_ms=25000
move_to_playful_wait_ms=30000
sit_detection_interval_ms=10000
sit_trigger_chance_percent=30
playful_detection_interval_ms=10000
playful_trigger_chance_percent=30
sit_mode_duration_ms=30000
playful_mode_duration_ms=30000
angry_click_threshold=10
playmate_min_spacing_px=120
move_speed_px_per_sec=160
playmate_speed_scale=2
playmate_accel_scale=5
hint_display_duration_ms=2000
auto_sing_enabled=true
breath_effect_enabled=true
status_panel_enabled=false
stats_variable=true
stats_tick_interval_ms=10000
ai_reply_enabled=false
deepseek_api_key=obf:<Base64编码的XOR混淆值>
ai_max_history=11
ai_bubble_duration_ms=15000
ai_bubble_padding_px=50
ai_system_prompt=...

[window]
pos=@Point(100 100)
```

**扩展提示**：新增可配置参数的完整流程：
1. `BehaviorConfig` 结构体中添加字段（含默认值）
2. `ConfigManager::seedFromProfile()` 从 ProfileManager 读入
3. `ConfigManager::load()` 中添加 `s.value(...)` 行
4. `ConfigManager::save()` 中添加 `s.setValue(...)` 行
5. `ConfigManager::validate()` 中添加范围校验
6. `SettingsDialog::buildUI()` 中添加 UI 控件
7. `SettingsDialog::result()` 中回填到结构体

---

## 6. 养成属性系统

### 6.1 五种属性

| 属性 | 枚举值 | 初始值 | 范围 | 说明 |
|------|--------|--------|------|------|
| 快乐 | `Happiness` | 80 | 0–100 | 核心溢出池 |
| 兴致 | `Interest` | 80 | 0–100 | 仅在 Stand/Move 且空闲 >30s 时衰减 |
| 理智 | `Sanity` | 80 | 0–100 | tick +1，Angry 时额外 −5 |
| 饱食 | `Satiety` | 80 | 0–100 | tick −1 |
| 亲密 | `Affection` | 100 | 0–100 | tick +1 |

### 6.2 tick 规则（`StatusManager::onTick`，默认 10s）
受 `stats_variable` && `!frozen` 双重门控：
1. **Affection** +1
2. **Satiety** −1
3. **Sanity** +1
4. **Interest** −1（仅当 `Stand`/`Move` 且距离上次鼠标交互 > 30s）

### 6.3 负值溢出（Happiness Overflow）
当 Satiety/Sanity/Interest 已降至 0 而逻辑仍需扣减时，**溢出量从 Happiness 扣除**。使用 `addWithOverflow()` 方法。

### 6.4 特殊事件
- **Angry 触发** → `notifyAngry()` → Sanity 立即额外 −5
- **喂食** → `FoodMenuWidget` 选中食物 → 5 维属性增减（`effects[5]` 数组）
- **五子棋** → `setFrozen(true)` 冻结全部 tick；结束后 `setFrozen(false)` 恢复

---

## 7. AI 对话系统

### 7.1 架构

```
Widget::openTalkInput() → QInputDialog (非模态)
  → Widget::ensureAIChat() → 创建/复用 DeepSeekChat + ChatBubbleWidget
  → Widget::sendAIMessage(text) → DeepSeekChat::sendMessage(text) (异步)
    → HTTP POST → DeepSeek API
    → 响应 → parseResponse() → emit responseReady(reply)
    → ChatBubbleWidget::setResponseText(reply)
```

### 7.2 滑动窗口历史
- 历史数组 `m_history`：`[system_prompt, user1, assistant1, user2, assistant2, ...]`
- 最大条数由 `ai_max_history` 控制（默认 11 = 1 system + 5 对话对）
- `trimHistory()`：O(n) 单次重建，保留 system prompt [0] + 最新尾部
- 网络/解析错误时自动回滚孤立的 user 消息（`rollbackUserMsg()`）

### 7.3 API Key 安全
- INI 存储：`obf:` 前缀 + XOR(`"LmPr2025"`) + Base64
- 加载时 `deobfuscateKey()` 自动解码，兼容旧版明文存储
- **注意**：这只是混淆，不是加密。敏感场景需更强保护

---

## 8. 五子棋系统

### 8.1 模块

| 类 | 职责 |
|----|------|
| `GomokuEngine` | 纯逻辑：15×15 棋盘、`placePiece`、四方向胜负检测、AI 评分 |
| `GomokuWidget` | UI：金丝棋盘、径向渐变棋子、粒子汇聚、胜利发光、淡出动画 |

### 8.2 AI
- `computeAIMove(Cell aiPiece) const`：const 方法，通过 `QtConcurrent::run` 在工作线程执行
- 评分规则：立即胜利 > 阻止对手胜利 > 开放型四连 > 一般威胁
- 仅检查已有棋子 2 格范围内的空位（减少搜索空间）

### 8.3 与主窗口的集成
1. `Widget::startGomoku(humanFirst)` → 冻结属性、暂停行为定时器、角色飞到右上角
2. AI 落子后发射 `aiMoveReady(screenPos)` → 角色飞到落子位置 → `aiPlaceDone()`
3. 游戏结束 → `gameFinished()` → `endGomoku()` → 恢复所有状态
4. 对局中右键菜单仅显示"先到这里吧"
5. Angry 触发时 Gomoku 暂时挂起（`m_gomokuSuspended`），结束后恢复

### 8.4 character.json 中的参数
```json
"gomoku": {
    "board_size": 15,           // 棋盘格数 [5–19]
    "cell_size": 50,            // 格子像素 [20–120]
    "board_padding": 40,        // 棋盘边距 [10–200]
    "border_line_width": 3.5,   // 外框线宽
    "inner_line_width": 1.2,    // 内线线宽
    "line_color": [218,165,32], // 金色线
    "board_bg_color": [50,40,30,220],
    "piece_radius": 20,         // 棋子半径 [5–cell_size/2]
    "particle_count": 24,       // 粒子数量
    "particle_duration_ms": 250,
    "win_glow_duration_ms": 3000,
    "fade_out_duration_ms": 500,
    "ai_move_fly_duration_ms": 400,
    "ai_think_delay_ms": 200
}
```

---

## 9. GPU 呼吸特效

### 9.1 架构
`EffectManager`（继承 `QObject`，不是 QWidget）：
- `QOpenGLContext` + `QOffscreenSurface` + `QOpenGLFramebufferObject`
- 渲染到 FBO → `toImage()` 回读 → `Widget::paintEvent` 中 `drawImage()`

### 9.2 纹理上传
```cpp
QImage img = pixmap.toImage().convertToFormat(QImage::Format_RGBA8888).mirrored();
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());
```
- CPU 端统一为 `RGBA8888`（字节序 R,G,B,A，跨平台一致）
- GPU 端 `GL_RGBA + GL_UNSIGNED_BYTE` 精确匹配
- `force_swap_rb`：character.json 中的安全阀，某些 GPU 驱动需要 R↔B 交换

### 9.3 着色器
- `breathe.vert`：直通 position + texcoord
- `breathe.frag`：呼吸 alpha 调制 + CPU 高负载暖色叠加 + 可选 R↔B swap

---

## 10. 关键扩展点

| 需求 | 修改位置 |
|------|----------|
| **新增角色动作** | `roleact.h` 添加枚举 → `FormDef` 添加 `SpriteEntry` → `parseFormDef` 添加解析 → `SpriteResource::loadAll` 添加加载 → `Widget::showActAnimation` 添加分支 |
| **新增角色形态** | `SpritesProfile` 添加 `FormDef` 字段 → `parseSprites` 添加解析 → `SpriteResource` 添加第三套 map |
| **新增精灵帧动画** | `character.json` 中对应 `count` 改为 >1 → 确保所有帧图片已加入 `imgs.qrc` |
| **新增菜单项** | `Widget::initMenu()` |
| **新增配置参数** | 见 §5.2 扩展提示（7 步流程） |
| **新增食物** | `character.json → food_menu.items` 添加条目 + 对应图片加入 `imgs.qrc` |
| **新增语音** | `character.json → audio.voice_menu` 添加条目 + `.mp3` 文件加入 `audio.qrc` |
| **新增提示图** | 图片加入 `imgs.qrc` → `SpritesProfile` 添加路径字段 → `parseHints` 添加解析 → `Widget::showBottomHintTransient` 调用 |
| **新增平台特性** | `PlatformHAL`（所有 `#ifdef Q_OS_WIN` 集中于此） |
| **绘制逻辑** | `Widget::paintEvent()` |
| **新增 AI 能力** | `DeepSeekChat` 修改 system prompt / 新增工具调用 |
| **新增着色器效果** | `shaders/` 添加文件 → `shaders.qrc` 注册 → `EffectManager` 加载 |

---

## 11. Widget 内部定时器一览

| 定时器 | 用途 | 类型 |
|--------|------|------|
| `frame_timer` | 帧动画推进 | 周期 100ms |
| `idle_timer` | 空闲→随机行走 | 单次 |
| `sleep_timer` | 行走→睡眠 | 单次 |
| `sit_entry_timer` | 行走→坐下检测延迟 | 单次 |
| `sit_monitor_timer` | 坐下中窗口监控 | 周期 100ms |
| `sit_duration_timer` | 坐下持续时间 | 单次 |
| `stand_shake_timer` | 点击抖动步进 | 周期（动态间隔） |
| `click_reset_timer` | 连击计数重置 | 单次 |
| `playful_entry_timer` | 嬉戏模式检测延迟 | 单次 |
| `playful_duration_timer` | 嬉戏持续时间 | 单次 |
| `playmate_chase_timer` | 伙伴追逐物理帧 | 周期 16ms |
| `auto_sing_timer` | 自动哼歌间隔 | 单次（随机） |
| `clock_timer` | 时钟文字更新 | 周期 1s |
| `clock_display_timer` | 时钟叠层自动隐藏 | 单次 |
| `bottom_hint_timer` | 底部提示图自动隐藏 | 单次 |
| `m_hoverTimer` | 悬停→状态面板显示延迟 | 单次 |

---

## 12. widget.cpp 内部辅助函数

`widget.cpp` 文件顶部的匿名命名空间定义了 ProfileManager 快捷访问器（替代原来的 `#define` 宏）：

```cpp
namespace {
static inline const BehaviorConfig& PB()  { return ProfileManager::instance()->behavior(); }
static inline const SpriteConfig&  PS()  { return ProfileManager::instance()->sprites(); }
static inline const AudioConfig&   PAU() { return ProfileManager::instance()->audio(); }
static inline const GomokuConfig&  PG()  { return ProfileManager::instance()->gomoku(); }
}
```

调用方式：`PB().move_speed_px_per_sec`、`PS().role_display_size` 等。

---

## 13. 重构红线（绝不能破坏的特殊修复）

1. **NOMINMAX** — `platformhal.h` 中 `#include <windows.h>` 前定义 `#define NOMINMAX`
2. **边缘抖动修复** — `startRandomWalk()` 起点/终点 clamp、wave amplitude margin、每帧输出 clamp
3. **Playful/Sitting 概率触发** — 两阶段定时器：单次等待 → 周期检测，概率触发
4. **Move 动画帧逻辑** — 方向帧数量 = `paths.size()/2`，左帧区 `[0..N)`，右帧区 `[N..2N)`
5. **点击/拖拽检测** — `DragFilter` 20px 阈值区分拖拽与点击；Stand 下 500ms 抖动
6. **非模态对话框** — `openTalkInput` 和 `openSettingsDialog` 均使用 `open()` 而非 `exec()`，避免嵌套事件循环

---

## 14. 已踩过的致命坑（必读）

### 坑 1：禁止在 QApplication 之前构造 QPixmap / QIcon 等 GUI 类型

**现象**：Release 和 Debug 均崩溃，错误码 `0xC0000409`（STATUS_STACK_BUFFER_OVERRUN），模块 `Qt6Core.dll`。

**根因**：`static const QPixmap SpriteResource::s_nullPixmap;` — 静态成员在 `main()` 之前初始化，`QPixmap` 依赖 `QGuiApplication`。

**修复**：改用 Meyers 单例：
```cpp
const QPixmap& SpriteResource::nullPixmap() {
    static const QPixmap s_null;  // 惰性初始化
    return s_null;
}
```

**红线规则**：
- **严禁** `static QPixmap/QIcon/QImage/QFont` 等 GUI 类型的全局/静态类成员
- 必须使用函数内 `static local`（Meyers 单例）
- 路径常量用 `static const QString`（非 GUI 类型）

### 坑 2：GPU 呼吸特效在 Windows 下不可见

**现象**：`QOpenGLWidget` 作为 `WA_TranslucentBackground` 窗口的子控件时，GL 内容不被合成。

**修复**：改用 offscreen FBO 渲染 → `toImage()` → `Widget::paintEvent` 中 `drawImage()` 合成。

### 坑 3：OpenGL 纹理颜色通道错乱

**现象**：角色精灵在呼吸特效中显示为蓝色（R↔B 互换）。

**修复**：CPU 端 `convertToFormat(QImage::Format_RGBA8888)` + GPU 端 `GL_RGBA + GL_UNSIGNED_BYTE`。character.json 中 `force_swap_rb` 为驱动兼容性安全阀。

### 坑 4：FistWidget paintEvent 中每帧 QPixmap::scaled()

**现象**：拳头追踪动画卡顿（每 16ms 一次 SmoothTransformation 堆分配）。

**修复**：构造函数中预缩放 `m_fistScaled`，`paintEvent` 直接使用缓存版本。

---

## 15. 架构审计修复记录

| # | 严重程度 | 修复摘要 | 涉及文件 |
|---|----------|----------|----------|
| 6 | MED | `trimHistory()` O(n²)→O(n) 单次重建 | `deepseekchat.cpp` |
| 7 | MED | `ChatBubbleWidget` 传递 parent 给 QWidget 基类 | `chatbubblewidget.cpp` |
| 8 | MED | CJK 字体 fallback 抽取为 `fontutils.h` 共享工具 | `fontutils.h`(新), `foodmenuwidget.cpp`, `chatbubblewidget.cpp` |
| 9 | MED | `StatusPanel::paintEvent` QFont 缓存为成员变量 | `statuspanel.h/cpp` |
| 10 | MED | Widget `#define` 宏替换为 `static inline` 函数 | `widget.cpp` |
| 11 | MED | DeepSeekChat 网络错误时回滚孤立 user 消息 | `deepseekchat.cpp` |
| 12 | MED-HIGH | FistWidget 预缓存 scaled pixmap | `fistwidget.h/cpp` |
| 13 | MED-HIGH | openSettingsDialog `exec()`→非模态 `open()` | `widget.cpp` |
| 14 | MED | sendMessageSync 添加 orphan rollback | `deepseekchat.cpp` |
| 15 | MED | main.cpp 异常路径 flush/close 日志文件 | `main.cpp` |
| 16 | MED | API key INI 存储 XOR+Base64 混淆 | `configmanager.cpp` |

---

## 16. 构建与工具链

| 项目 | 值 |
|------|------|
| 项目根目录 | `D:\QtProject\LuminaPrayer` |
| Qt 版本 | 6.8.3 |
| Qt 安装路径 | `D:\software\Qt\6.8.3\msvc2022_64\` |
| 编译器 | Visual Studio 2022 / MSVC (x64) |
| 构建系统 | CMake + Ninja |
| Qt Creator 构建目录 (Debug) | `build/Desktop_Qt_6_8_3_MSVC2022_64bit-Debug` |
| Qt Creator 构建目录 (Release) | `build/Desktop_Qt_6_8_3_MSVC2022_64bit-Release` |
| VS2022 构建目录 | `build-vs2022` |

### CMake 依赖
```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia OpenGL Concurrent Network)
# Windows 额外链接:
target_link_libraries(LuminaPrayer PRIVATE user32)
```

### 命令行构建

```powershell
# Debug (Ninja)
$env:PATH = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"
cmd /c "`"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat`" amd64 >nul 2>&1 && ninja -C `"d:\QtProject\LuminaPrayer\build\Desktop_Qt_6_8_3_MSVC2022_64bit-Debug`""

# Release (Ninja)
cmd /c "`"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat`" amd64 >nul 2>&1 && ninja -C `"d:\QtProject\LuminaPrayer\build\Desktop_Qt_6_8_3_MSVC2022_64bit-Release`""

# VS2022 MSBuild
cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Release
```

---

## 17. 会话中断后的最短恢复路径

1. **读本文件**确认架构、文件地图、数据格式
2. 新增功能参考 §10 扩展点表格
3. 修改参数参考 §5.2 的 7 步流程
4. 碰到奇怪崩溃先看 §14 致命坑列表
5. 构建验证：`build/Desktop_Qt_6_8_3_MSVC2022_64bit-Release/LuminaPrayer.exe`
