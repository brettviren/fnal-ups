/************************************************************************
 *
 * FILE:
 *       ups_declare.c
 * 
 * DESCRIPTION: 
 *       This is the 'ups declare' command.
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
 *       Mon Dec 15, DjF, Starting...
 *
 ***********************************************************************/

/* standard include files */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>   /* toupper */
#include <time.h>

/* ups specific include files */
#include "ups.h"

/*
 * Definition of public variables.
 */

/*
 * Declaration of private functions.
 */

/*
 * Definition of global variables.
 */

#ifndef NULL
#define NULL 0
#endif

#define CHAIN "chain"
#define VERSION "version"

/*
 * Definition of public functions.
 */

/*-----------------------------------------------------------------------
 * ups_declare
 *
 * This is the main line for the 'ups declare' command.
 *
 * Input : argc, argv
 * Output: 
 * Return: 
 */
void ups_declare( t_upsugo_command * const uc )
{
  t_upslst_item *mproduct_list = NULL;
  t_upslst_item *minst_list = NULL;
  t_upslst_item *chain_list = NULL;
  t_upstyp_db *db_info = 0;
  t_upslst_item *db_list = 0;
  t_upstyp_matched_product *mproduct = NULL;
  t_upstyp_matched_instance *minst = NULL;
  int not_unique = 0;
  int need_unique = 1;
  t_upslst_item *save_flavor;
  t_upslst_item *save_qualifiers;
  t_upslst_item *save_chain;
  char * save_version;
  t_upstyp_product *product;
  char buffer[FILENAME_MAX+1];
  char *file=buffer;
  char *the_chain;
  char *the_flavor;
  char *the_qualifiers;
  t_upslst_item *cinst_list;                /* chain instance list */
  t_upstyp_instance *cinst;                 /* chain instance      */
  t_upstyp_instance *new_cinst;             /* new chain instance  */
/*  t_upslst_item *vinst_list;                 version instance list */
/*  t_upstyp_instance *vinst;                  version instance      */
  t_upstyp_instance *new_vinst;             /* new version instance  */
  char *username;
  char *time_date;
  struct tm *mytime;
  char *declared_date;
  time_t seconds=0;
  /* Get all the requested instances */

  if (!uc->ugo_product || !uc->ugo_version )
  { printf("To Declare a product you must specify a product and a version \n");
    abort();
  }
  if (!uc->ugo_f )
  { printf("To Declare a product you must specify a flavor \n");
    abort();
  }
  mproduct_list = upsmat_instance(uc, db_list , need_unique);
  if (mproduct_list)
  { printf("Exact instance of product exists doing NOTHING\n");
    abort();
  }
  save_chain=uc->ugo_chain;
  save_flavor=uc->ugo_flavor;
  save_qualifiers=uc->ugo_qualifiers;
  save_version=uc->ugo_version;
  username=upsutl_user();
  time_date=upsutl_time_date();
  seconds=time(0);
  mytime = localtime(&seconds);
  mytime->tm_mon++; /* correct jan=0 */
  declared_date = (char *) malloc((size_t)(9));
  sprintf(declared_date,"%d-%d-%d",
          mytime->tm_mon,mytime->tm_mday,mytime->tm_year);

/************************************************************************
 *
 * Find the right database to work with
 *
 * Check for product in ANY database if so use that database 
 * otherwise set to first database listed
 *
 ***********************************************************************/

 for (db_list = uc->ugo_db ; db_list ; db_list=db_list->next) 
 { db_info = (t_upstyp_db *)db_list->data;
   uc->ugo_chain = upslst_new((void *)ANY_MATCH);
   uc->ugo_version=0;
   uc->ugo_flavor = upslst_new((void *)ANY_MATCH);
   uc->ugo_qualifiers = upslst_new((void *)ANY_MATCH);
   mproduct_list = upsmat_instance(uc, db_list , not_unique);
   if (mproduct_list)    /* the product does exist */ 
   { upsver_mes(1,"Product %s currently exist in database %s\n",
                uc->ugo_product,
                db_info->name);
     break; 
   } db_info=0;
  } 
  if (!db_info) 
  { upslst_first(db_list);
    db_info = (t_upstyp_db *)db_list->data;
  } 

/************************************************************************
 *
 * If there was any chain specified at all we need to look at chain files
 *
 ***********************************************************************/

     if (save_chain)
     { uc->ugo_chain=save_chain;
       chain_list=uc->ugo_chain;
       the_chain=chain_list->data;
       mproduct_list = upsmat_instance(uc, db_list , not_unique);
       if (mproduct_list)  /* the chain exists */
       { upsver_mes(1,"Chain %s currently exist\n",the_chain);
         uc->ugo_flavor=save_flavor;
         uc->ugo_qualifiers=save_qualifiers;
         uc->ugo_chain=save_chain;
         mproduct_list = upsmat_instance(uc, db_list , need_unique);
         if (mproduct_list)  /* different version only */
         { upsver_mes(1,"same flavor and qualifiers exist\n");
           mproduct_list = upslst_first(mproduct_list);
           mproduct = (t_upstyp_matched_product *)mproduct_list->data;
           minst_list = (t_upslst_item *)mproduct->minst_list;
           minst = (t_upstyp_matched_instance *)(minst_list->data);
           cinst = (t_upstyp_instance *)minst->chain;
           product = upsget_chain_file(db_info->name,
                                       uc->ugo_product,
                                       the_chain, &file);
           if ((UPS_ERROR == UPS_SUCCESS) && product )
           { cinst_list=upsmat_match_with_instance( cinst, product );
             cinst=cinst_list->data;
             product->instance_list = 
                upslst_delete(product->instance_list,cinst,'d');
                upsver_mes(1,"Deleting %s of version %s\n",
                           the_chain,
                           cinst->version);
                (void )upsfil_write_file(product, file); 
            /* Do unchain action */
            /* un_the_chain???
               cmd_list = upsact_get_cmd((t_upsugo_command *)uc,
                                  mproduct, the_chain);
               if (UPS_ERROR == UPS_SUCCESS) {
                 upsact_process_commands(cmd_list, a_temp_file);
               }
               upsact_cleanup(cmd_list);
             */
           }
         } /* Get chain file (again) */
         product = upsget_chain_file(db_info->name,
                                     uc->ugo_product,
                                     the_chain, &file);
       } else { /* new chain does NOT exist at all */
         product = ups_new_product();
         sprintf(file,"%s/%s/%s%s",
                 db_info->name,
                 uc->ugo_product,
                 the_chain,CHAIN_SUFFIX);
       product->file = CHAIN;
       product->product=uc->ugo_product;
       product->chain = the_chain;
       }
       /* build new chain instance */
       new_cinst=ups_new_instance();
       new_cinst->version=save_version;
       the_flavor=save_flavor->data;
       new_cinst->flavor=the_flavor;
       the_qualifiers=save_qualifiers->data;
       new_cinst->qualifiers=the_qualifiers;
       new_cinst->declarer=username;
       new_cinst->declared=declared_date;
       product->instance_list = 
          upslst_add(product->instance_list,new_cinst);
       upsver_mes(1,"Adding chain %s version %s to %s\n",
                  the_chain,
                  new_cinst->version,
                  file);
       (void )upsfil_write_file(product, file);  
    }
/************************************************************************
 *
 * Chains have been complete on the version file...
 *
 ***********************************************************************/
/* We want NOTHING to do with chains at this point - it is out of sync */
    uc->ugo_chain=0;
    uc->ugo_version=save_version;  /* we must match of version now */
    uc->ugo_flavor = upslst_new((void *)ANY_MATCH);
    uc->ugo_qualifiers = upslst_new((void *)ANY_MATCH);
    mproduct_list = upsmat_instance(uc, db_list , not_unique);
    if (mproduct_list) 
    {  uc->ugo_flavor=save_flavor;
       uc->ugo_qualifiers=save_qualifiers;
       mproduct_list = upsmat_instance(uc, db_list , need_unique);
       if (mproduct_list) 
       { 
/*       { mproduct_list = upslst_first(mproduct_list);
         mproduct = (t_upstyp_matched_product *)mproduct_list->data;
         minst_list = (t_upslst_item *)mproduct->minst_list;
         minst = (t_upstyp_matched_instance *)(minst_list->data);
         vinst = (t_upstyp_instance *)minst->version;
         product = upsget_version_file(db_info->name,
                                       uc->ugo_product,
                                       uc->ugo_version, 
                                       &file);
         if ((UPS_ERROR == UPS_SUCCESS) && product )
         { printf("found version too!!!\n");
         } 
*/
       }
/*       product = upsget_version_file(db_info->name,
                                     uc->ugo_product,
                                     uc->ugo_version, 
                                     &file); */
    } else { /* new version does NOT exist at all */
      product = ups_new_product();
      sprintf(file,"%s/%s/%s%s",
              db_info->name,
              uc->ugo_product,
              uc->ugo_version,VERSION_SUFFIX);
      product->file = VERSION;
      product->product=uc->ugo_product;
      product->version = uc->ugo_version;
/*    } */
    /* build new version instance */
    new_vinst=ups_new_instance();
    new_vinst->version=save_version;
    the_flavor=save_flavor->data;
    new_vinst->flavor=the_flavor;
    the_qualifiers=save_qualifiers->data;
    new_vinst->qualifiers=the_qualifiers;
    new_vinst->declarer=username;
    new_vinst->declared=declared_date;
    new_vinst->prod_dir=uc->ugo_productdir;
    new_vinst->table_dir=uc->ugo_tablefiledir;
    new_vinst->table_file=uc->ugo_tablefile;
    new_vinst->ups_dir=uc->ugo_upsdir;
    product->instance_list = 
        upslst_add(product->instance_list,new_vinst);
    upsver_mes(1,"Adding version %s to %s\n",
               new_vinst->version,
               file);
    (void )upsfil_write_file(product, file);  
    }
}
