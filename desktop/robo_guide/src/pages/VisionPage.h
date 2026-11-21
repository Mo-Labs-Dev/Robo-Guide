#pragma once

#include <QVector>
#include <QWidget>

#include "AppContext.h"

class QLabel;
class QPushButton;

class VisionPage : public QWidget {
    Q_OBJECT
public:
    explicit VisionPage(const AppContext& ctx, QWidget* parent = nullptr);

private:
    struct Tile {
        QLabel* image = nullptr;
        QLabel* state = nullptr;
        QPushButton* toggle = nullptr;
    };
    void toggleCamera(int index);
    void snapshotAll();
    void toggleRecording();

    AppContext ctx_;
    QVector<Tile> tiles_;
    QPushButton* record_ = nullptr;
};
