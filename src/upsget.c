/************************************************************************
 *
 * FILE:
 *       upsget.c
 * 
 * DESCRIPTION: 
 *       This file contains routines to translate ups support variables
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
 *       12-Nov-1997, DjF, first
 *
 ***********************************************************************/

/* standard include files */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/utsname.h>

/* ups specific include files */

#include "ups.h"
 
/*
 * Definition of public variables.
 */

/*
 * Private constants
 */

#define UPSPRE "${UPS_"

#define get_element(STRING,ELEMENT)              \
{ STRING=0;                                      \
  if(instance->chain)                            \
  { if (instance->chain->ELEMENT)                \
    { STRING=instance->chain->ELEMENT;           \
    } else {                                     \
      if(instance->version)                      \
      { if (instance->version->ELEMENT)          \
        { STRING=instance->version->ELEMENT;     \
        } else {                                 \
          if (instance->table)                   \
          { if (instance->table->ELEMENT)        \
            { STRING=instance->table->ELEMENT; } \
          }                                      \
        }                                        \
      } else {                                   \
        if (instance->table)                     \
        { if (instance->table->ELEMENT)          \
          { STRING=instance->table->ELEMENT; }   \
        }                                        \
      }                                          \
    }                                            \
  } else {                                       \
    if(instance->version)                        \
    { if (instance->version->ELEMENT)            \
      { STRING=instance->version->ELEMENT;       \
      } else {                                   \
        if (instance->table)                     \
        { if (instance->table->ELEMENT)          \
          { STRING=instance->table->ELEMENT; }   \
        }                                        \
      }                                          \
    } else {                                     \
      if (instance->table)                       \
      { if (instance->table->ELEMENT)            \
        { STRING=instance->table->ELEMENT; }     \
      }                                          \
    }                                            \
  }                                              \
}


typedef char * (*var_func)(const t_upstyp_db * const db_info_ptr,
                           const t_upstyp_matched_instance * const instance,
                           const t_upsugo_command * const command_line );

typedef struct s_var_sub {
  char *string;
  var_func func;
} t_var_sub;

static t_var_sub g_var_subs[] = {
  { "${UPS_PROD_NAME", upsget_product },
  { "${UPS_PROD_VERSION", upsget_version },
  { "${UPS_PROD_FLAVOR", upsget_flavor },
  { "${UPS_OS_FLAVOR", upsget_OS_flavor },
  { "${UPS_PROD_QUALIFIERS", upsget_qualifiers },
  { "${UPS_PROD_DIR", upsget_prod_dir },
  { "${UPS_SHELL", upsget_shell },
  { "${UPS_OPTIONS", upsget_options },
  { "${UPS_VERBOSE", upsget_verbose },
  { "${UPS_EXTENDED", upsget_extended },
  { "${UPS_FLAGS", 0 },
  { "${UPS_FLAGSDEPEND", 0 },
  { "${UPS_THIS_DB", upsget_database },
  {  0, 0 }
} ;


/*
 * Definition of public functions.
 */
char *upsget_allout(const FILE * const stream, 
                    const t_upstyp_db * const db,
                    const t_upstyp_matched_instance * const instance,
                    const t_upsugo_command * const command_line )
{ char work[80];
  char *addr;
  char *name;
  if (command_line->ugo_shell == e_INVALID_SHELL)
  { upserr_add(UPS_NOSHELL);
  } else {
    get_element(name,product);
    strcpy(work,name);
    strcat(work,"_DIR");
    addr=work;
    while (*addr) {*addr=toupper((int)*addr);++addr;}
    if (command_line->ugo_shell == e_BOURNE )
    { fprintf((FILE *)stream,"UPS_PROD_NAME=%s;export UPS_PROD_NAME\n",name);
      fprintf((FILE *)stream,"UPS_PROD_VERSION=%s;export UPS_PROD_VERSION\n",
               upsget_version(db,instance,command_line));
      fprintf((FILE *)stream,"UPS_PROD_DIR=%s;export UPS_PROD_DIR\n",
               upsget_prod_dir(db,instance,command_line));
      fprintf((FILE *)stream,"%s=%s;export %s\n",work,
               upsget_prod_dir(db,instance,command_line));
      addr=upsget_verbose(db,instance,command_line);
      if (addr) 
      { fprintf((FILE *)stream,"UPS_VERBOSE=%s;export UPS_VERBOSE\n",addr); }
      addr=upsget_extended(db,instance,command_line);
      if (addr) 
      { fprintf((FILE *)stream,"UPS_EXTENDED=%s;export UPS_EXTENDED\n",addr); }
      fprintf((FILE *)stream,"UPS_THIS_DB=%s;export UPS_THIS_DB\n",
               upsget_database(db,instance,command_line));
    } else { 
      fprintf((FILE *)stream,"setenv UPS_PROD_NAME=%s\n",name);
      fprintf((FILE *)stream,"setenv UPS_PROD_VERSION=%s\n",
               upsget_version(db,instance,command_line));
      fprintf((FILE *)stream,"setenv UPS_PROD_DIR=%s\n",
               upsget_prod_dir(db,instance,command_line));
      addr=upsget_verbose(db,instance,command_line);
      if (addr) 
      { fprintf((FILE *)stream,"setenv UPS_VERBOSE=%s\n",addr); }
      addr=upsget_extended(db,instance,command_line);
      if (addr) 
      { fprintf((FILE *)stream,"setenv UPS_EXTENDED=%s\n",addr); }
      fprintf((FILE *)stream,"setenv UPS_THIS_DB=%s\n",
               upsget_database(db,instance,command_line));
    }
  } return 0;
}

char *upsget_translation( const t_upstyp_matched_product * const product,
                  const t_upsugo_command * const command_line,
                  char * const oldstr )
{
  t_upslst_item *inst_list;
  t_upstyp_matched_instance *instance;
  t_upstyp_db *db_info_ptr;
  static char newstr[4096];
  char * loc;
  char * upto;
  char * eaddr;             /* end addr of } */
  int count=0;
  int found=0;
  int idx=0;
  int any=0;
  char * value;
  newstr[0] = '\0';
  upto = oldstr;
  inst_list = product->minst_list;
  db_info_ptr = product->db_info;
  instance = (t_upstyp_matched_instance *)(inst_list->data);
  while ((loc = strstr(upto,UPSPRE))!= 0 ) 
  { count = ( loc - upto );
    strncat(newstr,upto,count);
    upto += count;
    eaddr =strchr(upto,'}');
    *eaddr = '\0';
    found=0;
    for ( idx=0; g_var_subs[idx].string!=0; idx++) 
    { if (!strcmp(g_var_subs[idx].string,upto)) 
      { if (g_var_subs[idx].func)
        {  value=g_var_subs[idx].func(db_info_ptr,instance,command_line);
           if(value) strcat(newstr,value);
        }
        found=1;
        any++;
      }
    }
    if (!found) { strcat(newstr,upto); strcat(newstr,"}");} 
    *eaddr = '}';
    upto =strchr(upto,'}') + 1;
  }
  if (any)
  { strcat(newstr,upto);
/* test 
    upsget_allout(stdout,db_info_ptr,instance,command_line);
*/
    return newstr;
  } else {
    return 0;
  }
}

char *upsget_prod_dir(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ char *string;
  get_element(string,prod_dir);
  return string;
}
char *upsget_product(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ char *string;
  get_element(string,product);
  return string;
}
char *upsget_version(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ char *string;
  get_element(string,version);
  return string;
}
char *upsget_flavor(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ char *string;
  get_element(string,flavor);
  return string;
}
char *upsget_qualifiers(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ char *string;
  get_element(string,qualifiers);
  return string;
}
char *upsget_shell(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ 
  if (command_line->ugo_shell != e_INVALID_SHELL )
  { if (!command_line->ugo_shell) 
    { return ("sh");
    } else {
      return ("csh");
    }
  }
}
char *upsget_verbose(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ char string[2];
  if (command_line->ugo_v)
  { sprintf(string,"%.1d",command_line->ugo_v);
    return string;
  } else {
    return 0;
  } 
}
char *upsget_extended(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ if (command_line->ugo_e)
  { return("extended");
  } else {
  } return 0;
}
char *upsget_options(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ if (command_line->ugo_O)
  { return command_line->ugo_options;
  } else {
    return 0;
  }
}
char *upsget_database(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ if ( db_info_ptr )
  { return db_info_ptr->name;
  } else {
    return 0;
  }
}
char *upsget_OS_flavor(const t_upstyp_db * const db_info_ptr,
                       const t_upstyp_matched_instance * const instance,
                       const t_upsugo_command * const command_line )
{  char                 uname_flavor[80];
   struct utsname       baseuname;              /* returned from uname */
   char                 *flavor;                /* flavor ptr */
   flavor = uname_flavor;                       /* init pointer */
   if (uname(&baseuname) == -1) return(0);     /* do uname */
   (void) strcpy (flavor,baseuname.sysname);    /* get sysname */
   (void) strcat (flavor,"+");                  /* add plus */
   if (strncmp(baseuname.sysname,"AIX",3) == 0) /* because AIX is different */
   { (void) strcat(flavor,baseuname.version);  /* add in version */
     (void) strcat(flavor,".");
   }
   (void) strcat(flavor,baseuname.release);     /* add in release */
   return flavor;
}
