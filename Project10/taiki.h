#pragma once
#include <vector>
#include <deque>
#include "DxLib.h"
#include "main.h"

// 出撃待ち情報（重いモデルではなく、出撃パラメータのみ保持）
struct SpawnRequest {
    VECTOR pos;
    int hp;
    int enemyType; // 0: ノーマルゴブリン, 1: 赤ゴブリン 等
};

class EnemyManager {
private:
    std::vector<int> activeIndices;        // 現在メインで稼働中のスロット番号
    std::vector<int> freeIndices;          // メイン配列の空きスロット番号
    std::deque<SpawnRequest> waitingQueue; // 詰まった敵を一時退避させる待機室

    int maxActiveLimit; // 同時出現の上限数（例: 35）

    // 内部出撃処理
    int DeployDirect(SCharaInfo* charaArray, const SpawnRequest& req);

public:
    EnemyManager();
    ~EnemyManager();

    // 初期化（管理する配列の開始インデックス・終了インデックス・同時出現上限）
    void Init(int startIndex, int endIndex, int activeLimit = 35);

    // スポーン要求（空きがあれば即出撃、満杯なら待機室へ一時退避）
    void RequestSpawn(SCharaInfo* charaArray, VECTOR spawnPos, int hp, int enemyType = 0);

    // 毎フレーム呼ぶ更新処理（メイン枠が空いていれば待機室からメイン配列へ戻す）
    void UpdateQueue(SCharaInfo* charaArray);

    // 死亡時・画面外離脱時の即時リサイクル（成仏処理）
    void Despawn(SCharaInfo* charaArray, int activeListIndex);

    // 現在稼働中の敵の総数
    int GetActiveCount() const { return (int)activeIndices.size(); }

    // 稼働中の敵の配列スロット番号を取得
    int GetActiveSlot(int index) const { return activeIndices[index]; }

    // 待機室に詰まっている敵の数
    int GetWaitingCount() const { return (int)waitingQueue.size(); }
};