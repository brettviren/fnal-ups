/*
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
#include <pwd.h>

/* ups specific include files */

#include "ups.h"
 
/*
 * Definition of public variables.
 */

/*
 * Private constants
 */

#define UPSPRE "${UPS_"
#define TILDE "~"

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
  { "${UPS_THIS_DB", upsget_this_db},
  { "${UPS_SETUP", upsget_envstr },
  { "${UPS_ORIGIN", upsget_origin },
  { "${PRODUCTS", upsget_database },
  {  0, 0 }
} ;


/*
 * Definition of public functions.
 */
void upsget_remall(const FILE * const stream, 
                    const t_upsugo_command * const command_line )
{
  if (command_line->ugo_shell == e_INVALID_SHELL)
  { upserr_add(UPS_NOSHELL);
  } else {
    if (command_line->ugo_shell == e_BOURNE )
    { fprintf((FILE *)stream,"unset UPS_PROD_NAME\n");
      fprintf((FILE *)stream,"unset UPS_PROD_VERSION\n");
      fprintf((FILE *)stream,"unset UPS_PROD_DIR\n");
      fprintf((FILE *)stream,"unset UPS_VERBOSE\n");
      fprintf((FILE *)stream,"unset UPS_EXTENDED\n");
      fprintf((FILE *)stream,"unset UPS_THIS_DB\n");
      fprintf((FILE *)stream,"unset UPS_OS_FLAVOR\n");
      fprintf((FILE *)stream,"unset UPS_PROD_FLAVOR\n");
      fprintf((FILE *)stream,"unset UPS_PROD_QUALIFIERS\n");
      fprintf((FILE *)stream,"unset UPS_SHELL\n");
      fprintf((FILE *)stream,"unset UPS_OPTIONS\n");
    } else { 
      fprintf((FILE *)stream,"unsetenv UPS_PROD_NAME\n");
      fprintf((FILE *)stream,"unsetenv UPS_PROD_VERSION\n");
      fprintf((FILE *)stream,"unsetenv UPS_PROD_DIR\n");
      fprintf((FILE *)stream,"unsetenv UPS_VERBOSE\n");
      fprintf((FILE *)stream,"unsetenv UPS_EXTENDED\n");
      fprintf((FILE *)stream,"unsetenv UPS_THIS_DB\n");
      fprintf((FILE *)stream,"unsetenv UPS_OS_FLAVOR\n");
      fprintf((FILE *)stream,"unsetenv UPS_PROD_FLAVOR\n");
      fprintf((FILE *)stream,"unsetenv UPS_PROD_QUALIFIERS\n");
      fprintf((FILE *)stream,"unsetenv UPS_SHELL\n");
      fprintf((FILE *)stream,"unsetenv UPS_OPTIONS\n");
    }
  } 
}

void upsget_envout(const FILE * const stream, 
                    const t_upstyp_db * const db,
                    const t_upstyp_matched_instance * const instance,
                    const t_upsugo_command * const command_line )
{ char *name;
  get_element(name,product);
  name = upsutl_upcase( name );
  if (command_line->ugo_shell == e_BOURNE )
  { fprintf((FILE *)stream,"SETUP_%s=\"%s\";export SETUP_%s\n#\n",
    name,upsget_envstr(db,instance,command_line),name);
  } else {
    fprintf((FILE *)stream,"setenv SETUP_%s \"%s\"\n#\n",
    name,upsget_envstr(db,instance,command_line));
  }
}
 
void upsget_allout(const FILE * const stream, 
                    const t_upstyp_db * const db,
                    const t_upstyp_matched_instance * const instance,
                    const t_upsugo_command * const command_line )
{ char *addr;
  char *name;
  if (command_line->ugo_shell == e_INVALID_SHELL)
  { upserr_add(UPS_NOSHELL);
  } else {
    get_element(name,product);
    if (!name)
    { name = " ";      /* no name in file, set to space */      
    }
    if (command_line->ugo_shell == e_BOURNE )
    { fprintf((FILE *)stream,"UPS_PROD_NAME=%s;export UPS_PROD_NAME\n",name);
      fprintf((FILE *)stream,"UPS_PROD_VERSION=%s;export UPS_PROD_VERSION\n",
               upsget_version(db,instance,command_line));
      fprintf((FILE *)stream,"UPS_PROD_DIR=%s;export UPS_PROD_DIR\n",
               upsget_prod_dir(db,instance,command_line));
      addr=upsget_verbose(db,instance,command_line);
      if (strlen(addr)) 
      { fprintf((FILE *)stream,"UPS_VERBOSE=%s;export UPS_VERBOSE\n",addr);
      } else { 
        fprintf((FILE *)stream,"unset UPS_VERBOSE\n");
      }
      addr=upsget_extended(db,instance,command_line);
      if (strlen(addr))
      { fprintf((FILE *)stream,"UPS_EXTENDED=%s;export UPS_EXTENDED\n",addr); 
      } else { 
        fprintf((FILE *)stream,"unset UPS_EXTENDED\n");
      } 
      fprintf((FILE *)stream,"UPS_THIS_DB=%s;export UPS_THIS_DB\n",
               upsget_this_db(db,instance,command_line));
      fprintf((FILE *)stream,"UPS_OS_FLAVOR=%s;export UPS_OS_FLAVOR\n",
               upsget_OS_flavor(db,instance,command_line));
      fprintf((FILE *)stream,"UPS_PROD_FLAVOR=%s;export UPS_PROD_FLAVOR\n",
               upsget_flavor(db,instance,command_line));
      fprintf((FILE *)stream,"UPS_PROD_QUALIFIERS=%s;export UPS_PROD_QUALIFIERS\n",
               upsget_qualifiers(db,instance,command_line));
      fprintf((FILE *)stream,"UPS_SHELL=%s;export UPS_SHELL\n",
               upsget_shell(db,instance,command_line));
      addr=upsget_options(db,instance,command_line);
      if (addr) 
      { fprintf((FILE *)stream,"UPS_OPTIONS=\"%s\";export UPS_OPTIONS\n",addr); } 
    } else { 
      fprintf((FILE *)stream,"setenv UPS_PROD_NAME %s\n",name);
      fprintf((FILE *)stream,"setenv UPS_PROD_VERSION %s\n",
               upsget_version(db,instance,command_line));
      fprintf((FILE *)stream,"setenv UPS_PROD_DIR %s\n",
               upsget_prod_dir(db,instance,command_line));
      addr=upsget_verbose(db,instance,command_line);
      if (strlen(addr))
      { fprintf((FILE *)stream,"setenv UPS_VERBOSE %s\n",addr);
      } else { 
        fprintf((FILE *)stream,"unsetenv UPS_VERBOSE\n");
      }
      addr=upsget_extended(db,instance,command_line);
      if (strlen(addr))
      { fprintf((FILE *)stream,"setenv UPS_EXTENDED %s\n",addr); 
      } else { 
        fprintf((FILE *)stream,"unsetenv UPS_EXTENDED\n");
      }
      fprintf((FILE *)stream,"setenv UPS_THIS_DB %s\n",
               upsget_this_db(db,instance,command_line));
      fprintf((FILE *)stream,"setenv UPS_OS_FLAVOR %s\n",
               upsget_OS_flavor(db,instance,command_line));
      fprintf((FILE *)stream,"setenv UPS_PROD_FLAVOR %s\n",
               upsget_flavor(db,instance,command_line));
      fprintf((FILE *)stream,"setenv UPS_PROD_QUALIFIERS %s\n",
               upsget_qualifiers(db,instance,command_line));
      fprintf((FILE *)stream,"setenv UPS_SHELL %s\n",
               upsget_shell(db,instance,command_line));
      addr=upsget_options(db,instance,command_line);
      if (addr) 
      { fprintf((FILE *)stream,"setenv UPS_OPTIONS \"%s\"\n",addr); }
    }
  } 
}

char *upsget_translation( const t_upstyp_matched_product * const product,
                  const t_upsugo_command * const command_line,
                  char * const oldstr )
{
  t_upslst_item *inst_list;
  t_upstyp_matched_instance *instance;
  t_upstyp_db *db_info_ptr;
  static char newstr[4096];
  static char holdstr[4096];
  char * loc;
  char * upto;
  char * eaddr;             /* end addr of } */
  int count=0;
  int found=0;
  int idx=0;
  int any=0;
  char * value;
  char * work;
  newstr[0] = '\0';
  inst_list = product->minst_list;
  db_info_ptr = product->db_info;
  instance = (t_upstyp_matched_instance *)(inst_list->data);
  /* work = (char *) malloc((size_t)(strlen(oldstr) +1)); */
  work=holdstr;
  strcpy(work,oldstr);
  upto = work;
  while ((loc = strstr(upto,UPSPRE))!= 0 ) 
  { count = ( loc - upto );
    strncat(newstr,upto,(unsigned int )count);
    upto += count;
    eaddr =strchr(upto,'}');
    *eaddr = '\0';
    found=0;
    for ( idx=0; g_var_subs[idx].string!=0; idx++) 
    { if (!strcmp(g_var_subs[idx].string,upto)) 
      { if (g_var_subs[idx].func)
        {  value=g_var_subs[idx].func(db_info_ptr,instance,command_line);
           if(value) { strcat(newstr,value); }
        }
        found=1;
        any++;
      }
    }
    if (!found) { strcat(newstr,upto); strcat(newstr,"}"); } 
    *eaddr = '}';
    upto =strchr(upto,'}') + 1;
  }
  if (any)
  { strcat(newstr,upto);
  } else {
    strcpy(newstr,work);
    /* free(work); now static */
  }
  /* work = (char *) malloc((size_t)(strlen(newstr) +1)); */
  work=holdstr;
  strcpy(work,newstr);
  upto = work;
  newstr[0] = '\0';
  any=0;
  while ((loc = strstr(upto,"${PRODUCTS}"))!=0) 
  { count = ( loc - upto );
    strncat(newstr,upto,(unsigned int )count);
    upto += count;
    eaddr =strchr(upto,'}');
    *eaddr = '\0';
    found=0;
    if (!strcmp("${PRODUCTS",upto)) 
    { value=upsget_database(db_info_ptr,instance,command_line);
      if(value) { strcat(newstr,value); }
      found=1;
      any++;
    } else { 
      strcat(newstr,upto); 
      strcat(newstr,"}");
    } 
    *eaddr = '}';
    upto = strchr(upto,'}') + 1;
  }
  if (any)
  { strcat(newstr,upto);
  } else {
    strcpy(newstr,work);
    /* free(work); no static */
  }
  /* work = (char *) malloc((size_t)(strlen(newstr) +1)); */
  work=holdstr;
  strcpy(work,newstr);
  upto = work;
  newstr[0] = '\0';
  any=0;
  while ((loc = strstr(upto,TILDE))!=0) 
  { count = ( loc - upto );
    strncat(newstr,upto,(unsigned int )count);
    upto += count;
    eaddr =strchr(upto,'/');
    *eaddr = '\0';
    found=0;
    if (strchr(upto,'~')) 
    { value=upsget_tilde_dir(upto);
      if(value) { strcat(newstr,value); }
      found=1;
      any++;
    } else { 
      strcat(newstr,upto); 
      strcat(newstr,"/");
    } 
    *eaddr = '/';
    upto = strchr(upto,'/') + 1;
  }
  if (any)
  { strcat(newstr,upto);
  } else {
    strcpy(newstr,work);
  }
  return newstr;
}

char *upsget_envstr(const t_upstyp_db * const db_info_ptr,
                    const t_upstyp_matched_instance * const instance,
                    const t_upsugo_command * const command_line )
{
  static char newstr[4096];
  static char *string = 0;
  get_element(string,product);
  strcpy(newstr,string);
  strcat(newstr," ");
  get_element(string,version);
  strcat(newstr,string);
  strcat(newstr," -f ");
  get_element(string,flavor);
  strcat(newstr,string);
  if ( db_info_ptr )
  { strcat(newstr," -z ");
    strcat(newstr,db_info_ptr->name);
  }
  get_element(string,qualifiers);
  if ( *string != 0 )
  { strcat(newstr," -q ");
    strcat(newstr,string);
  }
  if ( command_line->ugo_v)
  { strcat(newstr," -v "); }
  if ( command_line->ugo_r)
  { strcat(newstr," -r ");
    strcat(newstr,command_line->ugo_productdir);
  }
  if ( command_line->ugo_m)
  { strcat(newstr," -m ");
    strcat(newstr,command_line->ugo_tablefile);
  }
  if ( command_line->ugo_M)
  { strcat(newstr," -M ");
    strcat(newstr,command_line->ugo_tablefiledir);
  }
  if ( command_line->ugo_O)
  { strcat(newstr," -O \"");
    strcat(newstr,command_line->ugo_options);
    strcat(newstr,"\"");
  }
  if ( command_line->ugo_e)
  { strcat(newstr," -e "); }
  return newstr;
}

char *upsget_prod_dir(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char *string;
  if (command_line->ugo_r)
  { string=command_line->ugo_productdir;
  } else { 
    get_element(string,prod_dir);
  }
  return string;
}
char *upsget_origin(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char *string;
  get_element(string,origin);
  return string;
}
char *upsget_product(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char *string;
  get_element(string,product);
  return string;
}
char *upsget_version(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char *string;
  get_element(string,version);
  return string;
}
char *upsget_flavor(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char *string;
  get_element(string,flavor);
  return string;
}
char *upsget_qualifiers(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char *string;
  get_element(string,qualifiers);
  return string;
}
char *upsget_shell(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char SH[]="sh";
  static char CSH[]="csh";
  if (command_line->ugo_shell != e_INVALID_SHELL )
  { if (!command_line->ugo_shell) 
    { return (SH);
    } else {
      return (CSH);
    }
  } else {
    return (0);
  }
}
char *upsget_verbose(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char string[2];
  static char NOT[]="";
  if (command_line->ugo_v)
  { sprintf(string,"%.1d",command_line->ugo_v);
    return (string);
  } else {
    return(NOT);
  } 
}
char *upsget_extended(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char EXTENDED[]="1";
  static char NOT[]="";
  if (command_line->ugo_e)
  { return(EXTENDED);
  } else {
  } return(NOT);
}
char *upsget_options(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ static char NOT[]="";
  static char *string;
  if (command_line->ugo_O)
  { string=command_line->ugo_options;
  } else {
    string=NOT;
  }
  return(string);
}
char *upsget_database(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ t_upslst_item *db_list;
  t_upstyp_db *db;
  static char buffer[2028];
  static char *string=buffer;
  int count=0;
  buffer[0]='\0';
  for ( db_list=command_line->ugo_db; db_list; 
        db_list = db_list->next, count++ )
  { db=db_list->data;
    if(count) strcpy(string,":");
    strcpy(string,db->name);
  } return(string);     
} 
char *upsget_tilde_dir(char * addr)
{
  struct passwd *pdp;
  static char buffer[FILENAME_MAX+1];
  char *name;
  buffer[0]='\0';
  if(strlen(addr)==1)
  { pdp = getpwuid(getuid());
    strcpy(buffer,pdp->pw_dir);
  } else { 
    name=addr+1;
    pdp = getpwnam(name);
    if (pdp)
    { strcpy(buffer,pdp->pw_dir);
    }
  }
  return(buffer);
}

char *upsget_this_db(const t_upstyp_db * const db_info_ptr,
                      const t_upstyp_matched_instance * const instance,
                      const t_upsugo_command * const command_line )
{ t_upslst_item *db_list;
  static char NOT[]="";
  static char *string;
/*  if (command_line->ugo_z) 
  { db_list=upslst_first(command_line->ugo_db);
    string=db_list->data-;
  } else {
*/
/* If there is no db_info_ptr they must have specified a -M table_file
   on the command line - or match is broken..;)
*/
    if ( db_info_ptr )
    { string=db_info_ptr->name;
    } else {
      string=NOT;
    }
  return(string);
}
char *upsget_OS_flavor(const t_upstyp_db * const db_info_ptr,
                       const t_upstyp_matched_instance * const instance,
                       const t_upsugo_command * const command_line )
{  static char uname_flavor[80];
   struct utsname baseuname;              /* returned from uname */
   static char *flavor;                /* flavor ptr */
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

/*  upsget_chain_file
 *
 * Read the specified chain file.
 *
 * Input : a database
 *         a product name
 *         a chain name
 * Output: pointer to the buffer with the file path in it
 * Return: a product structure read from the file
 */
t_upstyp_product *upsget_chain_file(const char * const a_db,
                                    const char * const a_prod,
                                    const char * const a_chain,
                                    char ** const a_buffer)
{
  int file_chars = 0;
  static char buffer[FILENAME_MAX+1];
  t_upstyp_product *read_product = NULL;

  file_chars = (int )(strlen(a_chain) + strlen(a_prod) + strlen(a_db) + 
               sizeof(CHAIN_SUFFIX) + 4);
  if (file_chars <= FILENAME_MAX) {
    sprintf(buffer, "%s/%s/%s%s", a_db, a_prod, a_chain, CHAIN_SUFFIX);
    read_product = upsfil_read_file(&buffer[0]);
    *a_buffer = buffer;
  } else {
    upserr_vplace();
    upserr_add(UPS_FILENAME_TOO_LONG, UPS_FATAL, file_chars);
    *a_buffer = 0;
  }

  return(read_product);
}
/*  upsget_version_file
 *
 * Read the specified version file.
 *
 * Input : a database
 *         a product name
 *         a version 
 * Output: pointer to the buffer with the file path in it
 * Return: a product structure read from the file
 */
t_upstyp_product *upsget_version_file(const char * const a_db,
                                      const char * const a_prod,
                                      const char * const a_version,
                                      char ** const a_buffer)
{
  int file_chars = 0;
  char buffer[FILENAME_MAX+1];
  t_upstyp_product *read_product = NULL;

  file_chars = (int )(strlen(a_version) + strlen(a_prod) + strlen(a_db) + 
               sizeof(VERSION_SUFFIX) + 4);
  if (file_chars <= FILENAME_MAX) {
    sprintf(buffer, "%s/%s/%s%s", a_db, a_prod, a_version, VERSION_SUFFIX);
    read_product = upsfil_read_file(&buffer[0]);
    *a_buffer = buffer;
  } else {
    upserr_vplace();
    upserr_add(UPS_FILENAME_TOO_LONG, UPS_FATAL, file_chars);
    *a_buffer = 0;
  }

  return(read_product);
}
