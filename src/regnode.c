#include "regnode.h"


static int g_next_id = NODE_ID_START;

RegNode* regnode_Create(HKEY hKey, LPCWSTR keyName, HWND hwndTV, HTREEITEM parent) {
    RegNode *newNode = (RegNode*)malloc(sizeof(RegNode));
    newNode->id = g_next_id++;
    newNode->hkey = hKey;
    
    //wcsncpy_s(newNode->keyName, MAX_KEY_NAME, keyName, _TRUNCATE);
    //StringCchCopy(newNode->keyName, MAX_KEY_NAME, keyName);
    wcsncpy(newNode->keyName, keyName, MAX_KEY_NAME);
    
    // Copy the full path of the parent key to the newNode's full path
    if(parent == TVI_ROOT) {
        // This is the psuedo root key
        newNode->roothkey = NULL;
        wcsncpy(newNode->fullpath, L"", MAX_PATH);
    }
    else if(hKey == HKEY_CLASSES_ROOT || hKey == HKEY_CURRENT_USER || hKey == HKEY_LOCAL_MACHINE || hKey == HKEY_USERS) {
        // This is a predefined handle
        newNode->roothkey = hKey;
        newNode->hkey = NULL;
        wcsncpy(newNode->fullpath, L"", MAX_PATH);
    }
    else {
        // Get TVITEM of parent
        TVITEM tvParentItem;
        tvParentItem.hItem = parent;        // Which item to get information from
        tvParentItem.mask = TVIF_PARAM;     // Get the parent item's RegNode

        TreeView_GetItem(hwndTV, &tvParentItem);

        // Get RegNode struct
        RegNode *regnodeParent = (RegNode*)tvParentItem.lParam;
        
        // Copy the root hkey from the parent
        newNode->roothkey = regnodeParent->roothkey;

        // Copy the full path of the parent node
        wcsncpy(newNode->fullpath, regnodeParent->fullpath, MAX_PATH);
        if (wcscmp(regnodeParent->fullpath, L"") != 0) {
            // This is not the first child node of a predefined key
            // Add the path slash
            wcsncat(newNode->fullpath, L"\\", 2);
        }
        // Add the name of this key
        wcsncat(newNode->fullpath, keyName, MAX_PATH);
    }

    // Add to TreeView
    TVINSERTSTRUCT tvis = {0};
    tvis.hParent = parent;
    tvis.hInsertAfter = TVI_SORT;
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
