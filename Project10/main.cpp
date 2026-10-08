// DXライブラリーのインクルード
#include <DxLib.h>
#include <EffekseerForDXLib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "game.h"
#include "player.h"
#include "PlayerMotionAssets.h"
// The build writes the source directory beside the executable, so launching
// from AppData, Explorer or Visual Studio uses the same resource paths.
static bool SetGameResourceDirectory() {
    wchar_t executable[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return false;
    wchar_t* slash = wcsrchr(executable, L'\\');
    if (!slash) return false;
    wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - executable), L"Project10.assets.txt");
    HANDLE file = CreateFileW(executable, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        char utf8[4096] = {};
        DWORD bytes = 0;
        const bool read = ReadFile(file, utf8, sizeof(utf8) - 1, &bytes, nullptr) != FALSE;
        CloseHandle(file);
        if (!read || bytes == 0) return false;
        char* path = utf8;
        if (bytes >= 3 && static_cast<unsigned char>(path[0]) == 0xef &&
            static_cast<unsigned char>(path[1]) == 0xbb && static_cast<unsigned char>(path[2]) == 0xbf) path += 3;
        path[strcspn(path, "\r\n")] = 0;
        wchar_t directory[4096];
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, directory, 4096) ||
            !SetCurrentDirectoryW(directory)) return false;
    }
    return GetFileAttributesW(L"..\\Data\\Goblin\\Goblin.mv1") != INVALID_FILE_ATTRIBUTES;
}

static int MenuFont(int size) {
    static int fontSmall = -1, label = -1, heading = -1, title = -1;
    int* handle = size <= 17 ? &fontSmall : size <= 25 ? &label : size <= 40 ? &heading : &title;
    if (*handle < 0) *handle = CreateFontToHandle("Yu Gothic", size <= 17 ? 16 : size <= 25 ? 24 : size <= 40 ? 36 : 54, 2, DX_FONTTYPE_ANTIALIASING_8X8);
    return *handle;
}
static void MenuLabel(int x, int y, int size, const char* text, unsigned int color) {
    DrawStringToHandle(x, y, text, color, MenuFont(size));
}
static void MenuText(int y, int size, const char* text, unsigned int color) {
    const int font = MenuFont(size);
    DrawStringToHandle((900 - GetDrawStringWidthToHandle(text, (int)strlen(text), font)) / 2, y, text, color, font);
}
static void DrawGameMenu(bool results, const GameManager& game, int selected, const TCHAR* snapshot = nullptr) {
    SetDrawArea(0, 0, 900, 600); SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0); ClearDrawScreen();
    const auto gold = GetColor(222, 194, 137), white = GetColor(235, 236, 229), muted = GetColor(127, 145, 153);
    // A quiet, layered background with a softly lit sword emblem.
    for (int y = 0; y < 600; ++y) {
        const float t = y / 600.0f;
        DrawLine(0, y, 900, y, GetColor(8 + (int)(6*t), 17 + (int)(4*t), 25 + (int)(5*t)));
    }
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 22);
    for (int r = 295; r > 20; r -= 14) DrawCircle(675, 240, r, GetColor(41, 73, 78), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 65);
    DrawCircle(675, 240, 145, gold, FALSE);
    DrawCircle(675, 240, 157, GetColor(72, 109, 116), FALSE);
    DrawLine(675, 62, 675, 418, GetColor(68, 105, 111));
    DrawLine(500, 240, 850, 240, GetColor(68, 105, 111));
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    // Faceted blade, guard and grip, drawn from native geometry.
    DrawTriangle(675, 92, 653, 271, 675, 292, GetColor(165, 184, 183), TRUE);
    DrawTriangle(675, 92, 675, 292, 697, 271, GetColor(75, 104, 112), TRUE);
    DrawLine(675, 95, 675, 289, GetColor(220, 231, 223), 2);
    DrawLine(630, 293, 675, 283, gold, 5); DrawLine(675, 283, 720, 293, gold, 5);
    DrawBox(669, 292, 681, 345, GetColor(46, 59, 65), TRUE);
    for (int y = 302; y < 342; y += 10) DrawLine(670, y, 680, y, gold);
    DrawCircle(675, 351, 8, gold, TRUE);
    const float drift = GetNowCount() * 0.00015f;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 65);
    for (int i = 0; i < 22; ++i) {
        const int x = 440 + (i * 83 % 440);
        const int y = 60 + (int)fmodf(i * 47.0f + drift * 15, 450.0f);
        DrawCircle(x, y, i % 4 == 0 ? 2 : 1, gold, TRUE);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    if (!results) {
        MenuLabel(108, 167, 54, "アリーナ", white);
        MenuLabel(108, 230, 54, "バトル", white);
        DrawLine(112, 320, 165, 320, gold, 2);
    } else {
        // Opaque score panel keeps the result information clear over the emblem.
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 225);
        DrawBox(80, 65, 820, 416, GetColor(10, 21, 29), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        MenuText(85, 36, "試合結果", gold);
        MenuText(139, 36, game.gameState == 1 ? "プレイヤー１の勝利" : game.gameState == 2 ? "プレイヤー２の勝利" : "引き分け", white);
        DrawLine(120, 205, 780, 205, GetColor(53, 71, 78));
        MenuLabel(180, 229, 24, "プレイヤー１", GetColor(131, 187, 207));
        MenuLabel(525, 229, 24, "プレイヤー２", GetColor(219, 165, 140));
        char text[80];
        for (int p = 0; p < 2; ++p) {
            const int x = p == 0 ? 180 : 525;
            const int earned = p == 0 ? game.p1Score : game.p2Score;
            const int score = game.FinalScore(p);
            const int deathCost = game.deathCount[p] * GameManager::DeathPenalty;
            const int specialCost = game.specialUseCount[p] * GameManager::SpecialPenalty;
            sprintf_s(text, "獲得スコア   %d", earned); MenuLabel(x, 271, 16, text, muted);
            sprintf_s(text, "死亡 %d回   -%d", game.deathCount[p], deathCost); MenuLabel(x, 301, 16, text, muted);
            sprintf_s(text, "必殺技 %d回   -%d", game.specialUseCount[p], specialCost); MenuLabel(x, 331, 16, text, muted);
            sprintf_s(text, "合計   %d", score); MenuLabel(x, 366, 24, text, white);
        }
    }
    const int left = results ? 290 : 110, width = 320;
    for (int row = 0; row < 2; ++row) {
        const int y = (results ? 429 : 387) + row * 65;
        if (row == selected) {
            for (int x = 0; x < width; ++x) {
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90 - x * 75 / width);
                DrawLine(left + x, y, left + x, y + 49, GetColor(81, 99, 107));
            }
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            DrawBox(left, y, left + 3, y + 49, gold, TRUE);
            DrawLine(left, y + 49, left + width, y + 49, GetColor(77, 90, 92));
        }
        MenuLabel(left + 25, y + 8, 24, row == 1 ? "終了" : results ? "メニューに戻る" : "ゲーム開始", row == selected ? gold : muted);
        if (row == selected) MenuLabel(left + width - 32, y + 8, 24, ">", gold);
    }
    DrawLine(110, 548, 790, 548, GetColor(34, 51, 60));
    MenuText(566, 16, "↑↓ 選択     A / Enter 決定", muted);
    if (snapshot) SaveDrawScreenToPNG(0, 0, 900, 600, snapshot);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0); SetFontSize(20); ScreenFlip();
}
static int playerDamageAnimation = -1;
struct GameRuntimeCleanup {
    bool dxReady = false;
    bool effectReady = false;
    ~GameRuntimeCleanup() {
        if (effectReady) Effkseer_End();
        if (dxReady) DxLib_End();
    }
};

// DxLib projects against its logical viewport, which can differ from the clipped
// Direct3D viewport used by Effekseer during split-screen rendering.
static void SyncEffekseerDrawAreaProjection() {
	Effekseer_Sync3DSetting();
	MATRIX logicalViewport, apiViewport;
	GetTransformToViewportMatrix(&logicalViewport);
	GetTransformToAPIViewportMatrix(&apiViewport);
	const MATRIX projection = MMult(GetCameraProjectionMatrix(),
		MMult(logicalViewport, MInverse(apiViewport)));
	Effekseer::Matrix44 effectProjection;
	for (int row = 0; row < 4; ++row)
		for (int column = 0; column < 4; ++column)
			effectProjection.Values[row][column] = projection.m[row][column];
	GetEffekseer3DRenderer()->SetProjectionMatrix(effectProjection);
}





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
		}

		return handle;
	}

} // namespace




int WINAPI WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lpC, int nC)
{

	GameRuntimeCleanup runtimeCleanup;
	if (!SetGameResourceDirectory()) {
		MessageBoxW(nullptr, L"Game resources could not be found. Rebuild the project or launch with TryNewPlayer.cmd.", L"Project10", MB_OK | MB_ICONERROR);
		return -1;
	}
	int smokeLongRangeMoves = 0;
	const bool mergeSmoke = strstr(lpC, "--merge-smoke") != nullptr;
	const bool deathSmoke = strstr(lpC, "--death-smoke") != nullptr;
	const bool uiSmoke = strstr(lpC, "--ui-smoke") != nullptr;
	const bool gameSmoke = uiSmoke || deathSmoke || mergeSmoke || strstr(lpC, "--game-smoke") != nullptr;
	bool mergeAimPassed = false, mergeThrowPassed = false, mergeStunPassed = false;
	int smokeFrames = 0;
	int running = 0;
	int rootflm;
	MATRIX wpmatrix[PLAYER_COUNT], sayamatrix[PLAYER_COUNT];
	int		anim_neutral, anim_run, anim_jumpin, anim_jumploop, anim_jumpout, anim_damage, anim_down, enemy_anim_attack, enemy_anim_walk, enemy_anim_neutral{};

	//int redGoblinBaseModel = -1;

	 // ★これがあるか確認！
//	int red_goblin_anim_neutral;      // ★これがあるか確認！
	red_goblin_anim_attack = -1;       // ★これがあるか確認！

	red_goblin_anim_walk = -1;
	//int redGoblinBaseModel;
	int anim_attack[PLAYER_ATTACK_ANIM_COUNT];
	extern int stagedata;
	int sky;
	float skyRot = 0;
	static SCharaInfo charainfo[MAX_CHARA];//キャラクターのメモリ管理を配列で行ってる
	int playerWeaponModel[PLAYER_COUNT], playerWeaponFrame[PLAYER_COUNT];
	int playerSayaModel[PLAYER_COUNT], playerSayaFrame[PLAYER_COUNT];
	int normalAttackEffectResourceHandle = -1;
	int heavyAttackEffectHandle[PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT];
	VECTOR wpPosStart[PLAYER_COUNT], wpPosEnd[PLAYER_COUNT];
	int animHeavyAttack = -1;
	int animSpecialAttack = -1;
	int prevJKey = 0;
	int isBGMPlaying = 1;
	bool isAiming[PLAYER_COUNT] = {};
	bool prevAiming[PLAYER_COUNT] = {};
	float aimAngle[PLAYER_COUNT] = {};
	int grenadeBaseModel = -1;
	PlayerRuntimeState playerStates[PLAYER_COUNT];

	PlayerInputConfig playerInputs[PLAYER_COUNT] = {
		// 1P: キーボード(WASD) + 1Pゲームパッド。
		{ DX_INPUT_PAD1, KEY_INPUT_W, KEY_INPUT_S, KEY_INPUT_A, KEY_INPUT_D, KEY_INPUT_SPACE, KEY_INPUT_E, KEY_INPUT_F, KEY_INPUT_Q, PAD_INPUT_1, PAD_INPUT_3, PAD_INPUT_4, PAD_INPUT_2 },
		// 2P: キーボード(矢印) + 2Pゲームパッド。
		{ DX_INPUT_PAD2, KEY_INPUT_UP, KEY_INPUT_DOWN, KEY_INPUT_LEFT, KEY_INPUT_RIGHT, KEY_INPUT_RETURN, KEY_INPUT_RSHIFT, KEY_INPUT_RCONTROL, -1, PAD_INPUT_1, PAD_INPUT_3, PAD_INPUT_4, PAD_INPUT_2 }
	};



	int enemyCount = 0;          // 現在の敵の数
//	int spawnTimer = 0;          // 出現までのカウント用
	const int SPAWN_INTERVAL = 300; // 出現間隔

	float attackInEndTime[PLAYER_ATTACK_ANIM_COUNT] = { ATTACK_FIRST_ENDTIME, ATTACK_SECOND_ENDTIME, ATTACK_THIERD_ENDTIME };


	GameManager game;
	game.Init();
	VECTOR stagepos = VGet(0.0f, 0.0f,1200.0f);



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
	char NormalAttackEffect_FilePath[] = "Sword2.efkefc";
	char HeavyAttackEffect_FilePath[] = "重攻撃.png";
	char SpecialAttackEffect_FilePath[] = "swordMega.efkefc";
	int SEattackHandle, SEjumpHandle, SEdamageHandle;
	int specialAttackEffectResourceHandle = -1;
	int hpBarTex = -1;


//キャラ情報

	// 0番を1P、1番を2P、2番をテスト用敵として使う。
	for (int i = 0; i < MAX_CHARA; i++) {
		charainfo[i].model1 = -1;
		charainfo[i].attachidx = -1;
		charainfo[i].mode = NONE;
		charainfo[i].direction = Direction::DOWN;
		charainfo[i].dir = DX_PI_F;
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

	// Effekseer を利用するため、DxLib を DirectX 11 で初期化する。
	SetUseDirect3DVersion(DX_DIRECT3D_11);

	// DXライブラリの初期化
	if (DxLib_Init() == -1) {

		return -1;
	}

	// 同時に表示できる最大パーティクル数を指定して Effekseer を初期化する。
	runtimeCleanup.dxReady = true;
	if (Effekseer_Init(8000) == -1) {
		runtimeCleanup.dxReady = false; DxLib_End();
		return -1;
	}
	runtimeCleanup.effectReady = true;
	SetChangeScreenModeGraphicsSystemResetFlag(FALSE);
	Effekseer_SetGraphicsDeviceLostCallbackFunctions();

	for (int i = 0; i < PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT; i++) {
		heavyAttackEffectHandle[i] = -1;
	}


	int baseGoblinModel = MV1LoadModel("..\\Data\\Goblin\\Goblin.mv1");

	enemy_anim_neutral = baseGoblinModel;
	if (baseGoblinModel == -1) {
	}

	for (int i = TEST_ENEMY_INDEX; i < TEST_ENEMY_RED; i++) {
		if (baseGoblinModel != -1) {
			// モデルを複製
			charainfo[i].model1 = MV1DuplicateModel(baseGoblinModel);
		}
		else {
			charainfo[i].model1 = -1;
		}

		if (charainfo[i].model1 == -1) {
			continue;
		}

		// 最初は非表示
		MV1SetVisible(charainfo[i].model1, FALSE);
		charainfo[i].mode = NONE;

		// ★追加：複製したゴブリンに最初からニュートラルアニメーションをアタッチしておく
		charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, charainfo[i].enemyIdleAnimation);
		if (charainfo[i].attachidx != -1) {
			charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
		}

		charainfo[i].playtime = 0.0f;
	}


	//モデル読み込み
	redGoblinBaseModel = MV1LoadModel("..\\Data\\RedGoblin\\RedGoblin.mv1");
	red_goblin_anim_neutral = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Neutral.mv1");
	red_goblin_anim_walk = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Walk.mv1");
	red_goblin_anim_attack = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Attack1.mv1");
	for (int e = TEST_ENEMY_RED; e < MAX_CHARA; ++e) { charainfo[e].enemyIdleAnimation = red_goblin_anim_neutral; charainfo[e].enemyWalkAnimation = red_goblin_anim_walk; }


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

	
	for (int i = 0; i < PLAYER_COUNT; i++) {
		charainfo[i].model1 = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? "new\\Player_MainModel.mv1" : "PC.mv1");
		if (charainfo[i].model1 == -1) {
			return -1;
		}
		MV1SetPosition(charainfo[i].model1, charainfo[i].pos);
		MV1SetScale(charainfo[i].model1, VGet(PLAYER_MODEL_SCALE, PLAYER_MODEL_SCALE, PLAYER_MODEL_SCALE));
		if (PLAYER_USE_NEW_MODEL) {
			playerStates[i].motionRootFrame = MV1SearchFrame(charainfo[i].model1, "mixamorig:Hips");
			if (playerStates[i].motionRootFrame < 0) return -1;
			MATRIX root = MV1GetFrameLocalMatrix(charainfo[i].model1, playerStates[i].motionRootFrame);
			playerStates[i].motionRootX = root.m[3][0];
			playerStates[i].motionRootZ = root.m[3][2];
		}

	}
	if (strstr(lpC, "--player-smoke") != nullptr) {
		FILE* report = nullptr;
		fopen_s(&report, "player-smoke.txt", "w");
		bool passed = report != nullptr;
		int hand = MV1SearchFrame(charainfo[0].model1, "mixamorig:RightHand");
		int foot = MV1SearchFrame(charainfo[0].model1, "mixamorig:LeftFoot");
		int hips = MV1SearchFrame(charainfo[0].model1, "mixamorig:Hips");
		for (int i = 0; i < PlayerMotions::count; ++i) {
			int motion = LoadPlayerAssetModel(PlayerMotions::files[i]);
			int attached = motion < 0 ? -1 : MV1AttachAnim(charainfo[0].model1, 0, motion);
			float duration = attached < 0 ? 0.0f : MV1GetAttachAnimTotalTime(charainfo[0].model1, attached);
			float movement = 0.0f, rootTravel = 0.0f;
			if (hand >= 0 && foot >= 0 && hips >= 0 && attached >= 0 && duration > 0) {
				MV1SetAttachAnimTime(charainfo[0].model1, attached, 0.0f);
				charainfo[0].attachidx = attached;
				ApplyPlayerMotionRoot(charainfo[0], playerStates[0]);
				VECTOR firstHand = MV1GetFramePosition(charainfo[0].model1, hand);
				VECTOR firstFoot = MV1GetFramePosition(charainfo[0].model1, foot);
				VECTOR firstHips = MV1GetFramePosition(charainfo[0].model1, hips);
				for (int sample = 1; sample <= 4; ++sample) {
					MV1SetAttachAnimTime(charainfo[0].model1, attached, duration * sample / 4.0f);
					ApplyPlayerMotionRoot(charainfo[0], playerStates[0]);
					float delta = VSize(VSub(MV1GetFramePosition(charainfo[0].model1, hand), firstHand)) +
						VSize(VSub(MV1GetFramePosition(charainfo[0].model1, foot), firstFoot));
					if (delta > movement) movement = delta;
					VECTOR displacement = VSub(MV1GetFramePosition(charainfo[0].model1, hips), firstHips);
					displacement.y = 0.0f;
					float travel = VSize(displacement);
					if (travel > rootTravel) rootTravel = travel;
				}
			}
			bool valid = attached >= 0 && duration > 0 && movement > 0.01f && rootTravel < 0.01f;
			passed = passed && valid;
			if (report) fprintf(report, "%s frames=%.1f boneMotion=%.3f rootTravel=%.3f %s\n",
				PlayerMotions::files[i], duration, movement, rootTravel, valid ? "PASS" : "FAIL");
			if (attached >= 0) MV1DetachAnim(charainfo[0].model1, attached);
			if (hips >= 0) MV1ResetFrameUserLocalMatrix(charainfo[0].model1, hips);
			if (motion >= 0) MV1DeleteModel(motion);
		}
		int runMotion = LoadPlayerAssetModel(PlayerMotions::files[PlayerMotions::run]);
		charainfo[0].attachidx = -1;
		charainfo[0].mode = STAND;
		bool movementSwitchPassed = runMotion >= 0;
		if (movementSwitchPassed) {
			UpdatePlayerMovement(charainfo[0], playerStates[0], 0.0f, MOVE_SPEED, runMotion);
			movementSwitchPassed = playerStates[0].movementAnimation == runMotion && charainfo[0].direction == Direction::UP;
			charainfo[0].playtime = 3.0f;
			UpdatePlayerMovement(charainfo[0], playerStates[0], 0.0f, MOVE_SPEED, runMotion);
			movementSwitchPassed = movementSwitchPassed && charainfo[0].playtime == 3.0f;
			UpdatePlayerMovement(charainfo[0], playerStates[0], 0.0f, -MOVE_SPEED, runMotion);
			movementSwitchPassed = movementSwitchPassed && playerStates[0].movementAnimation == runMotion &&
				charainfo[0].direction == Direction::DOWN && charainfo[0].move.z < 0.0f && charainfo[0].playtime == 3.0f;
			UpdatePlayerMovement(charainfo[0], playerStates[0], MOVE_SPEED, 0.0f, runMotion);
			movementSwitchPassed = movementSwitchPassed && playerStates[0].movementAnimation == runMotion &&
				charainfo[0].direction == Direction::RIGHT;
		}
		passed = passed && movementSwitchPassed;
		if (report) fprintf(report, "movement facing and uninterrupted playback: %s\n", movementSwitchPassed ? "PASS" : "FAIL");
		int idleMotion = LoadPlayerAssetModel(PlayerMotions::files[PlayerMotions::idle]);
		bool idlePassed = idleMotion >= 0;
		if (idlePassed) {
			charainfo[0].mode = STAND;
			SetCharacterAnimation(charainfo[0], idleMotion);
			const float idleDuration = charainfo[0].anim_totaltime;
			charainfo[0].playtime = idleDuration + 1.0f;
			UpdatePlayerAnimationProgress(charainfo[0], playerStates[0], idleMotion);
			idlePassed = idleDuration > 0 && charainfo[0].mode == STAND && charainfo[0].playtime == 0.0f;
			charainfo[0].mode = ATTACKOUT;
			SetCharacterAnimation(charainfo[0], runMotion);
			charainfo[0].playtime = charainfo[0].anim_totaltime + 1.0f;
			UpdatePlayerAnimationProgress(charainfo[0], playerStates[0], idleMotion);
			idlePassed = idlePassed && charainfo[0].mode == STAND && charainfo[0].anim_totaltime == idleDuration;
		}
		passed = passed && idlePassed;
		if (report) fprintf(report, "idle loop and return after attack: %s\n", idlePassed ? "PASS" : "FAIL");
		int hurtMotion = LoadPlayerAssetModel(PlayerMotions::files[PlayerMotions::damage]);
        int deathMotion = LoadPlayerAssetModel(PlayerMotions::files[PlayerMotions::death]);
        bool hurtDeathPassed = hurtMotion >= 0 && deathMotion >= 0;
        for (int p = 0; p < PLAYER_COUNT && hurtDeathPassed; ++p) {
            charainfo[p].mode = DAMAGE;
            SetCharacterAnimation(charainfo[p], hurtMotion);
            if (p == 0) {
                auto savePose = [&](int motion, float time, const char* path) {
                    SetCharacterAnimation(charainfo[0], motion, time);
                    MV1SetAttachAnimTime(charainfo[0].model1, charainfo[0].attachidx, time);
                    charainfo[0].pos = VGet(0, 0, 0);
                    MV1SetPosition(charainfo[0].model1, charainfo[0].pos);
                    ApplyPlayerMotionRoot(charainfo[0], playerStates[0]);
                    SetDrawScreen(DX_SCREEN_BACK); ClearDrawScreen();
                    SetCameraPositionAndTargetAndUpVec(VGet(180, 190, -400), VGet(0, 65, 0), VGet(0, 1, 0));
                    MV1DrawModel(charainfo[0].model1);
                    SaveDrawScreenToPNG(0, 0, 900, 600, path);
                };
                savePose(hurtMotion, MV1GetAnimTotalTime(hurtMotion, 0) * 0.5f, "..\\.merge-review\\damage-pose.png");
                savePose(deathMotion, MV1GetAnimTotalTime(deathMotion, 0), "..\\.merge-review\\death-pose.png");
                SetCharacterAnimation(charainfo[0], hurtMotion);
            }
            for (int frame = 0; frame < 400; ++frame) UpdatePlayerAnimationProgress(charainfo[p], playerStates[p], idleMotion);
            hurtDeathPassed = hurtDeathPassed && charainfo[p].mode == STAND;
            charainfo[p].mode = DOWNMODE;
            SetCharacterAnimation(charainfo[p], deathMotion);
            const float duration = charainfo[p].anim_totaltime;
            for (int frame = 0; frame < 500; ++frame) UpdatePlayerAnimationProgress(charainfo[p], playerStates[p], idleMotion);
            hurtDeathPassed = hurtDeathPassed && duration > 0 && charainfo[p].mode == DOWNMODE && charainfo[p].playtime == duration;
            charainfo[p].mode = STAND;
            SetCharacterAnimation(charainfo[p], idleMotion);
            UpdatePlayerMovement(charainfo[p], playerStates[p], MOVE_SPEED, 0, runMotion);
            hurtDeathPassed = hurtDeathPassed && charainfo[p].mode == RUN;
        }
        passed = passed && hurtDeathPassed;
        if (report) fprintf(report, "two players: hurt recovery, death last frame, resurrection control: %s\n", hurtDeathPassed ? "PASS" : "FAIL");
        if (hurtMotion >= 0) MV1DeleteModel(hurtMotion);
        if (deathMotion >= 0) MV1DeleteModel(deathMotion);
        GameManager scoreRules;
        scoreRules.Init();
        scoreRules.AddScore(0, 5000);
        scoreRules.AddDeath(0);
        scoreRules.RecordSpecialAttack(0);
        scoreRules.RecordSpecialAttack(0);
        scoreRules.AddDeath(1);
        scoreRules.RecordSpecialAttack(1);
        bool scorePassed = scoreRules.p1Score == 5000 && scoreRules.p2Score == 0 &&
            scoreRules.FinalScore(0) == 0 && scoreRules.FinalScore(1) == -4000 &&
            scoreRules.deathCount[0] == 1 && scoreRules.specialUseCount[0] == 2 && scoreRules.specialUseCount[1] == 1;
        scoreRules.mainTimer = 0;
        scoreRules.Update(charainfo, charainfo);
        scorePassed = scorePassed && scoreRules.gameState == 1 && scoreRules.p1Score == 5000;
        for (const auto& popup : scoreRules.popups) scorePassed = scorePassed && !popup.active;
        scoreRules.AddDeath(0);
        scoreRules.AddDeath(0);
        scoreRules.gameState = 0;
        scoreRules.Update(charainfo, charainfo);
        scorePassed = scorePassed && scoreRules.gameState == 2 && scoreRules.FinalScore(0) == -6000 && scoreRules.p1Score == 5000;
        scoreRules.Init();
        scorePassed = scorePassed && scoreRules.p1Score == 0 && scoreRules.p2Score == 0 &&
            scoreRules.deathCount[0] == 0 && scoreRules.specialUseCount[0] == 0 && scoreRules.specialUseCount[1] == 0;
        passed = passed && scorePassed;
        if (report) fprintf(report, "score rules: unchanged live scores, no penalty popup, settlement deductions, net winner and reset: %s\n", scorePassed ? "PASS" : "FAIL");
        int swordEffect = LoadEffekseerEffect("..\\Data\\Effect\\Sword2.efkefc", PLAYER_NORMAL_ATTACK_EFFECT_MAGNIFICATION);
		bool swordPassed = swordEffect >= 0;
		if (swordPassed) {
			for (int i = 0; i < PLAYER_COUNT; i++) playerStates[i].normalAttackEffectResourceHandle = swordEffect;
			for (int attack = 0; attack < PLAYER_ATTACK_ANIM_COUNT; attack++) {
				for (int i = 0; i < PLAYER_COUNT; i++) {
					playerStates[i].attackIndex = attack;
					PlayPlayerNormalAttackEffect(charainfo[i], playerStates[i]);
					swordPassed = swordPassed && playerStates[i].isNormalAttackEffectPlaying &&
						IsEffekseer3DEffectPlaying(playerStates[i].normalAttackPlayingHandle) == 0;
				}
				swordPassed = swordPassed && playerStates[0].normalAttackPlayingHandle != playerStates[1].normalAttackPlayingHandle;
				UpdateEffekseer3D();
			}
			for (int frame = 0; frame < 240; frame++) {
				UpdateEffekseer3D();
				for (int i = 0; i < PLAYER_COUNT; i++) UpdatePlayerAnimationProgress(charainfo[i], playerStates[i], idleMotion);
			}
			for (int i = 0; i < PLAYER_COUNT; i++) swordPassed = swordPassed &&
				!playerStates[i].isNormalAttackEffectPlaying && playerStates[i].normalAttackPlayingHandle == -1;
			DeleteEffekseerEffect(swordEffect);
		}
		passed = passed && swordPassed;
		if (report) fprintf(report, "Sword2 load, three combo stages, two players and completion: %s\n", swordPassed ? "PASS" : "FAIL");
		int megaEffect = LoadEffekseerEffect("..\\Data\\Effect\\swordMega.efkefc", PLAYER_SPECIAL_ATTACK_EFFECT_MAGNIFICATION);
		bool megaPassed = megaEffect >= 0;
		PlayerRuntimeState megaState;
		megaState.isSpecialAttack = true;
		megaState.isSpecialAttackEffectPlaying = true;
		megaState.specialAttackPlayingHandle = megaEffect < 0 ? -1 : PlayEffekseer3DEffect(megaEffect);
		megaPassed = megaPassed && megaState.specialAttackPlayingHandle >= 0;
		SCharaInfo& attacker = charainfo[0];
		attacker.pos = VGet(0, 0, 0);
		attacker.direction = Direction::DOWN;
		const float megaYaw = DX_PI_F * 0.5f * attacker.direction;
		for (int e = 2; e <= 3; ++e) {
			charainfo[e].mode = STAND;
			charainfo[e].enemyHP = 6;
			charainfo[e].charahitinfo.Width = 40;
			charainfo[e].charahitinfo.Height = 100;
			charainfo[e].pos = VGet(-sinf(megaYaw) * 150, 0, -cosf(megaYaw) * 150);
		}
		megaState.specialAttackEffectFrame = 39;
		CheckPlayerSpecialAttackHit(game, attacker, megaState, charainfo[2], -1, enemy_anim_neutral, 0, 2);
		megaPassed = megaPassed && charainfo[2].enemyHP == 6;
		megaState.specialAttackEffectFrame = 40;
		for (int e = 2; e <= 3; ++e)
			CheckPlayerSpecialAttackHit(game, attacker, megaState, charainfo[e], -1, enemy_anim_neutral, 0, e);
		megaPassed = megaPassed && charainfo[2].enemyHP == 4 && charainfo[3].enemyHP == 4;
		charainfo[2].mode = STAND;
		CheckPlayerSpecialAttackHit(game, attacker, megaState, charainfo[2], -1, enemy_anim_neutral, 0, 2);
		megaPassed = megaPassed && charainfo[2].enemyHP == 4;
		megaState.specialHitTargets[2] = false;
		charainfo[2].pos = VGet(sinf(megaYaw) * 150, 0, cosf(megaYaw) * 150);
		CheckPlayerSpecialAttackHit(game, attacker, megaState, charainfo[2], -1, enemy_anim_neutral, 0, 2);
		megaPassed = megaPassed && charainfo[2].enemyHP == 4;
		megaState.specialAttackEffectFrame = 100;
		charainfo[2].pos = VGet(-sinf(megaYaw) * 70 + cosf(megaYaw) * 180, 0,
			-cosf(megaYaw) * 70 - sinf(megaYaw) * 180);
		CheckPlayerSpecialAttackHit(game, attacker, megaState, charainfo[2], -1, enemy_anim_neutral, 0, 2);
		megaPassed = megaPassed && charainfo[2].enemyHP == 2;
		megaState.specialAttackEffectFrame = 0;
		for (int frame = 0; frame < PLAYER_SPECIAL_ATTACK_EFFECT_FRAME_COUNT; ++frame) {
			UpdateEffekseer3D();
			UpdatePlayerAnimationProgress(attacker, megaState, idleMotion);
		}
		megaPassed = megaPassed && !megaState.isSpecialAttackEffectPlaying && megaState.specialAttackPlayingHandle == -1 && !megaState.isSpecialAttack;
        attacker.mode = ATTACK;
        megaState.isSpecialAttack = true;
        megaState.isSpecialAttackEffectPlaying = false;
        UpdatePlayerAnimationProgress(attacker, megaState, idleMotion);
        bool recoveryPassed = attacker.mode == STAND && !megaState.isSpecialAttack && megaState.attackIndex == 0;
        UpdatePlayerMovement(attacker, megaState, MOVE_SPEED, 0, runMotion);
        recoveryPassed = recoveryPassed && attacker.mode == RUN && attacker.move.x == MOVE_SPEED;
        const float recoveryTime = attacker.playtime;
        UpdatePlayerAnimationProgress(attacker, megaState, idleMotion);
        recoveryPassed = recoveryPassed && fabsf(attacker.playtime - recoveryTime - (PLAYER_USE_NEW_MODEL ? 0.5f : 0.3f)) < 0.001f;
        passed = passed && recoveryPassed;
        if (report) fprintf(report, "special completion restores running speed and animation: %s\n", recoveryPassed ? "PASS" : "FAIL");
		if (megaEffect >= 0) DeleteEffekseerEffect(megaEffect);
		passed = passed && megaPassed;
		if (report) fprintf(report, "SwordMega load, startup, forward strike, area, multiple targets, single hit and cleanup: %s\n", megaPassed ? "PASS" : "FAIL");
		bool projectionPassed = true;
		float oldMaxError = 0.0f, correctedMaxError = 0.0f;
		for (int side = 0; side < 3; ++side) {
			SetDrawArea(side == 2 ? 0 : side * 450, 0, side == 2 ? 900 : (side + 1) * 450, 600);
			SetCameraPositionAndTargetAndUpVec(VGet(-200, 450, -900), VGet(0, 0, 0), VGet(0, 1, 0));
			SyncEffekseerDrawAreaProjection();
			MATRIX apiVp, corrected;
			GetTransformToAPIViewportMatrix(&apiVp);
			const Effekseer::Matrix44 effectProj = GetEffekseer3DRenderer()->GetProjectionMatrix();
			for (int row = 0; row < 4; ++row)
				for (int column = 0; column < 4; ++column)
					corrected.m[row][column] = effectProj.Values[row][column];
			for (int sample = 0; sample < 5; ++sample) {
				const VECTOR point = VGet((sample - 2) * 75.0f, sample * 20.0f, sample * 30.0f);
				const VECTOR expected = ConvWorldPosToScreenPos(point);
				const VECTOR cameraPoint = VTransform(point, GetCameraViewMatrix());
				for (int fixed = 0; fixed < 2; ++fixed) {
					const MATRIX proj = fixed ? corrected : GetCameraProjectionMatrix();
					const float w = cameraPoint.x * proj.m[0][3] + cameraPoint.y * proj.m[1][3] + cameraPoint.z * proj.m[2][3] + proj.m[3][3];
					const VECTOR ndc = VScale(VTransform(cameraPoint, proj), 1.0f / w);
					const VECTOR pixel = VTransform(ndc, apiVp);
					const float error = fabsf(pixel.x - expected.x) + fabsf(pixel.y - expected.y);
					if (fixed) {
						if (error > correctedMaxError) correctedMaxError = error;
						projectionPassed = projectionPassed && error < 0.01f;
					} else if (error > oldMaxError) oldMaxError = error;
				}
			}
		}
		passed = passed && projectionPassed && oldMaxError > 100.0f;
		if (report) fprintf(report, "left/right/full viewport projection: oldError=%.3fpx correctedError=%.6fpx %s\n", oldMaxError, correctedMaxError, projectionPassed ? "PASS" : "FAIL");
		SCharaInfo pullPlayers[PLAYER_COUNT];
		PlayerRuntimeState pullStates[PLAYER_COUNT];
		for (int i = 0; i < PLAYER_COUNT; ++i) {
			pullPlayers[i].mode = STAND;
			pullPlayers[i].pos = VGet(i * 400.0f, 0, 0);
			pullPlayers[i].charahitinfo.Width = 40;
			pullPlayers[i].charahitinfo.Height = 100;
		}
		SCharaInfo pullTarget;
		pullTarget.mode = STAND;
		pullTarget.enemyHP = 6;
		pullTarget.charahitinfo.Width = 40;
		pullTarget.charahitinfo.Height = 100;
		pullTarget.pos = VGet(240, 0, 0);
		UpdateWhirlwindPull(pullPlayers, pullStates, pullTarget);
		bool pullPassed = pullTarget.pos.x == 240;
		pullStates[0].isHeavyAttackEffectPlaying = true;
		UpdateWhirlwindPull(pullPlayers, pullStates, pullTarget);
		pullPassed = pullPassed && pullTarget.pos.x == 228 && pullTarget.pos.y == 0 && pullTarget.enemyHP == 6;
		for (int frame = 0; frame < 60; ++frame) UpdateWhirlwindPull(pullPlayers, pullStates, pullTarget);
		pullPassed = pullPassed && fabsf(pullTarget.pos.x - PLAYER_WHIRLWIND_PULL_STOP_DISTANCE) < 0.001f;
		pullTarget.pos = VGet(301, 0, 0);
		UpdateWhirlwindPull(pullPlayers, pullStates, pullTarget);
		pullPassed = pullPassed && pullTarget.pos.x == 301;
		pullTarget.pos = VGet(200, 300, 0);
		UpdateWhirlwindPull(pullPlayers, pullStates, pullTarget);
		pullPassed = pullPassed && pullTarget.pos.x == 200;
		pullTarget.pos = VGet(200, 0, 0);
		pullTarget.mode = NONE;
		UpdateWhirlwindPull(pullPlayers, pullStates, pullTarget);
		pullPassed = pullPassed && pullTarget.pos.x == 200;
		pullTarget.mode = DAMAGE;
		UpdateWhirlwindPull(pullPlayers, pullStates, pullTarget);
		pullPassed = pullPassed && pullTarget.pos.x == 188;
		pullStates[0].heavyAttackEffectFrame = PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT;
		UpdateWhirlwindPull(pullPlayers, pullStates, pullTarget);
		pullPassed = pullPassed && pullTarget.pos.x == 188;
		pullStates[0].heavyAttackEffectFrame = 10;
		pullStates[1].isHeavyAttackEffectPlaying = true;
		pullTarget.pos = VGet(240, 0, 0);
		UpdateWhirlwindPull(pullPlayers, pullStates, pullTarget);
		pullPassed = pullPassed && pullTarget.pos.x == 252;
		passed = passed && pullPassed;
		if (report) fprintf(report, "whirlwind pull: inactive, range, convergence, height, dead target, damaged target, expiry and nearest player: %s\n", pullPassed ? "PASS" : "FAIL");
		bool enemyWalkPassed = true;
		const char* enemyFolders[] = {"Goblin", "RedGoblin", "Golem"};
		for (const char* folder : enemyFolders) {
			char path[256];
			sprintf_s(path, "..\\Data\\%s\\Anim_Walk.mv1", folder);
			int walk = MV1LoadModel(path);
			sprintf_s(path, "..\\Data\\%s\\Anim_Neutral.mv1", folder);
			int idle = MV1LoadModel(path);
			SCharaInfo enemy;
			enemy.model1 = idle < 0 ? -1 : MV1DuplicateModel(idle);
			enemy.attachidx = -1;
			enemy.mode = STAND;
			enemy.enemyIdleAnimation = idle;
			enemy.enemyWalkAnimation = walk;
			bool valid = walk >= 0 && idle >= 0 && enemy.model1 >= 0;
			if (valid) {
				UpdateEnemyLocomotionAnimation(enemy, false);
				UpdateEnemyLocomotionAnimation(enemy, true);
				valid = enemy.enemyWalking && enemy.attachidx >= 0 && enemy.anim_totaltime > 0;
				enemy.playtime = 5.0f;
				const int attach = enemy.attachidx;
				UpdateEnemyLocomotionAnimation(enemy, true);
				valid = valid && enemy.playtime == 5.0f && enemy.attachidx == attach;
				enemy.mode = DAMAGE;
				UpdateEnemyLocomotionAnimation(enemy, true);
				valid = valid && !enemy.enemyWalking && enemy.attachidx == attach;
				enemy.mode = STAND;
				UpdateEnemyLocomotionAnimation(enemy, true);
				valid = valid && enemy.enemyWalking && enemy.playtime == 0;
				UpdateEnemyLocomotionAnimation(enemy, false);
				valid = valid && !enemy.enemyWalking && enemy.playtime == 0;
			}
			enemyWalkPassed = enemyWalkPassed && valid;
			if (report) fprintf(report, "%s walk/idle switching, continuous playback and damage recovery: %s\n", folder, valid ? "PASS" : "FAIL");
			if (enemy.model1 >= 0) MV1DeleteModel(enemy.model1);
			if (walk >= 0) MV1DeleteModel(walk);
			if (idle >= 0) MV1DeleteModel(idle);
		}
		passed = passed && enemyWalkPassed;
		if (report) fclose(report);
		runtimeCleanup.effectReady = false; Effkseer_End();
		runtimeCleanup.dxReady = false; DxLib_End();
		return passed ? 0 : 1;
	}

	// 2P は仮で少し青くして、同じモデルでも見分けやすくする。
	MV1SetMaterialDrawAddColorAll(charainfo[PLAYER2_INDEX].model1, 0, 0, 60);

	charainfo[TEST_ENEMY_GOLEM].model1 = MV1LoadModel("..\\Data\\Golem\\Golem.mv1");
	if (charainfo[TEST_ENEMY_GOLEM].model1 == -1) {
	}
	else {
		// 最初は非表示にしておく
		MV1SetVisible(charainfo[TEST_ENEMY_GOLEM].model1, FALSE);
		charainfo[TEST_ENEMY_GOLEM].mode = NONE;
		MV1SetScale(charainfo[TEST_ENEMY_GOLEM].model1, VGet(1.0f, 1.0f, 1.0f)); // 必要ならサイズ調整
	}




	//ルートフレーム
	for (int i = 0; !PLAYER_USE_NEW_MODEL && i < PLAYER_COUNT; i++) {
		rootflm = MV1SearchFrame(charainfo[i].model1, "root");
		if (rootflm != -1) {
			MV1SetFrameUserLocalMatrix(charainfo[i].model1, rootflm, MGetIdent());
		}
		else {
		}
	}
	rootflm = MV1SearchFrame(charainfo[TEST_ENEMY_INDEX].model1, "root");
	if (rootflm != -1) {
		MV1SetFrameUserLocalMatrix(charainfo[TEST_ENEMY_INDEX].model1, rootflm, MGetIdent());
	}


	for (int i = 0; i < PLAYER_COUNT; i++) {
		//武器モデル
		playerWeaponModel[i] = PLAYER_USE_NEW_MODEL ? -1 : LoadPlayerAssetModel("Sabel.mv1");
		if (!PLAYER_USE_NEW_MODEL && playerWeaponModel[i] == -1) return -1;
		//武器フレーム
		playerWeaponFrame[i] = MV1SearchFrame(charainfo[i].model1, PLAYER_USE_NEW_MODEL ? "mixamorig:RightHand" : "wp");
		if (playerWeaponFrame[i] == -1) {
		}
		//鞘モデル
		playerSayaModel[i] = PLAYER_USE_NEW_MODEL ? -1 : LoadPlayerAssetModel("Saya.mv1");
		if (!PLAYER_USE_NEW_MODEL && playerSayaModel[i] == -1) return -1;
		//鞘フレーム
		playerSayaFrame[i] = MV1SearchFrame(charainfo[i].model1, "sayabone");
		if (!PLAYER_USE_NEW_MODEL && playerSayaFrame[i] == -1) {
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
	sky = MV1LoadModel("..\\Data\\Stage\\Sage_1214.mv1");
	if (sky == -1) return -1;
	MV1SetPosition(sky, VGet(0, -1000, 0));

	// モデル全体のコリジョン情報のセットアップ
	MV1SetupCollInfo(stagedata, -1);

	int MeshNum;

	// モデルに含まれるメッシュの数を取得する
	MeshNum = MV1GetMeshNum(stagedata);
	SetTransColor(0, 0, 0);
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

	anim_neutral = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? PlayerMotions::files[PlayerMotions::idle] : "Anim_Neutral.mv1");
	if (anim_neutral == -1) return -1;
	anim_run = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? PlayerMotions::files[PlayerMotions::run] : "Anim_Run.mv1");
	if (anim_run == -1) return -1;
	anim_jumpin = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? "new\\Player_MainModel.mv1" : "Anim_Jump_In.mv1");
	if (anim_jumpin == -1) return -1;
	anim_jumploop = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? "new\\Player_MainModel.mv1" : "Anim_Jump_Loop.mv1");
	if (anim_jumploop == -1) return -1;
	anim_jumpout = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? "new\\Player_MainModel.mv1" : "Anim_Jump_Out.mv1");
	if (anim_jumpout == -1) return -1;
	anim_attack[0] = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? PlayerMotions::files[PlayerMotions::normalAttack[0]] : "Anim_Attack1.mv1");
	if (anim_attack[0] == -1) return -1;
	anim_attack[1] = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? PlayerMotions::files[PlayerMotions::normalAttack[1]] : "Anim_Attack2.mv1");
	if (anim_attack[1] == -1) return -1;
	anim_attack[2] = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? PlayerMotions::files[PlayerMotions::normalAttack[2]] : "Anim_Attack3.mv1");
	if (anim_attack[2] == -1) return -1;
	animHeavyAttack = PLAYER_USE_NEW_MODEL ? LoadPlayerAssetModel(PlayerMotions::files[PlayerMotions::heavyAttack]) : anim_attack[2];
	animSpecialAttack = PLAYER_USE_NEW_MODEL ? LoadPlayerAssetModel(PlayerMotions::files[PlayerMotions::specialAttack]) : anim_attack[2];
	if (animHeavyAttack == -1 || animSpecialAttack == -1) return -1;
	if (PLAYER_USE_NEW_MODEL) {
		for (int i = 0; i < PLAYER_ATTACK_ANIM_COUNT; ++i) {
			attackInEndTime[i] = MV1GetAnimTotalTime(anim_attack[i], 0);
		}
	}
	anim_damage = LoadPlayerAssetModel("Anim_Damage.mv1");
	if (anim_damage == -1) return -1;
	playerDamageAnimation = PLAYER_USE_NEW_MODEL ? LoadPlayerAssetModel(PlayerMotions::files[PlayerMotions::damage]) : anim_damage;
	if (playerDamageAnimation < 0) return -1;
	anim_down = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? PlayerMotions::files[PlayerMotions::death] : "Anim_Down_Loop.mv1");
	if (anim_down == -1) return -1;
	enemy_anim_attack = MV1LoadModel("..\\Data\\Goblin\\Anim_Attack1.mv1");		// 被撃アニメ
	if (enemy_anim_attack == -1) return -1;
	enemy_anim_walk = MV1LoadModel("..\\Data\\Goblin\\Anim_Walk.mv1");		// 被撃アニメ
	if (enemy_anim_walk == -1) return -1;

	enemy_anim_neutral = MV1LoadModel("..\\Data\\Goblin\\Anim_Neutral.mv1");		// 被撃アニメ
	if (enemy_anim_neutral == -1) return -1;
	game.enemy_anim_neutral = enemy_anim_neutral;
	game.enemy_anim_walk = enemy_anim_walk;
	for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
		charainfo[e].enemyIdleAnimation = enemy_anim_neutral;
		charainfo[e].enemyWalkAnimation = enemy_anim_walk;
	}
	charainfo[TEST_ENEMY_GOLEM].enemyIdleAnimation = MV1LoadModel("..\\Data\\Golem\\Anim_Neutral.mv1");
	charainfo[TEST_ENEMY_GOLEM].enemyWalkAnimation = MV1LoadModel("..\\Data\\Golem\\Anim_Walk.mv1");
	if (charainfo[TEST_ENEMY_GOLEM].enemyIdleAnimation < 0 || charainfo[TEST_ENEMY_GOLEM].enemyWalkAnimation < 0) return -1;

	SetTransColor(255, 255, 255);
	hpBarTex = LoadGraph("..\\Data\\UI\\Hpbar023.png"); // ※実際のファイルパスに合わせて調整してください

	if (hpBarTex == -1) {
		// 読み込みに失敗した場合のエラー処理（必要に応じて）
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

	// 通常攻撃エフェクトのスプライトシートを分割して読み込む。
	sprintf_s(String, sizeof(String), "..\\Data\\Effect\\%s", NormalAttackEffect_FilePath);
	normalAttackEffectResourceHandle = LoadEffekseerEffect(String, PLAYER_NORMAL_ATTACK_EFFECT_MAGNIFICATION);
	if (normalAttackEffectResourceHandle == -1) {
		return -1;
	}
	for (int i = 0; i < PLAYER_COUNT; i++) {
		playerStates[i].normalAttackEffectResourceHandle = normalAttackEffectResourceHandle;
	}

	// 重攻撃エフェクトのスプライトシートを分割して読み込む。
	sprintf_s(String, sizeof(String), "..\\Data\\Effect\\%s", HeavyAttackEffect_FilePath);
	if (LoadDivGraph(String, PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT, PLAYER_HEAVY_ATTACK_EFFECT_COLUMN_COUNT, PLAYER_HEAVY_ATTACK_EFFECT_ROW_COUNT, PLAYER_HEAVY_ATTACK_EFFECT_FRAME_WIDTH, PLAYER_HEAVY_ATTACK_EFFECT_FRAME_HEIGHT, heavyAttackEffectHandle) == -1) {
		return -1;
	}

	// Load the exported SwordMega effect.
	sprintf_s(String, sizeof(String), "..\\Data\\Effect\\%s", SpecialAttackEffect_FilePath);
	specialAttackEffectResourceHandle = LoadEffekseerEffect(String, PLAYER_SPECIAL_ATTACK_EFFECT_MAGNIFICATION);
	if (specialAttackEffectResourceHandle == -1) {
		return -1;
	}
    enum class Screen { Menu, Playing, Results };
    Screen screen = gameSmoke && !uiSmoke ? Screen::Playing : Screen::Menu;
    int menuSelection = 0;
    bool prevMenuConfirm = false, prevMenuDirection = false;
    bool uiStarted = false, uiResults = false, uiReturned = false, uiRestarted = false;
    auto resetMatch = [&]() {
        game.Init();
        for (int p = 0; p < PLAYER_COUNT; ++p) {
            auto old = playerStates[p];
            if (old.normalAttackPlayingHandle >= 0) StopEffekseer3DEffect(old.normalAttackPlayingHandle);
            if (old.specialAttackPlayingHandle >= 0) StopEffekseer3DEffect(old.specialAttackPlayingHandle);
            playerStates[p] = PlayerRuntimeState{};
            playerStates[p].normalAttackEffectResourceHandle = old.normalAttackEffectResourceHandle;
            playerStates[p].motionRootFrame = old.motionRootFrame;
            playerStates[p].motionRootX = old.motionRootX;
            playerStates[p].motionRootZ = old.motionRootZ;
            charainfo[p].pos = VGet(1300, 800, p == 0 ? 100.0f : -400.0f);
            charainfo[p].HP = MAX_HP; charainfo[p].deaths = 0; charainfo[p].mode = STAND; charainfo[p].isHit = false;
            ResetMove(charainfo[p]); SetCharacterAnimation(charainfo[p], anim_neutral);
            MV1SetPosition(charainfo[p].model1, charainfo[p].pos);
            isAiming[p] = false; prevAiming[p] = false; playerWhiteoutTimer[p] = 0; flashStock[p] = 1;
        }
        for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
            charainfo[e].mode = NONE; charainfo[e].invincibleTimer = 0;
            if (charainfo[e].model1 >= 0) MV1SetVisible(charainfo[e].model1, FALSE);
        }
        for (int f = 0; f < MAX_THROWN_FLASH; ++f) thrownFlashes[f].active = false;
        dropFlashItem.active = false; flashSpawnTimer = 0;
    };
	while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0 && (!gameSmoke || smokeFrames++ < (uiSmoke ? 180 : (deathSmoke ? 1800 : (mergeSmoke ? 520 : 360))))) {
        if (uiSmoke && screen == Screen::Playing && smokeFrames == 60) game.mainTimer = 1;
        if (screen == Screen::Playing && game.gameState != 0) {
            screen = Screen::Results; menuSelection = 0; prevMenuConfirm = true; uiResults = true;
        }
        if (screen != Screen::Playing) {
            int pad = ReadPlayerPadState(DX_INPUT_PAD1) | ReadPlayerPadState(DX_INPUT_PAD2);
            bool confirm = CheckHitKey(KEY_INPUT_RETURN) || (pad & PAD_INPUT_1);
            bool direction = CheckHitKey(KEY_INPUT_UP) || CheckHitKey(KEY_INPUT_DOWN) || (pad & (PAD_INPUT_UP | PAD_INPUT_DOWN));
            if (direction && !prevMenuDirection) menuSelection = 1 - menuSelection;
            if (uiSmoke) confirm = smokeFrames == 10 || smokeFrames == 90 || smokeFrames == 120;
            if (confirm && !prevMenuConfirm) {
                if (menuSelection == 1) break;
                if (screen == Screen::Results) { screen = Screen::Menu; uiReturned = true; }
                else { resetMatch(); screen = Screen::Playing; if (uiStarted) uiRestarted = true; uiStarted = true; }
            }
            prevMenuConfirm = confirm; prevMenuDirection = direction;
            if (screen != Screen::Playing) { DrawGameMenu(screen == Screen::Results, game, menuSelection, uiSmoke && smokeFrames == 1 ? TEXT("..\\.merge-review\\start-menu.png") : uiSmoke && smokeFrames == 65 ? TEXT("..\\.merge-review\\results.png") : nullptr); WaitTimer(16); }
            continue;
        }



		if (deathSmoke && smokeFrames == 100) { charainfo[0].HP = 0; charainfo[1].HP = 0; }
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
				CancelPlayerCombat(playerStates[i]);
				ResetMove(charainfo[i]);
				SetCharacterAnimation(charainfo[i], anim_down);
				isAiming[i] = false;

				// ★ここでカウントする

			game.AddDeath(i);
			}
		}
		for (int i = 0; i < PLAYER_COUNT; i++) {
			// 死亡状態(DOWNMODE)で、ジャンプボタン（1P:SPACE, 2P:RETURN）が押されたら
			if (charainfo[i].mode == DOWNMODE && (CheckHitKey(playerInputs[i].jumpKey) == 1 || (ReadPlayerPadState(playerInputs[i].padType) & playerInputs[i].jumpPadButton))) {

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

		for (int p = 0; p < PLAYER_COUNT; ++p) playerStates[p].key = ReadPlayerPadState(playerInputs[p].padType);
		for (int p = 0; p < PLAYER_COUNT; p++) {
			bool currentAimInput = false;
			if (p == 0) {
				currentAimInput = (CheckHitKey(KEY_INPUT_G) == 1) || (playerStates[p].key & PAD_INPUT_5);
			}
			else {
				currentAimInput = (CheckHitKey(KEY_INPUT_RALT) == 1) || (playerStates[p].key & PAD_INPUT_5);
			}

			if (mergeSmoke && p == 0) currentAimInput = smokeFrames >= 20 && smokeFrames < 30;
		if (flashStock[p] > 0 && charainfo[p].mode != DAMAGE && charainfo[p].mode != DOWNMODE) {
				if (currentAimInput && !prevAiming[p]) {
					isAiming[p] = true;
					aimAngle[p] = DX_PI_F + DX_PI_F * 0.5f * charainfo[p].direction;
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

			if (mergeSmoke && thrownFlashes[f].ownerIndex == 0 && thrownFlashes[f].timer == 1) {
				charainfo[TEST_ENEMY_INDEX].pos = thrownFlashes[f].pos;
				charainfo[TEST_ENEMY_INDEX].mode = STAND;
				charainfo[TEST_ENEMY_INDEX].enemyHP = 6;
			}
			thrownFlashes[f].timer--;
			if (thrownFlashes[f].timer <= 0) {
				thrownFlashes[f].active = false;
				MV1SetVisible(thrownFlashes[f].model, FALSE);

				const float FLASH_RANGE_SQ = 450.0f * 450.0f;

				// ① 敵AIのスタン
				for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
					if (charainfo[i].mode == NONE || charainfo[i].model1 < 0) continue;
					float edx = charainfo[i].pos.x - thrownFlashes[f].pos.x;
					float edz = charainfo[i].pos.z - thrownFlashes[f].pos.z;
					if (edx * edx + edz * edz < FLASH_RANGE_SQ) {
						charainfo[i].invincibleTimer = 120;
					SetCharacterAnimation(charainfo[i], charainfo[i].enemyIdleAnimation);
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
						float fwdX = -sinf(DX_PI_F * 0.5f * charainfo[p].direction);
						float fwdZ = -cosf(DX_PI_F * 0.5f * charainfo[p].direction);
						float dot = fwdX * toX + fwdZ * toZ;

						// dot > 0 で直視判定
						if (dot > 0.0f) {
							charainfo[p].mode = DAMAGE;
							charainfo[p].playtime = 0.0f;
							MV1DetachAnim(charainfo[p].model1, charainfo[p].attachidx);
							charainfo[p].attachidx = MV1AttachAnim(charainfo[p].model1, 0, playerDamageAnimation);
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
			if (charainfo[i].mode == NONE || charainfo[i].model1 < 0) continue;

			bool isRed = (i >= TEST_ENEMY_RED);
		if (mergeSmoke && charainfo[TEST_ENEMY_INDEX].invincibleTimer > 0) mergeStunPassed = true;
			int animNeutral = charainfo[i].enemyIdleAnimation;
			int animAttack = isRed ? red_goblin_anim_attack : enemy_anim_attack;

			if (charainfo[i].invincibleTimer > 0) { --charainfo[i].invincibleTimer; continue; }
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
                        if (mergeSmoke && targetDistSq > 300.0f * 300.0f) ++smokeLongRangeMoves;
					}

					if (charainfo[i].currentAnimType != 2) {
						charainfo[i].currentAnimType = 2;
						charainfo[i].playtime = 0.0f;

						if (charainfo[i].attachidx != -1) {
							MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
						}
						charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, charainfo[i].enemyWalkAnimation);
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
                                if (charainfo[p].mode != DAMAGE) {
                                    CancelPlayerCombat(playerStates[p]);
                                    ResetMove(charainfo[p]);
                                    SetCharacterAnimation(charainfo[p], playerDamageAnimation);
                                    charainfo[p].mode = DAMAGE;
                                    isAiming[p] = false;
                                }
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
					if (charainfo[j].mode != STAND || FindWhirlwindPullPlayer(charainfo, playerStates, charainfo[j]) >= 0) continue;

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
			// 1P/2P の入力、移動、待機/走り切替、攻撃予約を共通処理する。
			if (isAiming[i] || charainfo[i].mode == DAMAGE || charainfo[i].mode == DOWNMODE) { ResetMove(charainfo[i]); continue; }
            const bool specialWasActive = playerStates[i].isSpecialAttack;
			UpdatePlayerInput(
				charainfo[i],
				playerStates[i],
				playerInputs[i],
				anim_attack,
				animHeavyAttack,
				animSpecialAttack,
				anim_neutral,
				anim_run,
				anim_jumpin,
				SEattackHandle,
				SEjumpHandle,
				specialAttackEffectResourceHandle
			);
            if (!specialWasActive && playerStates[i].isSpecialAttack) game.RecordSpecialAttack(i);
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

		const bool isPlayerOverlappingNow = HitCheck_Capsule_Capsule(
			charainfo[PLAYER1_INDEX].pos,
			VAdd(charainfo[PLAYER1_INDEX].pos, VGet(0, charainfo[PLAYER1_INDEX].charahitinfo.Height, 0)),
			charainfo[PLAYER1_INDEX].charahitinfo.Width / 2,
			charainfo[PLAYER2_INDEX].pos,
			VAdd(charainfo[PLAYER2_INDEX].pos, VGet(0, charainfo[PLAYER2_INDEX].charahitinfo.Height, 0)),
			charainfo[PLAYER2_INDEX].charahitinfo.Width / 2) == TRUE;
		const bool willPlayerOverlapNext = HitCheck_Capsule_Capsule(
			VAdd(charainfo[PLAYER1_INDEX].pos, charainfo[PLAYER1_INDEX].move),
			VAdd(VAdd(charainfo[PLAYER1_INDEX].pos, charainfo[PLAYER1_INDEX].move), VGet(0, charainfo[PLAYER1_INDEX].charahitinfo.Height, 0)),
			charainfo[PLAYER1_INDEX].charahitinfo.Width / 2,
			VAdd(charainfo[PLAYER2_INDEX].pos, charainfo[PLAYER2_INDEX].move),
			VAdd(VAdd(charainfo[PLAYER2_INDEX].pos, charainfo[PLAYER2_INDEX].move), VGet(0, charainfo[PLAYER2_INDEX].charahitinfo.Height, 0)),
			charainfo[PLAYER2_INDEX].charahitinfo.Width / 2) == TRUE;
		if (!isPlayerOverlappingNow && willPlayerOverlapNext) {
			// 新しくプレイヤー同士が重なりそうな場合だけ、横移動を止める。
			// 既に重なっている場合は、離れるための移動を許可する。
			charainfo[PLAYER1_INDEX].move.x = 0.0f;
			charainfo[PLAYER1_INDEX].move.z = 0.0f;
			charainfo[PLAYER2_INDEX].move.x = 0.0f;
			charainfo[PLAYER2_INDEX].move.z = 0.0f;
		}



		for (int i = 0; i < PLAYER_COUNT; i++) {
			for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; e++) {
				if (charainfo[e].mode == NONE || charainfo[e].mode == DOWNMODE) continue;
				const bool isEnemyOverlappingNow = HitCheck_Capsule_Capsule(
					charainfo[i].pos,
					VAdd(charainfo[i].pos, VGet(0, charainfo[i].charahitinfo.Height, 0)),
					charainfo[i].charahitinfo.Width / 2,
					charainfo[e].pos,
					VAdd(charainfo[e].pos, VGet(0, charainfo[e].charahitinfo.Height, 0)),
					charainfo[e].charahitinfo.Width / 2) == TRUE;
				const bool willEnemyOverlapNext = HitCheck_Capsule_Capsule(
					VAdd(charainfo[i].pos, charainfo[i].move),
					VAdd(VAdd(charainfo[i].pos, charainfo[i].move), VGet(0, charainfo[i].charahitinfo.Height, 0)),
					charainfo[i].charahitinfo.Width / 2,
					charainfo[e].pos,
					VAdd(charainfo[e].pos, VGet(0, charainfo[e].charahitinfo.Height, 0)),
					charainfo[e].charahitinfo.Width / 2) == TRUE;
				if (charainfo[e].mode != DOWNMODE && !isEnemyOverlappingNow && willEnemyOverlapNext) {
					// 新しくテスト敵と重なりそうな場合だけ、そのプレイヤーの横移動を止める。
					charainfo[i].move.x = 0.0f;
					charainfo[i].move.z = 0.0f;
				}
			}

			// ==========================================
			// 場外落下（デス）判定
			// ==========================================
			float DEATH_LINE = -300.0f; // このY座標を下回ったら落下死（ステージに合わせて調整）

			if (charainfo[i].pos.y < DEATH_LINE) {
				// 1. 死亡回数（DEATHS）を増やす
				charainfo[i].deaths++;
                if (charainfo[i].mode != DOWNMODE) game.AddDeath(i);
                CancelPlayerCombat(playerStates[i]);

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









		// Apply suction after enemy AI and player movement, before hit detection.
		for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e)
			UpdateWhirlwindPull(charainfo, playerStates, charainfo[e]);
        // Update floor height after pursuit, separation and suction.
        for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
            if (charainfo[e].mode != NONE) PlaceEnemyOnGround(charainfo[e]);
        }

		DrawTriangle3D(PolyCharaHitField[0], PolyCharaHitField[1], PolyCharaHitField[2], GetColor(255, 0, 0), TRUE);
		for (int i = 0; i < PLAYER_COUNT; i++) {
			MV1SetPosition(charainfo[i].model1, charainfo[
				i].pos);
			ApplyPlayerMotionRoot(charainfo[i], playerStates[i]);
			//鞘の座標更新
			if (playerSayaFrame[i] != -1 && playerSayaModel[i] != -1) {
				sayamatrix[i] = MV1GetFrameLocalWorldMatrix(charainfo[i].model1, playerSayaFrame[i]);
				MV1SetMatrix(playerSayaModel[i], sayamatrix[i]);
			}
			//武器の座標更新
			if (playerWeaponFrame[i] != -1 && (PLAYER_USE_NEW_MODEL || playerWeaponModel[i] != -1)) {
				wpmatrix[i] = MV1GetFrameLocalWorldMatrix(charainfo[i].model1, playerWeaponFrame[i]);
				if (playerWeaponModel[i] != -1) MV1SetMatrix(playerWeaponModel[i], wpmatrix[i]);
				//攻撃判定の更新
				wpPosStart[i] = VGet(0.0f, 0.0f, 0.0f);
				wpPosEnd[i] = VGet(0.0f, -90.0f, 0.0f);
				wpPosStart[i] = VTransform(wpPosStart[i], wpmatrix[i]);
				wpPosEnd[i] = VTransform(wpPosEnd[i], wpmatrix[i]);
				for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; e++) {
					// 生成されていない敵（NONE）はスキップ
					if (playerStates[i].isSpecialAttack || charainfo[e].mode == NONE) continue;
					CheckAttackHit(game, charainfo, &charainfo[i], &charainfo[e], wpPosStart[i], wpPosEnd[i], SEdamageHandle, anim_damage);
				}
			}

			for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; e++) {
				CheckPlayerSpecialAttackHit(game, charainfo[i], playerStates[i], charainfo[e],
					SEdamageHandle, anim_damage, i, e);
			}
		}

		MV1SetAttachAnimTime(charainfo[TEST_ENEMY_INDEX].model1, charainfo[TEST_ENEMY_INDEX].attachidx, charainfo[TEST_ENEMY_INDEX].playtime);

		if ((playerStates[PLAYER1_INDEX].key & PAD_INPUT_2) && charainfo[TEST_ENEMY_INDEX].mode == DOWNMODE) {
			charainfo[TEST_ENEMY_INDEX].enemyHP = 6;
			charainfo[TEST_ENEMY_INDEX].mode = STAND;
			SetCharacterAnimation(charainfo[TEST_ENEMY_INDEX], enemy_anim_neutral);
		}


		// 画面の消去
		UpdateEffekseer3D();
		skyRot += 0.001f;
		MV1SetRotationXYZ(sky, VGet(0, skyRot, 0));
		ClearDrawScreen();



		// 四角形を表示 最後の引数をfalseにすると塗りつぶし無し
		DrawBox(0, 0, 900, 600, GetColor(255, 255, 255), true);





		//ここから下が２重ループになってて重くなってる原因！
		for (int pIdx = 0; pIdx < PLAYER_COUNT; pIdx++) {

			// 1. 左右の画面領域（ビューポート）を決定
			int startX = (pIdx == 0) ? 0 : 450;
			int endX = (pIdx == 0) ? 450 : 900;

			SetDrawArea(startX, 0, endX, 600);

			// 2. 現在のプレイヤーの座標を取得
			VECTOR pPos = charainfo[pIdx].pos;

			// 3. 【カメラ位置と注視点の調整（斜め上見下ろし視点）】
			VECTOR cPos, cTarget;

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

			SyncEffekseerDrawAreaProjection();
			for (int i = 0; i < PLAYER_COUNT; i++) {
				DrawPlayerHeavyAttackEffect(charainfo[i], playerStates[i], heavyAttackEffectHandle);
			}
			DrawEffekseer3D();
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
			//DrawCrownIcon(pIdx, startX, 20, game.p1Score, game.p2Score, crownGraphHandle);

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
		SetDrawArea(0, 0, 900, 600);
		if (mergeSmoke) {
			if (isAiming[0]) mergeAimPassed = true;
			if (thrownFlashes[0].active && flashStock[0] == 0) mergeThrowPassed = true;
		}
		game.Update(charainfo, charainfo);
		game.DrawTimer(900, 600);
		ScreenFlip();
	}
	// DXライブラリの終了処理
	if (normalAttackEffectResourceHandle != -1) {
		DeleteEffekseerEffect(normalAttackEffectResourceHandle);
	}
	for (int i = 0; i < PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT; i++) {
		if (heavyAttackEffectHandle[i] != -1) {
			DeleteGraph(heavyAttackEffectHandle[i]);
		}
	}
	if (specialAttackEffectResourceHandle != -1) {
		DeleteEffekseerEffect(specialAttackEffectResourceHandle);
	}
	if (mergeSmoke) {
        FILE* report = nullptr; fopen_s(&report, "merge-smoke.txt", "w");
        bool redSpawned = false;
        for (int e = TEST_ENEMY_RED; e < MAX_CHARA; ++e) redSpawned = redSpawned || charainfo[e].mode != NONE;
        const bool timerPassed = game.mainTimer < 99 * 60;
        int activeSpawned = 0;
        for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) if (charainfo[e].mode != NONE) ++activeSpawned;
        if (report) fprintf(report, "spawn count=%d long range pursuit=%d\n", activeSpawned, smokeLongRangeMoves);
        bool grounded = true;
        for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
            if (charainfo[e].mode == NONE) continue;
            const auto hit = MV1CollCheck_Line(stagedata, -1,
                VGet(charainfo[e].pos.x, 2000, charainfo[e].pos.z),
                VGet(charainfo[e].pos.x, -1000, charainfo[e].pos.z));
            grounded = grounded && hit.HitFlag && fabsf(charainfo[e].pos.y - hit.HitPosition.y) < 0.1f;
        }
        if (report) fprintf(report, "enemy ground contact=%d\n", grounded);
        const bool passed = mergeAimPassed && mergeThrowPassed && mergeStunPassed && redSpawned && timerPassed && grounded && activeSpawned >= 20 && smokeLongRangeMoves > 0;
        if (report) { fprintf(report, "aim=%d throw=%d stun=%d redSpawn=%d timer=%d %s\n", mergeAimPassed, mergeThrowPassed, mergeStunPassed, redSpawned, timerPassed, passed ? "PASS" : "FAIL"); fclose(report); }
        if (!passed) return 1;
    }
    if (deathSmoke) {
        FILE* report = nullptr; fopen_s(&report, "death-smoke.txt", "w");
        const bool passed = game.deathCount[0] == 1 && game.deathCount[1] == 1 &&
            charainfo[0].mode == DOWNMODE && charainfo[1].mode == DOWNMODE;
        if (report) { fprintf(report, "both players dead; death counts=%d,%d; frames=%d; %s\n", game.deathCount[0], game.deathCount[1], smokeFrames, passed ? "PASS" : "FAIL"); fclose(report); }
        if (!passed) return 1;
    }
    if (uiSmoke) {
        FILE* report = nullptr; fopen_s(&report, "ui-smoke.txt", "w");
        const bool passed = uiStarted && uiResults && uiReturned && uiRestarted && game.mainTimer > 0 && game.p1Score == 0 && game.p2Score == 0;
        if (report) { fprintf(report, "start=%d results=%d menu=%d restart=%d %s\n", uiStarted, uiResults, uiReturned, uiRestarted, passed ? "PASS" : "FAIL"); fclose(report); }
        if (!passed) return 1;
    }
	runtimeCleanup.effectReady = false; Effkseer_End();

	runtimeCleanup.dxReady = false; DxLib_End();


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
				target->attachidx = MV1AttachAnim(target->model1, 0, (PLAYER_USE_NEW_MODEL && (target == &charainfo[0] || target == &charainfo[1])) ? playerDamageAnimation : anim_damage);
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
							game.AddScore(0, 100); game.AddScorePopup(0, 100);
						}
						else if (attacker == &charainfo[1]) {
							game.AddScore(1, 100); game.AddScorePopup(1, 100);
						}
					}
					else {
						// まだHPが残っている場合はダメージモーション
						MV1DetachAnim(target->model1, target->attachidx);
						target->attachidx = MV1AttachAnim(target->model1, 0, (PLAYER_USE_NEW_MODEL && (target == &charainfo[0] || target == &charainfo[1])) ? playerDamageAnimation : anim_damage);
						target->anim_totaltime = MV1GetAttachAnimTotalTime(target->model1, target->attachidx);
						target->playtime = 0.0f;
						target->mode = DAMAGE;
					}
				}
			}
		}
	}
}
