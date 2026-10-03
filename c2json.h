#ifndef C2JSON_B_CJSON_H
#define C2JSON_B_CJSON_H
#include <math.h>
#define SIZE_TYPES 4
#include "atoms.h"
/* #include "./object.h" */

typedef void (*GENFUNC)(void*);

typedef enum {
  TYP_CORE = 0,
  TYP_OBJ,
  TYP_ARY,
  TYP_GEN
} typ_;

typedef struct core_vtable CORE_VTABLE;
typedef struct obj_vtable OBJ_VTABLE;
typedef struct ary_vtable ARY_VTABLE;
typedef struct gen_vtable GEN_VTABLE;

typedef union {
  const CORE_VTABLE *Mcore;
  const OBJ_VTABLE *Mobject;
  const ARY_VTABLE *Marray;
  const GEN_VTABLE *Mgen;
}method_obj;

typedef struct core_instance CORE_INSTANCE;
typedef struct obj_instance OBJ_INSTANCE;
typedef struct ary_instance ARY_INSTANCE;
typedef struct gen_instance GEN_INSTANCE;

typedef union {
  CORE_INSTANCE *Icore;
  OBJ_INSTANCE *Iobj;
  ARY_INSTANCE *Iary;
  GEN_INSTANCE *Igen;
}instance_obj;

typedef struct {
  typ_ Typ;
  method_obj Method;
}method_container;

typedef struct {
  typ_ Typ;
  instance_obj Instance;
}instance_container;

/***********
 * C O R E *
 ***********/
typedef struct core CORE;
typedef CORE OBJ;
typedef CORE ARY;
typedef CORE GEN;

struct core {
  int typ;
  int size;
  ATOM *head;
  ATOM *tail;
  char *str_ID;
  method_container contain[SIZE_TYPES];
  instance_container member[SIZE_TYPES];
};

struct core_instance {
  unsigned int signature;
  LIST *deep_copy;
  LIST *flat_copy;
  OBJ *sphere;
};

struct core_vtable {
  // Methoden CORE
  int (*destroy_core)(CORE**);
  int (*upcast_core)(CORE**,int,char*);
  CORE* (*downcast_core)(void*,int);
  void (*create_deep_copy)(CORE*,CORE**);
  void (*create_flat_copy)(CORE*,CORE*);
  int (*json_string2obj)(const char **, OBJ **);
  char *(*obj2json_string)(OBJ *);
  int (*update_atom)(OBJ*,const char*,int,void *);
};

CORE *init_core(void); // v
int destroy_core_meth_inst(CORE **core); // v
void create_deep_copy(CORE *orig,CORE **copy); // v
void create_flat_copy(CORE *orig,CORE *copy);
CORE* downcast_core_(void *data,int type); // v
int upcast_core_(CORE **core,int typ,char *ID); // v
int destroy_core_(CORE **core); // v
ATOM *deep_copy_primitive_atom(ATOM *current_elmt,OBJ *sphere);
ATOM *deep_copy_atom(ATOM *current_elmt);
ATOM *flat_copy_atom(ATOM *current_elmt);



void append_to_string(char **dest, size_t *current_size, const char *src);
void obj_to_json_recursive(OBJ *obj, char **buffer, size_t *buffer_size,
			   size_t *offset);
char *obj_to_json_string(OBJ *obj); // erzeugt aus objekt ein json string
void skip_whitespace(const char **str);
char *parse_json_string(const char **str);
int json_string_to_obj(const char **str, OBJ **target_obj); //erzeugt aus json ein objekt
int update_atom_value(OBJ *obj, const char *str_key, int new_token,
                      void *new_ptr_data);


/***************
 * O B J E C T *
 ***************/

struct obj_instance {
  unsigned int signature;
  void *data_gen;
};

struct obj_vtable {
  int (*rem_atom)(OBJ *, ATOM *);
  int (*atom_next)(OBJ *, ATOM *, ATOM *);
  int (*atom_prev)(OBJ *, ATOM *, ATOM *);
  OBJ *(*obj_next)(OBJ *, ATOM *, OBJ *);
  OBJ *(*obj_prev)(OBJ *, ATOM *, OBJ *);
  OBJ *(*array_next)(OBJ *, ATOM *, ARY *);
  OBJ *(*array_prev)(OBJ *, ATOM *, ARY *);
  OBJ *(*gen_next)(OBJ *, ATOM *, GEN *);
  OBJ *(*gen_prev)(OBJ *, ATOM *, GEN *);
  int (*getf_obj)(OBJ *, char *, ...);  
};
  
int create__ID(OBJ *ob,char *pattern);
int init_obj(OBJ **object,char *ID);
int rem_atom_(OBJ *object,ATOM *node_rem);
int atom_ins_next(OBJ *object,ATOM *node,ATOM *data);
int atom_ins_prev(OBJ *object,ATOM *node,ATOM *data);

OBJ *obj_ins_obj_next(OBJ *object,ATOM *node,OBJ *data);
OBJ *obj_ins_obj_prev(OBJ *object,ATOM *node,OBJ *data);

OBJ *array_ins_obj_next(OBJ *object,ATOM *node,ARY *data);
OBJ *array_ins_obj_prev(OBJ *object, ATOM *node, ARY *data);

OBJ *gen_ins_obj_next(OBJ *object, ATOM *node, GEN *data);
OBJ *gen_ins_obj_prev(OBJ *object, ATOM *node, GEN *data);

int getf_obj(OBJ *object, char *format_str, ...);


/*************
 * A R R A Y *
 *************/

struct ary_instance {
  unsigned int signature;
  unsigned int typ_elements;
  void **pos;
};

struct ary_vtable {
  int (*ary_typ_atom)(ARY **, int, ATOM *);
  int (*ary_typ_obj)(ARY **,int,OBJ *);
  int (*ary_typ_ary)(ARY **, int, ARY *);
  int (*setf_array)(ARY *, char *, ...);
  void *(*get_by_index)(ARY *, int);
};
  

int init_array(ARY **array,char *ID);
int def_arrayATOM(ARY **array,int size,ATOM *data);
int def_arrayOBJ(ARY **array,int size,OBJ *data);
int def_arrayARY(ARY **array,int size,ARY *data);
int setf_array(ARY *array,char *format_str,...);
void create_flatlist(CORE *core, LIST **flatlist);
int set_array(ARY *array, int new_size);
void *get_index(ARY* array,int index);
void create_symlist(OBJ *obj, LIST **symlist);
ATOM *create_atom(CORE *core,ATOM *old_atom);
OBJ *create_obj(OBJ *old_obj);
ARY *create_ary(ARY *old_ary);
// create gen --> high orbit ion cannon (verteiltes ddos system)


/**************
 **** GEN *****
 *************/

typedef struct container_method {
  unsigned int size_paras;
  unsigned int typ_return;
  char *name_value;
  GENFUNC *func; // hier kommen nicht die nativen server funktioennen rein
                 // sondern die containerfunktionen
}Cmethod;


struct gen_instance {
  unsigned int signature;
  ATOM *ret;
  Cmethod *gen_method;
};

struct gen_vtable {
  int (*run_gen)(GEN *, const char *);
  Cmethod (*load_object)(GEN *, const char *);
  int (*load_gen)(GEN **);
  int (*objectivate_gen)(GEN **genobject);
};


int init_gen(OBJ **genobject);
int load_gen(GEN **genobject);
int run_gen(GEN *gen, const char *name);
int objectivate_gen(GEN **genobject);
int bind_gen_payload(GEN *payload);
#endif
