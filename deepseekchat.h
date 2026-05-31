#ifndef DEEPSEEKCHAT_H
#define DEEPSEEKCHAT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class DeepSeekChat : public QObject {
    Q_OBJECT

public:
    explicit DeepSeekChat(QObject *parent = nullptr);
    ~DeepSeekChat() override;

    void setApiKey(const QString &key);
    void setModel(const QString &model);
    void setSystemPrompt(const QString &prompt);
    void setTemperature(double t);
    void setMaxTokens(int tokens);
    void setTimeoutMs(int ms);
    void setMaxHistory(int n);
    void clearHistory();

    QString apiKey()   const { return m_apiKey; }
    QString model()    const { return m_model; }

    void sendMessage(const QString &message);

    QString sendMessageSync(const QString &message);
    QString lastError() const { return m_lastError; }

signals:
    void responseReady(const QString &response);
    void errorOccurred(const QString &error);

private:
    QNetworkRequest buildRequest() const;
    QByteArray     buildBody() const;
    QJsonObject    parseResponse(const QByteArray &data);
    void           ensureSystemPrompt();
    void           trimHistory();

    QNetworkAccessManager *m_nam;
    QString m_apiKey;
    QString m_model;
    QString m_systemPrompt;
    QString m_apiUrl;
    QString m_lastError;
    double  m_temperature;
    int     m_maxTokens;
    int     m_timeoutMs;

    QJsonArray m_history;          // sliding-window conversation pool
    int m_maxHistory;               // 1 system + N user/assistant msgs
};

#endif // DEEPSEEKCHAT_H
