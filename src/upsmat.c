/************************************************************************
 *
 * FILE:
 *       upsmat.c
 * 
 * DESCRIPTION: 
 *        Compare the instance(s) requested on the command line with those
 *        contined in the product UPS files and return a list of matches.
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
 *       25-Jul-1997, EB, first
 *
 ***********************************************************************/

/* standard include files */
#include <string.h>
#include <stdio.h>

/* ups specific include files */
#include "upsmat.h"
#include "upserr.h"
#include "upslst.h"
#include "upstyp.h"
#include "upsmem.h"
#include "upsfil.h"
#include "upsutl.h"
#include "upsget.h"

/*
 * Definition of public variables.
 */
extern int UPS_VERBOSE;

/*
 * Declaration of private functions.
 */
static t_upstyp_matched_product *match_instance_core(
			   const t_upsugo_command * const a_command_line,
			   const t_upstyp_db * const a_db_info,
			   const char * const a_prod_name,
			   const t_upslst_item * const a_chain_list,
			   const t_upslst_item * const a_version,
			   const int a_need_unique,
			   const int a_any_version,
			   const int a_ant_chain);
static int match_from_chain( const char * const a_product,
			     const char * const a_chain,
			     const char * const a_version,
			     const char * const a_upsdir,
			     const char * const a_productdir,
			     const t_upstyp_db * const a_db_info,
			     const int a_need_unique,
			     const t_upslst_item * const a_flavor_list,
			     const t_upslst_item * const a_quals_list,
			     const int a_any_version,
			     t_upslst_item ** const a_minst_list);
static int match_from_version( const char * const a_product,
			       const char * const a_version,
			       const char * const a_upsdir,
			       const char * const a_productdir,
			       const t_upstyp_db * const a_db_info,
			       const int a_need_unique,
			       const t_upslst_item * const a_flavor_list,
			       const t_upslst_item * const a_quals_list,
			       t_upslst_item ** const a_minst_list);
static int match_from_table( const char * const a_product,
			     const char * const a_tablefile,
			     const char * const a_tablefiledir,
			     const char * const a_upsdir,
			     const char * const a_productdir,
			     const t_upstyp_db * const a_db_info,
			     const int a_need_unique,
			     const t_upslst_item * const a_flavor_list,
			     const t_upslst_item * const a_quals_list,
			     t_upslst_item ** const a_minst_list);
static char *get_table_file_path( const char * const a_prodname,
				  const char * const a_tablefile,
				  const char * const a_tablefiledir,
				  const char * const a_upsdir,
				  const char * const a_productdir,
				  const t_upstyp_db * const a_db_info);
static int get_instance(const t_upslst_item * const a_read_instances,
			const t_upslst_item * const a_flavor_list,
			const t_upslst_item * const a_quals_list,
			const int a_need_unique,
			const int a_file_type,
			t_upslst_item ** const a_minst_list);

/*
 * Definition of global variables.
 */
#ifndef NULL
#define NULL
#endif

/* sometimes when matching for > 1 instance, and error is found with a
   product being matched.  the match routines record that error, stop 
   matching on that product and go to the next product.  but we need to keep
   track of the error that occurred so that when we return to the calling
   routine, we can correctly report that an error occurred. and return all
   the matches that we were able to make. */
static int g_ups_error;

#define VPREFIX  "UPSMAT: "

#define TMP_LISTS_FREE() \
      if (tmp_flavor_list) {                                     \
	/* set to initial value first */                         \
	tmp_flavor_list->data = (void *)first_flavor;            \
	tmp_quals_list->data = (void *)first_quals;              \
	tmp_flavor_list = upslst_free(tmp_flavor_list, ' ');     \
	tmp_quals_list = upslst_free(tmp_quals_list, ' ');       \
      }
	
#define TMP_LISTS_SET()	\
     if (tmp_flavor_list == NULL) {                                        \
       tmp_flavor_list = upslst_insert(tmp_flavor_list, inst->flavor);     \
       tmp_quals_list = upslst_insert(tmp_quals_list, inst->qualifiers);   \
       /* save these first values as when we delete the list we will have  \
          to put these back on first so that the reference counter can     \
          be decremented properly. */                                      \
       first_flavor = inst->flavor;                                        \
       first_quals = inst->qualifiers;                                     \
     } else {                                                              \
       /* we already have a list just change the value pointed to */       \
       tmp_flavor_list->data = (void *)(inst->flavor);                     \
       tmp_quals_list->data = (void *)(inst->qualifiers);                  \
     }

#define USE_CMD_LINE_INFO() \
      if (a_upsdir) {                                \
	tmp_upsdir = (char *)a_upsdir;               \
      } else {                                       \
	tmp_upsdir = inst->ups_dir;                  \
      }                                              \
      if (a_productdir) {                            \
	tmp_productdir = (char *)a_productdir;       \
      } else {                                       \
	tmp_productdir = inst->prod_dir;             \
      }

#define CHECK_UNIQUE(a_list, type) \
      if (a_list) {                                                    \
        a_list = upslst_first(a_list);                                 \
        if (a_need_unique) {                                           \
	  /* we need a unique instance, make sure we only have one */  \
  	  if (a_list->next) {                                          \
	    /* we have more than one, this is an error */              \
            upserr_vplace();                                           \
	    upserr_add(UPS_NEED_UNIQUE, UPS_FATAL, type);              \
	    break;                                                     \
	  }                                                            \
	}                                                              \
      }

#define CHECK_NO_FILE(file) \
      if ((UPS_ERROR == UPS_NO_FILE) && ! a_need_unique) {      \
	upserr_clear();						\
      } 

#define CHECK_SOFT_ERR() \
      if (UPS_ERROR != UPS_SUCCESS) {                                   \
         if ((UPS_ERROR == UPS_NO_FILE ||                               \
              UPS_ERROR == UPS_NO_TABLE_MATCH ||                        \
              UPS_ERROR == UPS_NO_VERSION_MATCH) && ! a_need_unique) {  \
	   g_ups_error = UPS_ERROR;                                     \
	   upserr_clear();						\
	 }                                                              \
         else {								\
	   break;                                                       \
         }								\
      }

#define GET_NEW_MINST(inst_type) \
      minst = ups_new_matched_instance();                          \
      minst->inst_type = instance;                                 \
      upsmem_inc_refctr((void *)instance);                         \
      *a_minst_list = upslst_add(*a_minst_list, (void *)minst);


#define ADD_TO_MINST_LIST(inst_type) \
      if (*a_minst_list) {                                             \
	t_upslst_item *tmp_list = *a_minst_list;                       \
	t_upstyp_matched_instance *tmp_minst = NULL;                   \
	while (tmp_list) {                                             \
	  tmp_minst = (t_upstyp_matched_instance *)(tmp_list->data);   \
	  if (tmp_minst->inst_type) {                                  \
	    *a_minst_list = tmp_list;                                  \
   	    tmp_list = tmp_list->next;                                 \
	  } else {                                                     \
	    tmp_minst->inst_type = instance;                           \
	    *a_minst_list = tmp_list;                                  \
	    upsmem_inc_refctr((void *)instance);                       \
	    break;                                                     \
	  }                                                            \
	}                                                              \
	if (!tmp_list) {                                               \
	  GET_NEW_MINST(inst_type);                                    \
	}                                                              \
      } else {                                                         \
	  GET_NEW_MINST(inst_type);                                    \
      }

#define ADD_TO_MPRODUCT_LIST() \
      if (mproduct) {                                                  \
	mproduct_list = upslst_add(mproduct_list, (void *)mproduct);   \
      }

#define MATCH_TABLE_ONLY(product) \
    mproduct = match_instance_core(a_command_line, db_info,            \
				   product, NULL, NULL, a_need_unique, \
                                   any_version, any_chain);            \
    if (UPS_ERROR != UPS_SUCCESS) {                                    \
      if (UPS_ERROR == UPS_NO_FILE && ! a_need_unique) {               \
	upserr_clear();                                                \
      }                                                                \
    }                                                                  \
    /* update the mproduct_list structure with the new info */         \
    ADD_TO_MPRODUCT_LIST();

#define GET_CONFIG_FILE() \
    if (db_info && !db_info->config) {               \
      config_ptr = upsutl_get_config(db_info->name); \
      if (config_ptr) {                              \
	db_info->config = config_ptr->config;        \
	upsmem_inc_refctr(db_info->config);          \
      }                                              \
    }


#define FREE_CONFIG_FILE() \
    if (db_info && db_info->config) {               \
      db_info->config = upsutl_free_config(db_info->config);  \
    }

#define FREE_INVALID_INSTANCES() \
    if ((tmp_minst_list = master_minst_list)) {                              \
      int did_delete;                                                        \
      do {                                                                   \
	did_delete = 0;                                                      \
	minst = (t_upstyp_matched_instance *)tmp_minst_list->data;           \
	if (minst->invalid_instance) {                                       \
	  if (! tmp_minst_list->prev) {                                      \
	    /* this is the first element on the list, master_minst_list will \
	       need to be reset as it is now pointing to a deleted entry */  \
	    master_minst_list = tmp_minst_list->next;                        \
	  }                                                                  \
	  tmp_minst_list = upslst_delete_safe(tmp_minst_list, minst, ' ');   \
	  minst = ups_free_matched_instance(minst);                          \
	  did_delete = 1;                                                    \
	}                                                                    \
      } while (tmp_minst_list && tmp_minst_list->next &&                     \
	       (!did_delete && (tmp_minst_list = tmp_minst_list->next)));    \
    }

/*
 * Definition of public functions.
 */

/*-----------------------------------------------------------------------
 * upsmat_instance
 *
 * Given the input from the command line, determine which products, versions
 * and chains were asked for.  Then call the matching routine to fetch them.
 * All products will be matched in the first database and then the second etc.
 *
 * Input : command line information,
 *         flag indicating if we only want one instance,
 * Output: None
 * Return: a list of matched products
 */
t_upslst_item *upsmat_instance(t_upsugo_command * const a_command_line,
			       const t_upslst_item * const a_db_info,
			       const int a_need_unique)
{
  t_upstyp_db *db_info = NULL;
  t_upslst_item *db_list;
  t_upslst_item *db_item, *all_products = NULL, *product_item;
  t_upslst_item *all_versions = NULL;
  t_upslst_item *chain_item, *all_chains = NULL;
  t_upstyp_matched_product *mproduct = NULL;
  t_upslst_item *mproduct_list = NULL;
  t_upstyp_product *config_ptr = NULL;
  char *prod_name, *the_chain, *new_string = NULL, *location = NULL;
  char do_delete = 'd';
  int got_all_products = 0, got_all_versions = 0, got_all_chains = 0;
  int need_all_chains = 0;
  int any_version = 0, any_chain = 0;
  void *saved_next = NULL;

  /* initialize this to success */
  g_ups_error = UPS_SUCCESS;

  /* In order to avoid doing this for each database, if a product name was
     entered, create a list (with 1 element) here */
  if (strcmp(a_command_line->ugo_product, ANY_MATCH)) {
    /* we only have one to list out */
    if ((new_string = upsutl_str_create(a_command_line->ugo_product, ' '))) {
      all_products = upslst_add(all_products, new_string);
      got_all_products = 1;
    }
  }
  
  /* In order to avoid doing this for each database, if chains were
     entered, create a list here (can only do if '*' was not one of the
     requested chains. */
  if (a_command_line->ugo_chain) {
    /* first make sure none of the requested chains were '*' */
    for (chain_item = a_command_line->ugo_chain ; chain_item ;
	 chain_item = chain_item->next) {
      the_chain = (char *)(chain_item->data);
      if (! strcmp(the_chain, ANY_MATCH)) {
	need_all_chains = 1;
	break;
      }
    }
    if (! need_all_chains) {
      /* none of the chains were '*' so they will remain the same for
	 all products.  just point to the list we have. */
      all_chains = a_command_line->ugo_chain;
      got_all_chains = 1;
    }
  }

  /* In order to avoid doing this for each database, if a version was
     entered, create a list (with 1 element) here */
  if (a_command_line->ugo_version &&
      strcmp(a_command_line->ugo_version, ANY_MATCH)) {
    /* we only have one to list out */
    if ((new_string = upsutl_str_create(a_command_line->ugo_version,  ' '))) {
      all_versions = upslst_add(all_versions, new_string);
      got_all_versions = 1;
    }
  }

  /* figure out where we are getting our database from */
  if (a_db_info) {
    /* the user passed a particular one */
    db_list = (t_upslst_item *)a_db_info;
    saved_next = db_list->next;
    db_list->next = NULL;                     /* we only want this one */
  } else {
    db_list = a_command_line->ugo_db;
  }

  if (db_list) {
    if (a_command_line->ugo_m && a_command_line->ugo_product) {
     /* We have a table file  and we have a product name, we do not need the
        db. */
      if (UPS_VERBOSE) {
	printf("%sUsing Table File - %s\n", VPREFIX,
	       (char *)a_command_line->ugo_tablefile);
      }
      for (db_item = db_list ; db_item ; db_item = db_item->next) {
	db_info = (t_upstyp_db *)db_item->data;
	if (UPS_VERBOSE) {
	  printf("%sSearching UPS database - %s\n", VPREFIX, db_info->name);
	}
	/* If the user did not enter a product name, get all the product names
	   in the current db. */
	if (! got_all_products) {
	  upsutl_get_files(db_info->name, (char *)ANY_MATCH, &all_products);
	}

	if (all_products) {
	  /* make sure if we need a unique instance that we only have one */
	  CHECK_UNIQUE(all_products, "products");

	  /* read in the config file associated with this database and
	     save it */
	  GET_CONFIG_FILE();

	  /* for each product, get all the requested instances */
	  for (product_item = all_products ; product_item ;
	       product_item = product_item->next) {
	    prod_name = (char *)product_item->data;
	  
	    if (UPS_VERBOSE) {
	      printf("%sLooking for Product = %s\n", VPREFIX, prod_name);
	    }
	    MATCH_TABLE_ONLY(prod_name);
	  }
	  /* may no longer need product list - free it */
	  if (! got_all_products) {
	    all_products = upslst_free(all_products, do_delete);
	  }
	}
	/* if we have a match and are asking for a unique one, we do not need
	   to go to the next db */
	if (a_need_unique && mproduct) {
	  break;
	}
	/* free the db configuration info, it we previously read it in */
	FREE_CONFIG_FILE();
      }
    } else {
      /* we have at least one db */
      for (db_item = db_list ; db_item ; db_item = db_item->next) {
	db_info = (t_upstyp_db *)db_item->data;
	if (UPS_VERBOSE) {
	  printf("%sSearching UPS database - %s\n", VPREFIX, db_info->name);
	}
	/* If the user did not enter a product name, get all the product names
	   in the current db. */
	if (! got_all_products) {
	  upsutl_get_files(db_info->name, (char *)ANY_MATCH, &all_products);
	}

	if (all_products) {
	  /* make sure if we need a unique instance that we only have one */
	  CHECK_UNIQUE(all_products, "products");

	  /* read in the config file associated with this database and
	     save it */
	  GET_CONFIG_FILE();

	  /* for each product, get all the requested instances */
	  for (product_item = all_products ; product_item ;
	       product_item = product_item->next) {
	    prod_name = (char *)product_item->data;
	    location = upsutl_get_prod_dir(db_info->name, prod_name);
	  
	    if (UPS_VERBOSE) {
	      printf("%sLooking for Product = %s\n", VPREFIX, prod_name);
	    }
	    /* Check if chains were requested */
	    if (a_command_line->ugo_chain) {
	      /* we may have already made a list of them above. check before
		 doing anything here */
	      if (! got_all_chains) {
		for (chain_item = a_command_line->ugo_chain ; chain_item ;
		     chain_item = chain_item->next) {
		  the_chain = (char *)(chain_item->data);
		
		  if (! strcmp(the_chain, ANY_MATCH)) {
		    /* get all the chains in the current product area */
		    upsutl_get_files(location, (char *)CHAIN_SUFFIX,
				     &all_chains);
		    any_chain = 1;             /* originally chain was *  */
		  
		  } else {
		    /* Now add this chain to the master list */
		    all_chains = upslst_add(all_chains, the_chain);
		  }
		}
	      }
	      /* make sure if we need unique instance that we only have one */
	      CHECK_UNIQUE(all_chains, "chains");
	    }

	    /* Look to see if a version was specified. */
	    if (a_command_line->ugo_version) {
	      if (! got_all_versions) {
		/* get all the versions in the current product area */
		upsutl_get_files(location, (char *)VERSION_SUFFIX,
				 &all_versions);
		any_version = 1;             /* originally version was *  */
	      }
	      /* make sure if need unique instance that we only have one */
	      CHECK_UNIQUE(all_versions, "versions");
	    }
	      
	    /* now do the instance matching */
	    mproduct = match_instance_core(a_command_line, db_info,
					   prod_name, all_chains,
					   all_versions, a_need_unique,
					   any_version, any_chain);
	    /* may no longer need version list - free it */
	    if (! got_all_versions) {
	      all_versions = upslst_free(all_versions, do_delete);
	    }
	      
	    /* may no longer need chain list - free it */
	    if (! got_all_chains) {
	      all_chains = upslst_free(all_chains, do_delete);
	    }
	    
	    /* get out of the loop if we got an error */
	    CHECK_SOFT_ERR();
	  
	    /* update the mproduct_list structure with the new info */
	    ADD_TO_MPRODUCT_LIST();
	  }
	  /* may no longer need product list - free it */
	  if (! got_all_products) {
	    all_products = upslst_free(all_products, do_delete);
	  }
	}
	/* if we have a match and are asking for a unique one, we do not need
	   to go to the next db */
	if (a_need_unique && mproduct) {
	  break;
	}
	/* free the db configuration info, it we previously read it in */
	FREE_CONFIG_FILE();
      }
    }
  } else if (a_command_line->ugo_m && a_command_line->ugo_product) {
    /* We have a table file and no db, that is ok */
    if (UPS_VERBOSE) {                                                 
      printf("%sNo UPS Database, using Table File - %s\n", VPREFIX,    
	     (char *)a_command_line->ugo_tablefile);
    }
    MATCH_TABLE_ONLY(a_command_line->ugo_product);
  } else {
    /* we have no db and no table file or no product name, this is an error */
    upserr_vplace();
    upserr_add(UPS_NO_DATABASE, UPS_FATAL);
  }

  /* make sure we have cleaned up */
  /* free the db configuration info, it we previously read it in */
  FREE_CONFIG_FILE();

  if (all_products) {
    /* no longer need product list - free it */
    all_products = upslst_free(all_products, do_delete);
  }
  if (all_versions) {
    /* no longer need version list - free it */
    all_versions = upslst_free(all_versions, do_delete);
  }
  if (all_chains && (!got_all_chains)) {
    /* no longer need chain list - free it */
    all_chains = upslst_free(all_chains, do_delete);
  }      

  /* back up to the front of the list */
  mproduct_list = upslst_first(mproduct_list);

  /* restore the next pointer of the db list */
  if (a_db_info) {
    db_list->next = saved_next;
  }

  /* return any error we encountered along the way */
  if (g_ups_error != UPS_SUCCESS) {
    UPS_ERROR = g_ups_error;
  }
  return(mproduct_list);
}

/*-----------------------------------------------------------------------
 * upsmat_match_with_instance
 *
 * Given an instance and a read in product structure, return a pointer
 * to the instance (in the product structure) that matches (version, flavor
 * and quals) the passed in instance.  This will be used for example. to pick
 * out chains that point to already matched versions.
 * 
 *
 * Input : instance, and a read in product structure
 * Output: None
 * Return: the instance list item pointer that matches.
 *         if no match, then NULL.
 */
t_upslst_item *upsmat_match_with_instance(
				    const t_upstyp_instance * const a_instance,
				    const t_upstyp_product * const a_product)
{
  t_upslst_item *instance_list = NULL;
  t_upstyp_instance *inst = NULL;

  if (a_instance->version) {
    for (instance_list = a_product->instance_list ; instance_list ;
	 instance_list = instance_list->next) {
      inst = (t_upstyp_instance *)instance_list->data;
      if (inst->version) {       /* make sure we have one */
	if (! strcmp(a_instance->version, inst->version)) {
	  /* they are the same version */
	  if (! strcmp(a_instance->flavor, inst->flavor)) {
	    /* they have the same flavor */
	    if (! strcmp(a_instance->qualifiers, inst->qualifiers)) {
	      /* they have the same qualifiers, we found a match, return it */
	      break;
	    }
	  }
	}
      }
    }
  }
  return(instance_list);
}

/*
 * Definition of private globals.
 */
#define G_SAVE_PATH(s) path_ptr = (char *)upsmem_malloc(s); strcpy(path_ptr, buffer)

/*
 * Definition of private functions.
 */

/*-----------------------------------------------------------------------
 * match_instance_core
 *
 * Actually do all the matching work for the product and version.
 *
 * Input : the command line input, a database, product name, list of chains,
 *         list of versions and a flag specifying a unique instance is desired.
 * Output: none
 * Return: a pointer to the instances matched from the files read.
 */
static t_upstyp_matched_product *match_instance_core(
				 const t_upsugo_command * const a_command_line,
				 const t_upstyp_db * const a_db_info,
				 const char * const a_prod_name,
				 const t_upslst_item * const a_chain_list,
				 const t_upslst_item * const a_version_list,
				 const int a_need_unique,
				 const int a_any_version,
				 const int a_any_chain)
{
  t_upstyp_matched_product *mproduct = NULL;
  t_upslst_item *tmp_minst_list = NULL, *vminst_list = NULL;
  t_upslst_item *master_minst_list = NULL, *matched_item = NULL;
  t_upslst_item *chain_list = NULL, *version_list = NULL;
  t_upstyp_matched_instance *minst = NULL;
  t_upstyp_product *read_product = NULL;
  int num_matches = 0, tmp_num_matches;
  char *chain, *version, *dummy;

  /* see if we were passed a table file. if so, don't worry about
     version and chain files, just read the table file */
  if (a_command_line->ugo_m) {
    if (UPS_VERBOSE) {
      printf("%sMatching with Table file  - %s\n", VPREFIX,
	     a_command_line->ugo_tablefile);
    }
    num_matches = match_from_table(a_prod_name,
				   a_command_line->ugo_tablefile,
				   a_command_line->ugo_tablefiledir,
				   a_command_line->ugo_upsdir,
				   a_command_line->ugo_productdir, a_db_info, 
				   a_need_unique, a_command_line->ugo_flavor,
				   a_command_line->ugo_qualifiers,
				   &master_minst_list);
    if (num_matches > 0) {
      /* we got some matches, fill out our matched product structure */
      /* clean out any invalid instances */
      FREE_INVALID_INSTANCES();

      mproduct = ups_new_matched_product(a_db_info, a_prod_name,
					 master_minst_list);
    }
    
  /* we were not passed a specific list of chains (or we were passed no chains
     at all) and we have some versions,  start by reading the version files. */
  } else if ((a_any_chain || !a_chain_list) && a_version_list) { 
    for (version_list = (t_upslst_item *)a_version_list; version_list;
	 version_list = version_list->next) {
      /* get the version */
      version = (char *)version_list->data;
      if (UPS_VERBOSE) {
	printf("%sMatching with Version - %s\n", VPREFIX, version);
      }
      tmp_num_matches = match_from_version(a_prod_name, version,
					   a_command_line->ugo_upsdir,
					   a_command_line->ugo_productdir,
					   a_db_info, a_need_unique,
					   a_command_line->ugo_flavor,
					   a_command_line->ugo_qualifiers, 
					   &tmp_minst_list);

      /* append matched instances to master list */
      master_minst_list = upslst_add_list(master_minst_list, tmp_minst_list);
      tmp_minst_list = NULL;

      /* if we had an error & it was an error that the requested file could
	 not be found and we are asking for many instances, then continue
	 with the next version.  else get out. */
      CHECK_SOFT_ERR();
      num_matches += tmp_num_matches;
    }
    if (a_chain_list && (num_matches > 0)) {
      /* Now we need to go thru the list of chains that were passed us, read in
	 each file, and associate any chains with the matched version
	 instances */
      for (chain_list = (t_upslst_item *)a_chain_list ; chain_list ;
	   chain_list = chain_list->next) {
	/* get the chain name */
	chain = (char *)chain_list->data;
	read_product = upsget_chain_file(a_db_info->name, a_prod_name, chain, &dummy);
	if ((UPS_ERROR == UPS_SUCCESS) && read_product) {
	  for (vminst_list = master_minst_list ; vminst_list ; 
	       vminst_list = vminst_list->next) {
	    minst = (t_upstyp_matched_instance *)vminst_list->data;
	    if (minst->version) {       /* make sure we have one */
	      if ((matched_item = 
		   upsmat_match_with_instance(minst->version, read_product))) {
		/* this is a match */
		if (UPS_VERBOSE) {
			printf("%sFound associated chain - %s\n", VPREFIX,
			       chain);
		}
		if (! minst->chain ) {
		  /* no chain here yet, fill it in */
		  minst->chain = (t_upstyp_instance *)matched_item->data;
		  upsmem_inc_refctr((void *)minst->chain);
		} else {
		  /* we need to add a list element to the list of extra
		     chains */
		  minst->xtra_chains = upslst_add(minst->xtra_chains,
				      (t_upstyp_instance *)matched_item->data);
		}
	      }
	    }
	  }
	} else {
	  /* if we could not find the file and we are looking for more than one
	     instance then clear out the error.  otherwise if a ups list is
	     done on an entire db for v2_0, all products without a v2_0 will
	     generate an error */
	  CHECK_NO_FILE(chain);
	}
      }

      /* return the xtra chain lists to the first list element */
      for (vminst_list = master_minst_list ; vminst_list ; 
	   vminst_list = vminst_list->next) {
	minst = (t_upstyp_matched_instance *)vminst_list->data;
	if (minst->xtra_chains) {
	  minst->xtra_chains = upslst_first(minst->xtra_chains);
	}
      }
    }

    /* We went thru the list of versions, get a matched product
       structure if we got no errors */
    if (num_matches > 0) {
      /* clean out any invalid instances */
      FREE_INVALID_INSTANCES();

      mproduct = ups_new_matched_product(a_db_info, a_prod_name,
					 master_minst_list);
    }
  } else if (a_chain_list) {
    /* we need to start with any requested chains and find the associated
       version and then table files. */
    for (chain_list = (t_upslst_item *)a_chain_list; chain_list;
	 chain_list = chain_list->next) {
      /* get the chain name */
      chain = (char *)chain_list->data;
      if (UPS_VERBOSE) {
	printf("%sMatching with Chain - %s\n", VPREFIX, chain);
      }
      tmp_num_matches = match_from_chain(a_prod_name, chain, 
					 a_command_line->ugo_version,
					 a_command_line->ugo_upsdir,
					 a_command_line->ugo_productdir,
					 a_db_info, 
					 a_need_unique,
					 a_command_line->ugo_flavor,
					 a_command_line->ugo_qualifiers, 
					 a_any_version, &tmp_minst_list);

      /* append matched instances to master list */
      master_minst_list = upslst_add_list(master_minst_list, tmp_minst_list);
      tmp_minst_list = NULL;

      /* we had an error, if it was an error that the requested file could
	 not be found and we are asking for many instances, then continue
	 with the next version.  else get out. */
      CHECK_SOFT_ERR();
      
      num_matches += tmp_num_matches;
    }

    /* We went thru the list of chain instances, get a matched product
       structure if we got any matches */
    if (num_matches > 0) {
      /* clean out any invalid instances */
      FREE_INVALID_INSTANCES();

      mproduct = ups_new_matched_product(a_db_info, a_prod_name,
					 master_minst_list);
    }
  }

  return mproduct;
}

/*-----------------------------------------------------------------------
 * match_from_chain
 *
 * Given the the command line inputs, return the matched instances read in
 * from the chain file, version file, and the table file.
 *
 * Input : product name, chain name, version from command line,
 *         table file name, table file directory
 *         product ups directory, product root directory,
 *         db name, unique instance flag,
 *         pointer to a list of flavors to match,
 *         pointer to a list of qualifiers to match,
 *         pointer to a list of instances (chain, version and table)
 * Output: pointer to updated lists of matched instances
 * Return: number of instances added to the list
 */
static int match_from_chain( const char * const a_product,
			     const char * const a_chain,
			     const char * const a_version,
			     const char * const a_upsdir,
			     const char * const a_productdir,
			     const t_upstyp_db * const a_db_info,
			     const int a_need_unique,
			     const t_upslst_item * const a_flavor_list,
			     const t_upslst_item * const a_quals_list,
			     const int a_any_version,
			     t_upslst_item ** const a_minst_list)
{
  int num_matches = 0, tmp_num_matches = 0;
  t_upstyp_product *read_product;
  t_upslst_item *cinst;
  t_upslst_item *tmp_flavor_list = NULL, *tmp_quals_list = NULL;
  t_upstyp_instance *inst = NULL;
  t_upstyp_matched_instance *tmp_minst_ptr = NULL;
  char *first_flavor, *first_quals, *buffer = NULL;
  char *tmp_upsdir, *tmp_productdir;
  int do_need_unique = 1;

  read_product = upsget_chain_file(a_db_info->name, a_product, a_chain, &buffer);
  if ((UPS_ERROR == UPS_SUCCESS) && read_product) {
    /* get all the instances that match command line input */
    tmp_num_matches = get_instance(read_product->instance_list,
				   a_flavor_list, a_quals_list,
				   a_need_unique, e_file_chain,
				   a_minst_list);
    if (UPS_VERBOSE) {
      printf("%sFound %d instances in %s\n", VPREFIX, tmp_num_matches,
	     buffer);
    }
    /* for each instance that was matched, open the version file, and only
       look for the instance that matches the instance found in the chain
       file.  this insures that an instance in a chain file is
       matched only with an instance in the associated version file. */
    if ((cinst = *a_minst_list)) {
      do {
	/* get a matched instance */
	tmp_minst_ptr = (t_upstyp_matched_instance *)(cinst->data);
	inst = tmp_minst_ptr->chain;
	
	/* check to see if a specific version was entered along with the
	   chain.  if so, then we only match those chains that point to the
	   same version as that which was entered. */
	if (a_version && !a_any_version) {
	  /* compare the entered version with the one associated with the
	     chain. if they do not match, get the next chain */
	  if (strcmp(inst->version, a_version)) {
	    /* mark this item to be deleted */
	    tmp_minst_ptr->invalid_instance = 1;
	    continue;
	  }
	} /* if (a_version && !a_any_version) */

	/* make 2 lists (tmp_flavor_list and tmp_quals_list), one of the 
	   desired flavor and one of the desired qualifier to match */
	TMP_LISTS_SET();

	/* see if any command line info should override what we read from the
	   files  - set tmp_upsdir, tmp_productdir */
	USE_CMD_LINE_INFO();

	if (UPS_VERBOSE) {
	  printf("%sMatching with Version %s in Product %s\n", VPREFIX,
		 inst->version, a_product);
	  printf("%sUsing Flavor = %s, and Qualifiers = %s\n", VPREFIX,
		 (char *)(tmp_flavor_list->data),
		 (char *)(tmp_quals_list->data));
	}
	tmp_num_matches = match_from_version(a_product, inst->version,
					     tmp_upsdir, tmp_productdir,
					     a_db_info,
					     do_need_unique, tmp_flavor_list,
					     tmp_quals_list, &cinst);
	if (tmp_num_matches == 0) {
	  /* We should have had a match, this is an error */
	  upserr_vplace();
	  upserr_add(UPS_NO_VERSION_MATCH, UPS_FATAL, buffer,
		     inst->version);
	  
	  /* mark this item to be deleted */
	  tmp_minst_ptr->invalid_instance = 1;
	} else {
	  /* keep a running total of the matches we found */
	  ++num_matches;
	}
	/* only point cinst to the next element on the list if there is one 
	   and if there was no error in which case we are already pointing
	   there */
      } while (cinst->next && (cinst = cinst->next));
    
    /* we no longer need the lists */
    TMP_LISTS_FREE();
    
    *a_minst_list = upslst_first(cinst);
    } /* if (cinst) */
  } else {
    /* if we could not find the file and we are looking for more than one
       instance then clear out the error.  otherwise if a ups list is done 
       on an entire db for v2_0, all products without a v2_0 will generate
       an error */
    CHECK_NO_FILE(a_db_info->name);
  }
  
return num_matches;
}

/*-----------------------------------------------------------------------
 * match_from_version
 *
 * Given the the command line inputs, return the matched instances read in
 * from the version file and the table file.
 *
 * Input : product name, product version, 
 *         table file name, table file directory
 *         product ups directory, product root directory,
 *         db name, unique instance flag,
 *         pointer to a list of flavors to match,
 *         pointer to a list of qualifiers to match,
 *         pointer to a list of instances (version and table)
 * Output: pointer to updated lists of matched instances
 * Return: number of instances added to the list
 */
static int match_from_version( const char * const a_product,
			       const char * const a_version,
			       const char * const a_upsdir,
			       const char * const a_productdir,
			       const t_upstyp_db * const a_db_info,
			       const int a_need_unique,
			       const t_upslst_item * const a_flavor_list,
			       const t_upslst_item * const a_quals_list,
			       t_upslst_item ** const a_minst_list)
{

  int file_chars, num_matches = 0, tmp_num_matches = 0;
  char buffer[FILENAME_MAX+1];
  t_upstyp_product *read_product;
  t_upslst_item *vinst;
  t_upslst_item *tmp_flavor_list = NULL, *tmp_quals_list = NULL;
  t_upstyp_instance *inst;
  t_upstyp_matched_instance *tmp_minst_ptr = NULL;
  char *first_flavor, *first_quals;
  char *tmp_upsdir, *tmp_productdir;
  int do_need_unique = 1;

  /* Get total length of version file name including path */
  file_chars = (int )(strlen(a_version) + strlen(a_product) +
		      strlen(a_db_info->name) + sizeof(VERSION_SUFFIX) + 4);
  if (file_chars <= FILENAME_MAX) {
    sprintf(buffer, "%s/%s/%s%s", a_db_info->name, a_product, a_version,
	    VERSION_SUFFIX);
    read_product = upsfil_read_file(&buffer[0]);
    if ((UPS_ERROR == UPS_SUCCESS) && read_product) {
      /* get all the instances that match command line input */
      tmp_num_matches = get_instance(read_product->instance_list,
				     a_flavor_list, a_quals_list,
				     a_need_unique, e_file_version, 
				     a_minst_list);
      if (UPS_VERBOSE) {
	printf("%sFound %d instances in %s\n", VPREFIX, tmp_num_matches,
	       buffer);
      }

      /* for each instance that was matched, open the table file, and only
	 look for the instance that matches the instance found in the version
	 file.  this insures that an instance in a version file is
	 matched only with an instance in the associated table file. */
      if ((vinst = *a_minst_list)) {
	do {
	  /* get an instance and thus a table file */
	  tmp_minst_ptr = (t_upstyp_matched_instance *)(vinst->data);
	  if ((inst = tmp_minst_ptr->version) != NULL) {
	    /* It is ok if we do not have a table file.  then we just do
	       whatever the default action is */
	    if (inst->table_file) {
	      /* make 2 lists (tmp_flavor_list and tmp_quals_list), one of the 
		 desired flavor and one of the desired qualifier to match */
	      TMP_LISTS_SET();
	      
	      /* see if any command line info should override what we read from
		 the files - set tmp_upsdir, tmp_productdir */
	      USE_CMD_LINE_INFO();
	      
	      if (UPS_VERBOSE) {
		printf("%sMatching with Version %s in Product %s using Table file %s\n",
		       VPREFIX, inst->version, a_product,
		       inst->table_file);
		printf("%sUsing Flavor %s and Qualifiers %s\n", VPREFIX,
		       (char *)(tmp_flavor_list->data),
		       (char *)(tmp_quals_list->data));
	      }
	      tmp_num_matches = match_from_table(a_product,
						 inst->table_file,
						 inst->table_dir, tmp_upsdir,
						 tmp_productdir, a_db_info,
						 do_need_unique,
						 tmp_flavor_list, 
						 tmp_quals_list, &vinst);
	      if (tmp_num_matches == 0) {
		/* We should have had a match, this is an error */
		upserr_vplace();
		upserr_add(UPS_NO_TABLE_MATCH, UPS_FATAL, buffer,
			   inst->table_file, inst->flavor, inst->qualifiers);

		/* mark this item to be deleted */
		tmp_minst_ptr->invalid_instance = 1;
	      } else {
		/* keep a running total of the matches we found */
		++num_matches;
	      }
	    } else {
	      /* this one did not have a table file so we just record the
		 match from the version file */
	      ++num_matches;
	    }
	  } else {
	    /* There are no more version matched instances filled out here */
	    break;
	  }
	} while (vinst->next && (vinst = vinst->next));
      
	/* we no longer need the lists */
	TMP_LISTS_FREE();
      }
    } else {
      /* if we could not find the file and we are looking for more than one
	 instance then clear out the error.  otherwise if a ups list is done 
	 on an entire db for v2_0, all products without a v2_0 will generate
	 an error */
      CHECK_NO_FILE(&buffer[0]);
    }
  } else {
    upserr_vplace();
    upserr_add(UPS_FILENAME_TOO_LONG, UPS_FATAL, file_chars);
  }

  return num_matches;
}

/*-----------------------------------------------------------------------
 * match_from_table
 *
 * Given the the command line inputs, return the matched instances read in
 * from the table file.
 *
 * Input : product name, a table file, a table file directory,
 *         products' ups directory, product root directory,
 *         db name, unique instance flag,
 *         pointer to a list of flavors to match,
 *         pointer to a list of qualifiers to match,
 *         pointer to a list of instances (table)
 * Output: pointer to an updated list of matched instances
 * Return: number of instances added to the list
 */
static int match_from_table( const char * const a_product,
			     const char * const a_tablefile,
			     const char * const a_tablefiledir,
			     const char * const a_upsdir,
			     const char * const a_productdir,
			     const t_upstyp_db * const a_db_info,
			     const int a_need_unique,
			     const t_upslst_item * const a_flavor_list,
			     const t_upslst_item * const a_quals_list,
			     t_upslst_item ** const a_minst_list)
{
  char *full_table_file;
  t_upstyp_product *read_product;
  int num_matches = 0;

  full_table_file = get_table_file_path(a_product, a_tablefile,
					a_tablefiledir, a_upsdir, a_productdir,
					a_db_info);

  if (full_table_file != NULL) {
    if ((read_product = upsfil_read_file(full_table_file)) != NULL) {
      /* get all the instances that match command line input */
       num_matches = get_instance(read_product->instance_list, a_flavor_list,
				  a_quals_list, a_need_unique, e_file_table,
				  a_minst_list);
       if (UPS_VERBOSE) {
	 printf("%sFound %d instances in %s\n", VPREFIX, num_matches,
		full_table_file);
       }

      /* free the table_file_path */
      upsmem_free(full_table_file);
    }
  }    
    return num_matches;
}

/*-----------------------------------------------------------------------
 * get_table_file_path
 *
 * Given table file and the directories, find the absolute path
 * to the file.  This depends on the following hierarchy for places to look
 * for the existence of the table file  -
 *
 *    Look in each of the following successively till the file is found.  If
 *    the file is not found, it is an error.  If one of the pieces is missing,
 *    say - ups_dir - then that step is skipped.  NOTE: there is no default for
 *    the table file name. if the prod_dir_prefix is missing that step is
 *    still done with the prefix just left off.
 *
 *         tablefiledir/tablefile
 *         tablefile
 *         ups_dir/tablefile
 *         prod_dir_prefix/prod_dir/ups_dir/tablefile
 *         db/prodname/tablefile
 * 
 *
 * Input : product name
 *         table file name
 *         table file directory
 *         ups directory information
 *         product dir
 *         ups database directory
 * Output: none
 * Return: table file location
 */
static char *get_table_file_path( const char * const a_prodname,
				  const char * const a_tablefile,
				  const char * const a_tablefiledir,
				  const char * const a_upsdir,
				  const char * const a_productdir,
				  const t_upstyp_db * const a_db_info)
{
  char buffer[FILENAME_MAX+1];   /* max size of file name and path on system */
  char *path_ptr = NULL;
  int file_chars, total_chars;
  int found = 0;

  if (a_tablefile != NULL) {
    file_chars = (int )strlen(a_tablefile) + 2;  /* length plus trailing null 
						    and leading '/' */
    /* try tablefiledir/tablefile */
    if (a_tablefiledir != NULL) {
      if ((total_chars = file_chars + (int )strlen(a_tablefiledir))
	  <= FILENAME_MAX) {
	sprintf(buffer, "%s/%s", a_tablefiledir, a_tablefile);
	if (upsutl_is_a_file(buffer) == UPS_SUCCESS) {
	  G_SAVE_PATH(total_chars);      /* found it - save it */
	  found = 1;
	}
      } else {
	upserr_vplace();
	upserr_add(UPS_FILENAME_TOO_LONG, UPS_FATAL, total_chars);
      }
    }
    /* try ./tablefile */
    if ((found == 0) && ((total_chars = file_chars) <= FILENAME_MAX)) {
      sprintf(buffer, "%s", a_tablefile);
      if (upsutl_is_a_file(buffer) == UPS_SUCCESS) {
	G_SAVE_PATH(file_chars);            /* found it */
	found = 1;
      }
    } else {
      upserr_vplace();
      upserr_add(UPS_FILENAME_TOO_LONG, UPS_FATAL, total_chars);
    }
    /* try ups_dir/tablefile */
    if ((found == 0) && (a_upsdir != NULL)) {
      if ((total_chars = file_chars + (int )strlen(a_upsdir))
	  <= FILENAME_MAX) {
	sprintf(buffer, "%s/%s", a_upsdir, a_tablefile);
	if (upsutl_is_a_file(buffer) == UPS_SUCCESS) {
	  G_SAVE_PATH(total_chars);         /* found it */
	  found = 1;
	}
      } else {
	upserr_vplace();
	upserr_add(UPS_FILENAME_TOO_LONG, UPS_FATAL, total_chars);
      }
    }
    /* try prod_dir/ups_dir/tablefile */
    if ((found == 0) && (a_upsdir != NULL) && (a_productdir != NULL)) {
      if (a_db_info->config && a_db_info->config->prod_dir_prefix) {
	if ((total_chars += (int )strlen(a_productdir) + 
	                    (int )strlen(a_db_info->config->prod_dir_prefix) +
	                    1) <= FILENAME_MAX) {
	  sprintf(buffer, "%s/%s/%s/%s", a_db_info->config->prod_dir_prefix,
		                         a_productdir, a_upsdir, a_tablefile);
	  if (upsutl_is_a_file(buffer) == UPS_SUCCESS) {
	    G_SAVE_PATH(total_chars);        /* found it */
	    found = 1;
	  }
	} else {
	  upserr_vplace();
	  upserr_add(UPS_FILENAME_TOO_LONG, UPS_FATAL, total_chars);
	}
      } else {
	if ((total_chars += (int )strlen(a_productdir) + 1) <= FILENAME_MAX) {
	  sprintf(buffer, "%s/%s/%s", a_productdir, a_upsdir, a_tablefile);
	  if (upsutl_is_a_file(buffer) == UPS_SUCCESS) {
	    G_SAVE_PATH(total_chars);        /* found it */
	    found = 1;
	  }
	} else {
	  upserr_vplace();
	  upserr_add(UPS_FILENAME_TOO_LONG, UPS_FATAL, total_chars);
	}
      }
    }
    /* try db/prod_name/tablefile */
    if ((found == 0) && (a_db_info != NULL) && (a_prodname != NULL)) {
      if ((total_chars = file_chars + (int )(strlen(a_prodname) +
					     strlen(a_db_info->name)) + 1)
	  <= FILENAME_MAX) {
	sprintf(buffer, "%s/%s/%s", a_db_info->name, a_prodname, a_tablefile);
	if (upsutl_is_a_file(buffer) == UPS_SUCCESS) {
	  G_SAVE_PATH(total_chars);        /* found it */
	}
      } else {
	upserr_vplace();
	upserr_add(UPS_FILENAME_TOO_LONG, UPS_FATAL, total_chars);
      }
    }
  }

return path_ptr;
}

/*-----------------------------------------------------------------------
 * get_instance
 *
 * Given a list of instances locate the best match with the flavor and
 * qualifier arrays.
 *
 * Input : a list of instances read in from a file,
 *         list of flavors to match,
 *         list of qualifiers to match,
 *         flag indicating if a unique instance is desired
 *         a list (empty or not) of instances
 * Output: pointer to last element on updated list of instances
 * Return: number of instances added to the list
 */
static int get_instance(const t_upslst_item * const a_read_instances,
			const t_upslst_item * const a_flavor_list,
			const t_upslst_item * const a_quals_list,
			const int a_need_unique,
			const int a_file_type,
			t_upslst_item ** const a_minst_list)
{
  int got_match;
  t_upslst_item *tmp_list, *tmp_flavor_list, *tmp_quals_list;
  t_upstyp_matched_instance *minst = NULL;
  t_upstyp_instance *instance;
  t_upslst_item *first_matched_inst = NULL;
  char *flavor = NULL, *quals = NULL;
  int num_matches  = 0, want_all_f = 1, want_all_q = 1;

  /* loop over all the flavors from the flavor list */
  for (tmp_flavor_list = (t_upslst_item *)a_flavor_list; tmp_flavor_list ;
       tmp_flavor_list = tmp_flavor_list->next) {
    flavor = (char *)tmp_flavor_list->data;
    got_match = 0;
    want_all_f = (! strcmp(flavor, ANY_MATCH));   /* true if flavor = *  */

    for (tmp_quals_list = (t_upslst_item *)a_quals_list; tmp_quals_list ;
	 tmp_quals_list = tmp_quals_list->next) {
      quals = (char *)tmp_quals_list->data;
      want_all_q = (! strcmp(quals, ANY_MATCH));   /* true if quals = *  */
      /* Check to see if the flavors match or want any flavor */
      for (tmp_list = (t_upslst_item *)a_read_instances; tmp_list ;
	   tmp_list = tmp_list->next) {
	instance = (t_upstyp_instance *)(tmp_list->data);
	if (want_all_f || (! strcmp(instance->flavor, flavor))) {
	  /* They do - now compare the qualifiers */
	  if (want_all_q || (! strcmp(instance->qualifiers, quals))) {
	    /* They do. Save the instances in the order they came in. */
	    if (a_file_type == e_file_chain) {
	      /* this instance was read in from a chain file, create a new
	         matched instance structure and start to fill it */
	      ADD_TO_MINST_LIST(chain);
	    } else if (a_file_type == e_file_version) {
	      /* this instance was read in from a version file */
	      ADD_TO_MINST_LIST(version);
	    } else {
	      /* this instance was read in from a table file */
	      ADD_TO_MINST_LIST(table);
	    }

	    if (first_matched_inst == NULL) {
	      /* Save this so we can return it - this is the first new instance
	         we added to the list this time */
	      first_matched_inst = *a_minst_list;
	    }
	    ++num_matches;
	    got_match = 1;
	    if ((!want_all_q && !want_all_f) || a_need_unique) {
	      break;
	    }
  	  }
	}
      }
      /* if we got a match and we only want one, break. */
      if (got_match && a_need_unique) {
	/* go get the next flavor */
	break;
      }
    }
    /* If we only want one instance - we got it, leave now */
    if (a_need_unique && got_match) {
      break;
    }
  }

  /* if we matched 1 or more instances this time, return a pointer to the first
     instance that was matched.  if no match was made, return what we were
     passed */
  if (first_matched_inst != NULL) {
    *a_minst_list = first_matched_inst;
  }

  return num_matches;
}
