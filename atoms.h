//
// Created by oreql on 16.12.25.
//

#ifndef C2JSON_B_ATOMS_H
#define C2JSON_B_ATOMS_H
//#include "../Lists/dllist.h"
#include "../llist/llist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
typedef enum { ID_STR = 1,ID_INT,ID_FLT,ID_GEN }TYP;

typedef struct atom ATOM;
typedef int (*add_set_atom)(ATOM*,int,char*,void*);
typedef void (*add_remove_atom)(ATOM**);

typedef struct generic_func_P GEN_FP;
struct generic_func_P {
  int signature;
  int return_type;
  int parameter_count;
  void* (*generic_func)(void *self);
};

struct atom {
  /*
  wenn type_data:
  - 1 dann handelt es sich um DT
  - 2 dann handelt es sich um OBJ
  - 3 dann handlet es sich um ARRAY
  - 4 dann handeslt es sich um GEN   */
  int type_data;
  int ID; 
  struct atom *next;
  struct atom *prev;
  LIST *elmt;
  add_set_atom SETF_ATOM;
  add_remove_atom REM_ATOM;
};
void rem_atom(ATOM **atom); 
int set_atom(ATOM *atom,int token,char *str_key,void *ptr_data);
ATOM *init_atom(void);


#endif
