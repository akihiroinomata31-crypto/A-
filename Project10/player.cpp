#include "player.h"

#include <DxLib.h>
#include <math.h>

namespace {

bool IsKeyDown(int keyCode) {
	return keyCode >= 0 && CheckHitKey(keyCode) != 0;
}

bool CanControlPlayerMode(int mode) {
	return mode == STAND || mode == RUN || mode == ATTACK || mode == ATTACKOUT;
}

constexpr int PAD_ANALOG_DEAD_ZONE = 400;

bool IsPadButtonDown(int padState, int padButton) {
	return padButton != 0 && (padState & padButton) != 0;
}

bool IsPadOrKeyDown(int padState, int padButton, int keyCode) {
	return IsPadButtonDown(padState, padButton) || IsKeyDown(keyCode);
}

void GetMoveInput(const PlayerRuntimeState& state, const PlayerInputConfig& input, float& inputX, float& inputZ) {
	// 方向キー、十字キー、左スティックをまとめて移動入力に変換する。
	bool moveDown = IsPadButtonDown(state.key, PAD_INPUT_DOWN) || IsKeyDown(input.downKey);
	bool moveUp = IsPadButtonDown(state.key, PAD_INPUT_UP) || IsKeyDown(input.upKey);
	bool moveLeft = IsPadButtonDown(state.key, PAD_INPUT_LEFT) || IsKeyDown(input.leftKey);
	bool moveRight = IsPadButtonDown(state.key, PAD_INPUT_RIGHT) || IsKeyDown(input.rightKey);

	int analogX = 0;
	int analogY = 0;
	if (GetJoypadAnalogInput(&analogX, &analogY, input.padType) == 0) {
		if (analogX < -PAD_ANALOG_DEAD_ZONE) {
			moveLeft = true;
		}
		else if (analogX > PAD_ANALOG_DEAD_ZONE) {
			moveRight = true;
		}

		if (analogY < -PAD_ANALOG_DEAD_ZONE) {
			moveUp = true;
		}
		else if (analogY > PAD_ANALOG_DEAD_ZONE) {
			moveDown = true;
		}
	}

	inputX = 0.0f;
	inputZ = 0.0f;
	if (moveDown) {
		inputZ -= MOVE_SPEED;
	}
	if (moveUp) {
		inputZ += MOVE_SPEED;
	}
	if (moveLeft) {
		inputX -= MOVE_SPEED;
	}
	if (moveRight) {
		inputX += MOVE_SPEED;
	}
}

void SetDirectionByMove(SCharaInfo& player, float inputX, float inputZ) {
	// 移動方向に合わせてキャラクターの向きを変える。
	if (fabsf(inputX) > fabsf(inputZ)) {
		player.direction = inputX < 0.0f ? Direction::LEFT : Direction::RIGHT;
	}
	else {
		player.direction = inputZ < 0.0f ? Direction::DOWN : Direction::UP;
	}
}

void StartPlayerAttack(SCharaInfo& player, PlayerRuntimeState& state, const int animAttack[], int seAttackHandle) {
	state.attackIndex = 0;
	state.isAttackBuffered = false;
	player.mode = ATTACK;
	player.isHit = false;
	SetCharacterAnimation(player, animAttack[state.attackIndex]);
	PlaySoundMem(seAttackHandle, DX_PLAYTYPE_BACK);
}

void ApplyAttackStepMove(SCharaInfo& player, PlayerRuntimeState& state, const PlayerInputConfig& input) {
	ResetMove(player);

	float inputX = 0.0f;
	float inputZ = 0.0f;
	GetMoveInput(state, input, inputX, inputZ);
	if (inputX != 0.0f || inputZ != 0.0f) {
		SetDirectionByMove(player, inputX, inputZ);
	}

	switch (player.direction)
	{
	case Direction::DOWN:
		player.move.z = -7.0f;
		break;
	case Direction::UP:
		player.move.z = 7.0f;
		break;
	case Direction::LEFT:
		player.move.x = -7.0f;
		break;
	case Direction::RIGHT:
		player.move.x = 7.0f;
		break;
	default:
		break;
	}
}

} // namespace

void ResetMove(SCharaInfo& chara) {
	chara.move.x = 0.0f;
	chara.move.y = 0.0f;
	chara.move.z = 0.0f;
}

void SetCharacterAnimation(SCharaInfo& chara, int animHandle, float playtime) {
	MV1DetachAnim(chara.model1, chara.attachidx);
	chara.attachidx = MV1AttachAnim(chara.model1, 0, animHandle);
	chara.anim_totaltime = MV1GetAttachAnimTotalTime(chara.model1, chara.attachidx);
	chara.playtime = playtime;
}

void UpdatePlayerAnimationProgress(SCharaInfo& player, PlayerRuntimeState& state, int animNeutral) {
	if (player.mode != JUMPOUT) {
		player.playtime += 0.3f;
	}
	else {
		player.playtime += 0.1f;
	}

	if (player.mode != FALL && player.mode != JUMPIN && player.mode != JUMPLOOP
		&& player.mode != ATTACK) {
		if (player.playtime > player.anim_totaltime) {
			if ((player.mode == JUMPOUT) || (player.mode == ATTACKOUT)) {
				if (player.mode == ATTACKOUT) {
					state.attackIndex = 0;
					player.isHit = false;
				}
				SetCharacterAnimation(player, animNeutral);
				player.mode = STAND;
			}
			player.playtime = 0.0f;
		}
	}

	MV1SetAttachAnimTime(player.model1, player.attachidx, player.playtime);
}

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
) {
	// プレイヤーを操作できる状態か確認する。
	state.moveInput = false;
	bool attackPressed = false;
	bool jumpPressed = false;

	if (CanControlPlayerMode(player.mode)) {
		state.key = GetJoypadInputState(input.padType);
		if (state.key < 0) {
			// パッド未接続時は入力なしとして扱う。
			state.key = 0;
		}
		// 攻撃/ジャンプは押した瞬間だけ反応させる。
		const int currentAttackButton = (IsPadOrKeyDown(state.key, input.attackPadButton, input.attackKey) ||
			IsPadButtonDown(state.key, PAD_INPUT_10)) ? 1 : 0;
		const int currentJumpButton = IsPadOrKeyDown(state.key, input.jumpPadButton, input.jumpKey) ? 1 : 0;
		attackPressed = (currentAttackButton == 1 && state.prevAttackButton == 0);
		jumpPressed = (currentJumpButton == 1 && state.prevJumpButton == 0);
		state.prevAttackButton = currentAttackButton;
		state.prevJumpButton = currentJumpButton;
	}
	else {
		state.key = 0;
		state.prevAttackButton = 0;
		state.prevJumpButton = 0;
	}

	if (player.mode == STAND || player.mode == RUN) {
		ResetMove(player);

		if (attackPressed) {
			StartPlayerAttack(player, state, animAttack, seAttackHandle);
		}
		else {
			// キーボード、十字キー、左スティック入力を移動量に変換する。
			float inputX = 0.0f;
			float inputZ = 0.0f;
			GetMoveInput(state, input, inputX, inputZ);

			if (inputX != 0.0f || inputZ != 0.0f) {
				state.moveInput = true;
				player.move.x = inputX;
				player.move.z = inputZ;
				SetDirectionByMove(player, inputX, inputZ);
			}

			if (jumpPressed) {
				player.mode = JUMPIN;
				SetCharacterAnimation(player, animJumpIn, 0.3f);
				MV1SetAttachAnimTime(player.model1, player.attachidx, player.playtime);
				PlaySoundMem(seJumpHandle, DX_PLAYTYPE_NORMAL);
			}
		}
	}

	// 攻撃中にもう一度押したら次の攻撃を予約する。
	if (player.mode == ATTACK && attackPressed && state.attackIndex < PLAYER_ATTACK_ANIM_COUNT - 1) {
		state.isAttackBuffered = true;
	}

	MV1SetRotationXYZ(player.model1, VGet(0.0f, DX_PI_F * 0.5f * player.direction, 0.0f));

	// 入力の有無で待機/走りアニメを切り替える。
	if (!state.moveInput) {
		if (player.mode == RUN) {
			ResetMove(player);
			player.mode = STAND;
			SetCharacterAnimation(player, animNeutral);
		}
	}
	else {
		if (player.mode == STAND) {
			player.mode = RUN;
			SetCharacterAnimation(player, animRun);
		}
	}
}

void UpdatePlayerAttackState(
	SCharaInfo& player,
	PlayerRuntimeState& state,
	const PlayerInputConfig& input,
	const int animAttack[],
	const float attackEndTime[],
	int seAttackHandle
) {
	// 攻撃アニメーション中の移動と連撃遷移を処理する。
	if (player.mode != ATTACK) {
		return;
	}

	if (player.move.x != 0 || player.move.z != 0) {
		switch (player.direction)
		{
		case Direction::DOWN:
			player.move.z += 0.25f;
			break;
		case Direction::UP:
			player.move.z -= 0.25f;
			break;
		case Direction::LEFT:
			player.move.x += 0.25f;
			break;
		case Direction::RIGHT:
			player.move.x -= 0.25f;
			break;
		default:
			break;
		}
	}

	if (player.playtime >= attackEndTime[state.attackIndex]) {
		// 予約入力があれば次の攻撃アニメへつなげる。
		if (state.isAttackBuffered && state.attackIndex < PLAYER_ATTACK_ANIM_COUNT - 1) {
			ApplyAttackStepMove(player, state, input);
			state.attackIndex++;
			state.isAttackBuffered = false;
			player.isHit = false;
			SetCharacterAnimation(player, animAttack[state.attackIndex]);
			PlaySoundMem(seAttackHandle, DX_PLAYTYPE_BACK);
		}
		else {
			state.isAttackBuffered = false;
			player.mode = ATTACKOUT;
		}
	}
}
