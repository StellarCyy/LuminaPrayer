#include "deepseekchat.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QThread>
#include <QCoreApplication>
#include <QEventLoop>
#include <QUrl>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

DeepSeekChat::DeepSeekChat(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_apiKey()
    , m_model(QStringLiteral("deepseek-v4-pro"))
    , m_apiUrl(QStringLiteral("https://api.deepseek.com/v1/chat/completions"))
    , m_temperature(0.8)
    , m_maxTokens(4096)
    , m_timeoutMs(120000)
    , m_maxHistory(51)
{
}

DeepSeekChat::~DeepSeekChat() = default;

// ── 配置 ──

void DeepSeekChat::setApiKey(const QString &key)      { m_apiKey = key; }
void DeepSeekChat::setModel(const QString &model)      { m_model = model; }
void DeepSeekChat::setTemperature(double t)            { m_temperature = t; }
void DeepSeekChat::setMaxTokens(int tokens)            { m_maxTokens = tokens; }
void DeepSeekChat::setTimeoutMs(int ms)                { m_timeoutMs = ms; }
void DeepSeekChat::setMaxHistory(int n)                { m_maxHistory = qMax(3, n); }

void DeepSeekChat::setSystemPrompt(const QString &p) {
    if (m_systemPrompt == p) return;   // no change → keep history intact
    m_systemPrompt = p;
    // Reset history with the new system prompt locked at index 0
    m_history = QJsonArray();
    if (!p.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg[QStringLiteral("role")]    = QStringLiteral("system");
        sysMsg[QStringLiteral("content")] = p;
        m_history.append(sysMsg);
    }
}

void DeepSeekChat::clearHistory() {
    m_history = QJsonArray();
    ensureSystemPrompt();
}

// ── 会话记忆维护 ──

void DeepSeekChat::ensureSystemPrompt() {
    // Guarantee index 0 is always the system prompt
    if (m_systemPrompt.isEmpty()) return;
    if (m_history.isEmpty() ||
        m_history[0].toObject()[QStringLiteral("role")].toString() != QStringLiteral("system")) {
        QJsonObject sysMsg;
        sysMsg[QStringLiteral("role")]    = QStringLiteral("system");
        sysMsg[QStringLiteral("content")] = m_systemPrompt;
        m_history.prepend(sysMsg);
    }
}

void DeepSeekChat::trimHistory() {
    // O(n) rebuild: keep system prompt [0] + newest tail within budget.
    if (m_history.size() <= m_maxHistory) return;

    QJsonArray trimmed;
    // Protect system prompt at index 0
    if (!m_history.isEmpty() &&
        m_history[0].toObject()[QStringLiteral("role")].toString() == QStringLiteral("system")) {
        trimmed.append(m_history[0]);
    }
    // Keep only the newest entries that fit within budget
    const int keep = m_maxHistory - trimmed.size();
    const int start = m_history.size() - keep;
    for (int i = qMax(start, trimmed.size()); i < m_history.size(); ++i)
        trimmed.append(m_history[i]);

    m_history = trimmed;
}

// ── 构建 HTTP 请求 ──

QNetworkRequest DeepSeekChat::buildRequest() const
{
    QNetworkRequest req{QUrl(m_apiUrl)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    return req;
}

QByteArray DeepSeekChat::buildBody() const
{
    QJsonObject body;
    body[QStringLiteral("model")]       = m_model;
    body[QStringLiteral("messages")]    = m_history;
    body[QStringLiteral("max_tokens")]  = m_maxTokens;
    body[QStringLiteral("stream")]      = false;

    // ── 思考模式 ──
    QJsonObject thinking;
    thinking[QStringLiteral("type")] = QStringLiteral("enabled");
    body[QStringLiteral("thinking")] = thinking;
    body[QStringLiteral("reasoning_effort")] = QStringLiteral("high");

    // 思考模式下 temperature/top_p 等参数不生效，但传入不报错
    // 保留 temperature 以兼容非思考模式模型切换
    body[QStringLiteral("temperature")] = m_temperature;

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

// ── 解析响应 ──
// 返回完整的 assistant message JSON object（包含 reasoning_content）
// 思考模式下必须保留 reasoning_content 并在下一轮请求中传回，否则 API 返回 400

QJsonObject DeepSeekChat::parseResponse(const QByteArray &data)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError) {
        m_lastError = QStringLiteral("JSON parse error: ") + err.errorString();
        return {};
    }

    const QJsonObject root = doc.object();

    // 检查 API 错误
    if (root.contains("error")) {
        const QJsonObject errObj = root["error"].toObject();
        m_lastError = QStringLiteral("API error: ") +
                      errObj["message"].toString(QStringLiteral("unknown"));
        return {};
    }

    // 提取 choices[0].message（完整对象，包含 content + reasoning_content）
    const QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) {
        m_lastError = QStringLiteral("No choices in response");
        return {};
    }

    const QJsonObject choice   = choices[0].toObject();
    const QJsonObject message  = choice["message"].toObject();
    const QString content      = message["content"].toString();

    if (content.isEmpty()) {
        m_lastError = QStringLiteral("Empty content in response");
    }

    // 返回完整 message 对象，调用方会直接存入历史以保留 reasoning_content
    return message;
}

// ── 异步发送 ──

void DeepSeekChat::sendMessage(const QString &message)
{
    m_lastError.clear();

    if (m_apiKey.isEmpty()) {
        emit errorOccurred(QStringLiteral("API Key 未设置"));
        return;
    }

    // 发送前：追加用户消息到记忆池
    ensureSystemPrompt();
    QJsonObject userMsg;
    userMsg[QStringLiteral("role")]    = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = message;
    m_history.append(userMsg);
    trimHistory();

    QNetworkReply *reply = m_nam->post(buildRequest(), buildBody());

    // 超时处理
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        reply->abort();
    });
    timer->start(m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this, [this, reply, timer]() {
        timer->stop();
        timer->deleteLater();
        reply->deleteLater();

        // Helper: pop orphaned user message on any error path
        auto rollbackUserMsg = [this]() {
            if (m_history.size() > 0 &&
                m_history.last().toObject()[QStringLiteral("role")].toString() == QStringLiteral("user")) {
                m_history.removeLast();
            }
        };

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                m_lastError = QStringLiteral("TIMEOUT");
            } else {
                const int httpCode = reply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
                m_lastError = (httpCode > 0)
                    ? QStringLiteral("HTTP_%1").arg(httpCode)
                    : QStringLiteral("NET_%1").arg(reply->errorString());
            }
            rollbackUserMsg();
            emit errorOccurred(m_lastError);
            return;
        }

        // Check for API-level errors even on HTTP 200
        const int httpStatus = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus != 200 && httpStatus != 0) {
            m_lastError = QStringLiteral("HTTP_%1").arg(httpStatus);
            rollbackUserMsg();
            emit errorOccurred(m_lastError);
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject msgObj = parseResponse(data);
        const QString result = msgObj["content"].toString();

        if (result.isEmpty() && !m_lastError.isEmpty()) {
            rollbackUserMsg();
            emit errorOccurred(m_lastError);
        } else {
            // 返回后：追加完整 AI 回复到记忆池（保留 reasoning_content）
            m_history.append(msgObj);
            trimHistory();

            emit responseReady(result);
        }
    });
}

// ── 同步发送（阻塞直到返回，仅用于工作线程） ──

QString DeepSeekChat::sendMessageSync(const QString &message)
{
    // H-01: Guard against GUI-thread re-entrancy — QEventLoop::exec() on the
    // GUI thread would fire all timers/paint/signal handlers during the wait,
    // silently corrupting Widget state machines.
    Q_ASSERT_X(QThread::currentThread() != QCoreApplication::instance()->thread(),
               "DeepSeekChat::sendMessageSync",
               "Must NOT be called from the GUI thread (nested QEventLoop danger)");

    m_lastError.clear();

    if (m_apiKey.isEmpty()) {
        m_lastError = QStringLiteral("API Key 未设置");
        return {};
    }

    // 发送前：追加用户消息到记忆池
    ensureSystemPrompt();
    QJsonObject userMsg;
    userMsg[QStringLiteral("role")]    = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = message;
    m_history.append(userMsg);
    trimHistory();

    // 使用独立的 QNetworkAccessManager（不设 parent，手动管理生命周期）
    QNetworkAccessManager nam;
    QNetworkReply *reply = nam.post(buildRequest(), buildBody());

    // 用 QEventLoop 等待网络返回
    QEventLoop loop;

    // 超时
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, [&]() {
        reply->abort();
        loop.quit();
    });
    timer.start(m_timeoutMs);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // 阻塞当前线程，直到 finished 或 timeout
    loop.exec();

    timer.stop();

    // MED-14: Helper to pop orphaned user message on error (same as async path)
    auto rollbackUserMsg = [this]() {
        if (m_history.size() > 0 &&
            m_history.last().toObject()[QStringLiteral("role")].toString() == QStringLiteral("user")) {
            m_history.removeLast();
        }
    };

    if (reply->error() != QNetworkReply::NoError) {
        m_lastError = (reply->error() == QNetworkReply::OperationCanceledError)
            ? QStringLiteral("请求超时（%1 ms）").arg(m_timeoutMs)
            : reply->errorString();
        reply->deleteLater();
        rollbackUserMsg();
        return {};
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    const QJsonObject msgObj = parseResponse(data);
    const QString result = msgObj["content"].toString();
    if (result.isEmpty() && !m_lastError.isEmpty()) {
        rollbackUserMsg();
    } else if (!result.isEmpty()) {
        // 返回后：追加完整 AI 回复到记忆池（保留 reasoning_content）
        m_history.append(msgObj);
        trimHistory();
    }
    return result;
}

