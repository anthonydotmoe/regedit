#pragma comment(lib,"User32.lib")
#pragma comment(lib,"Advapi32.lib")
#pragma comment(lib,"Comctl32.lib")

#include <windows.h>
#include <string.h>
#include <mbstring.h>
#include <commctrl.h>
#include "regedit.h"
#include "regtree.h"

#define MAX_KEY_LENGTH 255

struct WindowSize {
	int x;
	int y;
	int w;
	int h;
};

LPCTSTR		g_szClassName = L"Main window class";
LPCTSTR		g_szTitle = L"Main window";
HINSTANCE 	g_hInst = NULL;
HWND 		g_hwndMain = NULL;
HWND		g_hwndTreeView = NULL;
HWND		g_hwndSplitter = NULL;
HWND		g_hwndListView = NULL;
DWORD		g_dwSplitterPos = 0;
BOOL		g_bDragging = FALSE;
RegKeyTree	g_regkeytree;
HCURSOR		g_hcResize;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT handle_create(HWND, UINT, WPARAM, LPARAM);
void handle_resize(HWND hwnd);
HTREEITEM AddItemToTree(HWND, LPTSTR, int);
void DisplayErrorInMsgBox(DWORD);
void DisplayWindowsVersionBox();
void DisplayScreenSizeBox();
BOOL InitApplication(HINSTANCE);
BOOL InitInstance(HINSTANCE, int);
BOOL InitTreeViewItems(HWND hwndTV);
HWND CreateTreeView(HWND hwndParent);
HWND CreateListView(HWND hwndParent);
HWND CreateSplitter(HWND hwndParent);
BOOL InitListViewColumns(HWND hwndLV);
VOID QueryKey(HWND, HANDLE);
struct WindowSize GetTreeViewSize(DWORD dwSplitterPos, RECT *rcClient);
struct WindowSize GetListViewSize(DWORD dwSplitterPos, RECT *rcClient);

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	//DisplayScreenSizeBox();
	//DisplayWindowsVersionBox();

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

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static POINT ptPrev;
	
	switch (uMsg) {
		case WM_NOTIFY:
			if (((LPNMHDR)lParam)->idFrom == IDC_TREEVIEW) {
				LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
				switch (pnmtv->hdr.code) {
					case TVN_ITEMEXPANDING:
						{
							TVITEM item = pnmtv->itemNew;
							RegKeyNode *node = (RegKeyNode*)item.lParam;
							if(node != NULL) {
								// Query and add sub-keys if necessary
								RegKeyNode *subKey = regtree_CreateNode(&g_regkeytree, HKEY_CLASSES_ROOT, L"HKCR", node);
								regtree_AddChildNode(node, subKey);
							}
						}
						break;
					case TVN_SELCHANGED:
						// Handle selection change, possibly for updating other UI elements
						break;
				}
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

HTREEITEM AddItemToTree(HWND hwndTV, LPTSTR lpszItem, int nLevel) {
	TVITEM tvi;
	TVINSERTSTRUCT tvins;
	static HTREEITEM hPrev = (HTREEITEM)TVI_FIRST;
	static HTREEITEM hPrevRootItem = NULL;
	static HTREEITEM hPrevLev2Item = NULL;
	HTREEITEM hti;

	tvi.mask = TVIF_TEXT | TVIF_PARAM;

	// Set the text of the item
	tvi.pszText = lpszItem;
	tvi.cchTextMax = sizeof(tvi.pszText)/sizeof(tvi.pszText[0]);

	// Save the heading level in the item's application-defined
	// data area.
	tvi.lParam = (LPARAM)nLevel;
	tvins.item = tvi;
	tvins.hInsertAfter = hPrev;

	// Set the parent item based on the specified level
	if (nLevel == 1)
		tvins.hParent = TVI_ROOT;
	else if (nLevel == 2)
		tvins.hParent = hPrevRootItem;
	else
		tvins.hParent = hPrevLev2Item;

	// Add the item to the treeview control
	hPrev = (HTREEITEM)SendMessage(hwndTV, TVM_INSERTITEM, 0, (LPARAM)(LPTVINSERTSTRUCT)&tvins);

	if (hPrev == NULL)
		return NULL;

	// Save the handle to the item
	if (nLevel == 1)
		hPrevRootItem = hPrev;
	else if (nLevel == 2)
		hPrevLev2Item = hPrev;

	return hPrev;
}

BOOL InitTreeViewItems(HWND hwndTV) {
	
	regtree_Initialize(&g_regkeytree, hwndTV);

	// Add predefined keys as root nodes
	RegKeyNode *hklm = regtree_CreateNode(&g_regkeytree, HKEY_LOCAL_MACHINE, L"HKLM", NULL);
	g_regkeytree.root = hklm;

	RegKeyNode *hkcu = regtree_CreateNode(&g_regkeytree, HKEY_CURRENT_USER, L"HKCU", NULL);
	hklm->next = hkcu;


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

void DisplayScreenSizeBox() {
	LPCTSTR fmt = L"Resolution: (%dx%d)";

	// Get screen size
	int x, y;
	WCHAR wszMsgBuff[512];

	x = GetSystemMetrics(SM_CXSCREEN);
	y = GetSystemMetrics(SM_CYSCREEN);

	wsprintf(wszMsgBuff, fmt, x, y);

	MessageBox(0, wszMsgBuff, L"Info", 0);
}

void DisplayWindowsVersionBox() {
	LPCTSTR fmt = L"Hello from Windows CE %d.%d (%d)";
	WCHAR wszMsgBuff[512];

	// Get version
	OSVERSIONINFO osvi;
	GetVersionEx(&osvi);

	wsprintf(wszMsgBuff, fmt,
			osvi.dwMajorVersion,
			osvi.dwMinorVersion,
			osvi.dwBuildNumber
	);

	MessageBox(
			0,
			wszMsgBuff,
			L"Version",
			0
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
	wndclass.lpszMenuName = MAKEINTRESOURCE(IDM_MENUBAR);
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

VOID EnumerateLevel(HWND hWnd, LPWSTR NameLBSelect, LPWSTR RegPath, DWORD RegPathLength, HKEY *hKeyRoot) {
	HKEY	hKey;
	DWORD	retCode;
	WCHAR	Buf[MAX_PATH];
	WCHAR	lpBuffer[128];
	size_t	ByteCount = MAX_PATH - 1;

	if (*hKeyRoot) {
		// If RegPath is not NULL, then you have to add a backslash to
		// the path name before appending the next level child name.
		if (wcscmp(RegPath, L"") != 0) {
			WCHAR *pEndOfBuffer;
			wcsncat_s(RegPath, RegPathLength, L"\\", 1);
			pEndOfBuffer = (WCHAR*) _mbsrchr ((unsigned char*)&RegPath[0], '\\');
			ByteCount -= (pEndOfBuffer - RegPath);
		}

		// Add the next level child name.
		wcsncat_s (RegPath, RegPathLength, NameLBSelect, ByteCount);

		// Use RegOpenKeyEx() with the new registry path to get an open
		// handle to the child key you want to enumerate.
		retCode = RegOpenKeyEx (
			*hKeyRoot,
			RegPath,
			0,
			KEY_ENUMERATE_SUB_KEYS | KEY_EXECUTE | KEY_QUERY_VALUE,
			&hKey
		);

		if (retCode != ERROR_SUCCESS) {
			if (retCode == ERROR_ACCESS_DENIED) {
				// LoadString IDS_CANTOPENKEY and copy it to Buf
			}
			else {
				// LoadString IDS_OPENKEYERR and copy it to Buf
			}
			MessageBox (hWnd, Buf, L"", MB_OK);
			return;
		}
	}
	else {
		// Set the *hKeyRoothandle based on the text taken from the ListBox

		if (wcscmp (NameLBSelect, L"HKEY_CLASSES_ROOT") == 0)
			*hKeyRoot = HKEY_CLASSES_ROOT;
		if (wcscmp (NameLBSelect, L"HKEY_USERS") == 0)
			*hKeyRoot = HKEY_USERS;
		if (wcscmp (NameLBSelect, L"HKEY_LOCAL_MACHINE") == 0)
			*hKeyRoot = HKEY_LOCAL_MACHINE;
		if (wcscmp (NameLBSelect, L"HKEY_CURRENT_USER") == 0)
			*hKeyRoot = HKEY_CURRENT_USER;

		hKey = *hKeyRoot;

	}

	QueryKey(hWnd, hKey);

	RegCloseKey(hKey);

	//SetDlgItemText(hWnd, IDE_TEXTOUT, RegPath);
}

/*
 *
 */
VOID QueryKey(HWND hWnd, HANDLE hKey) {

	WCHAR		KeyName[MAX_PATH];		// Key name
	DWORD		dwcSubKeys;				// Number of subkeys.
	DWORD		dwcMaxSubKey;			// Longest subkey size.
	DWORD		dwcValues;				// Number of values for this key.
	DWORD		dwcMaxValueName;		// Longest Value name.
	DWORD		dwcMaxValueData;		// Longest Value data.
	//DWORD		dwcSecDesc;				// Security descriptor.
	//FILETIME	ftLastWriteTime;		// Last write time.

	DWORD i;
	DWORD retCode;

	DWORD		j;
	DWORD		retValue;
	WCHAR		ValueName[MAX_VALUE_NAME];
	DWORD		dwcValueName = MAX_VALUE_NAME;
	WCHAR		Buf[80];
	WCHAR		lpBuffer[80];
	DWORD		dwMaxPath;
	FILETIME	ftFileTime;

	// Get value count
	RegQueryInfoKey(
		hKey,
		NULL,				// UNUSED - Buffer for class name
		NULL,				// UNUSED - Length of class string
		NULL,				// Reserved.
		&dwcSubKeys,		// Number of subkeys
		&dwcMaxSubKey,		// Longest subkey size
		NULL,				// UNUSED - Longest class string
		&dwcValues,			// Number of values for this key
		&dwcMaxValueName,	// Longest value name
		&dwcMaxValueData,	// Longest value data
		NULL,				// UNUSED - Security descriptor &dwcSecDesc
		NULL				// IGNORED - Last write time
	);



}