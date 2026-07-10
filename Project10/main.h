
#pragma once // 多重インクルード防止
#include "DxLib.h"
#include <vector>


#define EV_SCORE 0
#define EV_DEATH 1
#define EV_KILL  2

struct GameEvent {
    int type;
    int playerId; // どのプレイヤーが
    int value;   // 何点取ったか
};
class GameManager {

private:
    std::vector<GameEvent> eventQueue;
public:
    //スコア
    int time = 99;
    int p1Score = 0, p2Score = 0;
    // P1, P2それぞれのステータス
    int p1Score = 0, p2Score = 0;
    int p1Deaths = 0, p2Deaths = 0; // ライフ
    int p1Kills = 0, p2Kills = 0; // キル数



    int phase = 1;





    // プレイヤー担当者が呼ぶ関数
    void AddEvent(int pId,int value,int pts);
    // イベントを処理する関数
    void ProcessEvents();

    void Update();
    void DrawUI();
    void DrawCenteredText(int x, int y, const char* label, int value, unsigned int color);
};
