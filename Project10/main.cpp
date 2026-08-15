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

		// 先に新しく置いた Player フォルダを読む。
		sprintf_s(path, sizeof(path), "..\\Player\\%s", fileName);
		int handle = MV1LoadModel(path);
		if (handle != -1) {
			return handle;
		}

		// 見つからない場合は、既存の Data\\Player フォルダから読む。
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
	int		anim_neutral, anim_run, anim_jumpin, anim_jumploop, anim_jumpout, anim_damage, anim_down, enemy_anim_attack, enemy_anim_walk, enemy_anim_neutral{};

	int anim_attack[PLAYER_ATTACK_ANIM_COUNT];
	int		stagedata;
	int sky;
	float skyRot = 0;
	static SCharaInfo charainfo[MAX_CHARA];
	int playerWeaponModel[PLAYER_COUNT], playerWeaponFrame[PLAYER_COUNT];
	int playerSayaModel[PLAYER_COUNT], playerSayaFrame[PLAYER_COUNT];
	VECTOR wpPosStart[PLAYER_COUNT], wpPosEnd[PLAYER_COUNT];
	int prevJKey = 0;
	int isBGMPlaying = 1;
	PlayerRuntimeState playerStates[PLAYER_COUNT];
	
	PlayerInputConfig playerInputs[PLAYER_COUNT] = {
		// 1P: キーボード(WASD) + 1Pゲームパッド。
		{ DX_INPUT_PAD1, KEY_INPUT_W, KEY_INPUT_S, KEY_INPUT_A, KEY_INPUT_D, KEY_INPUT_SPACE, KEY_INPUT_Q, PAD_INPUT_1, PAD_INPUT_2 },
		// 2P: キーボード(矢印) + 2Pゲームパッド。
		{ DX_INPUT_PAD2, KEY_INPUT_UP, KEY_INPUT_DOWN, KEY_INPUT_LEFT, KEY_INPUT_RIGHT, KEY_INPUT_RETURN, -1, PAD_INPUT_1, PAD_INPUT_2 }
	};


	int enemyCount = 0;          // 現在の敵の数
//	int spawnTimer = 0;          // 出現までのカウント用
	const int SPAWN_INTERVAL = 300; // 出現間隔

	float attackInEndTime[PLAYER_ATTACK_ANIM_COUNT] = { ATTACK_FIRST_ENDTIME, ATTACK_SECOND_ENDTIME, ATTACK_THIERD_ENDTIME };

	GameManager game;
	VECTOR stagepos = VGet(0.0f, 2000.0f, 0.0f);

	

	// ステージコリジョン情報
	MV1_COLL_RESULT_POLY_DIM HitDim;
	int WallNum;
	int FloorNum;										// 床ポリゴンと判断されたポリゴンの数
	MV1_COLL_RESULT_POLY* Wall[CHARA_MAX_HITCOLL];
	MV1_COLL_RESULT_POLY* Floor[CHARA_MAX_HITCOLL];
	int HitFlag = 0;
	int timeLimit = 1000; // ゲーム制限時間など（必要であれば適宜設定）
	static int spawnTimer = 0;
	spawnTimer++; // 毎フレーム加算
	MV1_COLL_RESULT_POLY* Poly;
	HITRESULT_LINE LineRes;

	// キャラがヒットした床のポリゴン表示の座標
	VECTOR PolyCharaHitField[3];



	char BGM0_FilePath[] = "BGM_stg0.ogg";	// BGMファイル名
	char String[256];						// メモリ展開する際に使う文字列
	int BGMSoundHandle;						// BGMサウンドハンドル
	int BGMLoopStartPosition = -1;
	int BGMLoopEndPosition = -1;

	char SEattack_FilePath[] = "swish_00.wav", SEjump_FilePath[] = "jumpIn_00.wav", SEdamage_FilePath[] = "dmg_bySword_00.wav";	// SEファイル名
	int SEattackHandle, SEjumpHandle, SEdamageHandle;						// BGMサウンドハンドル	


//キャラ情報

	// 0番を1P、1番を2P、2番をテスト用敵として使う。
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

	charainfo[PLAYER1_INDEX].pos = VGet(1300.0f, 10.0f, 100.0f);
	charainfo[PLAYER2_INDEX].pos = VGet(1300.0f, 0.0f, -400.0f);
	charainfo[TEST_ENEMY_INDEX].pos = VGet(750.0f, 0.0f, -150.0f); // 敵は真ん中あたり
	charainfo[TEST_ENEMY_GOLEM].pos = VGet(750.0f, 0.0f, -150.0f); // 敵は真ん中あたり
	for (int i = 0; i < PLAYER_COUNT; i++) {
		charainfo[i].mode = STAND;
		charainfo[i].HP = 6;
		charainfo[i].charahitinfo.Height = PC_HEIGHT * PLAYER_MODEL_SCALE;
		charainfo[i].charahitinfo.Width = PC_WIDTH * PLAYER_MODEL_SCALE;
		charainfo[i].charahitinfo.CenterPosition = charainfo[i].pos; // 座標確定後に代入
	}

	charainfo[TEST_ENEMY_INDEX].mode = STAND;
	charainfo[TEST_ENEMY_INDEX].enemyHP = 6;
	charainfo[TEST_ENEMY_INDEX].charahitinfo.CenterPosition = charainfo[TEST_ENEMY_INDEX].pos;

	//モデル座標初期セット
	VECTOR pos[2] = { VGet(1300.0f, 10.0f, 100.0f),VGet(1300.0f, 0.0f, -400.0f )};

	

	//サウンドファイルの読込みストリーミング設定にする
	SetCreateSoundDataType(DX_SOUNDDATATYPE_FILE);

	// ウインドウモードの切り替え
	ChangeWindowMode(TRUE);

	// ウインドウサイズの変更
	SetGraphMode(900, 600, 32);
	// DXライブラリの初期化
	if (DxLib_Init() == -1) {
		
		return -1;
	}

	int baseGoblinModel = MV1LoadModel("..\\Data\\Goblin\\Goblin.mv1");

	enemy_anim_neutral = baseGoblinModel;
	if (baseGoblinModel == -1) {
		printfDx("ゴブリンのベースモデル読み込み失敗！\n");
	}

	for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
		if (baseGoblinModel != -1) {
			// モデルを複製
			charainfo[i].model1 = MV1DuplicateModel(baseGoblinModel);
		}
		else {
			charainfo[i].model1 = -1;
		}

		if (charainfo[i].model1 == -1) {
			printfDx("ゴブリンのモデル生成失敗！(index:%d)\n", i);
			continue;
		}

		// 最初は非表示
		MV1SetVisible(charainfo[i].model1, FALSE);
		charainfo[i].mode = NONE;

		// ★追加：複製したゴブリンに最初からニュートラルアニメーションをアタッチしておく
		charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, enemy_anim_neutral);
		if (charainfo[i].attachidx != -1) {
			charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
		}
		charainfo[i].playtime = 0.0f;
	}
	//モデル読み込み
	for (int i = 0; i < PLAYER_COUNT; i++) {
		charainfo[i].model1 = LoadPlayerAssetModel("PC.mv1");
		if (charainfo[i].model1 == -1) {
			printfDx("プレイヤー%dのモデル読み込み失敗！\n", i + 1);
			return -1;
		}
		MV1SetPosition(charainfo[i].model1, charainfo[i].pos);
		MV1SetScale(charainfo[i].model1, VGet(PLAYER_MODEL_SCALE, PLAYER_MODEL_SCALE, PLAYER_MODEL_SCALE));
	}
	// 2P は仮で少し青くして、同じモデルでも見分けやすくする。
	MV1SetMaterialDrawAddColorAll(charainfo[PLAYER2_INDEX].model1, 0, 0, 60);

	charainfo[TEST_ENEMY_GOLEM].model1 = MV1LoadModel("..\\Data\\Golem\\Golem.mv1");
	if (charainfo[TEST_ENEMY_GOLEM].model1 == -1) {
		printfDx("ゴーレムのモデル読み込み失敗！\n");
	}
	else {
		// 最初は非表示にしておく
		MV1SetVisible(charainfo[TEST_ENEMY_GOLEM].model1, FALSE);
		charainfo[TEST_ENEMY_GOLEM].mode = NONE;
		MV1SetScale(charainfo[TEST_ENEMY_GOLEM].model1, VGet(1.0f, 1.0f, 1.0f)); // 必要ならサイズ調整
	}
	

	
	
	//ルートフレーム
	for (int i = 0; i < PLAYER_COUNT; i++) {
		rootflm = MV1SearchFrame(charainfo[i].model1, "root");
		if (rootflm != -1) {
			MV1SetFrameUserLocalMatrix(charainfo[i].model1, rootflm, MGetIdent());
		}
		else {
			printfDx("Player%d frame not found: root\n", i + 1);
		}
	}
	rootflm = MV1SearchFrame(charainfo[TEST_ENEMY_INDEX].model1, "root");
	if (rootflm != -1) {
		MV1SetFrameUserLocalMatrix(charainfo[TEST_ENEMY_INDEX].model1, rootflm, MGetIdent());
	}

	for (int i = 0; i < PLAYER_COUNT; i++) {
		//武器モデル
		playerWeaponModel[i] = LoadPlayerAssetModel("Sabel.mv1");
		if (playerWeaponModel[i] == -1) return -1;
		//武器フレーム
		playerWeaponFrame[i] = MV1SearchFrame(charainfo[i].model1, "wp");
		if (playerWeaponFrame[i] == -1) {
			printfDx("Player%d frame not found: wp\n", i + 1);
		}
		//鞘モデル
		playerSayaModel[i] = LoadPlayerAssetModel("Saya.mv1");
		if (playerSayaModel[i] == -1) return -1;
		//鞘フレーム
		playerSayaFrame[i] = MV1SearchFrame(charainfo[i].model1, "sayabone");
		if (playerSayaFrame[i] == -1) {
			printfDx("Player%d frame not found: sayabone\n", i + 1);
		}
	}


	// ステージ情報の読み込み
	stagedata = MV1LoadModel("..\\Data\\Stage\\Stage.mv1");
	if (stagedata == -1) return -1;
	MV1SetPosition(stagedata, stagepos);
	sky = MV1LoadModel("..\\Data\\Stage\\Stage00_sky.mv1");
	if (sky == -1) return -1;
	MV1SetPosition(sky, VGet(0, -1000, 0));

	// モデル全体のコリジョン情報のセットアップ
	MV1SetupCollInfo(stagedata, -1);

	int MeshNum;

	// モデルに含まれるメッシュの数を取得する
	MeshNum = MV1GetMeshNum(stagedata);



	anim_neutral = LoadPlayerAssetModel("Anim_Neutral.mv1");
	if (anim_neutral == -1) return -1;
	anim_run = LoadPlayerAssetModel("Anim_Run.mv1");
	if (anim_run == -1) return -1;
	anim_jumpin = LoadPlayerAssetModel("Anim_Jump_In.mv1");
	if (anim_jumpin == -1) return -1;
	anim_jumploop = LoadPlayerAssetModel("Anim_Jump_Loop.mv1");
	if (anim_jumploop == -1) return -1;
	anim_jumpout = LoadPlayerAssetModel("Anim_Jump_Out.mv1");
	if (anim_jumpout == -1) return -1;
	anim_attack[0] = LoadPlayerAssetModel("Anim_Attack1.mv1");
	if (anim_attack[0] == -1) return -1;
	anim_attack[1] = LoadPlayerAssetModel("Anim_Attack2.mv1");
	if (anim_attack[1] == -1) return -1;
	anim_attack[2] = LoadPlayerAssetModel("Anim_Attack3.mv1");
	if (anim_attack[2] == -1) return -1;
	anim_damage = LoadPlayerAssetModel("Anim_Damage.mv1");
	if (anim_damage == -1) return -1;
	anim_down = LoadPlayerAssetModel("Anim_Down_Loop.mv1");
	if (anim_down == -1) return -1;
	enemy_anim_attack = MV1LoadModel("..\\Data\\Goblin\\Anim_Attack1.mv1");		// 被撃アニメ
	if (enemy_anim_attack == -1) return -1;
	enemy_anim_walk = MV1LoadModel("..\\Data\\Goblin\\Anim_Walk.mv1");		// 被撃アニメ
	if (enemy_anim_walk == -1) return -1;

	enemy_anim_neutral = MV1LoadModel("..\\Data\\Goblin\\Anim_Neutral.mv1");		// 被撃アニメ
	if (enemy_anim_neutral == -1) return -1;

	



	for (int i = 0; i < PLAYER_COUNT; i++) {
		charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_neutral);
		charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
	}
	charainfo[TEST_ENEMY_INDEX].attachidx = MV1AttachAnim(charainfo[TEST_ENEMY_INDEX].model1, 0, enemy_anim_neutral);
	charainfo[TEST_ENEMY_INDEX].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[TEST_ENEMY_INDEX].model1, charainfo[TEST_ENEMY_INDEX].attachidx);


	SetDrawScreen(DX_SCREEN_BACK);
	//カメラの初期化
	//SetCameraPositionAndTargetAndUpVec(cpos, ctgt, VGet(0.0f, 1.0f, 0.0f));

	// ＢＧＭ用のサウンドファイルを読み込む
	sprintf_s(String, SOUND_DIRECTORY_PATH "BGM\\%s", BGM0_FilePath);
	BGMSoundHandle = LoadSoundMem(String);
	// 読み込みに失敗したらエラー
	if (BGMSoundHandle == -1) {
		return false;
	}
	// 有効なループポイントがある場合はループ再生する
	PlaySoundMem(BGMSoundHandle,
		BGMLoopStartPosition >= 0 ? DX_PLAYTYPE_LOOP : DX_PLAYTYPE_BACK);

	sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Weapon\\Sword\\%s", SEattack_FilePath);
	SEattackHandle = LoadSoundMem(String);
	// 読み込みに失敗したらエラー
	if (SEattackHandle == -1) {
		return false;
	}
	sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Player\\%s", SEdamage_FilePath);
	SEdamageHandle = LoadSoundMem(String);
	// 読み込みに失敗したらエラー
	if (SEdamageHandle == -1) {
		return false;
	}
	sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Player\\%s", SEjump_FilePath);
	SEjumpHandle = LoadSoundMem(String);
	// 読み込みに失敗したらエラー
	if (SEjumpHandle == -1) {
		return false;
	}

	while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0) {



		int currentJKey = CheckHitKey(KEY_INPUT_J);


		for (int i = 0; i < PLAYER_COUNT; i++) {
			// 1P/2P の再生時間と待機復帰を共通処理する。
			UpdatePlayerAnimationProgress(charainfo[i], playerStates[i], anim_neutral);
		}
		for (int i = 0; i < PLAYER_COUNT; i++) {
			// HPが0以下で、まだDOWNMODEになっていない場合
			if (charainfo[i].HP <= 0 && charainfo[i].mode != DOWNMODE) {

				// 状態をDOWNMODEへ
				charainfo[i].mode = DOWNMODE;

				// 死亡アニメーション処理など
				MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
				charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_down);
				charainfo[i].playtime = 0.0f;

				// ★ここでカウントする
				game.AddDeath(i);
			}
		}
		for (int i = 0; i < PLAYER_COUNT; i++) {
			// 死亡状態(DOWNMODE)で、ジャンプボタン（1P:SPACE, 2P:RETURN）が押されたら
			if (charainfo[i].mode == DOWNMODE && CheckHitKey(playerInputs[i].jumpKey) == 1) {

				// 1. HPを回復
				charainfo[i].HP = 6;

				// 2. モードを STAND に戻す
				charainfo[i].mode = STAND;

				// 3. アニメーションを通常に戻す
				MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
				charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_neutral);
				charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
				charainfo[i].playtime = 0.0f;
			}
		}
		// ==========================================
			// 敵（ゴブリン）のAI・移動・アニメーション処理
			// ==========================================
		for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
			if (charainfo[i].mode == NONE) continue;

			// もしダウンしている場合
			else if (charainfo[i].mode == DAMAGE) {
				// 被弾モーションを最後まで再生したら STAND に戻す
				if (charainfo[i].playtime >= charainfo[i].anim_totaltime) {
					charainfo[i].mode = STAND;
					charainfo[i].playtime = 0.0f;
					charainfo[i].isHit = false;

					if (charainfo[i].attachidx != -1) {
						MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
					}
					charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, enemy_anim_neutral);
					if (charainfo[i].attachidx != -1) {
						charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
					}
				}
			}

			// 1. プレイヤーとの距離を測り、近い方をターゲットにする
			int enemyTargetIndex = PLAYER1_INDEX;
			float enemyDistP1 = VSize(VSub(charainfo[PLAYER1_INDEX].pos, charainfo[i].pos));
			float enemyDistP2 = VSize(VSub(charainfo[PLAYER2_INDEX].pos, charainfo[i].pos));
			if (enemyDistP2 < enemyDistP1) {
				enemyTargetIndex = PLAYER2_INDEX;
			}

			VECTOR dir = VSub(charainfo[enemyTargetIndex].pos, charainfo[i].pos);
			dir.y = 0;
			float dist = VSize(dir);

			float angle = atan2f(dir.x, dir.z);
			MV1SetRotationXYZ(charainfo[i].model1, VGet(0.0f, angle, 0.0f));

			if (charainfo[i].mode == STAND) {
				if (dist > 150.0f) {
					VECTOR moveDir = VNorm(dir);
					moveDir.y = 0.0f;
					charainfo[i].pos = VAdd(charainfo[i].pos, VScale(moveDir, 1.5f));
					MV1SetPosition(charainfo[i].model1, charainfo[i].pos);
				}
				else {
					charainfo[i].mode = ATTACK;
					charainfo[i].playtime = 0.0f;
					charainfo[i].isHit = false;

					if (charainfo[i].attachidx != -1) {
						MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
					}
					charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, enemy_anim_attack);
					if (charainfo[i].attachidx != -1) {
						charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
					}
				}
			}
			else if (charainfo[i].mode == ATTACK) {
				if (charainfo[i].playtime >= charainfo[i].anim_totaltime * 0.4f &&
					charainfo[i].playtime <= charainfo[i].anim_totaltime * 0.6f)
				{
					if (charainfo[i].isHit == false) {
						for (int p = 0; p < PLAYER_COUNT; p++) {
							if (charainfo[p].mode == DOWNMODE) continue;

							if (HitCheck_Capsule_Capsule(
								charainfo[i].pos, VAdd(charainfo[i].pos, VGet(0, 50, 0)), 60.0f,
								charainfo[p].pos, VAdd(charainfo[p].pos, VGet(0, charainfo[p].charahitinfo.Height, 0)), charainfo[p].charahitinfo.Width / 2))
							{
								charainfo[p].HP -= 1;
								charainfo[i].isHit = true;
								printfDx("プレイヤー%dがゴブリンの攻撃を受けた！ HP:%d\n", p + 1, charainfo[p].HP);
								break;
							}
						}
					}
				}

				if (charainfo[i].playtime >= charainfo[i].anim_totaltime) {
					charainfo[i].mode = STAND;
					charainfo[i].playtime = 0.0f;
					charainfo[i].isHit = false;

					if (charainfo[i].attachidx != -1) {
						MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
					}
					charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, enemy_anim_neutral);
					if (charainfo[i].attachidx != -1) {
						charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
					}
				}
			}

			// アニメーション時間の進行
			charainfo[i].playtime += 0.5f;
			if (charainfo[i].attachidx != -1) {
				MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
			}
		}


		// キー操作
		for (int i = 0; i < PLAYER_COUNT; i++) {
			// 1P/2P の入力、移動、待機/走り切替、攻撃予約を共通処理する。
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
		}

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
			// 距離の近いプレイヤーを攻撃対象にする。
			float dist = VSize(VSub(charainfo[enemyTargetIndex].pos, charainfo[TEST_ENEMY_INDEX].pos));
			if (dist < 150.0f) {
				charainfo[TEST_ENEMY_INDEX].mode = ATTACK;
				SetCharacterAnimation(charainfo[TEST_ENEMY_INDEX], enemy_anim_attack);
			}
		}

		else if (charainfo[TEST_ENEMY_INDEX].mode == ATTACK) {

			// 攻撃アニメーションの「振り下ろし」タイミング
			if (charainfo[TEST_ENEMY_INDEX].playtime >= charainfo[TEST_ENEMY_INDEX].anim_totaltime * 0.4f &&
				charainfo[TEST_ENEMY_INDEX].playtime <= charainfo[TEST_ENEMY_INDEX].anim_totaltime * 0.6f)
			{
				// 攻撃がまだ一度も当たっていない場合のみ判定
				if (charainfo[TEST_ENEMY_INDEX].isHit == false)
				{
					for (int i = 0; i < PLAYER_COUNT; i++) {
						// テスト敵の攻撃は、範囲内のどちらのプレイヤーにも当たる。
						if (HitCheck_Capsule_Capsule(
							charainfo[TEST_ENEMY_INDEX].pos, VAdd(charainfo[TEST_ENEMY_INDEX].pos, VGet(0, 50, 0)), 60.0f,
							charainfo[i].pos, VAdd(charainfo[i].pos, VGet(0, charainfo[i].charahitinfo.Height, 0)), charainfo[i].charahitinfo.Width / 2))
						{
							charainfo[i].HP -= 1; // ダメージ発生
							charainfo[TEST_ENEMY_INDEX].isHit = true; // フラグを立てて連続ヒットを防止
							printfDx("プレイヤー%d被弾！HP:%d\n", i + 1, charainfo[i].HP);
							break;
						}
					}
				}
			}

			// アニメーションが終わったらフラグをリセットして通常モードへ
			if (charainfo[TEST_ENEMY_INDEX].playtime >= charainfo[TEST_ENEMY_INDEX].anim_totaltime) {
				charainfo[TEST_ENEMY_INDEX].mode = STAND;
				SetCharacterAnimation(charainfo[TEST_ENEMY_INDEX], enemy_anim_neutral);
				charainfo[TEST_ENEMY_INDEX].isHit = false;
			}
		}
		HitDim = MV1CollCheck_Sphere(stagedata, -1, charainfo[0].pos, CHARA_ENUM_DEFAULT_SIZE + VSize(charainfo[0].move));
		WallNum = 0;
		FloorNum = 0;
		// 検出されたポリゴンの数だけ繰り返し
		for (int i = 0; i < HitDim.HitNum; i++) {
			// ＸＺ平面に垂直かどうかはポリゴンの法線のＹ成分が０に限りなく近いかどうかで判断する
			if (HitDim.Dim[i].Normal.y < 0.000001f && HitDim.Dim[i].Normal.y > -0.000001f) {
				printf("壁扱い\n");
				// 壁ポリゴンと判断された場合でも、キャラクターのＹ座標＋１．０ｆより高いポリゴンのみ当たり判定を行う
				if (HitDim.Dim[i].Position[0].y > charainfo[0].pos.y + 1.0f ||
					HitDim.Dim[i].Position[1].y > charainfo[0].pos.y + 1.0f ||
					HitDim.Dim[i].Position[2].y > charainfo[0].pos.y + 1.0f) {
					// ポリゴンの数が列挙できる限界数に達していなかったらポリゴンを配列に追加
					if (WallNum < CHARA_MAX_HITCOLL) {
						// ポリゴンの構造体のアドレスを壁ポリゴンポインタ配列に保存する
						Wall[WallNum] = &HitDim.Dim[i];

						// 壁ポリゴンの数を加算する
						WallNum++;
					}
				}
			}
			else {
				// ポリゴンの数が列挙できる限界数に達していなかったらポリゴンを配列に追加
				if (FloorNum < CHARA_MAX_HITCOLL) {
					// ポリゴンの構造体のアドレスを床ポリゴンポインタ配列に保存する
					Floor[FloorNum] = &HitDim.Dim[i];

					// 床ポリゴンの数を加算する
					FloorNum++;
				}
			}
		}
		float MaxY;
		float MaxY_poly;

		if (HitCheck_Capsule_Capsule(
			VAdd(charainfo[PLAYER1_INDEX].pos, charainfo[PLAYER1_INDEX].move),
			VAdd(VAdd(charainfo[PLAYER1_INDEX].pos, charainfo[PLAYER1_INDEX].move), VGet(0, charainfo[PLAYER1_INDEX].charahitinfo.Height, 0)),
			charainfo[PLAYER1_INDEX].charahitinfo.Width / 2,
			VAdd(charainfo[PLAYER2_INDEX].pos, charainfo[PLAYER2_INDEX].move),
			VAdd(VAdd(charainfo[PLAYER2_INDEX].pos, charainfo[PLAYER2_INDEX].move), VGet(0, charainfo[PLAYER2_INDEX].charahitinfo.Height, 0)),
			charainfo[PLAYER2_INDEX].charahitinfo.Width / 2)
			== TRUE) {
			// プレイヤー同士が重なりそうな場合は、横移動だけ止める。
			charainfo[PLAYER1_INDEX].move.x = 0.0f;
			charainfo[PLAYER1_INDEX].move.z = 0.0f;
			charainfo[PLAYER2_INDEX].move.x = 0.0f;
			charainfo[PLAYER2_INDEX].move.z = 0.0f;
		}

		// プレイヤー・敵全体をループさせる
		for (int i = 0; i < PLAYER_COUNT; i++) {
			// 自分が倒れているなら、移動判定自体を行わない
			if (charainfo[i].mode == DOWNMODE) continue;

			// 内側のループを MAX_CHARA (全キャラ) まで回す
			for (int j = 0; j < MAX_CHARA; j++) {
				// 自分自身との判定はスキップする
				if (i == j) continue;

				// 相手が「生成されていない(NONE)」なら無視する
				if (charainfo[j].mode == NONE) continue;

				// 相手が倒れているなら、当たり判定の対象外にする
				if (charainfo[j].mode == DOWNMODE) continue;

				// 当たり判定の実行（HeightとWidthのタイポを修正）
				if (HitCheck_Capsule_Capsule(
					VAdd(charainfo[i].pos, charainfo[i].move),
					VAdd(VAdd(charainfo[i].pos, charainfo[i].move), VGet(0, charainfo[i].charahitinfo.Height, 0)),
					charainfo[i].charahitinfo.Width / 2,
					charainfo[j].pos,
					VAdd(charainfo[j].pos, VGet(0, charainfo[j].charahitinfo.Height, 0)),
					charainfo[j].charahitinfo.Width / 2)
					== TRUE)
				{
					// 当たったら移動をリセット
					charainfo[i].move.x = 0.0f;
					charainfo[i].move.z = 0.0f;

					// 一度当たったらそれ以上このループを回す必要はないので抜ける
					break;
				}
			}
		}
		// 床ポリゴンとの当たり判定
		if (FloorNum != 0) {
			// 床ポリゴンに当たったかどうかのフラグを倒しておく
			HitFlag = 0;
			// 一番高い床ポリゴンにぶつける為の判定用変数を初期化
			MaxY = 0.0f;
			MaxY_poly = 0.0f;

			// 床ポリゴンの数だけ繰り返し
			for (int i = 0; i < FloorNum; i++) {
				// i番目の床ポリゴンのアドレスを床ポリゴンポインタ配列から取得
				Poly = Floor[i];

				VECTOR cal_pos1 = VAdd(charainfo[0].pos, VGet(0.0f, PC_HEIGHT, 0.0f));
				VECTOR cal_pos2 = VAdd(charainfo[0].pos, VGet(0.0f, -5.0f, 0.0f));
				// 走っている場合は頭の先からそこそこ低い位置の間で当たっているかを判定( 傾斜で落下状態に移行してしまわない為 )
				LineRes = HitCheck_Line_Triangle(cal_pos1, cal_pos2, Poly->Position[0], Poly->Position[1], Poly->Position[2]);

				// 当たっていなかったら何もしない
				if (LineRes.HitFlag == TRUE) {
					PolyCharaHitField[0] = Poly->Position[0];
					PolyCharaHitField[1] = Poly->Position[1];
					PolyCharaHitField[2] = Poly->Position[2];
				}
				else {
					continue;
				}

				// 既に当たったポリゴンがあり、且つ今まで検出した床ポリゴンより低い場合は何もしない
				if (HitFlag == 1 && MaxY > LineRes.Position.y) {
					continue;
				}

				// ポリゴンに当たったフラグを立てる
				HitFlag = 1;

				// 接触したＹ座標を保存する
				MaxY = LineRes.Position.y;
				MaxY_poly = Poly->Position[1].y;
			}
		}
		if (HitFlag == 1) {

			charainfo[0].move.y = MaxY - charainfo[0].pos.y;

			if (charainfo[0].mode == JUMPLOOP || charainfo[0].mode == FALL) {
				{
					MV1DetachAnim(charainfo[0].model1, charainfo[0].attachidx);
					charainfo[0].mode = JUMPOUT;
					charainfo[0].playtime = 0.0f;
					charainfo[0].attachidx = MV1AttachAnim(charainfo[0].model1, 0, anim_jumpout);
					charainfo[0].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[0].model1, charainfo[0].attachidx);
					charainfo[0].move.x = 0.0f;
					charainfo[0].move.y = 0.0f;
					charainfo[0].move.z = 0.0f;
				}

			}
			else if (charainfo[0].mode == JUMPIN) {
				if (charainfo[0].playtime > charainfo[0].anim_totaltime) {
					MV1DetachAnim(charainfo[0].model1, charainfo[0].attachidx);
					charainfo[0].attachidx = MV1AttachAnim(
						charainfo[0].model1, 0, anim_jumploop);
					charainfo[0].anim_totaltime = MV1GetAttachAnimTotalTime(
						charainfo[0].model1, charainfo[0].attachidx);
					charainfo[0].mode = JUMPLOOP;
					charainfo[0].move.y = 15.0f;
					//ジャンプ直後の地面めり込みを避けるため
					charainfo[0].pos.y += charainfo[0].move.y;
				}
			}
		}
		else
		{
			// アニメのループ管理
			if (charainfo[0].mode != JUMPLOOP && charainfo[0].mode != FALL) {
				MV1DetachAnim(charainfo[0].model1, charainfo[0].attachidx);
				charainfo[0].mode = FALL;
				charainfo[0].attachidx = MV1AttachAnim(charainfo[0].model1, 0, anim_jumploop);
				charainfo[0].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[0].model1, charainfo[0].attachidx);
				charainfo[0].playtime = 7.0f;
				MV1SetAttachAnimTime(charainfo[0].model1, charainfo[0].attachidx, charainfo[0].playtime);
			}
		}


		// ジャンプ中だったら重力追加させる
		if (charainfo[0].mode == FALL || charainfo[0].mode == JUMPLOOP) {
			charainfo[0].move.y -= GRAVITY;
		}


		// 検出したキャラクターの周囲のポリゴン情報を開放する
		MV1CollResultPolyDimTerminate(HitDim);

		// 移動処理
		for (int i = 0; i < PLAYER_COUNT; i++) {
			charainfo[i].pos.x += charainfo[i].move.x;
			charainfo[i].pos.y += charainfo[i].move.y;
			charainfo[i].pos.z += charainfo[i].move.z;
			// 移動後の座標を攻撃判定用の中心にも反映する。
			charainfo[i].charahitinfo.CenterPosition = charainfo[i].pos;
		}





	


		DrawTriangle3D(PolyCharaHitField[0], PolyCharaHitField[1], PolyCharaHitField[2], GetColor(255, 0, 0), TRUE);
		for (int i = 0; i < PLAYER_COUNT; i++) {
			MV1SetPosition(charainfo[i].model1, charainfo[
				i].pos);
			//鞘の座標更新
			if (playerSayaFrame[i] != -1 && playerSayaModel[i] != -1) {
				sayamatrix[i] = MV1GetFrameLocalWorldMatrix(charainfo[i].model1, playerSayaFrame[i]);
				MV1SetMatrix(playerSayaModel[i], sayamatrix[i]);
			}
			//武器の座標更新
			if (playerWeaponFrame[i] != -1 && playerWeaponModel[i] != -1) {
				wpmatrix[i] = MV1GetFrameLocalWorldMatrix(charainfo[i].model1, playerWeaponFrame[i]);
				MV1SetMatrix(playerWeaponModel[i], wpmatrix[i]);
				//攻撃判定の更新
				wpPosStart[i] = VGet(0.0f, 0.0f, 0.0f);
				wpPosEnd[i] = VGet(0.0f, -90.0f, 0.0f);
				wpPosStart[i] = VTransform(wpPosStart[i], wpmatrix[i]);
				wpPosEnd[i] = VTransform(wpPosEnd[i], wpmatrix[i]);
				for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; e++) {
					// 生成されていない敵（NONE）はスキップ
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




		// 画面の消去
		ClearDrawScreen();



		// 四角形を表示 最後の引数をfalseにすると塗りつぶし無し
		DrawBox(0, 0, 900, 600, GetColor(255, 255, 255), true);
	


		


		for (int pIdx = 0; pIdx < PLAYER_COUNT; pIdx++) {

			// 1. 左右の画面領域（ビューポート）を決定
			int startX = (pIdx == 0) ? 0 : 450;
			int endX = (pIdx == 0) ? 450 : 900;

			SetDrawArea(startX, 0, endX, 600);

			// 2. 現在のプレイヤーの座標を取得
			VECTOR pPos = charainfo[pIdx].pos;

			// 3. 【カメラ位置と注視点の調整（斜め上見下ろし視点）】
			VECTOR cPos, cTarget;

			if (pIdx == 0) {
				// 【プレイヤー1用（左画面）】
				// 注視点をプレイヤーの少し上にする
				cTarget = VAdd(pPos, VGet(300.0f, 150.0f, 0.0f));

				// 斜め上から見下ろす（X:少し横にずらす, Y:高さ, Z:後ろに離す）
				cPos = VAdd(cTarget, VGet(-200.0f, 450.0f, -900.0f));
			}
			else {
				// 【プレイヤー2用（右画面）】
				cTarget = VAdd(pPos, VGet(-300.0f, 150.0f, 0.0f));

				// プレイヤー2も同様に反対側の斜め上から見下ろす
				cPos = VAdd(cTarget, VGet(200.0f, 450.0f, -900.0f));
			}

			// 4. カメラを適用
			SetCameraPositionAndTargetAndUpVec(cPos, cTarget, VGet(0.0f, 1.0f, 0.0f));

			

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

			game.DrawUI(pIdx, 900, 600, charainfo);

			
		}
		SetDrawArea(0, 0, 900, 600);
		game.Update(charainfo, charainfo);
		ScreenFlip();
	}
	
	// DXライブラリの終了処理
	DxLib_End();


	return 0;
}


void CheckAttackHit(GameManager& game, SCharaInfo* charainfo, SCharaInfo* attacker, SCharaInfo* target, VECTOR start, VECTOR end, int SEdamageHandle, int anim_damage) {
	// 攻撃中かつ、ターゲットがダウン中・ダメージ硬直中・すでにHPが0以下の場合は判定しない
	if (attacker->mode == ATTACK && target->mode != DOWNMODE && target->mode != DAMAGE)
	{
		if (attacker->isHit == true) {
			return;
		}

		// 半径を 30.0f に拡大
		if (HitCheck_Capsule_Capsule(start, end, 30.0f,
			target->pos,
			VAdd(target->pos, VGet(0, target->charahitinfo.Height, 0)),
			target->charahitinfo.Width / 2 + 30.0f))
		{
			attacker->isHit = true;

			// 被弾アニメーションへ遷移
			MV1DetachAnim(target->model1, target->attachidx);
			target->attachidx = MV1AttachAnim(target->model1, 0, anim_damage);
			target->anim_totaltime = MV1GetAttachAnimTotalTime(target->model1, target->attachidx);
			target->playtime = 0.0f;
			target->mode = DAMAGE;

			// SE再生
			PlaySoundMem(SEdamageHandle, DX_PLAYTYPE_BACK);

			if (target == &charainfo[0] || target == &charainfo[1]) {
				// ターゲットがプレイヤーの場合
				if (target->HP > 0) {
					target->HP--;
				}
			}
			else {
				// ターゲットが敵の場合
				if (target->enemyHP > 0) {
					target->enemyHP--;

					// 敵が倒れたかチェック
					if (target->enemyHP <= 0) {
						// 確実にダウン状態に切り替える
						target->mode = DOWNMODE;

						// 攻撃者がプレイヤー1だったら
						if (attacker == &charainfo[0]) {
							game.AddScore(0, 100); // P1に100点加算
						}
						// 攻撃者がプレイヤー2だったら
						else if (attacker == &charainfo[1]) {
							game.AddScore(1, 100); // P2に100点加算
						}
					}
				}
			}
			printfDx("ヒット！残リHP:%d\n", (target == &charainfo[0] ? target->HP : target->enemyHP));
		}
	}
}

