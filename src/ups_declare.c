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
#include <time.h>

/* ups specific include files */
#include "ups.h"

/*
 * Definition of public variables.
 */
extern t_cmd_info g_cmd_info[];

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
#define DECLARE "declare"
#define UPS_DECLARE "DECLARE: "

static char buf[MAX_LINE_LEN];

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
t_upslst_item *ups_declare( t_upsugo_command * const uc ,
			    const FILE * const tmpfile, const int ups_command)
{
  t_upslst_item *mproduct_list = NULL;
  t_upslst_item *minst_list = NULL;
  t_upslst_item *chain_list = NULL;
  t_upslst_item *cmd_list = NULL;
  t_upslst_item *auth_list=0;
  char *allauthnodes=0;
  int count=0;
  int chain=0; /* was a chain specified, trick on on info messages */
  int version=0; /* version was defined */
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
  char *the_flavor = NULL;
  char *the_qualifiers;
  char *saddr;				/* start address for -O manipulation */
  char *eaddr;				/* end address for -O manipulation */
  char *naddr;				/* new address for -O manipulation */
  t_upslst_item *cinst_list;                /* chain instance list */
  t_upstyp_instance *cinst;                 /* chain instance      */
  t_upstyp_instance *new_cinst;             /* new chain instance  */
  t_upstyp_instance *tinst;                 /*   table instance      */
  t_upstyp_instance *new_vinst;             /* new version instance  */
  char *hold_env; /* temporary holding place for translated env's */
  char *username;
  struct tm *mytime;
  char *declared_date;
  char *unchain;
  t_upslst_item *save_next;
  t_upslst_item *save_prev;
  time_t seconds=0;
  char * save_table_dir;		/* match won't work "how I want" */
  char * save_table_file;		/* with table specifications     */
  char * tmp_ptr;
  int save_m=uc->ugo_m;
  int save_M=uc->ugo_M;
  uc->ugo_m=0;
  uc->ugo_M=0;
  save_table_dir=uc->ugo_tablefiledir;
  save_table_file=uc->ugo_tablefile;
  uc->ugo_tablefile=0;
  uc->ugo_tablefiledir=0;
  save_flavor=uc->ugo_flavor;
  save_qualifiers=uc->ugo_qualifiers;
  save_version=uc->ugo_version;

  if (!uc->ugo_product || !uc->ugo_version )
  { upserr_add(UPS_INVALID_SPECIFICATION, UPS_FATAL, "Declare", 
               "Specification must include a product and a version");
    return 0;
  }
  save_chain=uc->ugo_chain;
  if ((int)(upslst_count(uc->ugo_flavor) != 2) ) /* remember any */
  { if(!uc->ugo_chain) /* not possibly just defining a chain */
    { upserr_add(UPS_INVALID_SPECIFICATION, UPS_FATAL, "Declare", 
                 "Specification must include a single flavor");
      return 0;
    } else {  /* if just defining a chain it be there */
      uc->ugo_chain=0;
      mproduct_list = upsmat_instance(uc, db_list , need_unique);
      if (!mproduct_list)
      { upserr_add(UPS_INVALID_SPECIFICATION, UPS_FATAL, "Declare", 
                   "Specification must include a single flavor");
        return 0;
      } else { /* get the flavor for chain */
        mproduct_list = upslst_first(mproduct_list);
        mproduct = (t_upstyp_matched_product *)mproduct_list->data;
        minst_list = (t_upslst_item *)mproduct->minst_list;
        minst = (t_upstyp_matched_instance *)(minst_list->data);
        the_flavor=minst->version->flavor;
      }
    }
  }
  uc->ugo_chain=save_chain;
/* if they are defining a version ONLY and it allready exists fail */
  if ( !uc->ugo_chain && uc->ugo_version) 
  if (mproduct_list)
  { upserr_add(UPS_INVALID_SPECIFICATION, UPS_FATAL, "Declare", 
               "Exact product definition exists");
    return 0;
  }
  if (mproduct_list)
  { if (uc->ugo_chain) { uc->ugo_version=0; }
/* Don't do this */
/*    ups_undeclare(uc, tmpfile, e_undeclare); */
    uc->ugo_version=save_version;
  }
  username=upsutl_str_create(upsutl_user(), STR_TRIM_DEFAULT);
  seconds=time(0);
  mytime = localtime(&seconds);
  mytime->tm_mon++; /* correct jan=0 */
  declared_date = upsutl_str_create(upsutl_time_date(STR_TRIM_DEFAULT),
				    STR_TRIM_DEFAULT);
/* (char *) malloc((size_t)(9));
  sprintf(declared_date,"%d-%d-%d",
          mytime->tm_mon,mytime->tm_mday,mytime->tm_year);
*/

/************************************************************************
 *
 * Find the right database to work with
 *
 * Check for product in ANY database if so use that database 
 * otherwise set to first database listed
 *
 ***********************************************************************/

 uc->ugo_chain = upslst_new(upsutl_str_create(ANY_MATCH,' '));
 uc->ugo_version=0;
 uc->ugo_flavor = upslst_new(upsutl_str_create(ANY_MATCH,' '));
 uc->ugo_qualifiers = upslst_new(upsutl_str_create(ANY_MATCH,' '));
 for (db_list = uc->ugo_db ; db_list ; db_list=db_list->next) 
 { db_info = (t_upstyp_db *)db_list->data;
   mproduct_list = upsmat_instance(uc, db_list , not_unique);
   if (UPS_ERROR != UPS_SUCCESS) 
   { return 0; 
   } 
/*   if (UPS_ERROR != UPS_SUCCESS) { upserr_clear(); }  */
   if (mproduct_list)    /* the product does exist */ 
   { upsver_mes(1,"%sProduct %s currently exist in database %s\n",
                UPS_DECLARE,
                uc->ugo_product,
                db_info->name);
     break; 
   } db_info=0;
  } 
  if (!db_info) 
  { db_list = upslst_first(uc->ugo_db);
    db_info = (t_upstyp_db *)db_list->data;
  } 
/* restore everything */
  uc->ugo_chain=upslst_free(uc->ugo_chain,'d');
  uc->ugo_chain=save_chain;
  uc->ugo_version=save_version;
  uc->ugo_flavor=upslst_free(uc->ugo_flavor,'d');
  uc->ugo_flavor=save_flavor;
  uc->ugo_qualifiers=upslst_free(uc->ugo_qualifiers,'d');
  uc->ugo_qualifiers=save_qualifiers;
/************************************************************************
 *
 * If there was any chain specified at all we need to look at chain files
 *
 ***********************************************************************/

     if (uc->ugo_chain)
     { chain=1;
       for (chain_list = uc->ugo_chain ; chain_list ;
         chain_list = chain_list->next) 
       { the_chain = (char *)(chain_list->data);
         uc->ugo_version=0;
         uc->ugo_flavor = upslst_new(upsutl_str_create(ANY_MATCH,' '));
         uc->ugo_qualifiers = upslst_new(upsutl_str_create(ANY_MATCH,' '));
           save_next = chain_list->next;
           save_prev = chain_list->prev;
           chain_list->next=0;
           chain_list->prev=0;
           uc->ugo_chain=chain_list;
         mproduct_list = upsmat_instance(uc, db_list , not_unique);
         if (UPS_ERROR != UPS_SUCCESS) 
         { upsfil_clear_journal_files(); 
           upserr_vplace();
           return 0; 
         }
           chain_list->next = save_next;
           chain_list->prev = save_prev;
         if (mproduct_list)  /* the chain exists */
         { upsver_mes(1,"%sChain %s currently exist\n",UPS_DECLARE,the_chain);
           uc->ugo_flavor=save_flavor;
           uc->ugo_qualifiers=save_qualifiers;
           uc->ugo_chain=chain_list;
           chain_list->next=0; /* undeclare bug */
           chain_list->prev=0; /* undeclare bug */
           mproduct_list = upsmat_instance(uc, db_list , need_unique);
           chain_list->next = save_next; /* undeclare bug */
           chain_list->prev = save_prev; /* undeclare bug */
           if (UPS_ERROR != UPS_SUCCESS) 
           { upsfil_clear_journal_files();
             upserr_vplace();
             return 0; 
           }
           if (mproduct_list)  /* different version only */
           { upsver_mes(1,"%ssame flavor and qualifiers exist\n",UPS_DECLARE);
             mproduct_list = upslst_first(mproduct_list);
             mproduct = (t_upstyp_matched_product *)mproduct_list->data;
             minst_list = (t_upslst_item *)mproduct->minst_list;
             minst = (t_upstyp_matched_instance *)(minst_list->data);
             cinst = (t_upstyp_instance *)minst->chain;
             the_flavor=cinst->flavor;
             product = upsget_chain_file(db_info->name,
                                         uc->ugo_product,
                                         the_chain, &file);
             strcpy(buffer,file);
             if ((UPS_ERROR == UPS_SUCCESS) && product )
             { cinst_list=upsmat_match_with_instance( cinst, product );
               cinst=cinst_list->data;
               product->instance_list = 
                  upslst_delete(product->instance_list,cinst,'d');
               upsver_mes(1,"%sDeleting %s chain of version %s\n",
                             UPS_DECLARE,
                             the_chain,
                             cinst->version);
               (void )upsfil_write_file(product, buffer,' ',JOURNAL); 
               unchain = (char *) malloc((size_t)(strlen(the_chain)+3));
               sprintf(unchain,"un%s",the_chain);
               cmd_list = upsact_get_cmd((t_upsugo_command *)uc,
                                          mproduct, unchain,ups_command);
               if (UPS_ERROR == UPS_SUCCESS) 
               { upsact_process_commands(cmd_list, tmpfile); 
                 upsact_cleanup(cmd_list);
               } else {
                 upsfil_clear_journal_files();
                 upserr_vplace();
                 return 0 ;
               }
             }
           } /* Get chain file (maybe again) */
           sprintf(buffer,"%s/%s/%s%s",
                   db_info->name,
                   uc->ugo_product,
                   the_chain,CHAIN_SUFFIX);
           if (upsfil_exist(file))
           { product = upsget_chain_file(db_info->name,
                                       uc->ugo_product,
                                       the_chain, &file);
           /* allready there  strcpy(buffer,file); */
           /* if(!product) Chain deleted was all only one */ 
           } else { 
             product = ups_new_product();
             product->file = upsutl_str_create( CHAIN, ' ' );
             product->product=upsutl_str_create( uc->ugo_product, ' ' );
             product->chain = upsutl_str_create( the_chain, ' ' );
             product->version = upsutl_str_create( save_version, ' ' );
           }
         } else { /* new chain does NOT exist at all */
           product = ups_new_product();
           sprintf(buffer,"%s/%s/%s%s",
                   db_info->name,
                   uc->ugo_product,
                   the_chain,CHAIN_SUFFIX);
           product->file = upsutl_str_create( CHAIN, ' ' );
           product->product=upsutl_str_create( uc->ugo_product, ' ' );
           product->chain = upsutl_str_create( the_chain, ' ' );
           product->version = upsutl_str_create( save_version, ' ' );
         }
         /* build new chain instance */
         new_cinst=ups_new_instance();
         new_cinst->product=upsutl_str_create( uc->ugo_product, ' ' );
         new_cinst->version=upsutl_str_create(save_version,' ');
         if (!the_flavor)
         { the_flavor=save_flavor->data; 
         }
         new_cinst->flavor=upsutl_str_create(the_flavor,' ');
         the_qualifiers=save_qualifiers->data;
         new_cinst->qualifiers=upsutl_str_create(the_qualifiers,' ');
         new_cinst->declarer=upsutl_str_create(username,' ');
         new_cinst->declared=upsutl_str_create(declared_date,' ');
         new_cinst->modifier=upsutl_str_create(username,' ');
         new_cinst->modified=upsutl_str_create(declared_date,' ');
         product->instance_list = 
            upslst_add(product->instance_list,new_cinst);
         upsver_mes(1,"%sAdding %s chain version %s to %s\n",
                    UPS_DECLARE,
                    the_chain,
                    new_cinst->version,
                    buffer);
         (void )upsfil_write_file(product, buffer,' ',JOURNAL);  
        }
      }
/************************************************************************
 *
 * Chains have been complete on the version file...
 *
 ***********************************************************************/
/* We want NOTHING to do with chains at this point - it is out of sync */
    uc->ugo_chain=0;
    uc->ugo_version=save_version;  /* we must match of version now */
    uc->ugo_flavor = upslst_new(upsutl_str_create(ANY_MATCH,' '));
    uc->ugo_qualifiers = upslst_new(upsutl_str_create(ANY_MATCH,' '));
    mproduct_list = upsmat_instance(uc, db_list , not_unique);
    if (UPS_ERROR != UPS_SUCCESS) 
    { upsfil_clear_journal_files();
      upserr_vplace();
      return 0; 
    }
    if (mproduct_list) 
    {  uc->ugo_flavor=save_flavor;
       uc->ugo_qualifiers=save_qualifiers;
       mproduct_list = upsmat_instance(uc, db_list , need_unique);
       if (UPS_ERROR != UPS_SUCCESS) 
       { upsfil_clear_journal_files();
         upserr_vplace();
         return 0; 
       }
       if (!mproduct_list) 
       { upsver_mes(1,"%sVersion exists adding additional instance\n",
                    UPS_DECLARE);
         product = upsget_version_file(db_info->name,
                                       uc->ugo_product,
                                       uc->ugo_version,
                                       &file);
         strcpy(buffer,file);
       } else { 
         if (!save_chain) /* declaring the same this over and not chains */
         { upserr_add(UPS_INVALID_SPECIFICATION, UPS_FATAL, "Declare", 
                      "Specified product and version currently exists");
           return 0;
         }
         upsver_mes(chain,"INFORMATIONAL: Instance in version file already exists\n");
         buffer[0]=0; /* don't create instance */
         mproduct_list = upslst_first(mproduct_list);
         mproduct = (t_upstyp_matched_product *)mproduct_list->data;
         minst_list = (t_upslst_item *)mproduct->minst_list;
         minst = (t_upstyp_matched_instance *)(minst_list->data);
       }
    } else { /* new version does NOT exist at all */
      product = ups_new_product();
      sprintf(buffer,"%s/%s/%s%s",
              db_info->name,
              uc->ugo_product,
              uc->ugo_version,VERSION_SUFFIX);
      product->file = upsutl_str_create(VERSION,' ');
      product->product=upsutl_str_create(uc->ugo_product,' ');
      product->version = upsutl_str_create(uc->ugo_version,' ');
    }
/* no more matching replace "faked out stuff" */
    uc->ugo_tablefiledir=save_table_dir;
    uc->ugo_tablefile=save_table_file;
    uc->ugo_m=save_m;
    uc->ugo_M=save_M;
    /* build new version instance */
    if (buffer[0]!=0) /* instance doesn't exist */
    { new_vinst=ups_new_instance();
      new_vinst->product=upsutl_str_create(uc->ugo_product,' ');
      new_vinst->version=upsutl_str_create(save_version,' ');
      the_flavor=save_flavor->data;
      new_vinst->flavor=upsutl_str_create(the_flavor,' ');
      the_qualifiers=save_qualifiers->data;
      new_vinst->qualifiers=upsutl_str_create(the_qualifiers,' ');
      new_vinst->declarer=upsutl_str_create(username,' ');
      new_vinst->declared=upsutl_str_create(declared_date,' ');
      new_vinst->modifier=upsutl_str_create(username,' ');
      new_vinst->modified=upsutl_str_create(declared_date,' ');
      new_vinst->prod_dir=upsutl_str_create(uc->ugo_productdir,' ');
/*      new_vinst->table_dir=uc->ugo_tablefiledir;
        new_vinst->table_file=uc->ugo_tablefile;   */
      new_vinst->table_dir=upsutl_str_create(save_table_dir,' ');
      new_vinst->table_file=upsutl_str_create(save_table_file,' ');
      new_vinst->ups_dir=upsutl_str_create(uc->ugo_upsdir,' ');
      /* if no ups dir was entered on the command line, then check if 
	 PROD_DIR/ups exists. if it does, then set this = to ups. EFB */
      if ((! new_vinst->ups_dir) && new_vinst->prod_dir) {
	/* we must pass prod_dir through the env variable translator first */
	if (!(tmp_ptr = upsget_translation_env(new_vinst->prod_dir))) {
	  /* there was nothing to translate, use the original */
	  tmp_ptr = new_vinst->prod_dir;
	}
	if (db_info && db_info->config && db_info->config->prod_dir_prefix
	    && UPSRELATIVE(tmp_ptr)) {
	  /* we don't have to translate prod_dir_prefix, it is done when it
	     is read in */
	  sprintf(buf,"%s/%s/ups", db_info->config->prod_dir_prefix, tmp_ptr);
        } else {
	  sprintf(buf,"%s/ups", tmp_ptr);
        }
	if (upsutl_is_a_file(buf) == UPS_SUCCESS) {
	  /* the file existed */
	  new_vinst->ups_dir=upsutl_str_create("ups",' ');
	}
      }
      new_vinst->origin=upsutl_str_create(uc->ugo_origin,' ');
      new_vinst->compile_file=upsutl_str_create(uc->ugo_compile_file,' ');
      new_vinst->compile_dir=upsutl_str_create(uc->ugo_compile_dir,' ');
      new_vinst->archive_file=upsutl_str_create(uc->ugo_archivefile,' ');
      if (uc->ugo_A)
      { allauthnodes=upsutl_str_create("",' ');
      }
      for ( auth_list = upslst_first(uc->ugo_auth ); auth_list; 
            auth_list = auth_list->next, count++ )
      { allauthnodes=upsutl_str_crecat(allauthnodes,auth_list->data);
        if (auth_list->next != 0) 
        { allauthnodes=upsutl_str_crecat(allauthnodes,":");
        }
      }
      new_vinst->authorized_nodes=allauthnodes;
/* I'm going to create the save instance and just put everything in 
   there as the first fix for this */
      new_vinst->sav_inst = ups_new_instance();
      new_vinst->sav_inst->prod_dir=upsutl_str_create(new_vinst->prod_dir,' ');
      if((hold_env=upsget_translation_env(new_vinst->prod_dir))!=0)
      { new_vinst->prod_dir=upsutl_str_create(hold_env,' '); }
      if((hold_env=upsget_translation_env(new_vinst->compile_dir))!=0)
      { new_vinst->compile_dir=upsutl_str_create(hold_env,' '); }
      if((hold_env=upsget_translation_env(new_vinst->ups_dir))!=0)
      { new_vinst->ups_dir=upsutl_str_create(hold_env,' '); }
      if((hold_env=upsget_translation_env(new_vinst->table_dir))!=0)
      { new_vinst->table_dir=upsutl_str_create(hold_env,' '); }
/* If I'm creating a totally matched version I have to create the matched 
   product structure by hand since it really doesn't exist yet on disk
   and a call to get it will fail
*/
      minst = ups_new_matched_instance();
      minst->version=new_vinst;
      tinst = upsmat_version(new_vinst,db_info);
      minst->table=tinst;
      minst_list = upslst_add(minst_list,minst);
      mproduct = ups_new_matched_product(db_info, uc->ugo_product,
                                         minst_list);
      if (!uc->ugo_r )
      { upserr_add(UPS_NO_INSTANCE, UPS_INFORMATIONAL, 
               uc->ugo_product, "product home", 
               "\nSpecification did not include a -r for product directory");
      }
      if (!uc->ugo_m )
      { upserr_add(UPS_NO_INSTANCE, UPS_INFORMATIONAL, 
               uc->ugo_product, "table file", 
               "\nSpecification did not include a -m for table file only default actions will be performed");
      }
      if (uc->ugo_O)
      { if ( strchr(uc->ugo_options,':') == 0)
        { new_vinst->user_list = 
             upslst_add(new_vinst->user_list,upsutl_str_create(uc->ugo_options,' '));
        } else {
          saddr=uc->ugo_options;
          while ((eaddr = strchr(saddr,':')) != 0 )
          { *eaddr='\0';
            naddr=upsutl_str_create(saddr,' ');
            new_vinst->user_list = 
               upslst_add(new_vinst->user_list,naddr);
            *eaddr=':'; /* put back : want to free whole list later */
            saddr=eaddr+1;
          }
          naddr=upsutl_str_create(saddr,' ');
          new_vinst->user_list = 
             upslst_add(new_vinst->user_list,naddr);
        }
      }
      if (uc->ugo_L)
      { new_vinst->statistics=upsutl_str_create("",' '); 
      }
      product->instance_list = 
          upslst_add(product->instance_list,new_vinst);
      upsver_mes(1,"%sAdding version %s to %s\n",
                 UPS_DECLARE,
                 new_vinst->version,
                 buffer);
      (void )upsfil_write_file(product, buffer, ' ', JOURNAL);  
      version=1;
/* Process the declare action */
      cmd_list = upsact_get_cmd((t_upsugo_command *)uc,
                                 mproduct, g_cmd_info[ups_command].cmd,
                                 ups_command);
      if (UPS_ERROR == UPS_SUCCESS) 
      { upsact_process_commands(cmd_list, tmpfile); 
        upsact_cleanup(cmd_list);
      } else {
        upsfil_clear_journal_files();
        upserr_vplace();
        return 0;
      } 
    } 
      if (!uc->ugo_C) /* Don't do anything but declare actions */
                      /* Do configure actions                  */
      { if (version) /* only do configure if version was actually defined */
        { cmd_list = upsact_get_cmd((t_upsugo_command *)uc,
                                     mproduct, g_cmd_info[e_configure].cmd,
                                     ups_command);
          if (UPS_ERROR == UPS_SUCCESS) 
          { upsact_process_commands(cmd_list, tmpfile); 
            upsact_cleanup(cmd_list);
          } else {
            upsfil_clear_journal_files();
            upserr_vplace();
            return 0;
          }
/* Let them know if there is a tailor action for this product */
          cmd_list = upsact_get_cmd((t_upsugo_command *)uc,
                                     mproduct, g_cmd_info[e_tailor].cmd,
                                     ups_command);
          if (UPS_ERROR == UPS_SUCCESS) 
          { if (cmd_list)
            { upsver_mes(0,"A UPS tailor exists for this product\n");
              upsact_cleanup(cmd_list);
            }
          } 
        }
      uc->ugo_chain=save_chain;
      if (uc->ugo_chain)
      { uc->ugo_flavor=save_flavor;
        uc->ugo_qualifiers=save_qualifiers;
        uc->ugo_version=save_version;
        for (chain_list = uc->ugo_chain ; chain_list ;
             chain_list = chain_list->next)
        { the_chain = (char *)(chain_list->data);
          save_next = chain_list->next;
          save_prev = chain_list->prev;
          chain_list->next=0;
          chain_list->prev=0;
          uc->ugo_chain=chain_list;
          cmd_list = upsact_get_cmd((t_upsugo_command *)uc,
                                    mproduct, the_chain,
                                    ups_command);
          if (UPS_ERROR == UPS_SUCCESS) 
          { upsact_process_commands(cmd_list, tmpfile);
            upsact_cleanup(cmd_list);
          } else {
            upsfil_clear_journal_files();
            upserr_vplace();
            return 0;
          } 
          if (strstr(the_chain,"current"))
          { cmd_list = upsact_get_cmd((t_upsugo_command *)uc,
                                       mproduct, g_cmd_info[e_start].cmd,
                                       ups_command);
            if (!cmd_list)
            { cmd_list = upsact_get_cmd((t_upsugo_command *)uc,
                                         mproduct, g_cmd_info[e_stop].cmd,
                                         ups_command);
            }
            /*          if (UPS_ERROR == UPS_SUCCESS)  */
            if (cmd_list)
            { upsver_mes(0,"A UPS start/stop exists for this product\n");
              upsact_cleanup(cmd_list);
            } 
            if (minst->table && minst->table->info_source_dir)
            {  minst->table->info_source_dir =
               upsget_translation( minst, db_info, 
                                   uc, minst->table->info_source_dir);
            }
/*            upsutl_copy_info( minst, tmpfile, db_info);
              upsutl_copy_man( minst, tmpfile, db_info);
*/
              upscpy_info(minst,db_info,uc,tmpfile);
              upscpy_man(minst,db_info,uc,tmpfile);
              upscpy_catman(minst,db_info,uc,tmpfile);
          }
          chain_list->next = save_next;
          chain_list->prev = save_prev;
        }
      }
    } 
    return 0;
}
