/************************************************************************
 *
 * FILE:
 *       upsfil.c
 * 
 * DESCRIPTION: 
 *       Will read an ups file, and fill corresponding data structures.
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
 *       23-Jun-1997, LR, first.
 *       31-Jul-1997, LR, Added use of upsmem.
 *       12-Aug-1997, LR, Added upsfil_write.
 *
 ***********************************************************************/

/* standard include files */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* ups specific include files */
#include "upsutl.h"
#include "upstyp.h"
#include "upslst.h"
#include "upsmem.h"
#include "upserr.h"
#include "upskey.h"
#include "upsfil.h"

/*
 * Definition of public variables
 */

/*
 * Declaration of private functions
 */
static int            read_file( void );
static int            read_file_type( void );
static int            read_file_desc( void );
static t_upstyp_instance *read_instance( void );
static t_upslst_item  *read_instances( void );
static t_upstyp_action   *read_action( void );
static t_upslst_item  *read_actions( void );
static t_upstyp_config   *read_config( void );
static t_upslst_item  *read_group( void );
static t_upslst_item  *read_groups( void );
static t_upslst_item  *read_comments( void );

static int            write_version_file( void );
static int            write_chain_file( void );
static int            write_table_file( void );
static int            write_action( t_upstyp_action * const act );
t_upslst_item         *find_group( t_upslst_item * const list_ptr, const char copt );

/* Line parsing */
static int            get_key( void );
static int            next_key( void );
static int            is_stop_key( void );
static int            is_start_key( void );
static int            put_key( const char * const key, const char * const val );

/* Utils */
static int            trim_qualifiers( char * const str );
static int            cfilei( void );
t_upslst_item         *copy_action_list( t_upslst_item * const list_ptr );

/* Print stuff */
static void           print_instance( t_upstyp_instance * const inst_ptr );
static void           print_action( t_upstyp_action * const act_ptr );
/* print_product has gone semi public */

/*
 * Definition of global variables
 */

/* enum of some extra keys */
enum {
  e_key_eol = -2,
  e_key_eof = -1
};

/* enum of known file types (changes here should be reflected in cfilei) */
enum e_ups_file {
  e_file_version = 0,
  e_file_table,
  e_file_chain,
  e_file_dbconfig,
  e_file_unknown,
  e_file_count
};

#define CHAR_REMOVE " \t\n\r\f\""

static t_upstyp_product  *g_pd = 0; /* current product to fill */
static FILE           *g_fh = 0; /* file handle */

static char           g_line[MAX_LINE_LEN] = "";  /* current line */
static char           g_key[MAX_LINE_LEN] = "";   /* current key */
static char           g_val[MAX_LINE_LEN] = "";   /* current value */
static t_upskey_map   *g_mkey = 0;                /* current map */
static int            g_ikey = e_key_unknown;     /* current key as enum */  
static int            g_ifile = e_file_unknown;   /* current file type as enum */

static int            g_imargin = 0;

/*
 * Definition of public functions
 */

/*-----------------------------------------------------------------------
 * upsfil_read_file
 *
 * Will read all instances in passed file
 *
 * Input : char *, path to a ups file
 * Output: none
 * Return: t_upstyp_product *, a pointer to a product
 */
t_upstyp_product *upsfil_read_file( const char * const ups_file )
{
  if ( !ups_file || strlen( ups_file ) <= 0 ) {
    upserr_vplace(); upserr_add( UPS_OPEN_FILE, UPS_FATAL, "" );
    return 0;
  }

  g_fh = fopen ( ups_file, "r" );
  if ( ! g_fh ) {
    upserr_vplace(); upserr_add( UPS_OPEN_FILE, UPS_FATAL, ups_file );
    return 0;
  }
  
  /* check if file is empty ??? */
    
  g_pd = ups_new_product();
  if ( !g_pd ) return 0;
       
  read_file();
  
  fclose( g_fh );  
  return g_pd;
}

/*-----------------------------------------------------------------------
 * upsfil_write_file
 *
 * Will write a product to a ups file
 *
 * Input : t_upstyp_product *, a pointer to a product
 *         char *, path to a ups file
 * Output: none
 * Return: int, UPS_SUCCESS just fine, else UPS_<error> number.
 */
int upsfil_write_file( t_upstyp_product * const prod_ptr,
		       const char * const ups_file )
{
  t_upslst_item *l_ptr = 0;
  
  if ( ! prod_ptr ) {
    upserr_vplace(); upserr_add( UPS_NOVALUE_ARGUMENT, UPS_FATAL, "0", "product pointer" );
    return UPS_NOVALUE_ARGUMENT;
  }
  g_pd = prod_ptr;

  if ( !ups_file || strlen( ups_file ) <= 0 ) {
    upserr_vplace(); upserr_add( UPS_OPEN_FILE, UPS_FATAL, "" );
    return UPS_OPEN_FILE;
  }
  
  g_fh = fopen ( ups_file, "w" );
  if ( ! g_fh ) {
    upserr_vplace(); upserr_add( UPS_OPEN_FILE, UPS_FATAL, ups_file );
    return UPS_OPEN_FILE;
  }    

  g_imargin = 0;
  
  /* write comments */

  l_ptr = upslst_first( g_pd->comment_list );
  for( ; l_ptr; l_ptr = l_ptr->next ) {
    fprintf( g_fh, "%s\n", (char *)l_ptr->data );
  }    

  /* write file type */
  
  if ( g_pd->file ) {
    put_key( "FILE", g_pd->file );
  }
  cfilei();

  if ( g_ifile == e_file_unknown ) {
    upserr_vplace();
    upserr_add( UPS_UNKNOWN_FILETYPE, UPS_WARNING, g_pd->file ? g_pd->file : "(null)" );
    return UPS_UNKNOWN_FILETYPE;
  }
    
  /* write instances */

  switch ( g_ifile ) {

  case e_file_version:
    write_version_file();
    break;
	
  case e_file_table:
    write_table_file();
    break;
	
  case e_file_chain:
    write_chain_file();
    break;
  }

  
  fclose( g_fh );

  return UPS_SUCCESS;
}
     
/*
 * Definition of private functions
 */

int write_version_file( void )
{
  t_upslst_item *l_ptr = 0;
  t_upstyp_instance *inst_ptr = 0;

  /* write file descriptor */
  
  put_key( "PRODUCT", g_pd->product );
  put_key( "VERSION", g_pd->version );
  put_key( "UPS_DB_VERSION", g_pd->ups_db_version );

  /* write instances */
  
  l_ptr = upslst_first( g_pd->instance_list );
  for( ; l_ptr; l_ptr = l_ptr->next ) {
    inst_ptr = (t_upstyp_instance *)l_ptr->data;
    if ( !inst_ptr || !inst_ptr->flavor ) {
      /* handle error !!! */
      return 0;
    }

    put_key( 0, "" );
    put_key( "FLAVOR", inst_ptr->flavor );    
    put_key( "QUALIFIERS", inst_ptr->qualifiers );
    
    g_imargin += 2;    
    put_key( "DECLARED", inst_ptr->declared );
    put_key( "DECLARER", inst_ptr->declarer );
    put_key( "PROD_DIR", inst_ptr->prod_dir );
    put_key( "UPS_DIR", inst_ptr->ups_dir );
    put_key( "TABLE_DIR", inst_ptr->table_dir );
    put_key( "TABLE_FILE", inst_ptr->table_file );
    put_key( "ARCHIVE_FILE", inst_ptr->archive_file );
    put_key( "AUTHORIZED_NODES", inst_ptr->authorized_nodes );
    if ( inst_ptr->statistics )
      put_key( 0, "STATISTICS" );    
    g_imargin -= 2;
  }  

  return 1;
} 

int write_chain_file( void )
{
  t_upslst_item *l_ptr = 0;
  t_upstyp_instance *inst_ptr = 0;

  /* write file descriptor */
  
  put_key( "PRODUCT", g_pd->product );
  put_key( "CHAIN", g_pd->chain );
  put_key( "UPS_DB_VERSION", g_pd->ups_db_version );

  /* write instances */
  
  l_ptr = upslst_first( g_pd->instance_list );
  for( ; l_ptr; l_ptr = l_ptr->next ) {
    inst_ptr = (t_upstyp_instance *)l_ptr->data;
    if ( !inst_ptr || !inst_ptr->flavor ) {
      /* handle error !!! */
      return 0;
    }


    put_key( 0, "" );
    put_key( "FLAVOR", inst_ptr->flavor );
    put_key( "QUALIFIERS", inst_ptr->qualifiers );
    
    g_imargin += 2;
    put_key( "VERSION", inst_ptr->version );
    put_key( "DECLARED", inst_ptr->declared );
    put_key( "DECLARER", inst_ptr->declarer );
    g_imargin -= 2;
  }  

  return 1;
} 

int write_table_file( void )
{
  t_upslst_item *l_inst = 0;
  t_upslst_item *l_act = 0;
  t_upstyp_instance *inst_ptr = 0;
  t_upstyp_action *act_ptr = 0;

  t_upslst_item *l_copy, *l_ptr;

  /* write file descriptor */
  
  put_key( "PRODUCT", g_pd->product );
  put_key( "VERSION", g_pd->version );
  put_key( "UPS_DB_VERSION", g_pd->ups_db_version );
  put_key( 0, "" );

  /* write groups */

  l_copy = upslst_copy( g_pd->instance_list );
  find_group( l_copy, 's' );
  while( (l_ptr = find_group( 0, ' ' )) ) {
    put_key( 0, "GROUP:");
    g_imargin += 2;
    for ( l_ptr = upslst_first( l_ptr ); l_ptr; l_ptr = l_ptr->next ) {
      inst_ptr = (t_upstyp_instance *)l_ptr->data;
      put_key( "FLAVOR", inst_ptr->flavor );
      put_key( "QUALIFIERS", inst_ptr->qualifiers );
      put_key( 0, "" );
      
    }
    g_imargin -= 2;
    put_key( 0, "COMMON:" );    
    g_imargin += 2;
    l_act = upslst_first( inst_ptr->action_list );
    for( ; l_act; l_act = l_act->next ) {
      act_ptr = (t_upstyp_action *)l_act->data;
      write_action( act_ptr );
    }
    g_imargin -= 2;      
    put_key( 0, "" );
    put_key( 0, "END:" );
    put_key( 0, "" );
  }
  l_copy = find_group( 0, 'e' );

  /* write instances */
  
  l_inst = upslst_first( l_copy );
  for( ; l_inst; l_inst = l_inst->next ) {
    inst_ptr = (t_upstyp_instance *)l_inst->data;
    if ( !inst_ptr || !inst_ptr->flavor ) {
      /* handle error !!! */
      return 0;
    }

    put_key( "FLAVOR", inst_ptr->flavor );
    put_key( "QUALIFIERS", inst_ptr->qualifiers );
    
    g_imargin += 2;
    put_key( "DESCRIPTION", inst_ptr->description );
    l_act = upslst_first( inst_ptr->action_list );
    for( ; l_act; l_act = l_act->next ) {
      act_ptr = (t_upstyp_action *)l_act->data;
      write_action( act_ptr );
    }
    g_imargin -= 2;
    put_key( 0, "" );    
  }  

  return 1;
} 

int write_action( t_upstyp_action * const act_ptr )
{
  t_upslst_item *l_com = 0;
  char *com;

  if ( !act_ptr ) return 0;

  put_key( "ACTION", act_ptr->action );
  
  g_imargin += 2;
  l_com = upslst_first( act_ptr->command_list );
  for( ; l_com; l_com = l_com->next ) {
    com = (char * )l_com->data;
    put_key( 0, com );
  }
  g_imargin -= 2;

  return 1;
}

/*-----------------------------------------------------------------------
 * read_file
 *
 * Will read all instances in an ups file
 *
 * Input : none
 * Output: none
 * Return: int, number of instances read
 */
int read_file( void )
{
  t_upslst_item *l_ptr = 0;
  
  /* read comments */

  g_pd->comment_list = read_comments();

  /* read file type */
  if ( !read_file_type() )
    return 0;

  /* if config file we are done quickly */
  
  if ( g_ifile == e_file_dbconfig ) {
    g_pd->config = read_config();
    return 1;
  }
  
  /* read file descriptor */

  if ( ! read_file_desc() )
    return 0;

  /* here, we expect only to see FLAVOR or GROUP: */

  while ( g_ikey != e_key_eof ) {
    l_ptr = 0;

    switch( g_ikey ) {

    case e_key_flavor:
      l_ptr = read_instances();
      break;
    
    case e_key_group:
      l_ptr = read_groups();
      break;

    default:
      next_key();
    }
    
    if ( l_ptr ) 
      g_pd->instance_list = upslst_add_list( g_pd->instance_list, l_ptr );
  }
	  
  return 1;
}

/*-----------------------------------------------------------------------
 * read_comments
 *
 * Will read a ups files comments (lines at the top of the file
 * starting with a '#').
 *
 * Input:  none
 * Output: none
 * Return: t_upslst_item *, pointer to a list of strings
 */
t_upslst_item *read_comments( void )
{
  t_upslst_item *l_ptr = 0;
  
  while ( fgets( g_line, MAX_LINE_LEN, g_fh ) ) {

    if ( !upsutl_str_remove_edges( g_line, CHAR_REMOVE ) ) continue;   
    if ( g_line[0] == '#' ) {
      l_ptr = upslst_add( l_ptr, upsutl_str_create( g_line, ' ' ) );
    }
    else {
      get_key();
      break;
    }
  }
  
  return l_ptr;
}
  
/*-----------------------------------------------------------------------
 * read_file_type
 *
 * Will read file descriptor: FILE
 *
 * Input:  none
 * Output: none
 * Return: int,  1 success, 0 error.
 */
int read_file_type( void )
{
  if ( g_ikey != e_key_file ) {
    upserr_vplace();
    upserr_add( UPS_UNKNOWN_FILETYPE, UPS_WARNING, "(null)" );    
    return 0;
  }  
  
  g_pd->file = upsutl_str_create( g_val, ' ' );
  
  /* translate file type to an enum */
  
  if ( cfilei() == e_file_unknown ) {
    upserr_vplace();
    upserr_add( UPS_UNKNOWN_FILETYPE, UPS_WARNING, g_pd->file );
  }  

  return 1;
}

/*-----------------------------------------------------------------------
 * read_file_desc
 *
 * Will read file descriptor: FILE, PRODUCT, VERSION/CHAIN.
 *
 * Input:  none
 * Output: none
 * Return: int,  1 if file descriptor was read, else 0
 */
int read_file_desc( void )
{
  if ( g_ikey != e_key_file ) {
    return 0;
  }  

  while ( next_key() != e_key_eof ) {

    if ( is_stop_key() ) break;
    
    if ( g_mkey && g_mkey->p_index != INVALID_INDEX ) 
      UPSKEY_PROD2ARR( g_pd )[g_mkey->p_index] = upsutl_str_create( g_val, ' ' );
    
  }

  return 1;
}

/*-----------------------------------------------------------------------
 * read_instances
 *
 * Will build a list of instances. Reading instances will continue
 * until next 'stop' key is not "FLAVOR".
 *
 * Input : none
 * Output: none
 * Return: t_upslst_item *, pointer to a list of instances
 */
t_upslst_item *read_instances( void )
{
    t_upslst_item *l_ptr = 0;
    t_upstyp_instance *inst_ptr = 0;
    
    while  ( g_ikey == e_key_flavor ) {
	inst_ptr = read_instance();
	
	if ( inst_ptr )
	  l_ptr = upslst_add( l_ptr, inst_ptr );
	else
	  break;
    }

    return upslst_first( l_ptr );
}
  
/*-----------------------------------------------------------------------
 * read_instance
 *
 * Will read a single instance
 *
 * Input : none
 * Output: none
 * Return: pointer to an instance
 */
t_upstyp_instance *read_instance( void )
{
  t_upstyp_instance *inst_ptr = 0;
  
  if ( g_ikey != e_key_flavor )
    return 0;    

  inst_ptr = ups_new_instance();

  /* fill information from file descriptor */
  
  inst_ptr->product = upsutl_str_create( g_pd->product, ' ' );
  if ( g_ifile == e_file_chain )
    inst_ptr->chain = upsutl_str_create( g_pd->chain, ' ' );
  else
    inst_ptr->version = upsutl_str_create( g_pd->version, ' ' );
    
  /* fill information from file */
  
  inst_ptr->flavor = upsutl_str_create( g_val, ' ' );
    
  while ( next_key() != e_key_eof ) {

    /* the world are full of special cases */

    if ( g_ikey == e_key_action )
      inst_ptr->action_list = read_actions();
      
    if ( is_stop_key() ) break;
    
    switch( g_ikey ) {

    case e_key_qualifiers:
      trim_qualifiers( g_val );
      inst_ptr->qualifiers = upsutl_str_create( g_val, ' ' );
      break;
      
    case e_key_unknown:
      if ( g_line[0] == '_' ) {	
	inst_ptr->unknown_list = upslst_add( inst_ptr->unknown_list,
					     upsutl_str_create( g_line, ' ' ) );
      }
      break;
      
    default:
      if ( g_mkey && g_mkey->i_index != INVALID_INDEX ) 
	UPSKEY_INST2ARR( inst_ptr )[g_mkey->i_index] = upsutl_str_create( g_val, ' ' );
      break;    

    }

  }

  return inst_ptr;
}

/*-----------------------------------------------------------------------
 * read_actions
 *
 * Will build a list of actions. Reading actions will continue
 * until next 'stop' key is not "ACTION".
 *
 * Input : none
 * Output: none
 * Return: pointer to a list of actions
 */
t_upslst_item *read_actions( void )
{
    t_upslst_item *l_ptr = 0;
    t_upstyp_action *act_ptr = 0;
    
    while  ( g_ikey == e_key_action ) {
	act_ptr = read_action();
	
	if ( act_ptr ) {
	  l_ptr = upslst_add( l_ptr, act_ptr );
	}
	else {
	  break;
	}
    }

    return upslst_first( l_ptr );
}

/*-----------------------------------------------------------------------
 * read_action
 *
 * Will read a single action
 *
 * Input : none
 * Output: none
 * Return: pointer to an action
 */
t_upstyp_action *read_action( void )
{
  t_upstyp_action *act_ptr = 0;
  t_upslst_item *l_cmd = 0;
  char *cmd_ptr = 0;

  if ( g_ikey != e_key_action ) 
    return 0;

  act_ptr = ups_new_action();
  act_ptr->action = upsutl_str_create( g_val, ' ' );

  while ( next_key() != e_key_eof ) {

    if ( is_stop_key() ) break;
    
    if ( g_ikey == e_key_unknown && strlen( g_line ) > 0 ) {
      cmd_ptr = upsutl_str_create( g_line, ' ' );
      if ( cmd_ptr )
	l_cmd = upslst_add( l_cmd, cmd_ptr );
    }
    
  }

  act_ptr->command_list = upslst_first( l_cmd );
  
  return act_ptr;
}

/*-----------------------------------------------------------------------
 * read_config
 *
 * Will read ups config file.
 *
 * Input : char *, path to a ups config file
 * Output: none
 * Return: t_upstyp_config *, a pointer to the config structure.
 */
t_upstyp_config *read_config( void )
{
  t_upstyp_config *conf_ptr = ups_new_config();

  while ( next_key() != e_key_eof ) {

    if ( g_ikey == e_key_statistics ) {
      upsutl_str_remove( g_val, CHAR_REMOVE );  
      upsutl_str_sort( g_val, ',' );
    }
      
    if ( g_mkey && g_mkey->c_index != INVALID_INDEX ) 
      UPSKEY_CONF2ARR( conf_ptr )[g_mkey->c_index] = upsutl_str_create( g_val, ' ' );
  }

  fclose( g_fh );  
  return conf_ptr;
}

/*-----------------------------------------------------------------------
 * read_groups()
 *
 * Will read a list of groups. Reading groups will
 * continue until 'stop' key is not "GROUP:" 
 *
 * Input : none.
 * Output: none.
 * Return: t_upslst_item *, pointer to a list of instances
 */
t_upslst_item *read_groups( void )
{    
    t_upslst_item *l_ptr = 0;
    t_upslst_item *l_tmp_ptr = 0;
    
    while  ( g_ikey == e_key_group ) {
	l_tmp_ptr = read_group();

	if ( l_tmp_ptr ) {
	  l_ptr = upslst_add_list( l_ptr, l_tmp_ptr );
	}
	else {
	  break;
	}
	
    }

    return upslst_first( l_ptr );
}

/*-----------------------------------------------------------------------
 * read_group
 *
 * Will read a single group.
 *
 * Input : none.
 * Output: none.
 * Return: t_upslst_item *, pointer to a list of instances
 */
t_upslst_item *read_group( void )
{
    t_upslst_item *l_ptr = 0;
    t_upslst_item *l_inst_ptr = 0;
    t_upslst_item *l_act_ptr = 0;
    t_upstyp_instance *inst_ptr = 0;

    if ( g_ikey != e_key_group ) 
      return 0;
    
    if ( next_key() != e_key_flavor )
      return 0;

    l_inst_ptr = read_instances();

    if ( !l_inst_ptr )
      return 0;

    if ( g_ikey == e_key_common ) {
	next_key();
	l_act_ptr = read_actions();
    }

    /* add actions to instances */

    if ( l_act_ptr ) {
      l_ptr = upslst_first( l_inst_ptr );
      inst_ptr = (t_upstyp_instance *)l_ptr->data;
      inst_ptr->action_list = l_act_ptr;
      l_ptr = l_ptr->next;

      /* make a reference for all other instances,  */
      /* not to the list but to list data elements. */
      /* can not call upslist_copy directly, since  */
      /* the command list would then be shared.     */
      
      for ( ; l_ptr; l_ptr = l_ptr->next ) {
	inst_ptr = (t_upstyp_instance *)l_ptr->data;	
	inst_ptr->action_list = copy_action_list( l_act_ptr );
      }
    }

    while ( next_key() == e_key_end ) {}
    
    return l_inst_ptr;
}

/*
 * Line parsing
 */

/*-----------------------------------------------------------------------
 * next_key
 *
 * Will read next not empty line from file.
 *
 * Input : none
 * Output: none
 * Return: int, enum of current key.
 */
int next_key( void )
{  
  g_key[0] = 0;
  g_val[0] = 0;
  g_line[0] = 0;
  g_ikey = e_key_eof;

  while ( fgets( g_line, MAX_LINE_LEN, g_fh ) ) {

    if ( strlen( g_line ) < 1 ) continue;
    if ( g_line[0] == '#' ) continue;
    if ( !upsutl_str_remove_edges( g_line, CHAR_REMOVE ) ) continue;

    if ( get_key() != e_key_eol ) {
      return g_ikey;
    }
  }

  return e_key_eof;
}

/*-----------------------------------------------------------------------
 * get_key
 *
 * Will parse the current line (g_line) and fill current key (g_key/g_ikey)
 * and current value (g_val)
 *
 * Input : none
 * Output: none
 * Return: char*, pointer to current key;
 */
int get_key( void )
{
  char *cp = g_line;
  int count = 0;

  /* check if line is not empty (again) */
  
  if ( strlen( g_line ) < 1 ) return e_key_eol;
  if ( g_line[0] == '#' ) return e_key_eol;
    
  /* check if line has a key/value pair */
  
  if ( !strchr( g_line, '=' ) ) {
    strcpy( g_key, g_line );
  }

  /* split line into key/value pair */

  else {
    count = 0;
    while ( cp && *cp && !isspace( *cp ) && *cp != '=' ) {
      g_key[count] = *cp;
      count++; cp++;
    }
    g_key[count] = 0;
    if ( strlen( g_key ) <= 0 ) return e_key_eol;
  
    while( cp && *cp && *cp != '=' ) { cp++; }
    cp++;
    while( cp && *cp && (isspace( *cp ) || *cp == '"') ) { cp++; }
    count = 0;
    while( cp && *cp && *cp != '\n' ) {
      g_val[count] = *cp;
      count++; cp++;
    }
    g_val[count] = 0;
  }

  g_mkey = upskey_get_map( g_key );
  if ( g_mkey )
    g_ikey = g_mkey->ikey;
  else
    g_ikey = e_key_unknown;
  
  return g_ikey;
}

/*-----------------------------------------------------------------------
 * put_key
 *
 * Will print and format passed key and val.
 * It's using current value of margin (g_imargin) to indent text.
 * It will not print anything if val is empty.
 *
 * Input : char *, key string
 *         char *, value string 
 * Output: none
 * Return: int, 1 fine, 0 not fine
 */
int put_key( const char * const key, const char * const val )
{
  int i = 0;

  if ( !val && upsutl_stricmp( "QUALIFIERS", key ) )
    return 0;

  for ( i=0; i<g_imargin; i++ )
    fputc( ' ', g_fh );
  if ( key && strlen( key ) > 0 ) {
    fprintf( g_fh, "%s = ", key );
    if ( !upsutl_stricmp( "QUALIFIERS", key ) ||
	 !upsutl_stricmp( "DESCRIPTION", key ) )
      fputc( '\"', g_fh );
    fprintf( g_fh, "%s", val );
    if ( !upsutl_stricmp( "QUALIFIERS", key ) ||
	 !upsutl_stricmp( "DESCRIPTION", key ) )
      fputc( '\"', g_fh );      
  }
  else {
    fprintf( g_fh, "%s", val );
  }

  fputc( '\n', g_fh );
  
  return 1;
}
     
int is_start_key( void )
{
  if ( g_ikey == e_key_flavor ) 
    return 1;
  if ( g_ikey == e_key_group ) 
    return 1;
  if ( g_ikey == e_key_common ) 
    return 1;
  if ( g_ikey == e_key_file )
    return 1;
  if ( g_ikey == e_key_action ) 
    return 1;

  return 0;
}

int is_stop_key( void )
{
  if ( is_start_key() )
    return 1;
  if ( g_ikey == e_key_end ) 
    return 1;

  return 0;
}

/*
 * Utils
 */

int cfilei( void )
{
  /* strings of known file types */
  static char *s_ups_files[e_file_count] = {
    "VERSION",
    "TABLE",
    "CHAIN",
    "DBCONFIG",
    ""
  };

  int i;
  g_ifile = e_file_unknown;

  if ( !g_pd->file ) return g_ifile;

  /* for now, just a linear search */

  for( i=0; i<e_file_count; i++ ) {    
    if ( !upsutl_stricmp( g_pd->file, s_ups_files[i] ) ) {
      g_ifile = i;
      break;
    }
  }

  return g_ifile;
}

int trim_qualifiers( char * const str )
{
  int i, len;
  
  if ( !str || strlen( str ) <= 0 ) return 0;

  len = (int)strlen( str );
  for ( i=0; i<len; i++ )
    str[i] = (char)tolower( (int)str[i] );
  
  upsutl_str_remove( str, CHAR_REMOVE );  
  upsutl_str_sort( str, ',' );

  return (int)strlen( str );
}

t_upslst_item *copy_action_list( t_upslst_item * const list_ptr )
{
  t_upslst_item *l_ptr1 = upslst_first( list_ptr );
  t_upslst_item *l_ptr2 = 0;

  if( !list_ptr )
    return 0;

  for ( ; l_ptr1; l_ptr1 = l_ptr1->next ) {
    t_upstyp_action *a_ptr1 = (t_upstyp_action *)l_ptr1->data;
    t_upstyp_action *a_ptr2 = ups_new_action();
    
    upsmem_inc_refctr( a_ptr1->action );
    a_ptr2->action = a_ptr1->action;
    a_ptr2->command_list = upslst_copy( a_ptr1->command_list );
    l_ptr2 = upslst_add( l_ptr2, a_ptr2 );
  }

  return upslst_first( l_ptr2 );
}

/*
 * Print stuff
 */

void print_instance( t_upstyp_instance * const inst_ptr )
{
  t_upslst_item *l_ptr = 0;
  
  if ( !inst_ptr ) return;
  
  printf( "\nproduct = %s\nversion = %s\nflavor = %s\nqualifiers = %s\n",
	  inst_ptr->product, inst_ptr->version, inst_ptr->flavor,
	  inst_ptr->qualifiers );
  
  printf( "chain = %s\ndeclarer = %s\ndeclared = %s\n",
	  inst_ptr->chain, inst_ptr->declarer, inst_ptr->declared );
  
  printf( "prod_dir = %s\nups_dir = %s\n",
	  inst_ptr->prod_dir, inst_ptr->ups_dir );
  
  printf( "table_dir = %s\ntable_file = %s\narchive_file = %s\nauthorized_nodes = %s\n",
	  inst_ptr->table_dir, inst_ptr->table_file, inst_ptr->archive_file,
	  inst_ptr->authorized_nodes );
  
  printf( "description = %s\n",
	  inst_ptr->description );

  if ( inst_ptr->action_list ) {
    printf( "Actions = \n" );
    l_ptr = upslst_first( inst_ptr->action_list );
    for ( ; l_ptr; l_ptr = l_ptr->next ) {
      print_action( l_ptr->data );
    }
  }
  else {
    printf( "Actions = %s\n", (char*)0 );
  }
  
  if ( inst_ptr->unknown_list ) {
    printf( "User Defined = \n" );
    l_ptr = upslst_first( inst_ptr->unknown_list );
    for ( ; l_ptr; l_ptr = l_ptr->next ) {
      printf( "%s\n", (char *) l_ptr->data );
    }
  }
  else {
    printf( "User Defined = %s\n", (char*)0 );
  }
}

void print_action( t_upstyp_action * const act_ptr )
{
  t_upslst_item *l_ptr = 0;
  
  if ( !act_ptr ) return;

  printf ( "action = %s\n", act_ptr->action );
  
  if ( act_ptr->command_list ) {
    printf( "Command list = \n" );
    l_ptr = upslst_first( act_ptr->command_list );
    for ( ; l_ptr; l_ptr = l_ptr->next ) {
      printf( "   %s\n", (char*)l_ptr->data );
    }
  }
  else {
    printf( "Command list = %s\n", (char*)0 );
  }
}

void g_print_product( t_upstyp_product * const prod_ptr )
{
  t_upslst_item *l_ptr = 0;

  l_ptr = upslst_first( prod_ptr->comment_list );
  for ( ; l_ptr; l_ptr = l_ptr->next ) {
    printf( "%s\n", (char *)l_ptr->data ); 
  }
  
  printf( "\nfile    = %s\n", prod_ptr->file );
  printf( "product = %s\n", prod_ptr->product );
  printf( "version  = %s\n", prod_ptr->version );
  printf( "chain  = %s\n", prod_ptr->chain );
  printf( "instance_list:\n" );
  
  l_ptr = upslst_first( prod_ptr->instance_list );
  for ( ; l_ptr; l_ptr = l_ptr->next ) {
    t_upstyp_instance *inst_ptr = (t_upstyp_instance *)l_ptr->data;
    print_instance( inst_ptr );
  }

  if ( prod_ptr->config )
    upskey_conf_print( prod_ptr->config ); 
}

int action_cmp ( const void * const d1, const void * const d2 )
{
  t_upstyp_action *a1 = (t_upstyp_action *)d1;
  t_upstyp_action *a2 = (t_upstyp_action *)d2;

  return upsutl_stricmp( a1->action, a2->action );
}

int cmp_actions( t_upslst_item *l_ptr1, t_upslst_item *l_ptr2 )
{
  l_ptr1 = upslst_first( l_ptr1 );
  l_ptr2 = upslst_first( l_ptr2 );
  
  if ( l_ptr1 == l_ptr2 )
    return 0;
  
  if ( l_ptr1 == 0 || l_ptr2 == 0 )
    return 1;

  if ( upslst_count( l_ptr1 ) != upslst_count( l_ptr2 ) )
    return 1;

  /* sort actions, so it's easier to compare */
  
  l_ptr1 = upslst_sort0( l_ptr1, action_cmp );
  l_ptr2 = upslst_sort0( l_ptr2, action_cmp );

  for ( ; l_ptr1 && l_ptr2; l_ptr1 = l_ptr1->next, l_ptr2 = l_ptr2->next ) {
    t_upstyp_action *a1 = (t_upstyp_action *)l_ptr1->data;
    t_upstyp_action *a2 = (t_upstyp_action *)l_ptr2->data;
    t_upslst_item *c1, *c2;

    /* same action name ? */
    
    if ( upsutl_stricmp( a1->action, a2->action ) )
      return 1;

    /* compare list of commands */
    
    c1 = upslst_first( a1->command_list );
    c2 = upslst_first( a2->command_list );
    
    if ( c1 == c2 )
      continue;
  
    if ( c1 == 0 || c2 == 0 )      
      return 1;

    if ( upslst_count( c1 ) != upslst_count( c2 ) )
      return 1;

    for ( ; c1 && c2; c1 = c1->next, c2 = c2->next ) {
      if ( strcmp( (char *)c1->data, (char *)c2->data ) )
	return 1;    
    }
  }

  /* if we came so far, they match */
  
  return 0;
}

t_upslst_item *find_group( t_upslst_item * const list_ptr, const char copt )
{
  t_upslst_item *l_grp = 0;
  t_upslst_item *l_itm = 0;
  t_upstyp_instance *inst = 0;
  static t_upslst_item* l_orig = 0;

  switch ( copt ) {
    
  case 's':
    l_orig = list_ptr;
    return 0;
    
  case 'e':
    l_itm = upslst_first( l_orig );
    l_orig = 0;
    return l_itm;
  }
  
  if ( !l_orig ) 
    return 0;

  /* find a group */

  l_itm = l_orig;
  while ( !l_grp && l_itm && l_itm->next ) {
    inst = (t_upstyp_instance *)l_itm->data;
    l_itm = l_itm->next;
    
    while ( l_itm ) {
      t_upstyp_instance *is = (t_upstyp_instance *)l_itm->data;
      if ( !cmp_actions( inst->action_list, is->action_list ) ) {
	l_grp = upslst_add( l_grp, is );
	l_itm = upslst_delete_safe( l_itm, is, ' ' );
      }
      else {
	l_itm = l_itm->next;
      }
    }
  }

  /* if any group, insert first instance in group */
  /* and remove it from original list.            */
  
  if ( l_grp ) {
    l_grp = upslst_insert( l_grp, inst );
    l_orig = upslst_delete( l_orig, inst, ' ' );
  }

  return l_grp;    
}
