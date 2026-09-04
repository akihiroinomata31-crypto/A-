#pragma once

#include "main.h"

// プレイヤー関連の固定値。
// 1P/2P のインデックスを明示して、敵と混ざらないようにする。
constexpr int PLAYER_ATTACK_ANIM_COUNT = 3;
constexpr int PLAYER_COUNT = 2;
constexpr int PLAYER1_INDEX = 0;
constexpr int PLAYER2_INDEX = 1;
constexpr int TEST_ENEMY_INDEX = 2;
constexpr float PLAYER_MODEL_SCALE = 1.1f;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT = 60;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_COLUMN_COUNT = 8;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_ROW_COUNT = 8;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_FRAME_WIDTH = 256;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_FRAME_HEIGHT = 256;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_FRAME_INTERVAL = 1;
constexpr float PLAYER_HEAVY_ATTACK_EFFECT_SIZE = 500.0f;

// 1人分の入力設定。
// キーボードとゲームパッド入力を、同じ処理で扱うためにまとめる。
struct PlayerInputConfig {
	int padType;
	int upKey;
	int downKey;
	int leftKey;
	int rightKey;
	int attackKey;
	int heavyAttackKey;
	int jumpKey;
	int attackPadButton;
	int heavyAttackPadButton;
	int jumpPadButton;
};

// 1人分の操作中状態。
// 連撃予約や攻撃ボタンの押した瞬間判定をプレイヤー別に持つ。
struct PlayerRuntimeState {
	int key = 0;
	int prevAttackButton = 0;
	int prevHeavyAttackButton = 0;
	int prevJumpButton = 0;
	int attackIndex = 0;
	int heavyAttackEffectFrame = 0;
	int heavyAttackEffectWait = 0;
	bool isAttackBuffered = false;
	bool isHeavyAttack = false;
	bool isHeavyAttackEffectPlaying = false;
	bool moveInput = false;
};

// 移動量をゼロに戻す。
void ResetMove(SCharaInfo& chara);

// 指定したアニメーションへ切り替える。
void SetCharacterAnimation(SCharaInfo& chara, int animHandle, float playtime = 0.0f);

// プレイヤーのアニメーション時間と待機復帰を更新する。
void UpdatePlayerAnimationProgress(SCharaInfo& player, PlayerRuntimeState& state, int animNeutral);

// 重攻撃エフェクトを描画する。
void DrawPlayerHeavyAttackEffect(const SCharaInfo& player, const PlayerRuntimeState& state, const int effectHandles[]);

// プレイヤーの入力、移動、待機/走り切り替え、攻撃開始を処理する。
void UpdatePlayerInput(
	SCharaInfo& player,
	PlayerRuntimeState& state,
	const PlayerInputConfig& input,
	const int animAttack[],
	int animNeutral,
	int animRun,
	int animJumpIn,
	int seAttackHandle,
	int seJumpHandle
);

// 攻撃中の移動減速と連撃遷移を処理する。
void UpdatePlayerAttackState(
	SCharaInfo& player,
	PlayerRuntimeState& state,
	const PlayerInputConfig& input,
	const int animAttack[],
	const float attackEndTime[],
	int seAttackHandle
);
