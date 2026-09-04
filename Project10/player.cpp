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

void StartHeavyAttackEffect(PlayerRuntimeState& state) {
	// 重攻撃開始時にエフェクトの先頭フレームへ戻す。
	state.heavyAttackEffectFrame = 0;
	state.heavyAttackEffectWait = 0;
	state.isHeavyAttackEffectPlaying = true;
}

void UpdateHeavyAttackEffect(PlayerRuntimeState& state) {
	// 重攻撃エフェクトのスプライトシートを1フレームずつ進める。
	if (!state.isHeavyAttackEffectPlaying) {
		return;
	}

	state.heavyAttackEffectWait++;
	if (state.heavyAttackEffectWait < PLAYER_HEAVY_ATTACK_EFFECT_FRAME_INTERVAL) {
		return;
	}

	state.heavyAttackEffectWait = 0;
	state.heavyAttackEffectFrame++;
	if (state.heavyAttackEffectFrame >= PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT) {
		state.heavyAttackEffectFrame = 0;
		state.isHeavyAttackEffectPlaying = false;
	}
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

void StartPlayerAttack(SCharaInfo& player, PlayerRuntimeState& state, const int animAttack[], int seAttackHandle, bool isHeavyAttack) {
	// 重攻撃は3段目の攻撃アニメーションを使い、通常攻撃より重い見た目にする。
	state.attackIndex = isHeavyAttack ? PLAYER_ATTACK_ANIM_COUNT - 1 : 0;
	state.isAttackBuffered = false;
	state.isHeavyAttack = isHeavyAttack;
	if (isHeavyAttack) {
		StartHeavyAttackEffect(state);
	}
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
	UpdateHeavyAttackEffect(state);
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
					state.isHeavyAttack = false;
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

void DrawPlayerHeavyAttackEffect(const SCharaInfo& player, const PlayerRuntimeState& state, const int effectHandles[]) {
	// 重攻撃中だけ、プレイヤー位置にエフェクトを表示する。
	if (!state.isHeavyAttackEffectPlaying) {
		return;
	}

	const int frame = state.heavyAttackEffectFrame;
	if (frame < 0 || frame >= PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT || effectHandles[frame] == -1) {
		return;
	}

	const VECTOR effectPos = VAdd(player.pos, VGet(0.0f, player.charahitinfo.Height * 0.45f, 0.0f));
	SetDrawBlendMode(DX_BLENDMODE_ADD, 220);
	DrawBillboard3D(effectPos, 0.5f, 0.5f, PLAYER_HEAVY_ATTACK_EFFECT_SIZE, 0.0f, effectHandles[frame], TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
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
	bool heavyAttackPressed = false;
	bool jumpPressed = false;

	if (CanControlPlayerMode(player.mode)) {
		state.key = GetJoypadInputState(input.padType);
		if (state.key < 0) {
			// パッド未接続時は入力なしとして扱う。
			state.key = 0;
		}
		// 攻撃、重攻撃、ジャンプは押した瞬間だけ反応させる。
		const int currentAttackButton = (IsPadOrKeyDown(state.key, input.attackPadButton, input.attackKey) ||
			IsPadButtonDown(state.key, PAD_INPUT_10)) ? 1 : 0;
		const int currentHeavyAttackButton = IsPadOrKeyDown(state.key, input.heavyAttackPadButton, input.heavyAttackKey) ? 1 : 0;
		const int currentJumpButton = IsPadOrKeyDown(state.key, input.jumpPadButton, input.jumpKey) ? 1 : 0;
		attackPressed = (currentAttackButton == 1 && state.prevAttackButton == 0);
		heavyAttackPressed = (currentHeavyAttackButton == 1 && state.prevHeavyAttackButton == 0);
		jumpPressed = (currentJumpButton == 1 && state.prevJumpButton == 0);
		state.prevAttackButton = currentAttackButton;
		state.prevHeavyAttackButton = currentHeavyAttackButton;
		state.prevJumpButton = currentJumpButton;
	}
	else {
		state.key = 0;
		state.prevAttackButton = 0;
		state.prevHeavyAttackButton = 0;
		state.prevJumpButton = 0;
	}

	if (player.mode == STAND || player.mode == RUN) {
		ResetMove(player);

		if (heavyAttackPressed) {
			StartPlayerAttack(player, state, animAttack, seAttackHandle, true);
		}
		else if (attackPressed) {
			StartPlayerAttack(player, state, animAttack, seAttackHandle, false);
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
	if (player.mode == ATTACK && !state.isHeavyAttack && attackPressed && state.attackIndex < PLAYER_ATTACK_ANIM_COUNT - 1) {
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
		if (state.isHeavyAttack) {
			// 重攻撃は単発で終了させる。
			state.isAttackBuffered = false;
			state.isHeavyAttack = false;
			player.mode = ATTACKOUT;
		}
		// 予約入力があれば次の攻撃アニメへつなげる。
		else if (state.isAttackBuffered && state.attackIndex < PLAYER_ATTACK_ANIM_COUNT - 1) {
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
