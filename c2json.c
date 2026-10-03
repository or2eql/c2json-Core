
#include "./c2json.h"
#include "atoms.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <dlfcn.h>

/********************
 ***   C O R E   ***
 *******************/
const static CORE_VTABLE global_core_vtable = {
  .destroy_core    = destroy_core_,
  .create_deep_copy     = create_deep_copy,
  .create_flat_copy     = create_flat_copy,
  .downcast_core = downcast_core_,
  .upcast_core   = upcast_core_,
  .json_string2obj   = json_string_to_obj,
  .obj2json_string   = obj_to_json_string,
  .update_atom   = update_atom_value
};

CORE *init_core(void) {
  // calloc füllt alles automatisch mit 0 und NULL!
  CORE *core = calloc(1, sizeof(CORE));
  if (core == NULL) {
    return NULL;
  }
  
  core->typ = TYP_CORE;
  core->contain[TYP_CORE].Method.Mcore = &global_core_vtable;
  
  core->member[TYP_CORE].Instance.Icore = calloc(1, sizeof(CORE_INSTANCE));
  if (core->member[TYP_CORE].Instance.Icore == NULL) {
    free(core); 
    return NULL;
  }
  
  // Kurzreferenz für sauberen, lesbaren Code
  CORE_INSTANCE *inst = core->member[TYP_CORE].Instance.Icore;
  inst->signature = 0xAAAAAAAA;
  inst->sphere = NULL; 
  
  // Zwingend & (Adresse) übergeben, keinen Pointer-Cast!
  if (init_list(&inst->deep_copy) != 0 || init_list(&inst->flat_copy) != 0) {
    // Teardown bei Fehler
    if (inst->deep_copy != NULL) destroy_list(&inst->deep_copy);
    if (inst->flat_copy != NULL) destroy_list(&inst->flat_copy);
    free(inst);
    free(core);
    return NULL;
  }
  
  return core;
}
/*******************************************************
 * SPHERE GC: Registriert einen Pointer in der Sphäre  *
 ******************************************************/
int register_in_sphere(CORE *parent, void *ptr) {
    if (!parent || !ptr) return -1;

    CORE_INSTANCE *inst = parent->member[TYP_CORE].Instance.Icore;

    // LAZY INIT: Sphäre mit init_obj sauber erzeugen, falls noch nicht vorhanden
    if (inst->sphere == NULL) {
        if (init_obj(&inst->sphere, "@sphere") != 0) {
            return -2;
        }
    }

    ATOM *ticket = init_atom();
    if (!ticket) return -3;
    // Typ 4 (ID_GEN) reicht völlig, da free() nur den void* braucht!
    ticket->SETF_ATOM(ticket, 4, "@gc_ptr", ptr);
    atom_ins_next(inst->sphere,inst->sphere->tail,ticket);
    return 0;
}
/*******************************************************
 * SPHERE GC: Kollabiert die Sphäre und gibt RAM frei  *
 ******************************************************/
void collapse_sphere(OBJ **sphere) {
    if (!sphere || !(*sphere)) return;

    // 1. Nur die registrierten Payloads zerstören
    ATOM *current = (*sphere)->head;
    while (current != NULL) {
        if (current->elmt && current->elmt->tail && current->elmt->tail->data) {
            free(current->elmt->tail->data);
            current->elmt->tail->data = NULL; // Schutz vor Double-Free
        }
        current = current->next;
    }
    
    // 2. Die komplette restliche Zerstörung (inkl. Tickets und OBJ_INSTANCE) 
    // übernimmt unsere kugelssichere destroy_core_!
    (*sphere)->contain[TYP_CORE].Method.Mcore->destroy_core(sphere);
}
int destroy_core_meth_inst(CORE **core) {
  if ((*core) == NULL) {
    return -1;
  }

  CORE *ptr_core = *core;
  /**********************************
   * Free the variables and methods *
   *********************************/
  ptr_core->contain[TYP_CORE].Method.Mcore = NULL;
  free((CORE_INSTANCE *)ptr_core->member[TYP_CORE].Instance.Icore);

  *core = ptr_core;
  return 0;
}

/*************************************
 * Downcast OBJ ARY or GEN to 'CORE' *
 * 'CORE' is the base typ            *
 ************************************/
CORE *downcast_core_(void *data, int type) {
  if (data == NULL) {
    return NULL;
  }

  switch (type) {
  case 1:
    /******************************************
     * The typ is proofed with it's signature *
     *****************************************/
    if (((OBJ *)data)->member[TYP_OBJ].Instance.Iobj->signature == 0xBBBBBBBB) {
      /******************************************
       * The methods are pointer to the V-Table *
       *****************************************/
      ((OBJ *)data)->contain[TYP_OBJ].Method.Mobject = NULL;
      ((CORE *)data)->contain[TYP_CORE].Method.Mcore = &global_core_vtable;
      /****************************************************************
       * The instance is a container of typ 'OBJ_INSTANCE'            *
       * There are two containers at the same time                    *
       * The 'CORE' - container (base typ) and the object container   *
       * -> CORE_INSTANCE                                             *
       * -> OBJ_INSTANCE                                              *
       ***************************************************************/
      free((OBJ_INSTANCE *)((OBJ *)data)->member[TYP_OBJ].Instance.Iobj);
      ((OBJ *)data)->member[TYP_OBJ].Instance.Iobj = NULL;
      break;
    } else {
      return NULL;
    }
  case 2:
    // Signature array
    if (((ARY *)data)->member[TYP_ARY].Instance.Iary->signature == 0xCCCCCCCC) {
      // Pointer to the V-Table
      ((ARY *)data)->contain[TYP_OBJ].Method.Mobject = NULL;
      ((CORE *)data)->contain[TYP_CORE].Method.Mcore = &global_core_vtable;
      // -> CORE_INSTANCE
      // -> ARY_INSTANCE 
      free((ARY_INSTANCE *)((ARY *)data)->member[TYP_ARY].Instance.Iary);
      ((ARY *)data)->member[TYP_ARY].Instance.Iary = NULL;
      break;
    } else {
      return NULL;
    }
  case 3:
    // Signature generic pointer
    if (((GEN *)data)->member[TYP_GEN].Instance.Igen->signature == 0xDDDDDDDD) {
      // Pointer to the V-Table
      ((GEN *)data)->contain[TYP_GEN].Method.Mgen = NULL;
      ((CORE *)data)->contain[TYP_CORE].Method.Mcore = &global_core_vtable;
      // -> CORE_INSTANCE
      // -> GEN_INSTANCE
      free((GEN_INSTANCE *)((GEN *)data)->member[TYP_GEN].Instance.Igen);
      ((GEN *)data)->member[TYP_GEN].Instance.Igen = NULL;
      break;
    } else {
      return NULL;
    }
  default: return NULL;
  }
  /*****************************************************************************
  * The 'CORE' instance container is left over and the signature is customized *
  *****************************************************************************/
  ((CORE *)data)->member[TYP_CORE].Instance.Icore->signature = 0xAAAAAAAA;
  ((CORE *)data)->typ = TYP_CORE;
      
  return (CORE *) data;
}
/*****************************************
 * Upcast 'CORE' to 'OBJ' 'ARY' or 'GEN' *
 ****************************************/
int upcast_core_(CORE **core, int typ, char *ID) {
  if ((*core) == NULL) {
    return - 1;
  }

  CORE *ptr_core = *core;
  /*********************************
   * Is the oject no more 'CORE' ? *
   * -> exit the method            *
   ********************************/
  if (ptr_core->typ != TYP_CORE) {
    return -2;
  }

  switch (typ) {
  case 0: return -3; // Cast 'CORE' to 'CORE is forbidden
  case 1:
    init_obj(&ptr_core, ID);
    break;
  case 2:
    init_array(&ptr_core, ID);
    break;
  
  default:
    return -3;
  }

  *core = ptr_core;
  return 0;
}
/******************************************
 * Destroys a object by cast it to 'CORE' *
 * The object is NULL then                *
 *****************************************/
int destroy_core_(CORE **core) {
  if (!core || !(*core)) {
    return -1;
  }

  // =========================================================================
  // PHASE 1: DEPENDENCIES ZUERST ZERSTÖREN (Registry aufräumen)
  // Das Original zwingt alle Kopien, sich aufzulösen, BEVOR die Daten sterben.
  // =========================================================================
  if ((*core)->member[TYP_CORE].Instance.Icore != NULL) {
      
      // A) Flat Copies abräumen
      if ((*core)->member[TYP_CORE].Instance.Icore->flat_copy != NULL) {
        LIST *flat_list = (*core)->member[TYP_CORE].Instance.Icore->flat_copy;
        NODE *ptr_node = flat_list->head;
        while (ptr_node != NULL) {
          CORE *flat_copy_obj = (CORE *)ptr_node->data;
          if (flat_copy_obj != NULL) {
              flat_copy_obj->contain[TYP_CORE].Method.Mcore->destroy_core(&flat_copy_obj);
          }
          ptr_node = ptr_node->next;
        }
        flat_list->Ldestr_list(&flat_list);
        (*core)->member[TYP_CORE].Instance.Icore->flat_copy = NULL;
      }

      // B) Deep Copies abräumen
      if ((*core)->member[TYP_CORE].Instance.Icore->deep_copy != NULL) {
        LIST *deep_list = (*core)->member[TYP_CORE].Instance.Icore->deep_copy;
        NODE *ptr_node = deep_list->head;
        while (ptr_node != NULL) {
          CORE *deep_copy_obj = (CORE *)ptr_node->data;
          if (deep_copy_obj != NULL) {
              deep_copy_obj->contain[TYP_CORE].Method.Mcore->destroy_core(&deep_copy_obj);
          }
          ptr_node = ptr_node->next;
        }
        deep_list->Ldestr_list(&deep_list);
        (*core)->member[TYP_CORE].Instance.Icore->deep_copy = NULL;
      }
  }

  // =========================================================================
  // PHASE 2: DIE EIGENEN DATEN ZERSTÖREN
  // Jetzt sind alle Abhängigkeiten weg, wir können sicher die Atome löschen.
  // =========================================================================
  int is_flat_copy = 0;
  if ((*core)->str_ID) {
    // Check, ob wir selbst nur eine Flat Copy sind!
    is_flat_copy = (strcmp((*core)->str_ID, "@flat-copy") == 0);

    ATOM *current_elmt = (*core)->head;
    while (current_elmt != NULL) {
      ATOM *next_elmt = current_elmt->next; 

      if (current_elmt->type_data != 1 && current_elmt->elmt && current_elmt->elmt->head) {
        CORE *child_elmt = (CORE *)current_elmt->elmt->head->data;
        if (child_elmt) {
          if (child_elmt->member[TYP_CORE].Instance.Icore == NULL) return -2;

          // HACKER-SCHUTZ: Flat Copies löschen niemals rekursiv fremde Kinder!
          if (!is_flat_copy) {
              if (child_elmt->member[TYP_CORE].Instance.Icore->signature != 0xAAAAAAAA) {
                child_elmt->contain[TYP_CORE].Method.Mcore->downcast_core(child_elmt, child_elmt->typ);
              }
              destroy_core_(&child_elmt);
          }
        }
      }

      ATOM *rem_elmt = current_elmt;
      if (is_flat_copy) {
          // Wir sind eine Kopie: Nur Verknüpfung trennen (die eigenen ATOM-Knoten, die flat_copy_atom() gemacht hat)
          // Das ist ein Platzhalter für deine Ldestr_node Logik oder free(rem_elmt)
          rem_elmt->prev = NULL; 
          rem_elmt->next = NULL;
      } else {
	// Wir sind das Original: Feuer frei!
	rem_elmt->REM_ATOM(&rem_elmt);
      }
      rem_elmt = NULL;
      current_elmt = next_elmt;
    }
    // Root-Objekt downcasten, um abgeleitete Instanzen (Iobj, Iary, Igen) abzuräumen
    if ((*core)->member[TYP_CORE].Instance.Icore->signature != 0xAAAAAAAA) {
      (*core)->contain[TYP_CORE].Method.Mcore->downcast_core(*core, (*core)->typ);
    }


    // SPHERE COLLAPSE: Alle registrierten Deep-Copy-Nutzdaten auf einmal freigeben!
    if ((*core)->member[TYP_CORE].Instance.Icore->sphere != NULL) {
        collapse_sphere(&(*core)->member[TYP_CORE].Instance.Icore->sphere);
    }

    destroy_core_meth_inst(core);

    if ((*core)->str_ID) {
      free((*core)->str_ID);
      (*core)->str_ID = NULL;
    }
  }

  free(*core);
  *core = NULL;
  return 0;
}
/****************************************
 * Copy a primitive atom                *
 * Help Function for 'create_deep_copy' *
 * Returns the new element              *
 ***************************************/
ATOM *deep_copy_primitive_atom(ATOM *current_elmt, OBJ *sphere) {
    if (current_elmt == NULL || current_elmt->elmt == NULL || 
        current_elmt->elmt->head == NULL || current_elmt->elmt->tail == NULL) {
        return NULL;
    }

    /********************
     * alocate new atom *
     *******************/
    ATOM *new_elmt = init_atom();
    if (new_elmt == NULL) {
        return NULL;
    }

    new_elmt->ID = current_elmt->ID;
    new_elmt->type_data = current_elmt->type_data;
    
    if (current_elmt->elmt->name_value) {
        new_elmt->elmt->Lstr_id(
            &new_elmt->elmt,
            current_elmt->elmt->name_value
        );
    }
    /*********************************************************************
     * create two empty nodes for the new list from the primitive layout *
     ********************************************************************/
    new_elmt->elmt->Lins_node_next(&new_elmt->elmt, new_elmt->elmt->tail, NULL); // first node is head
    new_elmt->elmt->Lins_node_next(&new_elmt->elmt, new_elmt->elmt->tail, NULL); // second node is tail

    if (new_elmt->elmt->head == NULL || new_elmt->elmt->tail == NULL) {
        new_elmt->REM_ATOM(&new_elmt);
        return NULL;
    }

    
    if (current_elmt->elmt->head->data == NULL) {
        new_elmt->REM_ATOM(&new_elmt);
        return NULL;
    }
    int type_data_primv = *(int*)current_elmt->elmt->head->data;
    /************************************************
     * allocate memory for the new typ-node (head)  *
     ***********************************************/
    new_elmt->elmt->head->data = malloc(sizeof(int));
    if (new_elmt->elmt->head->data == NULL) {   
        new_elmt->REM_ATOM(&new_elmt);
        return NULL;
    }
    *(int*)new_elmt->elmt->head->data = type_data_primv;
    /*******************************************
     * copy the value in tail based on the typ *
     ******************************************/
    switch (type_data_primv) {
        case 1: // String-Typ
            if (current_elmt->elmt->tail->data != NULL) {
                new_elmt->elmt->tail->data = strdup((char *)current_elmt->elmt->tail->data);
                if (new_elmt->elmt->tail->data == NULL) {
                    new_elmt->REM_ATOM(&new_elmt);
                    return NULL;
                }
		 register_in_sphere(sphere, (char *)current_elmt->elmt->tail->data);
            }
            break;

        case 2: // Integer-Typ
            if (current_elmt->elmt->tail->data != NULL) {
                new_elmt->elmt->tail->data = malloc(sizeof(int));
                if (new_elmt->elmt->tail->data == NULL) {
                    new_elmt->REM_ATOM(&new_elmt);
                    return NULL;
                }
		register_in_sphere(sphere, (int *)current_elmt->elmt->tail->data);
                *(int*)new_elmt->elmt->tail->data = *(int*)current_elmt->elmt->tail->data;
            }
            break;

        case 3: // Float-Typ
            if (current_elmt->elmt->tail->data != NULL) {
                new_elmt->elmt->tail->data = malloc(sizeof(float));
                if (new_elmt->elmt->tail->data == NULL) {
                    new_elmt->REM_ATOM(&new_elmt);
                    return NULL;
                }
		register_in_sphere(sphere, (float *)current_elmt->elmt->tail->data);
                *(float*)new_elmt->elmt->tail->data = *(float*)current_elmt->elmt->tail->data;
            }
            break;

        case 4: // Generic Pointer-Typ (flat copy)
          new_elmt->elmt->tail->data = current_elmt->elmt->tail->data; // void ist nicht void (*void)
	  
            break;

        default:
            new_elmt->REM_ATOM(&new_elmt);
            return NULL;
    }

    return new_elmt;
}

/*******************************************
 * copy a not primitive atom               *
 * Help function for 'create_deep_copy'    *
 * Returns the new element                 *
 ******************************************/
ATOM *deep_copy_atom(ATOM *current_elmt) {
  if(!current_elmt || !current_elmt->elmt) {
    return NULL;
  }

  /*******************************
   * initialize new atom element *
   ******************************/
  ATOM *new_elmt = init_atom();
  if (!new_elmt) {
    return NULL;
  }

  /************************
   * copy identifications *
   ***********************/
  new_elmt->ID = current_elmt->ID;
  new_elmt->type_data = current_elmt->type_data;

  /**************************************
   * copy string ID or name of the list *
   *************************************/
  if (current_elmt->elmt->name_value) {
    new_elmt->elmt->Lstr_id(&new_elmt->elmt, current_elmt->elmt->name_value);
  }
  /************************************
   * init core container and chain it *
   ***********************************/
  CORE *new_cont = init_core();
  if (!new_cont) {
    new_elmt->REM_ATOM(&new_elmt);
    return NULL;
  }
  // upcast the 'CORE' object
  CORE *child_core = (CORE*)current_elmt->elmt->head->data;
  
  // Wenn das Original keine ID hat, geben wir ihm einen Fallback-Namen, damit strdup nicht stirbt
  char *safe_id = child_core->str_ID ? child_core->str_ID : "@deep-copy";
  new_cont->contain[TYP_CORE].Method.Mcore->upcast_core(
      &new_cont, child_core->typ, safe_id);

  
  if (new_elmt->elmt->Lins_node_next(&new_elmt->elmt, new_elmt->elmt->head, new_cont) != 0) {
    destroy_core_(&new_cont);
    new_elmt->REM_ATOM(&new_elmt);
    return NULL;
  }
 
  return new_elmt;
}

/***************
 * Deep - Copy *
 **************/
void create_deep_copy(CORE *orig, CORE **copy) {
    if (orig == NULL || copy == NULL) return;
    
    CORE *ptr_copy = *copy;
    if (ptr_copy == NULL) {
        ptr_copy = init_core();
        if (ptr_copy == NULL) return;
    }        
    *copy = ptr_copy;

    ATOM *current_elmt = orig->head;
    while (current_elmt != NULL) {
        CORE *child_core = NULL;
        if (current_elmt->elmt && current_elmt->elmt->head) {
            child_core = (CORE*)current_elmt->elmt->head->data;
        }

        ATOM *new_atom = (current_elmt->type_data == 1) ? 
	  deep_copy_primitive_atom(current_elmt,ptr_copy) : deep_copy_atom(current_elmt);

        if (new_atom == NULL) {
            current_elmt = current_elmt->next;
            continue;
        }

        /*****************************
         * Chain doubble linked list *
	 ****************************/
        if (ptr_copy->head == NULL) {
            ptr_copy->head = new_atom;
            ptr_copy->tail = new_atom;
            new_atom->next = NULL;
            new_atom->prev = NULL;
        } else {
            ptr_copy->tail->next = new_atom;
            new_atom->prev = ptr_copy->tail;
            ptr_copy->tail = new_atom;
            new_atom->next = NULL;
        }

        /******************
         * Recursive Call *
	 *****************/
        if (current_elmt->type_data != 1 && child_core && new_atom->elmt && new_atom->elmt->head) {
            CORE *new_child_core = (CORE*)new_atom->elmt->head->data;
            if (new_child_core) {
                create_deep_copy(child_core, &new_child_core);
            }
        }

        ptr_copy->size++;
        current_elmt = current_elmt->next;
    }

    /* if (ptr_copy->str_ID != NULL) { */
    /*   free(ptr_copy->str_ID); */
    /* } */
    /* ptr_copy->str_ID = orig->str_ID ? strdup(orig->str_ID) : NULL; */
    ptr_copy->typ = orig->typ;


   // Registry the deep copy - JETZT MIT NULL-CHECK!
    if (ptr_copy->str_ID != NULL && !strcmp(ptr_copy->str_ID,"@deep-copy")) { // SS
      
      LIST *registry_deep_copy = NULL;
      if (orig->member[TYP_CORE].Instance.Icore != NULL) {
	registry_deep_copy = orig->member[TYP_CORE].Instance.Icore->deep_copy;
      }
    
      if (registry_deep_copy == NULL ||
	  (registry_deep_copy->Lins_node_next(
					      &registry_deep_copy, registry_deep_copy->tail, ptr_copy)) != 0) {
	ptr_copy->contain[TYP_CORE].Method.Mcore->destroy_core(&ptr_copy);
	ptr_copy = NULL;
      }
    }
    *copy = ptr_copy;    
}

/**************************************************
 * Flat - Copy eines primitiven Atoms             *
 * Erstellt neuen Knoten, aber teilt die Pointer  *
 *************************************************/
ATOM *flat_copy_atom(ATOM *current_elmt) {
    if (current_elmt == NULL || current_elmt->elmt == NULL || 
        current_elmt->elmt->head == NULL || current_elmt->elmt->tail == NULL) {
        return NULL;
    }

    ATOM *new_elmt = init_atom();
    if (new_elmt == NULL) {
        return NULL;
    }

    new_elmt->ID = current_elmt->ID;
    new_elmt->type_data = current_elmt->type_data;
    
    if (current_elmt->elmt->name_value) {
        new_elmt->elmt->Lstr_id(&new_elmt->elmt, current_elmt->elmt->name_value);
    }

    // Zwei leere Nodes für das innere Element erzeugen
    new_elmt->elmt->Lins_node_next(&new_elmt->elmt, new_elmt->elmt->tail, NULL); // head
    new_elmt->elmt->Lins_node_next(&new_elmt->elmt, new_elmt->elmt->tail, NULL); // tail

    if (new_elmt->elmt->head == NULL || new_elmt->elmt->tail == NULL) {
        new_elmt->REM_ATOM(&new_elmt);
        return NULL;
    }
    
    if (current_elmt->elmt->head->data == NULL) {
        new_elmt->REM_ATOM(&new_elmt);
        return NULL;
    }

    // Typ-Indikator tief kopieren (ein int braucht echten neuen Speicher)
    int type_data_primv = *(int*)current_elmt->elmt->head->data;
    new_elmt->elmt->head->data = malloc(sizeof(int));
    if (new_elmt->elmt->head->data == NULL) {   
        new_elmt->REM_ATOM(&new_elmt);
        return NULL;
    }
    *(int*)new_elmt->elmt->head->data = type_data_primv;

    // HIER IST DER HACKER-MOVE (SHALLOW COPY):
    // Kein malloc, kein strdup. Wir kopieren einfach exakt die Speicheradresse
    // der Nutzdaten (String, Float, Int) aus dem Original in die Kopie.
    new_elmt->elmt->tail->data = current_elmt->elmt->tail->data;

    return new_elmt;
}

/********************************************
 * a flat copy is a capsularing of a object *
 *******************************************/
void create_flat_copy(CORE *orig, CORE *copy) {
  if (orig == NULL || copy == NULL) {
    return;
  }
  if (orig->str_ID != NULL && strcmp(orig->str_ID, "@flat-copy") == 0) {
    return;
  }
  copy->contain[TYP_CORE].Method.Mcore->upcast_core(&copy, TYP_OBJ,
                                                    "@flat-copy"); // sollte zuerst downgecastet werden oder gleich nochmal machen
  ATOM *ptr = orig->head;
  while (ptr != NULL) {
    switch (orig->typ) {
    case 1:;
      ATOM *flat_node = flat_copy_atom(ptr);
      copy->contain[TYP_OBJ].Method.Mobject->atom_next(copy, copy->tail, flat_node);
      break;
    case 2:copy->contain[TYP_OBJ].Method.Mobject->obj_next(copy,copy->tail,ptr->elmt->head->data);break;
    case 3:copy->contain[TYP_OBJ].Method.Mobject->array_next(copy,copy->tail,ptr->elmt->head->data);break;
    case 4:copy->contain[TYP_OBJ].Method.Mobject->gen_next(copy,copy->tail,ptr->elmt->head->data);break;
    default:
      return;
    }
    ptr = ptr->next;
  }
  // Registry the flat copy
  LIST *registry_flat_copy = NULL;
  if (orig->member[TYP_CORE].Instance.Icore != NULL) {
    registry_flat_copy = orig->member[TYP_CORE].Instance.Icore->flat_copy;
  }
  if (registry_flat_copy == NULL || (registry_flat_copy->Lins_node_next(
									&registry_flat_copy, registry_flat_copy->tail, copy)) != 0) {
    copy->contain[TYP_CORE].Method.Mcore->destroy_core(&copy);
    copy = NULL;
  }
}

/*****************
 ***   O B J   ***
 ****************/

const static OBJ_VTABLE global_obj_vtable = {
    .rem_atom = rem_atom_,
    .obj_next = obj_ins_obj_next,
    .obj_prev = obj_ins_obj_prev,
    .array_next = array_ins_obj_next,
    .array_prev = array_ins_obj_prev,
    .gen_next = gen_ins_obj_next,
    .gen_prev = gen_ins_obj_prev,
    .atom_next = atom_ins_next,
    .atom_prev = atom_ins_prev,
    .getf_obj = getf_obj
};


int init_obj(OBJ **object, char *ID) {
  int is_new_core =  0;
  if ((*object) == NULL) {
    *object = init_core();
    if ((*object) == NULL) {
      return -1;
    }
    is_new_core = 1;
  }

  OBJ *ptr_object = *object;

  switch (ptr_object->member[TYP_CORE].Instance.Icore->signature) {
  case 0xAAAAAAAA: break; //Typ CORE ---> T
  case 0xBBBBBBBB: return 2; //Typ OBJ ---> schon ein OBJ
  case 0xCCCCCCCC: return 3; //Typ ARY F
  case 0xDDDDDDDD: return 4; //Typ GEN F
  default :return -3;
  }

  int id_set_here = 0;
  if (ptr_object->str_ID == NULL) {
    if ((ptr_object->str_ID = strdup(ID)) == NULL) {
      if (is_new_core) {
	destroy_core_(&ptr_object);
        *object = NULL;
      }
      return -4;
    }
    id_set_here = 1;
  
  } else { 
    return -5;
  }
  
  if ((ptr_object->member[TYP_OBJ].Instance.Iobj =
           malloc(sizeof(OBJ_INSTANCE))) == NULL) {
    if (id_set_here) {
      free(ptr_object->str_ID);
      ptr_object->str_ID = NULL;
    }

    if (is_new_core) {
      destroy_core_(&ptr_object);
      *object = NULL;
    }
    return -5;
  }
    
  
  ptr_object->typ = TYP_OBJ;
  ptr_object->head = NULL;
  ptr_object->tail = NULL;
  ptr_object->size = 0;

  ptr_object->contain[TYP_OBJ].Method.Mobject = &global_obj_vtable; 
  ptr_object->member[TYP_OBJ].Instance.Iobj->signature = 0xBBBBBBBB;
  ptr_object->member[TYP_OBJ].Instance.Iobj->data_gen = NULL;
  *object = ptr_object;
  return 0;
}

int rem_atom_(OBJ *object, ATOM *elmt) {
  if (object) {
    if ((!object->head && !object->tail) || !elmt) 
      return -1;
  }else{
    return -2;
  }
  
  if (elmt == (ATOM*)object->head) {
    object->head = elmt->next;
    if(!object->head) {
      object->tail = NULL;
    }else{
      elmt->next->prev = NULL;
    }
  }else{
    elmt->prev->next = elmt->next;
    if(!elmt->next) {
      object->tail = elmt->prev;
    }else{
      elmt->next->prev = elmt->prev;
    }
  }
  elmt->REM_ATOM(&elmt); 
  elmt = NULL;
  object->size --;
  return 0;
}


int atom_ins_next(OBJ *object, ATOM *node, ATOM *data) { 
  if (object == NULL) { 
    return -1;
  }
    
  ATOM *new_elmt = data; 
  if (data == NULL) { 
    return 0; 
  } else { 
    new_elmt->type_data = 1; 
    if (object->size == 0) { 
      object->head = new_elmt; 
      object->tail = new_elmt; 
      object->head->prev = NULL; 
      object->tail->next = NULL; 
    } else { 
      new_elmt->next = node->next; 
      new_elmt->prev = node; 
      if (new_elmt->next == NULL) { 
	object->tail = new_elmt; 
      } else { 
	new_elmt->next->prev = new_elmt; 
      } 
      node->next = new_elmt; 
    } 
    new_elmt->ID = object->size++; 
    return object->size;
  } 
} 

int atom_ins_prev(OBJ *object, ATOM *node, ATOM *data) {

  if (object == NULL) {
    return -1;
  }

  ATOM *new_elmt = data;
  if (data == NULL) {
    return 0;
  } else {
    new_elmt->type_data = 1;
    if (object->size == 0) {
      object->head = new_elmt;
      object->tail = new_elmt;
      object->head->prev = NULL;
      object->tail->next = NULL;
    } else {
      new_elmt->prev = node->prev;
      new_elmt->next = node;
      if (new_elmt->prev == NULL) {
	object->head = new_elmt;
      } else {
	new_elmt->prev->next = new_elmt;
      }
      node->prev = new_elmt;
    }
    new_elmt->ID = object->size++;
    return object->size;
  }
}

OBJ *obj_ins_obj_next(OBJ *object, ATOM *node, OBJ *data) {

  if (object == NULL || (node == NULL && object->size != 0)) {
    return NULL;
  }
  ATOM *new_elmt = init_atom();
  OBJ *ptr_data = data;
  // wenn das Objekt auf sich selbst referenziert
  if (object == data) {
    OBJ *new_dcopy = NULL;
    data->contain[TYP_CORE].Method.Mcore->create_deep_copy(data, &new_dcopy);
    ptr_data = new_dcopy;
  }
  new_elmt->elmt->Lins_node_next(&new_elmt->elmt, new_elmt->elmt->tail,ptr_data);     
  new_elmt->type_data = 2;
  if (object->size == 0) {
    object->head = new_elmt;
    object->tail = new_elmt;
    object->head->prev = NULL;
    object->tail->next = NULL;
  } else {
    new_elmt->next = node->next;
    new_elmt->prev = node;
    if (new_elmt->next == NULL) {
      object->tail = new_elmt;
    }else{
      new_elmt->next->prev = new_elmt;
    }
    node->next = new_elmt;
  }
  new_elmt->ID = object->size++;
    
  return object;
}

OBJ *obj_ins_obj_prev(OBJ *object, ATOM *node, OBJ *data) {
  if (object == NULL || (node == NULL && object->size != 0)) {
    return NULL;
  }
  ATOM *new_elmt = init_atom();
  OBJ *ptr_data = data;
  if (object == data) {
    OBJ *new_dcopy = NULL;
    data->contain[TYP_CORE].Method.Mcore->create_deep_copy(data, &new_dcopy);
    ptr_data = new_dcopy;
  }
  new_elmt->elmt->Lins_node_next(&new_elmt->elmt, new_elmt->elmt->tail, ptr_data);     
  new_elmt->type_data = 2;

  if (object->size == 0) {
    object->head = new_elmt;
    object->tail = new_elmt;
    new_elmt->prev = NULL;
    new_elmt->next = NULL;
  } else {
    new_elmt->next = node;
    new_elmt->prev = node->prev;
    if (node->prev == NULL) {
      object->head = new_elmt;
    } else {
      node->prev->next = new_elmt;
    }
    node->prev = new_elmt;
  }
  new_elmt->ID = object->size++;
  return object;
}

OBJ *array_ins_obj_next(OBJ *object, ATOM *node, ARY *data) {
  if (object == NULL || node == NULL || data == NULL) {
    return NULL;
  }

  OBJ *ptr_obj = object;
  object = obj_ins_obj_next(ptr_obj, node, (OBJ *)data);
  node->next->type_data = TYP_ARY;
  data->typ = TYP_ARY;
  return object;
}


OBJ *array_ins_obj_prev(OBJ *object, ATOM *node, ARY *data) {
  if (object == NULL || node == NULL || data == NULL) {
    return NULL;
  }

  OBJ *ptr_obj = object;
  object = obj_ins_obj_prev(ptr_obj, node, (OBJ *)data);
  node->next->type_data = TYP_ARY;
  data->typ = TYP_ARY;
  return object;
}
  int proof_gen_object_recursive(LIST *symlist, NODE *start) {
  if (symlist == NULL) return -1;
  if (start == NULL) return 0;

  if (start->data == NULL || strcmp((char*)start->data, "S") != 0 || symlist->size < 2) {
    return -2; 
  }

  NODE *typ_data = start->next;
  NODE *primitives = (typ_data != NULL) ? typ_data->next : NULL;
  
  int count_prameter = 0;
  int toggler = 0;

  while (typ_data != NULL && primitives != NULL && toggler < 2) {

    if (typ_data->data != NULL && !strcmp((char *)typ_data->data, "S")) {
      int ret = proof_gen_object_recursive(symlist, typ_data);
      if (ret != 0) return ret; 
      
      ++toggler;
      
      // SEGFAULT FIX: Die Child-Rekursion hat die Liste bereits geprüft.
      // Wenn wir hier den Pointer manuell weiterdrehen, kracht es.
      // Wir sind hier siegreich fertig -> Sofortiger Ausstieg!
      return 0; 
    }

    if (typ_data->data != NULL && !strcmp((char *)typ_data->data, "#")) {
      primitives = primitives->next;
      typ_data = typ_data->next;
      continue; 
    }

    if (typ_data->data == NULL) return -4;

    switch (*(int*)typ_data->data) {
    case 1: 
    case 2:
    case 3:
      ++count_prameter;
      break;
    case 4:
      if (primitives->data != NULL && ((GEN_FP *)primitives->data)->signature > 4) {
        if (((GEN_FP *)primitives->data)->parameter_count != count_prameter) {
          return -3;
        }
        if (symlist->head == NULL || symlist->head->next == NULL || symlist->head->next->data == NULL) {
            return -3;
        }
        if (memcmp((void*)&((GEN_FP *)primitives->data)->return_type, symlist->head->next->data, sizeof(int))) {
          return -3;
        }
        // ERFOLG! Die Signatur ist gültig. Wir brechen hier sofort erfolgreich ab!
        return 0; 
      }
      break;
    default:
      return -4;
    }

    typ_data = typ_data->next ? typ_data->next->next : NULL;
    primitives = primitives->next ? primitives->next->next : NULL;
  }  

  return 0; 
}
int phrase_gen(OBJ *object) {
  if (object == NULL) {
    return -1;
  }
  LIST *symlist = NULL;
  init_list(&symlist);
  create_symlist(object, &symlist);
  
  int result = proof_gen_object_recursive(symlist, symlist->head);
  
  // Transaktions-Ende: Der Besitzer der Liste räumt auf
  symlist->Ldestr_list(&symlist);
  
  return result;
}

OBJ *gen_ins_obj_next(OBJ *object, ATOM *node, GEN *data) {
  if (object == NULL || (node == NULL && object->size != 0) || data == NULL) {
    return NULL;
  }
  
  // BUGFIX: data ist jetzt ein GEN. Iobj existiert nicht mehr, wir prüfen Igen!
  if (data->member[TYP_GEN].Instance.Igen == NULL) {
    return NULL;
  }
  
  if (phrase_gen((OBJ *)data) != 0) {
      return NULL; // Signatur ist ungültig
  }
  
  OBJ *ptr_obj = object;
  object = obj_ins_obj_next(ptr_obj, node, (OBJ *)data);
  return object;
}

// Dasselbe für gen_ins_obj_prev:
OBJ *gen_ins_obj_prev(OBJ *object, ATOM *node, GEN *data) {
  if (object == NULL || (node == NULL && object->size != 0) || data == NULL) {
    return NULL;
  }
  
  if (data->member[TYP_GEN].Instance.Igen == NULL) {
    return NULL;
  }

  if (phrase_gen((OBJ *)data) != 0) {
      return NULL; 
  }

  OBJ *ptr_obj = object;
  object = obj_ins_obj_prev(ptr_obj, node, (OBJ *)data);
  return object;
}
int create__ID(OBJ *ob,char *pattern) {
    if (!ob || !pattern)
	return -1;
    ob->str_ID = strdup(pattern);
    if (ob->str_ID == NULL) {
      return -2;
    }
    return 0;
}


/**************************
 * M E T O D S  A R R A Y *
 *************************/

static ARY_VTABLE global_ary_vtable = {.ary_typ_ary = def_arrayARY,
                                       .ary_typ_atom = def_arrayATOM,
                                       .ary_typ_obj = def_arrayOBJ,
                                       .setf_array = setf_array,
                                       .get_by_index = get_index};


int init_array(ARY **array, char *ID) {
  int is_new_core = 0;
  if ((*array) == NULL) {
    *array = init_core();
    if ((*array) == NULL) {
      return -1;
    }
    is_new_core = 1; // Wir haben die Hülle erschaffen
  }

  ARY *ptr_array = *array;

  switch (ptr_array->member[TYP_CORE].Instance.Icore->signature) {
  case 0xAAAAAAAA: break;      // Typ CORE ---> T
  case 0xBBBBBBBB: return -2;  // Typ OBJ F
  case 0xCCCCCCCC: return 333; // Typ ARY F
  case 0xDDDDDDDD: return 444; // Typ GEN F
  default: return -3;
  }

  int id_set_here = 0;
  if (ptr_array->str_ID == NULL) {
    if ((ptr_array->str_ID = strdup(ID)) == NULL) {
      // Rollback bei strdup-Fehler
      if (is_new_core) {
        destroy_core_(&ptr_array);
        *array = NULL;
      }
      return -4;
    }
    id_set_here = 1; // Wir haben die ID gesetzt
  } else {
    return -5;
  }

  if ((ptr_array->member[TYP_ARY].Instance.Iary = malloc(sizeof(ARY_INSTANCE))) == NULL) {
    // Rollback bei malloc-Fehler: Nur aufräumen, was WIR verbrochen haben
    if (id_set_here) {
      free(ptr_array->str_ID);
      ptr_array->str_ID = NULL;
    }
    if (is_new_core) {
     destroy_core_(&ptr_array);
      *array = NULL;
    }
    return -5;
  }

  ptr_array->typ = TYP_ARY;
  ptr_array->head = NULL;
  ptr_array->tail = NULL;
  ptr_array->size = 0;

  ptr_array->contain[TYP_ARY].Method.Marray = &global_ary_vtable;
  ptr_array->member[TYP_ARY].Instance.Iary->signature = 0xCCCCCCCC;
  ptr_array->member[TYP_ARY].Instance.Iary->typ_elements = 0;
  
  *array = ptr_array;
  return 0;
}
ATOM *create_atom(CORE *core,ATOM *old_atom) {
  ATOM *new_atom = NULL;
  new_atom = deep_copy_primitive_atom(old_atom,core->member[TYP_CORE].Instance.Icore->sphere);
  return new_atom;
}

OBJ *create_obj(OBJ *old_obj)  {
  OBJ *new_obj = NULL;
  init_obj(&new_obj,"@deep-copy");
  if (new_obj == NULL) {
      return NULL;
  }
  create_deep_copy(old_obj, &new_obj);
  return new_obj;
}

ARY *create_ary(ARY *old_ary) {
  ARY *new_ary = NULL;
  init_array(&new_ary,"@deep-copy");
  if (new_ary == NULL) {
      return NULL;
  }
  create_deep_copy(old_ary, &new_ary);
  return new_ary;
}

//create_gen ()

int def_arrayATOM(ARY **array, int size, ATOM *data) {
    if (*array == NULL || size == 0 || data == NULL) {
	return -1;
    }
    ARY *ptr_array = *array;
    ptr_array->member[TYP_ARY].Instance.Iary->pos = malloc(sizeof(void*) * size);
    ATOM *temp = NULL;
    int s = 0;
    for (s = 0; s < size; s++) {
      if (atom_ins_next(ptr_array, ptr_array->tail, temp = create_atom((CORE *)ptr_array,data)) < 0 || temp == NULL) {
	    return -2;
        } else {
	    ptr_array->member[TYP_ARY].Instance.Iary->pos[s] = temp; 
        }
    }
    switch (*(int *)data->elmt->head->data) {
    case 1:ptr_array->member[TYP_ARY].Instance.Iary->typ_elements = 11;break;
    case 2:ptr_array->member[TYP_ARY].Instance.Iary->typ_elements = 12;break;
    case 3:ptr_array->member[TYP_ARY].Instance.Iary->typ_elements = 13;break;
    case 4:ptr_array->member[TYP_ARY].Instance.Iary->typ_elements = 14;break;
    default : return -2;
    }
    
    *array = ptr_array;
    return 0;
}

int def_arrayOBJ(ARY **array, int size, OBJ *data) {
    if (array == NULL || size == 0 || data == NULL) {
	return -1;
    }
    ARY *ptr_array = *array;
    ptr_array->member[TYP_ARY].Instance.Iary->pos = malloc(sizeof(void*) * size);
    OBJ *temp = NULL;
    int s = 0;
    for (s = 0; s < size; s++) {
        if (obj_ins_obj_next(ptr_array,ptr_array->tail,temp = create_obj(data)) == NULL || temp == NULL) {
            return -2;
        } else {
	    ptr_array->member[TYP_ARY].Instance.Iary->pos[s] = temp; 
        }
    }
    ptr_array->member[TYP_ARY].Instance.Iary->typ_elements = TYP_OBJ;
    *array = ptr_array;
    return 0;
}

int def_arrayARY(ARY **array, int size, ARY *data) {
    if (array == NULL || size == 0 || data == NULL) {
	return -1;
    }
    ARY *ptr_array = *array;
    ptr_array->member[TYP_ARY].Instance.Iary->pos = malloc(sizeof(void*) * size);
    OBJ *temp = NULL;
    int s = 0;
    for (s = 0; s < size; s++) {
        if (obj_ins_obj_next(ptr_array,ptr_array->tail,temp = create_ary(data)) == NULL || temp == NULL) {
            return -2;
        } else {
	    ptr_array->member[TYP_ARY].Instance.Iary->pos[s] = temp; 
        }
    }
    ptr_array->member[TYP_ARY].Instance.Iary->typ_elements = TYP_ARY; 
    *array = ptr_array;
    return 0;
}

// def_genARY () 

void *get_index(ARY *array, int index) {
  if (array == NULL || array->size == 0) {
      return NULL;
  }
  int TYP = array->member[TYP_ARY].Instance.Iary->typ_elements;
  
  if (index < 0 || index >= array->size)
    return NULL;
  if (TYP > 10) {
    switch (TYP) {
    case 11:return (char **)array->member[TYP_ARY].Instance.Iary->pos[index];
    case 12:return(int*)array->member[TYP_ARY].Instance.Iary->pos[index];
    case 13:return (float*)array->member[TYP_ARY].Instance.Iary->pos[index];
    case 14:return (void*)array->member[TYP_ARY].Instance.Iary->pos[index]; // SS gen!!!
    }
  }
  switch (TYP) {
  case 0:
    return (CORE *)array->member[TYP_ARY].Instance.Iary->pos[index];
  case 1:
    return (OBJ *)array->member[TYP_ARY].Instance.Iary->pos[index];
  case 2:
    return (ARY *)array->member[TYP_ARY].Instance.Iary->pos[index];
  default:
    return NULL;
    
  }
    
}

/*************************************************************
 * setf_array() :
 * S -> start primitivs
 * $ -> platzhalter d. h. Wert wird nicht verändert
 * %i -> intenger; %f -> float; %s -> string / char %g -> gen
 ************************************************************/

void create_flatlist(CORE *core, LIST **flatlist) {
  if (core == NULL || *flatlist == NULL) return;

  ATOM *current_elmt = core->head;
  // Signalisiere den Beginn eines neuen Scopes / Containers
  (*flatlist)->Lins_node_next(flatlist, (*flatlist)->tail, strdup("S"));

  while (current_elmt != NULL) {
    if (current_elmt->type_data == 1) { 
      // Primitiv: Schreibe den echten Wert direkt in die flache Liste
      if (current_elmt->elmt && current_elmt->elmt->tail) {
	(*flatlist)->Lins_node_next(flatlist, (*flatlist)->tail, current_elmt->elmt->tail->data);
      }
    } else {
      // Komplexes Objekt/Array (type_data > 1): Rekursiv abtauchen
      CORE *child_core = (CORE*)current_elmt->elmt->head->data;
      if (child_core) {
	create_flatlist(child_core, flatlist);
      }
    }
    current_elmt = current_elmt->next;
  }
}

void create_symlist(OBJ *obj, LIST **symlist) {
  if (obj == NULL || *symlist == NULL) {
    return;
  }

  ATOM *current_elmt = obj->head;
  (*symlist)->Lins_node_next(symlist, (*symlist)->tail, strdup("S"));

  while (current_elmt != NULL) {
    if (current_elmt->type_data == 1) {
      if (current_elmt->elmt && current_elmt->elmt->tail &&
          current_elmt->elmt->head) {
        (*symlist)->Lins_node_next(symlist, (*symlist)->tail,
                                   current_elmt->elmt->head->data);
        (*symlist)->Lins_node_next(symlist, (*symlist)->tail,
                                   current_elmt->elmt->tail->data);
      }
    } 
    // HACKER-SCHUTZ FIX: Wir lassen jetzt OBJ (2) UND ARY (3) in die Rekursion!
    else if (current_elmt->type_data == 2 || current_elmt->type_data == 3) {
      if (current_elmt->elmt != NULL && current_elmt->elmt->head != NULL) {
          CORE *child_core = (CORE *)current_elmt->elmt->head->data;
          if (child_core) {
            // Wir nutzen '#' weiterhin als Indikator für komplexe Sub-Strukturen, 
            // da setf_array und getf_obj darauf geparst sind
            (*symlist)->Lins_node_next(symlist, (*symlist)->tail, strdup("#"));
            create_symlist(child_core, symlist); 
          }
      }
    }
    current_elmt = current_elmt->next;
  } 
}

/* int setf_obj(OBJ *obj, char *format_str, void *format, ...) {}  wird nicht benötigt !!*/

// HACKER-SCHUTZ 1: Keine void-Pointer als Platzhalter in der Signatur!
int setf_array(ARY *array, char *format_str, ...) {

  if (array == NULL || format_str == NULL) {
    return -1;
  }

  LIST *symlist = NULL;
  init_list(&symlist);
  create_symlist((OBJ*)array, &symlist);
  if (symlist->head == NULL && symlist->tail == NULL) {
    symlist->Ldestr_list(&symlist);
    return -2;
  }
  
  int size_args = symlist->size, counter = 0;
  char *ptr_format = format_str;
  NODE *ptr_symlist = symlist->head;
  
  // HACKER-SCHUTZ 2: Saubere Initialisierung der variadischen Liste
  va_list args;
  va_start(args, format_str);

  for (; *ptr_format != '\0' && counter < size_args; ptr_format++, counter++) {
    while (ptr_symlist != NULL) {        
      if (*ptr_format == 'S') {
        ++ptr_format;
        if (ptr_symlist->data != NULL && *(char*)ptr_symlist->data == 'S') {
          ptr_symlist = ptr_symlist->next;
          continue;
        } else {
          va_end(args);
          symlist->Ldestr_list(&symlist);
          return 666;
        }
      }
      if (ptr_symlist->data != NULL && *(char*)ptr_symlist->data == '#') {
        ptr_symlist = ptr_symlist->next;
        continue;
      }
      break;
    }

    // Sicherheitscheck: Sind wir ans Ende der AST-Liste gefallen?
    if (ptr_symlist == NULL || ptr_symlist->next == NULL) break;

    if (*ptr_format == '%') {
      ++ptr_format;
      if (*ptr_format == '$') {
        // Platzhalter überspringen: Wir müssen Typ UND Wert überspringen (2 Nodes)
        ptr_symlist = ptr_symlist->next->next; 
        continue;
      }
      
      // Zur besseren Lesbarkeit: Typ und Wert trennen
      NODE *node_type = ptr_symlist;
      NODE *node_value = ptr_symlist->next;

      switch (*ptr_format) {
      case 'i':
        *((int *)node_type->data) = ID_INT;
        // HACKER-SCHUTZ 3: Integer direkt als Wert vom Stack ziehen
        *((int *)node_value->data) = va_arg(args, int);
        break;
        
      case 'f':
        *((int *)node_type->data) = ID_FLT;
        // HACKER-SCHUTZ 4: Floats IMMER als double vom Stack ziehen und dann casten!
        *((float *)node_value->data) = (float)va_arg(args, double);
        break;
        
      case 'g':
      case 's':
        if (*(int*)node_type->data != ID_GEN) {
          *((int *)node_type->data) = ID_STR;
        }
        // String-Pointer vom Stack ziehen
        char *str_val = va_arg(args, char*);
        
        // Alten String sicher freigeben (Memory Leak verhindern)
        if (node_value->data != NULL) {
          free(node_value->data);
        }
        node_value->data = str_val ? strdup(str_val) : NULL;
        break;
        
      default:
        va_end(args);
        symlist->Ldestr_list(&symlist);
        return -3;
      }
      
      // Da wir Typ und Wert abgearbeitet haben, springen wir 2 Knoten weiter
      ptr_symlist = node_value->next; 
    } else if (*ptr_format == '$' || *ptr_format == '#') {
      // Wenn wir im Formatstring Platzhalter ignorieren, müssen wir sie auch vom va_args Stack räumen!
      (void)va_arg(args, void*);
    }
  }
  
  va_end(args);
  symlist->Ldestr_list(&symlist);
  return 0;
}
// Signatur anpassen: Keine expliziten void* Pointer mehr!
int getf_obj(OBJ *obj, char *format_str, ...) {
  if (obj == NULL || format_str == NULL) {
    return 0;
  }

  LIST *symlist = NULL;
  init_list(&symlist);
  create_symlist(obj, &symlist); 
  
  if (symlist->head == NULL && symlist->tail == NULL) {
    symlist->Ldestr_list(&symlist);
    return -2;
  }
  
  int size_args = symlist->size, counter = 0;
  char *ptr_format = format_str;
  NODE *ptr_symlist = symlist->head;
  
  va_list args;
  va_start(args, format_str);

  int val_format_str = 0; 
  
  for (; *ptr_format != '\0' && counter < size_args; ptr_format++, counter++) {
    while (ptr_symlist != NULL) {
      
      // BUGFIX: ZUERST nach '#' suchen und überspringen!
      // Wenn wir in ein Child-Objekt abtauchen, muss das '#' ignoriert werden,
      // BEVOR wir das 'S' (Scope) abgleichen!
      if (ptr_symlist->data != NULL && *(char *)ptr_symlist->data == '#') {
        ptr_symlist = ptr_symlist->next;
        continue;
      }

      // DANN erst das 'S' abgleichen
      if (*ptr_format == 'S') {
        ++ptr_format;
        if (ptr_symlist->data != NULL && *(char*)ptr_symlist->data == 'S') {
          ptr_symlist = ptr_symlist->next;
          continue;
        } else {
          va_end(args);
          symlist->Ldestr_list(&symlist);
          return 666;
        }
      }
      
      break;
    }
	
    // Sicherheitscheck für das Listenende
    if (ptr_symlist == NULL || ptr_symlist->next == NULL) break;

    if (*ptr_format == '%') {
      ++ptr_format;
      if (*ptr_format == '$') {
        // Platzhalter: Wir überspringen in der Liste Typ UND Wert (2 Nodes)
        ptr_symlist = ptr_symlist->next->next;
        continue;
      }
      
      NODE *node_type = ptr_symlist;
      NODE *node_value = ptr_symlist->next;

      switch (*ptr_format) {
      case 'g':
      case 's':
        val_format_str = ID_STR;
        if (val_format_str != *(int *)node_type->data) {
          // HACKER-SCHUTZ: Format-String niemals überschreiben! Abbruch.
          va_end(args);
          symlist->Ldestr_list(&symlist);
          return -5; 
        } else {
          char **target = va_arg(args, char **);
          if (target != NULL) {
            if (*target != NULL) {
              free(*target);
              *target = NULL;
            }
            *target = strdup((char*)node_value->data);
          }
        }
        break;
        
      case 'i':
        val_format_str = ID_INT;
        if (val_format_str != *(int *)node_type->data) {
          va_end(args);
          symlist->Ldestr_list(&symlist);
          return -5;
        } else {
          // Wir holen uns die ZIELADRESSE vom Stack und schreiben dort rein
          int *target = va_arg(args, int *);
          if (target != NULL) *target = *((int*)node_value->data);
        }
        break;
        
      case 'f':
        val_format_str = ID_FLT;
        if (val_format_str != *(int *)node_type->data) {
          va_end(args);
          symlist->Ldestr_list(&symlist);
          return -5;
        } else {
          float *target = va_arg(args, float *);
          if (target != NULL) *target = *((float *)node_value->data);
        }
        break;
        
      default:
        va_end(args);
        symlist->Ldestr_list(&symlist);
        return -6;
      }
      ptr_symlist = node_value->next;
      
    } else if (*ptr_format == '$' || *ptr_format == '#') {
      // Unbenutzte Parameter sicher vom Stack ignorieren
      (void)va_arg(args, void*);
    }
  }
  
  va_end(args);
  symlist->Ldestr_list(&symlist);
  return 0;
}

// ====================================================================
// ===================== AB HIER WIRD ES KRASS ========================
// ====================================================================

void append_to_string(char **dest, size_t *current_size, const char *src) {
    size_t src_len = strlen(src);
    size_t dest_len = *dest ? strlen(*dest) : 0;
    
    if (dest_len + src_len + 1 > *current_size) {
        *current_size = (*current_size * 2) + src_len + 1;
        *dest = realloc(*dest, *current_size);
    }
    
    if (dest_len == 0) {
        (*dest)[0] = '\0';
    }
    strcat(*dest, src);
}

void obj_to_json_recursive(OBJ *obj, char **buffer, size_t *buffer_size, size_t *offset) {
    if (obj == NULL) {
        append_to_string(buffer, buffer_size, "null");
        return;
    }

    // 1. Array-Erkennung: Wir prüfen den echten Core-Typ
    int is_array = (obj->typ == TYP_ARY);

    // 2. Formatierung anpassen: Eckige Klammern für Arrays
    if (is_array) {
        append_to_string(buffer, buffer_size, "[");
    } else {
        append_to_string(buffer, buffer_size, "{");
    }
    *offset = strlen(*buffer);

    ATOM *current_atom = obj->head;
    while (current_atom != NULL) {
        
        // 3. Keys NUR drucken, wenn es KEIN Array ist
        // (verhindert, dass interne Platzhalter wie "@ary_el" im JSON landen)
        if (!is_array) {
            char *key = current_atom->elmt->name_value;
            char temp_buffer[512];
            snprintf(temp_buffer, sizeof(temp_buffer), "\"%s\": ", key ? key : "unknown");
            append_to_string(buffer, buffer_size, temp_buffer);
        }

        if (current_atom->type_data == 1) { 
            int token = *(int*)current_atom->elmt->head->data;
            void *data = current_atom->elmt->tail->data;
            char temp_buffer[512];

            switch (token) {
            case ID_STR:
                snprintf(temp_buffer, sizeof(temp_buffer), "\"%s\"", (char*)data);
                break;
            case ID_INT:
                snprintf(temp_buffer, sizeof(temp_buffer), "%d", *(int*)data);
                break;
            case ID_FLT:
                snprintf(temp_buffer, sizeof(temp_buffer), "%f", *(float*)data);
                break;
            case ID_GEN:
                snprintf(temp_buffer, sizeof(temp_buffer), "\"native_code_ptr(%p)\"", data); 
                break;
            default:
                snprintf(temp_buffer, sizeof(temp_buffer), "null");
                break;
            }
            append_to_string(buffer, buffer_size, temp_buffer);

        // 4. HACKER-SCHUTZ: Rekursion für Objekte (2) UND Arrays (3) erlauben!
        } else if (current_atom->type_data == 2 || current_atom->type_data == 3) { 
            OBJ *child_obj = (OBJ*)current_atom->elmt->head->data;
            obj_to_json_recursive(child_obj, buffer, buffer_size, offset);
        }

        current_atom = current_atom->next;
        if (current_atom != NULL) {
            append_to_string(buffer, buffer_size, ", ");
        }
    }

    // 5. Array-Klammer wieder richtig schließen
    if (is_array) {
        append_to_string(buffer, buffer_size, "]");
    } else {
        append_to_string(buffer, buffer_size, "}");
    }
    *offset = strlen(*buffer);
}

char* obj_to_json_string(OBJ *obj) {
    size_t buffer_size = 1024;
    char *buffer = malloc(buffer_size);
    if (!buffer) return NULL;
    buffer[0] = '\0';
    
    size_t offset = 0;
    obj_to_json_recursive(obj, &buffer, &buffer_size, &offset);
    
    return buffer; 
}


// --- SICHERER PARSER ---

// Hilfsfunktion: Überspringt NUR echte Whitespaces, keine Syntax-Zeichen!
void skip_whitespace(const char **str) {
    while (**str && isspace((unsigned char)**str)) {
        (*str)++;
    }
}

// Hilfsfunktion: Extrahiert Strings (Keys oder Werte)
char* parse_json_string(const char **str) {
    skip_whitespace(str);
    if (**str != '"') return NULL;
    (*str)++; // Überspringe öffnendes '"'
    
    const char *start = *str;
    while (**str && **str != '"') {
        if (**str == '\\' && *(*str + 1) != '\0') {
            (*str) += 2; // Escaped Zeichen (\") sicher übergehen
        } else {
            (*str)++;
        }
    }
    
    size_t len = *str - start;
    char *res = malloc(len + 1);
    if (res) {
        memcpy(res, start, len);
        res[len] = '\0';
    }
    
    if (**str == '"') (*str)++; // Überspringe schließendes '"'
    return res;
}

// HACKER-SCHUTZ: Maximales Nesting-Level definieren
#define MAX_JSON_DEPTH 128
int json_string_to_core_recursive(const char **str, CORE **target_core, int depth) {
    if (depth > MAX_JSON_DEPTH) return -99; 
    
    skip_whitespace(str);
    if (!**str) return -1;

    // ================= OBJEKT PARSEN =================
    if (**str == '{') {
        (*str)++; 
        
        OBJ *obj = (OBJ*)*target_core;
        if (obj == NULL) {
            init_obj(&obj, "PARSED_OBJ");
            *target_core = (CORE*)obj;
        }

        while (**str) {
            skip_whitespace(str);
            if (**str == '}') {
                (*str)++; 
                break;
            }

            char *key = parse_json_string(str);
            if (!key) return -2;

            skip_whitespace(str);
            if (**str != ':') {
                free(key);
                return -3; 
            }
            (*str)++; 
            skip_whitespace(str);

            if (**str == '{') {
                OBJ *child_obj = NULL;
                int res = json_string_to_core_recursive(str, (CORE**)&child_obj, depth + 1);
                if (res != 0) { free(key); return res; }
                
                obj->contain[TYP_OBJ].Method.Mobject->obj_next(obj, obj->tail, child_obj);
                obj->tail->elmt->Lstr_id(&obj->tail->elmt, key);

            } else if (**str == '[') {
                ARY *child_ary = NULL;
                int res = json_string_to_core_recursive(str, (CORE**)&child_ary, depth + 1);
                if (res != 0) { free(key); return res; }

                obj->contain[TYP_OBJ].Method.Mobject->array_next(obj, obj->tail, child_ary);
                obj->tail->elmt->Lstr_id(&obj->tail->elmt, key);

            } else if (**str == '"') {
                char *val_str = parse_json_string(str);
                ATOM *a_str = init_atom();
                a_str->SETF_ATOM(a_str, ID_STR, key, val_str);
                obj->contain[TYP_OBJ].Method.Mobject->atom_next(obj, obj->tail, a_str);

            } else if (isdigit((unsigned char)**str) || **str == '-') {
                const char *start = *str;
                int is_float = 0;
                while (**str && (isdigit((unsigned char)**str) || **str == '.' || **str == '-')) {
                    if (**str == '.') is_float = 1;
                    (*str)++;
                }
                
                ATOM *a_num = init_atom();
                if (is_float) {
                    float *val_flt = malloc(sizeof(float));
                    *val_flt = strtof(start, NULL);
                    a_num->SETF_ATOM(a_num, ID_FLT, key, val_flt);
                } else {
                    int *val_int = malloc(sizeof(int));
                    *val_int = strtol(start, NULL, 10);
                    a_num->SETF_ATOM(a_num, ID_INT, key, val_int);
                }
                obj->contain[TYP_OBJ].Method.Mobject->atom_next(obj, obj->tail, a_num);
            }
            
            free(key);

            skip_whitespace(str);
            if (**str == ',') {
                (*str)++; 
            } else if (**str != '}') {
                return -4; 
            }
        }
        return 0;
    }
    
    // ================= ARRAY PARSEN =================
    else if (**str == '[') {
        (*str)++; 
        
        ARY *ary = (ARY*)*target_core;
        if (ary == NULL) {
            init_array(&ary, "PARSED_ARY");
            *target_core = (CORE*)ary;
        }

        while (**str) {
            skip_whitespace(str);
            if (**str == ']') {
                (*str)++; 
                break;
            }

            if (**str == '{') {
                OBJ *child_obj = NULL;
                int res = json_string_to_core_recursive(str, (CORE**)&child_obj, depth + 1);
                if (res != 0) return res;
                obj_ins_obj_next((OBJ*)ary, ((OBJ*)ary)->tail, child_obj); 

            } else if (**str == '[') {
                ARY *child_ary = NULL;
                int res = json_string_to_core_recursive(str, (CORE**)&child_ary, depth + 1);
                if (res != 0) return res;
                array_ins_obj_next((OBJ*)ary, ((OBJ*)ary)->tail, child_ary);

            } else if (**str == '"') {
                char *val_str = parse_json_string(str);
                ATOM *a_str = init_atom();
                a_str->SETF_ATOM(a_str, ID_STR, "@ary_el", val_str);
                atom_ins_next((OBJ*)ary, ((OBJ*)ary)->tail, a_str);

            } else if (isdigit((unsigned char)**str) || **str == '-') {
                const char *start = *str;
                int is_float = 0;
                while (**str && (isdigit((unsigned char)**str) || **str == '.' || **str == '-')) {
                    if (**str == '.') is_float = 1;
                    (*str)++;
                }
                
                ATOM *a_num = init_atom();
                if (is_float) {
                    float *val_flt = malloc(sizeof(float));
                    *val_flt = strtof(start, NULL);
                    a_num->SETF_ATOM(a_num, ID_FLT, "@ary_el", val_flt);
                } else {
		  int *val_int = malloc(sizeof(int));
		  *val_int = strtol(start, NULL, 10);
		  a_num->SETF_ATOM(a_num, ID_INT, "@ary_el", val_int);
                }
                atom_ins_next((OBJ*)ary, ((OBJ*)ary)->tail, a_num);
            }
	    
            skip_whitespace(str);
            if (**str == ',') {
	      (*str)++;
            } else if (**str != ']') {
	      return -4;
            }
        }
        return 0;
    }

    return -5; 
}
// Die offizielle API-Wrapper-Funktion
int json_string_to_obj(const char **str, OBJ **target_obj) {
    if (!str || !*str) return -1;
    // Einstieg in die Matrix bei Tiefe 0
    return json_string_to_core_recursive(str, (CORE**)target_obj, 0);
}


int update_atom_value(OBJ *obj, const char *str_key, int new_token, void *new_ptr_data) {
    if (obj == NULL || str_key == NULL || new_ptr_data == NULL) return -1;

    ATOM *current = obj->head;
    while (current != NULL) {
        if (current->elmt->name_value && strcmp(current->elmt->name_value, str_key) == 0) {
            if (current->type_data == 1) {
                int old_token = *(int*)current->elmt->head->data;
                // Sauber den alten Pointer freigeben
                if (old_token == ID_STR || old_token == ID_INT || old_token == ID_FLT) {  
                    free(current->elmt->tail->data);
                }
                
                *(int*)current->elmt->head->data = new_token;
                current->elmt->tail->data = new_ptr_data;
                return 0; 
            }
        }
        current = current->next;
    }
    
    // Fallback: Als neues Atom anhängen
    ATOM *new_atom = init_atom();
    new_atom->SETF_ATOM(new_atom, new_token, (char*)str_key, new_ptr_data);
    obj->contain[TYP_OBJ].Method.Mobject->atom_next(obj, obj->tail, new_atom);
    return 1;
}


/************************
 * M E T H O D S  G E N *
 ***********************/

// Die global definierte V-Table für GEN (Methoden-Container)
const static GEN_VTABLE global_gen_vtable = {
    .run_gen = run_gen,
    .load_gen = load_gen,
    .objectivate_gen = objectivate_gen
};

int init_gen(OBJ **genobject) {
    // 1. POINTER-SICHERHEIT: Existiert der Doppelpointer und das Objekt?
    if (genobject == NULL || *genobject == NULL) {
      return -1;
    }

    OBJ *ptr_gen = *genobject;

    // 2. STRUKTUR-CHECK: Hat das Objekt bereits Atome geladen?
    if (ptr_gen->size == 0) {
      return -2;
    } 
 
    // 3. TYP-CHECK (BUGFIX): Die Signatur liegt in der Instanz (Iobj), nicht in der Methode![cite: 1, 3]
    if (ptr_gen->member[TYP_OBJ].Instance.Iobj == NULL || 
	ptr_gen->member[TYP_OBJ].Instance.Iobj->signature != 0xBBBBBBBB) {
      return -3;
    }

    // 4. DOWNCAST ZUM CORE: Wir werfen die OBJ_INSTANCE restlos aus dem RAM.[cite: 1, 3]
    ptr_gen = (OBJ *)ptr_gen->contain[TYP_CORE].Method.Mcore->downcast_core(ptr_gen, TYP_OBJ);
    if (ptr_gen == NULL) {
      return -4;
    }

    // 5. CORE-VERIFIZIERUNG: Ist es jetzt wirklich ein sauberer CORE?[cite: 1, 3]
    if (ptr_gen->member[TYP_CORE].Instance.Icore == NULL || 
        ptr_gen->member[TYP_CORE].Instance.Icore->signature != 0xAAAAAAAA) {
      return -666;
    }

    // 6. ALLOKATION DER GEN_INSTANCE (Daten-Container)
    if ((ptr_gen->member[TYP_GEN].Instance.Igen = malloc(sizeof(GEN_INSTANCE))) == NULL) {
      return -5;
    }

    // 7. V-TABLE UND TYP VERKNÜPFEN
    ptr_gen->typ = TYP_GEN;
    ptr_gen->member[TYP_GEN].Instance.Igen->signature = 0xDDDDDDDD; // Zwingend 0xDDDDDDDD für downcast_core_!
    ptr_gen->contain[TYP_GEN].Method.Mgen = &global_gen_vtable;

    // 8. INTERNE VARIABLEN INITIALISIEREN (mit Rollback bei Fehler)
    ptr_gen->member[TYP_GEN].Instance.Igen->ret = init_atom();
    if (ptr_gen->member[TYP_GEN].Instance.Igen->ret == NULL) {
        free(ptr_gen->member[TYP_GEN].Instance.Igen);
        ptr_gen->member[TYP_GEN].Instance.Igen = NULL;
        return -6;
    }

    if ((ptr_gen->member[TYP_GEN].Instance.Igen->gen_method = malloc(sizeof(Cmethod))) == NULL) {
        // Speicher-Rollback: Ret-Atom wieder zerstören, bevor wir abbrechen
        ptr_gen->member[TYP_GEN].Instance.Igen->ret->REM_ATOM(&ptr_gen->member[TYP_GEN].Instance.Igen->ret);
        free(ptr_gen->member[TYP_GEN].Instance.Igen);
        ptr_gen->member[TYP_GEN].Instance.Igen = NULL;
        return -7;
    }

    // 9. STANDARDWERTE SETZEN
    ptr_gen->member[TYP_GEN].Instance.Igen->gen_method->size_paras = 0;
    ptr_gen->member[TYP_GEN].Instance.Igen->gen_method->typ_return = -1;
    ptr_gen->member[TYP_GEN].Instance.Igen->gen_method->name_value = NULL;
    ptr_gen->member[TYP_GEN].Instance.Igen->gen_method->func = NULL;

    *genobject = ptr_gen;
    return 0;
}

int load_gen(GEN **genobject) {
    if (genobject == NULL || *genobject == NULL) return -1;
    GEN *ptr_gen = *genobject;

    if (ptr_gen->member[TYP_GEN].Instance.Igen == NULL || 
        ptr_gen->member[TYP_GEN].Instance.Igen->gen_method == NULL) {
        return -2;
    }

    Cmethod *method = ptr_gen->member[TYP_GEN].Instance.Igen->gen_method;

    // BUGFIX: Intelligent nach "@name" suchen statt blind das Tail zu lesen!
    ATOM *curr = ptr_gen->head;
    while (curr != NULL) {
        if (curr->type_data == 1 && curr->elmt && curr->elmt->name_value && 
            strcmp(curr->elmt->name_value, "@name") == 0) {
            method->name_value = strdup((char*)curr->elmt->tail->data);
            break;
        }
        curr = curr->next;
    }

    if (!method->name_value) return -5;

    static void *lib_handle = NULL; 
    if (lib_handle == NULL) {
        lib_handle = dlopen("./container.so", RTLD_NOW | RTLD_LOCAL);
        if (!lib_handle) {
            fprintf(stderr, "[!] CRITICAL: dlopen Error: %s\n", dlerror());
            free(method->name_value);
            method->name_value = NULL;
            return -6;
        }
    }

    dlerror(); 
    method->func = (GENFUNC*)dlsym(lib_handle, method->name_value);
    
    char *dlsym_error = dlerror();
    if (dlsym_error != NULL) {
        fprintf(stderr, "[!] PAYLOAD ERROR: %s\n", dlsym_error);
        free(method->name_value);
        method->name_value = NULL;
        return -7;
    }

    return 0;
}

int run_gen(GEN *gen, const char *name) {
    // 1. HARTER SICHERHEITSCHECK
    if (gen == NULL || name == NULL) {
        return -1;
    }
    
    if (gen->member[TYP_GEN].Instance.Igen == NULL || 
        gen->member[TYP_GEN].Instance.Igen->signature != 0xDDDDDDDD) {
        return -2;
    }

    Cmethod *genM = gen->member[TYP_GEN].Instance.Igen->gen_method;
    if (genM == NULL || genM->func == NULL) {
        return -3; // Keine Waffe geladen
    }

    // 2. IDENTITÄTS-PRÜFUNG: Soll wirklich diese Funktion ausgeführt werden?
    if (genM->name_value == NULL || strcmp(genM->name_value, name) != 0) {
        return -4; // Namens-Mismatch (Schutz vor fehlerhaftem Aufruf)
    }

    ATOM *ptr_ret = gen->member[TYP_GEN].Instance.Igen->ret;
    if (ptr_ret == NULL || ptr_ret->SETF_ATOM == NULL) {
        return -5;
    }

    char *name_value = "return-value";

    // 3. EXECUTION & ISOLATION (Der Cast und die Speichersicherheit)
    switch (genM->typ_return) {
        
        case ID_STR: { // Eigener Scope für Variablen-Deklaration
            char **(*func_str)(GEN *) = (char ** (*)(GEN *)) genM->func;
            char **raw_result = func_str(gen);
            
            if (!raw_result || !*raw_result) return -6;
            
            // strdup rettet den String auf den Heap, entkoppelt von der .so Library
            char *safe_str = strdup(*raw_result);
            return ptr_ret->SETF_ATOM(ptr_ret, ID_STR, name_value, safe_str);
        }
        
        case ID_INT: {
            int *(*func_int)(GEN *) = (int *(*)(GEN *)) genM->func;
            int *raw_result = func_int(gen);
            
            if (!raw_result) return -6;
            
            // Frischer Heap-Speicher für den Garbage Collector
            int *safe_int = malloc(sizeof(int));
            if (!safe_int) return -7;
            *safe_int = *raw_result;
            
            return ptr_ret->SETF_ATOM(ptr_ret, ID_INT, name_value, safe_int);
        }
        
        case ID_FLT: {
            float *(*func_float)(GEN *) = (float *(*)(GEN *)) genM->func;
            float *raw_result = func_float(gen);
            
            if (!raw_result) return -6;
            
            // Frischer Heap-Speicher
            float *safe_flt = malloc(sizeof(float));
            if (!safe_flt) return -7;
            *safe_flt = *raw_result;
            
            return ptr_ret->SETF_ATOM(ptr_ret, ID_FLT, name_value, safe_flt);
        }
        
        default:
            return -8; // Unbekannter Return-Typ
    }
}

int objectivate_gen(GEN **genobject) {
    if (genobject == NULL || *genobject == NULL) {
        return -1;
    }

    GEN *ptr_gen = *genobject;

    // 1. SIGNATUR-CHECK DER HÜLLE
    if (ptr_gen->member[TYP_GEN].Instance.Igen == NULL || 
        ptr_gen->member[TYP_GEN].Instance.Igen->signature != 0xDDDDDDDD) {
        return -2; // Das ist kein gültiges GEN-Objekt
    }

    // 2. DER QUARANTÄNE-CHECK (Homoikonizität)
    // Wir casten kurz auf OBJ zurück, damit phrase_gen den AST prüfen kann.
    // phrase_gen() verifiziert, ob der Baum die Struktur eines Befehls hat.
    if (phrase_gen((OBJ *)ptr_gen) != 0) {
        return -3; // Payload ist fehlerhaft oder bösartig -> Abbruch!
    }

    // 3. DYNAMISCHES LADEN DER FUNKTION
    // Wir rufen load_gen auf. Da load_gen_ (wie vorhin definiert) den Namen
    // dynamisch aus dem AST liest und dlopen() ausführt, übergeben wir für 
    // method_contain einfach NULL und für typ_return einen Dummy (0), 
    // da der echte Wrapper das selbst regelt.
    if (ptr_gen->contain[TYP_GEN].Method.Mgen->load_gen(genobject) != 0) {
        return -4; // Wrapper-Funktion konnte in der .so nicht gefunden werden
    }

    // Das System ist jetzt scharfgeschaltet.
    return 0;
}

int bind_gen_payload(GEN *payload) {
    if (!payload || !payload->member[TYP_GEN].Instance.Igen) return -1;
    if (payload->head == NULL || payload->head->type_data != 1) return -2;

    int expected_return_type = *(int*)payload->head->elmt->head->data;
    
    int param_count = 0;
    ATOM *curr = payload->head;
    
    while (curr != NULL) {
      // HACKER-SCHUTZ: Wir akzeptieren Parameter-Container jetzt als Objekt (2) ODER Array (3)
      if ((curr->type_data == 2 || curr->type_data == 3) && curr->elmt && curr->elmt->head) {
	OBJ *args_obj = (OBJ*)curr->elmt->head->data;
	if (args_obj) {
	  ATOM *arg_atom = args_obj->head;
	  while (arg_atom != NULL) {
	    if (arg_atom->type_data == 1 && arg_atom->elmt && arg_atom->elmt->head) {
	      int token = *(int*)arg_atom->elmt->head->data;
	      if (token == 1 || token == 2 || token == 3) param_count++;
	    }
	    arg_atom = arg_atom->next;
	  }
	}
      }
      curr = curr->next;
    }

    payload->member[TYP_GEN].Instance.Igen->gen_method->size_paras = param_count;
    payload->member[TYP_GEN].Instance.Igen->gen_method->typ_return = expected_return_type;

    // SICHERHEIT: calloc nullt den gesamten Speicher, es gibt keinen "Garbage" mehr!
    GEN_FP *fp_meta = calloc(1, sizeof(GEN_FP));
    fp_meta->signature = 5; 
    fp_meta->parameter_count = param_count;
    fp_meta->return_type = expected_return_type;

    ATOM *fp_atom = init_atom();
    
    // Wir prüfen jetzt den Rückgabewert! Wenn es fehlschlägt, hängen wir nichts ein.
    if (fp_atom->SETF_ATOM(fp_atom, 4, "@gen_signature", fp_meta) != 0) {
        // fp_atom wurde von set_atom bereits zerstört, wir geben nur Error zurück
        return -3; 
    }
    
    // fp_atom ist jetzt 100% gesund und bereit.
    atom_ins_next((OBJ*)payload, payload->tail, fp_atom);

    return 0;
}
