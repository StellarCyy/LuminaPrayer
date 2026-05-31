#ifndef FOODMENUWIDGET_H
#define FOODMENUWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPixmap>
#include <QTimer>

class QScrollArea;

struct FoodItem {
    QString name;
    QString description;
    QString imagePath;   // qrc path
    QPixmap pixmap;
    // Stat effects: [Happiness, Interest, Sanity, Satiety, Affection]
    int effects[5] = {0, 0, 0, 0, 0};
};

// Inner widget that paints food cards in a flow layout
class FoodGridWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FoodGridWidget(const QVector<FoodItem> &foods, QWidget *parent = nullptr);

    void reflowCards(int viewportWidth);
    void flashCard(int index);

signals:
    void foodClicked(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    const QVector<FoodItem> &m_foods;

    // Computed layout
    int m_cols = 1;
    int m_marginLeft = 0;  // centering offset

    // Flash animation
    int    m_flashIndex = -1;
    int    m_flashStep  = 0;
    QTimer m_flashTimer;

    QFont m_serifFont;
    QFont m_nameFont;

    // Layout constants
    static constexpr int ImgSize     = 200;
    static constexpr int CardW       = 220;
    static constexpr int CardH       = 280;
    static constexpr int GridSpacing = 16;
    static constexpr int PadX        = 16;

    QRect cardRect(int index) const;
};

// Top-level window with title bar, scroll area, standard controls
class FoodMenuWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FoodMenuWidget(QWidget *parent = nullptr);

    void showCentered();

    const QVector<FoodItem> &foods() const { return m_foods; }

signals:
    void foodSelected(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void buildFoodList();

    QVector<FoodItem> m_foods;
    QScrollArea    *m_scrollArea;
    FoodGridWidget *m_gridWidget;
};

#endif // FOODMENUWIDGET_H
