/******************************************************************************
	4Col.c
******************************************************************************/
#pragma comment(lib, "user32.lib")
#pragma comment(lib,  "gdi32.lib")
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib,  "winmm.lib")

#define  UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <shlwapi.h>
#include <time.h>

/* デバッグインクルード */
#ifdef DBG
#include "debug.h"
#endif

/* 定数 */
#define D_BandX		(120)			// メニュー幅
#define D_Margin	(24)			// 余白
#define D_Radius	(8)			// ノード半径
#define D_Scale		(SHRT_MAX)		// 生成座標範囲
#define D_MaxNode	(10000)			// 最大ノード数
#define D_MaxMap	(1000000)		// 最大地図数
#define D_TimeOut	(10000)			// タイムアウト(msec)

/* 演算 */
#define di_Round(x)	((int)((x)+((x)<0.0?-0.5:0.5)))	// 四捨五入
#define dx_Key(x)	(GetKeyState(x)<0)		// 併用押下キーチェック
#define dx_Scope(n,x,m)	(((n)<=(x))&&((x)<=(m)))	// 範囲内チェック
#define dpt_Cat(x)	((x)+wcslen(x))			// 文字列終端アドレス

/* ソーティング比較関数定義 */
typedef int (*F_cmp)(const void *,const void *);

/* 構造体定義 */
//------------------------------------- ノード
typedef struct t_node {
	struct t_node* mpz_next;	// 次ポインタ
	struct t_line* mpz_line;	// 接続ライン
	struct t_line* mpz_pine;	// 三国峠接続ライン退避
	struct t_node* mpz1_sea[2];	// 海岸線ループポインタ( [0]後進 [1]前進 )
	struct t_node* mpz_way0;	// 減色周回ルート後進ポインタ
	struct t_node* mpz_way1;	// 減色周回ルート前進ポインタ
	struct t_node* mpz_bank;	// 最終海岸線ループポインタ(NULLで内陸表現)
	struct t_node* mpz_trac;	// トレース枝刈用の後戻ポインタ
	int mi_x;			// 座標X
	int mi_y;			// 座標Y
	int mi_dist;			// 地図中心距離
	short ms_can;			// 海岸線接続数(追加ノード検出用)
	short ms_div;			// トレース枝刈用の枝数カウンタ(-1,0,n～)
	short ms_id;			// ID番号
	char mc_col;			// 色(0,1,2,3,4,5)
	char mc_fan;			// 三つ巴スワップ番号(0～5)
	char mc1_area[2];		// エリアフラグ( 1:後方 -1:前方 0:外 ) [0]接岸ライン [1]結界ルート
	char mc_swap;			// スワップ試行結果
	char mc_done;			// スワップ済フラグ(0,1)
	char mc_draw;			// 描画時のライン２度書き防止フラグ
} T_node;

//------------------------------------- ライン
typedef struct t_line {
	struct t_line* mpz_next;	// 線形次ポインタ
	struct t_line* mpz1_spin[2];	// 周回ループ( [0]反時計回り [1]時計回り )
	struct t_node* mpz_node;	// 接続先ノード
} T_line;

//------------------------------------- スピン回転ソート用
typedef struct t_spin {
	struct t_line* mpz_line;	// 対象ラインポインタ
	long long ml_len;		// ライン長(自乗)
	long long ml_sin;		// ライン回転値(0～4*INT_MAX)
} T_spin;

//------------------------------------- 三角面リスト(地図作成用)
typedef struct t_tang {
	struct t_tang* mpz_next;	// 次ポインタ
	struct t_tang* mpz_back;	// 前ポインタ
	struct t_tang* mpz_list;	// 調査対象リスト
	struct t_tang* mpz1_meet[3];	// 接触三角面( 頂点ノード配列の対角でセット )
	struct t_node* mpz1_node[3];	// 三角面頂点ノード（ [0]主体ノード [1][2]共通辺ノード )
	double md_x;			// 外接円の中心X
	double md_y;			// 外接円の中心Y
	double md_r;			// 外接円の半径
	int mi_x1;			// 外接矩形(最小Ｘ)
	int mi_y1;			// 外接矩形(最小Ｙ)
	int mi_x2;			// 外接矩形(最大Ｘ)
	int mi_y2;			// 外接矩形(最大Ｙ)
} T_tang;

/* スタティック変数 */
static HINSTANCE sx_inst;		// インスタンス
static HDC sx_dc;			// メモリＤＣ
static WORD st1_ini[MAX_PATH];		// 初期化ファイルパス
static WINDOWPLACEMENT sz_wpos;		// ウィンドウ配置構造体
static HWND sx_main;			// メインＷハンドル
static HWND sx_disp;			// 描画Ｗハンドル
static WORD st1_code[16] = {0};		// 乱数コード(文字列)
static WORD st1_node[16] = {0};		// 生成ノード数(文字列)
static WORD st1_maps[16] = {0};		// バッチ地図数(文字列)
static T_node* spz_root = NULL;		// ノードメモリ先頭アドレス
static T_node* spz_node = NULL;		// ノードルート
static T_node* spz_sea  = NULL;		// 海岸線ルート
static T_node* spz_way  = NULL;		// 結界ルート始端
static T_node* spz_waz  = NULL;		// 結界ルート終端
static T_node* spz_pass = NULL;		// 三国峠ルート
static T_node* spz_bank = NULL;		// 最終海岸線ルート
static T_node* spz_add  = NULL;		// 新規追加ノード
static T_node* spz_qeen = NULL;		// 接岸ライン始端
static T_node* spz_king = NULL;		// 接岸ライン終端
static T_node* spz_base = NULL;		// 減色ノード
static T_node* spz_jack = NULL;		// 抜穴ノード
static T_tang* spz_tang = NULL;		// 三角面リストのルート
static T_tang* spz_invs = NULL;		// 調査リストのルート
static char sc1_swap[2];		// スワップ組色(1,2,3,4)
static char sc_lock = 0;		// ルート変更不可フラグ(0,1)
static char sc_fine = 0;		// 最終海岸線処理フラグ(0,1)
static int si_wx;			// 現時点の描画ＷのＸサイズ
static int si_wy;			// 現時点の描画ＷのＹサイズ
static HPEN sx1_pen[7];			// ペン( [0]最終海岸線 [1]逐次海岸線 [2]接岸ライン [3]減色ノード [4]結界ルート [5]抜穴ノード [6]ルート変更不可 )
static HBRUSH sx1_brs[6];		// ブラシ
static HFONT sx_font;			// フォント
static RECT sz1_rect[2];		// バッチ成否カウント表示矩形( [0]成功 [1]失敗 )
static WORD st2_cont[2][16];		// バッチ成否カウント文字列( [0]成功 [1]失敗 )
static WORD st2_info[2][64];		// バッチ諸元情報
static WORD st1_tim0[32] = {0};		// 総処理時間
static WORD st1_tim1[32] = {0};		// 地図生成時間
static WORD st1_tim2[32] = {0};		// 着色作業時間
static char sc_step = 0;		// 処理ステップ(-1,0～)
static char sc_phase = -1;		// 処理フェーズ( -1:起動直後 0:ノーマル 1:バッチ中 2:バッチ処理時間 )

const char cc3_col[3][2][2] = { 2,3, 3,2, 3,1, 1,3, 1,2, 2,1 };	// Baseスワップ
const char cc2_fan[6][2]    = { 1,2, 2,1, 2,3, 3,2, 3,1, 1,3 };	// Base三つ巴

const WORD* cpt1_rep[2]	 = { L"成功＼(^o^)／",L"失敗（´Д`）" };
const WORD* cpt1_item[2] = { L"成功 :",L"失敗 :" };
const WORD* cpt_ver	 = { L"四色問題 (V1.0)" };
const WORD* cpt_cw	 = { L"(C) 2026 加藤 一郎" };
const WORD ct1_col[] = L" RGYB*";

/* プロトタイプ宣言 */
void		fv_Init(HINSTANCE);
LONG CALLBACK	fx_CBmain(HWND,UINT,WPARAM,LPARAM);
LONG CALLBACK	fx_CBdisp(HWND,UINT,WPARAM,LPARAM);

void		fv_EditFocus(char);
void		fv_GrayOut(char);
void		fv_MakeMap(void);
short		fs_Color(char);

char		fc_Step0(void);
char		fc_Step1(void);
char		fc_Step2(void);
char		fc_Step3(void);
char		fc_Step4(void);
char		fc_Step5(void);
void		fv_Step6(void);
char		fc_Step7(void);

void		fv_BaseGard(char,char,char);
char		fc_BaseFix(char);
char		fc_GardChg(T_node*);
void		fv_CutBranch(T_node*);
void		fv_RCbran(T_node*,T_node*);

void		fv_SwapMsk(char,char);
void		fv_RCmask(char,T_node*);
void		fv_SwapFlg(T_node*,char);
void		fv_RCflag(char,T_node*);
void		fv_SwapExe(T_node*,char);
void		fv_RCswap(char,T_node*);

char		fc_GapCheck(T_node*,T_node**);
void		fv_CutPass(void);
void		fv_PutPass(void);
void		fv_SetSpin(void);
int		fi_CmpSpin(T_spin*,T_spin*);

short		fs_FailCheck(void);
void		fv_Batch(void);

void		fv_CreateMap(int);
void		fv_DivTang(T_tang*,T_tang*);
char		fc_CheckTang(T_tang*);
void		fv_FlipTang(T_tang*,char);
T_tang*		fpz_MakeTang(T_node*,T_node*,T_node*);
void		fv_DelTang(T_tang*);
void		fv_SetBank(void);

void		fv_MakeNode(int);
void		fv_MakeLine(T_node*,T_node*);
void		fv_DelLine(T_node*,T_node*);
void		fv_FlipLine(T_node*,T_node*,T_node*,T_node*);
void		fv_FreeMap();
void		fv_Clear(char);

char		fc_Inclusion(T_node*,T_node**);
int		fi_Distance(int,int,int,int);
int		fi_Range(int,int,int);

void		fv_IniLoad(void);
void		fv_IniSave(void);
void		fv_Export(void);
int		fi_CmpID(T_node**,T_node**);

/**----------------------------------------------------------------------------
＠ メインルーチン */
int WINAPI
wWinMain(
HINSTANCE ax_inst,	// <I>自前インスタンス
HINSTANCE ax_prev,	// <I>直前インスタンス
LPWSTR apt_arg,		// <I>引数
int ai_disp)		// <I>表示フラグ
/*
----------------------------------------------------------------------------**/
{
	MSG az_msg;
	HWND ax_win;
	int ai_btn;
	char ac_edit;

	/* デバッグウィンドウ */
	#ifdef DBG
	if ( DBGwin(ax_inst) ) return( 0 );
	#endif

	/* 初期化 */
	fv_Init(ax_inst);

	/* イベント無限ループ */
	while( GetMessage(&az_msg,NULL,0,0) ) {
		if ( az_msg.message == WM_KEYDOWN ) {

			/* フォーカスが存在するダイアログを調査 */
			ac_edit = 0;
			ax_win = GetFocus();
			     if ( ax_win == GetDlgItem(sx_main,1002) ) ac_edit = 1;
			else if ( ax_win == GetDlgItem(sx_main,1004) ) ac_edit = 2;
			else if ( ax_win == GetDlgItem(sx_main,1011) ) ac_edit = 3;

			/* 入力欄にフォーカスが存在する場合 */
			if ( ac_edit ) {
				switch( az_msg.wParam ) {
				    case VK_RETURN: SetFocus(sx_main);     break;
				    case VK_TAB:    fv_EditFocus(ac_edit); break;
				}
			}
			/* 実行ショートカットキー */
			else {
				ai_btn = 0;
				switch( az_msg.wParam ) {
				    case 'F':	    ai_btn = 1001;   break; // FIX
				    case 'A':	    ai_btn = 1003;   break; // AUTO
				    case VK_UP:     ai_btn = 1005;   break; // 地図生成
				    case VK_RIGHT:  ai_btn = 1007;   break; // 着色(逐次)
				    case VK_DOWN:   ai_btn = 1008;   break; // 着色(一括)
				    case VK_LEFT:   ai_btn = 1009;   break; // 着色クリア
				    case VK_TAB:    fv_EditFocus(0); break; // ＴＡＢ
				}
				if ( ai_btn && IsWindowEnabled(GetDlgItem(sx_main,ai_btn)) ) SendMessage(sx_main,WM_COMMAND,ai_btn,0);
			}
		}

		TranslateMessage(&az_msg);
		DispatchMessage(&az_msg);
	}

	return( az_msg.wParam );
}
/**----------------------------------------------------------------------------
＠ 初期化 */
void
fv_Init(
HINSTANCE ax_inst)	// <I>自前インスタンス
/*
----------------------------------------------------------------------------**/
{
	WNDCLASS az_class = {0};

	/* インスタンス確保 */
	sx_inst = ax_inst;

	/* 応答なしの無効化 */
	DisableProcessWindowsGhosting();

	/* UTF-16処理のためのワイド文字ロケール */
	_wsetlocale(0,L"");

	/* メモリＤＣ確保 */
	sx_dc = CreateCompatibleDC(NULL);

	/* 初期化ファイル読込 */
	GetModuleFileName(sx_inst,st1_ini,MAX_PATH);
	wcscpy(PathFindExtension(st1_ini),L".ini");
	fv_IniLoad();

	/* バッチ成否カウント表示矩形 */
	SetRect(&sz1_rect[0],80,70,140, 85);
	SetRect(&sz1_rect[1],80,90,140,105);

	/* ペンの生成 */
	sx1_pen[0] = CreatePen(PS_SOLID,3,RGB(  0,  0,  0)); // 最終海岸線
	sx1_pen[1] = CreatePen(PS_SOLID,7,RGB(224,224,224)); // 逐次海岸線
	sx1_pen[2] = CreatePen(PS_SOLID,7,RGB(255,192,160)); // 接岸ライン
	sx1_pen[3] = CreatePen(PS_SOLID,3,RGB(128,128,128)); // 減色ノード
	sx1_pen[4] = CreatePen(PS_SOLID,7,RGB(255,192,255)); // 結界ルート
	sx1_pen[5] = CreatePen(PS_SOLID,5,RGB(192,192,192)); // 抜穴ノード
	sx1_pen[6] = CreatePen(PS_SOLID,3,RGB(255,  0,  0)); // ルート変更不可

	/* ブラシの生成 */
	sx1_brs[0] = CreateSolidBrush(RGB(255,255,255)); // 白
	sx1_brs[1] = CreateSolidBrush(RGB(255, 96, 96)); // 赤
	sx1_brs[2] = CreateSolidBrush(RGB( 80,240,160)); // 緑
	sx1_brs[3] = CreateSolidBrush(RGB(255,216, 96)); // 黄
	sx1_brs[4] = CreateSolidBrush(RGB( 96,176,255)); // 水
	sx1_brs[5] = CreateSolidBrush(RGB(208,208,208)); // 灰

	/* フォントの生成 */
	sx_font = CreateFont(-12,0,0,0,FW_NORMAL,0,0,0,SHIFTJIS_CHARSET,0,0,0,0,L"Meiryo UI");

	/* メインＷ生成 */
	az_class.hInstance     = sx_inst;
	az_class.lpszClassName = L"CN_4col";
	az_class.lpfnWndProc   = fx_CBmain;
	az_class.hCursor       = LoadCursor(NULL,IDC_ARROW);
	az_class.hbrBackground = GetStockObject(LTGRAY_BRUSH);
	az_class.hIcon         = LoadIcon(sx_inst,L"#1");
	RegisterClass(&az_class);
	sx_main = CreateWindow(L"CN_4col",cpt_ver,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,8,16,0,0,NULL,0,sx_inst,NULL);
	SetWindowPlacement(sx_main,&sz_wpos);
	ShowWindow(sx_main,SW_SHOW);

	/* ボタンのグレーアウト */
	fv_GrayOut(0);
}
/**----------------------------------------------------------------------------
＠ メインＣＢ */
LONG CALLBACK
fx_CBmain(
HWND ax_win,	// <I>イベント発生ウィンドウハンドル
UINT ah_msg,	// <I>イベントタイプ
WPARAM ah_wpam,	// <I>メッセージ１
LPARAM ah_lpam)	// <I>メッセージ２
/*
----------------------------------------------------------------------------**/
{
	int n,m,h;
	DWORD w;
	WNDCLASS az_class = {0};

	switch( ah_msg ) {

	    case WM_CREATE:

		/* メニュー構築 */
		h = 0;
		m = 0;
		n = D_BandX-16;
		w = WS_CHILD|WS_VISIBLE|SS_CENTER;
		m +=  8 +h; h=16; CreateWindow(L"STATIC",L"",		 w,8,m, n,h,ax_win,(HMENU)1000,sx_inst,NULL);
		m +=  4 +h; h=24; CreateWindow(L"BUTTON",L"↓FIX　",	 w,8,m, n,h,ax_win,(HMENU)1001,sx_inst,NULL);
		m +=  3 +h; h=16; CreateWindow(L"EDIT",  L"",		 w,8,m, n,h,ax_win,(HMENU)1002,sx_inst,NULL);
		m +=  4 +h; h=24; CreateWindow(L"BUTTON",L"↑AUTO　",	 w,8,m, n,h,ax_win,(HMENU)1003,sx_inst,NULL);
		m += 24 +h; h=16; CreateWindow(L"EDIT",  L"",		 w,8,m, n,h,ax_win,(HMENU)1004,sx_inst,NULL);
		m +=  4 +h; h=24; CreateWindow(L"BUTTON",L"地図生成",	 w,8,m, n,h,ax_win,(HMENU)1005,sx_inst,NULL);
		m += 16 +h; h=16; CreateWindow(L"STATIC",L"",		 w,8,m, n,h,ax_win,(HMENU)1006,sx_inst,NULL);
		m +=  4 +h; h=24; CreateWindow(L"BUTTON",L"着色（逐次）",w,8,m, n,h,ax_win,(HMENU)1007,sx_inst,NULL);
		m +=  4 +h; h=24; CreateWindow(L"BUTTON",L"着色（一括）",w,8,m, n,h,ax_win,(HMENU)1008,sx_inst,NULL);
		m +=  4 +h; h=24; CreateWindow(L"BUTTON",L"着色クリア",	 w,8,m, n,h,ax_win,(HMENU)1009,sx_inst,NULL);
		m +=  4 +h; h=24; CreateWindow(L"BUTTON",L"EXPORT",    	 w,8,m, n,h,ax_win,(HMENU)1010,sx_inst,NULL);
		m += 24 +h; h=16; CreateWindow(L"EDIT",  L"",		 w,8,m, n,h,ax_win,(HMENU)1011,sx_inst,NULL);
		m +=  4 +h; h=24; CreateWindow(L"BUTTON",L"バッチ実行",	 w,8,m, n,h,ax_win,(HMENU)1012,sx_inst,NULL);

		/* フォント変更 */
		for ( n=1000; n<=1012; n++ ) SendDlgItemMessage(ax_win,n,WM_SETFONT,(WPARAM)sx_font,0);

		/* 入力文字数制限 */
		SendDlgItemMessage(ax_win,1002,EM_LIMITTEXT,8,0);
		SendDlgItemMessage(ax_win,1004,EM_LIMITTEXT,5,0);
		SendDlgItemMessage(ax_win,1011,EM_LIMITTEXT,7,0);

		/* 定数セット */
		SetWindowText(GetDlgItem(ax_win,1002),st1_code);
		SetWindowText(GetDlgItem(ax_win,1004),st1_node);
		SetWindowText(GetDlgItem(ax_win,1011),st1_maps);

		/* 描画Ｗ生成 */
		az_class.hInstance     = sx_inst;
		az_class.lpszClassName = L"CN_disp";
		az_class.lpfnWndProc   = fx_CBdisp;
		az_class.hCursor       = LoadCursor(NULL,IDC_ARROW);
		az_class.hbrBackground = GetStockObject(WHITE_BRUSH);
		RegisterClass(&az_class);
		sx_disp = CreateWindow(L"CN_disp",NULL,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,0,0,ax_win,0,sx_inst,NULL);
		break;

	    /* 結果表示のスタティック背景色 */
	    case WM_CTLCOLORSTATIC:
		if ( GetDlgCtrlID((HWND)ah_lpam) == 1006 ) {
			SetBkMode((HDC)ah_wpam,TRANSPARENT);
			return( (BOOL)GetStockObject(LTGRAY_BRUSH) );
		}
		break;

	    /* 描画Ｗサイズ変更 */
	    case WM_SIZE:
		si_wx = LOWORD(ah_lpam) - D_BandX;
		si_wy = HIWORD(ah_lpam);
		MoveWindow(sx_disp,D_BandX,0,si_wx,si_wy,TRUE);
		InvalidateRect(sx_disp,NULL,TRUE);
		break;

	    /* サイズ変更限界の制御 */
	    case WM_GETMINMAXINFO:
		((LPMINMAXINFO)ah_lpam)->ptMinTrackSize.x = 400;
		((LPMINMAXINFO)ah_lpam)->ptMinTrackSize.y = 500;
		break;

	    case WM_COMMAND:
		n = LOWORD(ah_wpam);
		switch( n ) {

		    /* ＦＩＸ */
		    case 1001:
			GetWindowText(GetDlgItem(ax_win,1000),st1_code,16);
			SetWindowText(GetDlgItem(ax_win,1002),st1_code);
			break;

		    /* ＡＵＴＯ */
		    case 1003:
			GetWindowText(GetDlgItem(ax_win,1002),st1_code,16);
			if ( st1_code[0] ) SetWindowText(GetDlgItem(ax_win,1000),st1_code);
			SetWindowText(GetDlgItem(ax_win,1002),L"");
			break;

		    /* 地図生成 */
		    case 1005:
			fv_MakeMap();
			break;

		    /* 着色(逐次) */
		    case 1007:
			fs_Color(0);
			break;

		    /* 着色(一括) */
		    case 1008:
			fv_GrayOut(0);
			fs_Color(1);
			break;

		    /* バッチ実行 */
		    case 1012:
			fv_Batch();
			break;

		    /* 着色クリア */
		    case 1009:
			sc_step = 0;
			SetWindowText(GetDlgItem(ax_win,1006),L"");
			fv_Clear(1);
			fv_PutPass();
			InvalidateRect(sx_disp,NULL,TRUE);
			fv_GrayOut(3);
			break;

		    /* エクスポート */
		    case 1010:
			fv_Export();
			break;
		}
		break;

	    case WM_DESTROY:
		fv_IniSave();
		PostQuitMessage(0);
		return( 0 );
	}

	return( DefWindowProc(ax_win,ah_msg,ah_wpam,ah_lpam) );
}
/**----------------------------------------------------------------------------
＠ 描画ＣＢ */
LONG CALLBACK
fx_CBdisp(
HWND ax_win,	// <I>イベント発生ウィンドウハンドル
UINT ah_msg,	// <I>イベントタイプ
WPARAM ah_wpam,	// <I>メッセージ１
LPARAM ah_lpam)	// <I>メッセージ２
/*
----------------------------------------------------------------------------**/
{
	int n;
	int x0,y0;
	int x1,y1;
	int ai_wx;
	int ai_wy;
	T_node* apz_node;
	T_line* apz_line;
	RECT az_rect;
	PAINTSTRUCT az_ps;
	HBITMAP ax_bitmap0;
	HBITMAP ax_bitmap1;

	switch( ah_msg ) {

	    /* 着色時の背景描画抑止 */
	    case WM_ERASEBKGND:
		if ( sc_phase == 0 ) return( TRUE );

	    case WM_PAINT:
		BeginPaint(ax_win,&az_ps);
		SelectObject(az_ps.hdc,sx_font);

		/* フェーズ分類 */
		switch( sc_phase ) {

		    /* 処理時間 */
		    case 2:
			TextOut(az_ps.hdc,20,125,st1_tim0,wcslen(st1_tim0));
			TextOut(az_ps.hdc,50,145,st1_tim1,wcslen(st1_tim1));
			TextOut(az_ps.hdc,50,165,st1_tim2,wcslen(st1_tim2));
			// ↓フォールスルー

		    /* 成否カウンタ */
		    case 1:
			TextOut(az_ps.hdc,20,20,st2_info[0],wcslen(st2_info[0]));
			TextOut(az_ps.hdc,20,40,st2_info[1],wcslen(st2_info[1]));
			for ( n=0; n<=1; n++ ) {
				TextOut(az_ps.hdc,50,sz1_rect[n].top,cpt1_item[n],4);
				DrawText(az_ps.hdc,st2_cont[n],wcslen(st2_cont[n]),&sz1_rect[n],DT_RIGHT|DT_NOCLIP|DT_NOPREFIX|DT_SINGLELINE);
			}
			// ↓フォールスルー

		    /* 版権 */
		    case -1:
			TextOut(az_ps.hdc,si_wx-130,si_wy-30,cpt_cw,wcslen(cpt_cw));
			break;

		    /* 地図描画 */
		    case 0:

			/* 描画倍率の算出 */
			ai_wx = si_wx - 2*D_Margin;
			ai_wy = si_wy - 2*D_Margin;

			/* メモリＤＣ割付 */
			ax_bitmap1 = CreateCompatibleBitmap(az_ps.hdc,si_wx,si_wy);
			ax_bitmap0 = (HBITMAP)SelectObject(sx_dc,ax_bitmap1);

			/* 白背景描画 */
			SetRect(&az_rect,0,0,si_wx,si_wy);
			FillRect(sx_dc,&az_rect,GetStockObject(WHITE_BRUSH));

			/* フラグ初期化 */
			for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) apz_node->mc_draw = 0;

			/* 逐次海岸線 */
			if ( spz_sea ) {
				SelectObject(sx_dc,sx1_pen[1]);
				apz_node = spz_sea;
				x0 = MulDiv(apz_node->mi_x,ai_wx,D_Scale) + D_Margin;
				y0 = MulDiv(apz_node->mi_y,ai_wy,D_Scale) + D_Margin;
				MoveToEx(sx_dc,x0,y0,NULL);
				while( 1 ) {
					apz_node = apz_node->mpz1_sea[0];
					x1 = MulDiv(apz_node->mi_x,ai_wx,D_Scale) + D_Margin;
					y1 = MulDiv(apz_node->mi_y,ai_wy,D_Scale) + D_Margin;
					LineTo(sx_dc,x1,y1);
					if ( apz_node == spz_sea ) break;
				}
			}

			/* 接岸ライン */
			if ( spz_qeen ) {
				SelectObject(sx_dc,sx1_pen[2]);
				apz_node = spz_king;
				x0 = MulDiv(apz_node->mi_x,ai_wx,D_Scale) + D_Margin;
				y0 = MulDiv(apz_node->mi_y,ai_wy,D_Scale) + D_Margin;
				SelectObject(sx_dc,GetStockObject(NULL_BRUSH));
				Ellipse(sx_dc,x0-D_Radius-3,y0-D_Radius-3,x0+D_Radius+3,y0+D_Radius+3);	// 終端描画
				MoveToEx(sx_dc,x0,y0,NULL);
				while( 1 ) {
					apz_node = apz_node->mpz1_sea[0];
					x1 = MulDiv(apz_node->mi_x,ai_wx,D_Scale) + D_Margin;
					y1 = MulDiv(apz_node->mi_y,ai_wy,D_Scale) + D_Margin;
					LineTo(sx_dc,x1,y1);
					if ( apz_node == spz_qeen ) break;
				}
			}

			/* 結界ルート */
			if ( spz_waz ) {
				SelectObject(sx_dc,sx1_pen[4]);
				x0 = MulDiv(spz_waz->mi_x,ai_wx,D_Scale) + D_Margin;
				y0 = MulDiv(spz_waz->mi_y,ai_wy,D_Scale) + D_Margin;
				SelectObject(sx_dc,GetStockObject(NULL_BRUSH));
				Ellipse(sx_dc,x0-D_Radius-3,y0-D_Radius-3,x0+D_Radius+3,y0+D_Radius+3);	// 終端描画
				MoveToEx(sx_dc,x0,y0,NULL);
				for ( apz_node=spz_waz; apz_node; apz_node=apz_node->mpz_way0 ) {
					x1 = MulDiv(apz_node->mi_x,ai_wx,D_Scale) + D_Margin;
					y1 = MulDiv(apz_node->mi_y,ai_wy,D_Scale) + D_Margin;
					LineTo(sx_dc,x1,y1);
				}
			}

			/* 最終海岸線 */
			if ( spz_bank ) {
				SelectObject(sx_dc,sx1_pen[0]);
				apz_node = spz_bank;
				x0 = MulDiv(apz_node->mi_x,ai_wx,D_Scale) + D_Margin;
				y0 = MulDiv(apz_node->mi_y,ai_wy,D_Scale) + D_Margin;
				MoveToEx(sx_dc,x0,y0,NULL);
				while( 1 ) {
					apz_node = apz_node->mpz_bank;
					x1 = MulDiv(apz_node->mi_x,ai_wx,D_Scale) + D_Margin;
					y1 = MulDiv(apz_node->mi_y,ai_wy,D_Scale) + D_Margin;
					LineTo(sx_dc,x1,y1);
					if ( apz_node == spz_bank ) break;
				}
			}

			/* ノードループ */
			for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
				x0 = MulDiv(apz_node->mi_x,ai_wx,D_Scale) + D_Margin;
				y0 = MulDiv(apz_node->mi_y,ai_wy,D_Scale) + D_Margin;

				/* ライン */
				SelectObject(sx_dc,GetStockObject(BLACK_PEN));
				for ( apz_line=apz_node->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
					if ( apz_line->mpz_node->mc_draw == 0 ) {
						x1 = MulDiv(apz_line->mpz_node->mi_x,ai_wx,D_Scale) + D_Margin;
						y1 = MulDiv(apz_line->mpz_node->mi_y,ai_wy,D_Scale) + D_Margin;
						MoveToEx(sx_dc,x0,y0,NULL);
						LineTo(sx_dc,x1,y1);
					}
				}

				/* 減色ノード */
				if ( apz_node == spz_base ) {
					SelectObject(sx_dc,GetStockObject(NULL_BRUSH));
					SelectObject(sx_dc,sx1_pen[3]);
					Ellipse(sx_dc,x0-D_Radius-5,y0-D_Radius-5,x0+D_Radius+5,y0+D_Radius+5);
				}

				/* 抜穴ノード */
				if ( apz_node == spz_jack ) {
					SelectObject(sx_dc,GetStockObject(NULL_BRUSH));
					SelectObject(sx_dc,sx1_pen[5]);
					Ellipse(sx_dc,x0-D_Radius-3,y0-D_Radius-3,x0+D_Radius+3,y0+D_Radius+3);

					/* ルート変更不可 */
					if ( sc_lock ) {
						SelectObject(sx_dc,sx1_pen[6]);
						Ellipse(sx_dc,x0-D_Radius-5,y0-D_Radius-5,x0+D_Radius+5,y0+D_Radius+5);
					}
				}

				/* ノード */
				SelectObject(sx_dc,sx1_brs[apz_node->mc_col]);
				SelectObject(sx_dc,GetStockObject(BLACK_PEN));
				Ellipse(sx_dc,x0-D_Radius,y0-D_Radius,x0+D_Radius,y0+D_Radius);
				apz_node->mc_draw = 1;
			}

			/* メモリＤＣ転送 */
			BitBlt(az_ps.hdc,0,0,si_wx,si_wy,sx_dc,0,0,SRCCOPY);

			/* ビットマップ解放 */
			SelectObject(sx_dc,ax_bitmap0);
			DeleteObject(ax_bitmap1);
			break;
		}
		EndPaint(ax_win,&az_ps);
		break;
	}

	return( DefWindowProc(ax_win,ah_msg,ah_wpam,ah_lpam) );
}
/**----------------------------------------------------------------------------
＠ 入力欄のタブキー制御 */
void
fv_EditFocus(
char ac_num)	// <I>入力欄番号(0,1,2,3)
/*
----------------------------------------------------------------------------**/
{
	int ai_set;

	switch( ac_num ) {
	    case 0: ai_set = ( dx_Key(VK_SHIFT) ? 1011 : 1002 ); break;
	    case 1: ai_set = ( dx_Key(VK_SHIFT) ? 1011 : 1004 ); break;
	    case 2: ai_set = ( dx_Key(VK_SHIFT) ? 1002 : 1011 ); break;
	    case 3: ai_set = ( dx_Key(VK_SHIFT) ? 1004 : 1002 ); break;
	}

	SetFocus(GetDlgItem(sx_main,ai_set));
	SendDlgItemMessage(sx_main,ai_set,EM_SETSEL,0,-1);
}
/**----------------------------------------------------------------------------
＠ メニューボタンのグレーアウト処理 */
void
fv_GrayOut(
char ac_sw)	// <I>指定モード(0,2,3,4,-1,1)
/*
		０　２　３　４
	逐次	□　□　■　■　　　－１：バッチin
	一括	□　□　■　■　　　　１：バッチout
	クリア	□　■　□　■
	EXPort	□　■　■　■
----------------------------------------------------------------------------**/
{
	int n;
	int ai_show;
	char ac_sw1;
	char ac_sw2;
	char ac_sw3;
	char ac_sw4;

	/* バッチ対応 */
	if ( ac_sw == 1 || ac_sw == -1 ) {
		ai_show = ( ac_sw < 0 ? SW_HIDE : SW_SHOWNORMAL );
		for ( n=1000; n<=1012; n++ ) ShowWindow(GetDlgItem(sx_main,n),ai_show);
		if ( ac_sw < 0 ) return;
		ac_sw = 0;
	}

	switch( ac_sw ) {
	    case 0: ac_sw1 = FALSE; ac_sw2 = FALSE; ac_sw3 = FALSE; ac_sw4 = FALSE; break;
	    case 2: ac_sw1 = FALSE; ac_sw2 = FALSE; ac_sw3 =  TRUE; ac_sw4 =  TRUE; break;
	    case 3: ac_sw1 =  TRUE; ac_sw2 =  TRUE; ac_sw3 = FALSE; ac_sw4 =  TRUE; break;
	    case 4: ac_sw1 =  TRUE; ac_sw2 =  TRUE; ac_sw3 =  TRUE; ac_sw4 =  TRUE; break;
	}

	EnableWindow(GetDlgItem(sx_main,1007),ac_sw1);
	EnableWindow(GetDlgItem(sx_main,1008),ac_sw2);
	EnableWindow(GetDlgItem(sx_main,1009),ac_sw3);
	EnableWindow(GetDlgItem(sx_main,1010),ac_sw4);

	SetFocus(sx_main);
}
/**----------------------------------------------------------------------------
＠ 地図の作成 */
void
fv_MakeMap()
/*
----------------------------------------------------------------------------**/
{
	int ai_cnt;
	long long al_code;
	DWORD ah_code;
	WORD at1_txt[64];

	/* 制御変数初期化 */
	sc_phase = 0;
	sc_step = 0;
	sc_fine = 0;

	/* 状態表示クリア */
	SetWindowText(GetDlgItem(sx_main,1006),L"");
	fv_GrayOut(0);

	/* 乱数コードの確保 */
	GetWindowText(GetDlgItem(sx_main,1002),st1_code,16);
	wcscpy(at1_txt,st1_code);

	/* 入力欄がブランクの場合は時刻値を採用 */
	al_code = ( at1_txt[0] ? wcstoll(st1_code,NULL,16) : timeGetTime() );
	wsprintf(st1_code,L"%08x",al_code);
	ah_code = (DWORD)al_code;

	/* コード欄のセット */
	SetWindowText(GetDlgItem(sx_main,1000),st1_code);
	if ( at1_txt[0] ) SetWindowText(GetDlgItem(sx_main,1002),st1_code);

	/* 乱数の種 */
	srand( ah_code );

	/* 生成ノード数の確保 */
	GetWindowText(GetDlgItem(sx_main,1004),st1_node,8);
	ai_cnt = fi_Range(3,_wtoi(st1_node),D_MaxNode);
	wsprintf(st1_node,L"%d",ai_cnt);
	SetWindowText(GetDlgItem(sx_main,1004),st1_node);
	UpdateWindow(GetDlgItem(sx_main,1004));

	/* 生成 */
	fv_CreateMap(ai_cnt);
	InvalidateRect(sx_disp,NULL,TRUE);
	fv_GrayOut(3);
}
/**----------------------------------------------------------------------------
＠ 着色プロセス */
short		// <R>結果( 0:成功 1:失敗ノードＩＤ )
fs_Color(
char ac_type)	// <I>着色刻みタイプ( 0:逐次 1:一括 )
/*
----------------------------------------------------------------------------**/
{
	short as_id;
	DWORD ah_tim0;
	DWORD ah_tim1;
	WORD at1_txt[64];

	/* 着色開始チェック */
	if ( spz_node == NULL || sc_step < 0 ) return( 0 );

	/* タイムアウト基点セット */
	ah_tim0 = timeGetTime();

	/* ダミーループ */
	as_id = 0;
	while( 1 ) {

		/* 一括の場合のみタイムアウトチェック */
		if ( ac_type && sc_step < 7 ) {
			ah_tim1 = timeGetTime();
			if ( ah_tim1 - ah_tim0 > D_TimeOut ) sc_step = 7;
		}

		/* 基点三角ネット */
		if ( sc_step == 0 ) {
			switch( fc_Step0() ) {
			    case 0: sc_step = 1; break;
			    case 1: sc_step = 7; continue;
			}
		}
		else
		/* 追加ノード選択（接岸ライン生成）*/
		if ( sc_step == 1 ) {
			switch( fc_Step1() ) {
			    case 0: sc_step = 1; break;
			    case 1: sc_step = 2; break;
			    case 2: sc_step = 7; continue;
			}
		}
		else
		/* 減色ノードの検出 */
		if ( sc_step == 2 ) {
			switch( fc_Step2() ) {
			    case 0: sc_step = 1; break;
			    case 1: sc_step = 3; break;
			}
		}
		else
		/* 減色ノードの処理（結界ルート生成）*/
		if ( sc_step == 3 ) {
			switch( fc_Step3() ) {
			    case 0: sc_step = 2; continue;
			    case 1: sc_step = 4; break;
			}
		}
		else
		/* 抜穴ノードの検出 */
		if ( sc_step == 4 ) {
			switch( fc_Step4() ) {
			    case 0: sc_step = 2; continue;
			    case 1: sc_step = 4; break;
			    case 2: sc_step = 5; break;
			}
		}
		else
		/* 抜穴ノードの封印 */
		if ( sc_step == 5 ) {
			switch( fc_Step5() ) {
			    case 0: sc_step = 4; break;
			    case 1: sc_step = 6; break;
			}
		}
		else
		/* 三つ巴スワップ */
		if ( sc_step == 6 ) {
			fv_Step6();
			sc_step = 2;
		}
		else
		/* 三国峠の着色復帰 */
		if ( sc_step == 7 ) {
			switch( fc_Step7() ) {
			    case 0: sc_step = 8; continue;
			    case 1: sc_step = 8; break;
			}
		}
		else
		/* 終了処理 */
		if ( sc_step == 8 ) {
			as_id = fs_FailCheck();	// 色干渉チェック
			if ( sc_phase != 1 ) {
				wcscpy(at1_txt,cpt1_rep[( as_id ? 1 : 0 )]);
				SetWindowText(GetDlgItem(sx_main,1006),at1_txt);
			}
			sc_step = -1;
			break;
		}

		/* 逐次脱出 */
		if ( ac_type == 0 ) break;
	}

	/* 逐次地図描画 */
	if ( sc_phase == 0 ) {
		fv_GrayOut( sc_step < 0 ? 2 : 4 );
		InvalidateRect(sx_disp,NULL,TRUE);
	}

	return( as_id );
}
/**----------------------------------------------------------------------------
＠ 基点三角ネット */
char		// <R>状態( 0:正常 1:異常検出 )
fc_Step0()
/*
	基点三角ネットを初期海岸線にセットする際、スピン０になるようにポインタを張る。
	これにより以降のノード追加によって海岸線が拡張していく場合もスピン０が保たれる。
	結界ルートの構築や連鎖ルートの枝刈などで、この前提が活用できる。
	なお、三国峠の切り離しが成されているため、ノードＩＤに歯抜けが生じている。
----------------------------------------------------------------------------**/
{
	int n,m;
	int x,y;
	int s1,s2;
	T_node* apz_set0;
	T_node* apz_set1;
	T_node* apz_set2;
	T_node* apz_set3;
	T_node* apz_node;
	T_line* apz_line;

	/* 三国峠の切り離し */
	fv_CutPass();

	/* 基本三角ネットの１ノード目を抽出 */
	m = INT_MAX;
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
		if ( m > apz_node->mi_dist ) {
			m = apz_node->mi_dist;
			apz_set1 = apz_node;
		}
	}

	/* １ノード目に接続されている２ノード目を抽出 */
	m = INT_MAX;
	for ( apz_line=apz_set1->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
		apz_node = apz_line->mpz_node;
		if ( m > apz_node->mi_dist ) {
			m = apz_node->mi_dist;
			apz_set2 = apz_node;
		}
	}

	/* １ノード目と２ノード目の共通ノードで最近接のノードを３ノード目に設定 */
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) apz_node->mc_done = 0;
	for ( apz_line=apz_set1->mpz_line; apz_line; apz_line=apz_line->mpz_next ) apz_line->mpz_node->mc_done = 1;
	m = INT_MAX;
	for ( apz_line=apz_set2->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
		apz_node = apz_line->mpz_node;
		if ( apz_node->mc_done == 0 ) continue;
		n = fi_Distance(apz_node->mi_x,apz_node->mi_y,apz_set1->mi_x,apz_set1->mi_y) +
		    fi_Distance(apz_node->mi_x,apz_node->mi_y,apz_set2->mi_x,apz_set2->mi_y);
		if ( m > n ) {
			m = n;
			apz_set3 = apz_node;
		}
	}

	/* Ｙ座標が最小のノードを選択 */
	apz_set0 = apz_set1;
	if ( apz_set0->mi_y > apz_set2->mi_y ) apz_set0 = apz_set2;
	if ( apz_set0->mi_y > apz_set3->mi_y ) apz_set0 = apz_set3;

	/* 最小以外のノードを移動 */
	     if ( apz_set0 == apz_set1 ) apz_set1 = apz_set3;
	else if ( apz_set0 == apz_set2 ) apz_set2 = apz_set3;

	/* 最小外ノード１のスピン回転値を算出 */
	x = apz_set1->mi_x - apz_set0->mi_x;
	y = apz_set1->mi_y - apz_set0->mi_y;
	s1 = MulDiv(y*y,INT_MAX/4,x*x+y*y);
	if ( x < 0 ) s1 = 2*(INT_MAX/4) - s1;

	/* 最小外ノード２のスピン回転値を算出 */
	x = apz_set2->mi_x - apz_set0->mi_x;
	y = apz_set2->mi_y - apz_set0->mi_y;
	s2 = MulDiv(y*y,INT_MAX/4,x*x+y*y);
	if ( x < 0 ) s2 = 2*(INT_MAX/4) - s2;

	/* スピン回転値比較でノード１とノード２を入替え */
	if ( s1 < s2 ) {
		apz_set3 = apz_set1;
		apz_set1 = apz_set2;
		apz_set2 = apz_set3;
	}

	/* 基点三角ネットを初期海岸線にセット */
	spz_sea = apz_set0;
	apz_set0->mpz1_sea[0] = apz_set1; apz_set1->mpz1_sea[1] = apz_set0;
	apz_set1->mpz1_sea[0] = apz_set2; apz_set2->mpz1_sea[1] = apz_set1;
	apz_set2->mpz1_sea[0] = apz_set0; apz_set0->mpz1_sea[1] = apz_set2;

	/* 基点三角ネット着色 */
	apz_set0->mc_col = 1;
	apz_set1->mc_col = 2;
	apz_set2->mc_col = 3;

	/* 追加ノード検出用の接続数セット */
	for ( apz_line=apz_set0->mpz_line; apz_line; apz_line=apz_line->mpz_next ) apz_line->mpz_node->ms_can++;
	for ( apz_line=apz_set1->mpz_line; apz_line; apz_line=apz_line->mpz_next ) apz_line->mpz_node->ms_can++;
	for ( apz_line=apz_set2->mpz_line; apz_line; apz_line=apz_line->mpz_next ) apz_line->mpz_node->ms_can++;

	return( 0 );
}
/**----------------------------------------------------------------------------
＠ 追加ノード選択（接岸ライン生成）*/
char		// <R>状態( 0:単純着色 1:接岸ライン発生 2:最終海岸線の完全着色 )
fc_Step1()
/*
	追加ノードの接岸ライン上の色数を調査する。
	３色以内の場合は、ノードを着色し海岸線の更新を行う。
	４色状態の場合は、接岸ラインを生成し、接岸端のqeenとkingにセットする。
----------------------------------------------------------------------------**/
{
	int m;
	char ac_col;
	BYTE ak_col;
	T_node* apz_node;
	T_node* apz1_temp[2];
	T_node* apz1_edge[2];
	T_line* apz_line;

	/* 最終海岸線の完全着色 */
	if ( sc_fine ) return( 2 );

	/* 追加ノード検出ループ */
	m = INT_MAX;
	spz_add = NULL;
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
		if ( apz_node->mc_col || apz_node->ms_can < 2 ) continue;	// 未着色の複数接続ノードのみ選択

		/* 地図の中心に近いノードを優先選択 */
		if ( apz_node->mi_dist < m ) {

			/* 連続着色ノード接続の禁則チェックに合格したら暫定候補としてセット */
			if ( fc_GapCheck(apz_node,apz1_temp) == 0 ) {
				m = apz_node->mi_dist;
				spz_add = apz_node;
				apz1_edge[0] = apz1_temp[0];
				apz1_edge[1] = apz1_temp[1];
			}
		}
	}

	/* 全ノード着色済 */
	if ( spz_add == NULL ) {

		/* 最終海岸線のＢノード存在チェック */
		apz_node = spz_bank;
		while( 1 ) {
			if ( apz_node->mc_col == 4 ) break;
			apz_node = apz_node->mpz_bank;
			if ( apz_node == spz_bank ) return( 2 );
		}

		/* 最終海岸線を接岸ラインとしてセット */
		spz_qeen = spz_sea;
		spz_king = spz_qeen->mpz1_sea[0];

		/* 三つ巴スワップ番号の初期化 */
		for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) apz_node->mc_fan = 0;

		/* 最終海岸線処理へ移行 */
		sc_fine = 1;
		return( 1 );
	}

	/* 接岸ライン上の出現色を調査 */
	ak_col = 0;
	apz_node = apz1_edge[0];
	while( 1 ) {
		ak_col |= 1 << (apz_node->mc_col - 1);
		if ( apz_node == apz1_edge[1] ) break;
		apz_node = apz_node->mpz1_sea[1];
	}

	/* 接岸ライン上に４色すべてが出現する場合 */
	if ( ak_col == 15 ) {
		spz_add->mc_col = 5;	// 追加ノードの選択色セット

		/* 接岸端のセット */
		spz_base = NULL;
		spz_qeen = apz1_edge[0];
		spz_king = apz1_edge[1];

		/* 三つ巴スワップ番号の初期化 */
		for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) apz_node->mc_fan = 0;
		return( 1 );
	}

	/* 非出現で着色 */
	for ( ac_col=1; ac_col<4; ac_col++ ) if ( ( ak_col & ( 1 << (ac_col - 1) ) ) == 0 ) break;
	spz_add->mc_col = ac_col;

	/* 海岸線ループの更新 */
	spz_sea = spz_add;
	spz_sea->mpz1_sea[0] = apz1_edge[0];
	spz_sea->mpz1_sea[1] = apz1_edge[1];
	apz1_edge[1]->mpz1_sea[0] = spz_sea;
	apz1_edge[0]->mpz1_sea[1] = spz_sea;

	/* 追加ノード検出用の接続数セット */
	for ( apz_line=spz_add->mpz_line; apz_line; apz_line=apz_line->mpz_next ) apz_line->mpz_node->ms_can++;

	return( 0 );
}
/**----------------------------------------------------------------------------
＠ 減色ノードの検出 */
char		// <R>状態( 0:一掃 1:検出 )
fc_Step2()
/*
	接岸ライン上に減色ノードが存在するか調査し、一掃されていたら追加ノードを着色し海岸線の更新を行う。
	残存していたら、そのノードをspz_baseとし、接岸ルートのエリア情報 area[0] をセットする。
----------------------------------------------------------------------------**/
{
	T_node* apz_node;
	T_line* apz_line;

	/* 既存の結界ルートをクリア */
	if ( spz_way ) {
		for ( apz_node=spz_way; apz_node; apz_node=apz_node->mpz_way1 ) apz_node->mc1_area[1] = 0;
		sc_lock = 0;
		spz_way = NULL;
		spz_waz = NULL;
		spz_jack = NULL;
	}

	/* 減色ノードの走査 */
	apz_node = spz_qeen;
	while( 1 ) {

		/* 減色ノード検出（接岸エリアのフラグをセット） */
		if ( apz_node->mc_col == 4 ) {
			spz_base = apz_node;
			for ( apz_node=spz_qeen; apz_node!=spz_base; apz_node=apz_node->mpz1_sea[1] ) apz_node->mc1_area[0] =  1;
			for ( apz_node=spz_king; apz_node!=spz_base; apz_node=apz_node->mpz1_sea[0] ) apz_node->mc1_area[0] = -1;
			spz_base->mc1_area[0] = 1;
			return( 1 );
		}

		if ( apz_node == spz_king ) break;
		apz_node = apz_node->mpz1_sea[1];
	}

	/* 最終海岸線では実施しない処理 */
	if ( sc_fine == 0 ) {

		/* 追加ノードを着色 */
		spz_add->mc_col = 4;

		/* 接岸ラインのエリアフラグをクリア */
		for ( apz_node=spz_qeen; apz_node!=spz_king; apz_node=apz_node->mpz1_sea[1] ) apz_node->mc1_area[0] = 0;
		spz_king->mc1_area[0] = 0;

		/* 海岸線ループの更新 */
		spz_sea = spz_add;
		spz_sea->mpz1_sea[1] = spz_king;
		spz_sea->mpz1_sea[0] = spz_qeen;
		spz_king->mpz1_sea[0] = spz_sea;
		spz_qeen->mpz1_sea[1] = spz_sea;

		/* 追加ノード検出用の接続数セット */
		for ( apz_line=spz_add->mpz_line; apz_line; apz_line=apz_line->mpz_next ) apz_line->mpz_node->ms_can++;
	}

	/* マークデータクリア */
	spz_king = NULL;
	spz_qeen = NULL;
	spz_base = NULL;

	return( 0 );
}
/**----------------------------------------------------------------------------
＠ 減色ノードの処理（結界ルート生成）*/
char		// <R>状態( 0:減色完了 1:結界ルート発生 )
fc_Step3()
/*
	減色ノードの内陸への押込みを試みる。
	もし失敗した場合は、前方エリアへのシフトを試みる。
	それも失敗した場合は、以下の帰着遮断を試みる。
	帰着遮断はＢに対峙する３色の連鎖ルートに対して２系統遮断スワップを行う。
	この２系統はスワップ色が排他的であるため、遮断効果が干渉しない。
	各２系統遮断スワップは、遮断色を入替えて連続実行する。
		ＢＲ連鎖ルート： ＢＧ＋ＲＹ (入替)→ ＢＹ＋ＲＧ
		ＢＧ連鎖ルート： ＢＹ＋ＧＲ (入替)→ ＢＲ＋ＧＹ
		ＢＹ連鎖ルート： ＢＲ＋ＹＧ (入替)→ ＢＧ＋ＹＲ
	これらのすべての試行が失敗した場合は、結界ルートを生成する。
----------------------------------------------------------------------------**/
{
	int n,m;
	char ac_col;
	char ac_bow;
	T_node* apz_node;
	T_node* apz_exam;
	T_line* apz_line;

	/* ＲＧＹスワップのループ */
	ac_bow = 0;
	for ( ac_col=1; ac_col<=3; ac_col++ ) {
		fv_SwapFlg(spz_base,ac_col);

		/* 後方エリア帰着チェック */
		for ( apz_node=spz_qeen; apz_node!=spz_base; apz_node=apz_node->mpz1_sea[1] ) if ( apz_node->mc_swap == 4 ) break;
		if ( apz_node == spz_base ) {

			/* 後方エリア未踏色の記憶 */
			if ( ac_bow == 0 ) ac_bow = ac_col;

			/* 前方エリア帰着チェック */
			for ( apz_node=spz_king; apz_node!=spz_base; apz_node=apz_node->mpz1_sea[0] ) if ( apz_node->mc_swap == 4 ) break;
			if ( apz_node == spz_base ) {
				fv_SwapExe(spz_base,ac_col);	// 内陸への押込み成功
				return( 0 );
			}
		}
	}

	/* 後方エリア未踏色でのスワップ */
	if ( ac_bow ) {
		fv_SwapExe(spz_base,ac_bow);
		return( 0 );
	}

	/* 連鎖色交代ループ */
	for ( m=0; m<3; m++ ) {

		/* 減色スワップによるフラグセット */
		fv_SwapFlg(spz_base,m+1);

		/* 遮断色入替ループ */
		for ( n=0; n<2; n++ ) {

			fv_CutBranch(spz_base);				// 枝刈
			fv_SwapMsk(cc3_col[m][n][0],4);			// Ｂ帰着抑止のマスク処理（接岸ライン後方）
			fv_BaseGard(4,m+1,cc3_col[m][n][0]);		// Ｂ遮断スワップ
			if ( spz_base->mc_col != 4 ) return( 0 );	// ダイレクト減色発生
			if ( fc_BaseFix(m+1) ) return( 0 );		// Ｂ遮断による減色確認

			fv_CutBranch(spz_base);				// 枝刈
			fv_BaseGard(m+1,4,cc3_col[m][n][1]);		// Ａ遮断スワップ
			if ( fc_BaseFix(m+1) ) return( 0 );		// Ａ遮断による減色確認

		}
	}

	/* 結界ルートの初期化 */
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
		apz_node->mpz_way0 = NULL;
		apz_node->mpz_way1 = NULL;
	}

	/* 結界ルート始終端のセット */
	spz_way = spz_base->mpz1_sea[0];
	spz_waz = spz_base->mpz1_sea[1];
	spz_way->mpz_way0 = NULL;
	spz_waz->mpz_way1 = NULL;

	/* 結界ルート始端ラインの検出 */
	for ( apz_line=spz_base->mpz_line; apz_line; apz_line=apz_line->mpz_next ) if ( apz_line->mpz_node == spz_way ) break;

	/* 結界ルート双方向ポインタの構築 */
	apz_node = spz_way;
	while( 1 ) {
		apz_line = apz_line->mpz1_spin[0];
		apz_exam = apz_line->mpz_node;

		/* ポインタ構築 */
		apz_node->mpz_way1 = apz_exam;
		apz_exam->mpz_way0 = apz_node;

		/* 接岸ライン前方ノードに到達したら停止 */
		if ( apz_exam == spz_waz ) break;
		apz_node = apz_exam;
	}

	/* 抜穴ノードの初期化 */
	sc_lock = 0;
	spz_jack = NULL;

	return( 1 );
}
/**----------------------------------------------------------------------------
＠ 抜穴ノードの検出 */
char		// <R>状態( 0:一掃 1:ＲＧガード迂回 2:検出 )
fc_Step4()
/*
	結界ルート上の抜穴ノード spz_jack を検出する。
	もし抜穴ノードが減色ノードの連鎖ルート経由点でない場合は、ＲＧガードが存在している可能性がある。
	よってその場合は、ＲＧガードで結界ルートを迂回する。
	ただし、結界ルートの前後エリアをＲＧガードが取り囲んでいない場合は、抜穴ノードとして扱う。
----------------------------------------------------------------------------**/
{
	T_node* apz_node;

	/* 抜穴ノード候補Ｙの検出ループ */
	sc_lock = 0;
	spz_jack = NULL;
	for ( apz_node=spz_way; apz_node; apz_node=apz_node->mpz_way1 ) if ( apz_node->mc_col == 3 ) break;

	/* 抜穴ノード一掃の場合は減色ノードでＢＹスワップし返却 */
	if ( apz_node == NULL ) {
		fv_SwapExe(spz_base,3);
		return( 0 );
	}

	/* 減色ノードからのＢＹ連鎖フラグセット */
	fv_SwapFlg(spz_base,3);
	fv_CutBranch(spz_base);

	/* ＢＹ連鎖の経由ノードでない場合はＲＧガード迂回を試行 */
	if ( apz_node->mc_swap != 4 && fc_GardChg(apz_node) ) return( 1 );

	/* 抜穴ノードの確定 */
	spz_jack = apz_node;
	return( 2 );
}
/**----------------------------------------------------------------------------
＠ 抜穴ノードの封印 */
char		// <R>封印状態( 0:成功 1:失敗 )
fc_Step5()
/*
	抜穴ノードの隣接ノード色で封印色を決定し、その封印色でスワップを試行する。
	結界ルート後方エリアに封印色が発生しない場合は、抜穴ノードを封印色で確定スワップする。
	封印色が発生する場合は、帰着ルート上の封印色の対色で遮断することを試みる。
	対色ガードをする際、抜穴ノードの隣接後方ノードの変色を防ぐため、マスクスワップをしておく。
	また対色ガード後に、新たな帰着ルートが発生する可能性があるため、帰着チェックを行う。
	帰着する場合は封印失敗となり、三つ巴スワップに移行する。
----------------------------------------------------------------------------**/
{
	char ac_gard;
	T_node* apz_node;

	/* 封印色の決定 */
	ac_gard = 3 - ( spz_jack == spz_way ? spz_jack->mpz_way1->mc_col : spz_jack->mpz_way0->mc_col );

	/* スワップによる結界ルート後方の封印色帰着チェック */
	fv_SwapFlg(spz_jack,ac_gard);
	for ( apz_node=spz_jack; apz_node; apz_node=apz_node->mpz_way0 ) if ( apz_node->mc_swap == 3 ) break;

	/* 帰着ルートの対色遮断 */
	if ( apz_node ) {

		/* 枝刈 */
		fv_CutBranch(spz_jack);

		/* 隣接後方ノードのマスクスワップ */
		sc1_swap[0] = 3-ac_gard;
		sc1_swap[1] = ac_gard;
		for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) apz_node->mc_done = 0;
		fv_RCmask(0,spz_jack->mpz_way0);

		/* 対色スワップによる帰着ルート遮断 */
		for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
			if ( apz_node->mc_swap && apz_node->mc_col == ac_gard ) fv_SwapExe(apz_node,3-ac_gard);
		}

		/* 対色遮断後の結界ルート後方への帰着チェック */
		fv_SwapFlg(spz_jack,ac_gard);
		for ( apz_node=spz_jack; apz_node; apz_node=apz_node->mpz_way0 ) if ( apz_node->mc_swap == 3 ) break;

		/* 三つ巴スワップへ移行 */
		if ( apz_node ) {
			sc_lock = 1;
			return( 1 );
		}
	}

	/* 確定スワップ */
	fv_SwapExe(spz_jack,ac_gard);
	return( 0 );
}
/**----------------------------------------------------------------------------
＠ 三つ巴スワップ */
void
fv_Step6()
/*
	減色ノードＢに対して、B - (R,G)(G,R)(G,Y)(Y,G)(Y,R)(R,Y)の６通りの三つ巴スワップを実施する。
	実施するスワップは、ルート変更不可に達した抜穴ノードが持つ三つ巴スワップ番号(mc_fan)で制御する。
	スワップ番号は、実施後に更新されローテーションで繰り返される。
----------------------------------------------------------------------------**/
{
	/* 三つ巴 */
	fv_SwapExe(spz_base,cc2_fan[spz_jack->mc_fan][0]);
	fv_SwapExe(spz_base,cc2_fan[spz_jack->mc_fan][1]);

	/* 更新 */
	spz_jack->mc_fan++;
	if ( spz_jack->mc_fan == 6 ) spz_jack->mc_fan = 0;
}
/**----------------------------------------------------------------------------
＠ 三国峠の着色復帰 */
char		// <R>処理状況( 0:無 1:有 )
fc_Step7()
/*
----------------------------------------------------------------------------**/
{
	BYTE ak_col;
	char ac_col;
	T_node* apz_node;
	T_line* apz_line;

	/* 制御データクリア */
	fv_Clear(0);

	/* 三国峠が存在しなかった場合 */
	if ( spz_pass == NULL ) return( 0 );

	/* 三国峠着色 */
	for ( apz_node=spz_pass; apz_node; apz_node=apz_node->mpz_next ) {

		/* 使用色の調査 */
		ak_col = 0;
		for ( apz_line=apz_node->mpz_line; apz_line; apz_line=apz_line->mpz_next ) ak_col |= 1 << (apz_line->mpz_node->mc_col - 1);

		/* 未使用色で着色 */
		for ( ac_col=1; ac_col<4; ac_col++ ) if ( ( ak_col & ( 1 << (ac_col - 1) ) ) == 0 ) break;
		apz_node->mc_col = ac_col;
	}

	/* 三国峠の復帰 */
	fv_PutPass();

	return( 1 );
}
/**----------------------------------------------------------------------------
＠ 遮断スワップ */
void
fv_BaseGard(
char ac_targ,	// <I>ターゲット色
char ac_swap,	// <I>フラグ色
char ac_gard)	// <I>遮断色
/*
	フラグ色のスワップフラグが立っているターゲット色のノードを遮断色でスワップする。
----------------------------------------------------------------------------**/
{
	T_node* apz_node;

	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
		if ( apz_node->mc_swap == ac_swap && apz_node->mc_col == ac_targ ) fv_SwapExe(apz_node,ac_gard);
	}
}
/**----------------------------------------------------------------------------
＠ 減色ノードの遮断 */
char		// <R>状態( 0:失敗 1:成功 )
fc_BaseFix(
ac_col)		// <I>変更色
/*
	減色ノードから与えられた変更色でスワップを試行し、
	接岸ライン後方エリアにＢが発生しない場合は、減色ノードを変更色で確定スワップする。
	Ｂが発生する場合は、何もせずに失敗を返す。
----------------------------------------------------------------------------**/
{
	T_node* apz_node;

	/* スワップによる接岸ラインＢ帰着チェック */
	fv_SwapFlg(spz_base,ac_col);
	for ( apz_node=spz_qeen; apz_node!=spz_base; apz_node=apz_node->mpz1_sea[1] ) if ( apz_node->mc_swap == 4 ) return( 0 );

	/* 確定スワップ */
	fv_SwapExe(spz_base,ac_col);
	return( 1 );
}
/**----------------------------------------------------------------------------
＠ ＲＧガード迂回 */
char			// <R>変更状況( 0:無 1:有 )
fc_GardChg(
T_node* apz_hole)	// <I>基点となる抜穴ノード
/*
	与えられた接岸ルート上の抜穴ノードＹが、減色ノードＢからのＢＹ連鎖帰着ルートの経由ノードでない場合、
	この抜穴ノードはＲＧ連鎖ガードで囲まれている可能性がある。
	前方エリアは、まだ未処理のＹが残っているため必ずしもＲＧガードが対象のＹを取り囲んでいるとは限らない。
	対して結界ルート後方はＲＧ化を終えているため、このチェックは後方から前方に至るＲＧスワップ試行で行う。
	もし取り囲まれている、つまり後方エリアのＲＧノードの連鎖が前方エリアのＲＧに到達していれば、
	このＲＧガードで結界ルートを変更し、対象のＹノードを結界の中に取り込む。
	もし取り囲まれていない場合は、ＲＧガード迂回は実施せずに返却し、通常の抜穴ノードとして扱う。

	  ○－－－－－●－－－－－◎
	apz_arm     apz_pin     apz_new

	apz_pinを中心として、その周辺ノードapz_armからスピン１方向に探索を開始。
	ルート変更は、結界ライン前方から後方に向けて探査する。
	遮断帰着ルート上のノードをapz_newとし、apz_pinとapz_newを新ルートとして採用する。
	現結界ルートに沿っている間は、ポインタを変更せずにそのまま進行する。
	新ルートのトレースは、まずmpz_way1だけを更新し、後処理でmpz_way0を付与する。
		- 現結界ルートに沿っている場合は差替えない
		- 現結界ルート前方エリアを横切る場合は差替えない
		- 前方エリアは通過する（ただし、終端に到達したら停止する）
		- 後方エリアにＹで到達した場合は停止する
----------------------------------------------------------------------------**/
{
	T_node* apz_pin;
	T_node* apz_arm;
	T_node* apz_new;
	T_node* apz_node;
	T_line* apz_line;

	/* 抜穴ノードの後方からのＲＧ試行スワップ */
	fv_SwapFlg(apz_hole->mpz_way0,3-apz_hole->mpz_way0->mc_col);

	/* 前方帰着点が存在しない場合はＲＧガード迂回不可 */
	for ( apz_node=apz_hole; apz_node; apz_node=apz_node->mpz_way1 ) if ( apz_node->mc_swap ) break;
	if ( apz_node == NULL ) return( 0 );

	/* 枝刈 */
	fv_CutBranch(apz_hole);

	/* エリアフラグのセット */
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) apz_node->mc1_area[1] =  0;
	for ( apz_node=apz_hole; apz_node; apz_node=apz_node->mpz_way1 ) apz_node->mc1_area[1] = -1;
	for ( apz_node=apz_hole; apz_node; apz_node=apz_node->mpz_way0 ) apz_node->mc1_area[1] =  1;

	/* トレース基点の後方にある近接帰着ノードを開始点として採用 */
	for ( apz_pin=apz_hole->mpz_way0; apz_pin; apz_pin=apz_pin->mpz_way0 ) if ( apz_pin->mc_swap ) break;

	/* 初期の周回探査開始ノードの設定 */
	apz_arm = ( apz_pin == spz_waz ? spz_base : apz_pin->mpz_way1 );

	/* 基点からのトレース */
	while( 1 ) {

		/* 周回探査開始ノードのライン抽出 */
		for ( apz_line=apz_pin->mpz_line; apz_line; apz_line=apz_line->mpz_next ) if ( apz_line->mpz_node == apz_arm ) break;

		/* 帰着ルート上の最近傍ノードを検出 */
		while( 1 ) {
			apz_line = apz_line->mpz1_spin[1];
			apz_new = apz_line->mpz_node;
			if ( apz_new->mc_swap ) break;
		}

		/* ポインタの挿替 */
		apz_pin->mpz_way1 = apz_new;

		/* 探査ノード更新 */
		apz_arm = apz_pin;
		apz_pin = apz_new;

		/* 前方エリア到達で終了フラグ */
		if ( apz_new == apz_hole || apz_new->mc1_area[1] < 0 ) break;
	}

	/* 最終ノードのライン抽出 */
	for ( apz_line=apz_pin->mpz_line; apz_line; apz_line=apz_line->mpz_next ) if ( apz_line->mpz_node == apz_arm ) break;

	/* 新しい後進ポインタを付与 */
	apz_pin = NULL;
	for ( apz_node=spz_way; apz_node; apz_node=apz_node->mpz_way1 ) {
		apz_node->mpz_way0 = apz_pin;
		apz_pin = apz_node;
	}

	return( 1 );
}
/**----------------------------------------------------------------------------
＠ 遮断帰着ルートの枝刈 */
void
fv_CutBranch(
T_node* apz_prim)	// <I>枝刈判定ノード
/*
	mc_swapフラグに対してトレース枝刈を実施する。
	mc_divは、初期状態で周辺ノードのswapノード数が格納されている。
	ある方向の枝をトレースする際に減算していくため、未処理の枝数として認識できる。
	mc_doneは、初期状態で接岸ライン後方エリアの帰着ノードに立てておく。
	そのフラグ値はスワップフラグの固定化を示し、以下の種類がある。
		0: 固定化されていない一般ノード
		1: 帰着ノードに接したことで固定されたノード
		2: 後方エリアに存在する帰着ノード
		3: 基点ノード( spz_base | spz_jack | etc )
----------------------------------------------------------------------------**/
{
	T_node* apz_pin;
	T_node* apz_arm;
	T_node* apz_fit;
	T_node* apz_node;
	T_line* apz_line;

	/* 枝刈用変数の初期化 */
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
		apz_node->ms_div = SHRT_MIN;
		apz_node->mc_done = 0;
		apz_node->mpz_trac = NULL;

		/* スワップフラグが立っている周辺ノード数を算出 */
		if ( apz_node->mc_swap ) {
			apz_node->ms_div = 0;
			for ( apz_line=apz_node->mpz_line; apz_line; apz_line=apz_line->mpz_next ) if ( apz_line->mpz_node->mc_swap ) apz_node->ms_div++;
		}
	}

	/* 接岸ライン帰着 */
	if ( apz_prim == spz_base ) {

		/* 接岸ライン後方エリアの帰着ノードにフラグをセット */
		spz_base->mc_done = 3;
		for ( apz_node=spz_qeen; apz_node!=spz_base; apz_node=apz_node->mpz1_sea[1] ) if ( apz_node->mc_swap == 4 ) apz_node->mc_done = 2;

		/* 基準ノードセット */
		apz_pin = spz_base;
		apz_arm = spz_base->mpz1_sea[0];
	}
	else
	/* 結界ルート帰着 */
	if ( apz_prim == spz_jack ) {

		/* 結界ルート後方エリアの帰着ノードにフラグをセット */
		spz_jack->mc_done = 3;
		for ( apz_node=spz_way; apz_node!=spz_jack; apz_node=apz_node->mpz_way1 ) if ( apz_node->mc_swap == 3 ) apz_node->mc_done = 2;

		/* 基準ノードセット */
		apz_pin = spz_jack;
		apz_arm = ( spz_jack == spz_way ? spz_base : spz_jack->mpz_way0 );
	}
	/* ＲＧガード迂回 */
	else {
		/* 近接帰着点の検索 */
		for ( apz_pin=apz_prim; apz_pin; apz_pin=apz_pin->mpz_way0 ) if ( apz_pin->mc_swap ) break;	// 後方の近接帰着点を開始点として採用
		for ( apz_fit=apz_prim; apz_fit; apz_fit=apz_fit->mpz_way1 ) if ( apz_fit->mc_swap ) break;	// 前方の近接帰着点を終了点として採用

		/* 結界ルート後方エリアの帰着ノードにフラグをセット */
		apz_pin->mc_done = 3;
		apz_fit->mc_done = 2;

		/* 基準ノードセット */
		apz_arm = ( apz_pin == spz_waz ? spz_base : apz_pin->mpz_way1 );
	}

	/* 再帰呼出し開始 */
	fv_RCbran(apz_pin,apz_arm);
}
/**----------------------------------------------------------------------------
＠ トレース枝刈の再帰処理 */
void
fv_RCbran(
T_node* apz_pin,	// <I>中心ノード
T_node* apz_arm)	// <I>周回探査起点ノード
/*
	  ○－－－－－●－－－－－◎
	apz_arm     apz_pin     apz_new

	中心ノードから周辺ノード接続をmpz1_spin[0]方向でトレースし、
	mc_swapノードのms_div値を減算しながら枝とループを刈る。
	固定ノードに到達した場合はmc_doneを伝播する。
	処理は周辺探査起点の次のノードから開始し、最後に周辺探査起点ノードを処理する。
----------------------------------------------------------------------------**/
{
	T_node* apz_new;
	T_node* apz_node;
	T_line* apz_line;

	/* 周回探査起点のライン抽出 */
	for ( apz_line=apz_pin->mpz_line; apz_line; apz_line=apz_line->mpz_next ) if ( apz_line->mpz_node == apz_arm ) break;

	/* 周回ラインで連鎖ルートをトレース */
	while( 1 ) {

		/* 分岐先の新規ノード確保 */
		apz_line = apz_line->mpz1_spin[0];
		apz_new = apz_line->mpz_node;

		/* 新規ノードの分岐処理が完了している場合は自身の枝数カウンタ減算のみ実施 */
		if ( apz_new->ms_div == 0 ) {
			apz_pin->ms_div--;
		}
		else
		/* 未処理分岐が残っている場合 */
		if ( apz_new->ms_div > 0 ) {

			/* 処理済み新規ノードに到達した場合は除去の可能性 */
			if ( apz_new->mpz_trac || apz_new->mc_done == 3 ) {

				/* 末端到達の可能性（新規ノードが探査起点） */
				if ( apz_new == apz_arm ) {
					if ( apz_pin->ms_div == 1 && apz_pin->mc_done == 0 ) apz_pin->mc_swap = 0; // スワップフラグ除去
				}
				else
				/* ループ形成の可能性（新規ノードが探査起点以外）*/
				if ( apz_pin->ms_div == 2 ) {

					/* ループ可能性チェック */
					for ( apz_node=apz_pin->mpz_trac; apz_node; apz_node=apz_node->mpz_trac ) {

						/* ループ確定 */
						if ( apz_node == apz_new ) {
							for ( apz_node=apz_pin; apz_node!=apz_new; apz_node=apz_node->mpz_trac ) apz_node->mc_swap = 0;	// スワップフラグ除去
							break;
						}
						if ( apz_node->ms_div != 2 || apz_node->mc_done ) break;	// 除去不可判定
					}
				}
			}

			/* 固定ノードに到達（固定フラグ付与） */
			if ( apz_pin->mc_swap && apz_new != apz_arm && apz_new->mc_done >= 2 ) {
				for ( apz_node=apz_pin; apz_node; apz_node=apz_node->mpz_trac ) {
					if ( apz_node->mc_done ) break;
					apz_node->mc_done = 1;
				}
			}

			/* 未処理の新規ノードは再帰呼出し */
			if ( apz_new->mpz_trac == NULL && apz_new->mc_done != 3 ) {
				apz_new->mpz_trac = apz_pin;	// 枝刈用の後戻ポインタ接続（処理済み判定兼用）
				fv_RCbran(apz_new,apz_pin);	// 再帰呼び出し
			}

			/* 自身の枝数カウンタ減算 */
			apz_pin->ms_div--;
		}

		/* 分岐の終了判定 */
		if ( apz_line->mpz_node == apz_arm ) break;
	}
}
/**----------------------------------------------------------------------------
＠ スワップ選別 */
void
fv_SwapMsk(
char ac_targ,		// <I>ターゲット色
char ac_swap)		// <I>スワップ色
/*
	与えられた後方エリアに存在するターゲット色のノードに対し、
	スワップ色で試行スワップを掛け、既にセットされているスワップフラグをクリアする。
	例えば、Ｙをターゲット色、Ｂをスワップ色とするＹＢ遮断を実施する際、
	本関数をコールする前にspz_baseからＹＲ試行スワップを行い、ＹＲ連鎖フラグを立てておく。
	次に、本関数を(Ｙ,Ｂ)としてコールすると、接岸ライン後方エリアに存在するＹノードから
	Ｂスワップ連鎖の調査が実施され、既にセットされているＹＲ連鎖のＹノードフラグがクリアされる。
	これにより、残されたＹＲ連鎖フラグのＹでＢスワップを掛けても後方エリアにＢが到達しないことが保証される。
----------------------------------------------------------------------------**/
{
	T_node* apz_node;
	T_node* apz_exam;

	/* スワップ組色セット */
	sc1_swap[0] = ac_targ;
	sc1_swap[1] = ac_swap;

	/* 接岸ライン後方（減色ノードを含まず） */
	for ( apz_node=spz_qeen; apz_node!=spz_base; apz_node=apz_node->mpz1_sea[1] ) {
		if ( apz_node->mc_col != ac_targ ) continue;
		for ( apz_exam=spz_node; apz_exam; apz_exam=apz_exam->mpz_next ) apz_exam->mc_done = 0;
		fv_RCmask(0,apz_node);
	}
}
/**----------------------------------------------------------------------------
＠ スワップ選別の再帰処理 */
void
fv_RCmask(
char ac_sw,		// <I>スワップ色フラグ(再帰起動コール時は０を指定)
T_node* apz_node)	// <I>対象ノード
/*
	実際のノード色は変更せず、mc_doneでスワップ済みを管理しながら、
	既にセットされているmc_swapをクリアしていく。
	コール前に対象のmc_doneを全クリアしておく必要がある。
	また、スワップ色はsc1_swap[0]および[1]にあらかじめセットしておく。
----------------------------------------------------------------------------**/
{
	T_line* apz_line;

	/* スワップ処理 */
	ac_sw = 1 - ac_sw;
	apz_node->mc_done = 1;
	apz_node->mc_swap = 0;

	/* 接続ラインで再帰呼び出し */
	for ( apz_line=apz_node->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
		if ( apz_line->mpz_node->mc_col != sc1_swap[ac_sw] ) continue;	// スワップ対象色ではないノード
		if ( apz_line->mpz_node->mc_done ) continue;			// 別ルートでのスワップで処理済み

		/* 再帰呼び出し */
		fv_RCmask(ac_sw,apz_line->mpz_node);
	}
}
/**----------------------------------------------------------------------------
＠ 試行スワップ */
void
fv_SwapFlg(
T_node* apz_pos,	// <I>起点ノード
char ac_col)		// <I>スワップ色
/*
----------------------------------------------------------------------------**/
{
	T_node* apz_node;

	sc1_swap[0] = apz_pos->mc_col;
	sc1_swap[1] = ac_col;
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) apz_node->mc_swap = 0;
	fv_RCflag(0,apz_pos);
}
/**----------------------------------------------------------------------------
＠ 試行スワップの再帰処理 */
void
fv_RCflag(
char ac_sw,		// <I>スワップ色フラグ(再帰起動コール時は０を指定)
T_node* apz_node)	// <I>対象ノード
/*
	実際のノード色は変更せず、mc_swapにスワップ結果色を格納する。
	コール前に対象のmc_swapを全クリアしておく必要がある。
	また、スワップ色はsc1_swap[0]および[1]にあらかじめセットしておく。
----------------------------------------------------------------------------**/
{
	T_line* apz_line;

	/* スワップ処理 */
	ac_sw = 1 - ac_sw;
	apz_node->mc_swap = sc1_swap[ac_sw];

	/* 接続ラインで再帰呼び出し */
	for ( apz_line=apz_node->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
		if ( apz_line->mpz_node->mc_col != sc1_swap[ac_sw] ) continue;	// スワップ対象色ではないノード
		if ( apz_line->mpz_node->mc_swap ) continue;			// 別ルートでのスワップで処理済み

		/* 再帰呼び出し */
		fv_RCflag(ac_sw,apz_line->mpz_node);
	}
}
/**----------------------------------------------------------------------------
＠ 確定スワップ */
void
fv_SwapExe(
T_node* apz_pos,	// <I>起点ノード
char ac_col)		// <I>スワップ色
/*
----------------------------------------------------------------------------**/
{
	T_node* apz_node;

	sc1_swap[0] = apz_pos->mc_col;
	sc1_swap[1] = ac_col;
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) apz_node->mc_done = 0;
	fv_RCswap(0,apz_pos);
}
/**----------------------------------------------------------------------------
＠ 確定スワップの再帰処理 */
void
fv_RCswap(
char ac_sw,		// <I>スワップ色フラグ(再帰起動コール時は０を指定)
T_node* apz_node)	// <I>対象ノード
/*
	実際のノード色を変更する。
	スワップ色は、sc1_swap[0]および[1]にあらかじめセットしておく。
	処理済みノードを判断するため、コール前にmc_doneフラグを全クリアしておく必要がある。
----------------------------------------------------------------------------**/
{
	T_line* apz_line;

	/* スワップ処理 */
	ac_sw = 1 - ac_sw;
	apz_node->mc_done = 1;
	apz_node->mc_col = sc1_swap[ac_sw];

	/* 接続ラインで再帰呼び出し */
	for ( apz_line=apz_node->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
		if ( apz_line->mpz_node->mc_col != sc1_swap[ac_sw] ) continue;	// スワップ対象色ではないノード
		if ( apz_line->mpz_node->mc_done ) continue;			// 別ルートでのスワップで処理済み

		/* 再帰呼び出し */
		fv_RCswap(ac_sw,apz_line->mpz_node);
	}
}
/**----------------------------------------------------------------------------
＠ 接続海岸線の禁則チェック */
char			// <R>禁則( 0:無 1:有 )
fc_GapCheck(
T_node* apz_add,	// <I>追加候補ノード
T_node** aqz_edge)	// <O>始終端ノード
/*
	禁則とは、与えられた追加候補を着色した際、既存の海岸線と新たな海岸線とで
	未着色ノードを取り囲んでしまう状況である。
	本関数は、接岸ライン始終端も抽出し引数で返却する。

			　　     apz_add
			　 sea[0]← ● →sea[1]
		    sea[1]		        sea[0]		┌─　←─┐
			↑			↑		│ sea[0] │
			│ 　sea[1]		│		│spin[0] │反時計回り
		edge[0] ◎ → 〇 → 〇 → 〇 → ◎ edge[1]	│  bank　│（数学での正回転）
			│ ← 　 ← 　 ← 　←  │		└─→　─┘
			│		 sea[0] │

	最終海岸線ノード間の禁則チェックは、その間の未着色ノードの存在有無で行う。
	それ以外の禁則チェックは、接続カウンタの一致判定で行う。
----------------------------------------------------------------------------**/
{
	int n;
	short as_can;
	T_node* apz_node;
	T_node* apz_exam;
	T_line* apz_line;
	T_line* apz_spin;
	T_line* apz1_lin[2];

	/* 着色済み代表接岸ノード抽出（内陸優先） */
	apz_exam = NULL;
	aqz_edge[0] = NULL;
	aqz_edge[1] = NULL;
	for ( apz_line=apz_add->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
		apz_node = apz_line->mpz_node;
		if ( apz_node->mc_col == 0 ) continue;

		/* 内陸 */
		if ( apz_node->mpz_bank == NULL ) {
			apz_exam = apz_line->mpz_node;
			apz_spin = apz_line;
			break;
		}
		/* 最終海岸線 */
		else {
			n = ( aqz_edge[0] ? 1 : 0 );
			aqz_edge[n] = apz_node;
			apz1_lin[n] = apz_line;
		}
	}

	/* 内陸の接岸ノードが存在しない場合 */
	if ( apz_exam == NULL ) {

		/* 始終端が逆セットされている可能性のある場合 */
		if ( aqz_edge[0]->mpz1_sea[1] != aqz_edge[1] ) {

			/* 始終端が直接接続されていない場合は間に未着色ノードが存在すると判定 */
			if ( aqz_edge[0]->mpz1_sea[0] != aqz_edge[1] ) return( 1 );

			/* 始終端の方向整合 */
			apz_node = aqz_edge[0]; aqz_edge[0] = aqz_edge[1]; aqz_edge[1] = apz_node;
			apz_line = apz1_lin[0]; apz1_lin[0] = apz1_lin[1]; apz1_lin[1] = apz_line;
		}

		/* 始終端間の未着色ノード存在チェック */
		apz_line = apz1_lin[0];
		while( 1 ) {
			apz_line = apz_line->mpz1_spin[0];
			if ( apz_line->mpz_node->mc_col == 0 ) return( 1 );
			if ( apz_line == apz1_lin[1] ) break;
		}
		return( 0 );
	}

	/* 左右方向の接岸探査 */
	as_can = -1;
	for ( n=0; n<=1; n++ ) {

		/* 代表ノードから開始 */
		apz_node = apz_exam;
		apz_line = apz_spin;
		while( 1 ) {

			/* 端点更新 */
			aqz_edge[n] = apz_node;
			as_can++;

			/* 探査停止条件 */
			apz_line = apz_line->mpz1_spin[1-n];
			if ( apz_node->mpz_bank || apz_line->mpz_node->mc_col == 0 ) break;

			/* 対象ノード更新 */
			apz_node = apz_line->mpz_node;
		}
	}

	/* 接続カウンタの一致チェック */
	if ( apz_add->ms_can == as_can ) return( 0 );
	return( 1 );
}
/**----------------------------------------------------------------------------
＠ 三国峠の切り離し */
void
fv_CutPass()
/*
	　　　　　　　　　　　　◎　　　　　◎　：切り離し後のノード
	　　　　　　　　　　　　　－－　－－	：切り離しラインの記憶(mpz_pine)
	◎＝＝●＝＝◎　　→

	　　　　　　　　　　　　　－－●－－	：切り離し三国峠(spz_pass)

----------------------------------------------------------------------------**/
{
	int m;
	char ac_cut;
	T_node* apz_node;
	T_node* apz_trim;
	T_node* apz_exam;
	T_node* apz_back;
	T_line* apz_line;
	T_line* apz_spin1;
	T_line* apz_spin0;

	/* 階層的に三国峠を除去 */
	spz_pass = NULL;
	ac_cut = 1;
	while( ac_cut ) {
		ac_cut = 0;
		apz_back = NULL;
		apz_node = spz_node;
		while( apz_node ) {

			/* 海岸線は対象外 */
			if ( apz_node->mpz_bank ) {
				apz_back = apz_node;
				apz_node = apz_node->mpz_next;
				continue;
			}

			/* ３枝ノードの確認 */
			for ( m=0,apz_line=apz_node->mpz_line; m<=3 && apz_line; apz_line=apz_line->mpz_next ) m++;

			/* 三国峠ではない場合 */
			if ( m != 3 ) {
				apz_back = apz_node;
				apz_node = apz_node->mpz_next;
				continue;
			}

			/* 切り離し処理準備 */
			ac_cut = 1;
			apz_trim = apz_node;
			apz_node = apz_node->mpz_next;

			/* 三国峠をノードリストから切り離し */
			*( apz_back ? &(apz_back->mpz_next) : &(spz_node) ) = apz_trim->mpz_next;

			/* 三国峠ノード移動 */
			apz_trim->mpz_next = spz_pass;
			spz_pass = apz_trim;

			/* ラインリストから切り離し */
			for ( apz_line=apz_trim->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
				apz_exam = apz_line->mpz_node;
				apz_spin0 = NULL;
				for ( apz_spin1=apz_exam->mpz_line; apz_spin1; apz_spin1=apz_spin1->mpz_next ) {
					if ( apz_spin1->mpz_node == apz_trim ) {
						*( apz_spin0 ? &(apz_spin0->mpz_next) : &(apz_exam->mpz_line) ) = apz_spin1->mpz_next;
						apz_spin1->mpz_next = apz_exam->mpz_pine;
						apz_exam->mpz_pine = apz_spin1;
						break;
					}
					apz_spin0 = apz_spin1;
				}
			}
		}
	}

	/* 周回ループの再生成 */
	if ( spz_pass ) fv_SetSpin();
}
/**----------------------------------------------------------------------------
＠ 三国峠の復帰 */
void
fv_PutPass()
/*
----------------------------------------------------------------------------**/
{
	T_node* apz_node;
	T_line* apz_line;

	/* 三国峠がない場合 */
	if ( spz_pass == NULL ) return;

	/* ノードリスト末尾の検出 */
	for ( apz_node=spz_node; apz_node->mpz_next; apz_node=apz_node->mpz_next );

	/* ノードリスト末尾に三国峠リストを接続 */
	apz_node->mpz_next = spz_pass;

	/* 三国峠ルートのクリア */
	spz_pass = NULL;

	/* 切り離しラインの復帰 */
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
		if ( apz_node->mpz_pine ) {
			for ( apz_line=apz_node->mpz_line; apz_line->mpz_next; apz_line=apz_line->mpz_next );
			apz_line->mpz_next = apz_node->mpz_pine;
			apz_node->mpz_pine = NULL;
		}
	}

	/* 周回ループの再生成 */
	fv_SetSpin();
}
/**----------------------------------------------------------------------------
＠ 周回ループ生成 =*/
void
fv_SetSpin()
/*
	Mを(INT_MAX)として、以下の回転値によりスピンを整列させる。
	同一回転値の場合は、枝の長さで比較する。
	ソーティング後、同一直線状に位置するノードは、接続順を参照して調整する。

		　　　　　Ｙ
		　　　　　│
		　　　　 M│M
		　　　　　│
		　2M　　　│　　　 0
		─────┼─────Ｘ
		　2M　　　│　　　4M
		　　　　　│
		　　　　3M│3M
		　　　　　│

----------------------------------------------------------------------------**/
{
	int m,n;
	int ai_x0,ai_y0;
	short as_lct;
	long long x,y;
	long long al_sin;
	long long al_mag;
	T_node* apz_node;
	T_node* apz_exam;
	T_line* apz_line;
	T_line* apz_buff;
	T_spin* apz_root;
	T_spin* apz_spin;
	T_spin* apz_s0;
	T_spin* apz_s1;
	T_spin* apz_t0;
	T_spin* apz_t1;

	/* 全ノードループ */
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {

		/* 基準座標確保 */
		ai_x0 = apz_node->mi_x;
		ai_y0 = apz_node->mi_y;

		/* 枝数のカウント */
		as_lct = 0;
		for ( apz_line=apz_node->mpz_line; apz_line; apz_line=apz_line->mpz_next ) as_lct++;

		/* ソート用配列を確保 */
		apz_root = (T_spin*)malloc(sizeof(T_spin)*as_lct);

		/* 各配列へデータセット */
		al_mag = INT_MAX;
		apz_spin = apz_root;
		for ( apz_line=apz_node->mpz_line; apz_line; apz_line=apz_line->mpz_next) {
			apz_spin->mpz_line = apz_line;
			x = apz_line->mpz_node->mi_x - ai_x0;
			y = apz_line->mpz_node->mi_y - ai_y0;

			apz_spin->ml_len = x*x + y*y;
			al_sin = al_mag*y*y/apz_spin->ml_len;
			if ( y >= 0 ) {
				if ( x >= 0 ) { apz_spin->ml_sin =	      al_sin; }
				else	      { apz_spin->ml_sin = 2*al_mag - al_sin; }
			}
			else {
				if ( x >= 0 ) { apz_spin->ml_sin = 4*al_mag - al_sin; }
				else	      { apz_spin->ml_sin = 2*al_mag + al_sin; }
			}

			apz_spin++;
		}

		/* スピンソート */
		qsort(&(*apz_root),as_lct,sizeof(T_spin),(F_cmp)fi_CmpSpin);

		/* 同一ライン回転値の調整 */
		apz_s0 = apz_root;
		for ( n=1; n<=as_lct; n++,apz_s0++ ) {

			/* 同一ライン回転値の抽出 */
			apz_s1 = apz_s0;
			for ( ; n<as_lct; n++,apz_s0++ ) if ( (apz_s0+1)->ml_sin != apz_s1->ml_sin ) break;

			/* 連続する同一ライン回転値の接続矛盾をチェック */
			if ( apz_s1 != apz_s0 ) {

				/* 起点に最も近いノードが次のノードと接続されているかをチェック */
				apz_exam = ( apz_s0==apz_root+as_lct-1 ? apz_root : apz_s0+1 )->mpz_line->mpz_node;
				for ( apz_line=apz_s0->mpz_line->mpz_node->mpz_line; apz_line; apz_line=apz_line->mpz_next ) if ( apz_line->mpz_node == apz_exam ) break;

				/* 接続矛盾の場合は同一ライン回転値の並びを逆順に組換え */
				if ( apz_line == NULL ) {
					apz_t0 = apz_s0;
					apz_t1 = apz_s1;
					for ( m=0; m<((apz_t0 - apz_t1) + 1)/2; m++ ) {
						apz_buff = apz_t1->mpz_line;
						apz_t1->mpz_line = apz_t0->mpz_line;
						apz_t0->mpz_line = apz_buff;
						apz_t1++;
						apz_t0--;
					}
				}
			}
		}

		/* ラインループポインタのセット */
		apz_s0 = &apz_root[as_lct-1];
		apz_s1 = apz_root;
		for ( n=0; n<as_lct; n++ ) {
			apz_s0->mpz_line->mpz1_spin[0] = apz_s1->mpz_line;
			apz_s1->mpz_line->mpz1_spin[1] = apz_s0->mpz_line;
			apz_s0 = apz_s1;
			apz_s1 = apz_s1++;
		}

		/* ソート用配列を解放 */
		free( apz_root );
	}
}
/**----------------------------------------------------------------------------
＠ スピン比較関数 =*/
int
fi_CmpSpin(
T_spin* apz_spin1,	// <I>比較構造体１
T_spin* apz_spin2)	// <I>比較構造体２
/*
----------------------------------------------------------------------------**/
{
	if ( apz_spin1->ml_sin < apz_spin2->ml_sin ) return(  1 );
	if ( apz_spin1->ml_sin > apz_spin2->ml_sin ) return( -1 );
	if ( apz_spin1->ml_len < apz_spin2->ml_len ) return(  1 );
	if ( apz_spin1->ml_len > apz_spin2->ml_len ) return( -1 );
	return( 0 );
}
/**----------------------------------------------------------------------------
＠ 塗分け結果の成否チェック */
short			// <R>結果( 0:成功 1:失敗ノードＩＤ )
fs_FailCheck()
/*
----------------------------------------------------------------------------**/
{
	char ac_col;
	T_node* apz_node;
	T_line* apz_line;

	/* 全ノードループ */
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {

		/* 着色失敗チェック */
		if ( !dx_Scope(1,apz_node->mc_col,4) ) return( apz_node->ms_id );

		/* 海岸線Ｂチェック */
		if ( apz_node->mpz_bank && apz_node->mc_col == 4 ) return( apz_node->ms_id );

		/* 色干渉チェック */
		ac_col = apz_node->mc_col;
		for ( apz_line = apz_node->mpz_line; apz_line; apz_line = apz_line->mpz_next ) {
			if ( apz_line->mpz_node->mc_col == ac_col ) return( apz_node->ms_id );
		}
	}

	return( 0 );
}
/**----------------------------------------------------------------------------
＠ バッチ実行 */
void
fv_Batch()
/*
	地図コードの入力欄に値が入っている場合は、そのコードから開始し、
	１ずつインクリメントしながら新しい地図を生成する。
	入力欄がブランクの場合は、開始コードを時刻から生成する。
----------------------------------------------------------------------------**/
{
	int n,m;
	int ai_cnt;
	int ai1_rep[2];
	int ai_map;
	short as_err;
	long al_fpos;
	DWORD ah_org;
	DWORD ah_code;
	WORD at1_dir[32];
	WORD at1_txt[MAX_PATH];
	DWORD t0,t1;
	DWORD s1,s2;
	DWORD u0,u1,u2;
	SYSTEMTIME az_time;
	FILE* apx_file;

	/* バッチモードＯＮ */
	sc_phase = 1;

	/* 地図クリア */
	wcscpy(st2_cont[0],L"0");
	wcscpy(st2_cont[1],L"0");
	fv_FreeMap();

	/* 生成ノード数の確保 */
	GetWindowText(GetDlgItem(sx_main,1004),st1_node,8);
	ai_cnt = fi_Range(3,_wtoi(st1_node),D_MaxNode);
	wsprintf(st1_node,L"%d",ai_cnt);
	SetWindowText(GetDlgItem(sx_main,1004),st1_node);

	/* 地図数の確保 */
	GetWindowText(GetDlgItem(sx_main,1011),st1_maps,8);
	ai_map = fi_Range(1,_wtoi(st1_maps),D_MaxMap);
	wsprintf(st1_maps,L"%d",ai_map);
	SetWindowText(GetDlgItem(sx_main,1011),st1_maps);

	/* 初期乱数コードのセット */
	GetWindowText(GetDlgItem(sx_main,1002),at1_txt,16);
	ah_org = ( at1_txt[0] ? (DWORD)wcstoll(at1_txt,NULL,16) : timeGetTime() );
	wsprintf(at1_txt,L"%08x",ah_org);
	SetWindowText(GetDlgItem(sx_main,1000),at1_txt);

	/* バッチ諸元情報 */
	wsprintf(st2_info[0],L"ノード数 (%d)   地図数 (%d)",ai_cnt,ai_map);
	wsprintf(st2_info[1],L"乱数コード ( %08x ) ～ ( %08x )",ah_org,ah_org+ai_map-1);

	/* ボタン抑止 */
	fv_GrayOut(-1);
	InvalidateRect(sx_disp,NULL,TRUE);
	UpdateWindow(sx_main);

	/* ファイル名作成 */
	GetLocalTime(&az_time);
	wsprintf(at1_dir,L"Log_%04d",az_time.wYear);
	GetModuleFileName(sx_inst,at1_txt,MAX_PATH);
	wsprintf(PathFindFileName(at1_txt),at1_dir);
	CreateDirectory(at1_txt,NULL);
	wsprintf(PathFindExtension(at1_txt),L"\\(%s)%s_@%02d%02d%02d%02d%02d.txt"
		,st1_node,st1_maps
		,az_time.wMonth,az_time.wDay
		,az_time.wHour,az_time.wMinute,az_time.wSecond);

	/* オープン */
	apx_file = _wfopen(at1_txt,L"wt");
	if ( apx_file == NULL ) return;

	/* 処理時間の測定開始 */
	s1 = 0;
	s2 = 0;
	t0 = timeGetTime();

	/* バッチループ */
	as_err = 0;
	al_fpos = 0;
	ai1_rep[0] = 0;
	ai1_rep[1] = 0;
	ah_code = ah_org;
	for ( n=1; n<=ai_map; n++,ah_code++ ) {

		/* 乱数コード更新 */
		srand( (DWORD)ah_code );
		wsprintf(st1_code,L"%08x",ah_code);

		/* ログ出力 */
		fseek(apx_file,al_fpos,SEEK_SET);
		fwprintf(apx_file,L"%08x(%d)	;最終地図",ah_code,ai_cnt);
		fflush(apx_file);

		/* 実行 */
		sc_step = 0;
		u0 = timeGetTime();
		fv_CreateMap(ai_cnt);
		u1 = timeGetTime();
		as_err = fs_Color(1);
		u2 = timeGetTime();
		s1 += u1 - u0;
		s2 += u2 - u1;

		/* 成否カウンタ更新 */
		m = ( as_err ? 1 : 0 );
		ai1_rep[m]++;
		wsprintf(st2_cont[m],L"%d",ai1_rep[m]);
		InvalidateRect(sx_disp,&sz1_rect[m],FALSE);
		UpdateWindow(sx_disp);

		/* 色干渉ノードＩＤ付加 */
		if ( as_err ) {
			fseek(apx_file,al_fpos,SEEK_SET);
			fwprintf(apx_file,L"%08x(%d)	;Error(%d)\n",ah_code,ai_cnt,as_err);
			al_fpos = ftell(apx_file);
		}
		else {
			fwprintf(apx_file,L"\n");
		}
		fflush(apx_file);

		/* 地図クリア */
		fv_FreeMap();
	}

	/* バッチモードＯＦＦ */
	sc_phase = 2;

	/* 処理時間の測定終了 */
	t1 = timeGetTime();
	t1 = ((t1 + (( t1<t0 ? UINT_MAX : 0 ) - t0)) + 500)/1000;
	wsprintf(st1_tim0,L"総処理時間 :  %02d 分 %02d 秒",t1/60,t1%60);
	wsprintf(st1_tim1,L"地図 : %8d (msec)",s1);
	wsprintf(st1_tim2,L"着色 : %8d (msec)",s2);

	/* 結果の出力 */
	fwprintf(apx_file,L"%08x(%d)	;初期地図\n",ah_org,ai_cnt);
	fwprintf(apx_file,L"--------------------------\n");
	fwprintf(apx_file,L"ノード数 : %d\n",ai_cnt);
	fwprintf(apx_file,L"地図数 : %d\n",ai_map);
	fwprintf(apx_file,L"--------------------------\n");
	fwprintf(apx_file,L"%s\n",st1_tim0);
	fwprintf(apx_file,L"%s\n",st1_tim1);
	fwprintf(apx_file,L"%s\n",st1_tim2);

	/* クローズ */
	fclose( apx_file );

	/* 後処理 */
	sc_step = 0;
	fv_GrayOut(1);

	/* 最終結果表示 */
	wcscpy(at1_txt,cpt1_rep[( ai1_rep[1] ? 1 : 0 )]);
	SetWindowText(GetDlgItem(sx_main,1006),at1_txt);
	InvalidateRect(sx_disp,NULL,TRUE);
	UpdateWindow(sx_main);
}
/**----------------------------------------------------------------------------
＠ 地図生成 */
void
fv_CreateMap(
int ai_nct)	// <I>生成ノード数
/*
----------------------------------------------------------------------------**/
{
	int n;
	char sts;
	T_node* apz1_big[3];
	T_tang* apz_pot0;
	T_tang* apz_pot1;
	T_tang* apz_tang;
	T_node* apz_node;
	T_line* apz_line;
	T_line* apz_next;

	/* 現在の地図を解放 */
	fv_FreeMap();

	/* ノード生成 */
	fv_MakeNode(ai_nct);

	/* 外郭３ノードの作成 */
	for ( n=0; n<=2; n++ ) {
		apz1_big[n] = (T_node*)malloc(sizeof(T_node));
		apz1_big[n]->mpz_line = NULL;
		apz1_big[n]->ms_id = -(n+1);
	}

	/* 外郭三角面の座標 */
	n = D_Scale/4;
	apz1_big[0]->mi_x = -n*3; apz1_big[0]->mi_y = -n;
	apz1_big[1]->mi_x =  n*7; apz1_big[1]->mi_y = -n;
	apz1_big[2]->mi_x =  n*2; apz1_big[2]->mi_y =  n*8;

	/* 外郭三角面の作成 */
	spz_tang = NULL;
	apz_tang = fpz_MakeTang(apz1_big[0],apz1_big[1],apz1_big[2]);

	/* 接触三角面の初期化 */
	apz_tang->mpz1_meet[0] = NULL;
	apz_tang->mpz1_meet[1] = NULL;
	apz_tang->mpz1_meet[2] = NULL;

	/* 地図生成ループ */
	n = 0;
	spz_invs = NULL;
	for ( spz_add=spz_node; spz_add; spz_add=spz_add->mpz_next ) {

		/* ＩＤ付与 */
		n++;
		spz_add->ms_id = n;

		/* 包囲三角面の検出ループ */
		apz_pot0 = NULL;
		apz_pot1 = NULL;
		for ( apz_tang=spz_tang; apz_tang; apz_tang=apz_tang->mpz_next ) {

			/* 三角面の外接矩形での内外チェック */
			if ( spz_add->mi_x < apz_tang->mi_x1 || spz_add->mi_x > apz_tang->mi_x2 ) continue;
			if ( spz_add->mi_y < apz_tang->mi_y1 || spz_add->mi_y > apz_tang->mi_y2 ) continue;

			sts = fc_Inclusion(spz_add,apz_tang->mpz1_node);
			if ( sts < 0 ) continue;

			/* １つ目 */
			if ( apz_pot0 == NULL ) {
				apz_pot0 = apz_tang;
				if ( sts > 0 ) break;	// 内部は無条件に完了
			}
			/* ２つ目 */
			else {
				apz_pot1 = apz_tang;
				break;			 // 二つ目は無条件に完了
			}
		}

		/* 包囲三角面の分割 */
		fv_DivTang(apz_pot0,apz_pot1);

		/* 調査リストのループ */
		while( spz_invs ) {

			/* 調査リストからの取出 */
			apz_tang = spz_invs;
			spz_invs = spz_invs->mpz_list;
			apz_tang->mpz_list = NULL;

			/* 干渉チェック */
			sts = fc_CheckTang(apz_tang);

			/* フリップ処理 */
			if ( sts >= 0 ) fv_FlipTang(apz_tang,sts);
		}
	}

	/* 外郭三角面に接続されているノード情報を利用し最終海岸線にフラグを付与 */
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) apz_node->mc_done = 0;
	for ( n=0; n<=2; n++ ) {
		for ( apz_line=apz1_big[n]->mpz_line; apz_line; apz_line=apz_line->mpz_next ) apz_line->mpz_node->mc_done = 1;
	}

	/* 外郭三角面の接続ラインを除去解放 */
	for ( n=0; n<=2; n++ ) {
		apz_line = apz1_big[n]->mpz_line;
		while( apz_line ) {
			apz_next = apz_line->mpz_next;
			fv_DelLine(apz1_big[n],apz_line->mpz_node);
			apz_line = apz_next;
		}
	}

	/* 外郭３ノードを解放 */
	for ( n=0; n<=2; n++ ) free( apz1_big[n] );

	/* 三角面リスト解放 */
	while( spz_tang ) {
		apz_tang = spz_tang->mpz_next;
		free( spz_tang );
		spz_tang = apz_tang;
	}

	/* 最終海岸線の設定 */
	fv_SetBank();
}
/**----------------------------------------------------------------------------
＠ 包囲三角面の分割 */
void
fv_DivTang(
T_tang* apz_pot0,	// <I>包囲三角面０
T_tang* apz_pot1)	// <I>包囲三角面１
/*
	分割には以下の２系統がある。
		＜３分割＞　１つの包囲三角面内に追加ノードが存在
		＜４分割＞　２つの包囲三角面の共通辺上に追加ノードが存在
----------------------------------------------------------------------------**/
{
	int n;
	int c01,c02;
	int c11,c12;
	int ex0,ex1;
	T_tang* apz_meet;
	T_tang* apz_tang0;
	T_tang* apz_tang1;
	T_tang* apz_tang2;
	T_tang* apz_tang01;
	T_tang* apz_tang02;
	T_tang* apz_tang11;
	T_tang* apz_tang12;

	/* ３分割の場合 */
	if ( apz_pot1 == NULL ) {

		/* 接続ライン追加 */
		fv_MakeLine(spz_add,apz_pot0->mpz1_node[0]);
		fv_MakeLine(spz_add,apz_pot0->mpz1_node[1]);
		fv_MakeLine(spz_add,apz_pot0->mpz1_node[2]);

		/* ３分割三角面の追加 */
		apz_tang0 = fpz_MakeTang(spz_add,apz_pot0->mpz1_node[1],apz_pot0->mpz1_node[2]);
		apz_tang1 = fpz_MakeTang(spz_add,apz_pot0->mpz1_node[2],apz_pot0->mpz1_node[0]);
		apz_tang2 = fpz_MakeTang(spz_add,apz_pot0->mpz1_node[0],apz_pot0->mpz1_node[1]);

		/* 接触三角面のセット */
		apz_tang0->mpz1_meet[0] = apz_pot0->mpz1_meet[0];
		apz_tang1->mpz1_meet[0] = apz_pot0->mpz1_meet[1];
		apz_tang2->mpz1_meet[0] = apz_pot0->mpz1_meet[2];

		apz_tang0->mpz1_meet[1] = apz_tang1;
		apz_tang1->mpz1_meet[1] = apz_tang2;
		apz_tang2->mpz1_meet[1] = apz_tang0;

		apz_tang0->mpz1_meet[2] = apz_tang2;
		apz_tang1->mpz1_meet[2] = apz_tang0;
		apz_tang2->mpz1_meet[2] = apz_tang1;

		/* 相手の接触三角面ポインタを変更 */
		apz_meet = apz_pot0->mpz1_meet[0];  if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot0 ) { apz_meet->mpz1_meet[n] = apz_tang0; break; }
		apz_meet = apz_pot0->mpz1_meet[1];  if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot0 ) { apz_meet->mpz1_meet[n] = apz_tang1; break; }
		apz_meet = apz_pot0->mpz1_meet[2];  if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot0 ) { apz_meet->mpz1_meet[n] = apz_tang2; break; }

		/* 包囲三角面の削除 */
		fv_DelTang(apz_pot0);

		/* 調査リストへの格納 */
		apz_tang0->mpz_list = spz_invs;
		apz_tang1->mpz_list = apz_tang0;
		apz_tang2->mpz_list = apz_tang1;
		spz_invs = apz_tang2;
		return;
	}

	/* 処理対象の４頂点を分別 */
	for ( n=0; n<=2; n++ ) {
		if ( apz_pot0->mpz1_node[n] == apz_pot1->mpz1_node[0] ) { c01 = n; c11 = 0; }
		if ( apz_pot0->mpz1_node[n] == apz_pot1->mpz1_node[1] ) { c01 = n; c11 = 1; }
		if ( apz_pot0->mpz1_node[n] == apz_pot1->mpz1_node[2] ) { c01 = n; c11 = 2; }
		if ( apz_pot0->mpz1_meet[n] == apz_pot1 ) ex0 = n;
		if ( apz_pot1->mpz1_meet[n] == apz_pot0 ) ex1 = n;
	}
	c02 = 3 - (c01 + ex0);
	c12 = 3 - (c11 + ex1);

	/* ライン入替 */
	fv_FlipLine(apz_pot0->mpz1_node[c01],apz_pot0->mpz1_node[c02],apz_pot0->mpz1_node[c01],spz_add);

	/* 接続ライン追加 */
	fv_MakeLine(spz_add,apz_pot0->mpz1_node[c02]);
	fv_MakeLine(spz_add,apz_pot0->mpz1_node[ex0]);
	fv_MakeLine(spz_add,apz_pot1->mpz1_node[ex1]);

	/* ４分割三角面の追加 */
	apz_tang01 = fpz_MakeTang(spz_add,apz_pot0->mpz1_node[ex0],apz_pot0->mpz1_node[c01]);
	apz_tang02 = fpz_MakeTang(spz_add,apz_pot0->mpz1_node[ex0],apz_pot0->mpz1_node[c02]);
	apz_tang11 = fpz_MakeTang(spz_add,apz_pot1->mpz1_node[ex1],apz_pot1->mpz1_node[c11]);
	apz_tang12 = fpz_MakeTang(spz_add,apz_pot1->mpz1_node[ex1],apz_pot1->mpz1_node[c12]);

	/* 接触三角面のセット */
	apz_tang01->mpz1_meet[0] = apz_pot0->mpz1_meet[c02];
	apz_tang01->mpz1_meet[1] = apz_tang11;
	apz_tang01->mpz1_meet[2] = apz_tang02;

	apz_tang02->mpz1_meet[0] = apz_pot0->mpz1_meet[c01];
	apz_tang02->mpz1_meet[1] = apz_tang12;
	apz_tang02->mpz1_meet[2] = apz_tang01;

	apz_tang11->mpz1_meet[0] = apz_pot1->mpz1_meet[c12];
	apz_tang11->mpz1_meet[1] = apz_tang01;
	apz_tang11->mpz1_meet[2] = apz_tang12;

	apz_tang12->mpz1_meet[0] = apz_pot1->mpz1_meet[c11];
	apz_tang12->mpz1_meet[1] = apz_tang02;
	apz_tang12->mpz1_meet[2] = apz_tang11;

	/* 相手の接触三角面ポインタを変更 */
	apz_meet = apz_pot0->mpz1_meet[c01];  if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot0 ) { apz_meet->mpz1_meet[n] = apz_tang02; break; }
	apz_meet = apz_pot0->mpz1_meet[c02];  if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot0 ) { apz_meet->mpz1_meet[n] = apz_tang01; break; }
	apz_meet = apz_pot1->mpz1_meet[c11];  if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot1 ) { apz_meet->mpz1_meet[n] = apz_tang12; break; }
	apz_meet = apz_pot1->mpz1_meet[c12];  if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot1 ) { apz_meet->mpz1_meet[n] = apz_tang11; break; }

	/* 包囲三角面の削除 */
	fv_DelTang(apz_pot0);
	fv_DelTang(apz_pot1);

	/* 調査リストへの格納 */
	apz_tang01->mpz_list = spz_invs;
	apz_tang02->mpz_list = apz_tang01;
	apz_tang11->mpz_list = apz_tang02;
	apz_tang12->mpz_list = apz_tang11;
	spz_invs = apz_tang12;
}
/**----------------------------------------------------------------------------
＠ 三角面の干渉チェック */
char			// <R>接触三角面の孤立頂点番号(0,1,2) -1:干渉無し
fc_CheckTang(
T_tang* apz_tang)	// <I>チェック対象三角面
/*
	外郭三角形の頂点は無限遠に存在すると考えて処理する。
	したがって、髭や耳の発生を抑止するため、外接円判定の前に以下の処理を実施する。
	まず、孤立頂点のみ外郭三角面の頂点の場合はフリップしないと判定する。
	（すなわち、直線である共通辺が、外接円の一部であると認識して判定することになる）
	次に、共通頂点に外郭三角形の頂点が存在する場合、外接円の内外判定ではなく、
	フリップすることで生じる新しい三角面内に、もう片方の共通頂点が内在するか否かで判定する。
	外部に位置する場合は、フリップが必要と判定する。
	内部あるいは辺上に位置する場合は、通常の外接円チェックを実施する。
	（すなわち、直線であるフリップ線分が、外接円の一部であると認識して判定することになる）
----------------------------------------------------------------------------**/
{
	char ac_sol;
	double dx,dy;
	T_tang* apz_meet;
	T_node* apz_sol;
	T_node* apz_pin;
	T_node* apz1_box[3];

	/* 接触三角面が不在の場合(外郭三角面) */
	apz_meet = apz_tang->mpz1_meet[0];
	if ( apz_meet == NULL ) return( -1 );

	/* 孤立頂点のセット */
	for ( ac_sol=0; ac_sol<=2; ac_sol++ ) if ( apz_meet->mpz1_meet[ac_sol] == apz_tang ) break;
	apz_sol = apz_meet->mpz1_node[ac_sol];

	/* 孤立頂点のみ外郭三角面の頂点の場合 */
	if ( apz_sol->ms_id < 0 && apz_tang->mpz1_node[1]->ms_id > 0 && apz_tang->mpz1_node[2]->ms_id > 0 ) return( -1 );

	/* 共通頂点に外郭三角面頂点が存在するか確認 */
	apz_pin = NULL;
	if ( apz_tang->mpz1_node[1]->ms_id < 0 ) {
		apz_pin = apz_tang->mpz1_node[2];
		apz1_box[0] = apz_tang->mpz1_node[1];
	}
	else
	if ( apz_tang->mpz1_node[2]->ms_id < 0 ) {
		apz_pin = apz_tang->mpz1_node[1];
		apz1_box[0] = apz_tang->mpz1_node[2];
	}

	/* 共通頂点に外郭三角面頂点が存在する場合のフリップチェック */
	if ( apz_pin ) {
		apz1_box[1] = spz_add;
		apz1_box[2] = apz_sol;
		if ( fc_Inclusion(apz_pin,apz1_box) < 0 ) return ( ac_sol );
	}

	/* 孤立頂点の三角面外接円内チェック */
	dx = (double)(apz_tang->md_x - apz_sol->mi_x);
	dy = (double)(apz_tang->md_y - apz_sol->mi_y);
	if ( apz_tang->md_r > sqrt(dx*dx + dy*dy) ) return( ac_sol );

	return( -1 );
}
/**----------------------------------------------------------------------------
＠ 三角面フリップ */
void
fv_FlipTang(
T_tang* apz_pot0,	// <I>フリップ対象三角面
char ac_num)		// <I>フリップ相手の孤立頂点番号
/*

		     meet[2]		      spz_add
	spz_add ●───────〇		●───────〇 [e1]
		│[0] 　　 [1]／		│[0] 　　 [1]／│
		│　　　　　／			│　　　　　／　│
	 meet[1]│　pot0　／		→	│　　　　／　　│
		│　　　／ meet[0]		│　　　／　　　│
		│[2] ／			│[2] ／　　　　│
		│　／				│　／　　pot1　│
		│／			    [e2]│／　　　　　　│
		〇				〇───────◎ apz_sol

----------------------------------------------------------------------------**/
{
	int n;
	char e1,e2;
	T_node* apz_sol;
	T_tang* apz_pot1;
	T_tang* apz_tang1;
	T_tang* apz_tang2;
	T_tang* apz_meet;

	/* フィリップ相手の情報セット */
	apz_pot1 = apz_pot0->mpz1_meet[0];
	apz_sol = apz_pot1->mpz1_node[ac_num];
	for ( n=0; n<=2; n++ ) {
		if ( apz_pot1->mpz1_node[n] == apz_pot0->mpz1_node[1] ) e1 = n;
		if ( apz_pot1->mpz1_node[n] == apz_pot0->mpz1_node[2] ) e2 = n;
	}

	/* ライン入替 */
	fv_FlipLine(apz_pot0->mpz1_node[1],apz_pot0->mpz1_node[2],spz_add,apz_sol);

	/* 新しい三角面の追加 */
	apz_tang1 = fpz_MakeTang(spz_add,apz_sol,apz_pot0->mpz1_node[1]);
	apz_tang2 = fpz_MakeTang(spz_add,apz_sol,apz_pot0->mpz1_node[2]);

	/* 接触三角面のセット */
	apz_tang1->mpz1_meet[0] = apz_pot1->mpz1_meet[e2];
	apz_tang1->mpz1_meet[1] = apz_pot0->mpz1_meet[ 2];
	apz_tang1->mpz1_meet[2] = apz_tang2;

	apz_tang2->mpz1_meet[0] = apz_pot1->mpz1_meet[e1];
	apz_tang2->mpz1_meet[1] = apz_pot0->mpz1_meet[ 1];
	apz_tang2->mpz1_meet[2] = apz_tang1;

	/* 相手の接触三角面ポインタを変更 */
	apz_meet = apz_pot0->mpz1_meet[ 1]; if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot0 ) { apz_meet->mpz1_meet[n] = apz_tang2; break; }
	apz_meet = apz_pot0->mpz1_meet[ 2]; if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot0 ) { apz_meet->mpz1_meet[n] = apz_tang1; break; }
	apz_meet = apz_pot1->mpz1_meet[e1]; if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot1 ) { apz_meet->mpz1_meet[n] = apz_tang2; break; }
	apz_meet = apz_pot1->mpz1_meet[e2]; if ( apz_meet ) for ( n=0; n<=2; n++ ) if ( apz_meet->mpz1_meet[n] == apz_pot1 ) { apz_meet->mpz1_meet[n] = apz_tang1; break; }

	/* 元の三角面を削除 */
	fv_DelTang(apz_pot0);
	fv_DelTang(apz_pot1);

	/* 調査リストへの格納 */
	apz_tang1->mpz_list = spz_invs;
	apz_tang2->mpz_list = apz_tang1;
	spz_invs = apz_tang2;
}
/**----------------------------------------------------------------------------
＠ 三角面の生成 */
T_tang*			// <R>生成した三角面
fpz_MakeTang(
T_node* apz_p1,		// <I>頂点ノード１
T_node* apz_p2,		// <I>頂点ノード２
T_node* apz_p3)		// <I>頂点ノード３
/*
----------------------------------------------------------------------------**/
{
	int n;
	double a,b,c;
	double x1,y1;
	double x2,y2;
	double x3,y3;
	T_tang* apz_tang;

	/* 生成 */
	apz_tang = (T_tang*)malloc(sizeof(T_tang));

	/* 頂点ノードセット */
	apz_tang->mpz1_node[0] = apz_p1;
	apz_tang->mpz1_node[1] = apz_p2;
	apz_tang->mpz1_node[2] = apz_p3;

	x1 = apz_p1->mi_x; y1 = apz_p1->mi_y;
	x2 = apz_p2->mi_x; y2 = apz_p2->mi_y;
	x3 = apz_p3->mi_x; y3 = apz_p3->mi_y;

	a = x2*x2 - x1*x1 + y2*y2 - y1*y1;
	b = x3*x3 - x1*x1 + y3*y3 - y1*y1;
	c = 2*( (x2 - x1)*(y3 - y1) - (y2 - y1)*(x3 - x1) );

	/* 外接円の中心座標をセット */
	apz_tang->md_x = ( a*(y3 - y1) + b*(y1 - y2) )/c;
	apz_tang->md_y = ( a*(x1 - x3) + b*(x2 - x1) )/c;

	/* 外接円の半径をセット */
	x1 = x2 - apz_tang->md_x;
	y1 = y2 - apz_tang->md_y;
	apz_tang->md_r = sqrt( x1*x1 + y1*y1 );

	/* 外接矩形のセット */
	apz_tang->mi_x1 = apz_tang->mi_y1 = INT_MAX;
	apz_tang->mi_x2 = apz_tang->mi_y2 = INT_MIN;
	for ( n=0; n<=2; n++ ) {
		if ( apz_tang->mi_x1 > apz_tang->mpz1_node[n]->mi_x ) { apz_tang->mi_x1 = apz_tang->mpz1_node[n]->mi_x; }
		if ( apz_tang->mi_y1 > apz_tang->mpz1_node[n]->mi_y ) { apz_tang->mi_y1 = apz_tang->mpz1_node[n]->mi_y; }
		if ( apz_tang->mi_x2 < apz_tang->mpz1_node[n]->mi_x ) { apz_tang->mi_x2 = apz_tang->mpz1_node[n]->mi_x; }
		if ( apz_tang->mi_y2 < apz_tang->mpz1_node[n]->mi_y ) { apz_tang->mi_y2 = apz_tang->mpz1_node[n]->mi_y; }
	}

	/* リスト先頭に挿入 */
	apz_tang->mpz_next = spz_tang;
	apz_tang->mpz_back = NULL;
	if ( spz_tang ) spz_tang->mpz_back = apz_tang;
	spz_tang = apz_tang;

	return( apz_tang );
}
/**----------------------------------------------------------------------------
＠ 三角面の削除 */
void
fv_DelTang(
T_tang* apz_tang)	// <I>削除する三角面
/*
----------------------------------------------------------------------------**/
{
	*( apz_tang->mpz_back ? &apz_tang->mpz_back->mpz_next : &spz_tang ) = apz_tang->mpz_next;
	if ( apz_tang->mpz_next ) apz_tang->mpz_next->mpz_back = apz_tang->mpz_back;

	free( apz_tang );
}
/**----------------------------------------------------------------------------
＠ 最終海岸線の設定 */
void
fv_SetBank()
/*
	周回ループの生成を行い、スピン回転値を足掛かりに
	spz_bankのループポインタを構築する。
	なお、最終海岸線ノードにのみmc_doneフラグが立っていることを前提とする。
----------------------------------------------------------------------------**/
{
	int n,m;
	int x0,y0;
	long long x,y,s;
	long long al_sin;
	long long al_mag;
	T_node* apz_pin;
	T_node* apz_arm;
	T_node* apz_new;
	T_node* apz_node;
	T_line* apz_line;

	/* 周回ループの生成 */
	fv_SetSpin();

	/* Ｙ座標が最も小さいノードをピンに設定 */
	m = INT_MAX;
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
		if ( apz_node->mc_done == 0 ) continue;		// 最終海岸線判定
		n = apz_node->mi_y;
		if ( m > n ) {
			m = n;
			apz_pin = apz_node;
		}
	}

	/* ピン周辺の各ノードのスピン値を比較しアームを決定 */
	s = LLONG_MAX;
	al_mag = INT_MAX;
	x0 = apz_pin->mi_x;
	y0 = apz_pin->mi_y;
	for ( apz_line=apz_pin->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
		apz_node = apz_line->mpz_node;
		x = apz_node->mi_x - x0;
		y = apz_node->mi_y - y0;
		if ( apz_node->mc_done == 0 ) continue;		// 最終海岸線判定

		/* スピン値の算出 */
		al_sin = (al_mag*y*y)/(x*x+y*y);
		if ( x < 0 ) al_sin = 2*al_mag - al_sin;

		/* 最小スピン値の更新 */
		if ( s > al_sin ) {
			s = al_sin;
			apz_arm = apz_node;
		}
	}

	/* 最終海岸線ポインタ構築 */
	spz_bank = apz_pin;
	while( 1 ) {
		for ( apz_line=apz_pin->mpz_line; apz_line; apz_line=apz_line->mpz_next ) if ( apz_line->mpz_node == apz_arm ) break;
		while( 1 ) {
			apz_line = apz_line->mpz1_spin[0];
			apz_new = apz_line->mpz_node;
			if ( apz_new->mc_done ) break;		// 最終海岸線判定
		}
		apz_pin->mpz_bank = apz_new;
		if ( apz_new == spz_bank ) break;
		apz_arm = apz_pin;
		apz_pin = apz_new;
	}
}
/**----------------------------------------------------------------------------
＠ ノード生成 */
void
fv_MakeNode(
int ai_nct)	// <I>生成数
/*
----------------------------------------------------------------------------**/
{
	int n;
	int x,y;
	T_node* apz_node;
	T_node* apz_pos;

	/* ノードメモリ一括確保 */
	spz_root = (T_node*)malloc(sizeof(T_node)*ai_nct);

	/* ノードループ */
	apz_pos = spz_root;
	for ( n=1; n<=ai_nct; n++ ) {

		/* 座標調整のダミーループ */
		while( 1 ) {
			x = MulDiv(rand(),D_Scale,RAND_MAX);
			y = MulDiv(rand(),D_Scale,RAND_MAX);

			/* ＸＹ座標が既生成ノードと１以下で近接している場合は乱数生成を再実施 */
			for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
				if ( abs(x - apz_node->mi_x) <= 1 && abs(y - apz_node->mi_y) <= 1 ) break;
			}
			if ( apz_node ) continue;

			/* ３番ノードが１番２番と直線上の場合は再実施 */
			if ( n == 3 ) {
				if ( ( x - spz_node->mi_x )*( y - spz_node->mpz_next->mi_y ) ==
				     ( y - spz_node->mi_y )*( x - spz_node->mpz_next->mi_x ) ) continue;
			}
			break;
		}

		apz_pos->mpz_line = NULL;
		apz_pos->mpz_pine = NULL;
		apz_pos->mpz_bank = NULL;
		apz_pos->mi_x = x;
		apz_pos->mi_y = y;
		apz_pos->ms_id = 0;
		apz_pos->ms_can = 0;
		apz_pos->mc_col = 0;
		apz_pos->mc1_area[0] = 0;
		apz_pos->mc1_area[1] = 0;
		apz_pos->mi_dist = fi_Distance(D_Scale/2,D_Scale/2,x,y);

		apz_pos->mpz_next = spz_node;
		spz_node = apz_pos;

		apz_pos++;
	}
}
/**----------------------------------------------------------------------------
＠ ライン生成 */
void
fv_MakeLine(
T_node* apz_n1,	// <I>接続ノード１
T_node* apz_n2)	// <I>接続ノード２
/*
	双方向ポインタも構築する。
----------------------------------------------------------------------------**/
{
	T_line* apz_line;

	apz_line = (T_line*)malloc(sizeof(T_line));
	apz_line->mpz_node = apz_n2;
	apz_line->mpz_next = apz_n1->mpz_line;
	apz_n1->mpz_line = apz_line;

	apz_line = (T_line*)malloc(sizeof(T_line));
	apz_line->mpz_node = apz_n1;
	apz_line->mpz_next = apz_n2->mpz_line;
	apz_n2->mpz_line = apz_line;
}
/**----------------------------------------------------------------------------
＠ ライン除去 */
void
fv_DelLine(
T_node* apz_n1,	// <I>接続ノード１
T_node* apz_n2)	// <I>接続ノード２
/*
	双方向ポインタも除去する。
----------------------------------------------------------------------------**/
{
	T_line* apz_line;
	T_line* apz_back;

	apz_back = NULL;
	for ( apz_line=apz_n1->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
		if ( apz_line->mpz_node == apz_n2 ) {
			if ( apz_back ) {
				apz_back->mpz_next = apz_line->mpz_next;
			}
			else {
				apz_n1->mpz_line = apz_line->mpz_next;
			}
			free(apz_line);
			break;
		}
		apz_back = apz_line;
	}

	apz_back = NULL;
	for ( apz_line=apz_n2->mpz_line; apz_line; apz_line=apz_line->mpz_next ) {
		if ( apz_line->mpz_node == apz_n1 ) {
			if ( apz_back ) {
				apz_back->mpz_next = apz_line->mpz_next;
			}
			else {
				apz_n2->mpz_line = apz_line->mpz_next;
			}
			free(apz_line);
			break;
		}
		apz_back = apz_line;
	}
}
/**----------------------------------------------------------------------------
＠ ライン入替 */
void
fv_FlipLine(
T_node* apz_d1,	// <I>旧ノード１
T_node* apz_d2,	// <I>旧ノード２
T_node* apz_a1,	// <I>新ノード１
T_node* apz_a2)	// <I>新ノード２
/*
----------------------------------------------------------------------------**/
{
	T_line* apz_lin1;
	T_line* apz_lin2;
	T_line* apz_back;

	apz_back = NULL;
	for ( apz_lin1=apz_d1->mpz_line; apz_lin1; apz_lin1=apz_lin1->mpz_next ) {
		if ( apz_lin1->mpz_node == apz_d2 ) {
			if ( apz_back ) {
				apz_back->mpz_next = apz_lin1->mpz_next;
			}
			else {
				apz_d1->mpz_line = apz_lin1->mpz_next;
			}
			break;
		}
		apz_back = apz_lin1;
	}

	apz_back = NULL;
	for ( apz_lin2=apz_d2->mpz_line; apz_lin2; apz_lin2=apz_lin2->mpz_next ) {
		if ( apz_lin2->mpz_node == apz_d1 ) {
			if ( apz_back ) {
				apz_back->mpz_next = apz_lin2->mpz_next;
			}
			else {
				apz_d2->mpz_line = apz_lin2->mpz_next;
			}
			break;
		}
		apz_back = apz_lin2;
	}

	apz_lin1->mpz_node = apz_a2;
	apz_lin1->mpz_next = apz_a1->mpz_line;
	apz_a1->mpz_line = apz_lin1;

	apz_lin2->mpz_node = apz_a1;
	apz_lin2->mpz_next = apz_a2->mpz_line;
	apz_a2->mpz_line = apz_lin2;
}
/**----------------------------------------------------------------------------
＠ 地図クリア */
void
fv_FreeMap()
/*
----------------------------------------------------------------------------**/
{
	T_line* apz_line0;
	T_line* apz_line1;

	/* 離脱先の三国峠ライン解放 */
	while( spz_pass ) {
		apz_line0 = spz_pass->mpz_line;
		while( apz_line0 ) {
			apz_line1 = apz_line0->mpz_next;
			free(apz_line0);
			apz_line0 = apz_line1;
		}
		spz_pass = spz_pass->mpz_next;
	}

	/* ライン解放用のノードループ */
	while( spz_node ) {

		/* 離脱元の三国峠ライン */
		apz_line0 = spz_node->mpz_pine;
		while( apz_line0 ) {
			apz_line1 = apz_line0->mpz_next;
			free(apz_line0);
			apz_line0 = apz_line1;
		}

		/* 通常ライン */
		apz_line0 = spz_node->mpz_line;
		while( apz_line0 ) {
			apz_line1 = apz_line0->mpz_next;
			free(apz_line0);
			apz_line0 = apz_line1;
		}

		/* 更新 */
		spz_node = spz_node->mpz_next;
	}

	/* ノード解放 */
	if ( spz_root ) {
		free(spz_root);
		spz_root = NULL;
	}

	/* 最終海岸線ルートのクリア */
	spz_bank = NULL;

	/* 制御データクリア */
	fv_Clear(0);
}
/**----------------------------------------------------------------------------
＠ 制御データクリア */
void
fv_Clear(
char ac_sw)	// <I>クリア範囲( 0:制御系データのみ 1:ノード内データ含む )
/*
	spz_bankはクリアしない
----------------------------------------------------------------------------**/
{
	T_node* apz_node;

	/* 制御データクリア */
	sc_fine = 0;
	spz_add = NULL;
	spz_sea = NULL;

	/* 描画データクリア */
	sc_lock = 0;
	spz_base = NULL;
	spz_king = NULL;
	spz_qeen = NULL;
	spz_jack = NULL;
	spz_way  = NULL;
	spz_waz  = NULL;

	if ( ac_sw == 0 ) return;

	/* ノード内データクリア */
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) {
		apz_node->mc_col = 0;
		apz_node->ms_can = 0;
		apz_node->mc1_area[0] = 0;
		apz_node->mc1_area[1] = 0;
	}
}
/**----------------------------------------------------------------------------
＠ 点と三角形の内外判定 */
char		// <R>重なり状況( -1:外 0:辺上 1:内 )
fc_Inclusion(
T_node* apz_pin,	// <I>判定点ノード
T_node** apz1_box)	// <I>三角形ノード配列
/*
----------------------------------------------------------------------------**/
{
	double x0,y0;
	double x1,x2,x3;
	double y1,y2,y3;
	double a,b,c;

	x0 = apz_pin->mi_x;
	y0 = apz_pin->mi_y;

	x1 = apz1_box[0]->mi_x; y1 = apz1_box[0]->mi_y;
	x2 = apz1_box[1]->mi_x; y2 = apz1_box[1]->mi_y;
	x3 = apz1_box[2]->mi_x; y3 = apz1_box[2]->mi_y;

	a = (x2-x1)*(y0-y2) - (y2-y1)*(x0-x2);
	b = (x3-x2)*(y0-y3) - (y3-y2)*(x0-x3);
	c = (x1-x3)*(y0-y1) - (y1-y3)*(x0-x1);

	if ( ( a >  0 && b >  0 && c >  0 ) || ( a <  0 && b <  0 && c <  0 ) ) return( 1 );
	if ( ( a >= 0 && b >= 0 && c >= 0 ) || ( a <= 0 && b <= 0 && c <= 0 ) ) return( 0 );
	return( -1 );
}
/**----------------------------------------------------------------------------
＠ ２点間の距離 */
int		// <R>距離
fi_Distance(
int ai_x1,	// <I>座標Ｘ1
int ai_y1,	// <I>座標Ｙ1
int ai_x2,	// <I>座標Ｘ2
int ai_y2)	// <I>座標Ｙ2
/*
----------------------------------------------------------------------------**/
{
	double dx,dy;

	dx = ai_x2 - ai_x1;
	dy = ai_y2 - ai_y1;

	return( di_Round(sqrt(dx*dx + dy*dy)) );
}
/**----------------------------------------------------------------------------
＠ 範囲律則 */
int		// <R>律則後の値
fi_Range(
int ai_min,	// <I>下限値
int ai_val,	// <I>判定値
int ai_max)	// <I>上限値
/*
	上限と下限の大小関係の整合は、コール側で確保しておく必要がある。
----------------------------------------------------------------------------**/
{
	if ( ai_val <= ai_min ) return( ai_min );
	if ( ai_val >= ai_max ) return( ai_max );
	return( ai_val );
}
/**----------------------------------------------------------------------------
＠ 初期化ファイル読込 */
void
fv_IniLoad()
/*
----------------------------------------------------------------------------**/
{
	int n;
	WORD at2_txt[16][64];
	WORD* apt_pos;
	FILE* apx_file;

	apx_file = _wfopen(st1_ini,L"rt,ccs=UNICODE");
	if ( apx_file == NULL ) return;

	for ( n=0; n<=13; n++) {
		fgetws(at2_txt[n],32,apx_file);
		apt_pos = wcschr(at2_txt[n],'\n');
		if ( apt_pos ) *apt_pos = '\0';
	}
	fclose(apx_file);

	wcscpy(st1_code,at2_txt[0]);
	wcscpy(st1_node,at2_txt[1]);
	wcscpy(st1_maps,at2_txt[2]);

	sz_wpos.length		= _wtoi(at2_txt[3]);
	sz_wpos.flags		= _wtoi(at2_txt[4]);
	sz_wpos.showCmd		= _wtoi(at2_txt[5]);
	sz_wpos.ptMinPosition.x = _wtoi(at2_txt[6]);
	sz_wpos.ptMinPosition.y = _wtoi(at2_txt[7]);
	sz_wpos.ptMaxPosition.x = _wtoi(at2_txt[8]);
	sz_wpos.ptMaxPosition.y = _wtoi(at2_txt[9]);
	SetRect(&sz_wpos.rcNormalPosition,_wtoi(at2_txt[10]),_wtoi(at2_txt[11]),_wtoi(at2_txt[12]),_wtoi(at2_txt[13]));
}
/**----------------------------------------------------------------------------
＠ 初期化ファイル保存 */
void
fv_IniSave()
/*
----------------------------------------------------------------------------**/
{
	FILE* apx_file;

	GetWindowText(GetDlgItem(sx_main,1002),st1_code,16);
	GetWindowText(GetDlgItem(sx_main,1004),st1_node,16);
	GetWindowText(GetDlgItem(sx_main,1011),st1_maps,16);
	GetWindowPlacement(sx_main,&sz_wpos);

	apx_file = _wfopen(st1_ini,L"wt,ccs=UNICODE");
	if ( apx_file == NULL ) return;

	fwprintf(apx_file,
		L"%s\n%s\n%s\n"
		L"%d\n%d\n%d\n"
		L"%d\n%d\n"
		L"%d\n%d\n"
		L"%d\n%d\n%d\n%d\n"
		,st1_code,st1_node,st1_maps
		,sz_wpos.length,sz_wpos.flags,sz_wpos.showCmd
		,sz_wpos.ptMinPosition.x,sz_wpos.ptMinPosition.y
		,sz_wpos.ptMaxPosition.x,sz_wpos.ptMaxPosition.y
		,sz_wpos.rcNormalPosition.left,sz_wpos.rcNormalPosition.top,sz_wpos.rcNormalPosition.right,sz_wpos.rcNormalPosition.bottom);

	fclose(apx_file);
}
/**----------------------------------------------------------------------------
＠ エクスポート */
void
fv_Export()
/*
----------------------------------------------------------------------------**/
{
	int n,m;
	int ai_cnt0;
	int ai_cnt1;
	T_node* apz_node;
	T_line* apz_line;
	T_node** aqz_root0;
	T_node** aqz_pos0;
	T_node** aqz_root1;
	T_node** aqz_pos1;
	FILE* apx_file;
	WORD at1_name[MAX_PATH];

	if ( spz_node == NULL ) return;

	/* ノードのソート用にノード構造体アドレスの配列を確保 */
	ai_cnt0 = 0;
	for ( apz_node=spz_node; apz_node; apz_node=apz_node->mpz_next ) ai_cnt0++;
	aqz_root0 = (T_node**)malloc(sizeof(T_node*)*ai_cnt0);

	/* ノード配列にノード構造体アドレスをセット */
	for ( apz_node=spz_node,aqz_pos0=aqz_root0; apz_node; apz_node=apz_node->mpz_next,aqz_pos0++ ) *aqz_pos0 = apz_node;

	/* ソート関数の駆動 */
	qsort(aqz_root0,ai_cnt0,sizeof(T_node*),(F_cmp)fi_CmpID);

	/* ファイル名作成 */
	GetModuleFileName(sx_inst,at1_name,MAX_PATH);
	wsprintf(PathFindFileName(at1_name),L"Export");
	CreateDirectory(at1_name,NULL);
	wsprintf(PathFindExtension(at1_name),L"\\%s(%s).txt",st1_code,st1_node);

	/* オープン */
	apx_file = _wfopen(at1_name,L"wt,ccs=UNICODE");
	if ( apx_file == NULL ) return;

	/* ノード */
	for ( n=1,aqz_pos0=aqz_root0; n<=ai_cnt0; n++,aqz_pos0++ ) {
		fwprintf(apx_file,L"[%c]%5d (%04x,%04x) ",ct1_col[(*aqz_pos0)->mc_col],(*aqz_pos0)->ms_id,(*aqz_pos0)->mi_x,(*aqz_pos0)->mi_y);

		/* ラインのソート用にノード構造体アドレスの配列を確保 */
		ai_cnt1 = 0;
		for ( apz_line=(*aqz_pos0)->mpz_line; apz_line; apz_line=apz_line->mpz_next ) ai_cnt1++;
		aqz_root1 = (T_node**)malloc(sizeof(T_node*)*ai_cnt1);

		/* ライン配列にノード構造体アドレスをセット */
		for ( apz_line=(*aqz_pos0)->mpz_line,aqz_pos1=aqz_root1; apz_line; apz_line=apz_line->mpz_next,aqz_pos1++ ) *aqz_pos1 = apz_line->mpz_node;

		/* ソート関数の駆動 */
		qsort(aqz_root1,ai_cnt1,sizeof(T_node*),(F_cmp)fi_CmpID);

		/* ライン */
		for ( m=1,aqz_pos1=aqz_root1; m<=ai_cnt1; m++,aqz_pos1++ ) {
			fwprintf(apx_file,L"%d",(*aqz_pos1)->ms_id);
			fwprintf(apx_file,( m==ai_cnt1 ? L"\n" : L"," ));
		}

		/* ライン用ソート配列の解放 */
		free( aqz_root1 );
	}

	/* クローズ */
	fclose(apx_file);

	/* ノード用ソート配列の解放 */
	free( aqz_root0 );
}
/**----------------------------------------------------------------------------
＠ ＩＤソート比較関数 */
int			// <R>比較値
fi_CmpID(
T_node** aqz_node1,	// <I>比較構造体１
T_node** aqz_node2)	// <I>比較構造体２
/*
----------------------------------------------------------------------------**/
{
	if ( (*aqz_node1)->ms_id > (*aqz_node2)->ms_id ) return(  1 );
	if ( (*aqz_node1)->ms_id < (*aqz_node2)->ms_id ) return( -1 );
	return( 0 );
}