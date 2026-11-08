#include "taiki.h"

EnemyManager::EnemyManager() : maxActiveLimit(35) {
}

EnemyManager::~EnemyManager() {
    activeIndices.clear();
    freeIndices.clear();
    waitingQueue.clear();
}

void EnemyManager::Init(int startIndex, int endIndex, int activeLimit) {
    activeIndices.clear();
    freeIndices.clear();
    waitingQueue.clear();
    maxActiveLimit = activeLimit;

    // 使えるスロット番号をすべて空きリストに登録
    for (int i = startIndex; i < endIndex; i++) {
        freeIndices.push_back(i);
    }
}

void EnemyManager::RequestSpawn(SCharaInfo* charaArray, VECTOR spawnPos, int hp, int enemyType) {
    SpawnRequest req = { spawnPos, hp, enemyType };

    // メイン枠に空きがあり、同時出現上限未満なら即時出撃
    if ((int)activeIndices.size() < maxActiveLimit && !freeIndices.empty()) {
        DeployDirect(charaArray, req);
    }
    else {
        // 満杯で出撃できない場合は待機室へ一時退避（溜まりすぎ防止の上限20体）
        if (waitingQueue.size() < 20) {
            waitingQueue.push_back(req);
        }
    }
}

void EnemyManager::UpdateQueue(SCharaInfo* charaArray) {
    // メイン枠に空きができ、かつ待機室に敵がいればメイン配列に戻す
    while ((int)activeIndices.size() < maxActiveLimit && !freeIndices.empty() && !waitingQueue.empty()) {
        SpawnRequest nextReq = waitingQueue.front();
        waitingQueue.pop_front();

        DeployDirect(charaArray, nextReq);
    }
}

int EnemyManager::DeployDirect(SCharaInfo* charaArray, const SpawnRequest& req) {
    int slot = freeIndices.back();
    freeIndices.pop_back();
    activeIndices.push_back(slot);

    SCharaInfo& e = charaArray[slot];
    e.mode = STAND;
    e.pos = req.pos;
    e.move = VGet(0.0f, 0.0f, 0.0f);
    e.enemyHP = req.hp;
    e.playtime = 0.0f;
    e.isHit = false;
    e.currentAnimType = 1;

    if (e.model1 != -1) {
        MV1SetPosition(e.model1, e.pos);
        MV1SetVisible(e.model1, TRUE);
        if (e.attachidx != -1) {
            MV1SetAttachAnimTime(e.model1, e.attachidx, 0.0f);
        }
    }

    return slot;
}

void EnemyManager::Despawn(SCharaInfo* charaArray, int activeListIndex) {
    if (activeListIndex < 0 || activeListIndex >= (int)activeIndices.size()) return;

    int slot = activeIndices[activeListIndex];

    // データを完全に初期化して画面外へ退避
    SCharaInfo& e = charaArray[slot];
    e.mode = NONE;
    e.enemyHP = 0;
    e.pos = VGet(-99999.0f, -99999.0f, -99999.0f);

    if (e.model1 != -1) {
        MV1SetVisible(e.model1, FALSE);
        MV1SetPosition(e.model1, e.pos);
    }

    // 空きリストへ返却
    freeIndices.push_back(slot);

    // アクティブリストから O(1) で高速削除（末尾と入れ替えて pop）
    activeIndices[activeListIndex] = activeIndices.back();
    activeIndices.pop_back();
}