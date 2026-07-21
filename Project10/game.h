#pragma once
#include "DxLib.h"
#include <vector>
struct SCharaInfo;
struct ReplayFrame {
    VECTOR pos[5];
    int action;
};

class GameManager {
public:

    int timeLimit = 99 * 100;
    int p1Score = 0, p2Score = 0;
    std::vector<ReplayFrame> replayData;
    int spawnTimer = 0;
    //void Update();
    void DrawUI();
    int deathCount = 0; // €‚ñ‚¾‰ñ”
    void AddDeath() { deathCount++; }
    //int p1Score = 0;

    void AddScore(int score);
   
    // ƒŠƒvƒŒƒC‚ğ‹L˜^‚·‚éŠÖ”‚ğ’Ç‰Á
    void RecordFrame(VECTOR p1, VECTOR p2, int act);
    void Update(SCharaInfo* enemyList);
    void ActivateEnemy(SCharaInfo* enemyList, float x, float z);
};