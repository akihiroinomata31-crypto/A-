#include "player.h"

#include "game.h"

#include <DxLib.h>
#include <EffekseerForDXLib.h>
#include <math.h>

// Normalize Xbox face buttons to A=1, B=2, X=3, Y=4.
int ReadPlayerPadState(int padType) {
    int buttons = GetJoypadInputState(padType);
    if (buttons < 0) buttons = 0;
    XINPUT_STATE xbox = {};
    if (GetJoypadXInputState(padType, &xbox) == 0) {
        buttons &= ~(PAD_INPUT_1 | PAD_INPUT_2 | PAD_INPUT_3 | PAD_INPUT_4);
        if (xbox.Buttons[XINPUT_BUTTON_A]) buttons |= PAD_INPUT_1;
        if (xbox.Buttons[XINPUT_BUTTON_B]) buttons |= PAD_INPUT_2;
        if (xbox.Buttons[XINPUT_BUTTON_X]) buttons |= PAD_INPUT_3;
        if (xbox.Buttons[XINPUT_BUTTON_Y]) buttons |= PAD_INPUT_4;
    }
    return buttons;
}

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


void StopNormalAttackEffect(PlayerRuntimeState& state) {
	if (state.normalAttackPlayingHandle != -1) {
		StopEffekseer3DEffect(state.normalAttackPlayingHandle);
	}
	state.normalAttackPlayingHandle = -1;
	state.isNormalAttackEffectPlaying = false;
}

void PositionNormalAttackEffect(const SCharaInfo& player, const PlayerRuntimeState& state) {
	const float yaw = DX_PI_F * 0.5f * player.direction;
	SetPosPlayingEffekseer3DEffect(state.normalAttackPlayingHandle,
		player.pos.x - sinf(yaw) * 85.0f,
		player.pos.y + player.charahitinfo.Height * 0.55f,
		player.pos.z - cosf(yaw) * 85.0f);
	SetRotationPlayingEffekseer3DEffect(state.normalAttackPlayingHandle, 0.0f, yaw,
		state.attackIndex == 1 ? DX_PI_F : 0.0f);
}

void UpdateNormalAttackEffect(const SCharaInfo& player, PlayerRuntimeState& state) {
	if (!state.isNormalAttackEffectPlaying) return;
	if (IsEffekseer3DEffectPlaying(state.normalAttackPlayingHandle) != 0) {
		StopNormalAttackEffect(state);
		return;
	}
	PositionNormalAttackEffect(player, state);
}

void StartHeavyAttackEffect(PlayerRuntimeState& state) {
	// 重攻撃開始時にエフェクトの先頭フレームへ戻す。
	state.heavyAttackEffectFrame = 0;
	state.heavyAttackEffectWait = 0;
	state.isHeavyAttackEffectPlaying = true;
}

void StopHeavyAttackEffect(PlayerRuntimeState& state) {
	state.heavyAttackEffectFrame = 0;
	state.heavyAttackEffectWait = 0;
	state.isHeavyAttackEffectPlaying = false;
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
		StopHeavyAttackEffect(state);
	}
}

void PositionSpecialAttackEffect(const SCharaInfo& player, const PlayerRuntimeState& state) {
	const float yaw = DX_PI_F * 0.5f * player.direction;
	SetPosPlayingEffekseer3DEffect(state.specialAttackPlayingHandle,
		player.pos.x - sinf(yaw) * PLAYER_SPECIAL_ATTACK_EFFECT_FORWARD_OFFSET,
		player.pos.y + player.charahitinfo.Height * 0.55f + PLAYER_SPECIAL_ATTACK_EFFECT_HEIGHT_OFFSET,
		player.pos.z - cosf(yaw) * PLAYER_SPECIAL_ATTACK_EFFECT_FORWARD_OFFSET);
	SetRotationPlayingEffekseer3DEffect(state.specialAttackPlayingHandle, 0.0f, yaw - DX_PI_F * 0.5f, 0.0f);
}
void StartSpecialAttackEffect(
	const SCharaInfo& player,
	PlayerRuntimeState& state,
	int effectResourceHandle
) {
	// 必殺技ごとに判定時間、多重ヒット防止、Effekseer の再生位置を初期化する。
	state.specialAttackEffectFrame = 0;
	state.specialAttackEffectWait = 0;
	state.isSpecialAttackEffectPlaying = true;
	for (int i = 0; i < MAX_CHARA; ++i) state.specialHitTargets[i] = false;
	state.specialAttackPlayingHandle = PlayEffekseer3DEffect(effectResourceHandle);
	if (state.specialAttackPlayingHandle != -1) {
		PositionSpecialAttackEffect(player, state);
	}
}

void StopSpecialAttackEffect(PlayerRuntimeState& state) {
	if (state.specialAttackPlayingHandle != -1) {
		StopEffekseer3DEffect(state.specialAttackPlayingHandle);
		state.specialAttackPlayingHandle = -1;
	}
	state.specialAttackEffectFrame = 0;
	state.specialAttackEffectWait = 0;
	state.isSpecialAttackEffectPlaying = false;
}

void UpdateSpecialAttackEffect(const SCharaInfo& player, PlayerRuntimeState& state) {
	if (!state.isSpecialAttackEffectPlaying) {
		return;
	}

	// プレイヤー座標を毎フレーム反映し、1P/2P の各エフェクトを本人に追従させる。
	if (state.specialAttackPlayingHandle != -1) {
		PositionSpecialAttackEffect(player, state);
	}

	state.specialAttackEffectWait++;
	if (state.specialAttackEffectWait < PLAYER_SPECIAL_ATTACK_EFFECT_FRAME_INTERVAL) {
		return;
	}

	state.specialAttackEffectWait = 0;
	state.specialAttackEffectFrame++;
	if (state.specialAttackEffectFrame >= PLAYER_SPECIAL_ATTACK_EFFECT_FRAME_COUNT) {
		StopSpecialAttackEffect(state);
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

void StartPlayerAttack(SCharaInfo& player, PlayerRuntimeState& state, const int animAttack[], int seAttackHandle, bool isHeavyAttack, int animHeavyAttack) {
	// 重攻撃は3段目の攻撃アニメーションを使い、通常攻撃と重攻撃を区別する。
	state.attackIndex = isHeavyAttack ? PLAYER_ATTACK_ANIM_COUNT - 1 : 0;
	state.isAttackBuffered = false;
	state.isHeavyAttack = isHeavyAttack;
	state.isSpecialAttack = false;
	for (int i = 0; i < MAX_CHARA; ++i) state.specialHitTargets[i] = false;
	StopSpecialAttackEffect(state);
	if (isHeavyAttack) {
		StopNormalAttackEffect(state);
		StartHeavyAttackEffect(state);
	}
	else {
		PlayPlayerNormalAttackEffect(player, state);
	}
	player.mode = ATTACK;
	player.isHit = false;
	SetCharacterAnimation(player, isHeavyAttack ? animHeavyAttack : animAttack[state.attackIndex]);
	PlaySoundMem(seAttackHandle, DX_PLAYTYPE_BACK);
}

void StartPlayerSpecialAttack(SCharaInfo& player, PlayerRuntimeState& state, const int animAttack[], int seAttackHandle, int effectResourceHandle, int animSpecialAttack) {
	// 専用モデルアニメーションができるまでは3段目の攻撃動作を流用する。
	state.attackIndex = PLAYER_ATTACK_ANIM_COUNT - 1;
	state.isAttackBuffered = false;
	state.isHeavyAttack = false;
	state.isSpecialAttack = true;
	StopNormalAttackEffect(state);
	StopHeavyAttackEffect(state);
	StartSpecialAttackEffect(player, state, effectResourceHandle);
	ResetMove(player);
	player.mode = ATTACK;
	player.isHit = false;
	SetCharacterAnimation(player, animSpecialAttack);
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
	if (chara.model1 < 0 || animHandle < 0) return;
	if (chara.attachidx >= 0) MV1DetachAnim(chara.model1, chara.attachidx);
	chara.attachidx = MV1AttachAnim(chara.model1, 0, animHandle);
	chara.anim_totaltime = MV1GetAttachAnimTotalTime(chara.model1, chara.attachidx);
	chara.playtime = playtime;
}

void CancelPlayerCombat(PlayerRuntimeState& state) {
    StopNormalAttackEffect(state);
    StopHeavyAttackEffect(state);
    StopSpecialAttackEffect(state);
    state.isSpecialAttack = false;
    state.isHeavyAttack = false;
    state.isAttackBuffered = false;
    state.attackIndex = 0;
    state.movementAnimation = -1;
}
void UpdatePlayerAnimationProgress(SCharaInfo& player, PlayerRuntimeState& state, int animNeutral) {
	if (player.mode == DAMAGE || player.mode == DOWNMODE) {
        CancelPlayerCombat(state);
        ResetMove(player);
        if (player.mode == DOWNMODE) {
            player.playtime = fminf(player.playtime + (PLAYER_USE_NEW_MODEL ? 0.5f : 0.3f), player.anim_totaltime);
            if (player.attachidx >= 0) MV1SetAttachAnimTime(player.model1, player.attachidx, player.playtime);
            return;
        }
    }
    UpdateNormalAttackEffect(player, state);
	UpdateHeavyAttackEffect(state);
	UpdateSpecialAttackEffect(player, state);
    // Restore control and animation together when the special effect ends.
    if (state.isSpecialAttack && !state.isSpecialAttackEffectPlaying) {
        state.isSpecialAttack = false;
        state.isHeavyAttack = false;
        state.isAttackBuffered = false;
        state.attackIndex = 0;
        state.movementAnimation = -1;
        player.isHit = false;
        if (player.mode == ATTACK || player.mode == ATTACKOUT) {
            ResetMove(player);
            SetCharacterAnimation(player, animNeutral);
            player.mode = STAND;
        }
    }
	if (player.mode != JUMPOUT) {
		player.playtime += PLAYER_USE_NEW_MODEL ? 0.5f : 0.3f;
	}
	else {
		player.playtime += PLAYER_USE_NEW_MODEL ? 0.5f : 0.1f;
	}

	if (player.mode != FALL && player.mode != JUMPIN && player.mode != JUMPLOOP
		&& player.mode != ATTACK) {
		if (player.playtime > player.anim_totaltime) {
			if ((player.mode == JUMPOUT) || (player.mode == ATTACKOUT) || (player.mode == DAMAGE)) {
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

	MV1SetAttachAnimTime(player.model1, player.attachidx,
		PLAYER_USE_NEW_MODEL && state.isSpecialAttack && player.playtime > player.anim_totaltime ? player.anim_totaltime : player.playtime);
}

void PlayPlayerNormalAttackEffect(const SCharaInfo& player, PlayerRuntimeState& state) {
	StopNormalAttackEffect(state);
	state.normalAttackPlayingHandle = PlayEffekseer3DEffect(state.normalAttackEffectResourceHandle);
	state.isNormalAttackEffectPlaying = state.normalAttackPlayingHandle != -1;
	if (state.isNormalAttackEffectPlaying) {
		SetSpeedPlayingEffekseer3DEffect(state.normalAttackPlayingHandle, PLAYER_NORMAL_ATTACK_EFFECT_PLAYBACK_SPEED);
		PositionNormalAttackEffect(player, state);
	}
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

void CheckPlayerSpecialAttackHit(
	GameManager& game,
	SCharaInfo& attacker,
	PlayerRuntimeState& state,
	SCharaInfo& target,
	int seDamageHandle,
	int animDamage,
	int playerIndex,
	int targetIndex
) {
	// 水波が見えるフレームだけ、プレイヤー中心の円形判定を有効にする。
	if (!state.isSpecialAttack || !state.isSpecialAttackEffectPlaying || targetIndex < 0 || targetIndex >= MAX_CHARA || state.specialHitTargets[targetIndex]) {
		return;
	}
	if (state.specialAttackEffectFrame < PLAYER_SPECIAL_ATTACK_ACTIVE_START_FRAME ||
		state.specialAttackEffectFrame > PLAYER_SPECIAL_ATTACK_ACTIVE_END_FRAME) {
		return;
	}
	if (target.mode == NONE || target.mode == DOWNMODE || target.mode == DAMAGE || target.enemyHP <= 0) {
		return;
	}

	// 波紋の広がりに合わせ、開始フレームから最大半径まで判定を拡大する。
	// The first impact is a forward capsule; the sword shower expands around the effect.
	const float yaw = DX_PI_F * 0.5f * attacker.direction;
	const float forwardX = -sinf(yaw), forwardZ = -cosf(yaw);
	const float dx = target.pos.x - attacker.pos.x, dz = target.pos.z - attacker.pos.z;
	const float targetRadius = target.charahitinfo.Width * 0.5f;
	if (target.pos.y > attacker.pos.y + attacker.charahitinfo.Height + 160.0f ||
		target.pos.y + target.charahitinfo.Height < attacker.pos.y) return;
	float centerX, centerZ, attackRadius;
	if (state.specialAttackEffectFrame < 70) {
		float along = dx * forwardX + dz * forwardZ;
		if (along < 40.0f) along = 40.0f;
		if (along > 384.0f) along = 384.0f;
		centerX = forwardX * along;
		centerZ = forwardZ * along;
		attackRadius = 104.0f;
	} else {
		centerX = forwardX * PLAYER_SPECIAL_ATTACK_EFFECT_FORWARD_OFFSET;
		centerZ = forwardZ * PLAYER_SPECIAL_ATTACK_EFFECT_FORWARD_OFFSET;
		float rate = static_cast<float>(state.specialAttackEffectFrame - 70 + 1) /
			static_cast<float>(PLAYER_SPECIAL_ATTACK_FULL_RADIUS_FRAME - 70 + 1);
		if (rate > 1.0f) rate = 1.0f;
		attackRadius = 160.0f + (PLAYER_SPECIAL_ATTACK_MAX_RADIUS - 160.0f) * rate;
	}
	const float hitDistance = attackRadius + targetRadius;
	const float distanceX = dx - centerX, distanceZ = dz - centerZ;
	if (distanceX * distanceX + distanceZ * distanceZ > hitDistance * hitDistance) return;
	// Each target takes damage once per cast.
	state.specialHitTargets[targetIndex] = true;
	SetCharacterAnimation(target, animDamage);
	target.mode = DAMAGE;
	PlaySoundMem(seDamageHandle, DX_PLAYTYPE_BACK);

	target.enemyHP -= PLAYER_SPECIAL_ATTACK_DAMAGE;
	if (target.enemyHP <= 0) {
		target.enemyHP = 0;
		target.mode = NONE;
		MV1SetVisible(target.model1, FALSE);
		game.AddScore(playerIndex, 100); game.AddScorePopup(playerIndex, 100);
	}
}

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
) {
	// プレイヤーを操作できる状態か確認する。
	state.moveInput = false;
	bool attackPressed = false;
	bool heavyAttackPressed = false;
	bool specialAttackPressed = false;

	if (CanControlPlayerMode(player.mode)) {
		state.key = ReadPlayerPadState(input.padType);
		if (state.key < 0) {
			// パッド未接続時は入力なしとして扱う。
			state.key = 0;
		}
		// 攻撃、重攻撃、ジャンプは押した瞬間だけ反応させる。
		const int currentAttackButton = (IsPadOrKeyDown(state.key, input.attackPadButton, input.attackKey) ||
			IsPadButtonDown(state.key, PAD_INPUT_10)) ? 1 : 0;
		const int currentHeavyAttackButton = IsPadOrKeyDown(state.key, input.heavyAttackPadButton, input.heavyAttackKey) ? 1 : 0;
		const int currentSpecialAttackButton = IsPadOrKeyDown(state.key, input.specialAttackPadButton, input.specialAttackKey) ? 1 : 0;
		attackPressed = (currentAttackButton == 1 && state.prevAttackButton == 0);
		heavyAttackPressed = (currentHeavyAttackButton == 1 && state.prevHeavyAttackButton == 0);
		specialAttackPressed = (currentSpecialAttackButton == 1 && state.prevSpecialAttackButton == 0);
		state.prevAttackButton = currentAttackButton;
		state.prevHeavyAttackButton = currentHeavyAttackButton;
		state.prevSpecialAttackButton = currentSpecialAttackButton;
	}
	else {
		state.key = 0;
		state.prevAttackButton = 0;
		state.prevHeavyAttackButton = 0;
		state.prevSpecialAttackButton = 0;
		state.prevJumpButton = 0;
	}

	if (player.mode == STAND || player.mode == RUN) {
		ResetMove(player);

		if (specialAttackPressed) {
			StartPlayerSpecialAttack(player, state, animAttack, seAttackHandle, specialAttackEffectResourceHandle, animSpecialAttack);
		}
		else if (heavyAttackPressed) {
			StartPlayerAttack(player, state, animAttack, seAttackHandle, true, animHeavyAttack);
		}
		else if (attackPressed) {
			StartPlayerAttack(player, state, animAttack, seAttackHandle, false, animHeavyAttack);
		}
		else {
			// キーボード、十字キー、左スティック入力を移動量に変換する。
			float inputX = 0.0f;
			float inputZ = 0.0f;
			GetMoveInput(state, input, inputX, inputZ);

			if (inputX != 0.0f || inputZ != 0.0f) {
				UpdatePlayerMovement(player, state, inputX, inputZ, animRun);
			}

            // The former jump button is reserved for resurrection in main.cpp.
		}
	}

	// 攻撃中にもう一度押したら次の攻撃を予約する。
	if (player.mode == ATTACK && !state.isHeavyAttack && !state.isSpecialAttack && attackPressed && state.attackIndex < PLAYER_ATTACK_ANIM_COUNT - 1) {
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

	// 必殺技中は移動を止め、エフェクトの終了と同時に攻撃後状態へ移る。
	if (state.isSpecialAttack) {
		ResetMove(player);
		if (!state.isSpecialAttackEffectPlaying) {
			state.isAttackBuffered = false;
			state.isSpecialAttack = false;
			player.mode = ATTACKOUT;
		}
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

	if (player.playtime >= (PLAYER_USE_NEW_MODEL && state.isHeavyAttack ? player.anim_totaltime : attackEndTime[state.attackIndex])) {
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
			PlayPlayerNormalAttackEffect(player, state);
			SetCharacterAnimation(player, animAttack[state.attackIndex]);
			PlaySoundMem(seAttackHandle, DX_PLAYTYPE_BACK);
		}
		else {
			state.isAttackBuffered = false;
			player.mode = ATTACKOUT;
		}
	}
}

void ApplyPlayerMotionRoot(SCharaInfo& player, const PlayerRuntimeState& state) {
	if (!PLAYER_USE_NEW_MODEL || state.motionRootFrame < 0 || player.attachidx < 0) return;
	MV1ResetFrameUserLocalMatrix(player.model1, state.motionRootFrame);
	MATRIX root = MV1GetFrameLocalMatrix(player.model1, state.motionRootFrame);
	root.m[3][0] = state.motionRootX;
	root.m[3][2] = state.motionRootZ;
	MV1SetFrameUserLocalMatrix(player.model1, state.motionRootFrame, root);
}

void UpdatePlayerMovement(SCharaInfo& player, PlayerRuntimeState& state,
	float inputX, float inputZ, int animRun) {
	if ((player.mode != STAND && player.mode != RUN) || (inputX == 0.0f && inputZ == 0.0f)) return;
	state.moveInput = true;
	player.move.x = inputX;
	player.move.z = inputZ;
	SetDirectionByMove(player, inputX, inputZ);
	const int animation = animRun;
	if (player.mode == STAND || state.movementAnimation != animation) {
		player.mode = RUN;
		SetCharacterAnimation(player, animation);
		state.movementAnimation = animation;
	}
}

int FindWhirlwindPullPlayer(const SCharaInfo players[], const PlayerRuntimeState states[], const SCharaInfo& target) {
	if (target.mode == NONE || target.mode == DOWNMODE || target.enemyHP <= 0) return -1;
	int closest = -1;
	float closestDistanceSquared = PLAYER_WHIRLWIND_PULL_RADIUS * PLAYER_WHIRLWIND_PULL_RADIUS;
	for (int i = 0; i < PLAYER_COUNT; ++i) {
		if (!states[i].isHeavyAttackEffectPlaying || players[i].mode == NONE || players[i].mode == DOWNMODE ||
			states[i].heavyAttackEffectFrame < 0 || states[i].heavyAttackEffectFrame >= PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT) continue;
		if (target.pos.y > players[i].pos.y + players[i].charahitinfo.Height + PLAYER_WHIRLWIND_PULL_HEIGHT ||
			target.pos.y + target.charahitinfo.Height < players[i].pos.y) continue;
		const float dx = players[i].pos.x - target.pos.x;
		const float dz = players[i].pos.z - target.pos.z;
		const float distanceSquared = dx * dx + dz * dz;
		if (distanceSquared <= closestDistanceSquared && (closest < 0 || distanceSquared < closestDistanceSquared)) {
			closest = i;
			closestDistanceSquared = distanceSquared;
		}
	}
	return closest;
}

void UpdateWhirlwindPull(const SCharaInfo players[], const PlayerRuntimeState states[], SCharaInfo& target) {
	const int owner = FindWhirlwindPullPlayer(players, states, target);
	if (owner < 0) return;
	const float dx = players[owner].pos.x - target.pos.x;
	const float dz = players[owner].pos.z - target.pos.z;
	const float distance = sqrtf(dx * dx + dz * dz);
	float stopDistance = players[owner].charahitinfo.Width * 0.5f + target.charahitinfo.Width * 0.25f;
	if (stopDistance < PLAYER_WHIRLWIND_PULL_STOP_DISTANCE) stopDistance = PLAYER_WHIRLWIND_PULL_STOP_DISTANCE;
	if (distance <= stopDistance) return;
	float step = PLAYER_WHIRLWIND_PULL_SPEED;
	if (step > distance - stopDistance) step = distance - stopDistance;
	target.pos.x += dx * step / distance;
	target.pos.z += dz * step / distance;
	target.charahitinfo.CenterPosition = target.pos;
	if (target.model1 != -1) MV1SetPosition(target.model1, target.pos);
}
void UpdateEnemyLocomotionAnimation(SCharaInfo& enemy, bool moving) {
	if (enemy.mode != STAND) {
		enemy.enemyWalking = false;
		return;
	}
	if (moving == enemy.enemyWalking && enemy.attachidx >= 0) return;
	const int animation = moving ? enemy.enemyWalkAnimation : enemy.enemyIdleAnimation;
	if (animation < 0) return;
	SetCharacterAnimation(enemy, animation);
	enemy.enemyWalking = moving;
}