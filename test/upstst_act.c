/*

Authors:        Margaret Votava
e-mail:         "votava@fnal.gov"

Include files:-
===============
*/

#include <unistd.h>

/* ups specific include files */
#include "ups.h"
#include "upstst_parse.h"
#include "upstst_macros.h"

/* ==========================================================================

    upstst_act_print - tests ups list 

   ==========================================================================*/

int upstst_act_print (int argc, char ** const argv)
{
static char     *myfunc = "upsact_print";
static char     *options;			/* options */
static char	*outfile;			/* filename to output */
static char     *difffile;			/* file to diff */
static char     *action;			/* file to diff */
int		status;				/* function status */
FILE		*ofd;				/* outfile file descriptor */
upstst_argt     argt[] = {{"-options",UPSTST_ARGV_STRING,NULL,&options},
                          {"-out",    UPSTST_ARGV_STRING,NULL,&outfile},
                          {"-diff",   UPSTST_ARGV_STRING,NULL,&difffile},
                          {"<action>", UPSTST_ARGV_STRING,NULL,&action},
                          {NULL,      UPSTST_ARGV_END,   NULL,NULL}};
t_upsugo_command	*uc =0;			/* ups command */
char            diffcmd[132];                   /* diff command */
int		stdout_dup;			/* dup of stdout */

/* parse command line
   ------------------ */

outfile = NULL; difffile = NULL; options = NULL; action = NULL;
status = upstst_parse (&argc, argv, argt, UPSTST_PARSE_EXACTMATCH);
UPSTST_CHECK_PARSE(status,argt,argv[0]);
if (!options) options = "tal";
if (outfile) 					/* don't use stdout */
   {
   if (!(ofd = fopen(outfile,"w")))		/* open file */
      { perror(outfile); return (1); }
   }
else
   {ofd = stdout;}

stdout_dup = dup(STDOUT_FILENO);		/* dup stdout */
fflush(stdout);					/* clear output buffer */
dup2(fileno(ofd),STDOUT_FILENO);		/* reset it to output file */

/* call the real routine
   --------------------- */

UPS_ERROR = UPS_SUCCESS;
while (uc = upsugo_next(argc,argv,UPSTST_ALLOPTS))	/* for all commands */
   {
   UPSTST_CHECK_UPS_ERROR(UPS_SUCCESS);		/* check UPS_ERROR */
   upsact_print(uc,NULL,action,options);
   UPSTST_CHECK_UPS_ERROR(UPS_SUCCESS);		/* check UPS_ERROR */
   }

/* dump the output to specified file and compare
   --------------------------------------------- */

fflush(stdout);                                 /* flush buffer */
dup2(stdout_dup,STDOUT_FILENO);                 /* reset stdout */
close(stdout_dup);                              /* close files*/
if(fileno(ofd) != STDOUT_FILENO) fclose(ofd);

if (difffile && outfile)
   {
   sprintf (diffcmd,"diff %s %s",difffile,outfile);
   system(diffcmd);
   }

return (0);
}

/* ==========================================================================

    upstst_act_process_commands - tests ups list 

   ==========================================================================*/

int upstst_act_process_commands (int argc, char ** const argv)
{
static char     *myfunc = "upsact_process_commands";
static char     *options;			/* options */
static char	*outfile;			/* filename to output */
static char     *difffile;			/* file to diff */
static char     *action;			/* file to diff */
int		status;				/* function status */
FILE		*ofd;				/* outfile file descriptor */
upstst_argt     argt[] = {{"-options",UPSTST_ARGV_STRING,NULL,&options},
                          {"-out",    UPSTST_ARGV_STRING,NULL,&outfile},
                          {"-diff",   UPSTST_ARGV_STRING,NULL,&difffile},
                          {"<action>", UPSTST_ARGV_STRING,NULL,&action},
                          {NULL,      UPSTST_ARGV_END,   NULL,NULL}};
t_upsugo_command	*uc =0;			/* ups command */
char            diffcmd[132];                   /* diff command */
int		stdout_dup;			/* dup of stdout */

/* parse command line
   ------------------ */

outfile = NULL; difffile = NULL; options = NULL; action = NULL;
status = upstst_parse (&argc, argv, argt, UPSTST_PARSE_EXACTMATCH);
UPSTST_CHECK_PARSE(status,argt,argv[0]);
if (outfile) 					/* don't use stdout */
   {
   if (!(ofd = fopen(outfile,"w")))		/* open file */
      { perror(outfile); return (1); }
   }
else
   {ofd = stdout;}

stdout_dup = dup(STDOUT_FILENO);		/* dup stdout */
fflush(stdout);					/* clear output buffer */
dup2(fileno(ofd),STDOUT_FILENO);		/* reset it to output file */

/* call the real routine
   --------------------- */

UPS_ERROR = UPS_SUCCESS;
while (uc = upsugo_next(argc,argv,UPSTST_ALLOPTS))	/* for all commands */
   {
   UPSTST_CHECK_UPS_ERROR(UPS_SUCCESS);		/* check UPS_ERROR */
   upsact_print(uc,NULL,action,options);
   }

/* dump the output to specified file and compare
   --------------------------------------------- */

fflush(stdout);                                 /* flush buffer */
dup2(stdout_dup,STDOUT_FILENO);                 /* reset stdout */
close(stdout_dup);                              /* close files*/
if(fileno(ofd) != STDOUT_FILENO) fclose(ofd);

if (difffile && outfile)
   {
   sprintf (diffcmd,"diff %s %s",difffile,outfile);
   system(diffcmd);
   }

return (0);
}
