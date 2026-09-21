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

	int redGoblinBaseModel = -1;


	int red_goblin_anim_neutral = -1;
	int red_goblin_anim_walk = -1;
	int red_goblin_anim_attack = -1;
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
	VECTOR stagepos = VGet(0.0f, 0.0f,0.0f);

	

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
	int hpBarTex = -1;
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
	// 初期化の場所（GameInit関数など）
	charainfo[0].deaths = 0;
	charainfo[1].deaths = 0;
	charainfo[PLAYER1_INDEX].pos = VGet(1300.0f, 800.0f, 100.0f);
	charainfo[PLAYER2_INDEX].pos = VGet(1300.0f, 800.0f, -400.0f);
	
	for (int i = 0; i < PLAYER_COUNT; i++) {
		charainfo[i].mode = STAND;

		charainfo[i].HP = 6;
		charainfo[i].charahitinfo.Height = PC_HEIGHT * PLAYER_MODEL_SCALE;
		charainfo[i].charahitinfo.Width = PC_WIDTH * PLAYER_MODEL_SCALE;
		charainfo[i].charahitinfo.CenterPosition = charainfo[i].pos; // 座標確定後に代入
	}

	charainfo[TEST_ENEMY_INDEX].mode = STAND;
	charainfo[TEST_ENEMY_INDEX].enemyHP = 2;
	charainfo[TEST_ENEMY_INDEX].charahitinfo.CenterPosition = charainfo[TEST_ENEMY_INDEX].pos;

	//モデル座標初期セット
	//VECTOR pos[2] = { VGet(1300.0f, 0.0f, 100.0f),VGet(1300.0f, 0.0f, 200.0f) };

	

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
	stagedata = MV1LoadModel("..\\Data\\Stage\\Stage_4545.mv1");
	if (stagedata == -1) return -1;
	MV1SetPosition(stagedata, stagepos);
	int meshNum = MV1GetMeshNum(stagedata);
	for (int i = 0; i < meshNum; i++)
	{
		MV1SetMeshBackCulling(stagedata, i, FALSE);
	}
	sky = MV1LoadModel("..\\Data\\Stage\\Stage00_sky.mv1");
	if (sky == -1) return -1;
	MV1SetPosition(sky, VGet(0, -1000, 0));

	// モデル全体のコリジョン情報のセットアップ
	MV1SetupCollInfo(stagedata, -1);

	int MeshNum;

	// モデルに含まれるメッシュの数を取得する
	MeshNum = MV1GetMeshNum(stagedata);
	SetTransColor(255, 255, 255);
	int crownGraphHandle = LoadGraph("..\\Data\\UI\\crown.png");

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
	game.enemy_anim_neutral = enemy_anim_neutral;
	
	SetTransColor(255, 255, 255);
	hpBarTex = LoadGraph("..\\Data\\UI\\Hpbar023.png"); // ※実際のファイルパスに合わせて調整してください

	if (hpBarTex == -1) {
		// 読み込みに失敗した場合のエラー処理（必要に応じて）
		printfDx("HPバーの画像読み込み失敗！\n");
	}


	 redGoblinBaseModel = MV1LoadModel("..\\Data\\RedGoblin\\RedGoblin.mv1");
	 red_goblin_anim_neutral = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Neutral.mv1");
	 red_goblin_anim_walk = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Walk.mv1");
	 red_goblin_anim_attack = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Attack1.mv1");


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

			// もしダメージ（被弾）中の場合
			if (charainfo[i].mode == DAMAGE) {
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

				// アニメーション時間を進める
				charainfo[i].playtime += 0.2f;
				if (charainfo[i].attachidx != -1) {
					MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
				}

				// ★ダメージ中のときは、下の通常の移動・攻撃AIに進ませないようにここでスキップ！
				continue;
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

			// ★後ろ向きになる場合はここに DX_PI_F を足して向きを合わせます
			float angle = atan2f(dir.x, dir.z) + DX_PI_F;
			MV1SetRotationXYZ(charainfo[i].model1, VGet(0.0f, angle, 0.0f));

			if (charainfo[i].mode == STAND) {
				if (dist > 150.0f) {
					VECTOR moveDir = VNorm(dir);
					moveDir.y = 0.0f;
					charainfo[i].pos = VAdd(charainfo[i].pos, VScale(moveDir, 1.5f));
				}

				// ==========================================
				// ★追加：他のゴブリンと密集しすぎたら押し返す処理
				// ==========================================
				for (int j = TEST_ENEMY_INDEX; j < MAX_CHARA; j++) {
					if (i == j) continue;
					if (charainfo[j].mode == NONE) continue;

					VECTOR diff = VSub(charainfo[i].pos, charainfo[j].pos);
					diff.y = 0.0f;
					float enemyDist = VSize(diff);

					// 判定距離を少し広げる（例: 70.0f）
					float minDistance = 170.0f;
					if (enemyDist < minDistance && enemyDist > 0.0001f) {
						VECTOR pushDir = VNorm(diff);
						// 押し出す力も少し強めにする（例: 1.5f）
						charainfo[i].pos = VAdd(charainfo[i].pos, VScale(pushDir, 1.5f));
					}
				}
				// ==========================================



				MV1SetPosition(charainfo[i].model1, charainfo[i].pos);

				// 150px以内に入ったら攻撃へ移行
				if (dist <= 150.0f) {
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
				if (charainfo[i].playtime >= charainfo[i].anim_totaltime * 0.2f &&
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
			charainfo[i].playtime += 0.2f;

			// ★STAND（待機・移動中）のときはアニメーションをループさせる
			if (charainfo[i].mode == STAND) {
				if (charainfo[i].playtime >= charainfo[i].anim_totaltime) {
					charainfo[i].playtime = 0.0f;
				}
			}

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
		for (int i = 0; i < HitDim.HitNum; i++)
		{
			// 法線のY成分が小さい → 壁
			if (fabs(HitDim.Dim[i].Normal.y) < 0.5f)
			{
				printf("壁扱い\n");

				if (HitDim.Dim[i].Position[0].y > charainfo[0].pos.y + 1.0f ||
					HitDim.Dim[i].Position[1].y > charainfo[0].pos.y + 1.0f ||
					HitDim.Dim[i].Position[2].y > charainfo[0].pos.y + 1.0f)
				{
					if (WallNum < CHARA_MAX_HITCOLL)
					{
						Wall[WallNum] = &HitDim.Dim[i];
						WallNum++;
					}
				}
			}
			else
			{
				// 床
				if (FloorNum < CHARA_MAX_HITCOLL)
				{
					Floor[FloorNum] = &HitDim.Dim[i];
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
		for (int i = 0; i < PLAYER_COUNT; i++) {
			// (ここに移動や床判定の処理...)

			// ==========================================
			// 場外落下（デス）判定
			// ==========================================
			float DEATH_LINE = -300.0f; // このY座標を下回ったら落下死（ステージに合わせて調整）
	
			if (charainfo[i].pos.y < DEATH_LINE) {
				// 1. 死亡回数（DEATHS）を増やす
				charainfo[i].deaths++;

				// 2. 復活位置（スポーン地点）へワープさせる
				if (i == 0) {
					charainfo[i].pos = VGet(0.0f, 800.0f, 0.0f); // 1Pの復活位置
				}
				else {
					charainfo[i].pos = VGet(100.0f, 800.0f, 0.0f); // 2Pの復活位置
				}

				// 3. 移動量や速度をリセット（落下した勢いを消す）
				charainfo[i].move = VGet(0.0f, 0.0f, 0.0f);

				// 4. HPを回復させる（ヘッダーに合わせて HP を大文字に）
				charainfo[i].HP = 100; // 最大HPの変数がないため、全快する数値を直接指定

				// 5. モードを通常状態（STAND）に戻す
				charainfo[i].mode = STAND;
				MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);

				// 待機モーション（anim_neutral を使用）に設定
				charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_neutral);
				charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
				charainfo[i].playtime = 0.0f;
			}
		}
		// ==========================================
			// 各プレイヤーの床ポリゴンとの当たり判定
			// ==========================================
		for (int i = 0; i < PLAYER_COUNT; i++) {
			if (charainfo[i].mode == DOWNMODE) continue;

			HitDim = MV1CollCheck_Sphere(stagedata, -1, charainfo[i].pos, CHARA_ENUM_DEFAULT_SIZE + VSize(charainfo[i].move));
			WallNum = 0;
			FloorNum = 0;
			for (int h = 0; h < HitDim.HitNum; h++)
			{
				if (fabs(HitDim.Dim[h].Normal.y) < 0.5f)
				{
					if (HitDim.Dim[h].Position[0].y > charainfo[i].pos.y + 1.0f ||
						HitDim.Dim[h].Position[1].y > charainfo[i].pos.y + 1.0f ||
						HitDim.Dim[h].Position[2].y > charainfo[i].pos.y + 1.0f)
					{
						if (WallNum < CHARA_MAX_HITCOLL)
						{
							Wall[WallNum] = &HitDim.Dim[h];
							WallNum++;
						}
					}
				}
				else
				{
					if (FloorNum < CHARA_MAX_HITCOLL)
					{
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
						if (i == 0) { // デバッグ用の描画は1Pのものを代表させる場合
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
			else
			{
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

			game.DrawUI(pIdx, 900, 600, charainfo, hpBarTex);
			DrawCrownOnLeader(pIdx, charainfo, crownGraphHandle, game);
			
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

			// SE再生
			PlaySoundMem(SEdamageHandle, DX_PLAYTYPE_BACK);

			if (target == &charainfo[0] || target == &charainfo[1]) {

				// ターゲットがプレイヤーの場合
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
				// ターゲットが敵の場合
				if (target->enemyHP > 0) {
					target->enemyHP--;

					// 敵のHPが0以下になったら消滅させる
					if (target->enemyHP <= 0) {
						target->mode = NONE; // 状態を無効にする
						MV1SetVisible(target->model1, FALSE); // 画面から消す

						// 攻撃者がプレイヤー1だったらスコア加算
						if (attacker == &charainfo[0]) {
							game.AddScore(0, 100);
						}
						else if (attacker == &charainfo[1]) {
							game.AddScore(1, 100);
						}
					}
					else {
						// まだHPが残っている場合はダメージモーション
						MV1DetachAnim(target->model1, target->attachidx);
						target->attachidx = MV1AttachAnim(target->model1, 0, anim_damage);
						target->anim_totaltime = MV1GetAttachAnimTotalTime(target->model1, target->attachidx);
						target->playtime = 0.0f;
						target->mode = DAMAGE;
					}
				}
			}
			printfDx("ヒット！残リHP:%d\n", (target == &charainfo[0] ? target->HP : target->enemyHP));
		}
	}
}

void DrawCrownOnLeader(int pIdx, SCharaInfo* charainfo, int crownGraphHandle, GameManager& game) {
	// 1位のプレイヤーを判定（例としてスコアを比較、あるいはP1/P2のスコア変数を使用）
	int leaderIdx = 0;
	// ※もしP2のスコアのほうが高ければ 1 にする判定をここに記述
	// if (p2Score > p1Score) { leaderIdx = 1; }

	// 1位のプレイヤーの3D座標を取得し、頭の上の高さに大きくオフセットする（例: +120.0fなどモデルの大きさに合わせる）
	VECTOR charaPos = charainfo[leaderIdx].pos;
	charaPos.y += 120.0f; // ★高さが足りない場合はここを大きく調整してください

	// 3D座標をウィンドウ全体の2D画面座標に変換
	VECTOR screenPos = ConvWorldPosToScreenPos(charaPos);

	// カメラの前にいる場合のみ
	if (screenPos.z > 0.0f && screenPos.z < 1.0f) {

		float drawX = screenPos.x;
		float drawY = screenPos.y;

		// 左右の画面分割（ビューポート）に合わせた補正
		// プレイヤー2（右画面：450〜900px）の場合、右画面用のローカル座標に調整する必要があるか確認
		if (pIdx == 1) {
			// もし右画面の描画領域にオフセットが必要な場合はここで調整
		}

		// 王冠を描画（サイズを少し大きくして見やすくする 例: 1.0f など）
		DrawRotaGraph((int)drawX, (int)drawY, 0.8f, 0.0f, crownGraphHandle, TRUE);
	}
}