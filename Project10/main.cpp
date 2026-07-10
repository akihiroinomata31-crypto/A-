// DXライブラリーのインクルード
#include <DxLib.h>
#include <math.h>

#include "main.h"
#include "game.h"




int WINAPI WinMain(HINSTANCE hI, HINSTANCE hP, LPSTR lpC, int nC)
{

	int key = 0;
	int running = 0;
	int rootflm;

	int		anim_neutral;

	
	int		stagedata;
	int sky;
	float skyRot = 0;
	SCharaInfo charainfo[MAX_CHARA];
	
	VECTOR wpPosStart, wpPosEnd;
	int prevJKey = 0;
	int isBGMPlaying = 1;



	VECTOR stagepos = VGet(0.0f, 0.0f, 0.0f);

	VECTOR cpos, ctgt;
	// カメラポジション cpos:カメラ位置　ctgt:カメラ注視点
	cpos = VGet(0.0f, 1000.0f, -800.0f);
	ctgt = VGet(0.0f, 500.0f, 0.0f);


	// ステージコリジョン情報
	MV1_COLL_RESULT_POLY_DIM HitDim;
	int WallNum;
	int FloorNum;										// 床ポリゴンと判断されたポリゴンの数
	MV1_COLL_RESULT_POLY* Wall[CHARA_MAX_HITCOLL];
	MV1_COLL_RESULT_POLY* Floor[CHARA_MAX_HITCOLL];
	int HitFlag = 0;
	MV1_COLL_RESULT_POLY* Poly;
	HITRESULT_LINE LineRes;

	GameManager game; // ここでスコア・時間を管理




	char SEattack_FilePath[] = "swish_00.wav", SEjump_FilePath[] = "jumpIn_00.wav", SEdamage_FilePath[] = "dmg_bySword_00.wav";	// SEファイル名
	int SEattackHandle, SEjumpHandle, SEdamageHandle;						// BGMサウンドハンドル	


//キャラ情報

	charainfo[0].pos = VGet(900.0f, 0.0f, -400.0f);
	charainfo[1].pos = VGet(1000.0f, 0.0f, -400.0f);
	for (int i = 0; i < 2; i++) {
		charainfo[i].charahitinfo.Height = PC_HEIGHT;
		charainfo[i].charahitinfo.Width = PC_WIDTH;
		charainfo[i].charahitinfo.CenterPosition = charainfo[i].pos;
	}

	//charainfo[0].direction = UP;
	charainfo[0].move.x = 0.0f;
	charainfo[0].move.y = 0.0f;
	charainfo[0].move.z = 0.0f;

	charainfo[1].mode = STAND;
	charainfo[1].move.x = 0.0f;
	charainfo[1].move.z = 0.0f;
	charainfo[1].move.y = 0.0f;


	//モデル座標初期セット
	VECTOR pos[2] = { VGet(450.0f, 200.0f, -350.0f),VGet(700.0f, 200.0f, -350.0f) };

	VECTOR cposdistance = VSub(cpos, pos[0]);
	VECTOR ctgtdistance = VSub(ctgt, pos[0]);



	// ウインドウモードの切り替え
	ChangeWindowMode(TRUE);

	// ウインドウサイズの変更
	SetGraphMode(900, 600, 32);

	// DXライブラリの初期化
	if (DxLib_Init() == -1) {
		return -1;
	}



	//モデル読み込み
	charainfo[0].model1 = MV1LoadModel("..\\Data\\Player\\PC.mv1");
	MV1SetPosition(charainfo[0].model1, charainfo[0].pos);
	if (charainfo[1].model1 == -1) {
		printfDx("プレイヤーのモデル読み込み失敗！\n");
	}


	//ルートフレーム
	//プレイヤー
	rootflm = MV1SearchFrame(charainfo[0].model1, "root");
	MV1SetFrameUserLocalMatrix(charainfo[0].model1, rootflm, MGetIdent());




	// ステージ情報の読み込み
	stagedata = MV1LoadModel("..\\Data\\Stage\\Tower_g.mv1");
	if (stagedata == -1) return -1;
	MV1SetPosition(stagedata, stagepos);
	//sky = MV1LoadModel("..\\Data\\Stage\\Stage00_sky.mv1");
	if (sky == -1) return -1;
	MV1SetPosition(sky, VGet(0, -1000, 0));

	// モデル全体のコリジョン情報のセットアップ
	MV1SetupCollInfo(stagedata, -1);

	int MeshNum;

	// モデルに含まれるメッシュの数を取得する
	MeshNum = MV1GetMeshNum(stagedata);



	anim_neutral = MV1LoadModel("..\\Data\\Player\\Anim_Neutral.mv1");
	if (anim_neutral == -1) return -1;

	SetDrawScreen(DX_SCREEN_BACK);
	//カメラの初期化
	SetCameraPositionAndTargetAndUpVec(cpos, ctgt, VGet(0.0f, 1.0f, 0.0f));

	
	while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0) {
	


		//カメラ追従
		ctgt = VAdd(charainfo[0].pos, VGet(0.0f, 0.0f, 0.0f));
		cpos = VAdd(ctgt, VGet(0.0f, 400.0f, -800.0f));
		SetCameraPositionAndTargetAndUpVec(cpos, ctgt, VGet(0.0f, 0.0f, 1.0f));


		// 画面の消去
		ClearDrawScreen();

		// 四角形を表示 最後の引数をfalseにすると塗りつぶし無し
		DrawBox(0, 0, 900, 600, GetColor(255, 255, 255), true);
		//モデル描画
		MV1DrawModel(charainfo[0].model1);
		MV1DrawModel(charainfo[1].model1);
		
		MV1DrawModel(stagedata);
		skyRot += 0.001f;
		MV1SetRotationXYZ(sky, VGet(0, skyRot, 0));
		MV1DrawModel(sky);
		game.Update();

		game.DrawUI();
		// 表画面と裏画面の切り替え
		ScreenFlip();

	}
	// DXライブラリの終了処理
	DxLib_End();

	return 0;
}



