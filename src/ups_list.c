/************************************************************************
 *
 * FILE:
 *       ups_list.c
 * 
 * DESCRIPTION: 
 *       This is the 'ups list' command.
 *
 * AUTHORS:
 *       Eileen Berman
 *       David Fagan
 *       Lars Rasmussen
 *
 *       Fermilab Computing Division
 *       Batavia, Il 60510, U.S.A.
 *
 * MODIFICATIONS:
 *       14-Aug-1997, EB, first
 *       Sep 19 1997, DjF, Taking over...
 *
 ***********************************************************************/

/* standard include files */
#include <string.h>
#include <stdio.h>
#include <ctype.h>   /* toupper */

/* ups specific include files */
#include "ups.h"

/*
 * Definition of public variables.
 */

/*
 * Declaration of private functions.
 */

t_upslst_item *ups_list_core(t_upsugo_command * const a_command_line);
void list_output(const t_upslst_item * const a_mproduct_list,
		 const t_upsugo_command * const a_command_line);

void list_K(const t_upstyp_matched_instance * const instance,
            const t_upsugo_command * const a_command_line, 
            const t_upstyp_matched_product * const product);
/*
 * Definition of global variables.
 */
#define VPREFIX "UPSLIST: "

#ifndef NULL
#define NULL 0
#endif

#define FromVersion(ELEMENT) \
{ if (!upsutl_stricmp((l_ptr->data),"" #ELEMENT ""))    \
  { if(instance->version)                               \
    { if (instance->version->ELEMENT)                   \
      { printf("%s ",instance->version->ELEMENT);       \
      } else {                                          \
        printf("\"\" ");                                \
      }                                                 \
    } else {                                            \
      printf("\"\" ");                                  \
    }                                                   \
  }                                                     \
}
#define FromDatabase(ELEMENT,STRING) \
{ if (!upsutl_stricmp((l_ptr->data),STRING))            \
  { if(product->db_info)                                \
    { if (product->db_info->ELEMENT)                    \
      { printf("%s ",product->db_info->ELEMENT);        \
      } else {                                          \
        printf("\"\" ");                                \
      }                                                 \
    } else {                                            \
      printf("\"\" ");                                  \
    }                                                   \
  }                                                     \
}
#define FromConfig(ELEMENT,STRING) \
{ if (!upsutl_stricmp((l_ptr->data),STRING))            \
  { if(config_ptr)                                      \
    { if (config_ptr->ELEMENT)                          \
      { printf("%s ",config_ptr->ELEMENT);              \
      } else {                                          \
        printf("\"\" ");                                \
      }                                                 \
    } else {                                            \
      printf("\"\" ");                                  \
    }                                                   \
  }                                                     \
}
#define FromAny(ELEMENT) \
{ if (!upsutl_stricmp(l_ptr->data,"" #ELEMENT "") ||    \
      !upsutl_stricmp(l_ptr->data,"+"))                 \
  { if(instance->chain)                                 \
    { if (instance->chain->ELEMENT)                     \
      { printf("%s ",instance->chain->ELEMENT);         \
      } else {                                          \
        if(instance->version)                           \
        { if (instance->version->ELEMENT)               \
          { printf("%s ",instance->version->ELEMENT);   \
          } else {                                      \
            if (instance->table)                        \
            { if (instance->table->ELEMENT)             \
              { printf("%s ",instance->table->ELEMENT); \
              } else {                                  \
                printf("\"\" ");                        \
              }                                         \
            } else {                                    \
              printf("\"\" ");                          \
            }                                           \
          }                                             \
        } else {                                        \
          if (instance->table)                          \
          { if (instance->table->ELEMENT)               \
            { printf("%s ",instance->table->ELEMENT);   \
            } else {                                    \
              printf("\"\" ");                          \
            }                                           \
          } else {                                      \
            printf("\"\" ");                            \
          }                                             \
        }                                               \
      }                                                 \
    } else {                                            \
      if(instance->version)                             \
      { if (instance->version->ELEMENT)                 \
        { printf("%s ",instance->version->ELEMENT);     \
        } else {                                        \
          if (instance->table)                          \
          { if (instance->table->ELEMENT)               \
            { printf("%s ",instance->table->ELEMENT);   \
            } else {                                    \
              printf("\"\" ");                          \
            }                                           \
          } else {                                      \
            printf("\"\" ");                            \
          }                                             \
        }                                               \
      } else {                                          \
        if (instance->table)                            \
        { if (instance->table->ELEMENT)                 \
          { printf("%s ",instance->table->ELEMENT);     \
          } else {                                      \
            printf("\"\" ");                            \
          }                                             \
        } else {                                        \
          printf("\"\" ");                              \
        }                                               \
      }                                                 \
    }                                                   \
  }                                                     \
}
#define FromBoth(ELEMENT) \
{ if (!upsutl_stricmp(l_ptr->data,"" #ELEMENT "") ||    \
      !upsutl_stricmp(l_ptr->data,"+"))                 \
  { if(instance->chain)                                 \
    { if (instance->chain->ELEMENT)                     \
      { printf("%s:",instance->chain->ELEMENT);         \
      } else {                                          \
        printf(":");                                    \
      }                                                 \
    } else {                                            \
      printf(":");                                      \
    }                                                   \
    if (instance->xtra_chains)                          \
    { for (clist = instance->xtra_chains ;              \
           clist ; clist = clist->next)                 \
      { cinst_ptr = (t_upstyp_instance *)clist->data;   \
        printf("%s:", cinst_ptr->ELEMENT);              \
      }                                                 \
    }                                                   \
    if(instance->version)                               \
    { if (instance->version->ELEMENT)                   \
      { printf("%s ",instance->version->ELEMENT);       \
      } else {                                          \
        printf(" ");                                    \
      }                                                 \
    } else {                                            \
      printf(" ");                                      \
    }                                                   \
  }                                                     \
}
#define defaults(INSTANCE) \
{   printf("	VERSION=%s", minst_ptr->INSTANCE->version);          \
    printf("	FLAVOR=%s\n", minst_ptr->INSTANCE->flavor);          \
    if (minst_ptr->INSTANCE->qualifiers &&                           \
       strlen(minst_ptr->INSTANCE->qualifiers))                      \
    { printf("	QUALIFIERS=%s", minst_ptr->INSTANCE->qualifiers);    \
    } else {                                                         \
      printf("	QUALIFIERS=\"\"");                                   \
    }                                                                \
    if (minst_ptr->xtra_chains)                                      \
    { printf("	CHAINS=%s", minst_ptr->INSTANCE->chain);             \
      for (clist = minst_ptr->xtra_chains ;                          \
           clist ; clist = clist->next)                              \
      { cinst_ptr = (t_upstyp_instance *)clist->data;                \
        printf(",%s", cinst_ptr->chain );                            \
      }                                                              \
    } else {                                                         \
      printf("	CHAIN=%s", minst_ptr->INSTANCE->chain);              \
    } printf("\n");                                                  \
}
int product_cmp ( const void * const d1, const void * const d2 )
{
  t_upstyp_product *a1 = (t_upstyp_product *)d1;
  t_upstyp_product *a2 = (t_upstyp_product *)d2;

  return upsutl_stricmp( a1->product, a2->product );
}
/*
 * Definition of public functions.
 */

/*-----------------------------------------------------------------------
 * ups_list
 *
 * This is the main line for the 'ups list' command.
 *
 * Input : argc, argv
 * Output: 
 * Return: 
 */
void ups_list( t_upsugo_command * const a_command_line )
{
  t_upslst_item *mproduct_list = NULL;
  t_upslst_item *tmp_mprod_list = NULL;
  t_upstyp_matched_product *mproduct = NULL;

  /* Get all the requested instances */
  mproduct_list = ups_list_core(a_command_line);
  /*  upsugo_prtlst(mproduct_list,"the products");*/
  mproduct_list = upslst_first(mproduct_list);  /* point to the start */
  upsver_mes(2,"Starting sort of product list\n");
  mproduct_list = upslst_sort0( mproduct_list , product_cmp );
  upsver_mes(2,"Ending sort of product list\n");
  mproduct_list = upslst_first(mproduct_list);  /* point to the start */

  /* Output the requested information from the instances */
  /*  upsugo_dump(a_command_line);*/
  list_output(mproduct_list, a_command_line);

  /* free the matched products */
  for (tmp_mprod_list = mproduct_list ; tmp_mprod_list ; 
       tmp_mprod_list = tmp_mprod_list->next) {
    mproduct = (t_upstyp_matched_product *)tmp_mprod_list->data;
    ups_free_matched_product(mproduct);      /* free the data */
  }
  /* now free the list */
  tmp_mprod_list = upslst_free(tmp_mprod_list, ' ');

}


/*-----------------------------------------------------------------------
 * ups_list_core
 *
 * Take the command line parameters and read in all of the desired instances
 *
 * Input : <input>
 * Output: <output>
 * Return: <return>
 */
t_upslst_item *ups_list_core(t_upsugo_command * const a_command_line)
{
  t_upslst_item *mproduct_list = NULL;
  int need_unique = 0;
  char * addr;

  /* if no chains were entered, ask for them all */
  if (! a_command_line->ugo_chain) { 
/* The following won't work, a list item will be eventually managed
   with our memory routines and since this in not allocated with
   the ups_malloc the extra stuff won't be there... DjF
    a_command_line->ugo_chain = upslst_new((void *)ANY_MATCH);
*/
    addr=upsutl_str_create(ANY_MATCH,' ');
    a_command_line->ugo_chain = upslst_new(addr);
  }

  /* Get all the instances that the user requested */
  mproduct_list = upsmat_instance(a_command_line, need_unique);

  return(mproduct_list);
}

/*-----------------------------------------------------------------------
 * list_output
 *
 * Output all of the desired information from the matched instances TBD
 *
 * Input : <input>
 * Output: <output>
 * Return: <return>
 */
void list_output(const t_upslst_item * const a_mproduct_list,
		 const t_upsugo_command * const a_command_line)
{
  t_upstyp_matched_product *mproduct = NULL;
  t_upslst_item *tmp_mprod_list = NULL;
  t_upslst_item *tmp_minst_list = NULL, *clist = NULL;
  t_upstyp_instance *cinst_ptr = NULL;
  t_upstyp_matched_instance *minst_ptr = NULL;
  t_upstyp_config  *config_ptr = 0;
  char *test="This is a test ${UPS_PROD_DIR} blaa ${UPS_PROD_NAME}";

  for (tmp_mprod_list = (t_upslst_item *)a_mproduct_list ; tmp_mprod_list ;
       tmp_mprod_list = tmp_mprod_list->next) 
  { mproduct = (t_upstyp_matched_product *)tmp_mprod_list->data;
    for (tmp_minst_list = mproduct->minst_list ; tmp_minst_list ;
	 tmp_minst_list = tmp_minst_list->next) 
    { minst_ptr = (t_upstyp_matched_instance *)(tmp_minst_list->data);
/* A as in a single product loop */
      if (!a_command_line->ugo_K)
      { printf("DATABASE=");
        if(mproduct->db_info)
        { if (mproduct->db_info->name)
          { printf("%s ",mproduct->db_info->name);
          } else {
            printf("\"\" ");
          }
        } else {
          printf("\"\" ");
        } printf("\n");
        printf("	PRODUCT=%s",mproduct->product);
        if (minst_ptr->chain) 
        { defaults(chain)
        } else { 
          if (minst_ptr->version )
          {  defaults(version) 
          } else { 
            if (minst_ptr->table )
            {  defaults(table)
            } /* else what in the hell are we doing here ?? */
          }
        }
        if (a_command_line->ugo_l && minst_ptr->version )
        { printf("	HOME=");
          if (mproduct->db_info) 
          { config_ptr = mproduct->db_info->config;
            if (config_ptr) 
            { if (config_ptr->prod_dir_prefix) 
              { printf("%s",config_ptr->prod_dir_prefix); }
            }
          }
          printf("%s\n", minst_ptr->version->prod_dir);
          if (minst_ptr->version->ups_dir)
          { printf("	UPS=%s\n", minst_ptr->version->ups_dir);
          } else {
            printf("	UPS=\"\"\n");
          }
          if (minst_ptr->version->table_dir)
          { printf("	TABLE_DIR=%s\n", minst_ptr->version->table_dir);
          } else {
            printf("	TABLE_DIR=\"\"\n");
          }
          if (minst_ptr->version->table_file)
          { printf("	TABLE_FILE=%s\n", minst_ptr->version->table_file);
          } else {
            printf("	TABLE_FILE=\"\"\n");
          }
          if (minst_ptr->version->description)
          { printf("	DESCRIPTION=%s\n", minst_ptr->version->description);
          } else {
            printf("	DESCRIPTION=\"\"\n");
          }
        }
        printf("\n\n");
      } else { 
          upstra_str(mproduct,test);
          list_K(minst_ptr,a_command_line,mproduct);
      }
    }
/* end product loop */
  }
}

void list_K(const t_upstyp_matched_instance * const instance, 
            const t_upsugo_command * const command, 
            const t_upstyp_matched_product * const product)
{
  t_upslst_item *l_ptr = 0;
  t_upstyp_instance *cinst_ptr = 0;
  t_upslst_item *clist = 0;
  t_upstyp_config  *config_ptr = 0;
  int count=0;
  if (product->db_info) 
  { config_ptr = product->db_info->config;
  }
  for ( l_ptr = upslst_first( command->ugo_key ); 
        l_ptr; l_ptr = l_ptr->next, count++ )
  { FromVersion(table_file)
    FromVersion(ups_dir)
    FromVersion(prod_dir)
    FromVersion(archive_file)
    FromVersion(description)
/*    FromVersion(db_dir) */
/* DO NOT CHANGE ORDER here it's the default !!! */
    FromAny(product) 
    FromAny(version) 
    FromAny(flavor) 
    FromAny(qualifiers)
    FromBoth(chain)
    if(upsutl_stricmp(l_ptr->data,"+"))
    { FromBoth(declarer)
      FromBoth(declared)
    }
/* to HERE */
    FromDatabase(name,"DATABASE")
    FromConfig(ups_db_version,"DB_VERSION")
    FromConfig(man_path,"MAN_PATH")
    FromConfig(html_path,"HTML_PATH")
    FromConfig(info_path,"INFO_PATH")
  }
  printf("\n");
}
