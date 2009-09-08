#! /bin/sh
#  This file (ups_declare.sh) was created by Ron Rechenmacher <ron@fnal.gov> on
#  Sep  1, 2009. "TERMS AND CONDITIONS" governing this file are in the README
#  or COPYING file. If you do not have such a file, one can be obtained by
#  contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
#  $RCSfile$
#  rev='$Revision$$Date$'
if [ "$1" = -x ];then set -x;shift; fi
set -u

USAGE="\
   usage: `basename $0` <dir>...
examples: `basename $0` <dir>...
"
opts_wo_args='v|n|ups|y'
do_opt="\
  case \$opt in
  \\?|h|help)    echo \"\$USAGE\"; exit 0;;
  $opts_wo_args) vv=opt_\`echo \$opt |sed -e 's/-/_/g'\`
                 eval xx=\\\${\$vv-0};eval \"\$vv=\`expr \$xx + 1\`\";;
  *) echo \"invalid option: \$opt\" 2>&1;echo \"\$USAGE\" 2>&1; exit 1;;
  esac"
while op=`expr "${1-}" : '-\(.*\)'`;do
    shift
    if xx=`expr "$op" : '-\(.*\)'`;then
        if   opt=`expr     "$xx" : '\([^=]*\)='`;then
            set ${-+-$-} -- "`expr "$xx" : '[^=]*=\(.*\)'`" "$@"
        else opt=$xx; fi
        opts="${opts-} '--$opt'"
        eval "$do_opt"
    else
        while opt=`expr "$op" : '\(.\)'`;do
            opts="${opts-} '-$opt'"
            rest=`expr "$op" : '.\(.*\)'`
            eval "$do_opt"
            op=$rest
        done
    fi
done

if [ $# -lt 1 ];then echo "$USAGE";exit; fi

if [ "${opt_y-}" = '' ];then
    echo '# No commands will be executed. Use -y options to cause command execution.'
fi

#-----------------------------------------------------------------------

echov() { if [ "${opt_v-}" ];then echo "#`date`; $@";fi; }
cmd()
{
    if [ "${opt_y-}" ];then
        echo "executing $@..."
        eval "$@"
    else
        echo "$@"
    fi
}

#-----------------------------------------------------------------------

set_PRODS_RT()    # aka PRODS_DB (I combine them)
{
    dd=$PWD
    PRODS_RT=
    while [ $dd != / ];do
        DB=$dd
        if [ -f $DB/.upsfiles/dbconfig ];then
            # check for PROD_DIR_PREFIX
            PROD_DIR_PREFIX=`sed -n '/^[ \t]*PROD_DIR_PREFIX/{s/.*= *//;s/[ \t]*$//;p}' $DB/.upsfiles/dbconfig`
            if [ "$PROD_DIR_PREFIX" = "$DB" -o "$PROD_DIR_PREFIX" = '${UPS_THIS_DB}' ];then
                PRODS_RT=$DB
                PRODS_DB=$DB
                break
            fi
        fi
        dd=`dirname $dd`
    done
    if [ "$PRODS_RT" = '' ];then
        echo "Incompatible DB $DB != $PROD_DIR_PREFIX"
        exit 1
    fi
    echov PRODS_RT=$PRODS_RT
}   # set_PRODS_RT

set_NAM_and_VER()   # $1=nam-ver
{
    if nam_ver=`expr "$PWD" : "$PRODS_RT/\([^/][^/]*/[^/][^/]*\)"`;then
        echov "name/ver=$nam_ver"
        prod_=`expr "$nam_ver" : '\([^/][^/]*\)'`
         ver_=`expr "$nam_ver" : '[^/][^/]*/\([^/][^/]*\)'`
    else
        echo 'Error: not in prod_root/prod/version directory' >&2
        exit 1
    fi
    PROD_NAM=$prod_;    echov "prod is $prod_"
    PROD_VER=$ver_;    echov "ver is $ver_"
}   # set_NAM_and_VER

#-----------------------------------------------------------------------

for dd in "$@";do
    echo "#$dd"
    cd $dd

    set_PRODS_RT

    set_NAM_and_VER

    cd $PRODS_RT/$PROD_NAM/$PROD_VER

    ups_flavor.sh ${opt_ups:+--ups} 2>/dev/null | while read flavor fq;do
        echo "#    $flavor $fq"
        PROD_FLV=`expr $flavor : 'flavor=\(.*\)'`
        PROD_FQ=`expr $fq : 'fq=\(.*\)'`

        opt_K="PRODUCT:VERSION:FLAVOR:QUALIFIERS:CHAIN:PROD_DIR"
        ll=`ups list -aK$opt_K -z$PRODS_DB -f$PROD_FLV $PROD_NAM $PROD_VER`
        if [ -n "$ll" ];then
            echo "#    prod exists: $ll"
            continue
        fi

        if [ ! -d ups ];then cmd "    mkdir ups"; fi
        if   [ -f ups/$PROD_NAM.table ];then tblf=$PROD_NAM.table
        elif [ -f ups/generic.table   ];then tblf=generic.table
        else
            # need to have a table file
            if [ ! -f $UPS_DIR/ups/generic.table ];then
                echo "Cannot find \$UPS_DIR/generic.table."
                echo "Skipping ups declare for $PROD_NAM $PROD_VER."
                echo "Please try to generate a table file."
                continue
            else
                tblf=$PROD_NAM.table
                if   qq=`expr "$PROD_FQ" : "${PROD_FLV}_\(.*\)"`;then
                    QUAL="-q`echo $qq | tr _ :`"
                elif [ "$PROD_FQ" != . -a "$PROD_FQ" != "$PROD_FLV" ];then
                    prod_name_uc=`echo $PROD_NAM | tr '[a-z]' '[A-Z]'`
                    VAR="--vars=${VAR+$VAR,}${prod_name_uc}_FQ=$PROD_FQ"
                fi
                if [ -z "${QUAL-}" -a -z "${DEP-}" -a -z "${VAR-}" ];then
                    cmd "    cp $UPS_DIR/ups/generic.table ups/$tblf"
                else
                    cmd "    ups_table.sh -f $PROD_FLV ${QUAL-} ${DEP+"$DEP"} ${VAR+"$VAR"} >ups/$tblf"
                fi
            fi
        fi
        declare="ups declare -c -z$PRODS_RT -r$PROD_NAM/$PROD_VER -Mups"
        declare="$declare -m$tblf -f$PROD_FLV $PROD_NAM $PROD_VER"
        cmd "    $declare"
    done
done