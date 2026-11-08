#include "EffekseerForDXLib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "main.h"
#include "game.h"
#include "player.h"
#include "PlayerMotionAssets.h"
#include "GameTiming.h"
#include "RuntimeProfile.h"
#include "OrbitCamera.h"
#include "JustGuard.h"
#include "SprintKnockback.h"
#include "SprintKnockbackChecks.h"
#include "AutoGuardChecks.h"
#include "../DXLib/DxLib.h"

#include "golem.h"






float aimPitchAngle[PLAYER_COUNT] = { 0.25f, 0.25f };
struct RevengeBoostState {
    bool isUsed = false;     // 1試合1回制限
    int timer = 0;           // 残り持続フレーム（60fps * 12秒 = 720）
    bool active = false;     // 現在発動中か
};
RevengeBoostState revengeBoost[PLAYER_COUNT];


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

// Use the existing arena floor triangles, preserving its circular silhouette and collision.
struct ArenaStoneFloor {
    int texture = -1;
    std::vector<VERTEX3D> vertices;
    void Initialize(int stage, VECTOR origin) {
        const auto hit = MV1CollCheck_Line(stage, -1,
            VAdd(origin, VGet(0, 10000, 0)), VAdd(origin, VGet(0, -10000, 0)));
        if (!hit.HitFlag) return;
        texture = LoadGraph("..\\Data\\Stage\\PavingStones036_Color.jpg");
        if (texture == -1) return;
        const auto mesh = MV1GetReferenceMesh(stage, -1, TRUE);
        // One seamless texture tile spans 600 world units, rather than the whole arena.
        constexpr float tileSize = 600.0f;
        for (int i = 0; i < mesh.PolygonNum; ++i) {
            const auto& polygon = mesh.Polygons[i];
            VECTOR p[3];
            bool floor = true;
            for (int j = 0; j < 3; ++j) {
                p[j] = mesh.Vertexs[polygon.VIndex[j]].Position;
                if (fabsf(p[j].y - hit.HitPosition.y) > 1.0f) floor = false;
            }
            if (!floor) continue;
            for (int j = 0; j < 3; ++j) {
                VERTEX3D v = {};
                v.pos = VAdd(p[j], VGet(0, 0.15f, 0));
                v.norm = VGet(0, 1, 0);
                v.dif = GetColorU8(255, 255, 255, 255);
                v.u = (p[j].x - origin.x) / tileSize;
                v.v = (p[j].z - origin.z) / tileSize;
                vertices.push_back(v);
            }
        }
        MV1TerminateReferenceMesh(stage, -1, TRUE);
    }
    ~ArenaStoneFloor() { if (texture != -1) DeleteGraph(texture); }
    void Draw() const {
        if (texture == -1 || vertices.empty()) return;
        const int culling = GetUseBackCulling();
        const int drawMode = GetDrawMode();
        SetUseBackCulling(DX_CULLING_NONE);
        SetUseZBuffer3D(TRUE);
        SetWriteZBuffer3D(TRUE);
        SetTextureAddressMode(DX_TEXADDRESS_WRAP);
        SetDrawMode(DX_DRAWMODE_BILINEAR);
        DrawPolygon3D(vertices.data(), (int)vertices.size() / 3, texture, FALSE);
        SetDrawMode(drawMode);
        SetTextureAddressMode(DX_TEXADDRESS_CLAMP);
        SetWriteZBuffer3D(FALSE);
        SetUseZBuffer3D(FALSE);
        SetUseBackCulling(culling);
    }
};
static int playerDamageAnimation = -1;

struct GameRuntimeCleanup {
    bool dxReady = false;
    bool effectReady = false;
    ~GameRuntimeCleanup() {
        if (effectReady) Effkseer_End();
        if (dxReady) 
            DxLib_End();
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
const int PLAYER_STUN_DURATION_FRAMES = 450; // 約0.75秒
int playerStunTimer[PLAYER_COUNT] = { 0, 0 };
JustGuardState playerJustGuard[PLAYER_COUNT];

static bool CanJustGuard(const SCharaInfo& player, int index) {
    return (player.mode == STAND || player.mode == RUN || player.mode == ATTACK || player.mode == ATTACKOUT) &&
        playerStunTimer[index] <= 0 && player.knockbackFrames <= 0;
}

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
static int player1MoonAttackEffectResourceHandle = -1;

int WINAPI WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lpC, int nC)
{
    GameRuntimeCleanup runtimeCleanup;
    GameManager gameManager;
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
    int animEnergyAttack = -1;
    int animGuard = -1;
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
        { DX_INPUT_PAD1, KEY_INPUT_W, KEY_INPUT_S, KEY_INPUT_A, KEY_INPUT_D, KEY_INPUT_SPACE, KEY_INPUT_E, KEY_INPUT_F, KEY_INPUT_Q, PAD_INPUT_1, PAD_INPUT_3, PAD_INPUT_4, PAD_INPUT_2, KEY_INPUT_LSHIFT },
		// 2P: キーボード(矢印) + 2Pゲームパッド。
		{ DX_INPUT_PAD2, KEY_INPUT_UP, KEY_INPUT_DOWN, KEY_INPUT_LEFT, KEY_INPUT_RIGHT, KEY_INPUT_NUMPAD1, KEY_INPUT_NUMPAD3, KEY_INPUT_NUMPAD0, KEY_INPUT_NUMPAD2, PAD_INPUT_1, PAD_INPUT_3, PAD_INPUT_4, PAD_INPUT_2, KEY_INPUT_RSHIFT }
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
    char NormalAttackEffect_FilePath[] = "MoonFullMoon\\SwordGlowMoonFullMoonFree.efkefc";
    char HeavyAttackEffect_FilePath[] = "重攻撃.png";
    char SpecialAttackEffect_FilePath[] = "swordMega.efkefc";
    int SEattackHandle, SEjumpHandle, SEdamageHandle;
	int specialAttackEffectResourceHandle[PLAYER_COUNT] = { -1, -1 };
    int energyAttackEffectResourceHandle[PLAYER_COUNT] = { -1, -1 };
    bool prevEnergyAttackButton[PLAYER_COUNT] = {};
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
    charainfo[PLAYER1_INDEX].pos = VGet(1300.0f, 600.0f, 100.0f);
    charainfo[PLAYER2_INDEX].pos = VGet(1300.0f, 600.0f, -400.0f);

    for (int i = 0; i < PLAYER_COUNT; i++) {
        charainfo[i].mode = STAND;
        charainfo[i].HP = 15;
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
    SetWindowStyleMode(4);
	// ウインドウサイズの変更
    SetGraphMode(900, 600, 32);
    SetUseDirect3DVersion(DX_DIRECT3D_11);
    // 余計な仮想バッファを作らない
    SetEmulation320x240(FALSE);

    // 画面モードリセット時のグラフィックシステムリセットを抑制（不要な再確保を防ぐ）
    SetChangeScreenModeGraphicsSystemResetFlag(FALSE);
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
   

    MV1SetMaterialDrawAddColorAll(charainfo[PLAYER2_INDEX].model1, 0, 0, 60);

 
    if (charainfo[TEST_ENEMY_GOLEM].model1 != -1) {
        MV1SetVisible(charainfo[TEST_ENEMY_GOLEM].model1, FALSE);
        charainfo[TEST_ENEMY_GOLEM].mode = NONE;
        MV1SetScale(charainfo[TEST_ENEMY_GOLEM].model1, VGet(1.0f, 1.0f, 1.0f));
        InitGolemAnimations(charainfo[TEST_ENEMY_GOLEM]);
    }

    //// ★ 初期化時は -1 にしておく（メモリ消費ゼロ）
    //charainfo[TEST_ENEMY_GOLEM].model1 = -1;
    //charainfo[TEST_ENEMY_GOLEM].mode = NONE;
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
    ArenaStoneFloor arenaStoneFloor;
    arenaStoneFloor.Initialize(stagedata, stagepos);

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

    const int MAX_DROP_FLASH = 7;
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

 
    //てんなん
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
    animEnergyAttack = PLAYER_USE_NEW_MODEL ? LoadPlayerAssetModel(PlayerMotions::files[PlayerMotions::energyAttack]) : anim_attack[2];
    animGuard = LoadPlayerAssetModel(PLAYER_USE_NEW_MODEL ? PlayerMotions::files[PlayerMotions::guard] : "Anim_Guard_In.mv1");
    if (animGuard == -1) return -1;
    if (animHeavyAttack == -1 || animSpecialAttack == -1 || animEnergyAttack == -1) return -1;

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

    const int goblinDamage = MV1LoadModel("..\\Data\\Goblin\\Anim_Damage.mv1");
    const int redGoblinDamage = MV1LoadModel("..\\Data\\RedGoblin\\Anim_Damage.mv1");
    if (goblinDamage < 0 || redGoblinDamage < 0) return -1;
    for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e)
        charainfo[e].enemyDamageAnimation = e >= TEST_ENEMY_RED ? redGoblinDamage : goblinDamage;

    game.enemy_anim_neutral = enemy_anim_neutral;
    game.enemy_anim_walk = enemy_anim_walk;
    for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
        charainfo[e].enemyIdleAnimation = enemy_anim_neutral;
        charainfo[e].enemyWalkAnimation = enemy_anim_walk;
    }

  
    hpBarTex = LoadGraph("..\\Data\\UI\\Hpbar023.png");
    SetTransColor(0, 0, 0);
    int spFrameHandle = LoadGraph("..\\Data\\UI\\Firefly.png");

    
   /* for (int i = 0; i < PLAYER_COUNT; i++) {
        charainfo[i].attachidx = MV1AttachAnim(charainfo[i].model1, 0, anim_neutral);
        charainfo[i].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[i].model1, charainfo[i].attachidx);
    }*/
    charainfo[TEST_ENEMY_INDEX].attachidx = MV1AttachAnim(charainfo[TEST_ENEMY_INDEX].model1, 0, enemy_anim_neutral);
    charainfo[TEST_ENEMY_INDEX].anim_totaltime = MV1GetAttachAnimTotalTime(charainfo[TEST_ENEMY_INDEX].model1, charainfo[TEST_ENEMY_INDEX].attachidx);

    SetDrawScreen(DX_SCREEN_BACK);

    sprintf_s(String, SOUND_DIRECTORY_PATH "BGM\\%s", BGM0_FilePath);
    BGMSoundHandle = LoadSoundMem(String);
    if (BGMSoundHandle != -1) PlaySoundMem(BGMSoundHandle, DX_PLAYTYPE_LOOP);

    int BGMInGameHandle = -1;
    bool isBGMInGameStarted = false; // 再生開始済みフラグ

    // 音源ファイルのロード（"BGM_ingame.wav" や "BGM_ingame.mp3" 等、実際のファイル名に合わせてください）
    BGMInGameHandle = LoadSoundMem("..\\Data\\BGM\\Stage_1.wav");

    sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Weapon\\Sword\\%s", SEattack_FilePath);
    SEattackHandle = LoadSoundMem(String);
    sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Player\\%s", SEdamage_FilePath);
    SEdamageHandle = LoadSoundMem(String);
    sprintf_s(String, SOUND_DIRECTORY_PATH "SE\\Player\\%s", SEjump_FilePath);
    SEjumpHandle = LoadSoundMem(String);

    sprintf_s(String, sizeof(String), "..\\Data\\Effect\\%s", NormalAttackEffect_FilePath);
    normalAttackEffectResourceHandle = LoadEffekseerEffect(String, PLAYER2_MOON_ATTACK_EFFECT_MAGNIFICATION);
    player1MoonAttackEffectResourceHandle = LoadEffekseerEffect(
        "..\\Data\\Effect\\MoonLunarEclipse\\SwordGlowMoonLunarEclipseFree.efkefc", PLAYER1_MOON_ATTACK_EFFECT_MAGNIFICATION);
    if (normalAttackEffectResourceHandle < 0 || player1MoonAttackEffectResourceHandle < 0) return -1;
    for (int i = 0; i < PLAYER_COUNT; i++) playerStates[i].normalAttackEffectResourceHandle = normalAttackEffectResourceHandle;
    playerStates[PLAYER1_INDEX].normalAttackEffectResourceHandle = player1MoonAttackEffectResourceHandle;
    playerStates[PLAYER1_INDEX].normalAttackEffectPlaybackSpeed = PLAYER1_MOON_ATTACK_EFFECT_PLAYBACK_SPEED;
    playerStates[PLAYER2_INDEX].normalAttackEffectPlaybackSpeed = PLAYER2_MOON_ATTACK_EFFECT_PLAYBACK_SPEED;

    sprintf_s(String, sizeof(String), "..\\Data\\Effect\\%s", HeavyAttackEffect_FilePath);
    LoadDivGraph(String, PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT, PLAYER_HEAVY_ATTACK_EFFECT_COLUMN_COUNT, PLAYER_HEAVY_ATTACK_EFFECT_ROW_COUNT, PLAYER_HEAVY_ATTACK_EFFECT_FRAME_WIDTH, PLAYER_HEAVY_ATTACK_EFFECT_FRAME_HEIGHT, heavyAttackEffectHandle);

    sprintf_s(String, sizeof(String), "..\\Data\\Effect\\%s", SpecialAttackEffect_FilePath);
	specialAttackEffectResourceHandle[0] = LoadEffekseerEffect(String, PLAYER_SPECIAL_ATTACK_EFFECT_MAGNIFICATION);
    specialAttackEffectResourceHandle[1] = LoadEffekseerEffect(String, PLAYER_SPECIAL_ATTACK_EFFECT_MAGNIFICATION);
    playerStates[1].bloodLanceSpecial = false;
    for (int p = 0; p < PLAYER_COUNT; ++p) {
        energyAttackEffectResourceHandle[p] = LoadEffekseerEffect("..\\Data\\Effect\\Ember\\Sword_Ember.efkefc", PLAYER_EMBER_EFFECT_SCALE);
        if (energyAttackEffectResourceHandle[p] < 0) return -1;
    }
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
    int SEmenuDecideHandle = LoadSoundMem("..\\Data\\SE\\click.wav");
    int imgCount3 = LoadGraph("..\\Data\\UI\\3.png");
    int imgCount2 = LoadGraph("..\\Data\\UI\\2.png");
    int imgCount1 = LoadGraph("..\\Data\\UI\\1.png");



    auto resetMatch = [&]() {
        if (SEmenuDecideHandle >= 0) PlaySoundMem(SEmenuDecideHandle, DX_PLAYTYPE_BACK);
        game.Init();
        game.GetResultRollTimer(true);
        game.startCountdownTimer = gameSmoke ? 0 : 450;
        for (auto& camera : orbitCameras) camera = OrbitCamera{};
        flashSpawnTimer = 0;
        for (auto& item : dropFlashItems) {
            item.active = false;
            if (item.model >= 0) MV1SetVisible(item.model, FALSE);
        }
        if (BGMInGameHandle != -1) {
            StopSoundMem(BGMInGameHandle);
        }
        isBGMInGameStarted = false;
        for (int p = 0; p < PLAYER_COUNT; ++p) {
            auto old = playerStates[p];
            if (old.guardAnimationAttachment >= 0)
                MV1DetachAnim(charainfo[p].model1, old.guardAnimationAttachment);

            // プレイヤー状態の初期化
            playerStates[p] = PlayerRuntimeState{};
            playerStates[p].bloodLanceSpecial = false;
            playerStates[p].emberSpecial = false;
            prevEnergyAttackButton[p] = false;

            // ★ SPゲージのリセット）
            spGauge[p] = 0;

            // エフェクト設定の保持
            playerStates[p].normalAttackEffectResourceHandle = old.normalAttackEffectResourceHandle;
            playerStates[p].normalAttackEffectPlaybackSpeed = old.normalAttackEffectPlaybackSpeed;
            playerStates[p].motionRootFrame = old.motionRootFrame;
            playerStates[p].motionRootX = old.motionRootX;
            playerStates[p].motionRootZ = old.motionRootZ;

            // キャラクター基本情報のリセット
            charainfo[p].HP = MAX_HP;
            charainfo[p].deaths = 0;
            charainfo[p].mode = STAND;
            charainfo[p].isHit = false;
            charainfo[p].invincibleTimer = 0;
            charainfo[p].knockbackFrames = 0; // ★ i ではなく p に修正
            ResetMove(charainfo[p]);
            SetCharacterAnimation(charainfo[p], anim_neutral);
            MV1SetPosition(charainfo[p].model1, charainfo[p].pos);

            // ★ 1. 閃光弾のリセット（全て p に統一）
            flashStock[p] = MAX_DROP_FLASH;
            isAiming[p] = false;
            prevAiming[p] = false;
            playerWhiteoutTimer[p] = 0;

            // ★ 2. R必殺技・コンバットステートのリセット（全て p に統一）
            CancelPlayerCombat(playerStates[p]); // ★ i ではなく p に修正
            playerStates[p].isSpecialAttack = false;
            playerStates[p].isSpecialAttackEffectPlaying = false;
            playerStates[p].specialAttackPlayingHandle = -1;
            playerStates[p].specialAttackEffectFrame = 0;

            // ★ 3. 入力・スタン等の状態解除（全て p に統一）
            playerStunTimer[p] = 0; // ★ i ではなく p に修正
            playerJustGuard[p] = JustGuardState{};
            revengeBoost[p] = RevengeBoostState{};
            SprintKnockback::Clear(charainfo[p]);

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
    //if (strstr(lpC, "--boss-smoke")) {
    //    resetMatch();
    //    game.mainTimer = 75 * 60 + 1;
    //    game.Update(charainfo, charainfo);
    //    auto& boss = charainfo[TEST_ENEMY_GOLEM];
    //    auto& attacker = charainfo[PLAYER1_INDEX];
    //    const bool spawned = boss.mode == STAND && boss.enemyHP == 20;
    //    const VECTOR hitStart = VAdd(boss.pos, VGet(0, 40, 0));
    //    const VECTOR hitEnd = VAdd(hitStart, VGet(0, 0, 20));
    //    attacker.pos = VAdd(boss.pos, VGet(0, 0, 60));
    //    for (int hit = 0; hit < 20; ++hit) {
    //        boss.mode = STAND; // Advance past the damage reaction between separate swings.
    //        attacker.mode = ATTACK;
    //        attacker.isHit = false;
    //        CheckAttackHit(game, charainfo, &attacker, &boss, hitStart, hitEnd,
    //            -1, boss.enemyIdleAnimation);
    //    }
    //    const bool killed = boss.enemyHP == 0 && boss.mode == NONE &&
    //        game.killCount[0] == 1 && game.p1Score == 500;
    //    for (int tick = 0; tick < 120; ++tick) game.Update(charainfo, charainfo);
    //    const bool staysDead = boss.enemyHP == 0 && boss.mode == NONE &&
    //        !MV1GetVisible(boss.model1);
    //    resetMatch();
    //    game.mainTimer = 75 * 60 + 1;
    //    game.Update(charainfo, charainfo);
    //    const bool nextMatch = boss.mode == STAND && boss.enemyHP == 20 &&
    //        game.IsBossCutsceneActive();
    //    // Also verify that a lethal special attack uses the same no-respawn rule.
    //    boss.enemyHP = PLAYER_SPECIAL_ATTACK_DAMAGE;
    //    attacker.pos = boss.pos;
    //    attacker.direction = Direction::UP;
    //    auto& state = playerStates[0];
    //    state.isSpecialAttack = true;
    //    state.isSpecialAttackEffectPlaying = true;
    //    state.specialAttackEffectFrame = PLAYER_SPECIAL_ATTACK_FULL_RADIUS_FRAME;
    //    CheckPlayerSpecialAttackHit(game, attacker, state, boss, -1,
    //        boss.enemyIdleAnimation, 0, TEST_ENEMY_GOLEM);
    //    const bool specialKilled = boss.enemyHP == 0 && boss.mode == NONE;
    //    for (int tick = 0; tick < 120; ++tick) game.Update(charainfo, charainfo);
    //    const bool specialStaysDead = boss.enemyHP == 0 && boss.mode == NONE;
    //    const bool passed = spawned && killed && staysDead && nextMatch &&
    //        specialKilled && specialStaysDead;
    //   
    //}
    while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0 && (!gameSmoke || (performanceSmoke ? simulationTicks < (longPerformance ? 1800 : 720) : smokeFrames++ < (uiSmoke ? 180 : (deathSmoke ? 1800 : (mergeSmoke ? 520 : 360)))))) {
        if (uiSmoke && screen == Screen::Playing && smokeFrames == 60) game.mainTimer = 1;
        if (screen == Screen::Playing && game.gameState != 0) {
            screen = Screen::Results; // ★ここで完全にリザルト画面の状態へ強制変更！
            menuSelection = 0;        
                prevMenuConfirm = true;
            uiResults = true;
            game.GetResultRollTimer(true);
        }
        if (screen == Screen::Menu || screen == Screen::Results) {

            // フラグや入力チェックを行うかどうかの判定用フラグ
            bool canAcceptInput = true;

            // リザルト画面で、まだ演出タイマーが 600（約10秒）未満の場合は入力を受け付けない
            if (screen == Screen::Results) {
                int& s_rollTimer = game.GetResultRollTimer(false);
                if (s_rollTimer < 600) {
                    canAcceptInput = false; // まだ入力させない
                }
            }

            // 入力OKのときだけ、これからのキー判定や決定処理を行う
            if (canAcceptInput) {
                int pad = ReadPlayerPadState(DX_INPUT_PAD1) | ReadPlayerPadState(DX_INPUT_PAD2);
                bool confirm = CheckHitKey(KEY_INPUT_RETURN) || (pad & PAD_INPUT_1);
                bool direction = CheckHitKey(KEY_INPUT_UP) || CheckHitKey(KEY_INPUT_DOWN) || (pad & (PAD_INPUT_UP | PAD_INPUT_DOWN));
                if (uiSmoke) confirm = smokeFrames == 10 || smokeFrames == 90 || smokeFrames == 120;
                if (screen == Screen::Results && direction && !prevMenuDirection) menuSelection = 1 - menuSelection;
                if (screen == Screen::Menu) menuSelection = 0;

                if (confirm && !prevMenuConfirm) {
                    if (menuSelection == 1) break; // 終了
                    if (screen == Screen::Results) {
                        screen = Screen::Menu; uiReturned = true;
                    }
                    else {
                        // 「ゲーム開始」を押したらロード画面へ
                        resetMatch();
                        if (uiStarted) uiRestarted = true;
                        uiStarted = true;

                        const int LOAD_SECONDS = 6;
                        loadingTimer = LOAD_SECONDS * 60;

                        screen = Screen::Loading;
                    }
                }
                prevMenuConfirm = confirm;
                prevMenuDirection = direction;
            }

            logicClock.Reset(GetNowHiPerformanceCount());
            runtimeProfile.Reset(GetNowHiPerformanceCount());
            if (screen != Screen::Playing) {
                game.DrawGameMenu(screen == Screen::Results, game, menuSelection, uiSmoke && smokeFrames == 1 ? TEXT("..\\.merge-review\\start-menu.png") : uiSmoke && smokeFrames == 65 ? TEXT("..\\.merge-review\\results.png") : nullptr);
                WaitTimer(16);
            }
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


        //// カウントダウン減算部
        //if (game.startCountdownTimer > 0) {
        //    --game.startCountdownTimer;
        //}

        //// ★ カウントダウンが終了し、試合時間（mainTimer）が残っている間だけ再生
        //if (game.startCountdownTimer <= 0 && game.mainTimer > 0) {
        //    if (!isBGMInGameStarted && BGMInGameHandle != -1) {
        //        // カウント終了直後の最初のフレームで再生開始
        //        PlaySoundMem(BGMInGameHandle, DX_PLAYTYPE_LOOP);
        //        isBGMInGameStarted = true;
        //    }
        //}

        //// ★ 試合時間が0（タイマー終了）になったらBGMを停止
        //if (game.mainTimer <= 0 && isBGMInGameStarted) {
        //    if (BGMInGameHandle != -1) {
        //        StopSoundMem(BGMInGameHandle);
        //    }
        //    isBGMInGameStarted = false;
        //}


		frameStart = GetNowHiPerformanceCount();
        const int logicSteps = gameSmoke && !performanceSmoke ? 1 : logicClock.Advance(frameStart);
        if (logicSteps == 0) { WaitTimer(1); continue; }
        for (int logicStep = 0; logicStep < logicSteps; ++logicStep) {
            //++simulationTicks;
            //if (performanceSmoke && simulationTicks % 60 == 0) {
            //    FILE* memory = nullptr;
            //    fopen_s(&memory, "performance-memory.csv", simulationTicks == 60 ? "w" : "a");
            //    if (memory) {
            //        int models = 0, active = 0;
            //        for (int e = TEST_ENEMY_INDEX; e < MAX_CHARA; ++e) {
            //            if (charainfo[e].model1 >= 0) ++models;
            //            if (charainfo[e].mode != NONE) ++active;
            //        }
            //        if (simulationTicks == 60) fprintf(memory, "tick,dxBytes,dxAllocations,enemyModels,activeEnemies,effectInstances\n");
            //        fprintf(memory, "%d,%zu,%d,%d,%d,%d\n", simulationTicks, DxGetAllocSize(), DxGetAllocNum(), models, active, GetEffekseer3DManager()->GetTotalInstanceCount());
            //        fclose(memory);
            //    }
            //}
            //if (performanceSmoke && simulationTicks == 30) {
            //    for (int e = 0; e < 80; ++e) game.ActivateEnemy(charainfo, 800.0f + GetRand(1000), -650.0f + GetRand(1000));
            //}
            //if (performanceSmoke && !strstr(lpC, "--no-special") && simulationTicks >= 120 && (simulationTicks - 120) % 300 == 0) {
            //    for (int p = 0; p < PLAYER_COUNT; ++p) {
            //        charainfo[p].HP = MAX_HP;
            //        PlayPlayerSpecialAttack(charainfo[p], playerStates[p], animSpecialAttack, -1, specialAttackEffectResourceHandle[p]);
            //    }
            //}


            //ブースト処理
            if (deathSmoke && smokeFrames == 100) { charainfo[0].HP = 0; charainfo[1].HP = 0; }
            int remainSec = game.mainTimer / 60;
            for (int p = 0; p < PLAYER_COUNT; p++) {
                int opp = 1 - p;
                int myScore = (p == 0) ? game.p1Score : game.p2Score;
                int oppScore = (p == 0) ? game.p2Score : game.p1Score;

                if (!revengeBoost[p].isUsed && remainSec <= 75 && remainSec > 0 && game.gameState == 0) {
                    if (oppScore - myScore >= 3000) {
                        revengeBoost[p].active = true;
                        revengeBoost[p].isUsed = true;
                        revengeBoost[p].timer = 60 * 32; // 12秒間
                    }
                }

                // ブースト処理の更新ループ内（main.cpp の中など）
                if (revengeBoost[p].active) {
                    revengeBoost[p].timer--;

                    // 残り15秒以下かどうか
                    bool isFeverTime = (game.mainTimer <= 15 * 60);

                    if (myScore >= oppScore || revengeBoost[p].timer <= 0) {
                        revengeBoost[p].active = false;
                        if (charainfo[p].model1 != -1) {
                            if (p == PLAYER2_INDEX) MV1SetMaterialDrawAddColorAll(charainfo[p].model1, 0, 0, 60);
                            else                    MV1SetMaterialDrawAddColorAll(charainfo[p].model1, 0, 0, 0);
                        }
                    }
                    else {
                        // オーラのカラー設定
                        if (charainfo[p].model1 != -1) {
                            if (isFeverTime) {
                                // ★【鬼化モード】：複雑な点滅なしで「シンプルにまっすぐ赤くする！」
                                // 引数は (Red, Green, Blue) です。赤を最大(255)、緑と青を 0 にします。
                                MV1SetMaterialDrawAddColorAll(charainfo[p].model1, 255, 0, 0);
                            }
                            else {
                                MV1SetMaterialDrawAddColorAll(charainfo[p].model1, 255, 120, 0);
                            }
                        }
                    }
                }
            }

        
        int currentJKey = CheckHitKey(KEY_INPUT_J);
      //  RespawnManager::Update(charainfo);
        for (int i = 0; i < PLAYER_COUNT; i++) {
            UpdatePlayerAnimationProgress(charainfo[i], playerStates[i], anim_neutral);
        }

        //死ぬアニメーション処理
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
        // ダウンからの復活入力
        for (int i = 0; i < PLAYER_COUNT; i++) {
            if (charainfo[i].mode == DOWNMODE) {
                int pad = ReadPlayerPadState(playerInputs[i].padType);
                bool respawnInput = (CheckHitKey(KEY_INPUT_Q) != 0) ||
                    (CheckHitKey(playerInputs[i].attackKey) != 0) ||
                    ((pad & playerInputs[i].attackPadButton) != 0) ||
                    ((pad & PAD_INPUT_1) != 0) ||
                    ((pad & PAD_INPUT_2) != 0);

                if (respawnInput) {
                    charainfo[i].HP = 15;
                    charainfo[i].mode = STAND;
                    charainfo[i].playtime = 0.0f;
                    charainfo[i].move = VGet(0.0f, 0.0f, 0.0f);
                    ResetMove(charainfo[i]);
                    // ★ 1. 必殺技（R技）の状態を完全に解除
                    CancelPlayerCombat(playerStates[i]);
                    playerStates[i].isSpecialAttack = false;
                    playerStates[i].isSpecialAttackEffectPlaying = false;
                    playerStates[i].specialAttackPlayingHandle = -1;
                    playerStates[i].specialAttackEffectFrame = 0;
                    playerStates[i].isHeavyAttack = false;
                    playerStates[i].isHeavyAttackEffectPlaying = false;

                    // ★ 2. 閃光弾の状態を解除
                    isAiming[i] = false;
                    //aimingTimer[i] = 0;

                    // ★ 3. 入力ロック・スタン・ノックバックの解除
                    playerStates[i].movementAnimation = -1;
                    playerStates[i].isAttackBuffered = false;
                    playerStunTimer[i] = 0;
                    charainfo[i].knockbackFrames = 0;
                    charainfo[i].invincibleTimer = 30;
                    float respawnX = 1300.0f;
                    float respawnZ = (i == 0) ? 100.0f : -400.0f;
                    float groundY = 0.15f;

                    if (stagedata >= 0) {
                        MV1_COLL_RESULT_POLY hit = MV1CollCheck_Line(stagedata, -1,
                            VGet(respawnX, 2000.0f, respawnZ),
                            VGet(respawnX, -1000.0f, respawnZ));
                        if (hit.HitFlag) groundY = hit.HitPosition.y;
                    }

                    charainfo[i].pos = VGet(respawnX, groundY, respawnZ);
                    charainfo[i].charahitinfo.CenterPosition = charainfo[i].pos;

                    SetCharacterAnimation(charainfo[i], anim_neutral, 0.0f);

                    if (charainfo[i].model1 >= 0) {
                        MV1SetPosition(charainfo[i].model1, charainfo[i].pos);
                        MV1SetRotationXYZ(charainfo[i].model1, VGet(0.0f, DX_PI_F * 0.5f * charainfo[i].direction, 0.0f));
                        if (charainfo[i].attachidx >= 0) {
                            MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, 0.0f);
                        }
                    }

                    charainfo[i].invincibleTimer = 120;
                    playerStates[i].movementAnimation = -1;
                }
            }
        }

        // 場外落下死処理
        for (int i = 0; i < PLAYER_COUNT; i++) {
            const float FALL_DEAD_Y = -150.0f;

            if (charainfo[i].pos.y < FALL_DEAD_Y && charainfo[i].mode != NONE) {
                game.AddDeath(i);
                CancelPlayerCombat(playerStates[i]);

                charainfo[i].HP = 15;
                charainfo[i].mode = STAND;
                charainfo[i].playtime = 0.0f;
                charainfo[i].move = VGet(0.0f, 0.0f, 0.0f);
                ResetMove(charainfo[i]);

                float respawnX = 1300.0f;
                float respawnZ = (i == 0) ? 100.0f : -400.0f;
                float groundY = 0.15f;
                // ★ 1. 必殺技（R技）の状態を完全に解除
                CancelPlayerCombat(playerStates[i]);
                playerStates[i].isSpecialAttack = false;
                playerStates[i].isSpecialAttackEffectPlaying = false;
                playerStates[i].specialAttackPlayingHandle = -1;
                playerStates[i].specialAttackEffectFrame = 0;
                playerStates[i].isHeavyAttack = false;
                playerStates[i].isHeavyAttackEffectPlaying = false;

                // ★ 2. 閃光弾の状態を解除
                isAiming[i] = false;
                //aimingTimer[i] = 0;

                // ★ 3. 入力ロック・スタン・ノックバックの解除
                playerStates[i].movementAnimation = -1;
                playerStates[i].isAttackBuffered = false;
                playerStunTimer[i] = 0;
                charainfo[i].knockbackFrames = 0;
                charainfo[i].invincibleTimer = 30;
                if (stagedata >= 0) {
                    MV1_COLL_RESULT_POLY hit = MV1CollCheck_Line(stagedata, -1,
                        VGet(respawnX, 2000.0f, respawnZ),
                        VGet(respawnX, -1000.0f, respawnZ));
                    if (hit.HitFlag) groundY = hit.HitPosition.y;
                }

                charainfo[i].pos = VGet(respawnX, groundY, respawnZ);
                charainfo[i].charahitinfo.CenterPosition = charainfo[i].pos;

                SetCharacterAnimation(charainfo[i], anim_neutral, 0.0f);

                if (charainfo[i].model1 >= 0) {
                    MV1SetPosition(charainfo[i].model1, charainfo[i].pos);
                    MV1SetRotationXYZ(charainfo[i].model1, VGet(0.0f, DX_PI_F * 0.5f * charainfo[i].direction, 0.0f));
                    if (charainfo[i].attachidx >= 0) {
                        MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, 0.0f);
                    }
                }

                charainfo[i].invincibleTimer = 120;
                playerStates[i].movementAnimation = -1;
            }
        }

       

			// ゲージ満タンかつボタンが押され、現在攻撃中やダウン中でない場合
for (int p = 0; p < PLAYER_COUNT; ++p) playerStates[p].key = ReadPlayerPadState(playerInputs[p].padType);
        for (int p = 0; p < PLAYER_COUNT; ++p) {
            XINPUT_STATE guardPad = {};
            const bool rt = GetJoypadXInputState(playerInputs[p].padType, &guardPad) == 0 &&
                guardPad.RightTrigger >= 30;
            const bool guardDown = rt || CheckHitKey(p == 0 ? KEY_INPUT_T : KEY_INPUT_NUMPAD6) != 0;
            playerJustGuard[p].Update(guardDown, CanJustGuard(charainfo[p], p));
        }
		for (int p = 0; p < PLAYER_COUNT; p++) {
                int rightX = 0, rightY = 0;
                GetJoypadAnalogInputRight(&rightX, &rightY, playerInputs[p].padType);
                orbitCameras[p].Update(rightX, rightY);

                bool currentAimInput = false;
                if (p == 0) {
                    currentAimInput = (CheckHitKey(KEY_INPUT_G) == 1) || ((playerStates[p].key & PAD_INPUT_5) != 0);
                }
                else {
                    currentAimInput = (CheckHitKey(KEY_INPUT_DECIMAL) == 1) || ((playerStates[p].key & PAD_INPUT_5) != 0);
                }

                if (mergeSmoke && p == 0) currentAimInput = smokeFrames >= 20 && smokeFrames < 30;
                bool canAim = (flashStock[p] > 0 &&
                    charainfo[p].mode != DAMAGE &&
                    charainfo[p].mode != DOWNMODE &&
                    playerStunTimer[p] <= 0 && charainfo[p].knockbackFrames <= 0 &&
                    !game.IsBossCutsceneActive());

                // 構え開始
                if (currentAimInput && !prevAiming[p] && canAim) {
                    isAiming[p] = true;
                    aimPitchAngle[p] = 0.25f;
                    aimAngle[p] = orbitCameras[p].yaw;
                }
                // 構え中：画角ズームなしで、右スティック/キーで上下左右にマーカーを動かす
                else if (currentAimInput && isAiming[p]) {
                    if (!canAim) {
                        isAiming[p] = false;
                    }
                    else {
                        // プレイヤーの向きをカメラに同期
                        aimAngle[p] = orbitCameras[p].yaw;
                        charainfo[p].dir = aimAngle[p];
                        if (charainfo[p].model1 != -1) {
                            MV1SetRotationXYZ(charainfo[p].model1, VGet(0.0f, aimAngle[p] + DX_PI_F, 0.0f));
                        }

                        // マーカー（上下ピッチ角）の調整
                        const float PITCH_SPEED = 0.04f;
                        if (p == 0) {
                            if (CheckHitKey(KEY_INPUT_I) || CheckHitKey(KEY_INPUT_UP))   aimPitchAngle[p] += PITCH_SPEED;
                            if (CheckHitKey(KEY_INPUT_K) || CheckHitKey(KEY_INPUT_DOWN)) aimPitchAngle[p] -= PITCH_SPEED;
                        }
                        else {
                            if (CheckHitKey(KEY_INPUT_NUMPAD8)) aimPitchAngle[p] += PITCH_SPEED;
                            if (CheckHitKey(KEY_INPUT_NUMPAD2)) aimPitchAngle[p] -= PITCH_SPEED;
                        }

                        if (abs(rightY) > 300) {
                            aimPitchAngle[p] -= (rightY / 1000.0f) * 0.04f;
                        }

                        if (aimPitchAngle[p] > 1.25f)  aimPitchAngle[p] = 1.25f;
                        if (aimPitchAngle[p] < -0.60f) aimPitchAngle[p] = -0.60f; // 下向き叩きつけ用
                    }
                }
                // 投擲（ボタンを離した瞬間）
                else if (!currentAimInput && isAiming[p]) {
                    isAiming[p] = false;

                    for (int f = 0; f < MAX_THROWN_FLASH; f++) {
                        if (!thrownFlashes[f].active) {
                            thrownFlashes[f].active = true;
                            thrownFlashes[f].ownerIndex = p;
                            thrownFlashes[f].timer = 100; // ★ 約1秒（60フレーム）後に空中起爆
                            flashStock[p]--;

                            float yaw = aimAngle[p];
                            float pitch = aimPitchAngle[p];

                            float sideOffsetX = cosf(yaw) * 12.0f;
                            float sideOffsetZ = -sinf(yaw) * 12.0f;

                            thrownFlashes[f].pos = VGet(
                                charainfo[p].pos.x + sinf(yaw) * 45.0f + sideOffsetX,
                                charainfo[p].pos.y + 45.0f,
                                charainfo[p].pos.z + cosf(yaw) * 45.0f + sideOffsetZ
                            );

                            // ★ 上向きなら放物線（高初速）、下向きなら地面へ叩きつけ
                            float cosP = cosf(pitch);
                            float sinP = sinf(pitch);
                            float hSpeed = 16.0f * (cosP > 0.15f ? cosP : 0.15f);
                            float vSpeed = (pitch >= 0.0f) ? (4.0f + sinP * 14.0f) : (sinP * 18.0f); // 下向きなら負の初速で叩きつける

                            thrownFlashes[f].move = VGet(
                                sinf(yaw) * hSpeed + charainfo[p].move.x * 0.7f,
                                vSpeed,
                                cosf(yaw) * hSpeed + charainfo[p].move.z * 0.7f
                            );

                            if (thrownFlashes[f].model != -1) {
                                MV1SetPosition(thrownFlashes[f].model, thrownFlashes[f].pos);
                                MV1SetVisible(thrownFlashes[f].model, TRUE);
                            }
                            break;
                        }
                    }
                }
                prevAiming[p] = currentAimInput;
                if (isAiming[p]) ResetMove(charainfo[p]);
            }



		// ==========================================
        // アイテム回収判定
        for (int i = 0; i < MAX_DROP_FLASH; i++) {
            if (!dropFlashItems[i].active) continue;
            for (int p = 0; p < PLAYER_COUNT; p++) {
                if (charainfo[p].mode == DOWNMODE || charainfo[p].mode == NONE) continue;
                if (charainfo[p].invincibleTimer > 0) continue;
                //鬼化状態はスキップ
                bool isFeverTime = (game.mainTimer <= 15 * 60);
                if (revengeBoost[p].active && isFeverTime) {
                    continue; // 拾う処理をスキップして次のプレイヤーへ
                }


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
                    float spawnX = 1300.0f + (float)(GetRand(1600) - 300);
                    float spawnZ = -300.0f + (float)(GetRand(1600) - 300);

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

            if (thrownFlashes[f].pos.y <= 0.15f) {
                thrownFlashes[f].pos.y = 0.15f;
                thrownFlashes[f].move.y = -thrownFlashes[f].move.y * 0.3f;
                thrownFlashes[f].move.x *= 0.6f;
                thrownFlashes[f].move.z *= 0.6f;
            }

            if (thrownFlashes[f].model != -1) {
                MV1SetPosition(thrownFlashes[f].model, thrownFlashes[f].pos);
            }

            if (mergeSmoke && thrownFlashes[f].ownerIndex == 0 && thrownFlashes[f].timer == 1) {
                charainfo[TEST_ENEMY_INDEX].pos = thrownFlashes[f].pos;
                charainfo[TEST_ENEMY_INDEX].mode = STAND;
                charainfo[TEST_ENEMY_INDEX].enemyHP = 6;
            }

            thrownFlashes[f].timer--;
            if (thrownFlashes[f].timer <= 0) {
                thrownFlashes[f].active = false;
                if (thrownFlashes[f].model != -1) {
                    MV1SetVisible(thrownFlashes[f].model, FALSE);
                }

                const float FLASH_RANGE_SQ = 750.0f * 750.0f;

                // 相手プレイヤーの直視判定（投げた本人は食らわない）
                for (int p = 0; p < PLAYER_COUNT; p++) {
                    if (charainfo[p].mode == DOWNMODE || p == thrownFlashes[f].ownerIndex) continue;

                    float toX = thrownFlashes[f].pos.x - charainfo[p].pos.x;
                    float toZ = thrownFlashes[f].pos.z - charainfo[p].pos.z;
                    float pDistSq = toX * toX + toZ * toZ;

                    // ① 爆発範囲内にいるか
                    if (pDistSq < FLASH_RANGE_SQ) {
                        // プレイヤーの正面ベクトル（directionから算出）
                        float fwdX = -sinf(DX_PI_F * 0.5f * charainfo[p].direction);
                        float fwdZ = -cosf(DX_PI_F * 0.5f * charainfo[p].direction);

                        // 内積で「光の方向を向いているか」判定（dot > 0 なら前方を向いている）
                        float dot = fwdX * toX + fwdZ * toZ;

                        if (dot > 0.0f) {
                            // ★ 光を直視していた場合：完全スタン（星ピヨり＆行動不能）
                            playerStunTimer[p] = PLAYER_STUN_DURATION_FRAMES;
                            playerWhiteoutTimer[p] = 120;
                            charainfo[p].mode = STAND;
                            ResetMove(charainfo[p]);
                            CancelPlayerCombat(playerStates[p]);
                        }
                        else {
                            // 背を向けていた場合：スタンはせず、軽い画面のチラつきのみ
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
                const float hurtStep = charainfo[i].hurtRecoveryTicks > 0
                    ? fmaxf(0.0f, charainfo[i].anim_totaltime - charainfo[i].playtime) / charainfo[i].hurtRecoveryTicks
                    : EnemyAnimationStep;
                if ((charainfo[i].hurtRecoveryTicks > 0 && --charainfo[i].hurtRecoveryTicks == 0) ||
                    charainfo[i].playtime >= charainfo[i].anim_totaltime) {
                    charainfo[i].hurtRecoveryTicks = 0;
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

				charainfo[i].playtime += charainfo[i].mode == DAMAGE ? hurtStep : EnemyAnimationStep;
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
                // 攻撃射程内に入ったら攻撃開始
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
                // 攻撃射程より外なら前進移動（歩行）
                else {
                    float realDist = sqrtf(targetDistSq);
                    if (realDist > 0.001f) {
                        VECTOR moveDir = VGet(dir.x / realDist, 0.0f, dir.z / realDist);
                        charainfo[i].pos = VAdd(charainfo[i].pos, VScale(moveDir, 1.8f));
                    }

                    // 歩行アニメーションの適用
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

       
        SprintKnockback::Release sprintReleases[PLAYER_COUNT];
        for (int i = 0; i < PLAYER_COUNT; ++i) {
            const bool held = (playerStates[i].key & playerInputs[i].sprintPadButton) != 0 ||
                CheckHitKey(playerInputs[i].sprintKey) != 0;
            sprintReleases[i] = SprintKnockback::Capture(charainfo[i], playerStates[i], held,
                !game.IsBossCutsceneActive() && !isAiming[i] && playerStunTimer[i] <= 0 &&
                charainfo[i].knockbackFrames <= 0 && !playerJustGuard[i].PoseActive());
        }



        // プレイヤー更新
        for (int i = 0; i < PLAYER_COUNT; i++) {
           
         
            if (game.IsBossCutsceneActive()) {
                ResetMove(charainfo[i]);
                continue;
            }


           
            // スタンタイマー処理
            if (playerStunTimer[i] > 0) {
                playerStunTimer[i]--;
                ResetMove(charainfo[i]);
                CancelPlayerCombat(playerStates[i]);
                isAiming[i] = false;

                if (charainfo[i].attachidx != -1) {
                    float freezeTime = charainfo[i].anim_totaltime * 0.2f;
                    MV1SetAttachAnimTime(charainfo[i].model1, charainfo[i].attachidx, freezeTime);
                }

                float shakeX = sinf(GetNowCount() * 0.09f) * 3.5f;
                MV1SetPosition(charainfo[i].model1, VAdd(charainfo[i].pos, VGet(shakeX, 0.0f, 0.0f)));

                int yellowPulse = 80 + (int)(sinf(GetNowCount() * 0.03f) * 60);
                MV1SetMaterialDrawAddColorAll(charainfo[i].model1, yellowPulse, yellowPulse, 0);

                if (playerStunTimer[i] <= 0) {
                    charainfo[i].mode = STAND;
                    charainfo[i].playtime = 0.0f;
                    SetCharacterAnimation(charainfo[i], anim_neutral);
                    MV1SetPosition(charainfo[i].model1, charainfo[i].pos);

                    if (i == PLAYER2_INDEX) MV1SetMaterialDrawAddColorAll(charainfo[i].model1, 0, 0, 60);
                    else                    MV1SetMaterialDrawAddColorAll(charainfo[i].model1, 0, 0, 0);
                }
                continue;
            }

            if (charainfo[i].mode == DAMAGE || charainfo[i].mode == DOWNMODE) {
                ResetMove(charainfo[i]);
                continue;
            }

            PlayerInputConfig currentInput = playerInputs[i];
            if (mergeSmoke && charainfo[TEST_ENEMY_INDEX].invincibleTimer > 0) mergeStunPassed = true;
            /*if (charainfo[i].invincibleTimer > 0) {
                currentInput.attackKey = -1;
                currentInput.heavyAttackKey = -1;
                currentInput.attackPadButton = 0;
                currentInput.heavyAttackPadButton = 0;
            }*/

            const bool specialWasActive = playerStates[i].isSpecialAttack;
            float speedMultiplier = revengeBoost[i].active ? 1.75f : 1.0f;

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

            if (revengeBoost[i].active && charainfo[i].mode == RUN) {
                charainfo[i].move.x *= speedMultiplier;
                charainfo[i].move.z *= speedMultiplier;
            }

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
        if (currentJKey == 1 && prevJKey == 0) {
            if (isBGMPlaying) { StopSoundMem(BGMSoundHandle); isBGMPlaying = 0; }
            else { PlaySoundMem(BGMSoundHandle, DX_PLAYTYPE_LOOP); isBGMPlaying = 1; }
        }
        prevJKey = currentJKey;


        for (int i = 0; i < PLAYER_COUNT; i++) {
            XINPUT_STATE energyPad = {};
            bool r3 = (ReadPlayerPadState(playerInputs[i].padType) & PAD_INPUT_12) != 0;
            if (GetJoypadXInputState(playerInputs[i].padType, &energyPad) == 0)
                r3 = energyPad.Buttons[XINPUT_BUTTON_RIGHT_THUMB] != 0;
            const bool energyButton = r3 || CheckHitKey(i == 0 ? KEY_INPUT_R : KEY_INPUT_NUMPAD5) != 0;
            const bool energyPressed = energyButton && !prevEnergyAttackButton[i];
            prevEnergyAttackButton[i] = energyButton;
            if (!energyPressed || isAiming[i] || charainfo[i].invincibleTimer > 0 || playerStunTimer[i] > 0 || charainfo[i].knockbackFrames > 0) continue;
            if (TryPlayPlayerEnergyAttack(charainfo[i], playerStates[i], animEnergyAttack, SEattackHandle,
                energyAttackEffectResourceHandle[i], spGauge[i], PLAYER_EMBER_FREE_CAST_TEST ? 0 : MAX_SP_GAUGE)) {
                isPlayerInterferenceActive[i] = false;
            }
        }

        // Commit both releases before resolving hits, avoiding a player-index advantage.
        for (int i = 0; i < PLAYER_COUNT; ++i) {
            sprintReleases[i].active = sprintReleases[i].active && !isAiming[i] &&
                (charainfo[i].mode == STAND || charainfo[i].mode == RUN) &&
                charainfo[i].knockbackFrames <= 0;
        }
        for (int i = 0; i < PLAYER_COUNT; ++i) {
            const int other = 1 - i;
            const auto hit = SprintKnockback::Resolve(sprintReleases[i], i, charainfo, playerStates,
                playerJustGuard, CanJustGuard(charainfo[other], other), playerDamageAnimation);
            if (hit == SprintKnockback::Result::Hit) {
                isAiming[other] = false;
                isPlayerInterferenceActive[other] = false;
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

           // RespawnManager::RespawnFallenPlayer(i, charainfo, playerStates[i], game, anim_neutral);
            if (!game.IsBossCutsceneActive()) SprintKnockback::Step(charainfo[i]);
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
                const float swingProgress = charainfo[i].anim_totaltime > 0.0f
                    ? charainfo[i].playtime / charainfo[i].anim_totaltime : 0.0f;
                const bool swingActive = playerStates[i].isHeavyAttack ||
                    (swingProgress >= 0.15f && swingProgress <= 0.65f);
                if (charainfo[i].mode == ATTACK && swingActive && !playerStates[i].isSpecialAttack && (!playerStates[i].isHeavyAttack || !charainfo[i].isHit)) {
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
        for (int p = 0; p < PLAYER_COUNT; ++p) {
            const bool guardPose = CanJustGuard(charainfo[p], p) && !isAiming[p] &&
                playerJustGuard[p].PoseActive();
            UpdatePlayerGuardAnimation(charainfo[p], playerStates[p], animGuard, guardPose);
        }
        UpdateEffekseer3D();
        effectUpdateMs += (GetNowHiPerformanceCount() - effectUpdateStart) / 1000.0;
        ++effectUpdates;
        skyRot += 0.001f;
        MV1SetRotationXYZ(sky, VGet(0, skyRot, 0));
        if (game.startCountdownTimer > 0) --game.startCountdownTimer;
        if (game.startCountdownTimer <= 90) game.Update(charainfo, charainfo);
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
                {
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
            arenaStoneFloor.Draw();
            MV1DrawModel(sky);
            for (int i = 0; i < PLAYER_COUNT; ++i) {
                DrawPlayerGroundShadow(playerShadowGround[i],
                    charainfo[i].pos.y - playerShadowGround[i].HitPosition.y);
            }
            worldDrawMs += (GetNowHiPerformanceCount() - worldStart) / 1000.0;
            ++worldDraws;

            for (int i = 0; i < PLAYER_COUNT; i++) {
                if (charainfo[i].mode == NONE) continue;
              //  if (RespawnManager::IsBlinkHidden(charainfo[i])) continue;

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

            if (game.gameState == 0) {
                game.DrawUI(
                    pIdx,
                    900,
                    600,
                    charainfo,
                    hpBarTex,
                    isPlayerInterferenceActive[pIdx],
                    playerInterferenceCooldown[pIdx]
                );
                game.DrawSpecialGauge(pIdx, startX, endX, 600, spGauge[pIdx], MAX_SP_GAUGE, spFrameHandle);
               
              
            
         
                // 戦闘中のみカウントダウンとタイマーを描画
                game.DrawStartCountdown(900, 600, imgCount3, imgCount2, imgCount1);
                game.DrawTimer(900, 600);
            }
            else {
                // ★★★ FINISH時：戦闘画面の上に直接中央リザルトを描画 ★★★
                game.DrawGameMenu(true, game, menuSelection, nullptr, 0);
            }

         
            game.DrawSpecialGauge(pIdx, startX, endX, 600, spGauge[pIdx], MAX_SP_GAUGE, spFrameHandle);
           
            game.DrawBossCutsceneUI(startX, endX, 600);
            if (revengeBoost[pIdx].active) {
                int centerX = startX + (endX - startX) / 2;
                int barY = 115;
                int barW = 180;
                int barH = 8;
                float rate = (float)revengeBoost[pIdx].timer / (float)(60 * 12);
                int fillW = (int)(barW * rate);

                SetFontSize(16);
                const char* bText = "REVENGE BOOST!";
                int tw = GetDrawStringWidth(bText, 14);
                int alpha = 180 + (int)(sinf(GetNowCount() * 0.02f) * 75);

                SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
                DrawString(centerX - tw / 2 + 1, barY - 22 + 1, bText, GetColor(0, 0, 0));
                DrawString(centerX - tw / 2, barY - 22, bText, GetColor(255, 120, 40));

                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
                DrawBox(centerX - barW / 2, barY, centerX + barW / 2, barY + barH, GetColor(20, 20, 20), TRUE);

                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                if (fillW > 0) {
                    DrawBox(centerX - barW / 2, barY, centerX - barW / 2 + fillW, barY + barH, GetColor(255, 140, 20), TRUE);
                }
                DrawBox(centerX - barW / 2, barY, centerX + barW / 2, barY + barH, GetColor(255, 220, 100), FALSE);
                SetFontSize(20);
            }

            if (isAiming[pIdx]) {
                float yaw = aimAngle[pIdx];
                float swordFwd = 60.0f;
                float swordSide = 14.0f;
                float swordY = 150.0f;
                float grenadeScale = 2.0f;
                // 剣の目の前（投擲待機位置）の座標
                VECTOR swordTipPos = VGet(
                    pPos.x + sinf(yaw) * swordFwd + cosf(yaw) * swordSide,
                    pPos.y + swordY,
                    pPos.z + cosf(yaw) * swordFwd - sinf(yaw) * swordSide
                );

                // ★★★ 白い球（DrawSphere3D）の代わりに閃光弾モデルを描画 ★★★
                if (grenadeBaseModel != -1) {
                    MV1SetVisible(grenadeBaseModel, TRUE);
                    MV1SetPosition(grenadeBaseModel, swordTipPos);

                    // 構え中に少し回転させて手榴弾を目立たせる演出（ピッチ＆ヨー）
                    float spin = GetNowCount() * 0.003f;
                    MV1SetRotationXYZ(grenadeBaseModel, VGet(aimPitchAngle[pIdx], yaw + spin, 0.0f));

                    // モデルの描画
                    MV1DrawModel(grenadeBaseModel);

                    // 描画後は非表示に戻す（他への影響防止）
                    MV1SetVisible(grenadeBaseModel, FALSE);
                }

                // ----------------------------------------------------
                // 画面中央付近の照準レティクル表示（線と円）
                // ----------------------------------------------------
                int screenMidX = (startX + endX) / 2;
                int markerY = 300 - (int)(aimPitchAngle[pIdx] * 200.0f);

                DrawCircle(screenMidX, markerY, 16, GetColor(255, 255, 255), FALSE);
                DrawCircle(screenMidX, markerY, 3, GetColor(255, 220, 50), TRUE);
                DrawLine(screenMidX - 22, markerY, screenMidX - 6, markerY, GetColor(255, 255, 255));
                DrawLine(screenMidX + 6, markerY, screenMidX + 22, markerY, GetColor(255, 255, 255));
                DrawLine(screenMidX, markerY - 22, screenMidX, markerY - 6, GetColor(255, 255, 255));
                DrawLine(screenMidX, markerY + 6, screenMidX, markerY + 22, GetColor(255, 255, 255));
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
		game.DrawStartCountdown(900, 600, imgCount3, imgCount2, imgCount1);
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
  /*if (performanceSmoke) {
        FILE* perf = nullptr; fopen_s(&perf, "performance-smoke.txt", "w");
        for (int phase = 0; perf && phase < 2; ++phase) {
            auto& cost = frameCost[phase];
            const int n = cost.count ? cost.count : 1;
            fprintf(perf, "%s frames=%d update=%.3fms render=%.3fms present=%.3fms max=%.3fms\n", phase ? "special" : "normal", cost.count, cost.update/n, cost.render/n, cost.present/n, cost.maximum);
        }
        if (perf) fprintf(perf, "effect update/tick=%.3fms effect draw/view=%.3fms world draw/view=%.3fms\n", effectUpdateMs / (effectUpdates ? effectUpdates : 1), effectDrawMs / (worldDraws ? worldDraws : 1), worldDrawMs / (worldDraws ? worldDraws : 1));
        if (perf) { fprintf(perf, "simulation ticks=%d remaining=%d wall=%.3fs\n", simulationTicks, game.mainTimer, (GetNowHiPerformanceCount() - simulationWallStart) / 1000000.0); fclose(perf); }
  }*/

    if (player1MoonAttackEffectResourceHandle != -1) DeleteEffekseerEffect(player1MoonAttackEffectResourceHandle);
    if (normalAttackEffectResourceHandle != -1) DeleteEffekseerEffect(normalAttackEffectResourceHandle);
    for (int i = 0; i < PLAYER_HEAVY_ATTACK_EFFECT_FRAME_COUNT; i++) {
		if (heavyAttackEffectHandle[i] != -1) {
			DeleteGraph(heavyAttackEffectHandle[i]);
		}
	}
    for (int effect : energyAttackEffectResourceHandle) if (effect >= 0) DeleteEffekseerEffect(effect);
	for (int effectIndex = 0; effectIndex < PLAYER_COUNT; ++effectIndex) if (specialAttackEffectResourceHandle[effectIndex] != -1) {
		DeleteEffekseerEffect(specialAttackEffectResourceHandle[effectIndex]);
	}
	/*if (mergeSmoke) {
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
    }*/
    //if (deathSmoke) {
    //    FILE* report = nullptr; fopen_s(&report, "death-smoke.txt", "w");
    //    const bool passed = game.deathCount[0] == 1 && game.deathCount[1] == 1 &&
    //        charainfo[0].mode == DOWNMODE && charainfo[1].mode == DOWNMODE;
    //    if (report) { fprintf(report, "both players dead; death counts=%d,%d; frames=%d; %s\n", game.deathCount[0], game.deathCount[1], smokeFrames, passed ? "PASS" : "FAIL"); fclose(report); }
    //    if (!passed) return 1;
    //}
    //if (uiSmoke) {
    //    FILE* report = nullptr; fopen_s(&report, "ui-smoke.txt", "w");
    //    const bool passed = uiStarted && uiResults && uiReturned && uiRestarted && game.mainTimer > 0 && game.p1Score == 0 && game.p2Score == 0;
    //    if (report) { fprintf(report, "start=%d results=%d menu=%d restart=%d %s\n", uiStarted, uiResults, uiReturned, uiRestarted, passed ? "PASS" : "FAIL"); fclose(report); }
    //    if (!passed) return 1;
    //}
    
    runtimeCleanup.effectReady = false; Effkseer_End();
    runtimeCleanup.dxReady = false; 
    DxLib_End();

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

        if (attackerIdx != -1 && targetIdx != -1) {
            if (!isPlayerInterferenceActive[attackerIdx] || target->invincibleTimer > 0) {
                return;
            }

            float attackProgress = (attacker->anim_totaltime > 0.0f) ? (attacker->playtime / attacker->anim_totaltime) : 0.0f;
            if (attackProgress < 0.15f || attackProgress > 0.65f) {
                return;
            }
        }

        if (HitCheck_Capsule_Capsule(start, end, 32.0f,
            target->pos,
            VAdd(target->pos, VGet(0, target->charahitinfo.Height, 0)),
            target->charahitinfo.Width / 2 + 25.0f))
        {
            attacker->isHit = true;
            if (hitTargets) hitTargets[areaTargetIndex] = true;
            PlaySoundMem(SEdamageHandle, DX_PLAYTYPE_BACK);

            if (attackerIdx != -1 && targetIdx != -1) {
                isPlayerInterferenceActive[attackerIdx] = false;
                playerInterferenceCooldown[attackerIdx] = INTERRUPT_COOLDOWN_FRAMES;

                if (target->HP > 0) target->HP--;

                playerStunTimer[targetIdx] = PLAYER_STUN_DURATION_FRAMES;

                MV1DetachAnim(target->model1, target->attachidx);
                target->attachidx = MV1AttachAnim(target->model1, 0, (PLAYER_USE_NEW_MODEL) ? playerDamageAnimation : anim_damage);
                target->anim_totaltime = MV1GetAttachAnimTotalTime(target->model1, target->attachidx);
                target->playtime = 0.0f;
                target->mode = DAMAGE;
                target->move = VGet(0.0f, 0.0f, 0.0f);
                ResetMove(*target);

                return;
            }

            if (target->enemyHP > 0) {
                int damage = (attackerIdx != -1 && revengeBoost[attackerIdx].active) ? 2 : 1;
                target->enemyHP -= damage;

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
                    //const int baseScore = (targetIndex == TEST_ENEMY_GOLEM) ? 500 : 100;

                    // 基本スコアを計算
                    int earnedScore ;

                    // ① 残り15秒以下（フィーバータイム）なら 2倍
                    if (isFeverTime) {
                        earnedScore *= 2;
                    }

                    // ② バフ状態（リベンジブースト中）ならさらに 2倍！（これで合計4倍に！）
                    if (attackerIdx != -1 && revengeBoost[attackerIdx].active) {
                        earnedScore *= 2;
                    }

                    if (attackerIdx != -1) {
                        game.AddScore(attackerIdx, earnedScore);
                        game.AddScorePopup(attackerIdx, earnedScore); // 画面上のポップアップ数字も跳ね上がります！
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


