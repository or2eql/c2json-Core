#include <stdlib.h>
#include <string.h>
#include "llist.h"

int init_list(LIST **list) {
    if ((*list) != NULL) {
        return -1;
    }
    if ((*list = malloc(sizeof(LIST))) == NULL) {
        return -2;
    }

    (*list)->head = NULL;
    (*list)->tail = NULL;
    (*list)->size = 0;
    (*list)->name_value = NULL;
    (*list)->Ldestr_list = destroy_list;
    (*list)->Lins_node_next = ins_node_next;
    (*list)->Lrem_next_elmt = rem_next_elmt;
    (*list)->Lstr_id = Lstr_ID;
    return 0;
}

int ins_node_next(LIST **list, NODE *node, void *data) {
    LIST *plist = *list;
    if (plist == NULL) {
        return -1;
    }

    NODE *new_node = NULL;
    if ((new_node = malloc(sizeof(NODE))) == NULL) {
        return -2;
    }

    // Initialisiere den neuen Knoten
    new_node->data = NULL;
    new_node->ID = 0;
    new_node->next = NULL;

    // Den neuen Knoten einsortieren
    if (node == NULL) { // Am Anfang einfügen (neuer Head)
        new_node->next = plist->head;
        plist->head = new_node;
        if (plist->tail == NULL) {
            plist->tail = new_node;
        }
    } else { // Nach dem Knoten 'node' einfügen
        new_node->next = node->next;
        node->next = new_node;
        if (new_node->next == NULL) {
            plist->tail = new_node;
        }
    }

    if (data) {
        new_node->data = (void*) data;
    }

    new_node->ID = plist->size++;
    *list = plist;

    return 0;
}

int rem_next_elmt(LIST **list, NODE *node, void **data) {
    LIST *plist = *list;
    if (plist == NULL || plist->size == 0) {
        return -1;
    }

    NODE *next_node = NULL;

    // Sonderfall: Wenn node == NULL, löschen wir das erste Element (Head)
    if (node == NULL) {
        next_node = plist->head;
        plist->head = next_node->next;
        if (plist->head == NULL) {
            plist->tail = NULL;
        }
    } else {
        if (node->next == NULL) {
            return -2; // Nichts zum Löschen da
        }
        next_node = node->next;
        node->next = next_node->next;
        if (node->next == NULL) {
            plist->tail = node;
        }
    }

    if (next_node->data != NULL) {
        *data = next_node->data;
    }

    free(next_node);
    plist->size--;
    *list = plist;

    return 0;
}

int destroy_list(LIST **list) {
    LIST *plist = *list;
    if (plist == NULL) {
        return -1;
    }

    void *data = NULL;
    // Solange die Liste nicht leer ist, lösche immer das Kopfelement (node = NULL)
    while (plist->size > 0) {
        rem_next_elmt(&plist, NULL, &data);
    }

    free(plist);
    *list = NULL;
    return 0;
}

int Lstr_ID(LIST **list, const char *str) {
    if ((*list) == NULL) {
        return -1;
    }
    if ((*list)->name_value != NULL || str == NULL || strlen(str) == 0) {
        return -2;
    }

    (*list)->name_value = strdup(str);
    
    // Check auf erfolgreiches strdup
    if ((*list)->name_value == NULL) {
        return -3; // Speicherfehler
    }
    
    return 0;
}
