#include "EffekseerForDXLib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "game.h"
#include "player.h"
#include "PlayerMotionAssets.h"
#include "GameTiming.h"
#include "RuntimeProfile.h"
#include "OrbitCamera.h"
#include "../DXLib/DxLib.h"
#include "senk.h"
void DrawGameMenu(bool results, const GameManager& game, int selected, const TCHAR* snapshot = nullptr);

// Alpha fades from the center to the edge of a ground-aligned disk.
static void DrawPlayerGroundShadow(const MV1_COLL_RESULT_POLY& ground, float height) {
    if (!ground.HitFlag || ground.Normal.y < 0.35f || height > 600.0f) return;
    const float fade = 1.0f - fmaxf(0.0f, height) / 600.0f;
    const float radius = 55.0f + fmaxf(0.0f, height) * 0.035f;
    const VECTOR center = VAdd(ground.HitPosition, VScale(ground.Normal, 0.6f));
    const VECTOR tangent = VNorm(VCross(VGet(0, 0, 1), ground.Normal));
    const VECTOR bitangent = VCross(ground.Normal, tangent);
    constexpr int segments = 40;
    VERTEX3D vertices[segments * 3] = {};
    for (int i = 0; i < segments; ++i) {
        vertices[i * 3].pos = center;
        vertices[i * 3].norm = ground.Normal;
        vertices[i * 3].dif = GetColorU8(0, 0, 0, (int)(170.0f * fade));
        for (int edge = 0; edge < 2; ++edge) {
            const float angle = (i + edge) * (6.283185307f / segments);
            auto& vertex = vertices[i * 3 + edge + 1];
            vertex.pos = VAdd(center, VAdd(VScale(tangent, cosf(angle) * radius),
                VScale(bitangent, sinf(angle) * radius)));
            vertex.norm = ground.Normal;
            vertex.dif = GetColorU8(0, 0, 0, 0);
        }
    }
    int blendMode, blendParam;
    GetDrawBlendMode(&blendMode, &blendParam);
    const int culling = GetUseBackCulling();
    SetUseBackCulling(DX_CULLING_NONE);
    SetUseLighting(FALSE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    SetUseZBuffer3D(TRUE);
    DrawPolygon3D(vertices, segments, DX_NONE_GRAPH, TRUE);
    SetUseZBuffer3D(FALSE);
    SetUseBackCulling(culling);
    SetUseLighting(TRUE);
    SetDrawBlendMode(blendMode, blendParam);
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

// 3D円描画関数
void DrawCircle3D(VECTOR center, float radius, unsigned int color, int divNum = 24)
{
    float step = DX_PI_F * 2.0f / (float)divNum;
    for (int i = 0; i < divNum; i++)
    {
        float a1 = step * (float)i;
        float a2 = step * (float)(i + 1);

        VECTOR p1 = VGet(center.x + cosf(a1) * radius, center.y, center.z + sinf(a1) * radius);
        VECTOR p2 = VGet(center.x + cosf(a2) * radius, center.y, center.z + sinf(a2) * radius);

        DrawLine3D(p1, p2, color);
    }
}

void DrawCircle3DFill(VECTOR center, float radius, unsigned int color, int divNum = 24)
{
    float step = DX_PI_F * 2.0f / (float)divNum;
    for (int i = 0; i < divNum; i++)
    {
        float a1 = step * (float)i;
        float a2 = step * (float)(i + 1);

        VECTOR p1 = VGet(center.x + cosf(a1) * radius, center.y, center.z + sinf(a1) * radius);
        VECTOR p2 = VGet(center.x + cosf(a2) * radius, center.y, center.z + sinf(a2) * radius);

        DrawTriangle3D(center, p1, p2, color, TRUE);
    }
}

const int SP_IMAGE_COUNT = 10;
int spGraphHandles[SP_IMAGE_COUNT];

const int MAX_SP_GAUGE = 100;
int spGauge[PLAYER_COUNT] = { 0, 0 };

const int INTERRUPT_COOLDOWN_FRAMES = 60 * 3;
int playerInterferenceCooldown[PLAYER_COUNT] = { 0, 0 };
bool isPlayerInterferenceActive[PLAYER_COUNT] = { false, false };

// ★ プレイヤーのスタン（行動不能）管理変数（グローバル定義）
const int PLAYER_STUN_DURATION_FRAMES = 45; // 約0.75秒
int playerStunTimer[PLAYER_COUNT] = { 0, 0 };

namespace {
    int LoadPlayerAssetModel(const char* fileName) {
        char path[256];
        sprintf_s(path, sizeof(path), "..\\Player\\%s", fileName);
        int handle = MV1LoadModel(path);
        if (handle != -1) return handle;

        sprintf_s(path, sizeof(path), "..\\Data\\Player\\%s", fileName);
        return MV1LoadModel(path);
    }
}

bool SetGameResourceDirectory() {
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


int normalAttackEffectResourceHandle = -1;

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
	const bool performanceSmoke = strstr(lpC, "--performance-smoke") != nullptr;
    const bool longPerformance = strstr(lpC, "--performance-smoke-long") != nullptr;
    const bool shadowSmoke = strstr(lpC, "--shadow-smoke") != nullptr;
	const bool gameSmoke = shadowSmoke || performanceSmoke || uiSmoke || deathSmoke || mergeSmoke || strstr(lpC, "--game-smoke") != nullptr;
    bool mergeAimPassed = false, mergeThrowPassed = false, mergeStunPassed = false;
    int smokeFrames = 0;
    int rootflm;
    MATRIX wpmatrix[PLAYER_COUNT], sayamatrix[PLAYER_COUNT];
    int anim_neutral, anim_run, anim_jumpin, anim_jumploop, anim_jumpout, anim_damage, anim_down, enemy_anim_attack, enemy_anim_walk, enemy_anim_neutral{};

    int red_goblin_anim_attack = -1;
    int red_goblin_anim_walk = -1;
    int anim_attack[PLAYER_ATTACK_ANIM_COUNT];
    extern int stagedata;
    int sky;
    float skyRot = 0;
    static SCharaInfo charainfo[MAX_CHARA];
    int playerWeaponModel[PLAYER_COUNT], playerWeaponFrame[PLAYER_COUNT];
    int playerSayaModel[PLAYER_COUNT], playerSayaFrame[PLAYER_COUNT];
    int heavyAttackEffectHandle[PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT];
    VECTOR wpPosStart[PLAYER_COUNT], wpPosEnd[PLAYER_COUNT];
    int animHeavyAttack = -1;
    int animSpecialAttack = -1;
    int prevJKey = 0;
    int isBGMPlaying = 1;
    bool isAiming[PLAYER_COUNT] = {};
    bool prevAiming[PLAYER_COUNT] = {};
    float aimAngle[PLAYER_COUNT] = {};
    OrbitCamera orbitCameras[PLAYER_COUNT];
    int grenadeBaseModel = -1;

    // ★ Loading を追加し、タイマー変数を用意
    enum class Screen { Menu, Loading, Playing, Results };
    Screen screen = gameSmoke && !uiSmoke ? Screen::Playing : Screen::Menu;
    int menuSelection = 0;
    bool prevMenuConfirm = false, prevMenuDirection = false;
    bool uiStarted = false, uiResults = false, uiReturned = false, uiRestarted = false;
    int loadingTimer = 0;
    PlayerRuntimeState playerStates[PLAYER_COUNT];

    PlayerInputConfig playerInputs[PLAYER_COUNT] = {
        { DX_INPUT_PAD1, KEY_INPUT_W, KEY_INPUT_S, KEY_INPUT_A, KEY_INPUT_D, KEY_INPUT_SPACE, KEY_INPUT_E, KEY_INPUT_F, KEY_INPUT_Q, PAD_INPUT_1, PAD_INPUT_3, PAD_INPUT_4, PAD_INPUT_2 },
		// 2P: キーボード(矢印) + 2Pゲームパッド。
		{ DX_INPUT_PAD2, KEY_INPUT_UP, KEY_INPUT_DOWN, KEY_INPUT_LEFT, KEY_INPUT_RIGHT, KEY_INPUT_NUMPAD1, KEY_INPUT_NUMPAD3, KEY_INPUT_NUMPAD0, KEY_INPUT_NUMPAD2, PAD_INPUT_1, PAD_INPUT_3, PAD_INPUT_4, PAD_INPUT_2 }
    };

    float attackInEndTime[PLAYER_ATTACK_ANIM_COUNT] = { ATTACK_FIRST_ENDTIME, ATTACK_SECOND_ENDTIME, ATTACK_THIERD_ENDTIME };

    int prevVKeyP1 = 0;
    int prevVKeyP2 = 0;
    GameManager game;
    game.Init();
    VECTOR stagepos = VGet(0.0f, 0.0f, 1200.0f);

    MV1_COLL_RESULT_POLY_DIM HitDim;
    int WallNum;
    int FloorNum;
    MV1_COLL_RESULT_POLY* Wall[CHARA_MAX_HITCOLL];
    MV1_COLL_RESULT_POLY* Floor[CHARA_MAX_HITCOLL];
    MV1_COLL_RESULT_POLY* Poly;
    HITRESULT_LINE LineRes;

    char BGM0_FilePath[] = "BGM_stg0.ogg";
    char String[256];
    int BGMSoundHandle;
    int BGMLoopStartPosition = -1;

    char SEattack_FilePath[] = "swish_00.wav", SEjump_FilePath[] = "jumpIn_00.wav", SEdamage_FilePath[] = "dmg_bySword_00.wav";
    char NormalAttackEffect_FilePath[] = "Sword2.efkefc";
    char HeavyAttackEffect_FilePath[] = "重攻撃.png";
    char SpecialAttackEffect_FilePath[] = "swordMega.efkefc";
    int SEattackHandle, SEjumpHandle, SEdamageHandle;
	int specialAttackEffectResourceHandle[PLAYER_COUNT] = { -1, -1 };
    int hpBarTex = -1;

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
    if (gameSmoke) SetAlwaysRunFlag(TRUE);

	// ウインドウサイズの変更
    SetGraphMode(900, 600, 32);
    SetUseDirect3DVersion(DX_DIRECT3D_11);

    if (DxLib_Init() == -1) return -1;

    runtimeCleanup.dxReady = true;
    if (Effekseer_Init(2000) == -1) {
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

	for (int i = TEST_ENEMY_INDEX; i < TEST_ENEMY_RED; ++i) {
        charainfo[i].model1 = -1;
        charainfo[i].attachidx = -1;
        charainfo[i].mode = NONE;
    }
    game.enemyBaseModel = baseGoblinModel;
    // Keep the one initial enemy; create the rest on their first actual spawn.
    charainfo[TEST_ENEMY_INDEX].model1 = baseGoblinModel < 0 ? -1 : MV1DuplicateModel(baseGoblinModel);
    if (charainfo[TEST_ENEMY_INDEX].model1 >= 0) MV1SetVisible(charainfo[TEST_ENEMY_INDEX].model1, FALSE);

    //モデル読み込み
	redGoblinBaseModel = MV1LoadModel("..\\Data\\RedGoblin\\RedGoblin.mv1");
	red_goblin_anim_neutral = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Neutral.mv1");
	red_goblin_anim_walk = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Walk.mv1");
	red_goblin_anim_attack = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Attack1.mv1");
	for (int e = TEST_ENEMY_RED; e < MAX_CHARA; ++e) { charainfo[e].enemyIdleAnimation = red_goblin_anim_neutral; charainfo[e].enemyWalkAnimation = red_goblin_anim_walk; }


	for (int i = TEST_ENEMY_RED; i < MAX_CHARA; ++i) {
        charainfo[i].model1 = -1;
        charainfo[i].attachidx = -1;
        charainfo[i].mode = NONE;
    }

    for (int i = 0; i < PLAYER_COUNT; i++) {
        charainfo[i].model1 = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? "new\\Player_MainModel.mv1" : "PC.mv1");
        if (charainfo[i].model1 == -1) return -1;
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
        OrbitCamera cameraTest[2];
        cameraTest[0].Update(100, -100);
        bool cameraPassed = cameraTest[0].yaw == 0 && cameraTest[0].pitch == OrbitCamera::DefaultPitch;
        for (int tick = 0; tick < 60; ++tick) cameraTest[0].Update(1000, 0);
        cameraPassed = cameraPassed && fabsf(cameraTest[0].yaw - 2.4f) < 0.001f && cameraTest[1].yaw == 0;
        const float stoppedYaw = cameraTest[0].yaw;
        cameraTest[0].Update(0, 0);
        cameraPassed = cameraPassed && cameraTest[0].yaw == stoppedYaw;
        for (int tick = 0; tick < 300; ++tick) cameraTest[0].Update(0, 1000);
        cameraPassed = cameraPassed && cameraTest[0].pitch == 1.15f;
        for (int tick = 0; tick < 300; ++tick) cameraTest[0].Update(0, -1000);
        cameraPassed = cameraPassed && cameraTest[0].pitch == 0.12f;
        passed = passed && cameraPassed;
        if (report) fprintf(report, "right stick camera: deadzone, rotation rate, release, pitch limits and two-player isolation: %s\n", cameraPassed ? "PASS" : "FAIL");
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
        // Verify real model timelines at 60 simulation ticks per second.
        bool animationTimingPassed = true;
        {
            PlayerRuntimeState timingState{};
            timingState.motionRootFrame = -1;
            auto& actor = charainfo[0];
            actor.mode = RUN;
            SetCharacterAnimation(actor, runMotion);
            for (int tick = 0; tick < 10; ++tick) UpdatePlayerAnimationProgress(actor, timingState, idleMotion);
            animationTimingPassed = fabsf(actor.playtime - 10.0f) < 0.001f;
            actor.mode = DAMAGE;
            SetCharacterAnimation(actor, hurtMotion);
            const int hurtTicks = static_cast<int>(ceilf(actor.anim_totaltime / PlayerAnimationStep)) + 1;
            for (int tick = 0; tick < hurtTicks; ++tick) UpdatePlayerAnimationProgress(actor, timingState, idleMotion);
            animationTimingPassed = animationTimingPassed && actor.mode == STAND;
            actor.mode = DOWNMODE;
            SetCharacterAnimation(actor, deathMotion);
            const float deathEnd = actor.anim_totaltime;
            for (int tick = 0; tick < static_cast<int>(ceilf(deathEnd / PlayerAnimationStep)); ++tick) UpdatePlayerAnimationProgress(actor, timingState, idleMotion);
            animationTimingPassed = animationTimingPassed && actor.playtime == deathEnd;
            const int castingMotion = LoadPlayerAssetModel(PlayerMotions::files[PlayerMotions::specialAttack]);
            actor.mode = ATTACK;
                actor.pos = VGet(0, 0, 0);
            SetCharacterAnimation(actor, castingMotion);
            timingState.isSpecialAttack = timingState.isSpecialAttackEffectPlaying = true;
            const int castTicks = static_cast<int>(actor.anim_totaltime) + 5;
            for (int tick = 0; tick < castTicks; ++tick) UpdatePlayerAnimationProgress(actor, timingState, idleMotion);
            animationTimingPassed = animationTimingPassed && timingState.isSpecialAttackEffectPlaying && actor.playtime > 0 && actor.playtime < actor.anim_totaltime;
            CancelPlayerCombat(timingState);
            actor.mode = STAND;
            SetCharacterAnimation(actor, idleMotion);
            MV1DeleteModel(castingMotion);
        }
        passed = passed && animationTimingPassed;
        if (report) fprintf(report, "60Hz animation pace, hurt/death duration and continuous casting motion: %s\n", animationTimingPassed ? "PASS" : "FAIL");
        if (hurtMotion >= 0) MV1DeleteModel(hurtMotion);
        if (deathMotion >= 0) MV1DeleteModel(deathMotion);
        bool timingPassed = true;
        for (int hz : { 30, 60, 120 }) {
            FixedStepClock clock; clock.Reset(0); int ticks = 0;
            for (int frame = 1; frame <= hz * 10; ++frame) ticks += clock.Advance(frame * 1000000LL / hz);
            timingPassed = timingPassed && ticks == 600;
        }
        FixedStepClock delayed; delayed.Reset(0); int delayedTicks = 0;
        for (int frame = 1; frame <= 300; ++frame) delayedTicks += delayed.Advance(frame * 40000LL);
        timingPassed = timingPassed && delayedTicks == 720;
        passed = passed && timingPassed;
        if (report) fprintf(report, "fixed 60Hz logic at 30/60/120Hz presentation and delayed frames: %s\n", timingPassed ? "PASS" : "FAIL");
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
            bool areaPassed = true;
            for (int p = 0; p < PLAYER_COUNT; ++p) {
                auto& actor = charainfo[p];
                actor.mode = ATTACK;
                actor.pos = VGet(0, 0, 0);
                actor.isHit = false;
                const VECTOR start = VGet(0, 40, 0), end = VGet(0, 40, 90);
                PlayPlayerNormalAttackEffect(actor, playerStates[p]);
                for (int e = 2; e <= 4; ++e) {
                    charainfo[e].mode = STAND;
                    charainfo[e].enemyHP = 3;
                    charainfo[e].charahitinfo.Height = 100;
                    charainfo[e].charahitinfo.Width = 40;
                    charainfo[e].pos = VGet(e == 4 ? 1000.0f : (e == 2 ? -20.0f : 20.0f), 0, 45);
                    CheckAttackHit(game, charainfo, &actor, &charainfo[e], start, end, -1, enemy_anim_neutral, playerStates[p].normalHitTargets);
                }
                areaPassed = areaPassed && charainfo[2].enemyHP == 2 && charainfo[3].enemyHP == 2 && charainfo[4].enemyHP == 3;
                for (int e = 2; e <= 3; ++e) {
                    charainfo[e].mode = STAND;
                    CheckAttackHit(game, charainfo, &actor, &charainfo[e], start, end, -1, enemy_anim_neutral, playerStates[p].normalHitTargets);
                    areaPassed = areaPassed && charainfo[e].enemyHP == 2;
                }
                PlayPlayerNormalAttackEffect(actor, playerStates[p]);
                for (int e = 2; e <= 3; ++e) {
                    areaPassed = areaPassed && charainfo[e].knockbackFrames == 0;
                    CheckAttackHit(game, charainfo, &actor, &charainfo[e], start, end, -1, enemy_anim_neutral,
                        playerStates[p].normalHitTargets, PLAYER_COMBO_FINISHER_KNOCKBACK_SPEED);
                    areaPassed = areaPassed && charainfo[e].enemyHP == 1 &&
                        charainfo[e].knockbackFrames == PLAYER_COMBO_FINISHER_KNOCKBACK_FRAMES;
                    const VECTOR before = charainfo[e].pos;
                    for (int tick = 0; tick < PLAYER_COMBO_FINISHER_KNOCKBACK_FRAMES; ++tick)
                        UpdateEnemyKnockback(charainfo[e]);
                    const VECTOR after = charainfo[e].pos;
                    areaPassed = areaPassed && after.z > before.z && after.y == before.y &&
                        (after.x - before.x) * before.x > 0 && charainfo[e].knockbackFrames == 0;
                    UpdateEnemyKnockback(charainfo[e]);
                    areaPassed = areaPassed && charainfo[e].pos.x == after.x && charainfo[e].pos.z == after.z;
                    charainfo[e].pos = before;
                }
                // Heavy attacks retain their existing one-target behavior.
                actor.isHit = false;
                for (int e = 2; e <= 3; ++e) {
                    charainfo[e].mode = STAND;
                    charainfo[e].enemyHP = 3;
                    CheckAttackHit(game, charainfo, &actor, &charainfo[e], start, end, -1, enemy_anim_neutral);
                }
                areaPassed = areaPassed && charainfo[2].enemyHP == 2 && charainfo[3].enemyHP == 3;
            }
            passed = passed && areaPassed;
            if (report) fprintf(report, "normal area attack: both players, multiple targets, range, one hit per swing, combo reset, finisher knockback and unchanged heavy attack: %s\n", areaPassed ? "PASS" : "FAIL");
            for (int frame = 0; frame < 240; ++frame) {
                UpdateEffekseer3D();
                for (int p = 0; p < PLAYER_COUNT; ++p) UpdatePlayerAnimationProgress(charainfo[p], playerStates[p], idleMotion);
            }
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
        attacker.mode = ATTACK;
		for (int frame = 0; frame < PLAYER_SPECIAL_ATTACK_EFFECT_FRAME_COUNT; ++frame) {
			UpdateEffekseer3D();
            UpdatePlayerAnimationProgress(attacker, megaState, idleMotion);
            if (frame == PLAYER_SPECIAL_ATTACK_MOVE_LOCK_FRAMES - 2)
                megaPassed = megaPassed && attacker.mode == ATTACK;
            if (frame == PLAYER_SPECIAL_ATTACK_MOVE_LOCK_FRAMES - 1) {
                megaPassed = megaPassed && attacker.mode == STAND && megaState.isSpecialAttackEffectPlaying;
                UpdatePlayerMovement(attacker, megaState, MOVE_SPEED, 0, runMotion);
                megaPassed = megaPassed && attacker.mode == RUN && attacker.move.x == MOVE_SPEED;
            }
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
        recoveryPassed = recoveryPassed && fabsf(attacker.playtime - recoveryTime - (PLAYER_USE_NEW_MODEL ? PlayerAnimationStep : LegacyPlayerAnimationStep)) < 0.001f;
        passed = passed && recoveryPassed;
        if (report) fprintf(report, "special completion restores running speed and animation: %s\n", recoveryPassed ? "PASS" : "FAIL");
		if (megaEffect >= 0) DeleteEffekseerEffect(megaEffect);
		passed = passed && megaPassed;
		if (report) fprintf(report, "SwordMega load, startup, forward strike, area, multiple targets, single hit and cleanup: %s\n", megaPassed ? "PASS" : "FAIL");
		int bloodLanceEffect = LoadEffekseerEffect("..\\Data\\Effect\\BloodLance.efkefc", PLAYER_BLOOD_LANCE_EFFECT_SCALE);
                auto bloodLanceResource = GetEffekseerEffect(bloodLanceEffect);
        if (report && bloodLanceResource) {
            for (int t = 0; t < bloodLanceResource->GetColorImageCount(); ++t) {
                char path[512] = {};
                WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)bloodLanceResource->GetColorImagePath(t), -1, path, 512, nullptr, nullptr);
                auto tex = bloodLanceResource->GetColorImage(t);
                fprintf(report, "bloodLance texture %s loaded=%d size=%dx%d\n", path, !!tex, tex ? tex->GetWidth() : 0, tex ? tex->GetHeight() : 0);

            }
            for (int m = 0; m < bloodLanceResource->GetModelCount(); ++m) fprintf(report, "bloodLance model %d loaded=%d\n", m, !!bloodLanceResource->GetModel(m));
        }
        PlayerRuntimeState bloodLanceState;
        bloodLanceState.bloodLanceSpecial = true;
        attacker.pos = VGet(0, 0, 0); attacker.direction = Direction::DOWN;
        PlayPlayerSpecialAttack(attacker, bloodLanceState, idleMotion, -1, bloodLanceEffect);
        bool bloodLancePassed = bloodLanceEffect >= 0 && bloodLanceState.specialAttackPlayingHandle >= 0 && bloodLanceState.isSpecialAttack;
        if (!bloodLanceResource) bloodLancePassed = false;
        else {
            for (int t = 0; t < bloodLanceResource->GetColorImageCount(); ++t) bloodLancePassed = bloodLancePassed && !!bloodLanceResource->GetColorImage(t);
            for (int t = 0; t < bloodLanceResource->GetDistortionImageCount(); ++t) bloodLancePassed = bloodLancePassed && !!bloodLanceResource->GetDistortionImage(t);
            for (int m = 0; m < bloodLanceResource->GetModelCount(); ++m) bloodLancePassed = bloodLancePassed && !!bloodLanceResource->GetModel(m);
        }
        SCharaInfo& bloodLanceTarget = charainfo[2];
        bloodLanceTarget.pos = VGet(0, 0, -200); bloodLanceTarget.mode = STAND; bloodLanceTarget.enemyHP = 6;
        bloodLanceState.specialAttackEffectFrame = 47;
        CheckPlayerSpecialAttackHit(game, attacker, bloodLanceState, bloodLanceTarget, -1, -1, 1, 2);
        bloodLancePassed = bloodLancePassed && bloodLanceTarget.enemyHP == 6;
        bloodLanceState.specialAttackEffectFrame = 60;
        CheckPlayerSpecialAttackHit(game, attacker, bloodLanceState, bloodLanceTarget, -1, -1, 1, 2);
        bloodLancePassed = bloodLancePassed && bloodLanceTarget.enemyHP == 4;
        bloodLanceTarget.mode = STAND;
        CheckPlayerSpecialAttackHit(game, attacker, bloodLanceState, bloodLanceTarget, -1, -1, 1, 2);
        bloodLancePassed = bloodLancePassed && bloodLanceTarget.enemyHP == 4;
        bloodLanceState.specialAttackEffectFrame = 0;
        for (int frame = 0; frame < PLAYER_BLOOD_LANCE_EFFECT_FRAMES; ++frame) {
            UpdatePlayerAnimationProgress(attacker, bloodLanceState, idleMotion);
            UpdateEffekseer3D();
            if (frame == 65 || frame == 100) {
                SetDrawScreen(DX_SCREEN_BACK); SetDrawArea(0, 0, 900, 600); ClearDrawScreen();
                MV1SetPosition(attacker.model1, attacker.pos);
                SetCameraPositionAndTargetAndUpVec(VGet(480, 420, -900), VGet(0, 100, 0), VGet(0, 1, 0));
                MV1DrawModel(attacker.model1); SyncEffekseerDrawAreaProjection(); DrawEffekseer3D();
                SaveDrawScreenToPNG(0, 0, 900, 600, frame == 65 ? "..\\.merge-review\\bloodLance-slash.png" : "..\\.merge-review\\bloodLance-strike.png");
            }
        }
        bloodLancePassed = bloodLancePassed && !bloodLanceState.isSpecialAttack && !bloodLanceState.isSpecialAttackEffectPlaying && bloodLanceState.specialAttackPlayingHandle == -1;
        bloodLanceResource.Reset();
        if (bloodLanceEffect >= 0) DeleteEffekseerEffect(bloodLanceEffect);
        passed = passed && bloodLancePassed;
        if (report) fprintf(report, "P2 BloodLance: runtime resources, start, active window, single hit, completion: %s\n", bloodLancePassed ? "PASS" : "FAIL");
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

    MV1SetMaterialDrawAddColorAll(charainfo[PLAYER2_INDEX].model1, 0, 0, 60);

    charainfo[TEST_ENEMY_GOLEM].model1 = MV1LoadModel("..\\Data\\Golem\\Golem.mv1");
    if (charainfo[TEST_ENEMY_GOLEM].model1 != -1) {
        MV1SetVisible(charainfo[TEST_ENEMY_GOLEM].model1, FALSE);
        charainfo[TEST_ENEMY_GOLEM].mode = NONE;
        MV1SetScale(charainfo[TEST_ENEMY_GOLEM].model1, VGet(1.0f, 1.0f, 1.0f));
    }

    for (int i = 0; !PLAYER_USE_NEW_MODEL && i < PLAYER_COUNT; i++) {
        rootflm = MV1SearchFrame(charainfo[i].model1, "root");
        if (rootflm != -1) MV1SetFrameUserLocalMatrix(charainfo[i].model1, rootflm, MGetIdent());
    }
    rootflm = MV1SearchFrame(charainfo[TEST_ENEMY_INDEX].model1, "root");
    if (rootflm != -1) MV1SetFrameUserLocalMatrix(charainfo[TEST_ENEMY_INDEX].model1, rootflm, MGetIdent());

    for (int i = 0; i < PLAYER_COUNT; i++) {
        playerWeaponModel[i] = PLAYER_USE_NEW_MODEL ? -1 : LoadPlayerAssetModel("Sabel.mv1");
        if (!PLAYER_USE_NEW_MODEL && playerWeaponModel[i] == -1) return -1;
        playerWeaponFrame[i] = MV1SearchFrame(charainfo[i].model1, PLAYER_USE_NEW_MODEL ? "mixamorig:RightHand" : "wp");
        playerSayaModel[i] = PLAYER_USE_NEW_MODEL ? -1 : LoadPlayerAssetModel("Saya.mv1");
        if (!PLAYER_USE_NEW_MODEL && playerSayaModel[i] == -1) return -1;
        playerSayaFrame[i] = MV1SearchFrame(charainfo[i].model1, "sayabone");
    }

    stagedata = MV1LoadModel("..\\Data\\Stage\\Stage_4545.mv1");
    if (stagedata == -1) return -1;
    MV1SetPosition(stagedata, stagepos);
    int meshNum = MV1GetMeshNum(stagedata);
    for (int i = 0; i < meshNum; i++) MV1SetMeshBackCulling(stagedata, i, FALSE);

    sky = MV1LoadModel("..\\Data\\Stage\\Sage_1214.mv1");
    if (sky == -1) return -1;
    MV1SetPosition(sky, VGet(0, -1000, 0));

    MV1SetupCollInfo(stagedata, -1);

    if (grenadeBaseModel == -1) {
        grenadeBaseModel = MV1LoadModel("..\\Data\\Weapon\\Senkou.mv1");
        int flashTexHandle = LoadGraph("..\\Data\\Weapon\\Cosmic_Retro_Grenades_Pack_1_Albedo.png");
        if (flashTexHandle != -1) MV1SetTextureGraphHandle(grenadeBaseModel, 0, flashTexHandle, FALSE);
    }
    if (grenadeBaseModel != -1) MV1SetVisible(grenadeBaseModel, FALSE);

    struct ThrownFlashbang {
        int model;
        VECTOR pos;
        VECTOR move;
        int timer;
        bool active;
        int ownerIndex;
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

    const int MAX_DROP_FLASH = 5;
    FlashDropItem dropFlashItems[MAX_DROP_FLASH];
    for (int i = 0; i < MAX_DROP_FLASH; i++) {
        dropFlashItems[i].active = false;
        dropFlashItems[i].basePosY = 0.0f;
        dropFlashItems[i].bobAngle = (float)i * DX_PI_F;
        dropFlashItems[i].pos = VGet(0.0f, 0.0f, 0.0f);
        dropFlashItems[i].model = (grenadeBaseModel != -1) ? MV1DuplicateModel(grenadeBaseModel) : -1;
        if (dropFlashItems[i].model != -1) {
            MV1SetVisible(dropFlashItems[i].model, FALSE);
            MV1SetScale(dropFlashItems[i].model, VGet(1.3f, 1.3f, 1.3f));
        }
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

    enemy_anim_attack = MV1LoadModel("..\\Data\\Goblin\\Anim_Attack1.mv1");
    if (enemy_anim_attack == -1) return -1;
    enemy_anim_walk = MV1LoadModel("..\\Data\\Goblin\\Anim_Walk.mv1");
    if (enemy_anim_walk == -1) return -1;
    enemy_anim_neutral = MV1LoadModel("..\\Data\\Goblin\\Anim_Neutral.mv1");
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

    hpBarTex = LoadGraph("..\\Data\\UI\\Hpbar023.png");
    SetTransColor(0, 0, 0);
    int spFrameHandle = LoadGraph("..\\Data\\UI\\Firefly.png");

    int pvpIconHandle = LoadGraph("..\\Data\\UI\\Aicon.png");
    if (pvpIconHandle == -1) {
        pvpIconHandle = LoadGraph("..\\Data\\UI\\Aicon.jpg");
    }

    for (int i = 0; i < PLAYER_COUNT; i++) {
        charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_neutral);
        charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
    }
    charainfo[TEST_ENEMY_INDEX].attachidx = MV1AttachAnim(charainfo[TEST_ENEMY_INDEX].model1, 0, enemy_anim_neutral);
    charainfo[TEST_ENEMY_INDEX].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[TEST_ENEMY_INDEX].model1, charainfo[TEST_ENEMY_INDEX].attachidx);

    SetDrawScreen(DX_SCREEN_BACK);

    sprintf_s(String, SOUND_DIRECTORY_PATH "BGM\\%s", BGM0_FilePath);
    BGMSoundHandle = LoadSoundMem(String);
    if (BGMSoundHandle != -1) PlaySoundMem(BGMSoundHandle, DX_PLAYTYPE_LOOP);

    sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Weapon\\Sword\\%s", SEattack_FilePath);
    SEattackHandle = LoadSoundMem(String);
    sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Player\\%s", SEdamage_FilePath);
    SEdamageHandle = LoadSoundMem(String);
    sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Player\\%s", SEjump_FilePath);
    SEjumpHandle = LoadSoundMem(String);

    sprintf_s(String, sizeof(String), "..\\Data\\Effect\\%s", NormalAttackEffect_FilePath);
    normalAttackEffectResourceHandle = LoadEffekseerEffect(String, PLAYER_NORMAL_ATTACK_EFFECT_MAGNIFICATION);
    for (int i = 0; i < PLAYER_COUNT; i++) playerStates[i].normalAttackEffectResourceHandle = normalAttackEffectResourceHandle;

    sprintf_s(String, sizeof(String), "..\\Data\\Effect\\%s", HeavyAttackEffect_FilePath);
    LoadDivGraph(String, PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT, PLAYER_HEAVY_ATTACK_EFFECT_COLUMN_COUNT, PLAYER_HEAVY_ATTACK_EFFECT_ROW_COUNT, PLAYER_HEAVY_ATTACK_EFFECT_FRAME_WIDTH, PLAYER_HEAVY_ATTACK_EFFECT_FRAME_HEIGHT, heavyAttackEffectHandle);

    sprintf_s(String, sizeof(String), "..\\Data\\Effect\\%s", SpecialAttackEffect_FilePath);
	specialAttackEffectResourceHandle[0] = LoadEffekseerEffect(String, PLAYER_SPECIAL_ATTACK_EFFECT_MAGNIFICATION);
    specialAttackEffectResourceHandle[1] = LoadEffekseerEffect("..\\Data\\Effect\\BloodLance.efkefc", PLAYER_BLOOD_LANCE_EFFECT_SCALE);
    playerStates[1].bloodLanceSpecial = true;
	if (specialAttackEffectResourceHandle[0] == -1 || specialAttackEffectResourceHandle[1] == -1) {
		return -1;
	}
    struct FrameCost { double update = 0, render = 0, present = 0, maximum = 0; int count = 0; } frameCost[2];
    double effectUpdateMs = 0, effectDrawMs = 0, worldDrawMs = 0;
    int effectUpdates = 0, worldDraws = 0;
    LONGLONG frameStart = 0, renderStart = 0, presentStart = 0;
    FixedStepClock logicClock;
    RuntimeProfile runtimeProfile;
    int simulationTicks = 0;
    const LONGLONG simulationWallStart = GetNowHiPerformanceCount();
    auto resetMatch = [&]() {
        game.Init();
        for (auto& camera : orbitCameras) camera = OrbitCamera{};
        flashSpawnTimer = 0;
        for (auto& item : dropFlashItems) {
            item.active = false;
            if (item.model >= 0) MV1SetVisible(item.model, FALSE);
        }
        for (int p = 0; p < PLAYER_COUNT; ++p) {
            auto old = playerStates[p];
            playerStates[p] = PlayerRuntimeState{};
            playerStates[p].bloodLanceSpecial = (p == 1);
            playerStates[p].normalAttackEffectResourceHandle = old.normalAttackEffectResourceHandle;
            playerStates[p].motionRootFrame = old.motionRootFrame;
            playerStates[p].motionRootX = old.motionRootX;
            playerStates[p].motionRootZ = old.motionRootZ;
            charainfo[p].pos = VGet(1300, 800, p == 0 ? 100.0f : -400.0f);
            charainfo[p].HP = MAX_HP; charainfo[p].deaths = 0; charainfo[p].mode = STAND; charainfo[p].isHit = false;
            ResetMove(charainfo[p]); SetCharacterAnimation(charainfo[p], anim_neutral);
            MV1SetPosition(charainfo[p].model1, charainfo[p].pos);
            isAiming[p] = false; prevAiming[p] = false; playerWhiteoutTimer[p] = 0; flashStock[p] = 1;

            // ★ スタンタイマーも確実にリセット
            playerStunTimer[p] = 0;
            // マテリアル発光リセット
            if (p == PLAYER2_INDEX) {
                MV1SetMaterialDrawAddColorAll(charainfo[p].model1, 0, 0, 60);
            }
            else {
                MV1SetMaterialDrawAddColorAll(charainfo[p].model1, 0, 0, 0);
            }
        }

        // 全敵を確実に消去
        for (int e = 0; e < MAX_CHARA; ++e) {
            if (e == PLAYER1_INDEX || e == PLAYER2_INDEX) continue;
            charainfo[e].mode = NONE;
            charainfo[e].enemyHP = 0;
            charainfo[e].invincibleTimer = 0;
            charainfo[e].isHit = false;
            if (charainfo[e].model1 >= 0) {
                MV1SetVisible(charainfo[e].model1, FALSE);
                if (charainfo[e].attachidx != -1) {
                    MV1DetachAnim(charainfo[e].model1, charainfo[e].attachidx);
                    charainfo[e].attachidx = -1;
                }
            }
        }

        // ゴーレム（ボス）を完全に非表示・リセット
        charainfo[TEST_ENEMY_GOLEM].mode = NONE;
        charainfo[TEST_ENEMY_GOLEM].enemyHP = 0;
        if (charainfo[TEST_ENEMY_GOLEM].model1 >= 0) {
            MV1SetVisible(charainfo[TEST_ENEMY_GOLEM].model1, FALSE);
            if (charainfo[TEST_ENEMY_GOLEM].attachidx != -1) {
                MV1DetachAnim(charainfo[TEST_ENEMY_GOLEM].model1, charainfo[TEST_ENEMY_GOLEM].attachidx);
                charainfo[TEST_ENEMY_GOLEM].attachidx = -1;
            }
        }

        for (int f = 0; f < MAX_THROWN_FLASH; ++f) thrownFlashes[f].active = false;
        for (int i = 0; i < MAX_DROP_FLASH; ++i) {
            dropFlashItems[i].active = false;
            if (dropFlashItems[i].model != -1) MV1SetVisible(dropFlashItems[i].model, FALSE);
        }
        flashSpawnTimer = 0;
    };
    while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0 && (!gameSmoke || (performanceSmoke ? simulationTicks < (longPerformance ? 1800 : 720) : smokeFrames++ < (uiSmoke ? 180 : (deathSmoke ? 1800 : (mergeSmoke ? 520 : 360)))))) {
        if (uiSmoke && screen == Screen::Playing && smokeFrames == 60) game.mainTimer = 1;
        if (screen == Screen::Playing && game.gameState != 0) {
            screen = Screen::Results; menuSelection = 0; prevMenuConfirm = true; uiResults = true;
        }
        if (screen == Screen::Menu || screen == Screen::Results) {
            int pad = ReadPlayerPadState(DX_INPUT_PAD1) | ReadPlayerPadState(DX_INPUT_PAD2);
            bool confirm = CheckHitKey(KEY_INPUT_RETURN) || (pad & PAD_INPUT_1);
            bool direction = CheckHitKey(KEY_INPUT_UP) || CheckHitKey(KEY_INPUT_DOWN) || (pad & (PAD_INPUT_UP | PAD_INPUT_DOWN));
            if (uiSmoke) confirm = smokeFrames == 10 || smokeFrames == 90 || smokeFrames == 120;
            if (direction && !prevMenuDirection) menuSelection = 1 - menuSelection;

            if (confirm && !prevMenuConfirm) {
                if (menuSelection == 1) break; // 終了
                if (screen == Screen::Results) {
                    screen = Screen::Menu; uiReturned = true;
                }
                else {
                    // ★ 「ゲーム開始」を押したらロード画面へ移行！
                    resetMatch();
                    if (uiStarted) uiRestarted = true;
                    uiStarted = true;
                    loadingTimer = uiSmoke ? 2 : 50; // 約0.8秒間ロード表示
                    screen = Screen::Loading;
                }
            }
            prevMenuConfirm = confirm; prevMenuDirection = direction;
            logicClock.Reset(GetNowHiPerformanceCount());
            runtimeProfile.Reset(GetNowHiPerformanceCount());
            if (screen != Screen::Playing) { DrawGameMenu(screen == Screen::Results, game, menuSelection, uiSmoke && smokeFrames == 1 ? TEXT("..\\.merge-review\\start-menu.png") : uiSmoke && smokeFrames == 65 ? TEXT("..\\.merge-review\\results.png") : nullptr); WaitTimer(16); }
            continue;
        }

        // ==========================================
        // 2. ロード画面（黒画面＋左下アニメーション）
        // ==========================================
        if (screen == Screen::Loading) {
            GameManager::DrawLoadingScreen();
            loadingTimer--;

            // 時間が切れたら戦闘画面へ！
            if (loadingTimer <= 0) {
                screen = Screen::Playing;
            }
            WaitTimer(16);
            continue;
        }

		frameStart = GetNowHiPerformanceCount();
        const int logicSteps = gameSmoke && !performanceSmoke ? 1 : logicClock.Advance(frameStart);
        if (logicSteps == 0) { WaitTimer(1); continue; }
        for (int logicStep = 0; logicStep < logicSteps; ++logicStep) {
            ++simulationTicks;
            if (performanceSmoke && simulationTicks % 60 == 0) {
                FILE* memory = nullptr;
                fopen_s(&memory, "performance-memory.csv", simulationTicks == 60 ? "w" : "a");
                if (memory) {
                    int models = 0, active = 0;
                    for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
                        if (charainfo[e].model1 >= 0) ++models;
                        if (charainfo[e].mode != NONE) ++active;
                    }
                    if (simulationTicks == 60) fprintf(memory, "tick,dxBytes,dxAllocations,enemyModels,activeEnemies,effectInstances\n");
                    fprintf(memory, "%d,%zu,%d,%d,%d,%d\n", simulationTicks, DxGetAllocSize(), DxGetAllocNum(), models, active, GetEffekseer3DManager()->GetTotalInstanceCount());
                    fclose(memory);
                }
            }
        if (performanceSmoke && simulationTicks == 30) {
            for (int e = 0; e < 80; ++e) game.ActivateEnemy(charainfo, 800.0f + GetRand(1000), -650.0f + GetRand(1000));
        }
        if (performanceSmoke && !strstr(lpC, "--no-special") && simulationTicks >= 120 && (simulationTicks - 120) % 300 == 0) {
            for (int p = 0; p < PLAYER_COUNT; ++p) {
                charainfo[p].HP = MAX_HP;
                PlayPlayerSpecialAttack(charainfo[p], playerStates[p], animSpecialAttack, -1, specialAttackEffectResourceHandle[p]);
            }
        }
        if (deathSmoke && smokeFrames == 100) { charainfo[0].HP = 0; charainfo[1].HP = 0; }
        int currentJKey = CheckHitKey(KEY_INPUT_J);
        RespawnManager::Update(charainfo);
        for (int i = 0; i < PLAYER_COUNT; i++) {
            UpdatePlayerAnimationProgress(charainfo[i], playerStates[i], anim_neutral);
        }
        for (int i = 0; i < PLAYER_COUNT; i++) {
            if (charainfo[i].HP <= 0 && charainfo[i].mode != DOWNMODE) {
                charainfo[i].mode = DOWNMODE;
                CancelPlayerCombat(playerStates[i]);
                ResetMove(charainfo[i]);
                SetCharacterAnimation(charainfo[i], anim_down);
                isAiming[i] = false;
                game.AddDeath(i);
            }
        }
        for (int i = 0; i < PLAYER_COUNT; i++) {
            RespawnManager::CheckAndRespawnDownPlayer(i, charainfo, playerInputs[i], anim_neutral);
        }

			// ゲージ満タンかつボタンが押され、現在攻撃中やダウン中でない場合
for (int p = 0; p < PLAYER_COUNT; ++p) playerStates[p].key = ReadPlayerPadState(playerInputs[p].padType);
		for (int p = 0; p < PLAYER_COUNT; p++) {
			int rightX = 0, rightY = 0;
            GetJoypadAnalogInputRight(&rightX, &rightY, playerInputs[p].padType);
            const float rightYaw = orbitCameras[p].Update(rightX, rightY);
            bool currentAimInput = false;
			if (p == 0) {
				currentAimInput = (CheckHitKey(KEY_INPUT_G) == 1) || (playerStates[p].key & PAD_INPUT_5);
			}
			else {
				currentAimInput = (CheckHitKey(KEY_INPUT_DECIMAL) == 1) || (playerStates[p].key & PAD_INPUT_5);
			}

            if (mergeSmoke && p == 0) currentAimInput = smokeFrames >= 20 && smokeFrames < 30;
            if (flashStock[p] > 0 && charainfo[p].mode != DAMAGE && charainfo[p].mode != DOWNMODE && playerStunTimer[p] <= 0) {
                if (currentAimInput && !prevAiming[p]) {
                    isAiming[p] = true;
                    aimAngle[p] = DX_PI_F + DX_PI_F * 0.5f * charainfo[p].direction;
                }
                else if (currentAimInput && isAiming[p]) {
                    aimAngle[p] += rightYaw;
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
                    if (abs(inputX) > 300) aimAngle[p] += (inputX / 1000.0f) * 0.05f;

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
                            thrownFlashes[f].ownerIndex = p;
                            flashStock[p]--;
                            thrownFlashes[f].pos = VAdd(charainfo[p].pos, VGet(0.0f, 10.0f, 0.0f));
                            float speed = 15.0f;
                            thrownFlashes[f].move = VGet(sinf(aimAngle[p]) * speed, 3.5f, cosf(aimAngle[p]) * speed);
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
        // アイテム回収判定
        for (int i = 0; i < MAX_DROP_FLASH; i++) {
            if (!dropFlashItems[i].active) continue;
            for (int p = 0; p < PLAYER_COUNT; p++) {
                if (charainfo[p].mode == DOWNMODE || charainfo[p].mode == NONE) continue;
                if (charainfo[p].invincibleTimer > 0) continue;

                float dx = charainfo[p].pos.x - dropFlashItems[i].pos.x;
                float dy = charainfo[p].pos.y - dropFlashItems[i].pos.y;
                float dz = charainfo[p].pos.z - dropFlashItems[i].pos.z;
                if (dx * dx + dy * dy + dz * dz < 60.0f * 60.0f) {
                    flashStock[p]++;
                    dropFlashItems[i].active = false;
                    if (dropFlashItems[i].model != -1) MV1SetVisible(dropFlashItems[i].model, FALSE);
                    break;
                }
            }
        }

        // アイテム出現
        bool anyActive = false;
        for (int i = 0; i < MAX_DROP_FLASH; i++) {
            if (dropFlashItems[i].active) anyActive = true;
        }

        if (!anyActive) {
            flashSpawnTimer++;
            if (flashSpawnTimer >= FLASH_SPAWN_INTERVAL) {
                flashSpawnTimer = 0;
                for (int i = 0; i < MAX_DROP_FLASH; i++) {
                    dropFlashItems[i].active = true;
                    float spawnX = 1300.0f + (float)(GetRand(600) - 300);
                    float spawnZ = -150.0f + (float)(GetRand(600) - 300);

                    MV1_COLL_RESULT_POLY hit = MV1CollCheck_Line(stagedata, -1,
                        VGet(spawnX, 2000.0f, spawnZ),
                        VGet(spawnX, -1000.0f, spawnZ)
                    );
                    dropFlashItems[i].basePosY = (hit.HitFlag == 1) ? hit.HitPosition.y : charainfo[0].pos.y;
                    dropFlashItems[i].pos = VGet(spawnX, dropFlashItems[i].basePosY, spawnZ);

                    if (dropFlashItems[i].model != -1) MV1SetVisible(dropFlashItems[i].model, TRUE);
                }
            }
        }

        for (int i = 0; i < MAX_DROP_FLASH; i++) {
            if (!dropFlashItems[i].active) continue;
            dropFlashItems[i].bobAngle += 0.05f;
            float currentY = dropFlashItems[i].basePosY + 8.0f + sinf(dropFlashItems[i].bobAngle) * 3.0f;
            MV1SetPosition(dropFlashItems[i].model, VGet(dropFlashItems[i].pos.x, currentY, dropFlashItems[i].pos.z));
            MV1SetRotationXYZ(dropFlashItems[i].model, VGet(0.0f, dropFlashItems[i].bobAngle, 0.0f));
        }

        // 閃光弾物理挙動
        for (int f = 0; f < MAX_THROWN_FLASH; f++) {
            if (!thrownFlashes[f].active) continue;

            thrownFlashes[f].pos.x += thrownFlashes[f].move.x;
            thrownFlashes[f].pos.y += thrownFlashes[f].move.y;
            thrownFlashes[f].pos.z += thrownFlashes[f].move.z;
            thrownFlashes[f].move.y -= 0.45f;

            if (thrownFlashes[f].pos.y <= 500.0f) {
                thrownFlashes[f].pos.y = 500.0f;
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
                // Preserve the enemy stun response, with duration measured in logic ticks.
                for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
                    auto& enemy = charainfo[e];
                    if (enemy.mode == NONE || enemy.enemyHP <= 0 || enemy.model1 < 0) continue;
                    const float dx = enemy.pos.x - thrownFlashes[f].pos.x;
                    const float dz = enemy.pos.z - thrownFlashes[f].pos.z;
                    if (dx * dx + dz * dz > FLASH_RANGE_SQ) continue;
                    enemy.invincibleTimer = 120;
                    enemy.mode = STAND;
                    enemy.currentAnimType = 1;
                    enemy.enemyWalking = false;
                    ResetMove(enemy);
                    SetCharacterAnimation(enemy, enemy.enemyIdleAnimation);
                }



				// ② 相手プレイヤーの直視判定（投げた本人は食らわない）
                for (int p = 0; p < PLAYER_COUNT; p++) {
                    if (charainfo[p].mode == DOWNMODE || p == thrownFlashes[f].ownerIndex) continue;

                    float toX = thrownFlashes[f].pos.x - charainfo[p].pos.x;
                    float toZ = thrownFlashes[f].pos.z - charainfo[p].pos.z;
                    float pDistSq = toX * toX + toZ * toZ;

                    if (pDistSq < FLASH_RANGE_SQ) {
                        float fwdX = -sinf(DX_PI_F * 0.5f * charainfo[p].direction);
                        float fwdZ = -cosf(DX_PI_F * 0.5f * charainfo[p].direction);
                        float dot = fwdX * toX + fwdZ * toZ;

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

        // ==========================================
        // 敵AI処理（範囲外Tポーズ修正・攻撃負荷軽減版）
        // ==========================================
        for (int i = TEST_ENEMY_INDEX; i < MAX_CHARA; i++) {
            if (charainfo[i].mode == NONE || charainfo[i].model1 < 0) continue;

            int animNeutral = enemy_anim_neutral;
            int animAttack = enemy_anim_attack;

            if (i == TEST_ENEMY_GOLEM) {
                animNeutral = charainfo[TEST_ENEMY_GOLEM].enemyIdleAnimation;
                animAttack = charainfo[TEST_ENEMY_GOLEM].enemyIdleAnimation;
            }
            else if (i >= TEST_ENEMY_RED) {
                animNeutral = red_goblin_anim_neutral;
                animAttack = red_goblin_anim_attack;
            }

            if (mergeSmoke && charainfo[TEST_ENEMY_INDEX].invincibleTimer > 0) mergeStunPassed = true;
            if (charainfo[i].invincibleTimer > 0) {
                --charainfo[i].invincibleTimer;
                continue;
            }

            // 被ダメージ中のアニメーション再生処理
            if (charainfo[i].mode == DAMAGE) {
                if (charainfo[i].playtime >= charainfo[i].anim_totaltime) {
                    charainfo[i].mode = STAND;
                    charainfo[i].playtime = 0.0f;
                    charainfo[i].isHit = false;
                    charainfo[i].currentAnimType = 1;

                    if (charainfo[i].attachidx != -1) MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
                    charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, animNeutral);
                    if (charainfo[i].attachidx != -1) {
                        charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
                        MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, 0.0f);
                    }
                }

				charainfo[i].playtime += EnemyAnimationStep;
				if (charainfo[i].attachidx != -1) {
					MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
				}
                continue;
            }

            bool p1Alive = (charainfo[PLAYER1_INDEX].mode != DOWNMODE && charainfo[PLAYER1_INDEX].mode != NONE);
            bool p2Alive = (charainfo[PLAYER2_INDEX].mode != DOWNMODE && charainfo[PLAYER2_INDEX].mode != NONE);

            if (!p1Alive && !p2Alive) {
                if (charainfo[i].currentAnimType != 1) {
                    charainfo[i].currentAnimType = 1;
                    charainfo[i].playtime = 0.0f;
                    if (charainfo[i].attachidx != -1) MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
                    charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, animNeutral);
                    if (charainfo[i].attachidx != -1) {
                        charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
                        MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, 0.0f);
                    }
                }
                charainfo[i].playtime += 0.2f;
                if (charainfo[i].playtime >= charainfo[i].anim_totaltime) charainfo[i].playtime = 0.0f;
                if (charainfo[i].attachidx != -1) MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
                continue;
            }

            int enemyTargetIndex = -1;
            float targetDistSq = 99999999.0f;

            float dx1 = charainfo[PLAYER1_INDEX].pos.x - charainfo[i].pos.x;
            float dz1 = charainfo[PLAYER1_INDEX].pos.z - charainfo[i].pos.z;
            float distSqP1 = dx1 * dx1 + dz1 * dz1;

            float dx2 = charainfo[PLAYER2_INDEX].pos.x - charainfo[i].pos.x;
            float dz2 = charainfo[PLAYER2_INDEX].pos.z - charainfo[i].pos.z;
            float distSqP2 = dx2 * dx2 + dz2 * dz2;

            if (p1Alive && p2Alive) {
                if (distSqP2 < distSqP1) {
                    enemyTargetIndex = PLAYER2_INDEX;
                    targetDistSq = distSqP2;
                }
                else {
                    enemyTargetIndex = PLAYER1_INDEX;
                    targetDistSq = distSqP1;
                }
            }
            else if (p1Alive) {
                enemyTargetIndex = PLAYER1_INDEX;
                targetDistSq = distSqP1;
            }
            else {
                enemyTargetIndex = PLAYER2_INDEX;
                targetDistSq = distSqP2;
            }

            const float detectRangeSq = 600.0f * 600.0f;
            const float attackRangeSq = 140.0f * 140.0f;

            if (charainfo[i].mode != ATTACK && targetDistSq > detectRangeSq) {
                if (charainfo[i].currentAnimType != 1) {
                    charainfo[i].currentAnimType = 1;
                    charainfo[i].playtime = 0.0f;
                    if (charainfo[i].attachidx != -1) MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);

                    charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, animNeutral);
                    if (charainfo[i].attachidx != -1) {
                        charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
                        MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, 0.0f);
                    }
                }

				// 待機アニメーションのループ更新
				charainfo[i].playtime += EnemyAnimationStep;
				if (charainfo[i].playtime >= charainfo[i].anim_totaltime) {
					charainfo[i].playtime = 0.0f;
				}
				if (charainfo[i].attachidx != -1) {
					MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
				}
				continue; // 向きの変更や移動処理をスキップして静止を維持
            }

            VECTOR dir = VSub(charainfo[enemyTargetIndex].pos, charainfo[i].pos);
            dir.y = 0;
            float angle = atan2f(dir.x, dir.z) + DX_PI_F;
            MV1SetRotationXYZ(charainfo[i].model1, VGet(0.0f, angle, 0.0f));

            if (charainfo[i].mode == STAND) {
                if (targetDistSq <= attackRangeSq) {
                    charainfo[i].mode = ATTACK;
                    charainfo[i].playtime = 0.0f;
                    charainfo[i].isHit = false;
                    charainfo[i].currentAnimType = 3;

                    if (charainfo[i].attachidx != -1) MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
                    charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, animAttack);
                    if (charainfo[i].attachidx != -1) {
                        charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
                        MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, 0.0f);
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
                        if (charainfo[i].attachidx != -1) MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);

                        int walkAnim = charainfo[i].enemyWalkAnimation;
                        if (i >= TEST_ENEMY_RED && i != TEST_ENEMY_GOLEM) {
                            walkAnim = red_goblin_anim_walk;
                        }
                        charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, walkAnim);
                        if (charainfo[i].attachidx != -1) {
                            charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
                            MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, 0.0f);
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
                            if (charainfo[p].invincibleTimer > 0) continue;

                            float cdx = charainfo[i].pos.x - charainfo[p].pos.x;
                            float cdz = charainfo[i].pos.z - charainfo[p].pos.z;
                            if (cdx * cdx + cdz * cdz > 120.0f * 120.0f) continue;

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

                    if (charainfo[i].attachidx != -1) MV1DetachAnim(charainfo[i].model1, charainfo[i].attachidx);
                    charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, animNeutral);
                    if (charainfo[i].attachidx != -1) {
                        charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
                        MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, 0.0f);
                    }
                }
            }

			charainfo[i].playtime += EnemyAnimationStep;
			if (charainfo[i].mode == STAND && charainfo[i].playtime >= charainfo[i].anim_totaltime) {
                charainfo[i].playtime = 0.0f;
            }

            if (charainfo[i].attachidx != -1) {
                MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, charainfo[i].playtime);
            }
        }

        // ==========================================
        // 敵押し合い処理（プレイヤー周辺限定・超高速版）
        // ==========================================
        static int pushFrameCount = 0;
        pushFrameCount++;

        if (pushFrameCount % 3 == 0)
        {

            const float SEARCH_DIST_SQ = 450.0f * 450.0f;
            const float minDistance = 85.0f, minDistanceSq = minDistance * minDistance;

			int crowdIndices[MAX_CHARA], crowdCount = 0;
            bool crowdPulled[MAX_CHARA] = {};
            for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
                if (charainfo[e].mode != STAND) continue;
                crowdIndices[crowdCount++] = e;
                crowdPulled[e] = FindWhirlwindPullPlayer(charainfo, playerStates, charainfo[e]) >= 0;
            }
            for (int a = 0; a < crowdCount; ++a) {
                const int i = crowdIndices[a];
                if (charainfo[i].currentAnimType != 2 || crowdPulled[i]) continue;
                for (int b = a + 1; b < crowdCount; ++b) {
                    const int j = crowdIndices[b];
                    if (crowdPulled[j]) continue;
                    float diffX = charainfo[i].pos.x - charainfo[j].pos.x;
                    if (fabsf(diffX) > minDistance) continue;

                    float diffZ = charainfo[i].pos.z - charainfo[j].pos.z;
                    if (fabsf(diffZ) > minDistance) continue;

                    float distSq = diffX * diffX + diffZ * diffZ;
                    if (distSq < minDistanceSq && distSq > 0.001f) {
                        float actualDist = sqrtf(distSq);
                        float pushDist = (minDistance - actualDist) * 0.35f;

                        float pushX = (diffX / actualDist) * pushDist;
                        float pushZ = (diffZ / actualDist) * pushDist;

                        charainfo[i].pos.x += pushX;
                        charainfo[i].pos.z += pushZ;
                        charainfo[j].pos.x -= pushX;
                        charainfo[j].pos.z -= pushZ;
                    }
                }
            }
        }

        // ==========================================
        // プレイヤー更新
        // ==========================================
        for (int i = 0; i < PLAYER_COUNT; i++) {
            // BOSS演出中の静止
            if (game.IsBossCutsceneActive()) {
                ResetMove(charainfo[i]);
                continue;
            }

            // ========================================================
            // ★ スタン中の処理（専用モーション不要版：痺れ・ブルブル震え・黄色発光）
            // ========================================================
            if (playerStunTimer[i] > 0) {
                playerStunTimer[i]--;
                ResetMove(charainfo[i]);              // 足を止める
                CancelPlayerCombat(playerStates[i]);  // 攻撃動作を中断
                isAiming[i] = false;

                // ① 被弾モーションの痛がっている瞬間でポーズを固定
                if (charainfo[i].attachidx != -1) {
                    float freezeTime = charainfo[i].anim_totaltime * 0.15f;
                    MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, freezeTime);
                }

                // ② 高速左右振動（ブルブル震えて麻痺・硬直感を出す）
                float shakeX = sinf(GetNowCount() * 0.09f) * 3.5f;
                MV1SetPosition(charainfo[i].model1, VAdd(charainfo[i].pos, VGet(shakeX, 0.0f, 0.0f)));

                // ③ 黄色くパルス発光（痺れ・帯電状態）
                int yellowPulse = 80 + (int)(sinf(GetNowCount() * 0.03f) * 60);
                MV1SetMaterialDrawAddColorAll(charainfo[i].model1, yellowPulse, yellowPulse, 0);

                // ★ スタン終了時の復帰処理
                if (playerStunTimer[i] <= 0) {
                    charainfo[i].mode = STAND;
                    charainfo[i].playtime = 0.0f;
                    SetCharacterAnimation(charainfo[i], anim_neutral);
                    MV1SetPosition(charainfo[i].model1, charainfo[i].pos); // 震えた位置を元に戻す

                    // 発光を元に戻す（2Pは青ベース、1Pは通常）
                    if (i == PLAYER2_INDEX) {
                        MV1SetMaterialDrawAddColorAll(charainfo[i].model1, 0, 0, 60);
                    }
                    else {
                        MV1SetMaterialDrawAddColorAll(charainfo[i].model1, 0, 0, 0);
                    }
                }
                continue; // スタン中はこのターンの入力・攻撃更新をすべてスキップ
            }

            if (isAiming[i] || charainfo[i].mode == DAMAGE || charainfo[i].mode == DOWNMODE) {
                ResetMove(charainfo[i]);
                continue;
            }

            // ★ 復活後1秒間（無敵中）は攻撃ボタン入力を無効化して渡す（移動とジャンプのみ許可）
            PlayerInputConfig currentInput = playerInputs[i];
            if (mergeSmoke && charainfo[TEST_ENEMY_INDEX].invincibleTimer > 0) mergeStunPassed = true;
            if (charainfo[i].invincibleTimer > 0) {
                currentInput.attackKey = -1;
                currentInput.heavyAttackKey = -1;
                currentInput.attackPadButton = 0;
                currentInput.heavyAttackPadButton = 0;
            }

            const bool specialWasActive = playerStates[i].isSpecialAttack;
            UpdatePlayerInput(
                charainfo[i],
                playerStates[i],
                currentInput,
                anim_attack,
                animHeavyAttack,
                animSpecialAttack,
                anim_neutral,
                anim_run,
                anim_jumpin,
                SEattackHandle,
                SEjumpHandle,
				specialAttackEffectResourceHandle[i]
            );
            if (!specialWasActive && playerStates[i].isSpecialAttack) game.RecordSpecialAttack(i);
            UpdatePlayerAttackState(
                charainfo[i],
                playerStates[i],
                currentInput,
                anim_attack,
                attackInEndTime,
                SEattackHandle
            );
        }

        if (currentJKey == 1 && prevJKey == 0) {
            if (isBGMPlaying) { StopSoundMem(BGMSoundHandle); isBGMPlaying = 0; }
            else { PlaySoundMem(BGMSoundHandle, DX_PLAYTYPE_LOOP); isBGMPlaying = 1; }
        }
        prevJKey = currentJKey;

        // ==========================================
        // Vキーによる妨害攻撃の即時発動
        // ==========================================
        for (int p = 0; p < PLAYER_COUNT; p++) {
            if (playerInterferenceCooldown[p] > 0) {
                playerInterferenceCooldown[p]--;
            }

            if (charainfo[p].mode != ATTACK) {
                isPlayerInterferenceActive[p] = false;
            }

            int currentVKey = 0;
            if (p == 0) {
                currentVKey = (CheckHitKey(KEY_INPUT_V) == 1 || (playerStates[p].key & PAD_INPUT_6)) ? 1 : 0;
            }
            else {
                currentVKey = (CheckHitKey(KEY_INPUT_NUMPAD1) == 1 || CheckHitKey(KEY_INPUT_B) == 1 || (playerStates[p].key & PAD_INPUT_6)) ? 1 : 0;
            }

            int& prevKey = (p == 0) ? prevVKeyP1 : prevVKeyP2;
            if (currentVKey == 1 && prevKey == 0) {
                if (playerInterferenceCooldown[p] <= 0 &&
                    playerStunTimer[p] <= 0 &&
                    charainfo[p].mode != ATTACK &&
                    charainfo[p].mode != DAMAGE &&
                    charainfo[p].mode != DOWNMODE &&
                    charainfo[p].invincibleTimer <= 0)
                {
                    charainfo[p].mode = ATTACK;
                    charainfo[p].playtime = 0.0f;
                    charainfo[p].isHit = false;
                    isPlayerInterferenceActive[p] = true;

                    SetCharacterAnimation(charainfo[p], anim_attack[0]);
                    PlaySoundMem(SEattackHandle, DX_PLAYTYPE_BACK);
                }
            }
            prevKey = currentVKey;
        }

        for (int i = 0; i < PLAYER_COUNT; i++) {
            if (charainfo[i].invincibleTimer > 0 || playerStunTimer[i] > 0) continue;

            bool spButton = false;
            if (i == 0) {
                if (CheckHitKey(KEY_INPUT_R) == 1 || (ReadPlayerPadState(playerInputs[i].padType) & PAD_INPUT_4)) {
                    spButton = true;
                }
            }
            else {
                if (CheckHitKey(KEY_INPUT_NUMPAD0) == 1 || (ReadPlayerPadState(playerInputs[i].padType) & PAD_INPUT_4)) {
                    spButton = true;
                }
            }

            if (spGauge[i] >= MAX_SP_GAUGE && spButton && charainfo[i].mode == STAND && !playerStates[i].isSpecialAttackEffectPlaying) {
                PlayPlayerSpecialAttack(charainfo[i], playerStates[i], animSpecialAttack, SEattackHandle, specialAttackEffectResourceHandle[i]);
                game.RecordSpecialAttack(i);
                spGauge[i] = 0;
            }
        }

        // プレイヤーの押し出し＆場外落下
        for (int i = 0; i < PLAYER_COUNT; i++) {
            const float PUSH_AWAY_RANGE = 45.0f;
            const float PUSH_POWER = 2.5f;

            for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; e++) {
                if (charainfo[e].mode == NONE || charainfo[e].mode == DOWNMODE) continue;

                float dx = charainfo[e].pos.x - charainfo[i].pos.x;
                float dz = charainfo[e].pos.z - charainfo[i].pos.z;
                float distSq = dx * dx + dz * dz;

                if (distSq < PUSH_AWAY_RANGE * PUSH_AWAY_RANGE && distSq > 0.001f) {
                    float dist = sqrtf(distSq);
                    charainfo[e].pos.x += (dx / dist) * PUSH_POWER;
                    charainfo[e].pos.z += (dz / dist) * PUSH_POWER;
                    MV1SetPosition(charainfo[e].model1, charainfo[e].pos);
                }
            }

            RespawnManager::RespawnFallenPlayer(i, charainfo, playerStates[i], game, anim_neutral);
        }

        // プレイヤー床コリジョン
        for (int i = 0; i < PLAYER_COUNT; i++) {
            if (charainfo[i].mode == DOWNMODE) continue;

            HitDim = MV1CollCheck_Sphere(stagedata, -1, charainfo[i].pos, CHARA_ENUM_DEFAULT_SIZE + VSize(charainfo[i].move));
            WallNum = 0; FloorNum = 0;
            for (int h = 0; h < HitDim.HitNum; h++) {
                if (fabs(HitDim.Dim[h].Normal.y) < 0.5f) {
                    if (WallNum < CHARA_MAX_HITCOLL) Wall[WallNum++] = &HitDim.Dim[h];
                }
                else {
                    if (FloorNum < CHARA_MAX_HITCOLL) Floor[FloorNum++] = &HitDim.Dim[h];
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
                        if (hitFloorFlag == 1 && MaxY > LineRes.Position.y) continue;
                        hitFloorFlag = 1;
                        MaxY = LineRes.Position.y;
                    }
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
                    charainfo[i].move = VGet(0.0f, 0.0f, 0.0f);
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

        // 移動座標反映
        for (int i = 0; i < PLAYER_COUNT; i++) {
            charainfo[i].pos.x += charainfo[i].move.x;
            charainfo[i].pos.y += charainfo[i].move.y;
            charainfo[i].pos.z += charainfo[i].move.z;
            charainfo[i].charahitinfo.CenterPosition = charainfo[i].pos;
        }

        for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) UpdateWhirlwindPull(charainfo, playerStates, charainfo[e]);

        // ★ 敵の床接地判定（8フレームに1回分散チェック）

        for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
            UpdateEnemyKnockback(charainfo[e]);
            if (charainfo[e].mode != NONE) PlaceEnemyOnGround(charainfo[e]);
        }

        // プレイヤーのモデル・武器更新＆当たり判定
        for (int i = 0; i < PLAYER_COUNT; i++) {
            MV1SetPosition(charainfo[i].model1, charainfo[i].pos);
            ApplyPlayerMotionRoot(charainfo[i], playerStates[i]);

            if (playerSayaFrame[i] != -1 && playerSayaModel[i] != -1) {
                sayamatrix[i] = MV1GetFrameLocalWorldMatrix(charainfo[i].model1, playerSayaFrame[i]);
                MV1SetMatrix(playerSayaModel[i], sayamatrix[i]);
            }

            if (playerWeaponFrame[i] != -1 && (PLAYER_USE_NEW_MODEL || playerWeaponModel[i] != -1)) {
                wpmatrix[i] = MV1GetFrameLocalWorldMatrix(charainfo[i].model1, playerWeaponFrame[i]);
                if (playerWeaponModel[i] != -1) MV1SetMatrix(playerWeaponModel[i], wpmatrix[i]);

                // ★ 攻撃中のみ当たり判定を行う
                if (charainfo[i].mode == ATTACK && !playerStates[i].isSpecialAttack && (!playerStates[i].isHeavyAttack || !charainfo[i].isHit)) {
                    wpPosStart[i] = VTransform(VGet(0.0f, 0.0f, 0.0f), wpmatrix[i]);
                    wpPosEnd[i] = VTransform(VGet(0.0f, -90.0f, 0.0f), wpmatrix[i]);

                    // 1. 相手プレイヤーへの妨害当たり判定
                    int opp = 1 - i;
                    if (charainfo[opp].mode != NONE && charainfo[opp].mode != DOWNMODE) {
                        CheckAttackHit(game, charainfo, &charainfo[i], &charainfo[opp], wpPosStart[i], wpPosEnd[i], SEdamageHandle, anim_damage);
                    }

                    // 2. 至近距離のエネミーだけを対象に精密当たり判定（軽量化）
                    const float HIT_RANGE_SQ = 180.0f * 180.0f;
                    for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; e++) {
					// 生成されていない敵（NONE）はスキップ
					if (playerStates[i].isSpecialAttack || charainfo[e].mode == NONE) continue;
					CheckAttackHit(game, charainfo, &charainfo[i], &charainfo[e], wpPosStart[i], wpPosEnd[i], SEdamageHandle, anim_damage,
                        playerStates[i].isHeavyAttack ? nullptr : playerStates[i].normalHitTargets,
                        !playerStates[i].isHeavyAttack && playerStates[i].attackIndex == PLAYER_ATTACK_ANIM_COUNT - 1
                            ? PLAYER_COMBO_FINISHER_KNOCKBACK_SPEED : 0.0f);
                }
            }
            }

            if (playerStates[i].isSpecialAttack) {
                for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; e++) {
                    if (charainfo[e].mode == NONE || charainfo[e].mode == DOWNMODE) continue;
                    CheckPlayerSpecialAttackHit(game, charainfo[i], playerStates[i], charainfo[e], SEdamageHandle, anim_damage, i, e);
                }
            }
        }

        MV1SetAttachAnimTime(charainfo[TEST_ENEMY_INDEX].model1, charainfo[TEST_ENEMY_INDEX].attachidx, charainfo[TEST_ENEMY_INDEX].playtime);

		if ((playerStates[PLAYER1_INDEX].key & PAD_INPUT_2) && charainfo[TEST_ENEMY_INDEX].mode == DOWNMODE) {
			charainfo[TEST_ENEMY_INDEX].enemyHP = 6;
			charainfo[TEST_ENEMY_INDEX].mode = STAND;
			SetCharacterAnimation(charainfo[TEST_ENEMY_INDEX], enemy_anim_neutral);
		}


        const LONGLONG effectUpdateStart = GetNowHiPerformanceCount();
        UpdateEffekseer3D();
        effectUpdateMs += (GetNowHiPerformanceCount() - effectUpdateStart) / 1000.0;
        ++effectUpdates;
        skyRot += 0.001f;
        MV1SetRotationXYZ(sky, VGet(0, skyRot, 0));
        game.Update(charainfo, charainfo);
        game.UpdateVisualTimers();
        for (int p = 0; p < PLAYER_COUNT; ++p) if (playerWhiteoutTimer[p] > 0) --playerWhiteoutTimer[p];
        if (game.gameState != 0) break;
        } // fixed simulation steps; render the latest state once
		renderStart = GetNowHiPerformanceCount();
        ClearDrawScreen();
        // Resolve ground once per frame, then reuse it in both viewports.
        MV1_COLL_RESULT_POLY playerShadowGround[PLAYER_COUNT] = {};
        for (int i = 0; i < PLAYER_COUNT; ++i) {
            if (charainfo[i].mode == NONE || charainfo[i].model1 == -1) continue;
            const VECTOR position = charainfo[i].pos;
            playerShadowGround[i] = MV1CollCheck_Line(stagedata, -1,
                VAdd(position, VGet(0, 30, 0)), VAdd(position, VGet(0, -650, 0)));
        }


        DrawBox(0, 0, 900, 600, GetColor(255, 255, 255), true);

        // ==========================================
        // 画面分割描画ループ
        // ==========================================
        for (int pIdx = 0; pIdx < PLAYER_COUNT; pIdx++) {
            int startX = (pIdx == 0) ? 0 : 450;
            int endX = (pIdx == 0) ? 450 : 900;
            SetDrawArea(startX, 0, endX, 600);

            VECTOR pPos = charainfo[pIdx].pos;
            VECTOR cPos, cTarget;

            // ★ 演出中（1.5秒間）はボスカメラ、タイマーが切れたら通常のプレイヤー追従カメラへ自動復帰
            if (!game.ApplyBossCamera(cPos, cTarget, charainfo)) {
                if (isAiming[pIdx]) {
                    float dist = 280.0f;
				float aimPitch = atan2f(180.0f, dist) + orbitCameras[pIdx].pitch - OrbitCamera::DefaultPitch;
                if (aimPitch < 0.15f) aimPitch = 0.15f;
                if (aimPitch > 1.2f) aimPitch = 1.2f;
                float height = dist * tanf(aimPitch);

				cPos = VGet(
					pPos.x - sinf(aimAngle[pIdx]) * dist,
					pPos.y + height,
					pPos.z - cosf(aimAngle[pIdx]) * dist
				);

                    cTarget = VAdd(pPos, VGet(sinf(aimAngle[pIdx]) * 350.0f, 40.0f, cosf(aimAngle[pIdx]) * 350.0f));
                }
                else {
                const auto& camera = orbitCameras[pIdx];
                const float cosine = cosf(camera.yaw), sine = sinf(camera.yaw);
                const float side = pIdx == 0 ? 1.0f : -1.0f;
                cTarget = VAdd(pPos, VGet(side * 300.0f * cosine, 150.0f, -side * 300.0f * sine));
                const float horizontal = sqrtf(440000.0f) * cosf(camera.pitch) / sqrtf(400000.0f);
                const float baseX = -side * 200.0f, baseZ = -600.0f;
                cPos = VAdd(cTarget, VGet((baseX * cosine + baseZ * sine) * horizontal,
                    sqrtf(440000.0f) * sinf(camera.pitch), (-baseX * sine + baseZ * cosine) * horizontal));
                }
            }
            SetCameraPositionAndTargetAndUpVec(cPos, cTarget, VGet(0.0f, 1.0f, 0.0f));

            // ★ 高速視界外カリング付き敵描画
            VECTOR camFwd = VNorm(VSub(cTarget, cPos));
            const float MAX_DRAW_DIST_SQ = 2400.0f * 2400.0f;

            const LONGLONG worldStart = GetNowHiPerformanceCount();
            for (int i = 0; i < MAX_CHARA; i++) {
                if (charainfo[i].mode == NONE || charainfo[i].model1 == -1) continue;

                VECTOR toEnemy = VSub(charainfo[i].pos, cPos);
                float dot = camFwd.x * toEnemy.x + camFwd.y * toEnemy.y + camFwd.z * toEnemy.z;
                if (dot < 10.0f) continue;

                float distSq = toEnemy.x * toEnemy.x + toEnemy.y * toEnemy.y + toEnemy.z * toEnemy.z;
                if (distSq > MAX_DRAW_DIST_SQ) continue;

                VECTOR scrPos = ConvWorldPosToScreenPos(charainfo[i].pos);
                if (scrPos.z < 0.0f || scrPos.z > 1.0f) continue;
                if (scrPos.x < (float)startX - 100.0f || scrPos.x >(float)endX + 100.0f) continue;
                if (scrPos.y < -100.0f || scrPos.y > 700.0f) continue;

                MV1DrawModel(charainfo[i].model1);
            }

            MV1DrawModel(stagedata);
            MV1DrawModel(sky);
            for (int i = 0; i < PLAYER_COUNT; ++i) {
                DrawPlayerGroundShadow(playerShadowGround[i],
                    charainfo[i].pos.y - playerShadowGround[i].HitPosition.y);
            }
            worldDrawMs += (GetNowHiPerformanceCount() - worldStart) / 1000.0;
            ++worldDraws;

            for (int i = 0; i < PLAYER_COUNT; i++) {
                if (charainfo[i].mode == NONE) continue;
                if (RespawnManager::IsBlinkHidden(charainfo[i])) continue;

                if (charainfo[i].model1 != -1) {
                    MV1DrawModel(charainfo[i].model1);
                }
                if (playerWeaponModel[i] != -1) MV1DrawModel(playerWeaponModel[i]);
                if (playerSayaModel[i] != -1)   MV1DrawModel(playerSayaModel[i]);
            }
            SyncEffekseerDrawAreaProjection();
			for (int i = 0; i < PLAYER_COUNT; i++) {
				DrawPlayerHeavyAttackEffect(charainfo[i], playerStates[i], heavyAttackEffectHandle);
			}
            const LONGLONG effectDrawStart = GetNowHiPerformanceCount();
			DrawEffekseer3D();
            effectDrawMs += (GetNowHiPerformanceCount() - effectDrawStart) / 1000.0;
            for (int f = 0; f < MAX_THROWN_FLASH; f++) {
                if (thrownFlashes[f].active && thrownFlashes[f].model != -1) {
                    MV1DrawModel(thrownFlashes[f].model);
                }
            }

            for (int i = 0; i < MAX_DROP_FLASH; i++) {
                if (dropFlashItems[i].active && dropFlashItems[i].model != -1) {
                    MV1DrawModel(dropFlashItems[i].model);
                }
            }

            // ========================================================
            // ★ スタン中の頭上ピヨり星（★）＆ STUN 表示（2画面両対応）
            // ========================================================
            for (int targetP = 0; targetP < PLAYER_COUNT; targetP++) {
                if (playerStunTimer[targetP] > 0) {
                    VECTOR headWorldPos = VAdd(charainfo[targetP].pos, VGet(0.0f, charainfo[targetP].charahitinfo.Height + 25.0f, 0.0f));
                    VECTOR headScreenPos = ConvWorldPosToScreenPos(headWorldPos);

                    if (headScreenPos.z > 0.0f && headScreenPos.z < 1.0f &&
                        headScreenPos.x >= (float)startX && headScreenPos.x <= (float)endX)
                    {
                        int hx = (int)headScreenPos.x;
                        int hy = (int)headScreenPos.y;

                        float angleBase = GetNowCount() * 0.008f;
                        for (int s = 0; s < 3; s++) {
                            float a = angleBase + (DX_PI_F * 2.0f / 3.0f) * s;
                            int starX = hx + (int)(cosf(a) * 26.0f);
                            int starY = hy + (int)(sinf(a) * 9.0f);

                            DrawCircle(starX, starY, 5, GetColor(255, 230, 40), TRUE);
                            DrawCircle(starX, starY, 6, GetColor(255, 120, 0), FALSE);
                        }

                        SetFontSize(16);
                        int textW = GetDrawStringWidth("STUN!", 5);
                        DrawString(hx - textW / 2, hy - 32, "STUN!", GetColor(255, 240, 50));
                        SetFontSize(20);
                    }
                }
            }

            // 個別UI
            game.DrawUI(
                pIdx,
                900,
                600,
                charainfo,
                hpBarTex,
                crownGraphHandle,
                pvpIconHandle,
                isPlayerInterferenceActive[pIdx],
                playerInterferenceCooldown[pIdx]
            );
            game.DrawSpecialGauge(pIdx, startX, endX, 600, spGauge[pIdx], MAX_SP_GAUGE, spFrameHandle);
            game.DrawBossCutsceneUI(startX, endX, 600);

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

            if (playerWhiteoutTimer[pIdx] > 0) {

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
		game.DrawTimer(900, 600);
        if (shadowSmoke && smokeFrames == 60) {
            SaveDrawScreenToPNG(0, 0, 900, 600, "..\\work\\shadow-preview.png");
            FILE* shadowReport = nullptr;
            fopen_s(&shadowReport, "..\\work\\shadow-report.txt", "w");
            for (int i = 0; shadowReport && i < PLAYER_COUNT; ++i) {
                const auto& hit = playerShadowGround[i];
                fprintf(shadowReport, "player=%d hit=%d normalY=%f playerY=%f groundY=%f\n",
                    i, hit.HitFlag, hit.Normal.y, charainfo[i].pos.y, hit.HitPosition.y);
            }
            if (shadowReport) fclose(shadowReport);
        }
        if (performanceSmoke && strstr(lpC, "--performance-smoke-slow")) WaitTimer(33);
        presentStart = GetNowHiPerformanceCount();
        ScreenFlip();
        int profileEnemies = 0;
        for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) if (charainfo[e].mode != NONE) ++profileEnemies;
        runtimeProfile.Frame(frameStart, renderStart, presentStart, GetNowHiPerformanceCount(), logicSteps, profileEnemies, GetEffekseer3DManager()->GetTotalInstanceCount(), DxGetAllocSize());
        if (performanceSmoke) {
            const double now = (double)GetNowHiPerformanceCount();
            auto& cost = frameCost[playerStates[0].isSpecialAttackEffectPlaying || playerStates[1].isSpecialAttackEffectPlaying ? 1 : 0];
            ++cost.count;
            cost.update += (renderStart - frameStart) / 1000.0;
            cost.render += (presentStart - renderStart) / 1000.0;
            cost.present += (now - presentStart) / 1000.0;
            const double total = (now - frameStart) / 1000.0;
            if (total > cost.maximum) cost.maximum = total;
        }
	}
    for (auto& item : dropFlashItems) {
        if (item.model >= 0) MV1DeleteModel(item.model);
        item.model = -1;
    }
    if (performanceSmoke) {
        FILE* perf = nullptr; fopen_s(&perf, "performance-smoke.txt", "w");
        for (int phase = 0; perf && phase < 2; ++phase) {
            auto& cost = frameCost[phase];
            const int n = cost.count ? cost.count : 1;
            fprintf(perf, "%s frames=%d update=%.3fms render=%.3fms present=%.3fms max=%.3fms\n", phase ? "special" : "normal", cost.count, cost.update/n, cost.render/n, cost.present/n, cost.maximum);
        }
        if (perf) fprintf(perf, "effect update/tick=%.3fms effect draw/view=%.3fms world draw/view=%.3fms\n", effectUpdateMs / (effectUpdates ? effectUpdates : 1), effectDrawMs / (worldDraws ? worldDraws : 1), worldDrawMs / (worldDraws ? worldDraws : 1));
        if (perf) { fprintf(perf, "simulation ticks=%d remaining=%d wall=%.3fs\n", simulationTicks, game.mainTimer, (GetNowHiPerformanceCount() - simulationWallStart) / 1000000.0); fclose(perf); }
    }

    if (normalAttackEffectResourceHandle != -1) DeleteEffekseerEffect(normalAttackEffectResourceHandle);
    for (int i = 0; i < PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT; i++) {
		if (heavyAttackEffectHandle[i] != -1) {
			DeleteGraph(heavyAttackEffectHandle[i]);
		}
	}
	for (int effectIndex = 0; effectIndex < PLAYER_COUNT; ++effectIndex) if (specialAttackEffectResourceHandle[effectIndex] != -1) {
		DeleteEffekseerEffect(specialAttackEffectResourceHandle[effectIndex]);
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
        const bool passed = mergeAimPassed && mergeThrowPassed && mergeStunPassed && redSpawned && timerPassed && grounded && activeSpawned >= 15 && smokeLongRangeMoves > 0;
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

void CheckAttackHit(GameManager& game, SCharaInfo* charainfo, SCharaInfo* attacker, SCharaInfo* target, VECTOR start, VECTOR end, int SEdamageHandle, int anim_damage, bool* hitTargets, float knockbackSpeed) {
    const int areaTargetIndex = static_cast<int>(target - charainfo);
    if (hitTargets && (areaTargetIndex < 0 || areaTargetIndex >= MAX_CHARA || hitTargets[areaTargetIndex])) return;
    if (attacker->mode == ATTACK && target->mode != DOWNMODE && target->mode != DAMAGE)
    {
        if (!hitTargets && attacker->isHit == true) return;

        int attackerIdx = (attacker == &charainfo[0]) ? 0 : ((attacker == &charainfo[1]) ? 1 : -1);
        int targetIdx = (target == &charainfo[0]) ? 0 : ((target == &charainfo[1]) ? 1 : -1);

        // ★★★ プレイヤー同士の判定 ★★★
        if (attackerIdx != -1 && targetIdx != -1) {
            // Vキーによる妨害攻撃中でない、または相手が無敵時間中はすり抜ける
            if (!isPlayerInterferenceActive[attackerIdx] || target->invincibleTimer > 0) {
                return;
            }

            // 剣を振り下ろしているインパクトの瞬間だけ判定を有効化
            float attackProgress = (attacker->anim_totaltime > 0.0f) ? (attacker->playtime / attacker->anim_totaltime) : 0.0f;
            if (attackProgress < 0.15f || attackProgress > 0.65f) {
                return;
            }
        }

        // 剣のカプセルと対象のカプセル判定
        if (HitCheck_Capsule_Capsule(start, end, 32.0f,
            target->pos,
            VAdd(target->pos, VGet(0, target->charahitinfo.Height, 0)),
            target->charahitinfo.Width / 2 + 25.0f))
        {
            attacker->isHit = true;
            if (hitTargets) hitTargets[areaTargetIndex] = true;
            PlaySoundMem(SEdamageHandle, DX_PLAYTYPE_BACK);

            // ★★★ プレイヤー同士で妨害攻撃が命中した瞬間 ★★★
            if (attackerIdx != -1 && targetIdx != -1) {
                isPlayerInterferenceActive[attackerIdx] = false;
                playerInterferenceCooldown[attackerIdx] = INTERRUPT_COOLDOWN_FRAMES;

                if (target->HP > 0) target->HP--;

                // ★ ノックバックを完全撤廃し、スタンタイマーをセット
                playerStunTimer[targetIdx] = PLAYER_STUN_DURATION_FRAMES;

                // 被弾モーションに切り替えて移動速度を完全ゼロ化
                MV1DetachAnim(target->model1, target->attachidx);
                target->attachidx = MV1AttachAnim(target->model1, 0, (PLAYER_USE_NEW_MODEL) ? playerDamageAnimation : anim_damage);
                target->anim_totaltime = MV1GetAttachAnimTotalTime(target->model1, target->attachidx);
                target->playtime = 0.0f;
                target->mode = DAMAGE;
                target->move = VGet(0.0f, 0.0f, 0.0f);
                ResetMove(*target);

                return;
            }

            // 2. プレイヤーからエネミー（ゴブリン／ゴーレム）への攻撃命中処理
            if (target->enemyHP > 0) {
                target->enemyHP--;
                if (knockbackSpeed > 0.0f && target->enemyHP > 0) StartEnemyKnockback(*attacker, *target, knockbackSpeed);

                int targetIndex = (int)(target - charainfo);
                if (targetIndex != TEST_ENEMY_GOLEM) {
                    VECTOR knockDir = VNorm(VSub(target->pos, attacker->pos));
                    target->pos = VAdd(target->pos, VScale(knockDir, 25.0f));
                    MV1SetPosition(target->model1, target->pos);
                }

                if (target->enemyHP <= 0) {
                    target->mode = NONE;
                    MV1SetVisible(target->model1, FALSE);

                    const bool isFeverTime = (game.mainTimer <= 15 * 60);
                    const int baseScore = (targetIndex == TEST_ENEMY_GOLEM) ? 500 : 100;
                    const int earnedScore = isFeverTime ? (baseScore * 2) : baseScore;

                    if (attackerIdx != -1) {
                        game.AddScore(attackerIdx, earnedScore);
                        game.AddScorePopup(attackerIdx, earnedScore);
                        game.AddKill(attackerIdx);

                        spGauge[attackerIdx] += (targetIndex == TEST_ENEMY_GOLEM) ? 40 : 10;
                        if (spGauge[attackerIdx] > MAX_SP_GAUGE) {
                            spGauge[attackerIdx] = MAX_SP_GAUGE;
                        }
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