#pragma once

#include "main.h"

// プレイヤー関連の固定値。
// 1P/2P のインデックスを明示して、敵と混ざらないようにする。
constexpr int PLAYER_ATTACK_ANIM_COUNT = 3;
constexpr int PLAYER_COUNT = 2;
constexpr int PLAYER1_INDEX = 0;
constexpr int PLAYER2_INDEX = 1;
constexpr int TEST_ENEMY_INDEX = 2;
constexpr int TEST_ENEMY_GOLEM = 3;
constexpr int TEST_ENEMY_RED = 500;
// Temporary new-character trial; false restores the original asset set.
constexpr bool PLAYER_USE_NEW_MODEL = true;
constexpr float PLAYER_MODEL_SCALE = 1.1f;
constexpr float PLAYER_NORMAL_ATTACK_EFFECT_MAGNIFICATION = 40.0f;
constexpr float PLAYER_NORMAL_ATTACK_EFFECT_PLAYBACK_SPEED = 2.0f / 3.0f;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT = 60;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_COLUMN_COUNT = 8;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_ROW_COUNT = 8;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_FRAME_WIDTH = 256;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_FRAME_HEIGHT = 256;
constexpr int PLAYER_HEAVY_ATTACK_EFFECT_FRAME_INTERVAL = 1;
constexpr float PLAYER_HEAVY_ATTACK_EFFECT_SIZE = 500.0f;
constexpr float PLAYER_WHIRLWIND_PULL_RADIUS = 300.0f;
constexpr float PLAYER_WHIRLWIND_PULL_SPEED = 12.0f;
constexpr float PLAYER_WHIRLWIND_PULL_STOP_DISTANCE = 60.0f;
constexpr float PLAYER_WHIRLWIND_PULL_HEIGHT = 160.0f;

// SwordMega runs at 60 Hz; the last frames are visual recovery only.
constexpr int PLAYER_SPECIAL_ATTACK_EFFECT_FRAME_COUNT = 240;
constexpr int PLAYER_SPECIAL_ATTACK_EFFECT_FRAME_INTERVAL = 1;
constexpr float PLAYER_SPECIAL_ATTACK_EFFECT_MAGNIFICATION = 32.0f;
// Compensate the source SwordContainer pivot (X=2, Y=-4).
constexpr float PLAYER_SPECIAL_ATTACK_EFFECT_FORWARD_OFFSET = 2.0f * PLAYER_SPECIAL_ATTACK_EFFECT_MAGNIFICATION;
constexpr float PLAYER_SPECIAL_ATTACK_EFFECT_HEIGHT_OFFSET = 4.0f * PLAYER_SPECIAL_ATTACK_EFFECT_MAGNIFICATION;
constexpr int PLAYER_SPECIAL_ATTACK_ACTIVE_START_FRAME = 40;
constexpr int PLAYER_SPECIAL_ATTACK_FULL_RADIUS_FRAME = 100;
constexpr int PLAYER_SPECIAL_ATTACK_ACTIVE_END_FRAME = 165;
constexpr float PLAYER_SPECIAL_ATTACK_MAX_RADIUS = 336.0f;
// 現在のテスト用HPでは通常攻撃1回を1としているため、必殺技300は2ダメージに換算する。
constexpr int PLAYER_SPECIAL_ATTACK_DAMAGE = 2;

const int MAX_HP = 6;
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
	int specialAttackKey;
	int jumpKey;
	int attackPadButton;
	int heavyAttackPadButton;
	int specialAttackPadButton;
	int jumpPadButton;
};

// 1人分の操作中状態。
// 連撃予約や攻撃ボタンの押した瞬間判定をプレイヤー別に持つ。
struct PlayerRuntimeState {
	int movementAnimation = -1;
	int motionRootFrame = -1;
	float motionRootX = 0.0f;
	float motionRootZ = 0.0f;
	int key = 0;
	int prevAttackButton = 0;
	int prevHeavyAttackButton = 0;
	int prevSpecialAttackButton = 0;
	int prevJumpButton = 0;
	int attackIndex = 0;
	int normalAttackEffectResourceHandle = -1;
	int normalAttackPlayingHandle = -1;
	int heavyAttackEffectFrame = 0;
	int heavyAttackEffectWait = 0;
	int specialAttackEffectFrame = 0;
	int specialAttackEffectWait = 0;
	int specialAttackPlayingHandle = -1;
	bool isAttackBuffered = false;
	bool isHeavyAttack = false;
	bool isSpecialAttack = false;
	bool isNormalAttackEffectPlaying = false;
	bool isHeavyAttackEffectPlaying = false;
	bool isSpecialAttackEffectPlaying = false;
	bool specialHitTargets[MAX_CHARA] = {};
	bool moveInput = false;
};

// 移動量をゼロに戻す。
void ResetMove(SCharaInfo& chara);

// 指定したアニメーションへ切り替える。
void SetCharacterAnimation(SCharaInfo& chara, int animHandle, float playtime = 0.0f);

// プレイヤーのアニメーション時間と待機復帰を更新する。
void UpdatePlayerAnimationProgress(SCharaInfo& player, PlayerRuntimeState& state, int animNeutral);

// 通常攻撃の1段目と2段目に斬撃エフェクトを描画する。
void PlayPlayerNormalAttackEffect(const SCharaInfo& player, PlayerRuntimeState& state);

// 重攻撃エフェクトを描画する。
void DrawPlayerHeavyAttackEffect(const SCharaInfo& player, const PlayerRuntimeState& state, const int effectHandles[]);

// 必殺技の水波が広がる時間と半径に合わせて敵への命中を判定する。
void CheckPlayerSpecialAttackHit(
	GameManager& game,
	SCharaInfo& attacker,
	PlayerRuntimeState& state,
	SCharaInfo& target,
	int seDamageHandle,
	int animDamage,
	int playerIndex,
	int targetIndex
);

// プレイヤーの入力、移動、待機/走り切り替え、攻撃開始を処理する。
void UpdatePlayerInput(
	SCharaInfo& player,
	PlayerRuntimeState& state,
	const PlayerInputConfig& input,
	const int animAttack[],
	int animHeavyAttack,
	int animSpecialAttack,
	int animNeutral,
	int animRun,
	int animJumpIn,
	int seAttackHandle,
	int seJumpHandle,
	int specialAttackEffectResourceHandle
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
// Keep animation root translation aligned with the gameplay position.
void ApplyPlayerMotionRoot(SCharaInfo& player, const PlayerRuntimeState& state);

// Apply a movement vector and face its direction using the running animation.
void UpdatePlayerMovement(SCharaInfo& player, PlayerRuntimeState& state,
	float inputX, float inputZ, int animRun);

// Select the nearest active whirlwind; each enemy moves toward one player only.
int FindWhirlwindPullPlayer(const SCharaInfo players[], const PlayerRuntimeState states[], const SCharaInfo& target);
void UpdateWhirlwindPull(const SCharaInfo players[], const PlayerRuntimeState states[], SCharaInfo& target);
// Switch locomotion once, preserving animation time while movement continues.
void UpdateEnemyLocomotionAnimation(SCharaInfo& enemy, bool moving);

int ReadPlayerPadState(int padType);

void CancelPlayerCombat(PlayerRuntimeState& state);
