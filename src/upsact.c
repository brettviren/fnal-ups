/************************************************************************
 *
 * FILE:
 *       upsact.c
 * 
 * DESCRIPTION: 
 *       This file contains routines to manage ups action lines.
 *       *** It need to be reentrant ***
 *
 *       Steps in order to add a new action -
 *           1. add a f_<action> prototype at the top of the file
 *           2. add an e_<action> to the enum below
 *           3. add a line to the g_cmd_maps structure
 *           4. add the code for the f_<action> function
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
 *       28-Aug-1997, EB, first
 *
 ***********************************************************************/

/* standard include files */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ups specific include files */
#include "upsact.h"
#include "upserr.h"
#include "upslst.h"
#include "upsutl.h"
#include "upsmem.h"
#include "upskey.h"
#include "upsugo.h"
#include "upsget.h"
#include "ups_main.h"

/*
 * Definition of public variables.
 */
int g_LOCAL_VARS_DEF = 0;

/*
 * Private constants
 */

#define DQUOTE '"'
#define COMMA ','
#define OPEN_PAREN '('
#define CLOSE_PAREN ')'
#define SLASH "/"
#define WSPACE " \t\n\r\f\""

/*
 * Private types
 */

/* this one is the type of a action command handler */
typedef void (*tpf_cmd)( const t_upstyp_matched_instance * const a_inst,
			 const t_upstyp_db * const a_db_info,
			 const t_upsugo_command * const a_command_line,
                         const FILE * const a_stream,
                         const t_upsact_cmd * const a_cmd);

/* this one is the type for a single action command */
typedef struct s_cmd_map {
  char *cmd;
  int  icmd;
  tpf_cmd func;
  int  min_params;
  int  max_params;
  int  icmd_undo;
} t_cmd_map;

/*
 * Declaration of private functions.
 */

int parse_params( const char * const a_params,
		   char *argv[] );

t_upslst_item *next_cmd( t_upslst_item * const top_list,
			 t_upslst_item *dep_list,
			 t_upsact_item * const p_cur,
			 const char * const act_name, 
			 const char copt );
t_upslst_item *get_top_prod( t_upsact_item *const p_cur, 
			     const char *const act_name );
t_upstyp_action *get_act( const t_upsugo_command * const ugo_cmd,
			  t_upstyp_matched_product * mat_prod,
			  const char * const act_name );
t_upsact_item *find_product( t_upslst_item *const dep_list,
			     const char *const prod_name );
t_upsact_item *find_product_ptr( t_upslst_item* const dep_list,
				 const t_upsact_item* const act_item );
t_upsact_item *new_act_item( t_upsugo_command * const ugo_cmd,
			     t_upstyp_matched_product *mat_prod,
			     const int level,
			     const char * const act_name );
t_upstyp_action *new_default_action( t_upsact_item *const p_cur, 
				     const char * const act_name, 
				     const int iact );
t_upslst_item *reverse_command_list( t_upsact_item *const p_cur,
				     t_upslst_item *const cmd_list );
int actname2enum( const char * const act_name );
char *action2inst( const t_upsact_item *const p_cur );

/* functions to handle specific action commands */

void f_copyhtml( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd);
void f_copyinfo( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd);
void f_copyman( const t_upstyp_matched_instance * const a_inst,
		const t_upstyp_db * const a_db_info,
		const t_upsugo_command * const a_command_line,
		const FILE * const a_stream,
		const t_upsact_cmd * const a_cmd);
void f_uncopyman( const t_upstyp_matched_instance * const a_inst,
		  const t_upstyp_db * const a_db_info,
		  const t_upsugo_command * const a_command_line,
		  const FILE * const a_stream,
		  const t_upsact_cmd * const a_cmd);
void f_copynews( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd);
void f_envappend( const t_upstyp_matched_instance * const a_inst,
		  const t_upstyp_db * const a_db_info,
		  const t_upsugo_command * const a_command_line,
		  const FILE * const a_stream,
		  const t_upsact_cmd * const a_cmd);
void f_envprepend( const t_upstyp_matched_instance * const a_inst,
		   const t_upstyp_db * const a_db_info,
		   const t_upsugo_command * const a_command_line,
		   const FILE * const a_stream,
		   const t_upsact_cmd * const a_cmd);
void f_envremove( const t_upstyp_matched_instance * const a_inst,
		  const t_upstyp_db * const a_db_info,
		  const t_upsugo_command * const a_command_line,
		  const FILE * const a_stream,
		  const t_upsact_cmd * const a_cmd);
void f_envset( const t_upstyp_matched_instance * const a_inst,
	       const t_upstyp_db * const a_db_info,
	       const t_upsugo_command * const a_command_line,
	       const FILE * const a_stream,
	       const t_upsact_cmd * const a_cmd);
void f_envunset( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd);
void f_exeaccess( const t_upstyp_matched_instance * const a_inst,
		  const t_upstyp_db * const a_db_info,
		  const t_upsugo_command * const a_command_line,
		  const FILE * const a_stream,
		  const t_upsact_cmd * const a_cmd);
void f_execute( const t_upstyp_matched_instance * const a_inst,
		const t_upstyp_db * const a_db_info,
		const t_upsugo_command * const a_command_line,
		const FILE * const a_stream,
		const t_upsact_cmd * const a_cmd);
void f_filetest( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd);
void f_makedir( const t_upstyp_matched_instance * const a_inst,
		const t_upstyp_db * const a_db_info,
		const t_upsugo_command * const a_command_line,
		const FILE * const a_stream,
		const t_upsact_cmd * const a_cmd);
void f_pathappend( const t_upstyp_matched_instance * const a_inst,
		   const t_upstyp_db * const a_db_info,
		   const t_upsugo_command * const a_command_line,
		   const FILE * const a_stream,
		   const t_upsact_cmd * const a_cmd);
void f_pathprepend( const t_upstyp_matched_instance * const a_inst,
		    const t_upstyp_db * const a_db_info,
		    const t_upsugo_command * const a_command_line,
		    const FILE * const a_stream,
		    const t_upsact_cmd * const a_cmd);
void f_pathremove( const t_upstyp_matched_instance * const a_inst,
		   const t_upstyp_db * const a_db_info,
		   const t_upsugo_command * const a_command_line,
		   const FILE * const a_stream,
		   const t_upsact_cmd * const a_cmd);
void f_pathset( const t_upstyp_matched_instance * const a_inst,
		const t_upstyp_db * const a_db_info,
		const t_upsugo_command * const a_command_line,
		const FILE * const a_stream,
		const t_upsact_cmd * const a_cmd);
void f_sourcerequired( const t_upstyp_matched_instance * const a_inst,
		       const t_upstyp_db * const a_db_info,
		       const t_upsugo_command * const a_command_line,
		       const FILE * const a_stream,
		       const t_upsact_cmd * const a_cmd);
void f_sourceoptional( const t_upstyp_matched_instance * const a_inst,
		       const t_upstyp_db * const a_db_info,
		       const t_upsugo_command * const a_command_line,
		       const FILE * const a_stream,
		       const t_upsact_cmd * const a_cmd);
void f_sourcereqcheck( const t_upstyp_matched_instance * const a_inst,
		       const t_upstyp_db * const a_db_info,
		       const t_upsugo_command * const a_command_line,
		       const FILE * const a_stream,
		       const t_upsact_cmd * const a_cmd);
void f_sourceoptcheck( const t_upstyp_matched_instance * const a_inst,
		       const t_upstyp_db * const a_db_info,
		       const t_upsugo_command * const a_command_line,
		       const FILE * const a_stream,
		       const t_upsact_cmd * const a_cmd);
void f_dodefaults( const t_upstyp_matched_instance * const a_inst,
		   const t_upstyp_db * const a_db_info,
		   const t_upsugo_command * const a_command_line,
		   const FILE * const a_stream,
		   const t_upsact_cmd * const a_cmd);

#define CHECK_NUM_PARAM(action) \
    if ((a_cmd->argc < g_cmd_maps[a_cmd->icmd].min_params) ||   \
        (a_cmd->argc > g_cmd_maps[a_cmd->icmd].max_params)) {   \
      upserr_vplace();                                          \
      upserr_add(UPS_INVALID_ACTION_PARAMS, UPS_FATAL,          \
                 action, g_cmd_maps[a_cmd->icmd].min_params,    \
                 g_cmd_maps[a_cmd->icmd].max_params,            \
		 a_cmd->argc);                                  \
    }

#define GET_DELIMITER() \
    if (a_cmd->argc == g_cmd_maps[a_cmd->icmd].max_params) {            \
      /* remember arrays start at 0, so subtract one here */            \
      delimiter =                                                       \
	(a_cmd->argv[g_cmd_maps[a_cmd->icmd].max_params-1]);            \
    } else {                                                            \
      /* use the default, nothing was entered */                        \
      delimiter = g_default_delimiter;                                  \
    }

#define GET_ERR_MESSAGE(msg_ptr) \
    if (msg_ptr) {                                 \
      err_message = msg_ptr;                       \
    } else {                                       \
      err_message = "";                            \
    }

#define FPRINTF_ERROR() \
    upserr_vplace();                                                     \
    upserr_add(UPS_SYSTEM_ERROR, UPS_FATAL, "fprintf", strerror(errno));

/*
 * Definition of global variables.
 */

static char *g_default_delimiter = ":";

/* 
 * Note: This array should in princip be in ups_main.c, but since
 * ups_main is not part of the ups library, it's here.
 *
 * The enum is defined in ups_main.h
 * where flag:
 * <byte>: <description>
 *   0   : specify if that action has a default set of commands
 */
t_cmd_info g_cmd_info[] = {
  {e_current,       "current", 0, 0x00000001, e_invalid_action},
  {e_development,   "development", 0, 0x00000000, e_invalid_action},
  {e_new,           "new", 0, 0x00000000, e_invalid_action},
  {e_old,           "old", 0, 0x00000000, e_invalid_action},
  {e_test,          "test", 0, 0x00000000, e_invalid_action},
  {e_chain,         "chain", 0, 0x00000000, e_invalid_action},
  {e_uncurrent,     "uncurrent", 0, 0x00000001, e_invalid_action},
  {e_undevelopment, "undevelopment", 0, 0x00000000, e_invalid_action},
  {e_unnew,         "unnew", 0, 0x00000000, e_invalid_action},
  {e_unold,         "unold", 0, 0x00000000, e_invalid_action},
  {e_untest,        "untest", 0, 0x00000000, e_invalid_action},
  {e_unchain,       "unchain", 0, 0x00000000, e_invalid_action},
  {e_setup,       "setup",       "?B:cde:f:g:H:jkm:M:noO:q:r:tU:vVz:Z", 0x00000001, e_invalid_action},
  {e_unsetup,     "unsetup",     "?cde:f:g:H:jm:M:noO:q:tU:vVz:Z", 0x00000001, e_setup},
  {e_list,        "list",        "a?cdf:g:h:H:K:lm:M:noq:r:tU:vVz:Z", 0x00000000, e_invalid_action},
  {e_configure,   "configure",   "?cdf:g:H:m:M:noO:q:r:tU:vVz:Z", 0x00000000, e_invalid_action},
  {e_copy,        "copy",        "?A:cCdf:g:H:m:M:noO:p:P:q:r:tT:U:vVWXz:Z", 0x00000000, e_invalid_action},
  {e_declare,     "declare",     "?A:cCdf:g:H:m:M:noO:p:q:r:tT:U:vVz:Z", 0x00000000, e_invalid_action},
  {e_depend,      "depend",      "?cdotg:f:H:K:m:M:q:r:U:vVz:Z", 0x00000000, e_invalid_action},
  {e_exist,       "exist",       "?B:cde:f:g:H:jkm:M:oO:q:r:tU:vVz:Z", 0x00000000, e_invalid_action},
  {e_modify,      "modify",      "?A:Ef:H:m:M:op:q:r:T:U:vVx:z:Z", 0x00000000, e_invalid_action},
  {e_start,       "start",       "?cdf:g:H:m:M:noO:q:r:tU:vVwz:Z", 0x00000000, e_invalid_action},
  {e_stop,        "stop",        "?cdf:g:H:m:M:noO:q:r:tU:vVz:Z", 0x00000000, e_invalid_action},
  {e_tailor,      "tailor",      "?cdf:g:h:H:K:m:M:noO:q:r:tU:vVz:Z", 0x00000000, e_invalid_action},
  {e_unconfigure, "unconfigure", "?cdf:g:H:m:M:noO:q:r:tU:vVz:Z", 0x00000000, e_invalid_action},
  {e_undeclare,   "undeclare",   "?cdf:g:H:m:M:noO:q:r:tU:vVyYz:Z", 0x00000000, e_invalid_action},
  {e_create,      "create",      "?f:H:m:M:p:q:vZ", 0x00000000, e_invalid_action},
  {e_get,         "get",         "?cdf:Fg:H:m:M:noq:r:tU:vVz:Z", 0x00000000, e_invalid_action},
  {e_validate,    "validate",    "?cdf:g:h:H:lm:M:nNoq:r:StU:vVz:Z", 0x00000000, e_invalid_action},
  {e_help,        "help",
            "a?A:B:cCdeEf:Fg:h:H:jkK:lm:M:nNoO:p:P:q:r:StT:U:vVwW:x:XyYz:Z", 0x00000000, e_invalid_action},
  /* the following one must always be at the end and contains all options */
  {e_unk,         NULL,
            "a?A:B:cCdeEf:Fg:h:H:jkK:lm:M:nNoO:p:P:q:r:StT:U:vVwW:x:XyYz:Z", 0x00000000, e_invalid_action}
};

enum {
  e_invalid_cmd = -1,
  e_setupoptional = 0,
  e_setuprequired,
  e_unsetupoptional,
  e_unsetuprequired, /* this action have to be the last setup/unsetup action */
  e_envappend,
  e_envremove,
  e_envprepend,
  e_envset,
  e_envunset,
  e_pathappend,
  e_pathremove,
  e_pathprepend,
  e_pathset,
  e_sourcerequired,
  e_sourceoptional,
  e_sourcereqcheck,
  e_sourceoptcheck,
  e_exeaccess,
  e_execute,
  e_filetest,
  e_makedir,
  e_copyhtml,
  e_copyinfo,
  e_copyman,
  e_uncopyman,
  e_copynews,
  e_dodefaults,
  e_nodefaults,
  e_nosetupenv,
  e_noproddir
};

/* These action commands are listed in order of use.  Hopefully the more
 * used actions are at the front of the list. Also the ones most used by
 * setup and unsetup are at the front of the array.  The actions in this
 * array MUST appear in the same order as in the above enumeration. NOTE:
 * the last two parameters are the minimum and maximum parameters expected
 * by the action.
 */
static t_cmd_map g_cmd_maps[] = {
  { "setupoptional", e_setupoptional, NULL, 0, 0, e_unsetupoptional },
  { "setuprequired", e_setuprequired, NULL, 0, 0, e_unsetuprequired },
  { "unsetupoptional", e_unsetupoptional, NULL, 0, 0, e_setupoptional },
  { "unsetuprequired", e_unsetuprequired, NULL, 0, 0, e_setuprequired },
  { "envappend", e_envappend, f_envappend, 2, 3, e_envremove },
  { "envremove", e_envremove, f_envremove, 2, 3, e_invalid_cmd },
  { "envprepend", e_envprepend, f_envprepend, 2, 3, e_envremove },
  { "envset", e_envset, f_envset, 2, 2, e_envunset },
  { "envunset", e_envunset, f_envunset, 1, 1, e_invalid_cmd },
  { "pathappend", e_pathappend, f_pathappend, 2, 3, e_pathremove },
  { "pathremove", e_pathremove, f_pathremove, 2, 3, e_invalid_cmd },
  { "pathprepend", e_pathprepend, f_pathprepend, 2, 3, e_pathremove },
  { "pathset", e_pathset, f_pathset, 2, 2, e_envunset },
  { "sourcerequired", e_sourcerequired, f_sourcerequired, 1, 1, e_invalid_cmd },
  { "sourceoptional", e_sourceoptional, f_sourceoptional, 1, 1, e_invalid_cmd },
  { "sourcereqcheck", e_sourcereqcheck, f_sourcereqcheck, 1, 1, e_invalid_cmd },
  { "sourceoptcheck", e_sourceoptcheck, f_sourceoptcheck, 1, 1, e_invalid_cmd },
  { "exeaccess", e_exeaccess, f_exeaccess, 1, 1, e_invalid_cmd },
  { "execute", e_execute, f_execute, 1, 2, e_invalid_cmd },
  { "filetest", e_filetest, f_filetest, 2, 3, e_invalid_cmd },
  { "makedir", e_makedir, f_makedir, 1, 1, e_invalid_cmd },
  { "copyhtml", e_copyhtml, f_copyhtml, 1, 1, e_invalid_cmd },
  { "copyinfo", e_copyinfo, f_copyinfo, 1, 1, e_invalid_cmd },
  { "copyman", e_copyman, f_copyman, 0, 1, e_uncopyman },
  { "uncopyman", e_uncopyman, f_uncopyman, 0, 1, e_copyman},
  { "copynews", e_copynews, f_copynews, 1, 1, e_invalid_cmd },
  { "dodefaults", e_dodefaults, f_dodefaults, 0, 0, e_dodefaults },
  { "nodefaults", e_nodefaults, NULL, 0, 0, e_nodefaults },
  { "nosetupenv", e_nosetupenv, NULL, 0, 0, e_invalid_cmd },
  { "noproddir", e_noproddir, NULL, 0, 0, e_invalid_cmd },
  { 0,0,0,0,0 }
};

/*
 * Definition of public functions.
 */

/*-----------------------------------------------------------------------
 * upsact_print_inst
 *
 * Input : t_upsugo_command *,
 *         t_upstyp_matched_product *,
 *         char *,
 * Output: none
 * Return: t_upsact_tree *,
 */
int upsact_print( t_upsugo_command * const ugo_cmd,
		  t_upstyp_matched_product *mat_prod,
		  const char * const act_name,
		  const char * const sopt )
{
  t_upslst_item *dep_list = 0;
  int doact = strchr( sopt, 'a' ) ? 1 : 0;

  if ( !ugo_cmd || !act_name )
    return 0;

  dep_list = upsact_get_cmd( ugo_cmd, mat_prod, act_name );

  /* print all actions */
  if ( doact ) {
    for ( ; dep_list; dep_list = dep_list->next )
      upsact_print_item( (t_upsact_item *)dep_list->data, sopt );
  }

  /* print only instances */
  else {
    t_upsugo_command *cur_ugo = 0;
    t_upslst_item *didit_list = 0;
    t_upsact_item *act0 = find_product( dep_list, ugo_cmd->ugo_product );

    /* add parent product to 'already done list' */
    didit_list = upslst_add( didit_list, act0 );

    for ( ; dep_list; dep_list = dep_list->next ) {
      t_upsact_item *act_ptr = (t_upsact_item *)dep_list->data;
      if ( act_ptr->ugo == cur_ugo )
	continue;
      if ( ! find_product_ptr( didit_list, act_ptr ) ) {
	didit_list = upslst_add( didit_list, act_ptr );
	upsact_print_item( act_ptr, sopt );
	cur_ugo = act_ptr->ugo;
      }
    }    
  }

  return 1;
}

/*-----------------------------------------------------------------------
 * upsact_get_cmd
 *
 * Input : t_upsugo_command *,
 *         t_upstyp_matched_product *,
 *         char *,
 * Output: none
 * Return: t_upsact_tree *,
 */
t_upslst_item *upsact_get_cmd( t_upsugo_command * const ugo_cmd,
			       t_upstyp_matched_product * const mat_prod,
			       const char * const act_name )
{
  t_upsact_item *new_cur;
  t_upslst_item *top_list = 0;
  t_upslst_item *dep_list = 0;

  if ( !ugo_cmd || !act_name )
    return 0;

  if ( !(new_cur = new_act_item( ugo_cmd, mat_prod, 0, act_name )) ) 
    return 0;
  top_list = get_top_prod( new_cur, act_name );

  dep_list = next_cmd( top_list, dep_list, new_cur, act_name, ' ' );

  return upslst_first( dep_list );
}

/*-----------------------------------------------------------------------
 * upsact_parse_cmd
 *
 * It will translate a single action command line.
 * The returned t_upsact_cmd structure is valid untill next call to
 * upsact_parse_cmd.
 *
 * Input : action line.
 * Output: none
 * Return: t_upsact_cmd *, a translated command line
 */
t_upsact_cmd *upsact_parse_cmd( const char * const cmd_str )
{
  static char trim_chars[] = " \t\n\r\f)";
  t_upsact_cmd *pcmd;
  char *act_s = (char *)cmd_str, *act_e = NULL;
  int icmd = e_invalid_action;
  int i;
  int len;

  /* get rid of leading spaces */
  
  while ( act_s && *act_s && isspace( *(act_s) ) ){ ++act_s; }

  if ( !act_s || !*act_s )
    return 0;

  /* create a new command structure */
  
  pcmd = (t_upsact_cmd *)malloc( sizeof( t_upsact_cmd ) + strlen( act_s ) + 1 );
  pcmd->pmem = (char *)(pcmd + 1);
  
  strcpy( pcmd->pmem, act_s );
  act_s = pcmd->pmem;

  if ( (act_e = strchr( act_s, OPEN_PAREN )) != NULL ) {
    len = act_e - act_s;

    /* look for action in the supported action array */
    
    for ( i = 0; g_cmd_maps[i].cmd; ++i ) {
      if ( !upsutl_strincmp( act_s, g_cmd_maps[i].cmd, (size_t)len ) ) {
	
	/* we found a match. create a pointer to a string with these parameters.
	   note - it does not include an open parenthesis */
	act_s = act_e + 1;
		 
	/* trim off whitespace & the ending ")" */	  
	upsutl_str_remove_edges( act_s, trim_chars );

        /* save the location in the array */
	icmd = i;
	
	break;
      }
    }
  }

  /* fill p_cmd, split parameter string into a list of arguments */
  
  if ( icmd != e_invalid_action ) {
    pcmd->icmd = icmd;
    pcmd->argc = parse_params( act_s, pcmd->argv );
    return pcmd;
  }
  else {
    free( pcmd );
    return 0;
  }
}

void upsact_cleanup( t_upslst_item *dep_list )
{
  /* here you should cleanup dep_list */
}


/*-----------------------------------------------------------------------
 * upsact_print_item
 *
 * options (sopt):
 *    always : print correspoding product instance.
 *    'l'    : also print instance level
 *    'a'    : also print corresponding action command
 *    't'    : also print indentions (corresponding to level)
 *
 * Input : action item
 *         iopt
 * Output: none
 * Return: none
 */
void upsact_print_item( const t_upsact_item *const p_cur, const char * const sopt )
{
  int i;

  if ( !p_cur )
    return;
  
  if ( strchr( sopt, 't' ) ) for ( i=0; i<p_cur->level; i++ ) { printf( "   " ); }
  if ( strchr( sopt, 'l' ) ) printf( "%d:", p_cur->level );
  printf( "%s", action2inst( p_cur ) );
  if ( strchr( sopt, 'a' ) ) {
    printf( ":" );
    upsact_print_cmd( p_cur->cmd );
  }
  else printf( "\n" );
}

void upsact_print_cmd( const t_upsact_cmd * const cmd_cur )
{
  int i;
  int icmd;
  
  if ( !cmd_cur )
    return;

  icmd = cmd_cur->icmd;
  
  printf( "%s(", g_cmd_maps[icmd].cmd );
 for ( i = 0; i < cmd_cur->argc; i++ ) {
    if ( i == cmd_cur->argc - 1 ) 
      printf( " %s ", cmd_cur->argv[i] );
    else
      printf( " %s,", cmd_cur->argv[i] );
  }
  printf( ")\n" ); 
}

/*-----------------------------------------------------------------------
 * upsact_check_files
 *
 * Check to see if the passed action command contains references to any
 * files.  
 *
 * Input : an action command
 * Output: none
 * Return: pointer to a list of file names
 */
t_upslst_item *upsact_check_files(
			    const t_upstyp_matched_product * const a_mproduct,
			    const t_upsugo_command * const a_command_line,
			    char *const a_cmd)
{
  t_upsact_cmd *parsed_cmd = NULL;
  t_upslst_item *file_list = NULL;
  char *new_string = NULL;
  char *trans_cmd = NULL;
  
  if (a_cmd) {
    /* translate any ${UPS...} variables */
    trans_cmd = upsget_translation(a_mproduct, a_command_line, a_cmd);

    /* parse the command */
    parsed_cmd = upsact_parse_cmd(trans_cmd);
    if (parsed_cmd) {
      /* command was successfully parsed.  if there were no arguments, none 
	 of them can be files (;-) */
      if (parsed_cmd->argc > 0) {
	/* there are arguments, now depending on the command, one of these
	   arguments may or may not be a file. only look at the argument if
	   the action parsed is capable of having a file as an argument */
	switch (parsed_cmd->icmd) {
	  /* none of these actions have associated files with them */
	case e_invalid_cmd :
	case e_setupoptional:
	case e_setuprequired:
	case e_unsetupoptional:
	case e_unsetuprequired:
	case e_envappend:
	case e_envremove:
	case e_envprepend:
	case e_envset:
	case e_envunset:
	case e_pathappend:
	case e_pathremove:
	case e_pathprepend:
	case e_pathset:
	case e_filetest:
	case e_dodefaults:
	case e_nodefaults:
	case e_nosetupenv:
	case e_noproddir:
	case e_exeaccess:
	case e_uncopyman:
	  break;
	  /* the following actions contain a file or directory path.  return
	     this value to the calling routine as a list element. */
	case e_sourcerequired:
	case e_sourceoptional:
	case e_sourcereqcheck:
	case e_sourceoptcheck:
	case e_copyhtml:
	case e_copyinfo:
	case e_copyman:
	case e_copynews:
	  if (parsed_cmd->argv[0]) {
	    new_string = upsutl_str_create(parsed_cmd->argv[0],
					   STR_TRIM_DEFAULT);
	    file_list = upslst_insert(file_list, new_string);
	  }
	  break;
	  /* if there is a path included when specifying the command to execute
	     then include this file, else we don't know the location as the
	     executable is assumed to be in the user's path. in the last case,
	     do not report any files mentioned here. */
	case e_execute:
	  if (parsed_cmd->argv[0] && 
	      (! strncmp(parsed_cmd->argv[0], SLASH, 1))) {
	    /* the first letter was a slash, so we assume that this string
	       contains a directory spec and a file name.  we return this to
	       the calling routine.  if the first letter was not a slash then
	       we cannot really tell where the binary is located as it is
	       either a relative path or assumed to be in the PATH environment
	       variable */
	    new_string = upsutl_str_create(parsed_cmd->argv[0],
					   STR_TRIM_DEFAULT);
	    file_list = upslst_insert(file_list, new_string);
	  }
	  break;
	}
      }
    }
  }

  return(file_list);
}

void upsact_free_upsact_cmd( t_upsact_cmd * const act_cmd )
{
  if ( act_cmd ) 
    free( act_cmd );
}

/*
 * Private functions
 */

/*-----------------------------------------------------------------------
 * get_act
 *
 * Input : t_upsugo_command *,
 * Output: none
 * Return: t_upsact_action *,
 */
t_upstyp_action *get_act( const t_upsugo_command * const ugo_cmd,
			  t_upstyp_matched_product * mat_prod,
			  const char * const act_name )
{
  t_upstyp_matched_instance *mat_inst;

  if ( !ugo_cmd || !act_name )
    return 0;

  if ( !mat_prod ) {
    t_upslst_item *l_mproduct = upsmat_instance( (t_upsugo_command *)ugo_cmd, NULL, 1 );
    if ( !l_mproduct || !l_mproduct->data )
      return 0;
    mat_prod = (t_upstyp_matched_product *)l_mproduct->data;
  }

  mat_inst = (t_upstyp_matched_instance *)(upslst_first( mat_prod->minst_list ))->data;
  if ( mat_inst->table ) 
    return upskey_inst_getaction( mat_inst->table, act_name );
  else
    return 0;
}

/*-----------------------------------------------------------------------
 * get_top_prod
 *
 * Input : t_upsact_tree *,
 * Output: none
 * Return: t_upsact_cmd *,
 */
t_upslst_item *get_top_prod( t_upsact_item *const p_cur, 
			     const char *const act_name )
{
  static char *valid_switch = "AacCdfghKtmMNoOPqrTuUv";
  t_upslst_item *l_cmd = p_cur->act ? p_cur->act->command_list : 0;
  t_upsact_cmd *p_cmd = 0;
  char *p_line = 0;
  t_upslst_item *top_list = 0;
  int i_act = actname2enum( act_name );

  for ( ; l_cmd; l_cmd = l_cmd->next ) {
    p_line = upsget_translation( p_cur->mat, p_cur->ugo,
				 (char *)l_cmd->data );
    p_cmd = upsact_parse_cmd( p_line );

    if ( p_cmd && p_cmd->icmd >= 0 ) {
      t_upsact_item *new_cur = 0;
      t_upsugo_command *new_ugo = 0;
      p_cmd->iact = i_act;
      switch ( p_cmd->icmd ) 
      {
      case e_setuprequired: case e_setupoptional:
	new_ugo = upsugo_bldcmd( p_cmd->argv[0], valid_switch );
	new_cur = new_act_item( new_ugo, 0, 0, "SETUP");
	break;
	
      case e_unsetuprequired: case e_unsetupoptional:
	new_ugo = upsugo_bldcmd( p_cmd->argv[0], valid_switch );
	new_cur = new_act_item( new_ugo, 0, 0, "UNSETUP");
	break;
      }
      if ( new_cur )
	top_list = upslst_add( top_list, new_cur );
    }
  }

  return upslst_first( top_list );
}

/*-----------------------------------------------------------------------
 * next_cmd
 *
 * Input : t_upsact_tree *,
 * Output: none
 * Return: t_upsact_cmd *,
 */
t_upslst_item *next_cmd( t_upslst_item * const top_list,
			 t_upslst_item *dep_list,
			 t_upsact_item *const p_cur,
			 const char *const act_name,
			 const char copt )
{
  static char *valid_switch = "AacCdfghKtmMNoOPqrTuUv";

  t_upslst_item *l_cmd = 0;
  t_upsact_cmd *p_cmd = 0;
  char *p_line = 0;
  int i_act = e_invalid_action;

  if ( !p_cur )
    return dep_list;

  i_act = actname2enum( act_name );

  /* take care of defaults cases */

  if ( !p_cur->act ) {
    if ( !(p_cur->act = new_default_action( p_cur, act_name, i_act )) )
      return dep_list;
  }
  
  l_cmd = p_cur->act->command_list;
  for ( ; l_cmd; l_cmd = l_cmd->next ) {

    /* translate and parse command */    
    p_line = upsget_translation( p_cur->mat, p_cur->ugo,
				 (char *)l_cmd->data );
    p_cmd = upsact_parse_cmd( p_line );

    if ( p_cmd && p_cmd->icmd >= 0 ) {
      p_cmd->iact = i_act;
      if ( p_cmd->icmd > e_unsetuprequired ) {
	t_upsact_item *new_cur = (t_upsact_item *)upsmem_malloc( sizeof( t_upsact_item ) );
	new_cur->level = p_cur->level;
	new_cur->ugo = p_cur->ugo;
	new_cur->mat = p_cur->mat;
	new_cur->act = p_cur->act;
	new_cur->cmd = p_cmd;
	dep_list = upslst_add( dep_list, new_cur );
	continue;
      }
      else if ( copt != 't' ) { /* here we go again */
	t_upsact_item *new_cur = 0;
	t_upsugo_command *new_ugo = upsugo_bldcmd( p_cmd->argv[0], valid_switch );

	if ( !new_ugo ) {
	  printf( "???? no new ugo on %s\n", p_line );
	  continue;
	}

	/* check if product is already at top level */

	if ( p_cur->level > 0 ) {
	  new_cur = find_product( top_list, new_ugo->ugo_product );
	}

	/* check if product is already setup */
	
	if ( find_product( dep_list, new_ugo->ugo_product ) ) {
	  /* ??? free stuff */
	  continue;
	}
	
	if ( !new_cur ) {
	  switch ( p_cmd->icmd ) 
	  {
	  case e_setuprequired: case e_setupoptional:
	    new_cur = new_act_item( new_ugo, 0, 0, "SETUP");
	    break;	
	  case e_unsetuprequired: case e_unsetupoptional:
	    new_cur = new_act_item( new_ugo, 0, 0, "UNSETUP");
	    break;
	  }
	  if ( !new_cur ) {
	    printf( "???? no action_item for %s\n", p_line );
	    continue;
	  }
	}
	new_cur->level = p_cur->level + 1;
	dep_list = next_cmd( top_list, dep_list, new_cur, act_name, copt );
	continue;
      }
    }
    else {
      printf( "parse_cmd failed on %s\n", p_line );
    }
  }
  
  return dep_list;
}

t_upsact_item *find_product( t_upslst_item* const dep_list,
			     const char *const prod_name )
{
  t_upslst_item *l_ptr = upslst_first( (t_upslst_item *)dep_list );
  for ( ; l_ptr; l_ptr = l_ptr->next ) {
    t_upsact_item *p_item = (t_upsact_item *)l_ptr->data;
    if ( upsutl_stricmp( prod_name, p_item->ugo->ugo_product ) == 0 )
      return p_item;
  }
  return 0;    
}

t_upsact_item *find_product_ptr( t_upslst_item* const dep_list,
				 const t_upsact_item* const act_item )
{
  t_upslst_item *l_ptr = (t_upslst_item *)upslst_first( dep_list );
  for ( ; l_ptr; l_ptr = l_ptr->next ) {
    t_upsact_item *act_ptr = (t_upsact_item *)l_ptr->data;
    if ( act_item->ugo == act_ptr->ugo )
      return act_ptr;
  }
  return 0;    
}

char *action2inst( const t_upsact_item *const p_cur )
{
  static char buf[MAX_LINE_LEN];
  t_upstyp_matched_instance *mat_inst;
  t_upslst_item *l_item;
  
  if ( !p_cur )
    return 0;

  l_item = upslst_first( p_cur->mat->minst_list );
  mat_inst = (t_upstyp_matched_instance *)l_item->data;
  strcpy( buf, upsget_envstr( 0, mat_inst, p_cur->ugo ) );
  l_item = upslst_first( p_cur->ugo->ugo_chain );
  if ( l_item && l_item->data ) {
    strcat( buf, " -g " );
    strcat( buf, (char *)l_item->data );
  }

  return buf;
}

int actname2enum( const char * const act_name )
{
  int iact = e_invalid_action, i = 0;

  if ( act_name ) {
    for ( i=0; g_cmd_info[i].cmd; i++ ) {
      if (! upsutl_stricmp(g_cmd_info[i].cmd, act_name)) {
	iact = i;
	break;
      }
    }
  }
  
  return iact;
}

/*-----------------------------------------------------------------------
 * parse_params
 *
 * Given an action's parameters split them up into separate params and
 * return a linked list of the separate parameters.  The parameters are
 * separated by commas, but ignore commas within quotes.
 *
 * The routine writes into passed parameter string.
 *
 * called by upsact_parse.
 *
 * Input : parameter string
 *         pointer to array of arguments
 * Output: pointer to array of arguments
 * Return: number of arguments found
 */
int parse_params( const char * const a_params, char **argv )
{
  char *ptr = (char *)a_params, *saved_ptr = NULL;
  char *new_ptr;
  int count = 0;

  while ( ptr && *ptr ) {

    if ( count >= UPS_MAX_ARGC ) {
      upserr_vplace();
      upserr_add( UPS_TOO_MANY_ACTION_ARG, UPS_FATAL, a_params );
      return 0;
    }
    
    switch ( *ptr ) {
      
    case DQUOTE:    /* found a double quote */
      
      /* this may be the beginning of the line, saved_ptr is not set yet
	 so do it now. */
      
      if ( !saved_ptr )
	saved_ptr = ptr;           /* the beginning of a new parameter */
      
      /* found a double quote, skip to next double quote */
      
      if ( (new_ptr = strchr( ++ptr, (int)DQUOTE) ) == NULL ) {
	
	/* did not find another quote  - take the end of the line as end of
	   string and end of param list */
	
	saved_ptr = ptr;         /* no longer valid, we got the param */
	ptr = NULL;               /* all done */
      }
      else {
	
	/* point string just past double quote */
	
	ptr = ++new_ptr;
      }
      break;
      
    case COMMA:     /* found a comma */       

      if ( saved_ptr ) {
	
	/* we have a param, create a new pointer to the string */
        /* and add it to the list */
	
	*ptr = '\0';
	upsutl_str_remove_edges( saved_ptr, WSPACE );
	argv[count++] = saved_ptr;
	
      }
      ++ptr;                       /* go past the comma */
      saved_ptr = ptr;             /* start of param */
      break;
      
    default:       /* go to next char */
      
      if ( !saved_ptr ) {
	saved_ptr = ptr;           /* the beginning of a new parameter */
      }
      ++ptr;                       /* go to the next character */
    }
  }

  if ( saved_ptr ) {
    
    /* Get the last one too */
    upsutl_str_remove_edges( saved_ptr, WSPACE );
    argv[count++] = saved_ptr;
  }
  
  return count;
}


t_upsact_item *new_act_item( t_upsugo_command * const ugo_cmd,
			     t_upstyp_matched_product *mat_prod,
			     const int level,
			     const char * const act_name )
{		       
  t_upsact_item *act_item = 0;

  if ( !mat_prod ) {
    t_upslst_item *l_mproduct = upsmat_instance( ugo_cmd, NULL, 1 );
    if ( !l_mproduct || !l_mproduct->data )
      return 0;
    mat_prod = (t_upstyp_matched_product *)l_mproduct->data;
  }

  act_item = (t_upsact_item *)upsmem_malloc( sizeof( t_upsact_item ) );
  act_item->level = level;
  act_item->ugo = ugo_cmd;
  act_item->mat = mat_prod;
  act_item->act = get_act( ugo_cmd, mat_prod, act_name );
  act_item->cmd = 0;

  return act_item;
}

t_upstyp_action *new_default_action( t_upsact_item *const p_cur, 
				     const char * const act_name, 
				     const int iact )
{
  t_upstyp_action *new_act = 0;
  int i_uncmd = 0;

  if ( iact == e_invalid_action )
    return 0;

  if ( (i_uncmd = g_cmd_info[iact].uncmd_index) != e_invalid_action ) {
    t_upstyp_action* act_p = get_act( p_cur->ugo, p_cur->mat, g_cmd_info[i_uncmd].cmd );

    new_act = (t_upstyp_action *)malloc( sizeof( t_upstyp_action ) );
    new_act->action = (char *)malloc( strlen( act_name ) + 1 );
    strcpy( new_act->action, act_name );
    new_act->command_list = reverse_command_list( p_cur, act_p->command_list );
  }      
  else if ( (g_cmd_info[iact].flags)&0x00000001 ) {
    new_act = (t_upstyp_action *)malloc( sizeof( t_upstyp_action ) );
    new_act->action = (char *)malloc( strlen( act_name ) + 1 );
    strcpy( new_act->action, act_name );
    new_act->command_list = 0;
    new_act->command_list = upslst_add( new_act->command_list, 
					upsutl_str_create( "dodefault()", ' ' ) );
  }

  return new_act;
}

t_upslst_item *reverse_command_list( t_upsact_item *const p_cur, 
				     t_upslst_item *const cmd_list )
{
  static char buf[MAX_LINE_LEN];
  t_upslst_item *l_ucmd = 0;
  t_upslst_item *l_cmd = upslst_first( cmd_list );
  int i_uncmd = e_invalid_cmd;
  int i_cmd = e_invalid_cmd;
  int argc = 0;
  char *p_line;
  t_upsact_cmd *p_cmd;
  int i;

  for ( ; l_cmd; l_cmd = l_cmd->next ) {
    /* translate and parse command */    
    p_line = upsget_translation( p_cur->mat, p_cur->ugo,
				 (char *)l_cmd->data );
    p_cmd = upsact_parse_cmd( p_line );
    if ( p_cmd && ((i_cmd = p_cmd->icmd) != e_invalid_cmd) ) {
      if ( (i_uncmd = g_cmd_maps[p_cmd->icmd].icmd_undo) != e_invalid_cmd ) {
	strcpy( buf,  g_cmd_maps[i_uncmd].cmd );

	argc = g_cmd_maps[i_cmd].max_params;
	if ( argc > g_cmd_maps[i_uncmd].max_params )
	  argc = g_cmd_maps[i_uncmd].max_params;
	if ( argc > p_cmd->argc )
	  argc = p_cmd->argc;

	strcat( buf, "(" );
	for ( i=0; i<argc; i++ ) {
	  if ( i > 0 ) strcat( buf, ", " );	    
	  strcat( buf, p_cmd->argv[i] );
	}
	strcat( buf, ")" );

	l_ucmd = upslst_add( l_ucmd, 
			     upsutl_str_create( buf, ' ' ) );
      }
      free( p_cmd );      
    }    
  }

  return upslst_first( l_ucmd );
}

/*-----------------------------------------------------------------------
 * upsact_process_commands
 *
 * Given a list of commands, call each associated function to output the
 * shell specific code.
 *
 * Input : list of upsact_cmd items
 *         a stream to write to
 * Output: 
 * Return: none
 */
void upsact_process_commands( const t_upslst_item * const a_cmd_list,
			      const FILE * const a_stream)
{
  t_upslst_item *cmd_item;
  t_upsact_item *the_cmd;

  for (cmd_item = (t_upslst_item *)a_cmd_list ; cmd_item ;
       cmd_item = cmd_item->next ) {
    the_cmd = (t_upsact_item *)cmd_item->data;
    
    /* call the function associated with the command */
    if (g_cmd_maps[the_cmd->cmd->icmd].func) {
      g_cmd_maps[the_cmd->cmd->icmd].func(
		  (t_upstyp_matched_instance *)the_cmd->mat->minst_list->data,
		  the_cmd->mat->db_info, the_cmd->ugo, a_stream, the_cmd->cmd);
      if (UPS_ERROR != UPS_SUCCESS) {
	break;
      }
    }
  }
}


/* Action handling - the following routines are the ones that output shell
 *   specific code for each action supported by UPS
 */

void f_copyhtml( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("copyHtml");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {

    /* Make sure we have somewhere to copy the files to. */
    if (!a_db_info->config->html_path) {
      upserr_vplace();
      upserr_add(UPS_NO_DESTINATION, UPS_WARNING, "html");
    } else {  
      switch ( a_command_line->ugo_shell ) {
      case e_BOURNE:
      case e_CSHELL:
	if (fprintf((FILE *)a_stream, "cp %s/* %s\n#\n", 
		    a_cmd->argv[0], a_db_info->config->html_path) < 0) {
	  FPRINTF_ERROR();
	}
	break;
      default:
	upserr_vplace();
	upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
      }
      if (UPS_ERROR != UPS_SUCCESS) {
	upserr_vplace();
	upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		   g_cmd_maps[a_cmd->icmd].cmd);
      }
    }
  }
}

void f_copyinfo( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("copyInfo");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {

    /* Make sure we have somewhere to copy the files to. */
    if (!a_db_info->config->info_path) {
      upserr_vplace();
      upserr_add(UPS_NO_DESTINATION, UPS_WARNING, "info");
    } else {  
      switch ( a_command_line->ugo_shell ) {
      case e_BOURNE:
      case e_CSHELL:
	if (fprintf((FILE *)a_stream, "cp %s/* %s\n#\n", 
		    a_cmd->argv[0], a_db_info->config->info_path) < 0) {
	  FPRINTF_ERROR();
	}
	break;
      default:
	upserr_vplace();
	upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
      }
      if (UPS_ERROR != UPS_SUCCESS) {
	upserr_vplace();
	upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		   g_cmd_maps[a_cmd->icmd].cmd);
      }
    }
  }
}

void f_copyman( const t_upstyp_matched_instance * const a_inst,
		const t_upstyp_db * const a_db_info,
		const t_upsugo_command * const a_command_line,
		const FILE * const a_stream,
		const t_upsact_cmd * const a_cmd)
{
  char *buf = NULL;

  CHECK_NUM_PARAM("copyMan");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {

    /* Make sure we have somewhere to copy the files to. */
    if (!a_db_info->config->man_path) {
      upserr_vplace();
      upserr_add(UPS_NO_DESTINATION, UPS_WARNING, "man");
    } else {  
      switch ( a_command_line->ugo_shell ) {
      case e_BOURNE:
      case e_CSHELL:
	if (a_cmd->argc == 1) {
	  /* the user specified a source in the action */
	  buf = (char *)a_cmd->argv[0];
	} else {
	  /* we have to construct a source */
	  buf = upsutl_find_manpages(a_inst, a_db_info);
	}
	if (fprintf((FILE *)a_stream, "cp %s/* %s\n#\n", 
		    buf, a_db_info->config->man_path) < 0) {
	  FPRINTF_ERROR();
	}
	break;
      default:
	upserr_vplace();
	upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
      }
      if (UPS_ERROR != UPS_SUCCESS) {
	upserr_vplace();
	upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		   g_cmd_maps[a_cmd->icmd].cmd);
      }
    }
  }
}

void f_uncopyman( const t_upstyp_matched_instance * const a_inst,
		  const t_upstyp_db * const a_db_info,
		  const t_upsugo_command * const a_command_line,
		  const FILE * const a_stream,
		  const t_upsact_cmd * const a_cmd)
{
  char *buf = NULL;
  t_upslst_item *man_item, *man_list;

  CHECK_NUM_PARAM("uncopyMan");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {

    /* Make sure we have somewhere to copy the files to. */
    if (!a_db_info->config->man_path) {
      upserr_vplace();
      upserr_add(UPS_NO_DESTINATION, UPS_WARNING, "man");
    } else {  
      switch ( a_command_line->ugo_shell ) {
      case e_BOURNE:
      case e_CSHELL:
	if (a_cmd->argc == 1) {
	  /* the user specified a source in the action (gotten from current
	     action */
	  buf = (char *)a_cmd->argv[0];
	} else {
	  /* we have to construct a source */
	  buf = upsutl_find_manpages(a_inst, a_db_info);
	}

	/* Get a list of all the files in the specified directory */
	upsutl_get_files(buf, ANY_MATCH, &man_list);

	for (man_item = man_list ; man_item ; man_item = man_item->next) {
	  if (fprintf((FILE *)a_stream, "rm %s/%s\n", 
		      a_db_info->config->man_path, (char *)man_item->data)
	      < 0) {
	    FPRINTF_ERROR();
	    break;
	  }
	}
	fprintf((FILE *)a_stream, "#\n");
	break;
      default:
	upserr_vplace();
	upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
      }
      if (UPS_ERROR != UPS_SUCCESS) {
	upserr_vplace();
	upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		   g_cmd_maps[a_cmd->icmd].cmd);
      }
    }
  }
}

void f_copynews( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("copyNews");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {

    /* Make sure we have somewhere to copy the files to. */
    if (!a_db_info->config->news_path) {
      upserr_vplace();
      upserr_add(UPS_NO_DESTINATION, UPS_WARNING, "news");
    } else {  
      switch ( a_command_line->ugo_shell ) {
      case e_BOURNE:
      case e_CSHELL:
	if (fprintf((FILE *)a_stream, "cp %s/* %s\n#\n", 
		    a_cmd->argv[0], a_db_info->config->news_path) < 0) {
	  FPRINTF_ERROR();
	}
	break;
      default:
	upserr_vplace();
	upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
      }
      if (UPS_ERROR != UPS_SUCCESS) {
	upserr_vplace();
	upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		   g_cmd_maps[a_cmd->icmd].cmd);
      }
    }
  }
}

void f_envappend( const t_upstyp_matched_instance * const a_inst,
		  const t_upstyp_db * const a_db_info,
		  const t_upsugo_command * const a_command_line,
		  const FILE * const a_stream,
		  const t_upsact_cmd * const a_cmd)
{
  char *delimiter;
  
  CHECK_NUM_PARAM("envAppend");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    /* get the correct delimiter */
    GET_DELIMITER();

    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "%s=\${%s-}%s%s;export %s\n#\n",
		  a_cmd->argv[0], a_cmd->argv[0], delimiter, a_cmd->argv[1],
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "setenv %s \${%s}%s%s\n#\n",
		  a_cmd->argv[0], a_cmd->argv[0], delimiter,
		  a_cmd->argv[1]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_envprepend( const t_upstyp_matched_instance * const a_inst,
		   const t_upstyp_db * const a_db_info,
		   const t_upsugo_command * const a_command_line,
		   const FILE * const a_stream,
		   const t_upsact_cmd * const a_cmd)
{
  char *delimiter;
  
  CHECK_NUM_PARAM("envPrepend");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    /* get the correct delimiter */
    GET_DELIMITER();

    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "%s=%s%s\${%s-};export %s\n#\n",
		  a_cmd->argv[0], a_cmd->argv[1], delimiter, a_cmd->argv[0],
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "setenv %s %s%s\${%s}\n#\n",
		  a_cmd->argv[0], a_cmd->argv[1], delimiter,
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }

    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_envremove( const t_upstyp_matched_instance * const a_inst,
		  const t_upstyp_db * const a_db_info,
		  const t_upsugo_command * const a_command_line,
		  const FILE * const a_stream,
		  const t_upsact_cmd * const a_cmd)
{
  char *delimiter;
  
  CHECK_NUM_PARAM("envRemove");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    /* get the correct delimiter */
    GET_DELIMITER();

    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream,
		  "upstmp=`dropit.pl %s %s %s`;\nif [ $? -eq 0 ]; then %s=$upstmp; fi\nunset upstmp;\n#\n",
		  a_cmd->argv[0], a_cmd->argv[1], delimiter,
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream,
		  "setenv upstmp \"`dropit.pl %s %s %s`\"\nif ($status == 0) setenv %s $upstmp\nunsetenv upstmp\n#\n",
		  a_cmd->argv[0], a_cmd->argv[0], delimiter,
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    
      if (UPS_ERROR != UPS_SUCCESS) {
	upserr_vplace();
	upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		   g_cmd_maps[a_cmd->icmd].cmd);
      }
    }
  }
}

void f_envset( const t_upstyp_matched_instance * const a_inst,
	       const t_upstyp_db * const a_db_info,
	       const t_upsugo_command * const a_command_line,
	       const FILE * const a_stream,
	       const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("envSet");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "%s=%s;export %s\n#\n", a_cmd->argv[0],
		  a_cmd->argv[1], a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "setenv %s %s\n#\n", a_cmd->argv[0],
		  a_cmd->argv[1]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_envunset( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("envUnset");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "unset %s\n#\n", a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "unsetenv %s\n#\n", a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_exeaccess( const t_upstyp_matched_instance * const a_inst,
		  const t_upstyp_db * const a_db_info,
		  const t_upsugo_command * const a_command_line,
		  const FILE * const a_stream,
		  const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("exeAccess");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream,
		  "hash %s;\nif [ $? -eq 1 ]; then return 1; fi\n#\n",
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream,
		  "whereis %s\nif ($status == 1) return 1\n#\n",
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_execute( const t_upstyp_matched_instance * const a_inst,
		const t_upstyp_db * const a_db_info,
		const t_upsugo_command * const a_command_line,
		const FILE * const a_stream,
		const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("execute");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "%s=`%s`;export %s\n#\n", a_cmd->argv[1],
		  a_cmd->argv[0], a_cmd->argv[1]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "setenv %s \"`%s`\"\n#\n", a_cmd->argv[1],
		  a_cmd->argv[0])< 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_filetest( const t_upstyp_matched_instance * const a_inst,
		 const t_upstyp_db * const a_db_info,
		 const t_upsugo_command * const a_command_line,
		 const FILE * const a_stream,
		 const t_upsact_cmd * const a_cmd)
{
  char *err_message;
  
  CHECK_NUM_PARAM("fileTest");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    /* get the correct error message */
    GET_ERR_MESSAGE((char *)a_cmd->argv[g_cmd_maps[a_cmd->icmd].max_params-1]);

    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream,
		  "if [ ! %s %s ]; then\necho %s;\nreturn 1;\nfi;\n#\n",
		  a_cmd->argv[1], a_cmd->argv[0], err_message) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream,
		  "if ( ! %s %s ) then\necho %s\nreturn 1\nendif\n#\n", 
		  a_cmd->argv[1], a_cmd->argv[0], err_message) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_makedir( const t_upstyp_matched_instance * const a_inst,
		const t_upstyp_db * const a_db_info,
		const t_upsugo_command * const a_command_line,
		const FILE * const a_stream,
		const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("makeDir");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "mkdir %s\n#\n", a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_pathappend( const t_upstyp_matched_instance * const a_inst,
		   const t_upstyp_db * const a_db_info,
		   const t_upsugo_command * const a_command_line,
		   const FILE * const a_stream,
		   const t_upsact_cmd * const a_cmd)
{
  char *delimiter;
  
  CHECK_NUM_PARAM("pathAppend");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    /* get the correct delimiter */
    GET_DELIMITER();

    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "%s=${%s-}%s%s;export %s\n#\n",
		  a_cmd->argv[0], a_cmd->argv[0], delimiter, a_cmd->argv[1],
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "set %s=($%s %s)\nrehash\n#\n",
		  a_cmd->argv[0], a_cmd->argv[0], a_cmd->argv[1]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_pathprepend( const t_upstyp_matched_instance * const a_inst,
		    const t_upstyp_db * const a_db_info,
		    const t_upsugo_command * const a_command_line,
		    const FILE * const a_stream,
		    const t_upsact_cmd * const a_cmd)
{
  char *delimiter;
  
  CHECK_NUM_PARAM("pathPrepend");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    /* get the correct delimiter */
    GET_DELIMITER();

    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "%s=%s%s\${%s-};export %s\n#\n",
		  a_cmd->argv[0], a_cmd->argv[1], delimiter, a_cmd->argv[0],
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "set %s=(%s $%s)\nrehash\n#\n",
		  a_cmd->argv[0], a_cmd->argv[1], a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }

    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_pathremove( const t_upstyp_matched_instance * const a_inst,
		   const t_upstyp_db * const a_db_info,
		   const t_upsugo_command * const a_command_line,
		   const FILE * const a_stream,
		   const t_upsact_cmd * const a_cmd)
{
  char *delimiter;
  
  CHECK_NUM_PARAM("pathRemove");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    /* get the correct delimiter */
    GET_DELIMITER();

    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream,
		  "upstmp=`dropit.pl %s %s %s`;\nif [ $? -eq 0 ]; then %s=$upstmp; fi\nunset upstmp;\n#\n",
		  a_cmd->argv[0], a_cmd->argv[1], delimiter,
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream,
		  "setenv upstmp \"`dropit.pl %s %s %s`\"\nif ($status == 0) set %s=$upstmp\nrehash\nunsetenv upstmp\n#\n",
		  a_cmd->argv[0], a_cmd->argv[0], delimiter, 
		  a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    
      if (UPS_ERROR != UPS_SUCCESS) {
	upserr_vplace();
	upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		   g_cmd_maps[a_cmd->icmd].cmd);
      }
    }
  }
}

void f_pathset( const t_upstyp_matched_instance * const a_inst,
		const t_upstyp_db * const a_db_info,
		const t_upsugo_command * const a_command_line,
		const FILE * const a_stream,
		const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("pathSet");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "%s=%s;export %s\n#\n", a_cmd->argv[0],
		  a_cmd->argv[1], a_cmd->argv[0]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "set %s=(%s)\nrehash\n#\n", a_cmd->argv[0],
		  a_cmd->argv[1]) < 0) {
	FPRINTF_ERROR();
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_sourcerequired( const t_upstyp_matched_instance * const a_inst,
		       const t_upstyp_db * const a_db_info,
		       const t_upsugo_command * const a_command_line,
		       const FILE * const a_stream,
		       const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("sourceRequired");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    /* define all of the UPS local variables that the user may need. */
    upsget_allout(a_stream, a_db_info, a_inst, a_command_line);
    if (UPS_ERROR == UPS_SUCCESS) {
      g_LOCAL_VARS_DEF = 1;   /* keep track that we defined local variables */
      switch ( a_command_line->ugo_shell ) {
      case e_BOURNE:
	if (fprintf((FILE *)a_stream, ". %s\n#\n", a_cmd->argv[0]) < 0) {
	  FPRINTF_ERROR();
	}
	break;
      case e_CSHELL:
	if (fprintf((FILE *)a_stream, "source %s\n#\n", a_cmd->argv[0]) < 0) {
	  FPRINTF_ERROR();
	}
	break;
      default:
	upserr_vplace();
	upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
      }
    } else {
      FPRINTF_ERROR();
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_sourceoptional( const t_upstyp_matched_instance * const a_inst,
		       const t_upstyp_db * const a_db_info,
		       const t_upsugo_command * const a_command_line,
		       const FILE * const a_stream,
		       const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("sourceOptional");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "if [ -s %s ]; then\n", a_cmd->argv[0])
	  < 0) {
	FPRINTF_ERROR();
      } else {
	if (fprintf((FILE *)a_stream,
		    "if [ ! -r %s -o ! -x %s]; then\n  echo File to be optionally sourced (%s) is not readable or not executable;\nelse\n",
		    a_cmd->argv[0], a_cmd->argv[0], a_cmd->argv[0]) < 0) {
	  FPRINTF_ERROR();
	} else {
	  /* define all of the UPS local variables that the user may need. */
	  upsget_allout(a_stream, a_db_info, a_inst, a_command_line);
	  if (UPS_ERROR == UPS_SUCCESS) {
	    g_LOCAL_VARS_DEF = 1;   /* we defined local variables */
	    if (fprintf((FILE *)a_stream, "  . %s;\n;\nfi;\nfi;\n#\n", 
			a_cmd->argv[0]) < 0) {
	      FPRINTF_ERROR();
	    }
	  } else {
	    FPRINTF_ERROR();
	  }
	}
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "if (-e %s) then\n\n", a_cmd->argv[0])
	  < 0) {
	FPRINTF_ERROR();
      } else {
	if (fprintf((FILE *)a_stream,
		    "if (! -r %s || ! -x %s) then\n  echo File to be optionally sourced (%s) is not readable or not executable\nelse\n", 
		  a_cmd->argv[0], a_cmd->argv[0], a_cmd->argv[0]) < 0) {
	  FPRINTF_ERROR();
	} else {
	  /* define all of the UPS local variables that the user may need. */
	  upsget_allout(a_stream, a_db_info, a_inst, a_command_line);
	  if (UPS_ERROR == UPS_SUCCESS) {
	    g_LOCAL_VARS_DEF = 1;   /* we defined local variables */
	    if (fprintf((FILE *)a_stream, "  source %s\n\nendif\nendif\n#\n", 
			a_cmd->argv[0]) < 0) {
	      FPRINTF_ERROR();
	    }
	  } else {
	    FPRINTF_ERROR();
	  }
	}
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_sourcereqcheck( const t_upstyp_matched_instance * const a_inst,
		       const t_upstyp_db * const a_db_info,
		       const t_upsugo_command * const a_command_line,
		       const FILE * const a_stream,
		       const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("sourceReqCheck");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
    /* define all of the UPS local variables that the user may need. */
    upsget_allout(a_stream, a_db_info, a_inst, a_command_line);
    g_LOCAL_VARS_DEF = 1;   /* we defined local variables */
    if (UPS_ERROR == UPS_SUCCESS) {
      switch ( a_command_line->ugo_shell ) {
      case e_BOURNE:
	if (fprintf((FILE *)a_stream,
		    ". %s;\nif [ $? -eq 1 ]; then return 1; fi;\n#\n",
		    a_cmd->argv[0]) < 0) {
	  FPRINTF_ERROR();
	}
	break;
      case e_CSHELL:
	if (fprintf((FILE *)a_stream,
		    "source %s\nif ($status == 1) return 1/n#\n",
		    a_cmd->argv[0]) < 0) {
	  FPRINTF_ERROR();
	}
	break;
      default:
	upserr_vplace();
	upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
      }
    } else {
      FPRINTF_ERROR();
    }

    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_sourceoptcheck( const t_upstyp_matched_instance * const a_inst,
		       const t_upstyp_db * const a_db_info,
		       const t_upsugo_command * const a_command_line,
		       const FILE * const a_stream,
		       const t_upsact_cmd * const a_cmd)
{
  CHECK_NUM_PARAM("sourceOptCheck");

  /* only proceed if we have a valid number of parameters and a stream to write
     them to */
  if ((UPS_ERROR == UPS_SUCCESS) && a_stream) {
  
    switch ( a_command_line->ugo_shell ) {
    case e_BOURNE:
      if (fprintf((FILE *)a_stream, "if [ -s %s ]; then\n", a_cmd->argv[0])
	  < 0) {
	FPRINTF_ERROR();
      } else {
	if (fprintf((FILE *)a_stream,
		    "if [ ! -r %s -o ! -x %s]; then\n  echo File to be optionally sourced (%s) is not readable or not executable;\nelse\n",
		    a_cmd->argv[0], a_cmd->argv[0], a_cmd->argv[0]) < 0) {
	  FPRINTF_ERROR();
	} else {
	  /* define all of the UPS local variables that the user may need. */
	  upsget_allout(a_stream, a_db_info, a_inst, a_command_line);
	  if (UPS_ERROR == UPS_SUCCESS) {
	    g_LOCAL_VARS_DEF = 1;   /* we defined local variables */
	    if (fprintf((FILE *)a_stream,
			"  . %s;\n  if [ $? -eq 1 ]; then return 1; fi;\nfi;\nfi;\n#\n", 
			a_cmd->argv[0]) < 0) {
	      FPRINTF_ERROR();
	    }
	  } else {
	    FPRINTF_ERROR();
	  }
	}
      }
      break;
    case e_CSHELL:
      if (fprintf((FILE *)a_stream, "if (-e %s) then\n\n", a_cmd->argv[0])
	  < 0) {
	FPRINTF_ERROR();
      } else {
	if (fprintf((FILE *)a_stream,
		    "if (! -r %s || ! -x %s) then\n  echo File to be optionally sourced (%s) is not readable or not executable\nelse\n", 
		  a_cmd->argv[0], a_cmd->argv[0], a_cmd->argv[0]) < 0) {
	  FPRINTF_ERROR();
	} else {
	  /* define all of the UPS local variables that the user may need. */
	  upsget_allout(a_stream, a_db_info, a_inst, a_command_line);
	  if (UPS_ERROR == UPS_SUCCESS) {
	    g_LOCAL_VARS_DEF = 1;   /* we defined local variables */
	    if (fprintf((FILE *)a_stream,
			"  source %s\n  if ($status == 1) return 1/nendif\nendif\nendif\n#\n", 
			a_cmd->argv[0]) < 0) {
	      FPRINTF_ERROR();
	    }
	  } else {
	    FPRINTF_ERROR();
	  }
	}
      }
      break;
    default:
      upserr_vplace();
      upserr_add(UPS_INVALID_SHELL, UPS_FATAL, a_command_line->ugo_shell);
    }
    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

void f_dodefaults( const t_upstyp_matched_instance * const a_inst,
		   const t_upstyp_db * const a_db_info,
		   const t_upsugo_command * const a_command_line,
		   const FILE * const a_stream,
	   const t_upsact_cmd * const a_cmd)
{
  t_upsact_cmd lcl_cmd;
  static char buff[MAX_LINE_LEN];
  char *tmp_prod_dir = NULL;
  char *uprod_name;

  /* only proceed if we have a stream to write the output to */
  if (a_stream) {
    switch ( a_cmd->iact ) {
    case e_setup:	/* Define <PROD>_DIR and SETUP_<PROD> */
      /* use our local copy since we have to change it */
      lcl_cmd.iact = a_cmd->iact;
      lcl_cmd.argc = g_cmd_maps[e_envset].min_params;   /* # of args */
      lcl_cmd.icmd = e_envset;
      lcl_cmd.argv[0] = buff;

      /* since the prod_dir may come from the command line we need to check
	 if the user entered one that we have to use */
      if (a_command_line->ugo_productdir) {
	tmp_prod_dir = a_command_line->ugo_productdir;
      } else if (a_inst->version && a_inst->version->prod_dir) {
	tmp_prod_dir = a_inst->version->prod_dir;
      }
      if (a_inst->version && tmp_prod_dir && a_inst->version->product) {
	uprod_name = upsutl_upcase(a_inst->version->product);
	if (UPS_ERROR == UPS_SUCCESS) {
	  sprintf(buff, "%s_DIR", uprod_name);
	  
	  lcl_cmd.argv[1] = tmp_prod_dir;
	  f_envset(a_inst, a_db_info, a_command_line, a_stream, &lcl_cmd);
	}
      }
      upsget_envout(a_stream, a_db_info, a_inst, a_command_line);
      break;
    case e_chain:	/* None */
      break;
    case e_configure:	/* None */
      break;
    case e_copy:    	/* None */
      break;
    case e_create:	/* None */
      break;
    case e_current:     /* Copy man pages to man page area in dbconfig file */
      /* use our local copy since we have to change it */
      lcl_cmd.iact = a_cmd->iact;
      lcl_cmd.argc = g_cmd_maps[e_copyman].min_params;   /* # of args */
      f_copyman(a_inst, a_db_info, a_command_line, a_stream, a_cmd);
      break;
    case e_declare:	/* None */
      break;
    case e_depend:	/* None */
      break;
    case e_development:	/* None */
      break;
    case e_exist:	/* None */
      break;
    case e_get: 	/* None */
      break;
    case e_list:	/* None */
      break;
    case e_modify:	/* None */
      break;
    case e_new :	/* None */
      break;
    case e_old:   	/* None */
      break;
    case e_start:	/* None */
      break;
    case e_stop:	/* None */
      break;
    case e_tailor:	/* None */
      break;
    case e_test:	/* None */
      break;
    case e_unchain:	/* None */
      break;
    case e_unconfigure:	/* None */
      break;
    case e_uncurrent:   /* Remove the man pages from the man page area */
      /* use our local copy since we have to change it */
      lcl_cmd.iact = a_cmd->iact;
      lcl_cmd.argc = g_cmd_maps[e_uncopyman].min_params;   /* # of args */
      f_uncopyman(a_inst, a_db_info, a_command_line, a_stream, a_cmd);
      break;
    case e_undeclare:	/* None */
      break;
    case e_undevelopment:	/* None */
      break;
    case e_unk:	/* None */
      break;
    case e_unnew:	/* None */
      break;
    case e_unold:	/* None */
      break;
    case e_unsetup:
      /* use our local copy since we have to change it */
      lcl_cmd.iact = a_cmd->iact;
      lcl_cmd.argc = g_cmd_maps[e_envunset].min_params;   /* # of args */
      lcl_cmd.icmd = e_envunset;
      lcl_cmd.argv[0] = buff;
      if (a_inst->version && a_inst->version->product) {
	uprod_name = upsutl_upcase(a_inst->version->product);
	if (UPS_ERROR == UPS_SUCCESS) {
	  sprintf(buff, "%s_DIR", uprod_name);
	  f_envunset(a_inst, a_db_info, a_command_line, a_stream, &lcl_cmd);
	  sprintf(buff, "%s%s", SETUPENV, uprod_name);
	  f_envunset(a_inst, a_db_info, a_command_line, a_stream, &lcl_cmd);
	}
      }
      break;
    case e_untest:	/* None */
      break;
    case e_validate:	/* None */
      break;
    default:
      break;
    }

    if (UPS_ERROR != UPS_SUCCESS) {
      upserr_vplace();
      upserr_add(UPS_ACTION_WRITE_ERROR, UPS_FATAL,
		 g_cmd_maps[a_cmd->icmd].cmd);
    }
  }
}

