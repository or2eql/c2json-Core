//
// Created by oreql on 16.12.25.
//

#include "atoms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>


void rem_atom(ATOM **atom) {
  if (atom == NULL || *atom == NULL) return;
  
  ATOM *ptr_atom = *atom;
  if (ptr_atom->elmt != NULL) {
    ptr_atom->elmt->Ldestr_list(&(ptr_atom->elmt));
  }
  free(ptr_atom);
  *atom = NULL;
}

//atom->typ_data = 1 !! SS
int set_atom(ATOM *atom,int token,char *str_key,void *ptr_data) {
  if (atom == NULL || ptr_data == NULL) {
    return -1;
  }
  atom->type_data = 1;
  //JSON Variable Name wird hinterlegt in '..elmt->name_value'
  //JSON ID für Typsicherheit wird hinterlegt in 'head'
  //JSON Variable Wert wird hinterlegt in 'tail'

  if (atom->elmt->Lins_node_next(&atom->elmt, atom->elmt->head, NULL) != 0) {
    atom->elmt->Ldestr_list(&atom->elmt);
    atom->elmt = NULL;
    rem_atom(&atom);
    return -2;
  }
  // Ein zweiter Knoten für den Tail (Wert) wird benötigt:
  if (atom->elmt->Lins_node_next(&atom->elmt, atom->elmt->tail, NULL) != 0) {
    atom->elmt->Ldestr_list(&atom->elmt);
    atom->elmt = NULL;
    rem_atom(&atom);
    return -2;
  }

  if ((atom->elmt->head->data = malloc(sizeof(int))) == NULL) {
    atom->elmt->Ldestr_list(&atom->elmt);
    atom->elmt = NULL;
    rem_atom(&atom);
    return -3;
  }
  //JSON Name

  if ((atom->elmt->Lstr_id(&atom->elmt, str_key)) != 0) {
    free(atom->elmt->head->data);
    atom->elmt->head->data = NULL;
    atom->elmt->Ldestr_list(&atom->elmt);
    atom->elmt = NULL;
    rem_atom(&atom);
    atom = NULL;
    return -4;
  }


  //TYP - ID
  if (token <= 4 && token > 0) {
    *((int*)atom->elmt->head->data) = token;
  } else {
    free(atom->elmt->head->data);
    atom->elmt->head->data = NULL;
    atom->elmt->Ldestr_list(&atom->elmt);
    atom->elmt = NULL;
    rem_atom(&atom);
    return -5;
  }

  // WICHTIG: Keine Sonderbehandlung mehr für ID_GEN! 
  // Egal ob String, Float oder unser GEN_FP-Struct, der Pointer wird einfach zugewiesen.
  atom->elmt->tail->data = (void *)ptr_data;  
  
  return 0;
}

ATOM *init_atom(void) {
  ATOM *atom = NULL;
  if ((atom = malloc(sizeof(ATOM))) == NULL) {
    return NULL;
  }
  memset(atom,0,sizeof(ATOM));
  init_list(&atom->elmt);
  if (atom->elmt == NULL) {
    rem_atom(&atom);
    return NULL;
  }
  atom->type_data = 0;
  atom->ID = 0;
  atom->next = NULL;
  atom->prev = NULL;
  atom->SETF_ATOM = set_atom;
  atom->REM_ATOM = rem_atom;
  return atom;
}

