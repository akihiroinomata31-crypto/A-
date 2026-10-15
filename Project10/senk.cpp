#include "senk.h"
#include "../DXLib/DxLib.h"

namespace {
    // ステージ上の安全なリスポーン候補地点リスト
    const VECTOR RESPAWN_POINTS[] = {
        VGet(1300.0f, 800.0f,  100.0f),
        VGet(1300.0f, 800.0f, -400.0f),
        VGet(-800.0f, 800.0f,  200.0f),
        VGet(-800.0f, 800.0f, -300.0f),
        VGet(0.0f, 800.0f,  600.0f),
        VGet(0.0f, 800.0f, -600.0f)
    };
    const int RESPAWN_POINT_COUNT = sizeof(RESPAWN_POINTS) / sizeof(RESPAWN_POINTS[0]);

    // 無敵時間のフレーム数（120FPS基準で1秒間）
    const int INVINCIBLE_FRAMES = 120;
}

// ランダムなリスポーン座標を取得
VECTOR RespawnManager::GetRandomRespawnPos() {
    int r = GetRand(RESPAWN_POINT_COUNT - 1);
    return RESPAWN_POINTS[r];
}

// 初期化
void RespawnManager::Init(SCharaInfo* charainfo) {
    for (int p = 0; p < PLAYER_COUNT; ++p) {
        charainfo[p].invincibleTimer = 0;
    }
}

// 毎フレームのタイマー更新
void RespawnManager::Update(SCharaInfo* charainfo) {
    for (int p = 0; p < PLAYER_COUNT; ++p) {
        if (charainfo[p].invincibleTimer > 0) {
            charainfo[p].invincibleTimer--;
        }
    }
}

// ダウン状態からの復帰（ボタン入力検知）
void RespawnManager::CheckAndRespawnDownPlayer(
    int playerIdx,
    SCharaInfo* charainfo,
    const PlayerInputConfig& inputConfig,
    int animNeutralHandle)
{
    SCharaInfo& player = charainfo[playerIdx];

    if (player.mode != DOWNMODE) return;

    // ジャンプキーまたはパッドボタンの入力判定
    bool isPressed = false;
    if (CheckHitKey(inputConfig.jumpKey) == 1) {
        isPressed = true;
    }
    else if (ReadPlayerPadState(inputConfig.padType) & inputConfig.jumpPadButton) {
        isPressed = true;
    }

    if (isPressed) {
        player.HP = MAX_HP;
        player.mode = STAND;

        // ランダムなリスポーン位置へ移動し、移動ベクトルをゼロにする
        player.pos = GetRandomRespawnPos();
        player.move = VGet(0.0f, 0.0f, 0.0f);
        MV1SetPosition(player.model1, player.pos);

        // 1秒間の無敵タイマーを設定
        player.invincibleTimer = INVINCIBLE_FRAMES;

        // 待機アニメーションへ戻す
        if (player.attachidx != -1) {
            MV1DetachAnim(player.model1, player.attachidx);
        }
        player.attachidx = MV1AttachAnim(player.model1, 0, animNeutralHandle);
        if (player.attachidx != -1) {
            player.anim_totaltime = MV1GetAttachAnimTotalTime(player.model1, player.attachidx);
        }
        player.playtime = 0.0f;
    }
}

// 場外落下時の即時リスポーン
void RespawnManager::RespawnFallenPlayer(
    int playerIdx,
    SCharaInfo* charainfo,
    PlayerRuntimeState& playerState,
    GameManager& game,
    int animNeutralHandle)
{
    SCharaInfo& player = charainfo[playerIdx];

    // リングアウトライン（Y: -300 以下）
    if (player.pos.y >= -300.0f) return;

    player.deaths++;
    if (player.mode != DOWNMODE) {
        game.AddDeath(playerIdx);
    }

    CancelPlayerCombat(playerState);

    // ランダムなリスポーン位置へ再出現
    player.pos = GetRandomRespawnPos();
    player.move = VGet(0.0f, 0.0f, 0.0f);
    player.HP = MAX_HP;
    player.mode = STAND;
    MV1SetPosition(player.model1, player.pos);

    // 1秒間の無敵タイマーを設定
    player.invincibleTimer = INVINCIBLE_FRAMES;

    // 待機アニメーションへ戻す
    if (player.attachidx != -1) {
        MV1DetachAnim(player.model1, player.attachidx);
    }
    player.attachidx = MV1AttachAnim(player.model1, 0, animNeutralHandle);
    if (player.attachidx != -1) {
        player.anim_totaltime = MV1GetAttachAnimTotalTime(player.model1, player.attachidx);
    }
    player.playtime = 0.0f;
}

// 無敵時の点滅判定（4フレームごとに点滅）
bool RespawnManager::IsBlinkHidden(const SCharaInfo& player) {
    if (player.invincibleTimer > 0) {
        if ((player.invincibleTimer / 4) % 2 == 0) {
            return true;
        }
    }
    return false;
}