#ifndef REGNODE_H
#define REGNODE_H

#include <windows.h>
#include <commctrl.h>
#include "regedit.h"

#define NODE_ID_START   5000;

typedef struct RegNode {
    int                 id;                     // Unique identifier for the node
    HKEY                hkey;                   // Handle to the registry key
    WCHAR               keyName[MAX_KEY_NAME];  // Name of the registry key
    HTREEITEM           htitem;                 // Handle to the tree item
} RegNode;

RegNode* regnode_Create(HKEY hKey, LPCWSTR keyName, HWND hwndTV, HTREEITEM parent);
void regnode_Destroy(RegNode *regnode);

#endif