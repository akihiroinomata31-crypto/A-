#pragma once
#include "DxLib.h"
#include <vector>

struct ReplayFrame {
    VECTOR pos[2];
    int action;
};

class GameManager {
public:
    int timeLimit = 99 * 60;
    int p1Score = 0, p2Score = 0;
    std::vector<ReplayFrame> replayData;

    void Update();
    void DrawUI();

    // ƒŠƒvƒŒƒC‚ğ‹L˜^‚·‚éŠÖ”‚ğ’Ç‰Á
    void RecordFrame(VECTOR p1, VECTOR p2, int act);
};