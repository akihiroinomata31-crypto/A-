#pragma once
#include "game.h"
#include "player.h"

// 復活および無敵演出を管理するクラス
class RespawnManager {
public:
    // 初期化
    static void Init(SCharaInfo* charainfo);

    // 毎フレームの無敵タイマー更新
    static void Update(SCharaInfo* charainfo);

    // ダウン状態からの復活入力判定と実行
    static void CheckAndRespawnDownPlayer(
        int playerIdx,
        SCharaInfo* charainfo,
        const PlayerInputConfig& inputConfig,
        int animNeutralHandle
    );

    // 落下（リングアウト）時の即時復活
    static void RespawnFallenPlayer(
        int playerIdx,
        SCharaInfo* charainfo,
        PlayerRuntimeState& playerState,
        GameManager& game,
        int animNeutralHandle
    );

    // 無敵時間中の点滅描画判定（描画を非表示にすべきフレームなら true）
    static bool IsBlinkHidden(const SCharaInfo& player);

    // 安全なリスポーン座標をランダムに取得
    static VECTOR GetRandomRespawnPos();
};