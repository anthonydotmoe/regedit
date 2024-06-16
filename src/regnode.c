#include "regnode.h"

static int g_next_id = NODE_ID_START;

RegNode* regnode_Create(HKEY hKey, LPCWSTR keyName, HWND hwndTV, HTREEITEM parent) {
    RegNode *newNode = (RegNode*)malloc(sizeof(RegNode));
    newNode->id = g_next_id++;
    newNode->hkey = hKey;
    //wcsncpy_s(newNode->keyName, MAX_KEY_NAME, keyName, _TRUNCATE);
    //StringCchCopy(newNode->keyName, MAX_KEY_NAME, keyName);
    wcsncpy(newNode->keyName, keyName, MAX_KEY_NAME);
    
    // Add to TreeView
    TVINSERTSTRUCT tvis = {0};
    tvis.hParent = parent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
    tvis.item.pszText = newNode->keyName;
    tvis.item.cChildren = 1;        // Node has one or more children
    tvis.item.lParam = (LPARAM)newNode;
    
    newNode->htitem = TreeView_InsertItem(hwndTV, &tvis);
    
    // Add a dummy item under the newly created item to allow for expansion
    if(parent != TVI_ROOT) {
        tvis.hParent = newNode->htitem;
        tvis.item.mask = TVIF_TEXT;
        tvis.item.pszText = L"DUMMYITEM";
        TreeView_InsertItem(hwndTV, &tvis) ;
    }
    
    return newNode;
}

void regnode_Destroy(RegNode *regnode) {
    if(NULL != regnode) {
        RegCloseKey(regnode->hkey);
        free((void*)regnode);
    }
}
