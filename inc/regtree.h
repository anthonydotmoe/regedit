#ifndef REGTREE_H
#define REGTREE_H

#include <windows.h>
#include <CommCtrl.h>
#include "regedit.h"

typedef struct RegKeyNode {
    int                 id;                     // Unique identifier for the node
    HKEY                hkey;                   // Handle to the registry key
    WCHAR               keyName[MAX_KEY_NAME];  // Name of the registry key
    HTREEITEM           hTreeItem;              // Handle to the TreeView item
    struct RegKeyNode   *parent;                // Pointer to the parent node
    struct RegKeyNode  *children;              // Pointer to the first child node
    struct RegKeyNode   *next;                  // Pointer to the next sibling node
} RegKeyNode;

typedef struct {
    RegKeyNode  *root;
    int         nextId;
    HWND        hTreeView;
} RegKeyTree;

void regtree_Initialize(RegKeyTree *tree, HWND hTreeView);

RegKeyNode* regtree_CreateNode(RegKeyTree *tree, HKEY hKey, LPCWSTR keyName, RegKeyNode *parent);
void regtree_FreeNode(RegKeyNode *node);
void regtree_AddChildNode(RegKeyNode *parent, RegKeyNode *child);
void regtree_GetFullPath(RegKeyNode *node, LPWSTR path, int size);

#endif