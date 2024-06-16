#include <string.h>
#include "regtree.h"

void regtree_Initialize(RegKeyTree *tree, HWND hTreeView) {
    tree->root = NULL;
    tree->nextId = 1;
    tree->hTreeView = hTreeView;
}

RegKeyNode* regtree_CreateNode(RegKeyTree *tree, HKEY hKey, LPCWSTR keyName, RegKeyNode *parent) {
    RegKeyNode *newNode = (RegKeyNode*)malloc(sizeof(RegKeyNode));
    newNode->id = tree->nextId++;
    newNode->hkey = hKey;
    wcsncpy_s(newNode->keyName, MAX_KEY_NAME, keyName, _TRUNCATE);
    newNode->parent = parent;
    newNode->children = NULL;
    newNode->next = NULL;

    // Add to TreeView
    TVINSERTSTRUCT tvis = {0};
    tvis.hParent = parent ? parent->hTreeItem : TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = newNode->keyName;
    tvis.item.lParam = (LPARAM)newNode;

    newNode->hTreeItem = TreeView_InsertItem(tree->hTreeView, &tvis);

    return newNode;
}

void regtree_FreeNode(RegKeyNode *node) {
    if (node == NULL) return;

    // Free children first
    RegKeyNode *child = node->children;
    while(child != NULL) {
        RegKeyNode *nextChild = child->next;
        regtree_FreeNode(child);
        child = nextChild;
    }

    // Free the node itself
    free(node);
}

void regtree_GetFullPath(RegKeyNode *node, LPWSTR path, int size) {
    if(node->parent != NULL) {
        regtree_GetFullPath(node->parent, path, size);
        wcsncat_s(path, size, L"\\", _TRUNCATE);
        wcsncat_s(path, size, node->keyName, _TRUNCATE);
    } else {
        wcsncpy_s(path, size, node->keyName, _TRUNCATE);
    }
}

void regtree_AddChildNode(RegKeyNode *parent, RegKeyNode *child) {
    if (parent->children == NULL) {
        parent->children = child;
    } else {
        RegKeyNode *sibling = parent->children;
        while (sibling->next != NULL) {
            sibling = sibling->next;
        }
        sibling->next = child;
    }
}