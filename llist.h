//
// Created by oreql on 16.12.25.
//

#ifndef C2JSON_B_LLIST_H
#define C2JSON_B_LLIST_H
typedef struct node NODE;
typedef struct list LIST;

//Methods
typedef int (*add_destroy_list)(LIST**);
typedef int (*add_ins_node_next)(LIST**,NODE*,void*);
typedef int (*add_rem_next_elmt)(LIST**,NODE*,void **);
typedef int (*add_str_ID)(LIST**,const char*);

struct node {
  void *data;
  NODE *next;
  int ID;
};

struct list {
  NODE *head;
  NODE *tail;
  char *name_value;
  int size;
  //Methods
  add_destroy_list Ldestr_list;
  add_ins_node_next Lins_node_next;
  add_rem_next_elmt Lrem_next_elmt;
  add_str_ID Lstr_id;
};

int init_list(LIST **list_);
int destroy_list(LIST **list);
int ins_node_next(LIST **list,NODE *node,void *data);
int rem_next_elmt(LIST **list,NODE *node,void **data);
int Lstr_ID(LIST **node,const char *str);
#endif //C2JSON_B_LLIST_H
