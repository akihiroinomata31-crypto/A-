#include "main.h"
#include "DxLib.h"

#include <stdio.h> // sprintf_sを使うために必要



// 共通の描画関数（これを定義しておけばどこからでも呼べる）
void GameManager::DrawCenteredText(int x, int y, const char* label, int value, unsigned int color) {
    char buf[64];
    sprintf_s(buf, "%s: %d", label, value);
    int w = GetDrawStringWidth(buf, strlen(buf));
    DrawString(x - w / 2, y, buf, color);
}


// 更新処理
void GameManager::Update() {
    // 99秒のカウントダウン処理
    static int frameCounter = 0;
    if (++frameCounter >= 120) { // DxLibの標準は60fpsなので60で1秒です
        if (time > 0) time--;
        frameCounter = 0;
    }

    // フェーズ管理
    if (time > 54) phase = 1;
    else if (time > 24) phase = 2;
    else phase = 3;
}


// 描画処理
void GameManager::DrawUI() {
    // 共通関数を使って描画（中央起点）
    DrawCenteredText(600, 20, "TIME", time, GetColor(255, 255, 0));
    DrawCenteredText(160, 50, "P1 Score", p1Score, GetColor(255, 255, 255));
    DrawCenteredText(1000, 50, "P2 Score", p2Score, GetColor(255, 255, 255));


    
}
// プレイヤー担当者が呼ぶ関数
void GameManager::AddEvent(int pId, int type, int value) {
    eventQueue.push_back({ type, pId, value });
}

// 毎フレームのUpdateの最後などで呼ぶ処理
void GameManager::ProcessEvents() {
    for (auto& e : eventQueue) {
        if (e.type == EV_SCORE) {
            if (e.playerId == 1) p1Score += e.value;
            else p2Score += e.value;
        }
        else if (e.type == EV_DEATH) {
            if (e.playerId == 1) p1Deaths++;
            else p2Deaths++;
        }
        else if (e.type == EV_KILL) {
            if (e.playerId == 1) p1Kills++;
            else p2Kills++;
        }
    }
    eventQueue.clear();
}

