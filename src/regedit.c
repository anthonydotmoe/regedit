#pragma comment(lib,"User32.lib")
#pragma comment(lib,"Advapi32.lib")
#pragma comment(lib,"Comctl32.lib")

#include <windows.h>
#include <string.h>
#include <mbstring.h>
#include <commctrl.h>
#include "regedit.h"
#include "regnode.h"

#define MAX_KEY_LENGTH 255


struct WindowSize {
	int x;
	int y;
	int w;
	int h;
};

LPCTSTR		g_szClassName = L"Main window class";
LPCTSTR		g_szTitle = L"regedit";
HINSTANCE 	g_hInst = NULL;
HWND 		g_hwndMain = NULL;
HWND		g_hwndTreeView = NULL;
HWND		g_hwndSplitter = NULL;
HWND		g_hwndListView = NULL;
DWORD		g_dwSplitterPos = 0;
BOOL		g_bDragging = FALSE;
HCURSOR		g_hcResize = NULL;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT handle_create(HWND, UINT, WPARAM, LPARAM);
void handle_resize(HWND hwnd);
void DisplayErrorInMsgBox(DWORD);
BOOL InitApplication(HINSTANCE);
BOOL InitInstance(HINSTANCE, int);
BOOL InitTreeViewItems(HWND hwndTV);
HWND CreateTreeView(HWND hwndParent);
HWND CreateListView(HWND hwndParent);
HWND CreateSplitter(HWND hwndParent);
BOOL InitListViewColumns(HWND hwndLV);
struct WindowSize GetTreeViewSize(DWORD dwSplitterPos, RECT *rcClient);
struct WindowSize GetListViewSize(DWORD dwSplitterPos, RECT *rcClient);

#ifdef UNDER_CE
#define WINMAIN WinMain
#else
#define WINMAIN wWinMain
#endif
int WINMAIN(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	if(!InitApplication(hInstance)) {
		MessageBox(0, L"RegisterClassW", L"Fail!", MB_ICONERROR);
		return FALSE;
	}

	if (!InitInstance(hInstance, nCmdShow)) {
		MessageBox(0, L"InitInstance", L"Fail!", MB_ICONERROR);
		return FALSE;
	}

	// Message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

LRESULT handle_treeview_notify(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
	static int useless = 0;

	switch (pnmtv->hdr.code) {
		case TVN_ITEMEXPANDING:
			// 1. Get the item that's expanding
			// 1. Delete all child items under it
			// 1. Query the node's subkeys and create a new node for each one
			
			// The expanding node
			RegNode *node;			
			node = (RegNode*) pnmtv->itemNew.lParam;
			
			// Break out if we aren't expanding
			if(!(pnmtv->action & TVE_EXPAND)) {
				break;
			}
			
			// Delete child items
			HTREEITEM child;
			while (child = TreeView_GetChild(g_hwndTreeView, node->htitem)) {
				TreeView_DeleteItem(g_hwndTreeView, child);
			}
			
			// TODO: Add subkeys
			regnode_Create(NULL, L"RegKey", g_hwndTreeView, node->htitem);

			break;
		case TVN_SELCHANGED:
			// Key selection has changed, update the listview with the values
			// for that key.
			break;
		case TVN_DELETEITEM:
		{
			// Item is being deleted, itemOld.hItem and itemOld.lParam members
			// contain valid information about the item being deleted.
			
			// pnmtv->itemOld.lParam will contain state information
			RegNode *regnode = (RegNode*)pnmtv->itemOld.lParam;
			regnode_Destroy(regnode);
		}
		break;
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static POINT ptPrev;
	
	switch (uMsg) {
		case WM_NOTIFY:
			if (((LPNMHDR)lParam)->idFrom == IDC_TREEVIEW) {
				return handle_treeview_notify(hwnd, wParam, lParam);
			}
			break;
		case WM_CREATE:
			return handle_create(hwnd, uMsg, wParam, lParam);
		case WM_SIZE:
			handle_resize(hwnd);
			break;
		
		// splitter bar
		case WM_SETCURSOR:	//TODO: This doesn't work
		{
			HWND hwndChild = (HWND)wParam;
			if (hwndChild == g_hwndSplitter) {
				SetCursor(g_hcResize);
				return TRUE;
			} else {
				return DefWindowProc(hwnd, uMsg, wParam, lParam);
			}
		}
		//break;
		case WM_LBUTTONDOWN:
		{
			POINTS pts = MAKEPOINTS(lParam);
			POINT pt = { pts.x, pts.y };
			HWND hwndChild = ChildWindowFromPoint(hwnd, pt);
			if (hwndChild == g_hwndSplitter) {
				g_bDragging = TRUE;
				SetCapture(hwnd);
				ptPrev.x = pts.x;
				ptPrev.y = pts.y;
			}
		}
		break;
		case WM_MOUSEMOVE:
			if(g_bDragging) {
				POINTS pts = MAKEPOINTS(lParam);
				POINT pt;
				pt.x = pts.x;
				pt.y = pts.y;
				RECT rcClient;
				GetClientRect(hwnd, &rcClient);
				
				int newSplitterPos = g_dwSplitterPos + (pt.x - ptPrev.x);
				if (newSplitterPos > 20 && newSplitterPos < rcClient.right - 20) {
					g_dwSplitterPos = newSplitterPos;
					ptPrev = pt;
					handle_resize(hwnd);
				}
			}
			break;
		case WM_LBUTTONUP:
			if(g_bDragging) {
				g_bDragging = FALSE;
				ReleaseCapture();
			}
			break;

		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			// Destroy the treeview tree
			TreeView_DeleteAllItems(g_hwndTreeView);

			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);

	}

	return 0;

}

LRESULT handle_create(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);
	
	g_dwSplitterPos = rcClient.right / 3;

	if (NULL == (g_hwndTreeView = CreateTreeView(hwnd))) {
		return -1;
	}

	if (NULL == (g_hwndSplitter = CreateSplitter(hwnd))) {
		return -1;
	}

	if (NULL == (g_hwndListView = CreateListView(hwnd))) {
		return -1;
	}

	return 0;
}

HWND CreateListView(HWND hwndParent) {
	RECT rcClient;
	HWND lv;

	GetClientRect(hwndParent, &rcClient);

	struct WindowSize lvs;

	lvs = GetListViewSize(g_dwSplitterPos, &rcClient);

	lv = CreateWindow(
			WC_LISTVIEW,
			L"",
			WS_VISIBLE | WS_CHILD | LVS_REPORT,
			lvs.x,
			lvs.y,
			lvs.w,
			lvs.h,
			hwndParent,
			(HMENU) IDC_LISTVIEW,
			g_hInst,
			NULL
	);

	if (lv == NULL) {
		return NULL;
	}

	if (!InitListViewColumns(lv)) {
		return NULL;
	}

	return lv;
}

BOOL InitListViewColumns(HWND hwndLV) {
	LVCOLUMN lvc;
	int iCol = 0;

	WCHAR lpszCol1[] = L"Name";
	WCHAR lpszCol2[] = L"Type";
	WCHAR lpszCol3[] = L"Data";

	// Initialize the LVCOLUMN structure.
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.cx = 100;

	// Add the columns.
	lvc.iSubItem = iCol++;
	lvc.pszText = lpszCol1;
	if (ListView_InsertColumn(hwndLV, 0, &lvc) == -1)
		return FALSE;
	lvc.iSubItem = iCol++;
	lvc.pszText = lpszCol2;
	if (ListView_InsertColumn(hwndLV, 0, &lvc) == -1)
		return FALSE;
	lvc.iSubItem = iCol++;
	lvc.pszText = lpszCol3;
	if (ListView_InsertColumn(hwndLV, 0, &lvc) == -1)
		return FALSE;

	return TRUE;
}


HWND CreateTreeView(HWND hwndParent) {
	RECT rcClient;
	HWND tv;

	GetClientRect(hwndParent, &rcClient);

	struct WindowSize tvs;
	tvs = GetTreeViewSize(g_dwSplitterPos, &rcClient);

	tv = CreateWindowEx (
			0,
			WC_TREEVIEW,
			L"Tree View",
			WS_VISIBLE | WS_CHILD | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
			tvs.x,
			tvs.y,
			tvs.w,
			tvs.h,
			hwndParent,
			(HMENU) IDC_TREEVIEW,
			g_hInst,
			NULL
	);

	if (tv == NULL) {
		return NULL;
	}

	if (!InitTreeViewItems(tv)) {
		return NULL;
	}

	return tv;
}

HWND CreateSplitter(HWND hwndParent) {
	RECT rcClient;
	HWND sb;
	
	GetClientRect(hwndParent, &rcClient);
	g_dwSplitterPos = rcClient.right / 3;
	
	sb = CreateWindow(
		L"STATIC",
		NULL,
		WS_CHILD | WS_BORDER | WS_VISIBLE | SS_LEFT,
		g_dwSplitterPos,
		0,
		SPLITTER_WIDTH,
		rcClient.bottom,
		hwndParent,
		(HMENU)IDC_SPLITTER,
		g_hInst,
		NULL
	);

	return sb;
}

struct WindowSize GetTreeViewSize(DWORD dwSplitterPos, RECT *rcClient) {
	struct WindowSize tvs;
	const int pad = CONTROL_PADDING;
	
	tvs.x = tvs.y = pad;
	tvs.w = dwSplitterPos - pad;
	tvs.h = rcClient->bottom - 2 * pad;
	
	return tvs;
}

struct WindowSize GetListViewSize(DWORD dwSplitterPos, RECT *rcClient) {
	struct WindowSize lvs;
	const int pad = CONTROL_PADDING;
	
	lvs.x = dwSplitterPos + SPLITTER_WIDTH;
	lvs.y = pad;
	lvs.w = rcClient->right - lvs.x - pad;
	lvs.h = rcClient->bottom - 2 * pad;
	
	return lvs;
}

void handle_resize(HWND hwnd) {
	const int pad = CONTROL_PADDING;
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);
	
	struct WindowSize lvs, tvs;
	int sb_x, sb_y, sb_w, sb_h;

	sb_x = g_dwSplitterPos;
	sb_y = pad;
	sb_w = SPLITTER_WIDTH,
	sb_h = rcClient.bottom - 2 * pad;
	
	lvs = GetListViewSize(g_dwSplitterPos, &rcClient);
	tvs = GetTreeViewSize(g_dwSplitterPos, &rcClient);

	MoveWindow(g_hwndTreeView, tvs.x, tvs.y, tvs.w, tvs.h, TRUE);
	MoveWindow(g_hwndSplitter, sb_x, sb_y, sb_w, sb_h, TRUE);
	MoveWindow(g_hwndListView, lvs.x, lvs.y, lvs.w, lvs.h, TRUE);
}

BOOL InitTreeViewItems(HWND hwndTV) {
	// Add the dummy root node "Computer"
	RegNode *dummy_root_node = regnode_Create(NULL, L"Computer", hwndTV, TVI_ROOT);

	// Add predefined keys as root nodes
	regnode_Create(HKEY_CLASSES_ROOT,	L"HKCR", hwndTV, dummy_root_node->htitem);
	regnode_Create(HKEY_CURRENT_USER,	L"HKCU", hwndTV, dummy_root_node->htitem);
	regnode_Create(HKEY_LOCAL_MACHINE,	L"HKLM", hwndTV, dummy_root_node->htitem);
	regnode_Create(HKEY_USERS,			L"HKU",  hwndTV, dummy_root_node->htitem);
	
	// Expand the Computer node
	TreeView_Expand(hwndTV, dummy_root_node->htitem, TVE_EXPAND);

	return TRUE;
}


void DisplayErrorInMsgBox(DWORD dwErr) {
	WCHAR wszMsgBuff[512];		// Buffer for text
	DWORD dwChars;			// Number of chars returned.

	// Try to get the message from the system error
	dwChars = FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dwErr,
			0,
			wszMsgBuff,
			512,
			NULL
	);

	MessageBox(
			0,
			dwChars ? wszMsgBuff : L"Error message not found.",
			L"Error",
			MB_ICONERROR
	);
}	

BOOL InitApplication(HINSTANCE hInstance) {
	// Init common controls
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES;
	if (!InitCommonControlsEx(&iccex)) {
		MessageBox(0, L"Couldn't init common controls", L"Error", MB_ICONERROR);
		return FALSE;
	}

	// Load resize cursor
	g_hcResize = LoadCursor(NULL, IDC_SIZEWE);
	WNDCLASS wndclass;

	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = (WNDPROC)WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hIcon = NULL;
	wndclass.hInstance = hInstance;
	wndclass.hCursor = NULL;
	wndclass.hbrBackground = (HBRUSH) COLOR_BACKGROUND + 1;
#ifdef UNDER_CE
	// lpszMenuName is not supported and must be NULL
	wndclass.lpszMenuName = NULL;
#else
	wndclass.lpszMenuName = MAKEINTRESOURCE(IDM_MENUBAR);
#endif
	wndclass.lpszClassName = g_szClassName;

	return RegisterClass(&wndclass);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {

	g_hInst = hInstance;

	g_hwndMain = CreateWindowEx(
			0,				// Optional window styles
			g_szClassName,			// Window class
			g_szTitle,			// Window text
			WS_OVERLAPPEDWINDOW,			// Window style

			// Size and position
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

			NULL,		// Parent window
			NULL,		// Menu
			hInstance,	// Instance handle
			NULL		// Additional application data
	);

	if (!g_hwndMain) {
		return FALSE;
	}

	// Show the window
	ShowWindow(g_hwndMain, nCmdShow);
	UpdateWindow(g_hwndMain);
	return TRUE;
}
