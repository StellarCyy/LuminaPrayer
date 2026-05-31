#include "gomokuengine.h"
#include <QReadWriteLock>
#include <QRandomGenerator>
#include <algorithm>

GomokuEngine::GomokuEngine(int boardSize, QObject *parent)
    : QObject(parent), m_size(boardSize), m_result(InProgress), m_moveCount(0)
{
    reset(boardSize);
}

void GomokuEngine::reset(int boardSize) {
    m_size = boardSize;
    m_board.resize(m_size);
    for (int i = 0; i < m_size; i++) {
        m_board[i].fill(Empty, m_size);
    }
    m_winLine.clear();
    m_result = InProgress;
    m_moveCount = 0;
}

GomokuEngine::Cell GomokuEngine::cellAt(int row, int col) const {
    if (row < 0 || row >= m_size || col < 0 || col >= m_size)
        return Empty;
    return m_board[row][col];
}

bool GomokuEngine::isEmpty(int row, int col) const {
    return cellAt(row, col) == Empty;
}

bool GomokuEngine::placePiece(int row, int col, Cell piece) {
    QWriteLocker locker(&m_lock);
    if (m_result != InProgress) return false;
    if (row < 0 || row >= m_size || col < 0 || col >= m_size) return false;
    if (m_board[row][col] != Empty) return false;

    m_board[row][col] = piece;
    m_moveCount++;

    if (checkWinAt(row, col, piece)) {
        findWinLine(row, col, piece);
        m_result = (piece == Black) ? BlackWins : WhiteWins;
    } else if (isBoardFull()) {
        m_result = Draw;
    }
    return true;
}

int GomokuEngine::countDir(int row, int col, int dr, int dc, Cell piece) const {
    int count = 0;
    int r = row + dr, c = col + dc;
    while (r >= 0 && r < m_size && c >= 0 && c < m_size && m_board[r][c] == piece) {
        count++;
        r += dr;
        c += dc;
    }
    return count;
}

bool GomokuEngine::checkWinAt(int row, int col, Cell piece) {
    static const int dr[] = {0, 1, 1, 1};
    static const int dc[] = {1, 0, 1, -1};
    for (int d = 0; d < 4; d++) {
        int total = 1 + countDir(row, col, dr[d], dc[d], piece)
                      + countDir(row, col, -dr[d], -dc[d], piece);
        if (total >= 5) return true;
    }
    return false;
}

void GomokuEngine::findWinLine(int row, int col, Cell piece) {
    static const int dr[] = {0, 1, 1, 1};
    static const int dc[] = {1, 0, 1, -1};
    m_winLine.clear();

    for (int d = 0; d < 4; d++) {
        int total = 1 + countDir(row, col, dr[d], dc[d], piece)
                      + countDir(row, col, -dr[d], -dc[d], piece);
        if (total >= 5) {
            // Walk backward to find start of the line
            int r = row, c = col;
            while (r - dr[d] >= 0 && r - dr[d] < m_size &&
                   c - dc[d] >= 0 && c - dc[d] < m_size &&
                   m_board[r - dr[d]][c - dc[d]] == piece) {
                r -= dr[d];
                c -= dc[d];
            }
            // Walk forward collecting all pieces in line
            while (r >= 0 && r < m_size && c >= 0 && c < m_size && m_board[r][c] == piece) {
                m_winLine.append(QPoint(r, c));
                r += dr[d];
                c += dc[d];
            }
            return;
        }
    }
}

int GomokuEngine::evaluateLine(int count, int openEnds) const {
    if (count >= 5) return 1000000;
    if (openEnds == 0) return 0;

    switch (count) {
    case 4: return (openEnds == 2) ? 100000 : 10000;
    case 3: return (openEnds == 2) ? 10000  : 1000;
    case 2: return (openEnds == 2) ? 500    : 100;
    case 1: return (openEnds == 2) ? 10     : 1;
    default: return 0;
    }
}

int GomokuEngine::scoreCellForPiece(int row, int col, Cell piece) const {
    if (m_board[row][col] != Empty) return -1;

    static const int dr[] = {0, 1, 1, 1};
    static const int dc[] = {1, 0, 1, -1};
    int totalScore = 0;

    for (int d = 0; d < 4; d++) {
        int countPos = 0, countNeg = 0;
        int openEnds = 0;

        // Positive direction
        int r = row + dr[d], c = col + dc[d];
        while (r >= 0 && r < m_size && c >= 0 && c < m_size && m_board[r][c] == piece) {
            countPos++;
            r += dr[d];
            c += dc[d];
        }
        if (r >= 0 && r < m_size && c >= 0 && c < m_size && m_board[r][c] == Empty)
            openEnds++;

        // Negative direction
        r = row - dr[d]; c = col - dc[d];
        while (r >= 0 && r < m_size && c >= 0 && c < m_size && m_board[r][c] == piece) {
            countNeg++;
            r -= dr[d];
            c -= dc[d];
        }
        if (r >= 0 && r < m_size && c >= 0 && c < m_size && m_board[r][c] == Empty)
            openEnds++;

        int lineCount = 1 + countPos + countNeg;
        totalScore += evaluateLine(lineCount, openEnds);
    }
    return totalScore;
}

QPoint GomokuEngine::computeAIMove(Cell aiPiece) const {
    QReadLocker locker(&m_lock);
    Cell opponent = (aiPiece == Black) ? White : Black;

    // First move: play near center
    if (m_moveCount == 0) {
        return QPoint(m_size / 2, m_size / 2);
    }
    // Second move: play adjacent to center if center taken
    if (m_moveCount == 1 && m_board[m_size / 2][m_size / 2] != Empty) {
        int off = QRandomGenerator::global()->bounded(2) == 0 ? -1 : 1;
        return QPoint(m_size / 2 + off, m_size / 2 + off);
    }

    int bestScore = -1;
    QVector<QPoint> bestMoves;

    for (int r = 0; r < m_size; r++) {
        for (int c = 0; c < m_size; c++) {
            if (m_board[r][c] != Empty) continue;

            // Only consider cells near existing pieces (2-cell radius)
            bool nearPiece = false;
            for (int dr = -2; dr <= 2 && !nearPiece; dr++) {
                for (int dc = -2; dc <= 2 && !nearPiece; dc++) {
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < m_size && nc >= 0 && nc < m_size
                        && m_board[nr][nc] != Empty)
                        nearPiece = true;
                }
            }
            if (!nearPiece) continue;

            int attackScore  = scoreCellForPiece(r, c, aiPiece);
            int defenseScore = scoreCellForPiece(r, c, opponent);
            int totalScore   = attackScore + static_cast<int>(defenseScore * 0.95);

            // Immediate win: highest priority
            if (attackScore >= 1000000)
                totalScore = 10000000;
            // Block opponent's immediate win
            else if (defenseScore >= 1000000)
                totalScore = 5000000;

            if (totalScore > bestScore) {
                bestScore = totalScore;
                bestMoves.clear();
                bestMoves.append(QPoint(r, c));
            } else if (totalScore == bestScore) {
                bestMoves.append(QPoint(r, c));
            }
        }
    }

    if (bestMoves.isEmpty()) {
        // Fallback: find any empty cell
        for (int r = 0; r < m_size; r++)
            for (int c = 0; c < m_size; c++)
                if (m_board[r][c] == Empty)
                    return QPoint(r, c);
        return QPoint(-1, -1);
    }

    return bestMoves[QRandomGenerator::global()->bounded(bestMoves.size())];
}

bool GomokuEngine::isBoardFull() const {
    for (int r = 0; r < m_size; r++)
        for (int c = 0; c < m_size; c++)
            if (m_board[r][c] == Empty) return false;
    return true;
}
