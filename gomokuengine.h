#ifndef GOMOKUENGINE_H
#define GOMOKUENGINE_H

#include <QObject>
#include <QPoint>
#include <QVector>
#include <QReadWriteLock>

class GomokuEngine : public QObject
{
    Q_OBJECT
public:
    enum Cell { Empty = 0, Black = 1, White = 2 };
    enum GameResult { InProgress, BlackWins, WhiteWins, Draw };

    explicit GomokuEngine(int boardSize = 15, QObject *parent = nullptr);

    void reset(int boardSize);
    int boardSize() const { return m_size; }
    Cell cellAt(int row, int col) const;
    bool isEmpty(int row, int col) const;

    // Returns true if placement succeeded
    bool placePiece(int row, int col, Cell piece);

    // AI: compute best move for given piece color
    QPoint computeAIMove(Cell aiPiece) const;

    GameResult lastResult() const { return m_result; }
    QVector<QPoint> winningLine() const { return m_winLine; }
    bool isBoardFull() const;
    int moveCount() const { return m_moveCount; }

private:
    int countDir(int row, int col, int dr, int dc, Cell piece) const;
    bool checkWinAt(int row, int col, Cell piece);
    void findWinLine(int row, int col, Cell piece);
    int scoreCellForPiece(int row, int col, Cell piece) const;
    int evaluateLine(int count, int openEnds) const;

    int m_size;
    QVector<QVector<Cell>> m_board;
    QVector<QPoint> m_winLine;
    GameResult m_result;
    int m_moveCount;
    mutable QReadWriteLock m_lock;  // H-02: protects m_board during concurrent AI read
};

#endif // GOMOKUENGINE_H
