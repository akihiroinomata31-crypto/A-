// DXライブラリーのインクルード
#include <DxLib.h>
#include <math.h>
#include <stdio.h>

#include "main.h"
#include "game.h"
#include "player.h"

namespace {

	int LoadPlayerAssetModel(const char* fileName) {
		char path[256];

		sprintf_s(path, sizeof(path), "..\\Player\\%s", fileName);
		int handle = MV1LoadModel(path);
		if (handle != -1) {
			return handle;
		}

		sprintf_s(path, sizeof(path), "..\\Data\\Player\\%s", fileName);
		handle = MV1LoadModel(path);
		if (handle == -1) {
			printfDx("Player asset load failed: %s\n", fileName);
		}

		return handle;
	}

} // namespace

int WINAPI WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lpC, int nC)
{
	int running = 0;
	int rootflm;
	MATRIX wpmatrix[PLAYER_COUNT], sayamatrix[PLAYER_COUNT];
	int	anim_neutral, anim_run, anim_jumpin, anim_jumploop, anim_jumpout, anim_damage, anim_down;
	int enemy_anim_attack, enemy_anim_run, enemy_anim_neutral;

	int redGoblinBaseModel = -1;
	int red_goblin_anim_neutral = -1;
	int red_goblin_anim_attack = -1;
	int red_goblin_anim_walk = -1;

	int anim_attack[PLAYER_ATTACK_ANIM_COUNT];
	int stagedata;
	int sky;
	float skyRot = 0;
	static SCharaInfo charainfo[MAX_CHARA];
	int playerWeaponModel[PLAYER_COUNT], playerWeaponFrame[PLAYER_COUNT];
	int playerSayaModel[PLAYER_COUNT], playerSayaFrame[PLAYER_COUNT];
	VECTOR wpPosStart[PLAYER_COUNT], wpPosEnd[PLAYER_COUNT];
	int prevJKey = 0;
	int isBGMPlaying = 1;

	// ==========================================
	// 閃光弾エイム用変数
	// ==========================================
	bool isAiming[PLAYER_COUNT] = { false, false };
	bool prevAiming[PLAYER_COUNT] = { false, false };
	float aimAngle[PLAYER_COUNT] = { 0.0f, 0.0f };
	PlayerRuntimeState playerStates[PLAYER_COUNT];

	PlayerInputConfig playerInputs[PLAYER_COUNT] = {
		{ DX_INPUT_PAD1, KEY_INPUT_W, KEY_INPUT_S, KEY_INPUT_A, KEY_INPUT_D, KEY_INPUT_SPACE, KEY_INPUT_Q, PAD_INPUT_1, PAD_INPUT_2 },
		{ DX_INPUT_PAD2, KEY_INPUT_UP, KEY_INPUT_DOWN, KEY_INPUT_LEFT, KEY_INPUT_RIGHT, KEY_INPUT_RETURN, -1, PAD_INPUT_1, PAD_INPUT_2 }
	};

	const int SPAWN_INTERVAL = 300;
	float attackInEndTime[PLAYER_ATTACK_ANIM_COUNT] = { ATTACK_FIRST_ENDTIME, ATTACK_SECOND_ENDTIME, ATTACK_THIERD_ENDTIME };

	GameManager game;
	VECTOR stagepos = VGet(0.0f, 0.0f, 1200.0f);

	// ステージコリジョン情報
	MV1_COLL_RESULT_POLY_DIM HitDim;
	int WallNum;
	int FloorNum;
	MV1_COLL_RESULT_POLY* Wall[CHARA_MAX_HITCOLL];
	MV1_COLL_RESULT_POLY* Floor[CHARA_MAX_HITCOLL];
	int HitFlag = 0;
	int timeLimit = 1000;
	static int spawnTimer = 0;
	spawnTimer++;
	MV1_COLL_RESULT_POLY* Poly;
	HITRESULT_LINE LineRes;

	VECTOR PolyCharaHitField[3];
	int grenadeBaseModel = -1;

	char BGM0_FilePath[] = "BGM_stg0.ogg";
	char String[256];
	int BGMSoundHandle;
	int BGMLoopStartPosition = -1;
	int BGMLoopEndPosition = -1;
	int hpBarTex = -1;
	char SEattack_FilePath[] = "swish_00.wav", SEjump_FilePath[] = "jumpIn_00.wav", SEdamage_FilePath[] = "dmg_bySword_00.wav";
	int SEattackHandle, SEjumpHandle, SEdamageHandle;

	// キャラ初期化
	for (int i = 0; i < MAX_CHARA; i++) {
		charainfo[i].model1 = -1;
		charainfo[i].attachidx = -1;
		charainfo[i].mode = NONE;
		charainfo[i].direction = Direction::DOWN;
		ResetMove(charainfo[i]);
		charainfo[i].charahitinfo.Height = PC_HEIGHT;
		charainfo[i].charahitinfo.Width = PC_WIDTH;
		charainfo[i].HP = 0;
		charainfo[i].enemyHP = 0;
		charainfo[i].isHit = false;
	}

	charainfo[0].deaths = 0;
	charainfo[1].deaths = 0;
	charainfo[PLAYER1_INDEX].pos = VGet(1300.0f, 800.0f, 100.0f);
	charainfo[PLAYER2_INDEX].pos = VGet(1300.0f, 800.0f, -400.0f);

	for (int i = 0; i < PLAYER_COUNT; i++) {
		charainfo[i].mode = STAND;
		charainfo[i].HP = 6;
		charainfo[i].charahitinfo.Height = PC_HEIGHT * PLAYER_MODEL_SCALE;
		charainfo[i].charahitinfo.Width = PC_WIDTH * PLAYER_MODEL_SCALE;
		charainfo[i].charahitinfo.CenterPosition = charainfo[i].pos;
	}

	charainfo[TEST_ENEMY_INDEX].mode = STAND;
	charainfo[TEST_ENEMY_INDEX].enemyHP = 2;
	charainfo[TEST_ENEMY_INDEX].charahitinfo.CenterPosition = charainfo[TEST_ENEMY_INDEX].pos;

	SetCreateSoundDataType(DX_SOUNDDATATYPE_FILE);
	ChangeWindowMode(TRUE);
	SetGraphMode(900, 600, 32);

	if (DxLib_Init() == -1) {
		return -1;
	}

	// 1. 通常ゴブリン読み込み
	int baseGoblinModel = MV1LoadModel("..\\Data\\Goblin\\Goblin.mv1");
	enemy_anim_neutral = baseGoblinModel;

	if (baseGoblinModel == -1) {
		printfDx("ゴブリンのベースモデル読み込み失敗！\n");
	}

	for (int i = TEST_ENEMY_INDEX; i < TEST_ENEMY_RED; i++) {
		if (baseGoblinModel != -1) {
			charainfo[i].model1 = MV1DuplicateModel(baseGoblinModel);
		}
		else {
			charainfo[i].model1 = -1;
		}

		if (charainfo[i].model1 == -1) continue;

		MV1SetVisible(charainfo[i].model1, FALSE);
		charainfo[i].mode = NONE;

		charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, enemy_anim_neutral);
		if (charainfo[i].attachidx != -1) {
			charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
		}
		charainfo[i].playtime = 0.3f;
	}

	// 2. 赤ゴブリン読み込み
	redGoblinBaseModel = MV1LoadModel("..\\Data\\RedGoblin\\RedGoblin.mv1");
	red_goblin_anim_neutral = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Neutral.mv1");
	red_goblin_anim_walk = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Walk.mv1");
	red_goblin_anim_attack = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Attack1.mv1");

	for (int i = TEST_ENEMY_RED; i < MAX_CHARA; i++) {
		if (redGoblinBaseModel != -1) {
			charainfo[i].model1 = MV1DuplicateModel(redGoblinBaseModel);
		}
		else {
			charainfo[i].model1 = -1;
		}

		if (charainfo[i].model1 == -1) continue;

		MV1SetVisible(charainfo[i].model1, FALSE);
		charainfo[i].mode = NONE;

		charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, red_goblin_anim_neutral);
		if (charainfo[i].attachidx != -1) {
			charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
		}
		charainfo[i].playtime = 0.3f;
	}

	// プレイヤーモデル読み込み
	for (int i = 0; i < PLAYER_COUNT; i++) {
		charainfo[i].model1 = LoadPlayerAssetModel("PC.mv1");
		if (charainfo[i].model1 == -1) return -1;
		MV1SetPosition(charainfo[i].model1, charainfo[i].pos);
		MV1SetScale(charainfo[i].model1, VGet(PLAYER_MODEL_SCALE, PLAYER_MODEL_SCALE, PLAYER_MODEL_SCALE));
	}
	MV1SetMaterialDrawAddColorAll(charainfo[PLAYER2_INDEX].model1, 0, 0, 60);

	charainfo[TEST_ENEMY_GOLEM].model1 = MV1LoadModel("..\\Data\\Golem\\Golem.mv1");
	if (charainfo[TEST_ENEMY_GOLEM].model1 != -1) {
		MV1SetVisible(charainfo[TEST_ENEMY_GOLEM].model1, FALSE);
		charainfo[TEST_ENEMY_GOLEM].mode = NONE;
		MV1SetScale(charainfo[TEST_ENEMY_GOLEM].model1, VGet(1.0f, 1.0f, 1.0f));
	}

	for (int i = 0; i < PLAYER_COUNT; i++) {
		rootflm = MV1SearchFrame(charainfo[i].model1, "root");
		if (rootflm != -1) {
			MV1SetFrameUserLocalMatrix(charainfo[i].model1, rootflm, MGetIdent());
		}
	}
	rootflm = MV1SearchFrame(charainfo[TEST_ENEMY_INDEX].model1, "root");
	if (rootflm != -1) {
		MV1SetFrameUserLocalMatrix(charainfo[TEST_ENEMY_INDEX].model1, rootflm, MGetIdent());
	}

	for (int i = 0; i < PLAYER_COUNT; i++) {
		playerWeaponModel[i] = LoadPlayerAssetModel("Sabel.mv1");
		if (playerWeaponModel[i] == -1) return -1;
		playerWeaponFrame[i] = MV1SearchFrame(charainfo[i].model1, "wp");

		playerSayaModel[i] = LoadPlayerAssetModel("Sabel.mv1");
		if (playerSayaModel[i] == -1) return -1;
		playerSayaFrame[i] = MV1SearchFrame(charainfo[i].model1, "sayabone");
	}

	// ステージ背面カリング有効化（描画負荷削減）
	stagedata = MV1LoadModel("..\\Data\\Stage\\Stage_4545.mv1");
	if (stagedata == -1) return -1;
	MV1SetPosition(stagedata, stagepos);
	int meshNum = MV1GetMeshNum(stagedata);
	for (int i = 0; i < meshNum; i++) {
		MV1SetMeshBackCulling(stagedata, i, TRUE);
	}
	sky = MV1LoadModel("..\\Data\\Stage\\Sage_1214.mv1");
	if (sky == -1) return -1;
	MV1SetPosition(sky, VGet(0, -1000, 0));

	MV1SetupCollInfo(stagedata, -1);
	SetTransColor(0, 0, 0);

	// ==========================================
	// 閃光弾モデルと構造体
	// ==========================================
	if (grenadeBaseModel == -1) {
		grenadeBaseModel = MV1LoadModel("..\\Data\\Weapon\\Senkou.mv1");
	}
	if (grenadeBaseModel != -1) {
		MV1SetVisible(grenadeBaseModel, FALSE);
	}

	struct ThrownFlashbang {
		int model;
		VECTOR pos;
		VECTOR move;
		int timer;
		bool active;
		int ownerIndex; // 投げたプレイヤー（0:1P, 1:2P）
	};

	const int MAX_THROWN_FLASH = 2;
	ThrownFlashbang thrownFlashes[MAX_THROWN_FLASH];
	for (int f = 0; f < MAX_THROWN_FLASH; f++) {
		thrownFlashes[f].active = false;
		thrownFlashes[f].ownerIndex = -1;
		thrownFlashes[f].model = (grenadeBaseModel != -1) ? MV1DuplicateModel(grenadeBaseModel) : -1;
		if (thrownFlashes[f].model != -1) {
			MV1SetVisible(thrownFlashes[f].model, FALSE);
			MV1SetScale(thrownFlashes[f].model, VGet(1.0f, 1.0f, 1.0f));
		}
	}

	int flashStock[PLAYER_COUNT] = { 1, 1 };
	int playerWhiteoutTimer[PLAYER_COUNT] = { 0, 0 };

	struct FlashDropItem {
		int model;
		VECTOR pos;
		float basePosY;
		float bobAngle;
		bool active;
	};

	FlashDropItem dropFlashItem;
	dropFlashItem.active = false;
	dropFlashItem.basePosY = 800.0f;
	dropFlashItem.bobAngle = 0.0f;
	dropFlashItem.model = (grenadeBaseModel != -1) ? MV1DuplicateModel(grenadeBaseModel) : -1;
	if (dropFlashItem.model != -1) {
		MV1SetVisible(dropFlashItem.model, FALSE);
		MV1SetScale(dropFlashItem.model, VGet(1.3f, 1.3f, 1.3f));
	}

	int flashSpawnTimer = 0;
	const int FLASH_SPAWN_INTERVAL = 60 * 18;

	int crownGraphHandle = LoadGraph("..\\Data\\UI\\win.png");

	anim_neutral = LoadPlayerAssetModel("Anim_Neutral.mv1");
	anim_run = LoadPlayerAssetModel("Anim_Run.mv1");
	anim_jumpin = LoadPlayerAssetModel("Anim_Jump_In.mv1");
	anim_jumploop = LoadPlayerAssetModel("Anim_Jump_Loop.mv1");
	anim_jumpout = LoadPlayerAssetModel("Anim_Jump_Out.mv1");
	anim_attack[0] = LoadPlayerAssetModel("Anim_Attack1.mv1");
	anim_attack[1] = LoadPlayerAssetModel("Anim_Attack2.mv1");
	anim_attack[2] = LoadPlayerAssetModel("Anim_Attack3.mv1");
	anim_damage = LoadPlayerAssetModel("Anim_Damage.mv1");
	anim_down = LoadPlayerAssetModel("Anim_Down_Loop.mv1");
	enemy_anim_attack = MV1LoadModel("..\\Data\\Goblin\\Anim_Attack1.mv1");
	enemy_anim_run = MV1LoadModel("..\\Data\\Goblin\\Anim_Run.mv1");
	enemy_anim_neutral = MV1LoadModel("..\\Data\\Goblin\\Anim_Neutral.mv1");
	game.enemy_anim_neutral = enemy_anim_neutral;

	SetTransColor(255, 255, 255);
	hpBarTex = LoadGraph("..\\Data\\UI\\Hpbar023.png");

	for (int i = 0; i < PLAYER_COUNT; i++) {
		charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_neutral);
		charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
	}
	charainfo[TEST_ENEMY_INDEX].attachidx = MV1AttachAnim(charainfo[TEST_ENEMY_INDEX].model1, 0, enemy_anim_neutral);
	charainfo[TEST_ENEMY_INDEX].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[TEST_ENEMY_INDEX].model1, charainfo[TEST_ENEMY_INDEX].attachidx);

	SetDrawScreen(DX_SCREEN_BACK);

	// カメラの初期画角設定
	SetupCamera_Perspective(60.0f * DX_PI_F / 180.0f);

	// サウンド
	sprintf_s(String, SOUND_DIRECTORY_PATH "BGM\\%s", BGM0_FilePath);
	BGMSoundHandle = LoadSoundMem(String);
	if (BGMSoundHandle != -1) {
		PlaySoundMem(BGMSoundHandle, BGMLoopStartPosition >= 0 ? DX_PLAYTYPE_LOOP : DX_PLAYTYPE_BACK);
	}

	sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Weapon\\Sword\\%s", SEattack_FilePath);
	SEattackHandle = LoadSoundMem(String);
	sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Player\\%s", SEdamage_FilePath);
	SEdamageHandle = LoadSoundMem(String);
	sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Player\\%s", SEjump_FilePath);
	SEjumpHandle = LoadSoundMem(String);

	// ==========================================
	// メインループ
	// ==========================================
	while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0) {

		int currentJKey = CheckHitKey(KEY_INPUT_J);

		for (int i = 0; i < PLAYER_COUNT; i++) {
			UpdatePlayerAnimationProgress(charainfo[i], playerStates[i], anim_neutral);
		}
		for (int i = 0; i < PLAYER_COUNT; i++) {
			if (charainfo[i].HP <= 0 && charainfo[i].mode != DOWNMODE) {
				game.AddScore(i, -500);
				charainfo[i].mode = DOWNMODE;
				MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
				charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_down);
				charainfo[i].playtime = 0.0f;
				game.AddDeath(i);
			}
		}
		for (int i = 0; i < PLAYER_COUNT; i++) {
			if (charainfo[i].mode == DOWNMODE && CheckHitKey(playerInputs[i].jumpKey) == 1) {
				charainfo[i].HP = 6;
				charainfo[i].mode = STAND;
				MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
				charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_neutral);
				charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
				charainfo[i].playtime = 0.0f;
			}
		}

		// ==========================================
		// 閃光弾：エイム操作 ＆ リリース投擲
		// ==========================================
		for (int p = 0; p < PLAYER_COUNT; p++) {
			bool currentAimInput = false;
			if (p == 0) {
				currentAimInput = (CheckHitKey(KEY_INPUT_E) == 1) || (playerStates[p].key & PAD_INPUT_5);
			}
			else {
				currentAimInput = (CheckHitKey(KEY_INPUT_RSHIFT) == 1) || (playerStates[p].key & PAD_INPUT_5);
			}

			if (flashStock[p] > 0 && charainfo[p].mode != DAMAGE && charainfo[p].mode != DOWNMODE) {
				if (currentAimInput && !prevAiming[p]) {
					isAiming[p] = true;
					aimAngle[p] = charainfo[p].dir;
				}
				else if (currentAimInput && isAiming[p]) {
					if (p == 0) {
						if (CheckHitKey(KEY_INPUT_A)) aimAngle[p] -= 0.05f;
						if (CheckHitKey(KEY_INPUT_D)) aimAngle[p] += 0.05f;
					}
					else {
						if (CheckHitKey(KEY_INPUT_LEFT))  aimAngle[p] -= 0.05f;
						if (CheckHitKey(KEY_INPUT_RIGHT)) aimAngle[p] += 0.05f;
					}

					int inputX = 0, inputY = 0;
					GetJoypadAnalogInput(&inputX, &inputY, (p == 0) ? DX_INPUT_PAD1 : DX_INPUT_PAD2);
					if (abs(inputX) > 300) {
						aimAngle[p] += (inputX / 1000.0f) * 0.05f;
					}

					charainfo[p].dir = aimAngle[p];
					MV1SetRotationXYZ(charainfo[p].model1, VGet(0.0f, aimAngle[p] + DX_PI_F, 0.0f));
					charainfo[p].move.x = 0.0f;
					charainfo[p].move.z = 0.0f;
				}
				else if (!currentAimInput && prevAiming[p] && isAiming[p]) {
					isAiming[p] = false;

					for (int f = 0; f < MAX_THROWN_FLASH; f++) {
						if (!thrownFlashes[f].active && thrownFlashes[f].model != -1) {
							thrownFlashes[f].active = true;
							thrownFlashes[f].timer = 75;
							thrownFlashes[f].ownerIndex = p; // 投げた本人を記録
							flashStock[p]--;

							thrownFlashes[f].pos = VAdd(charainfo[p].pos, VGet(0.0f, 70.0f, 0.0f));

							float speed = 15.0f;
							thrownFlashes[f].move = VGet(sinf(aimAngle[p]) * speed, 8.5f, cosf(aimAngle[p]) * speed);

							MV1SetVisible(thrownFlashes[f].model, TRUE);
							break;
						}
					}
				}
			}
			else {
				isAiming[p] = false;
			}

			prevAiming[p] = currentAimInput;
		}

		// ==========================================
		// アイテムの出現＆回収処理
		// ==========================================
		if (!dropFlashItem.active) {
			flashSpawnTimer++;
			if (flashSpawnTimer >= FLASH_SPAWN_INTERVAL) {
				flashSpawnTimer = 0;
				dropFlashItem.active = true;
				dropFlashItem.pos = VGet(1300.0f + (float)(GetRand(600) - 300), dropFlashItem.basePosY + 40.0f, -150.0f + (float)(GetRand(600) - 300));
				if (dropFlashItem.model != -1) {
					MV1SetVisible(dropFlashItem.model, TRUE);
				}
			}
		}
		else {
			dropFlashItem.bobAngle += 0.05f;
			float currentY = dropFlashItem.basePosY + 40.0f + sinf(dropFlashItem.bobAngle) * 15.0f;

			MV1SetPosition(dropFlashItem.model, VGet(dropFlashItem.pos.x, currentY, dropFlashItem.pos.z));
			MV1SetRotationXYZ(dropFlashItem.model, VGet(0.0f, dropFlashItem.bobAngle, 0.0f));

			const float PICKUP_RANGE_SQ = 130.0f * 130.0f;
			for (int p = 0; p < PLAYER_COUNT; p++) {
				if (charainfo[p].mode == DOWNMODE) continue;

				float dx = charainfo[p].pos.x - dropFlashItem.pos.x;
				float dy = charainfo[p].pos.y - currentY;
				float dz = charainfo[p].pos.z - dropFlashItem.pos.z;

				if (dx * dx + dy * dy + dz * dz < PICKUP_RANGE_SQ) {
					flashStock[p]++;
					dropFlashItem.active = false;
					if (dropFlashItem.model != -1) {
						MV1SetVisible(dropFlashItem.model, FALSE);
					}
					break;
				}
			}
		}

		// ==========================================
		// 閃光弾の物理挙動 ＆ 炸裂処理（高速版）
		// ==========================================
		for (int f = 0; f < MAX_THROWN_FLASH; f++) {
			if (!thrownFlashes[f].active) continue;

			thrownFlashes[f].pos.x += thrownFlashes[f].move.x;
			thrownFlashes[f].pos.y += thrownFlashes[f].move.y;
			thrownFlashes[f].pos.z += thrownFlashes[f].move.z;
			thrownFlashes[f].move.y -= 0.45f;

			if (thrownFlashes[f].pos.y <= 800.0f) {
				thrownFlashes[f].pos.y = 800.0f;
				thrownFlashes[f].move.y = -thrownFlashes[f].move.y * 0.3f;
				thrownFlashes[f].move.x *= 0.6f;
				thrownFlashes[f].move.z *= 0.6f;
			}

			MV1SetPosition(thrownFlashes[f].model, thrownFlashes[f].pos);

			thrownFlashes[f].timer--;
			if (thrownFlashes[f].timer <= 0) {
				thrownFlashes[f].active = false;
				MV1SetVisible(thrownFlashes[f].model, FALSE);

				const float FLASH_RANGE_SQ = 450.0f * 450.0f;

				// ① 敵AIのスタン
				for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
					if (charainfo[i].mode == NONE) continue;
					float edx = charainfo[i].pos.x - thrownFlashes[f].pos.x;
					float edz = charainfo[i].pos.z - thrownFlashes[f].pos.z;
					if (edx * edx + edz * edz < FLASH_RANGE_SQ) {
						charainfo[i].mode = DAMAGE;
						charainfo[i].playtime = 0.0f;
					}
				}

				// ② 相手プレイヤーの直視判定（投げた本人は食らわない）
				for (int p = 0; p < PLAYER_COUNT; p++) {
					if (charainfo[p].mode == DOWNMODE) continue;
					if (p == thrownFlashes[f].ownerIndex) continue; // 投げた本人は無効化

					float toX = thrownFlashes[f].pos.x - charainfo[p].pos.x;
					float toZ = thrownFlashes[f].pos.z - charainfo[p].pos.z;
					float pDistSq = toX * toX + toZ * toZ;

					if (pDistSq < FLASH_RANGE_SQ) {
						float fwdX = sinf(charainfo[p].dir);
						float fwdZ = cosf(charainfo[p].dir);
						float dot = fwdX * toX + fwdZ * toZ;

						// dot > 0 で直視判定
						if (dot > 0.0f) {
							charainfo[p].mode = DAMAGE;
							charainfo[p].playtime = 0.0f;
							MV1DetachAnim(charainfo[p].model1, charainfo[p].attachidx);
							charainfo[p].attachidx = MV1AttachAnim(charainfo[p].model1, 0, anim_damage);
							charainfo[p].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[p].model1, charainfo[p].attachidx);
							playerWhiteoutTimer[p] = 120;
						}
						else {
							playerWhiteoutTimer[p] = 20;
						}
					}
				}
			}
		}

		// 敵AI処理
		for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
			if (charainfo[i].mode == NONE) continue;

			bool isRed = (i >= TEST_ENEMY_RED);
			int animNeutral = isRed ? red_goblin_anim_neutral : enemy_anim_neutral;
			int animAttack = isRed ? red_goblin_anim_attack : enemy_anim_attack;

			if (charainfo[i].mode == DAMAGE) {
				if (charainfo[i].playtime >= charainfo[i].anim_totaltime) {
					charainfo[i].mode = STAND;
					charainfo[i].playtime = 0.0f;
					charainfo[i].isHit = false;
					charainfo[i].currentAnimType = 1;

					if (charainfo[i].attachidx != -1) {
						MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
					}
					charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, animNeutral);
					if (charainfo[i].attachidx != -1) {
						charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
					}
				}

				charainfo[i].playtime += 0.2f;
				if (charainfo[i].attachidx != -1) {
					MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
				}
				continue;
			}

			float dx1 = charainfo[PLAYER1_INDEX].pos.x - charainfo[i].pos.x;
			float dz1 = charainfo[PLAYER1_INDEX].pos.z - charainfo[i].pos.z;
			float distSqP1 = dx1 * dx1 + dz1 * dz1;

			float dx2 = charainfo[PLAYER2_INDEX].pos.x - charainfo[i].pos.x;
			float dz2 = charainfo[PLAYER2_INDEX].pos.z - charainfo[i].pos.z;
			float distSqP2 = dx2 * dx2 + dz2 * dz2;

			int enemyTargetIndex = (distSqP2 < distSqP1) ? PLAYER2_INDEX : PLAYER1_INDEX;
			float targetDistSq = (distSqP2 < distSqP1) ? distSqP2 : distSqP1;

			VECTOR dir = VSub(charainfo[enemyTargetIndex].pos, charainfo[i].pos);
			dir.y = 0;

			const float activeRangeSq = 300.0f * 300.0f;
			if (targetDistSq > activeRangeSq && charainfo[i].mode != ATTACK) {
				if (charainfo[i].currentAnimType != 1) {
					charainfo[i].currentAnimType = 1;
					charainfo[i].playtime = 0.0f;
					if (charainfo[i].attachidx != -1) {
						MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
					}
					charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, animNeutral);
					if (charainfo[i].attachidx != -1) {
						charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
					}
				}

				charainfo[i].playtime += 0.2f;
				if (charainfo[i].playtime >= charainfo[i].anim_totaltime) {
					charainfo[i].playtime = 0.0f;
				}
				if (charainfo[i].attachidx != -1) {
					MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
				}
				continue;
			}

			float angle = atan2f(dir.x, dir.z) + DX_PI_F;
			MV1SetRotationXYZ(charainfo[i].model1, VGet(0.0f, angle, 0.0f));

			const float attackRangeSq = 140.0f * 140.0f;

			if (charainfo[i].mode == STAND) {
				if (targetDistSq <= attackRangeSq) {
					charainfo[i].mode = ATTACK;
					charainfo[i].playtime = 0.0f;
					charainfo[i].isHit = false;
					charainfo[i].currentAnimType = 3;

					if (charainfo[i].attachidx != -1) {
						MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
					}
					charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, animAttack);
					if (charainfo[i].attachidx != -1) {
						charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
					}
				}
				else {
					float realDist = sqrtf(targetDistSq);
					if (realDist > 0.001f) {
						VECTOR moveDir = VGet(dir.x / realDist, 0.0f, dir.z / realDist);
						charainfo[i].pos = VAdd(charainfo[i].pos, VScale(moveDir, 1.8f));
					}

					if (charainfo[i].currentAnimType != 2) {
						charainfo[i].currentAnimType = 2;
						charainfo[i].playtime = 0.0f;

						if (charainfo[i].attachidx != -1) {
							MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
						}
						charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, enemy_anim_run);
						if (charainfo[i].attachidx != -1) {
							charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
						}
					}
				}
			}
			else if (charainfo[i].mode == ATTACK) {
				if (charainfo[i].playtime >= charainfo[i].anim_totaltime * 0.2f &&
					charainfo[i].playtime <= charainfo[i].anim_totaltime * 0.6f)
				{
					if (!charainfo[i].isHit) {
						for (int p = 0; p < PLAYER_COUNT; p++) {
							if (charainfo[p].mode == DOWNMODE) continue;

							if (HitCheck_Capsule_Capsule(
								charainfo[i].pos, VAdd(charainfo[i].pos, VGet(0, 50, 0)), 60.0f,
								charainfo[p].pos, VAdd(charainfo[p].pos, VGet(0, charainfo[p].charahitinfo.Height, 0)), charainfo[p].charahitinfo.Width / 2))
							{
								charainfo[p].HP -= 1;
								charainfo[i].isHit = true;
								break;
							}
						}
					}
				}

				if (charainfo[i].playtime >= charainfo[i].anim_totaltime) {
					charainfo[i].mode = STAND;
					charainfo[i].playtime = 0.0f;
					charainfo[i].isHit = false;
					charainfo[i].currentAnimType = 1;

					if (charainfo[i].attachidx != -1) {
						MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
					}
					charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, animNeutral);
					if (charainfo[i].attachidx != -1) {
						charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
					}
				}
			}

			charainfo[i].playtime += 0.2f;
			if (charainfo[i].mode == STAND) {
				if (charainfo[i].playtime >= charainfo[i].anim_totaltime) {
					charainfo[i].playtime = 0.0f;
				}
			}

			if (charainfo[i].attachidx != -1) {
				MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
			}
		}

		// 敵押し合い処理（2フレームに1回）
		static int pushFrameCount = 0;
		pushFrameCount++;

		if (pushFrameCount % 2 == 0)
		{
			const float minDistance = 120.0f;
			const float minDistanceSq = minDistance * minDistance;

			for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
				if (charainfo[i].mode != STAND || charainfo[i].currentAnimType != 2) continue;

				for (int j = i + 1; j < MAX_CHARA; j++) {
					if (charainfo[j].mode != STAND) continue;

					float diffX = charainfo[i].pos.x - charainfo[j].pos.x;
					float diffZ = charainfo[i].pos.z - charainfo[j].pos.z;
					float distSq = diffX * diffX + diffZ * diffZ;

					if (distSq < minDistanceSq && distSq > 0.001f) {
						float actualDist = sqrtf(distSq);
						float pushDist = (minDistance - actualDist) * 0.5f;

						float pushX = (diffX / actualDist) * pushDist;
						float pushZ = (diffZ / actualDist) * pushDist;

						charainfo[i].pos.x += pushX;
						charainfo[i].pos.z += pushZ;
						charainfo[j].pos.x -= pushX;
						charainfo[j].pos.z -= pushZ;
					}
				}

				MV1SetPosition(charainfo[i].model1, charainfo[i].pos);
			}
		}

		// プレイヤー更新
		for (int i = 0; i < PLAYER_COUNT; i++) {
			if (charainfo[i].mode == DAMAGE) {
				charainfo[i].move.x = 0.0f;
				charainfo[i].move.z = 0.0f;
				continue;
			}

			UpdatePlayerInput(
				charainfo[i],
				playerStates[i],
				playerInputs[i],
				anim_attack,
				anim_neutral,
				anim_run,
				anim_jumpin,
				SEattackHandle,
				SEjumpHandle
			);
			UpdatePlayerAttackState(
				charainfo[i],
				playerStates[i],
				playerInputs[i],
				anim_attack,
				attackInEndTime,
				SEattackHandle
			);

			if (currentJKey == 1 && prevJKey == 0) {
				if (isBGMPlaying) {
					StopSoundMem(BGMSoundHandle);
					isBGMPlaying = 0;
				}
				else {
					PlaySoundMem(BGMSoundHandle, DX_PLAYTYPE_LOOP);
					isBGMPlaying = 1;
				}
			}
			prevJKey = currentJKey;

			int enemyTargetIndex = PLAYER1_INDEX;
			float enemyDistP1 = VSize(VSub(charainfo[PLAYER1_INDEX].pos, charainfo[TEST_ENEMY_INDEX].pos));
			float enemyDistP2 = VSize(VSub(charainfo[PLAYER2_INDEX].pos, charainfo[TEST_ENEMY_INDEX].pos));
			if (enemyDistP2 < enemyDistP1) {
				enemyTargetIndex = PLAYER2_INDEX;
			}

			if (charainfo[TEST_ENEMY_INDEX].mode == STAND) {
				float dist = VSize(VSub(charainfo[enemyTargetIndex].pos, charainfo[TEST_ENEMY_INDEX].pos));
				if (dist < 150.0f) {
					charainfo[TEST_ENEMY_INDEX].mode = ATTACK;
					SetCharacterAnimation(charainfo[TEST_ENEMY_INDEX], enemy_anim_attack);
				}
			}
			else if (charainfo[TEST_ENEMY_INDEX].mode == ATTACK) {
				if (charainfo[TEST_ENEMY_INDEX].playtime >= charainfo[TEST_ENEMY_INDEX].anim_totaltime * 0.4f &&
					charainfo[TEST_ENEMY_INDEX].playtime <= charainfo[TEST_ENEMY_INDEX].anim_totaltime * 0.6f)
				{
					if (charainfo[TEST_ENEMY_INDEX].isHit == false) {
						for (int p = 0; p < PLAYER_COUNT; p++) {
							if (HitCheck_Capsule_Capsule(
								charainfo[TEST_ENEMY_INDEX].pos, VAdd(charainfo[TEST_ENEMY_INDEX].pos, VGet(0, 50, 0)), 60.0f,
								charainfo[p].pos, VAdd(charainfo[p].pos, VGet(0, charainfo[p].charahitinfo.Height, 0)), charainfo[p].charahitinfo.Width / 2))
							{
								charainfo[p].HP -= 1;
								charainfo[TEST_ENEMY_INDEX].isHit = true;
								break;
							}
						}
					}
				}

				if (charainfo[TEST_ENEMY_INDEX].playtime >= charainfo[TEST_ENEMY_INDEX].anim_totaltime) {
					charainfo[TEST_ENEMY_INDEX].mode = STAND;
					SetCharacterAnimation(charainfo[TEST_ENEMY_INDEX], enemy_anim_neutral);
					charainfo[TEST_ENEMY_INDEX].isHit = false;
				}
			}

			if (HitCheck_Capsule_Capsule(
				VAdd(charainfo[PLAYER1_INDEX].pos, charainfo[PLAYER1_INDEX].move),
				VAdd(VAdd(charainfo[PLAYER1_INDEX].pos, charainfo[PLAYER1_INDEX].move), VGet(0, charainfo[PLAYER1_INDEX].charahitinfo.Height, 0)),
				charainfo[PLAYER1_INDEX].charahitinfo.Width / 2,
				VAdd(charainfo[PLAYER2_INDEX].pos, charainfo[PLAYER2_INDEX].move),
				VAdd(VAdd(charainfo[PLAYER2_INDEX].pos, charainfo[PLAYER2_INDEX].move), VGet(0, charainfo[PLAYER2_INDEX].charahitinfo.Height, 0)),
				charainfo[PLAYER2_INDEX].charahitinfo.Width / 2) == TRUE)
			{
				charainfo[PLAYER1_INDEX].move.x = 0.0f;
				charainfo[PLAYER1_INDEX].move.z = 0.0f;
				charainfo[PLAYER2_INDEX].move.x = 0.0f;
				charainfo[PLAYER2_INDEX].move.z = 0.0f;
			}

			// 落下判定
			float DEATH_LINE = -300.0f;
			if (charainfo[i].pos.y < DEATH_LINE) {
				charainfo[i].deaths++;
				game.AddScore(i, -500);

				if (i == 0) {
					charainfo[i].pos = VGet(1300.0f, 800.0f, 100.0f);
				}
				else {
					charainfo[i].pos = VGet(1300.0f, 800.0f, -400.0f);
				}

				charainfo[i].move = VGet(0.0f, 0.0f, 0.0f);
				charainfo[i].HP = 6;
				charainfo[i].mode = STAND;
				MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
				charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_neutral);
				charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
				charainfo[i].playtime = 0.0f;
			}
		}

		// 床コリジョン判定
		for (int i = 0; i < PLAYER_COUNT; i++) {
			if (charainfo[i].mode == DOWNMODE) continue;

			HitDim = MV1CollCheck_Sphere(stagedata, -1, charainfo[i].pos, CHARA_ENUM_DEFAULT_SIZE + VSize(charainfo[i].move));
			WallNum = 0;
			FloorNum = 0;
			for (int h = 0; h < HitDim.HitNum; h++) {
				if (fabs(HitDim.Dim[h].Normal.y) < 0.5f) {
					if (HitDim.Dim[h].Position[0].y > charainfo[i].pos.y + 1.0f ||
						HitDim.Dim[h].Position[1].y > charainfo[i].pos.y + 1.0f ||
						HitDim.Dim[h].Position[2].y > charainfo[i].pos.y + 1.0f)
					{
						if (WallNum < CHARA_MAX_HITCOLL) {
							Wall[WallNum] = &HitDim.Dim[h];
							WallNum++;
						}
					}
				}
				else {
					if (FloorNum < CHARA_MAX_HITCOLL) {
						Floor[FloorNum] = &HitDim.Dim[h];
						FloorNum++;
					}
				}
			}

			float MaxY = 0.0f;
			int hitFloorFlag = 0;

			if (FloorNum != 0) {
				for (int f = 0; f < FloorNum; f++) {
					Poly = Floor[f];

					VECTOR cal_pos1 = VAdd(charainfo[i].pos, VGet(0.0f, PC_HEIGHT, 0.0f));
					VECTOR cal_pos2 = VAdd(charainfo[i].pos, VGet(0.0f, -5.0f, 0.0f));
					LineRes = HitCheck_Line_Triangle(cal_pos1, cal_pos2, Poly->Position[0], Poly->Position[1], Poly->Position[2]);

					if (LineRes.HitFlag == TRUE) {
						if (i == 0) {
							PolyCharaHitField[0] = Poly->Position[0];
							PolyCharaHitField[1] = Poly->Position[1];
							PolyCharaHitField[2] = Poly->Position[2];
						}
					}
					else {
						continue;
					}

					if (hitFloorFlag == 1 && MaxY > LineRes.Position.y) {
						continue;
					}

					hitFloorFlag = 1;
					MaxY = LineRes.Position.y;
				}
			}

			if (hitFloorFlag == 1) {
				charainfo[i].move.y = MaxY - charainfo[i].pos.y;

				if (charainfo[i].mode == JUMPLOOP || charainfo[i].mode == FALL) {
					MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
					charainfo[i].mode = JUMPOUT;
					charainfo[i].playtime = 0.0f;
					charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_jumpout);
					charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
					charainfo[i].move.x = 0.0f;
					charainfo[i].move.y = 0.0f;
					charainfo[i].move.z = 0.0f;
				}
				else if (charainfo[i].mode == JUMPIN) {
					if (charainfo[i].playtime > charainfo[i].anim_totaltime) {
						MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
						charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_jumploop);
						charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
						charainfo[i].mode = JUMPLOOP;
						charainfo[i].move.y = 15.0f;
						charainfo[i].pos.y += charainfo[i].move.y;
					}
				}
			}
			else {
				if (charainfo[i].mode != JUMPLOOP && charainfo[i].mode != FALL) {
					MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
					charainfo[i].mode = FALL;
					charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_jumploop);
					charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
					charainfo[i].playtime = 7.0f;
					MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
				}
			}

			if (charainfo[i].mode == FALL || charainfo[i].mode == JUMPLOOP) {
				charainfo[i].move.y -= GRAVITY;
			}

			MV1CollResultPolyDimTerminate(HitDim);
		}

		// 座標反映
		for (int i = 0; i < PLAYER_COUNT; i++) {
			charainfo[i].pos.x += charainfo[i].move.x;
			charainfo[i].pos.y += charainfo[i].move.y;
			charainfo[i].pos.z += charainfo[i].move.z;
			charainfo[i].charahitinfo.CenterPosition = charainfo[i].pos;
		}

		for (int i = 0; i < PLAYER_COUNT; i++) {
			MV1SetPosition(charainfo[i].model1, charainfo[i].pos);
			if (playerSayaFrame[i] != -1 && playerSayaModel[i] != -1) {
				sayamatrix[i] = MV1GetFrameLocalWorldMatrix(charainfo[i].model1, playerSayaFrame[i]);
				MV1SetMatrix(playerSayaModel[i], sayamatrix[i]);
			}
			if (playerWeaponFrame[i] != -1 && playerWeaponModel[i] != -1) {
				wpmatrix[i] = MV1GetFrameLocalWorldMatrix(charainfo[i].model1, playerWeaponFrame[i]);
				MV1SetMatrix(playerWeaponModel[i], wpmatrix[i]);
				wpPosStart[i] = VGet(0.0f, 0.0f, 0.0f);
				wpPosEnd[i] = VGet(0.0f, -90.0f, 0.0f);
				wpPosStart[i] = VTransform(wpPosStart[i], wpmatrix[i]);
				wpPosEnd[i] = VTransform(wpPosEnd[i], wpmatrix[i]);
				for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; e++) {
					if (charainfo[e].mode == NONE) continue;
					CheckAttackHit(game, charainfo, &charainfo[i], &charainfo[e], wpPosStart[i], wpPosEnd[i], SEdamageHandle, anim_damage);
				}
			}
		}

		MV1SetAttachAnimTime(charainfo[TEST_ENEMY_INDEX].model1, charainfo[TEST_ENEMY_INDEX].attachidx, charainfo[TEST_ENEMY_INDEX].playtime);

		if ((playerStates[PLAYER1_INDEX].key & PAD_INPUT_2) && charainfo[TEST_ENEMY_INDEX].mode == DOWNMODE) {
			charainfo[TEST_ENEMY_INDEX].enemyHP = 6;
			charainfo[TEST_ENEMY_INDEX].mode = STAND;
			SetCharacterAnimation(charainfo[TEST_ENEMY_INDEX], enemy_anim_neutral);
		}

		ClearDrawScreen();

		// ==============================================================
		// 画面分割描画パス
		// ==============================================================
		for (int pIdx = 0; pIdx < PLAYER_COUNT; pIdx++) {
			int startX = (pIdx == 0) ? 0 : 450;
			int endX = (pIdx == 0) ? 450 : 900;

			SetDrawArea(startX, 0, endX, 600);

			VECTOR pPos = charainfo[pIdx].pos;
			VECTOR cPos, cTarget;

			// エイム中と通常時のカメラ
			if (isAiming[pIdx]) {
				float dist = 280.0f;
				float height = 180.0f;

				cPos = VGet(
					pPos.x - sinf(aimAngle[pIdx]) * dist,
					pPos.y + height,
					pPos.z - cosf(aimAngle[pIdx]) * dist
				);

				cTarget = VAdd(pPos, VGet(sinf(aimAngle[pIdx]) * 350.0f, 40.0f, cosf(aimAngle[pIdx]) * 350.0f));
			}
			else {
				if (pIdx == 0) {
					cTarget = VAdd(pPos, VGet(300.0f, 150.0f, 0.0f));
					cPos = VAdd(cTarget, VGet(-200.0f, 200.0f, -600.0f));
				}
				else {
					cTarget = VAdd(pPos, VGet(-300.0f, 150.0f, 0.0f));
					cPos = VAdd(cTarget, VGet(200.0f, 200.0f, -600.0f));
				}
			}

			SetCameraPositionAndTargetAndUpVec(cPos, cTarget, VGet(0.0f, 1.0f, 0.0f));

			// 3Dモデル描画
			for (int i = 0; i < MAX_CHARA; i++) {
				if (charainfo[i].mode != NONE && charainfo[i].model1 != -1) {
					MV1DrawModel(charainfo[i].model1);
				}
			}
			MV1DrawModel(stagedata);
			MV1DrawModel(sky);

			for (int i = 0; i < PLAYER_COUNT; i++) {
				if (playerWeaponModel[i] != -1) MV1DrawModel(playerWeaponModel[i]);
				if (playerSayaModel[i] != -1)   MV1DrawModel(playerSayaModel[i]);
			}

			// 閃光弾
			for (int f = 0; f < MAX_THROWN_FLASH; f++) {
				if (thrownFlashes[f].active && thrownFlashes[f].model != -1) {
					MV1DrawModel(thrownFlashes[f].model);
				}
			}

			// 浮遊アイテム
			if (dropFlashItem.active && dropFlashItem.model != -1) {
				MV1DrawModel(dropFlashItem.model);
			}

			// 個別UI
			game.DrawUI(pIdx, 900, 600, charainfo, hpBarTex, crownGraphHandle);

			// ==========================================
			// エイム中の丸い照準マーカー（レティクル）
			// ==========================================
			if (isAiming[pIdx]) {
				int centerX = (startX + endX) / 2;
				int centerY = 300;

				DrawCircle(centerX, centerY, 18, GetColor(0, 255, 200), FALSE);
				DrawCircle(centerX, centerY, 19, GetColor(0, 255, 200), FALSE);
				DrawCircle(centerX, centerY, 3, GetColor(255, 255, 255), TRUE);

				DrawLine(centerX - 26, centerY, centerX - 12, centerY, GetColor(0, 255, 200), 2);
				DrawLine(centerX + 12, centerY, centerX + 26, centerY, GetColor(0, 255, 200), 2);
				DrawLine(centerX, centerY - 26, centerX, centerY - 12, GetColor(0, 255, 200), 2);
				DrawLine(centerX, centerY + 12, centerX, centerY + 26, GetColor(0, 255, 200), 2);
			}

			// ホワイトアウト描画
			if (playerWhiteoutTimer[pIdx] > 0) {
				playerWhiteoutTimer[pIdx]--;
				int alpha = (playerWhiteoutTimer[pIdx] > 40) ? 255 : (playerWhiteoutTimer[pIdx] * 255 / 40);

				SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
				DrawBox(startX, 0, endX, 600, GetColor(255, 255, 255), TRUE);
				SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
			}
		}

		game.Update(charainfo, charainfo);

		SetDrawArea(0, 0, 900, 600);
		DrawLine(450, 0, 450, 600, GetColor(0, 0, 0), 2);
		game.DrawTimer(900, 600);

		ScreenFlip();
	}

	// ==========================================
	// 終了処理（メモリ解放）
	// ==========================================
	MV1TerminateCollInfo(stagedata, -1);

	for (int i = 0; i < MAX_CHARA; i++) {
		if (charainfo[i].model1 != -1) {
			MV1DeleteModel(charainfo[i].model1);
			charainfo[i].model1 = -1;
		}
	}
	if (baseGoblinModel != -1) MV1DeleteModel(baseGoblinModel);
	if (redGoblinBaseModel != -1) MV1DeleteModel(redGoblinBaseModel);
	if (stagedata != -1) MV1DeleteModel(stagedata);
	if (sky != -1) MV1DeleteModel(sky);

	for (int i = 0; i < PLAYER_COUNT; i++) {
		if (playerWeaponModel[i] != -1) MV1DeleteModel(playerWeaponModel[i]);
		if (playerSayaModel[i] != -1) MV1DeleteModel(playerSayaModel[i]);
	}

	for (int f = 0; f < MAX_THROWN_FLASH; f++) {
		if (thrownFlashes[f].model != -1) {
			MV1DeleteModel(thrownFlashes[f].model);
			thrownFlashes[f].model = -1;
		}
	}
	if (dropFlashItem.model != -1) {
		MV1DeleteModel(dropFlashItem.model);
		dropFlashItem.model = -1;
	}
	if (grenadeBaseModel != -1) MV1DeleteModel(grenadeBaseModel);

	DxLib_End();

	return 0;
}

void CheckAttackHit(GameManager& game, SCharaInfo* charainfo, SCharaInfo* attacker, SCharaInfo* target, VECTOR start, VECTOR end, int SEdamageHandle, int anim_damage) {
	if (attacker->mode == ATTACK && target->mode != DOWNMODE && target->mode != DAMAGE)
	{
		if (attacker->isHit == true) {
			return;
		}

		if (HitCheck_Capsule_Capsule(start, end, 30.0f,
			target->pos,
			VAdd(target->pos, VGet(0, target->charahitinfo.Height, 0)),
			target->charahitinfo.Width / 2 + 30.0f))
		{
			attacker->isHit = true;
			PlaySoundMem(SEdamageHandle, DX_PLAYTYPE_BACK);

			if (target == &charainfo[0] || target == &charainfo[1]) {
				if (target->HP > 0) {
					target->HP--;
				}

				MV1DetachAnim(target->model1, target->attachidx);
				target->attachidx = MV1AttachAnim(target->model1, 0, anim_damage);
				target->anim_totaltime = MV1GetAttachAnimTotalTime(target->model1, target->attachidx);
				target->playtime = 0.0f;
				target->mode = DAMAGE;
			}
			else {
				if (target->enemyHP > 0) {
					target->enemyHP--;

					if (target->enemyHP <= 0) {
						target->mode = NONE;
						MV1SetVisible(target->model1, FALSE);

						if (attacker == &charainfo[0]) {
							game.AddScore(0, 100);
							game.AddScorePopup(0, 100);
						}
						else if (attacker == &charainfo[1]) {
							game.AddScore(1, 100);
							game.AddScorePopup(1, 100);
						}
					}
					else {
						MV1DetachAnim(target->model1, target->attachidx);
						target->attachidx = MV1AttachAnim(target->model1, 0, anim_damage);
						target->anim_totaltime = MV1GetAttachAnimTotalTime(target->model1, target->attachidx);
						target->playtime = 0.0f;
						target->mode = DAMAGE;
					}
				}
			}
		}
	}
}