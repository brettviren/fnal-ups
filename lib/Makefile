SHELL = /bin/sh
# 
#  1) This file is designed to be invoked by a shell script called premake
#  which selects supported options.
#
#=============================================================================
# TARGETS
#=============================================================================

default:	
	@echo Please invoke this makefile using premake. >&2
	@exit 1

all :
	@echo all target does nothing

#
# Now standard targets
#
clean :
	@ ../bin/upsClean

spotless : clean
	@ /bin/rm -f libups.a

