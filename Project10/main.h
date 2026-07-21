#pragma once

// 全角を半角に直す
#include <DxLib.h>

#define PC_WIDTH 80.0f
#define PC_HEIGHT 180.0f
#define MAX_CHARA 6
#define CHARA_ENUM_DEFAULT_SIZE		500.0f		// 周囲のポリゴン検出に使用する球の初期サイズ
#define CHARA_MAX_HITCOLL			2048		// 処理するコリジョンポリゴンの最大数


#define MOVE_SPEED					3.0f
#define GRAVITY						0.5f

#define SOUND_DIRECTORY_PATH "..\\Data\\Sound\\"
#define ATTACK_FIRST_ENDTIME 15.0f
#define ATTACK_SECOND_ENDTIME 15.0f
#define ATTACK_THIERD_ENDTIME 30.0f
class GameManager;
struct SCharaInfo;
enum Direction
{
	DOWN = 0,
	LEFT = 1,
	UP = 2,
	RIGHT = 3
};

// --- キャラの状態
enum CharaMode
{ // --- キャラの状態
	STAND,
	RUN,
	JUMPIN,
	JUMPLOOP,
	JUMPOUT,
	FALL,
	ATTACK,
	ATTACKOUT,
	DAMAGE,
	DOWNMODE,
	NONE,
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

	Direction direction;
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
extern  int		anim_neutral, anim_run, anim_jumpin, anim_jumploop, anim_jumpout, anim_damage, anim_down, enemy_anim_attack, enemy_anim_walk, enemy_anim_neutral;
extern void CheckAttackHit(
	GameManager& game,
	SCharaInfo* charainfo,
	SCharaInfo* attacker,
	SCharaInfo* target,
	VECTOR start,
	VECTOR end,
	int SEdamageHandle,
	int anim_damage
);