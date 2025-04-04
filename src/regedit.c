#include <windows.h>
#include <string.h>
#include <mbstring.h>
#include <commctrl.h>
#include <stdio.h>
#include "regedit.h"
#include "regnode.h"

#ifdef UNDER_CE
#include <sipapi.h>
#endif

LPCTSTR		g_szClassName = L"Main window class";
LPCTSTR		g_szTitle = L"regedit";
HINSTANCE 	g_hInst = NULL;
HANDLE	 	g_hProcessHeap = NULL;
HWND 		g_hwndMain = NULL;
HWND		g_hwndTreeView = NULL;
HWND		g_hwndSplitter = NULL;
HWND		g_hwndListView = NULL;
HWND		g_hwndCommandBar = NULL;
DWORD		g_dwSplitterPos = 0;
BOOL		g_bDragging = FALSE;
HCURSOR		g_hcResize = NULL;
HIMAGELIST	g_hImageList = NULL;
int         g_iCommandBarHeight = 0;

HIMAGELIST CreateImageList();
HWND CreateListView(HWND hwndParent);
HWND CreateSplitter(HWND hwndParent);
HWND CreateTreeView(HWND hwndParent);
void DisplayErrorInMsgBox(LPCWSTR, DWORD);
void FormatDwordForColumn(LPWSTR out, DWORD strlen, DWORD data);
RECT GetListViewSize(DWORD dwSplitterPos, RECT *rcClient);
RECT GetTreeViewSize(DWORD dwSplitterPos, RECT *rcClient);
void HandleTreeViewExpand(LPNMTREEVIEW pnmtv);
void HandleTreeViewSelection(LPNMTREEVIEW selected);
LRESULT HandleWmCreate(HWND, UINT, WPARAM, LPARAM);
void HandleWmSize(HWND hwnd);
BOOL InitApplication(HINSTANCE);
BOOL InitInstance(HINSTANCE, int);
BOOL InitListViewColumns(HWND hwndLV);
BOOL InitTreeViewItems(HWND hwndTV);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

#ifdef UNDER_CE
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
#else
int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
#endif
	if(!InitApplication(hInstance)) {
		MessageBox(0, L"RegisterClassW", L"Fail!", MB_ICONERROR);
		return FALSE;
	}

	if (!InitInstance(hInstance, nCmdShow)) {
		MessageBox(0, L"InitInstance", L"Fail!", MB_ICONERROR);
		return FALSE;
	}

	//SipShowIM(SIPF_ON);

	// Message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

void DisplayContextMenu(HWND hWnd, POINT pt) {
	HMENU hMenu;
	HMENU hMenuTrackPopup;

	// Load the menu resource
	if ((hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDM_KEYCTX))) == NULL)
		return;
	
	// Get the first menu from the resource
	hMenuTrackPopup = GetSubMenu(hMenu, 0);

	SetMenuDefaultItem(hMenuTrackPopup, IDM_KEYMODIFY, FALSE);

	// Display the shortcut menu. Track the right mouse button.
	TrackPopupMenu(hMenuTrackPopup,
		TPM_LEFTALIGN | TPM_RIGHTBUTTON,
		pt.x, pt.y, 0, hWnd, NULL
	);

	// Destroy the menu
	DestroyMenu(hMenu);
}

void HandleListViewRightClick(LPNMITEMACTIVATE pnmitem) {
	return;
	if (pnmitem->iItem != -1) {
		// Create a popup menu
		POINT p;
		GetCursorPos(&p);
		DisplayContextMenu(g_hwndMain, p);
	}
}

#define GET_X_LPARAM(lp)	((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)	((int)(short)HIWORD(lp))

LRESULT HandleWmContextMenu(HWND hwnd, WPARAM wParam, LPARAM lParam) {
	// Get mouse position
	POINT p;
	p.x = GET_X_LPARAM(lParam);
	p.y = GET_Y_LPARAM(lParam);

	if (p.x == -1 && p.y == -1) {
		// The context menu was spawned by button.
	}

	DisplayContextMenu(g_hwndMain, p);
}

LRESULT HandleListViewNotify(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	LPNMITEMACTIVATE pnmitem = (LPNMITEMACTIVATE)lParam;
	
	switch (pnmitem->hdr.code) {
		case NM_RCLICK:
			HandleListViewRightClick(pnmitem);
			break;
		default:
			break;
	}

	return 0;
}

LRESULT HandleTreeViewNotify(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;

	switch (pnmtv->hdr.code) {
		case TVN_ITEMEXPANDING:
			HandleTreeViewExpand(pnmtv);
			break;
		case TVN_SELCHANGED:
			// Key selection has changed, update the listview with the values
			// for that key.
			HandleTreeViewSelection(pnmtv);
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
		default:
			break;
	}

	return 0;
}

void HandleTreeViewExpand(LPNMTREEVIEW pnmtv) {
	// 1. Get the item that's expanding
	// 1. Delete all child items under it
	// 1. Query the node's subkeys and create a new node for each one
	
	// The expanding node
	RegNode *node;			
	node = (RegNode*) pnmtv->itemNew.lParam;
	
	// Break out if we aren't expanding
	if(!(pnmtv->action & TVE_EXPAND)) {
		return;
	}

	// Break out if we are the "root" node
	HTREEITEM parent;
	parent = TreeView_GetParent(g_hwndTreeView, node->htitem);

	// parent will be NULL if it's the root
	if(parent == NULL) {
		return;
	}
		
	// Delete child items
	HTREEITEM child;
	while (child = TreeView_GetChild(g_hwndTreeView, node->htitem)) {
		TreeView_DeleteItem(g_hwndTreeView, child);
	}

	// Retrieve info on expanding key
	
	if(node->hkey == NULL) {
		LONG ret;	
		// Handle isn't open, so open it
		ret = RegOpenKeyEx(
			node->roothkey,		// Handle to predefined open key
			node->fullpath,		// Name of the subkey to be opened
			0,					// Options
#ifdef UNDER_CE
			0,					// Desired Access Rights (unused in CE)
#else
			KEY_READ,
#endif
			&(node->hkey)
		);
		
		if(ret != ERROR_SUCCESS) {
			DisplayErrorInMsgBox(L"RegOpenKeyEx", ret);
			return;
		}
	}

	DWORD dwcSubKeys;	// Number of subkeys in key
	//HKEY hkeySubKey;

	RegQueryInfoKey(
		node->hkey,			// Handle to open key
		NULL,				// LPWSTR lpClass
		NULL,				// LPDWORD lpcbClass
		NULL,				// LPDWORD lpReserved
		&dwcSubKeys,		// LPDWORD lpcSubKeys
		NULL,				// LPDWORD lpcbMaxSubKeyLen
		NULL,				// LPDWORD lpcbMaxClassLen
		NULL,				// LPDWORD lpcValues
		NULL,				// LPDWORD lpcbMaxValueNameLen
		NULL,				// LPDWORD lpcbMaxValueLen
		NULL,				// LPDWORD lpcbSecurityDescriptor   Unused in CE
		NULL				// PFILETIME lpftLastWriteTime		Unused in CE
	);

	pnmtv->itemNew.cChildren = dwcSubKeys;
	pnmtv->itemNew.mask = TVIF_CHILDREN;
	TreeView_SetItem(g_hwndTreeView, &pnmtv->itemNew);
	
	// Enumerate subkeys
	WCHAR lpszSubKeyName[MAX_KEY_NAME];
	DWORD dwcchName;
	
	while (dwcSubKeys > 0) {	
		dwcchName = MAX_KEY_NAME;
		RegEnumKeyEx(node->hkey, --dwcSubKeys, lpszSubKeyName, &dwcchName, NULL, NULL, NULL, NULL);
		regnode_Create(NULL, lpszSubKeyName, g_hwndTreeView, node->htitem);
		//dwcSubKeys--;
	}

	return;
}

void HandleTreeViewSelection(LPNMTREEVIEW selected) {
	LVITEM lvi;
	RegNode *node;

	LPWSTR lpszBlank		= L"";

	// Get the RegNode for the selected item
	node = (RegNode*) selected->itemNew.lParam;

	// Clear the listview
	ListView_DeleteAllItems(g_hwndListView);

	if(node->roothkey == NULL) {
		// This is the fake root node, exit
		return;
	}

	// Make sure the reg handle is valid for the node
	if(node->hkey == NULL) {
		LONG ret;	
		// Handle isn't open, so open it
		ret = RegOpenKeyEx(
			node->roothkey,		// Handle to predefined open key
			node->fullpath,		// Name of the subkey to be opened
			0,					// Options
#ifdef UNDER_CE
			0,					// Desired Access Rights (unused in CE)
#else
			KEY_READ,
#endif
			&(node->hkey)
		);
		
		if(ret != ERROR_SUCCESS) {
			DisplayErrorInMsgBox(L"RegOpenKeyEx", ret);
			return;
		}
	}

	// Enumerate all values and add them to the list-view
	DWORD dwcValues;			// Number of values
	DWORD dwcbMaxValueLen;		// largest value data length in bytes
	DWORD dwchMaxValueNameLen;	// largest value name length in characters
	//HKEY hkeySubKey;

	RegQueryInfoKey(
		node->hkey,				// Handle to open key
		NULL,					// LPWSTR lpClass
		NULL,					// LPDWORD lpcbClass
		NULL,					// LPDWORD lpReserved
		NULL,					// LPDWORD lpcSubKeys
		NULL,					// LPDWORD lpcbMaxSubKeyLen
		NULL,					// LPDWORD lpcbMaxClassLen
		&dwcValues,				// LPDWORD lpcValues
		&dwchMaxValueNameLen,	// LPDWORD lpcbMaxValueNameLen
		&dwcbMaxValueLen,		// LPDWORD lpcbMaxValueLen
		NULL,					// LPDWORD lpcbSecurityDescriptor   Unused in CE
		NULL					// PFILETIME lpftLastWriteTime		Unused in CE
	);

	WCHAR szValueName[MAX_VALUE_NAME];
	DWORD dwcchName;
	DWORD dwType;
	WCHAR lpszValueType[32];
	WCHAR lpszValueData[MAX_DATA_LEN + 1];
	BYTE bValueData[MAX_DATA_LEN];
	DWORD dwcbData;
	BOOL allocated;
	LPWSTR lpszValueName;
	int lvidx;

	// Check if value name is larger than our stack allocated buffer
	if (dwchMaxValueNameLen > MAX_VALUE_NAME - 1) {
		lpszValueName = (LPWSTR) HeapAlloc(g_hProcessHeap, 0, (dwchMaxValueNameLen + 1) * sizeof(WCHAR));
		allocated = TRUE;
	} else {
		lpszValueName = szValueName;
		allocated = FALSE;
	}

	while (dwcValues > 0) {
		// Get the value's name, type, and data
		dwcchName = allocated ? dwchMaxValueNameLen + 1 : MAX_DATA_LEN;
		dwcbData = MAX_DATA_LEN;
		RegEnumValue(node->hkey, --dwcValues, lpszValueName, &dwcchName, NULL, &dwType, bValueData, &dwcbData);

		// Add it to the list-view
		lvi.mask = LVIF_TEXT;
		lvi.pszText = lpszValueName;
		lvi.iItem = 0;
		lvi.iSubItem = 0;
		lvidx = ListView_InsertItem(g_hwndListView, &lvi);

		// Item is created, now add column text
		// Name
		if (wcscmp(lpszValueName, L"") == 0) {
			// This is the "default" value
			LoadString(g_hInst, IDS_DEFAULTVALNAME, lpszValueName, MAX_VALUE_NAME);
			ListView_SetItemText(g_hwndListView, lvidx, 0, lpszValueName);
		}
		else {
			ListView_SetItemText(g_hwndListView, lvidx, 0, lpszValueName);
		}
		
		// Type
		switch (dwType) {
			case REG_NONE:
			case REG_SZ:
			case REG_EXPAND_SZ:
			case REG_BINARY:
			case REG_DWORD:
			case REG_DWORD_BIG_ENDIAN:
			case REG_LINK:
			case REG_MULTI_SZ:
			case REG_RESOURCE_LIST:
				LoadString(g_hInst, IDS_REGNONE + dwType, lpszValueType, 32); // Get read-only pointer to type name string
				ListView_SetItemText(g_hwndListView, lvidx, 1, lpszValueType);
				break;
			default:
				ListView_SetItemText(g_hwndListView, lvidx, 1, lpszBlank);
				break;
		}

		// Data
		switch (dwType) {
			case REG_BINARY:
				LoadString(g_hInst, IDS_BINPLACEHLDR, lpszValueData, MAX_DATA_LEN);
				ListView_SetItemText(g_hwndListView, lvidx, 2, lpszValueData);
				break;
			case REG_SZ:
			case REG_EXPAND_SZ:
				ListView_SetItemText(g_hwndListView, lvidx, 2, (LPWSTR) bValueData);	// Precarious cast retrieved registry data to string
				break;
			case REG_DWORD:
				FormatDwordForColumn(lpszValueData, sizeof(lpszValueData), *((DWORD*) bValueData));
				ListView_SetItemText(g_hwndListView, lvidx, 2, lpszValueData);
				break;
			default:
				ListView_SetItemText(g_hwndListView, lvidx, 2, lpszBlank);
				break;
		}

		// Icon
		lvi.iItem = lvidx;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_IMAGE;
		switch (dwType) {
			case REG_SZ:
			case REG_EXPAND_SZ:
			case REG_MULTI_SZ:
				lvi.iImage = 0;
				break;
			default:
				lvi.iImage = 1;
		}
		ListView_SetItem(g_hwndListView, &lvi);
	}

	if (allocated == TRUE) {
		HeapFree(g_hProcessHeap, 0, (LPVOID) lpszValueName);
	}

	return;
}

void FormatDwordForColumn(LPWSTR lpszDst, DWORD dwcStrLen, DWORD dwData) {
	_snwprintf(lpszDst, dwcStrLen, L"0x%08X (%d)", dwData, dwData);
}

LRESULT HandleWmCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
	if ((HIWORD(wParam) == 0) && (LOWORD(wParam) == IDM_KEYMODIFY)) {
		MessageBox(NULL, L"You selected Modify", L"You selected Modify", 0);
	}
	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static POINT ptPrev;
	
	switch (uMsg) {
		case WM_NOTIFY:
			if (((LPNMHDR)lParam)->idFrom == IDC_TREEVIEW) {
				return HandleTreeViewNotify(hwnd, wParam, lParam);
			}
			if (((LPNMHDR)lParam)->idFrom == IDC_LISTVIEW) {
				return HandleListViewNotify(hwnd, wParam, lParam);
			}
			break;
		case WM_CREATE:
			return HandleWmCreate(hwnd, uMsg, wParam, lParam);
		case WM_SIZE:
			HandleWmSize(hwnd);
			break;
		case WM_COMMAND:
			return HandleWmCommand(hwnd, wParam, lParam);
			break;
		case WM_CONTEXTMENU:
			return HandleWmContextMenu(hwnd, wParam, lParam);
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
					HandleWmSize(hwnd);
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
		case WM_DPICHANGED:
			DebugBreak();
			break;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);

	}

	return 0;

}

LRESULT HandleWmCreate(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);
	
	g_dwSplitterPos = rcClient.right / 3;

#ifdef UNDER_CE
	if (NULL == (g_hwndCommandBar = CommandBar_Create(g_hInst, hwnd, IDC_COMMANDBAR))) {
		DisplayErrorInMsgBox(L"CommandBar_Create", E_FAIL);
	} else {
		CommandBar_InsertMenubarEx(g_hwndCommandBar, g_hInst, MAKEINTRESOURCE(IDM_MENUBAR), 0);
		g_iCommandBarHeight = CommandBar_Height(g_hwndCommandBar);
	}
#endif
	if (NULL == (g_hImageList = CreateImageList())) {
		DisplayErrorInMsgBox(L"CreateImageList", E_FAIL);
		return -1;
	}

	if (NULL == (g_hwndTreeView = CreateTreeView(hwnd))) {
		DisplayErrorInMsgBox(L"CreateTreeView", E_FAIL);
		return -1;
	}

	if (NULL == (g_hwndSplitter = CreateSplitter(hwnd))) {
		DisplayErrorInMsgBox(L"CreateSplitter", E_FAIL);
		return -1;
	}

	if (NULL == (g_hwndListView = CreateListView(hwnd))) {
		DisplayErrorInMsgBox(L"CreateListView", E_FAIL);
		return -1;
	}

	return 0;
}

HWND CreateListView(HWND hwndParent) {
	HWND lv;
	RECT rcClient;

	GetClientRect(hwndParent, &rcClient);

	RECT lvs;

	lvs = GetListViewSize(g_dwSplitterPos, &rcClient);

	lv = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		WC_LISTVIEW,
		L"",
		WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SORTASCENDING | LVS_NOSORTHEADER,
		lvs.right,
		lvs.top,
		lvs.right - lvs.left,
		lvs.bottom - lvs.top,
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

	ListView_SetImageList(lv, g_hImageList, LVSIL_SMALL);

	return lv;
}

BOOL InitListViewColumns(HWND hwndLV) {
	WCHAR szText[8];
	LVCOLUMN lvc;
	int iCol;
	int iColWidths[3] = { 200, 120, 400 };

	// Initialize the LVCOLUMN structure.
	lvc.mask = LVCF_WIDTH | LVCF_TEXT;
	
	// Add the columns.
	for (iCol = 0; iCol < C_COLUMNS; iCol++) {
		lvc.iSubItem = iCol;
		lvc.pszText = szText;
		lvc.cx = iColWidths[iCol];
		
		// Load the names of the column headings from the string resources.
		LoadString(
			g_hInst,
			IDS_COLFIRST + iCol,
			szText,
			sizeof(szText)/sizeof(szText[0])
		);
		
		// Insert the columns into the list view.
		if (ListView_InsertColumn(hwndLV, iCol, &lvc) == -1)
			return FALSE;
	}

	return TRUE;
}

HWND CreateTreeView(HWND hwndParent) {
	HWND tv;
	RECT rcClient;

	GetClientRect(hwndParent, &rcClient);

	RECT tvs;
	tvs = GetTreeViewSize(g_dwSplitterPos, &rcClient);

	tv = CreateWindowEx (
			WS_EX_CLIENTEDGE,
			WC_TREEVIEW,
			L"Tree View",
			WS_VISIBLE | WS_CHILD | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
			tvs.left,
			tvs.top,
			tvs.right - tvs.left,
			tvs.bottom - tvs.top,
			hwndParent,
			(HMENU) IDC_TREEVIEW,
			g_hInst,
			NULL
	);

	if (tv == NULL) {
		DisplayErrorInMsgBox(L"CreateTreeView::CreateWindowEx", E_FAIL);
		return NULL;
	}

	if (!InitTreeViewItems(tv)) {
		return NULL;
	}

	TreeView_SetImageList(tv, g_hImageList, TVSIL_NORMAL);

	return tv;
}

HIMAGELIST CreateImageList() {
	HIMAGELIST	il;

	// Create the imagelist
	il = ImageList_Create(
		16, 16,			// image size
		ILC_COLORDDB | ILC_MASK,		// flags
		3,				// number of icons
		0				// extra icon slots
	);


	// Load the images
	for (int i = 0; i < 3; i++) {
		HICON	icon;
		int		ret;

		const LPWSTR ICON_IDS[3] = {
			MAKEINTRESOURCE(IDI_REGSZ),
			MAKEINTRESOURCE(IDI_REGBIN),
			MAKEINTRESOURCE(IDI_REGKEY)
		};

		// Load the bitmap from resource
		icon = LoadImage(
			g_hInst,				// handle to load from
			ICON_IDS[i],			// resource name
			IMAGE_ICON,				// image type
			16, 16,					// cx,cyDesired
			0						// fuLoad (set to 0)
		);

		if (icon == NULL) {
			// Failed to load bitmap
			DWORD error = GetLastError();
			DisplayErrorInMsgBox(L"LoadImage", error);
			ImageList_Destroy(il);
			return NULL;
		}
		ret = ImageList_AddIcon(il, icon);

		DeleteObject(icon);

		if (ret == -1) {
			DisplayErrorInMsgBox(L"ImageList_AddIcon", E_FAIL);
			ImageList_Destroy(il);
			return NULL;
		}
	}


	return il;
}

HWND CreateSplitter(HWND hwndParent) {
	RECT rcClient;
	HWND sb;
	
	GetClientRect(hwndParent, &rcClient);
	g_dwSplitterPos = rcClient.right / 3;
	
	sb = CreateWindow(
		L"STATIC",
		NULL,
		WS_CHILD | WS_VISIBLE | SS_LEFT,
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

RECT GetTreeViewSize(DWORD dwSplitterPos, RECT *rcClient) {
	RECT tvs;
	const int pad = CONTROL_PADDING;
	
	tvs.left = pad;
	tvs.top = pad + g_iCommandBarHeight;
	tvs.right = dwSplitterPos - pad;
	tvs.bottom = rcClient->bottom - 2 * pad - g_iCommandBarHeight;
	
	return tvs;
}

RECT GetListViewSize(DWORD dwSplitterPos, RECT *rcClient) {
	RECT lvs;
	const int pad = CONTROL_PADDING;

	lvs.left = dwSplitterPos + SPLITTER_WIDTH;
	lvs.top = pad + g_iCommandBarHeight;
	lvs.right = rcClient->right - pad;
	lvs.bottom = rcClient->bottom - 2 * pad - g_iCommandBarHeight;

	return lvs;
}

void HandleWmSize(HWND hwnd) {
	const int pad = CONTROL_PADDING;
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);
	
	RECT tvs;
	RECT lvs;
	int sb_x, sb_y, sb_w, sb_h;

	sb_x = g_dwSplitterPos;
	sb_y = pad;
	sb_w = SPLITTER_WIDTH,
	sb_h = rcClient.bottom - 2 * pad;
	
	lvs = GetListViewSize(g_dwSplitterPos, &rcClient);
	tvs = GetTreeViewSize(g_dwSplitterPos, &rcClient);

 	SetWindowPos(g_hwndTreeView, NULL, tvs.left, tvs.top, tvs.right - tvs.left, tvs.bottom - tvs.top, SWP_NOZORDER | SWP_NOACTIVATE);
	MoveWindow(g_hwndSplitter, sb_x, sb_y, sb_w, sb_h, TRUE);
 	SetWindowPos(g_hwndListView, NULL, lvs.left, lvs.top, lvs.right - lvs.left, lvs.bottom - lvs.top, SWP_NOZORDER | SWP_NOACTIVATE);
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

void DisplayErrorInMsgBox(LPCWSTR lpszTitle, DWORD dwErr) {
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
			lpszTitle,
			MB_ICONERROR
	);
}	

BOOL InitApplication(HINSTANCE hInstance) {
	// Init common controls
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
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
#ifdef UNDER_CE
	const DWORD dwExStyle = 0x40000000;
#else
	const DWORD dwExStyle = 0;
#endif

	g_hwndMain = CreateWindowEx(
			dwExStyle,				// Optional window styles
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

	g_hProcessHeap = GetProcessHeap();
	if (!g_hProcessHeap) {
		return FALSE;
	}

	// Show the window
	ShowWindow(g_hwndMain, nCmdShow);
	ShowWindow(g_hwndMain, SW_SHOWMAXIMIZED);
	UpdateWindow(g_hwndMain);

	return TRUE;
}
