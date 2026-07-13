#pragma once

// 全角を半角に直す
#include <DxLib.h>



#define PC_WIDTH 100.0f
#define PC_HEIGHT 180.0f
#define MAX_CHARA 5
#define CHARA_ENUM_DEFAULT_SIZE		500.0f		// 周囲のポリゴン検出に使用する球の初期サイズ
#define CHARA_MAX_HITCOLL			2048		// 処理するコリジョンポリゴンの最大数


#define MOVE_SPEED					3.0f
#define GRAVITY						0.5f






// --- キャラの状態
enum CharaMode
{ // --- キャラの状態
	STAND,
	
};
/* ------------------------------------------------------------------------
|
 構造体宣言
+ -------------------------------------------------------------------------- */
typedef struct
{
	float	Width, Height;
	VECTOR CenterPosition;
} SCharaHitInfo;





struct SCharaInfo
{
	int model1;

	//Direction direction;
	int attachidx;
	float playtime = 0, anim_totaltime;
	VECTOR pos;
	VECTOR move;
	SCharaHitInfo charahitinfo;
	int				mode;				// キャラの状態
	int enemyHP;
	int HP;
	float angle;
	bool isHit;

	float anim_time;

	float anim_total;
};

// キャラクターの当たり判定の情

// 当たり判定の幅、高さ
// 当たり判定の中心座標

extern  int		anim_neutral;

