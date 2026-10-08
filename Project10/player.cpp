#include "player.h"

#include "game.h"

#include <DxLib.h>
#include <EffekseerForDXLib.h>
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


void StartNormalAttackEffect(PlayerRuntimeState& state) {
	state.normalAttackEffectFrame = 0;
	state.normalAttackEffectWait = 0;
	state.isNormalAttackEffectPlaying = true;
}

void StopNormalAttackEffect(PlayerRuntimeState& state) {
	state.normalAttackEffectFrame = 0;
	state.normalAttackEffectWait = 0;
	state.isNormalAttackEffectPlaying = false;
}

void UpdateNormalAttackEffect(PlayerRuntimeState& state) {
	if (!state.isNormalAttackEffectPlaying) {
		return;
	}

	state.normalAttackEffectWait++;
	if (state.normalAttackEffectWait < PLAYER_NORMAL_ATTACK_EFFECT_FRAME_INTERVAL) {
		return;
	}

	state.normalAttackEffectWait = 0;
	state.normalAttackEffectFrame++;
	if (state.normalAttackEffectFrame >= PLAYER_NORMAL_ATTACK_EFFECT_FRAME_COUNT) {
		StopNormalAttackEffect(state);
	}
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

void StartSpecialAttackEffect(
	const SCharaInfo& player,
	PlayerRuntimeState& state,
	int effectResourceHandle
) {
	// 必殺技ごとに判定時間、多重ヒット防止、Effekseer の再生位置を初期化する。
	state.specialAttackEffectFrame = 0;
	state.specialAttackEffectWait = 0;
	state.isSpecialAttackEffectPlaying = true;
	state.isSpecialHitDone = false;
	state.specialAttackPlayingHandle = PlayEffekseer3DEffect(effectResourceHandle);
	if (state.specialAttackPlayingHandle != -1) {
		SetPosPlayingEffekseer3DEffect(
			state.specialAttackPlayingHandle,
			player.pos.x,
			player.pos.y + 8.0f,
			player.pos.z
		);
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
		SetPosPlayingEffekseer3DEffect(
			state.specialAttackPlayingHandle,
			player.pos.x,
			player.pos.y + 8.0f,
			player.pos.z
		);
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

void StartPlayerAttack(SCharaInfo& player, PlayerRuntimeState& state, const int animAttack[], int seAttackHandle, bool isHeavyAttack) {
	// 重攻撃は3段目の攻撃アニメーションを使い、通常攻撃と重攻撃を区別する。
	state.attackIndex = isHeavyAttack ? PLAYER_ATTACK_ANIM_COUNT - 1 : 0;
	state.isAttackBuffered = false;
	state.isHeavyAttack = isHeavyAttack;
	state.isSpecialAttack = false;
	state.isSpecialHitDone = false;
	StopSpecialAttackEffect(state);
	if (isHeavyAttack) {
		StopNormalAttackEffect(state);
		StartHeavyAttackEffect(state);
	}
	else {
		StartNormalAttackEffect(state);
	}
	player.mode = ATTACK;
	player.isHit = false;
	SetCharacterAnimation(player, animAttack[state.attackIndex]);
	PlaySoundMem(seAttackHandle, DX_PLAYTYPE_BACK);
}

void StartPlayerSpecialAttack(SCharaInfo& player, PlayerRuntimeState& state, const int animAttack[], int seAttackHandle, int effectResourceHandle) {
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
	UpdateNormalAttackEffect(state);
	UpdateHeavyAttackEffect(state);
	UpdateSpecialAttackEffect(player, state);
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

void DrawPlayerNormalAttackEffect(const SCharaInfo& player, const PlayerRuntimeState& state, const int effectHandles[]) {
	if (!state.isNormalAttackEffectPlaying || state.attackIndex >= PLAYER_ATTACK_ANIM_COUNT - 1) {
		return;
	}

	const int frame = state.normalAttackEffectFrame;
	if (frame < 0 || frame >= PLAYER_NORMAL_ATTACK_EFFECT_FRAME_COUNT || effectHandles[frame] == -1) {
		return;
	}

	VECTOR forward = VGet(0.0f, 0.0f, 0.0f);
	switch (player.direction) {
	case Direction::DOWN:  forward.z = -1.0f; break;
	case Direction::UP:    forward.z = 1.0f; break;
	case Direction::LEFT:  forward.x = -1.0f; break;
	case Direction::RIGHT: forward.x = 1.0f; break;
	default: break;
	}

	VECTOR effectPos = VAdd(player.pos, VScale(forward, 85.0f));
	effectPos.y += player.charahitinfo.Height * 0.55f;
	const float angle = state.attackIndex == 1 ? DX_PI_F : 0.0f;
	SetDrawBlendMode(DX_BLENDMODE_ADD, 230);
	DrawBillboard3D(effectPos, 0.5f, 0.5f, PLAYER_NORMAL_ATTACK_EFFECT_SIZE, angle, effectHandles[frame], TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
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
	int playerIndex
) {
	// 水波が見えるフレームだけ、プレイヤー中心の円形判定を有効にする。
	if (!state.isSpecialAttack || !state.isSpecialAttackEffectPlaying || state.isSpecialHitDone) {
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
	const int radiusFrame = state.specialAttackEffectFrame < PLAYER_SPECIAL_ATTACK_FULL_RADIUS_FRAME
		? state.specialAttackEffectFrame
		: PLAYER_SPECIAL_ATTACK_FULL_RADIUS_FRAME;
	const float radiusRate = static_cast<float>(radiusFrame - PLAYER_SPECIAL_ATTACK_ACTIVE_START_FRAME + 1) /
		static_cast<float>(PLAYER_SPECIAL_ATTACK_FULL_RADIUS_FRAME - PLAYER_SPECIAL_ATTACK_ACTIVE_START_FRAME + 1);
	const float attackRadius = PLAYER_SPECIAL_ATTACK_MAX_RADIUS * radiusRate;
	const float targetRadius = target.charahitinfo.Width * 0.5f;
	const float hitDistance = attackRadius + targetRadius;
	const float distanceX = target.pos.x - attacker.pos.x;
	const float distanceZ = target.pos.z - attacker.pos.z;
	if (distanceX * distanceX + distanceZ * distanceZ > hitDistance * hitDistance) {
		return;
	}

	// isSpecialHitDone により、同じ必殺技が毎フレーム連続ヒットするのを防ぐ。
	state.isSpecialHitDone = true;
	SetCharacterAnimation(target, animDamage);
	target.mode = DAMAGE;
	PlaySoundMem(seDamageHandle, DX_PLAYTYPE_BACK);

	target.enemyHP -= PLAYER_SPECIAL_ATTACK_DAMAGE;
	if (target.enemyHP <= 0) {
		target.enemyHP = 0;
		target.mode = NONE;
		MV1SetVisible(target.model1, FALSE);
		game.AddScore(playerIndex, 100);
	}
	printfDx("必殺技ヒット！残りHP:%d\n", target.enemyHP);
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
	int seJumpHandle,
	int specialAttackEffectResourceHandle
) {
	// プレイヤーを操作できる状態か確認する。
	state.moveInput = false;
	bool attackPressed = false;
	bool heavyAttackPressed = false;
	bool specialAttackPressed = false;
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
		const int currentSpecialAttackButton = IsPadOrKeyDown(state.key, input.specialAttackPadButton, input.specialAttackKey) ? 1 : 0;
		const int currentJumpButton = IsPadOrKeyDown(state.key, input.jumpPadButton, input.jumpKey) ? 1 : 0;
		attackPressed = (currentAttackButton == 1 && state.prevAttackButton == 0);
		heavyAttackPressed = (currentHeavyAttackButton == 1 && state.prevHeavyAttackButton == 0);
		specialAttackPressed = (currentSpecialAttackButton == 1 && state.prevSpecialAttackButton == 0);
		jumpPressed = (currentJumpButton == 1 && state.prevJumpButton == 0);
		state.prevAttackButton = currentAttackButton;
		state.prevHeavyAttackButton = currentHeavyAttackButton;
		state.prevSpecialAttackButton = currentSpecialAttackButton;
		state.prevJumpButton = currentJumpButton;
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
			StartPlayerSpecialAttack(player, state, animAttack, seAttackHandle, specialAttackEffectResourceHandle);
		}
		else if (heavyAttackPressed) {
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
			if (state.attackIndex < PLAYER_ATTACK_ANIM_COUNT - 1) {
				StartNormalAttackEffect(state);
			}
			else {
				StopNormalAttackEffect(state);
			}
			SetCharacterAnimation(player, animAttack[state.attackIndex]);
			PlaySoundMem(seAttackHandle, DX_PLAYTYPE_BACK);
		}
		else {
			state.isAttackBuffered = false;
			player.mode = ATTACKOUT;
		}
	}
}
