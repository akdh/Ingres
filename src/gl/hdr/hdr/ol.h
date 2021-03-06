/*
**	Copyright (c) 2004 Ingres Corporation
*/
#include    <olcl.h>

/**CL_SPEC
** Name:	OL.h	- Define OL function externs
**
** Specification:
**
** Description:
**	Contains OL function externs
**
** History:
**	2-jun-1993 (ed)
**	    initial creation to define func_externs in glhdr
**	21-jan-1999 (hanch04)
**	    replace nat and longnat with i4
**	31-aug-2000 (hanch04)
**	    cross change to main
**	    replace nat and longnat with i4
**/

FUNC_EXTERN STATUS  OLpcall(
#ifdef CL_PROTOTYPED
	    VOID	(*func)(), 
	    OL_PV	args[], 
	    i4		argcnt, 
	    i4		lang, 
	    i4		proc_rettype, 
	    OL_RET	*ret_value
#endif
);

#ifndef OLanguage
FUNC_EXTERN char * OLanguage(
#ifdef CL_PROTOTYPED
	    i4		ol_type
#endif
);
#endif
