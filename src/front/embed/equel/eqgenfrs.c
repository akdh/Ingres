/*
** Copyright (c) 2004 Ingres Corporation
*/

# include	<compat.h>
# include	<si.h>
# include	<er.h>
# include	<gl.h>
# include	<sl.h>
# include	<iicommon.h>
# include	<fsicnsts.h>
# include	<equel.h>
# include	<eqsym.h>
# include	<eqstmt.h>
# include	<equtils.h>
# include	<eqgen.h>
# include	<eqfrs.h>
# include	<eqscan.h>
# include	<ereq.h>

/*
+* Filename:	eqgenfrs.c
** Purpose:	Generate interpreted FRS names and statements into calls to the
**		run-time FRS support routines.
** Defines:	
**		frs_head	- Generate header of frs statement.
**		frs_constant	- Add an frs-constant.
**		frs_iqvar	- INQUIRE into variable.
**		frs_setval	- SET to a value.
**		frs_close	- Close off statement.
**		frs_dump	- Dump global data structure.
**		frs_mode	- Check the validity of object mode.
**		frs_error	- Report error for FRS module.
** Locals:
**		frs_add_const	- Add the frs-constant as argument.
**	
** History:	05-aug-1986	- Abstracted interpretive routines in "eqfrs.c".
**				  Moved these routines here and changed to call
**				  interpretive check routines.  (jhw)
**		28-aug-1985	- Written to support the new format of the
**				  INQUIRE/SET FRS statements. (ncg)
**		04-dec-1985	- Added COLO[U]R and ACTIVATE (ncg)
**		29-jun-87 (bab)	Code cleanup.
**              08-aug-1990 (barbara)
**                  Used these routines for support of with clause on
**                  loadtable and insertrow (setting per value display
**                  attributes).  Call (IILQsaSetAttrio) is VERY similar to
**                  call generated for SET_FRS.  Also made change to allow 
**                  ROW object types take char object-names on various 
**                  frs-constants (not just on CHANGE frs-constant).
**		20-feb-92 (sandyd)
**		    Moved "GLOBALREF FRS__ frs;" from eqfrs.h to here, because
**		    the W4GL OSLGRAM.MY includes eqfrs.h, and the "frs" 
**		    reference was causing most of equel to get pulled into the
**		    W4GL link on VMS.
**	11-Nov-1993 (tad)
**		Bug #56449
**		Replace %x with %p for pointer values where necessary.
**      12-Dec-95 (fanra01)
**              Added definitions for referencing data in DLLs on windows NT
**              from code built for static libraries.
**	14-mar-1996 (thoda04)
**		Added function prototypes.
** Notes:
**
**  1. Compatibilty - This file was written for INGRES 4.0, but must be
**   		      backward compatible in two ways.  Code generated by
**	INGRES 3.0 must work on all systems (can make 4.0 calls). The field
**	f_old is for this purpose.
**
** 2. Syntax History:
**
** 	Old (3.0):	INQUIRE_FRS "field" form1 field1 (var = "format")
** 	New (4.0):	INQUIRE_FRS field form1 (var = format(field1))
** 
** Because of the syntax differences we pass different arguments to the run-time
** routines.  Some of the argument-number problems are resolved here, while 
** others are resolved at run-time.  If we have an object entry with the 'old'
** flag on then we must be able to accept the old style with more arguments too.
**
** 3. Naming conventions used in this file are described by the following syntax
**    and example:
**
**	INQUIRE_FRS object-type [parent-names(s)] 
**		(var = frs-const[(object-name)], ...)
**	SET_FRS object-type [parent-names(s)] 
**		(frs-const[(object-name)] = var_or_val, ...)
**	Example:
**		INQUIRE_FRS field form1 (var = format(field1))
**		SET_FRS frs (menumap = 1)
**
**	0.  The sequence "INQUIRE_FRS field form1" is a header (frs_head()).
** 	1.  The occurance of the word "field" is an FRS object-type (FRS_OBJ).
**	2.  "form1" is an object parent-name (FRS_NAME).
** 	3.  The occurance of the word "format" is an FRS constant (FRS_CONST).
**	4.  "field1" is an object-name (FRS_NAME).
**
** 4. Handling old statements:
**
** Once the statement header is reduced we can decide whether we are using the 
** old routines or the new ones. The old routines will be used if (1) the 
** object-type is a supported old object-type, and (2) the number of 
** parent-names is more than expected under the new syntax.  If the old format 
** is used then we backup a funny character (-) into the input stream to force 
** YACC to reduce the old style target lists.
**
** 5. New WITH clause syntax handled through these routines:
** 
**	[WITH (ATTR(colname) = ATTR_VAL, {ATTR(colname) = ATTR_VAL})];
** 
**    This syntax may appear on LOADTABLE and INSERTROW statements.
**    Example:
**	LOADTABLE form1 table1 (col1 = val) WITH (reverse(col1) = 1);
**
**	0.  The implied object type is column
**	1.  No parent-names on WITH clause (run-time has this info from the
**	    LOADTABLE/INSERTROW part of the statement).
**	2.  ATTR (e.g. 'reverse')  is an FRS constant
**	3.  'colname' (e.g. col1)  is an object-name.
*/

/* Special char to force old-type target list */
# define	FRSeqOLD()		*--SC_PTR = '-'

/* Local routines */
static void		frs_add_const(i4 konst, char *objname, SYM *objvar,
           		              i4  rownum, char *rowname, SYM *rowvar);

# if defined(NT_GENERIC) && defined(__USE_LIB__)
GLOBALDLLREF	FRS__	frs;
# else              /* NT_GENERIC && __USE_LIB__ */
GLOBALREF	FRS__	frs;
# endif             /* NT_GENERIC && __USE_LIB__ */

/*
+* FRS Header Parsing:
**
** Source Code:	
**    INQUIRE:	INQUIRE_FRS column form1 table1 (target-list)
**
**    WITH clause on LOADTABLE/INSERTROW:
**              ... WITH (ATTR(colname) = ATTR_VAL)...
** Internal Calls:
**    INQUIRE:	frs_inqset( FRSinq );
**		frs_object( "column" );
**		frs_parentname( form1, T_CHAR, (SYM *)0 );
**		frs_parentname( table1, T_CHAR, (SYM *)0 );
**		frs_head();
**
**    WITH:     frs_inqset( FRSload );
**              frs_object( "row" );
**
** Generates:
**    INQUIRE:	if (IIiqset(FsiCOLUMN, 0, "form1", "table1", (char *)0) != 0)
**
**    WITH clause:
**              nothing (LOADTABLE/INSERTROW code generation serves as
**              the "header" code generation).
*/

/*
+* Procedure:	frs_head
** Purpose:	Generate frs statement header (decide on old or new).
** Parameters:
**	None
** Return Values:
-*	none
** Notes:
** 	Based on the number of args decide if it is new or old.  The old type
** 	calls required more information per object, except FRS which can be
** 	mapped to a new type call anyway.  If it was old then make the old call,
**	and backup the special charcater to make sure we are called no more...
*/

void
frs_head()
{
    FRS__			*f = &frs;
    register 	FRS_OBJ		*fo;		/* Current object */
    register	FRS_NAME	*fn;
    register	i4		i;

    if (f->f_err)
	return;
    fo = f->f_obj;

    /* 
    ** If the number of arguments are more than expected (old format) and 
    ** the object in use was accepted by the old format.  Spit out the obj-type
    ** name and then all the parent-names.
    */
    if (f->f_argcnt > fo->fo_args && fo->fo_old)
    {
	arg_str_add( ARG_CHAR, fo->fo_name );
	for (i = 0, fn = f->f_pnames; i < f->f_argcnt; i++, fn++)
	{
	    if (fn->fn_var)
		arg_var_add( (SYM *)fn->fn_var, fn->fn_name );
	    else
		arg_str_add( ARG_CHAR, fn->fn_name );
	}
	gen_if( G_OPEN, IIINQSET, C_NOTEQ, 0 );
	FRSeqOLD();		/* Fool ol' YACC */
	f->f_old = TRUE;	/* For frs_close */
	return;
    }
    if (f->f_argcnt != fo->fo_args)
    {
	er_write( E_EQ0158_fsOBJARG, EQ_ERROR, 2, fo->fo_name, 
		er_na(fo->fo_args) );
	f->f_err = TRUE;
	return;
    }
    arg_int_add( fo->fo_def );		/* Add object constant */
    fn = &f->f_row;
    if (fn->fn_var)			/* Add row number - 0 in most cases */
	arg_var_add( (SYM *)fn->fn_var, fn->fn_name );
    else
	arg_str_add( ARG_INT, fn->fn_name );
    for (i = 0, fn = f->f_pnames; i < fo->fo_args; i++, fn++)
    {
	if (fn->fn_var)
	    arg_var_add( (SYM *)fn->fn_var, fn->fn_name );
	else
	    arg_str_add( ARG_CHAR, fn->fn_name );
    }
    gen_if( G_OPEN, IIIQSET, C_NOTEQ, 0 );
}


/*
+* FRS Constant Parsing:
**
** Source Code:	
**		INQUIRE ... (var1:ind = format(field1))	
**		SET ... (menumap = "file.map")
**              LOADTABLE/INSERTROW ... WITH (REVERSE(:colvar) = :attr_var)
** Internal Calls:
**	INQUIRE:	frs_iqvar( var1, vartype, varsym, indid, indsym );
**			frs_constant( "format", "field1", T_CHAR, (SYM *)0 );
**	SET:		frs_constant( "menumap", (char *)0, T_NONE, (SYM *)0 );
**			frs_setval( "file.map", T_CHAR, (SYM *)0, (char *)0,
**				    (SYM *)0 );
**      WITH:           frs_constant( "reverse", "colvar", T_CHAR, (SYM *)sym );**                      frs_setval( "attr_var", T_INT, sym, (char *)0,
**                                  (SYM *)0 );
** Generates:
**	INQUIRE:	
**	    IIiqfrs(&ind, ISVAR, vartype, varlen, var1, FsiFORMAT, "field1", 0);
**	SET:
**	    IIstfrs(FsiMENUMAP, (char *)0,0,NULL, NOVAR, T_CHAR, 0, "file.map");
**
**      WITH clause on LOADTABLE and INSERTROW:
**          IILQsaSetAttrio(FsiREVERSE, colvar, (short *)0, 1,
**                              T_INT, 4, &attr_var);
*/


/*
+* Procedure:	frs_constant
** Purpose:	Setup frs-constant
** Parameters:
**	constname	- char *	- Frs-constant name.
**	objname		- char *	- Object name.
**	type		- i4		- Type of object name.
**	var		- SYM *		- Is object name a variable.
** Return Values:
-*	None
** Notes:
**   1. Most of the verification is done via table lookup, but some special
**	cases are handled individually.
**   2. In cases of INQUIRE generates the call (last thing done),
**	in case of SET saves frs-constant info for frs_setval.
*/

void
frs_constant( constname, objname, type, var )
char	*constname;
char	*objname;
i4	type;
SYM	*var;
{
    register FRS__	*f = &frs;
    register i4	fcdef;
    i4			value;

    if (f->f_err)
	return;

    fcdef = frsck_constant( constname, objname, type, var != NULL, &value );
    if (value >= 0)
	frs_add_const( fcdef, (char *)0, (SYM *)0, value, (char *)0, (SYM *)0 );
    else
    {
	/* 
	** May have an objname if not then it's null and uses current.
	** If under the ROW object-type and objname is integer (maybe var)
	** then pass as row); if not then row 0 will be used.
	**	set_frs row form tab (blink(4) = 1, change("col") = 0)
	**				    ^row#	    ^col name
	*/
       if (f->f_obj->fo_def == FsiROW && type == T_INT)
	    frs_add_const( fcdef, (char *)0, (SYM *)0, 0, objname, var );
	else
	    frs_add_const( fcdef, objname, var, 0, (char *)0, (SYM *)0 );
    }

    if (f->f_mode == FRSinq)	/* Generate call on INQUIRE */
	gen_call( IIIQFRS );
}


/*
+* Procedure:	frs_iqvar
** Purpose:	INQUIRE into a variable.
** Parameters:
**	name	- char *- Variable name.
**	type	- i4	- Variable type.
**	var	- SYM  *- Variable symbol table entry.
**	indid   - char *- Null indicator name
**	indsym  - SYM  *- Null indicator symbol table entry (might be NULL)
** Return Values:
-*	None
** Notes:
**	Saves the information away for frs_constant, and adds var as argument.
*/

void
frs_iqvar( name, type, var, indid, indsym )
char	*name;
i4	type;
SYM	*var;
char	*indid;
SYM	*indsym;
{
    register	FRS__	*f = &frs;

    if (f->f_err)
	return;
    /* Save information for later frs-constant type checking */
    frs_iqsave( name, type );

    if (indsym != (SYM *)0)
	arg_var_add( indsym, indid );
    else
	arg_int_add( 0 );
    arg_var_add( var, name );
}

/*
+* Procedure:	frs_setval
** Purpose:	SET a value for a frs-constant.
** Parameters:
**	name	- char *- Variable/value name.
**	type	- i4	- Variable/value type.
**	var	- SYM  *- Variable symbol table entry if variable.
**	indid	- char *- Null indicator variable name
**	indsym  - SYM  *- Null indicator symbol table entry (might be null)
** Return Values:
-*	None
** Notes:
**   1.	Checks to see that the information is compatible with the current
**	frs-constant, then adds the value as an argument and generates the
**	i/o call.
**   2.	Handles some special cases of map and modes.
**   3. Setting per-value display attributes (WITH clause on LOADTABLE/
**      INSERTROW) has separate gen_call.
*/

void
frs_setval( name, type, var, indid, indsym )
char	*name;
i4	type;
SYM	*var;
char	*indid;
SYM	*indsym;
{
    register FRS__	*f = &frs;
    register FRS_CONST	*fc;
    i4			value;

    if (f->f_err)		/* Error at statement level */
	return;
    fc = f->f_const;
    if (f->f_consterr)		/* Error at frs-constant */
    {
	arg_int_add( 0 );	/* Argument slot for indicator */
	arg_str_add( ARG_CHAR, name );
    }
    /* Handle special control and pf values in a map or label clause */
    else if (fc->fc_def == FsiMAP)
    {
	/* Allow integer or string variables (internal, undocumented) */
	if (var != NULL)
	{
	    /* Assume internal users won't use null indicators with MAP */
	    arg_int_add( 0 );
	    if (type != T_INT && type != T_CHAR && type != T_DBV)
	    {
		if (type != T_UNDEF)
		    er_write( E_EQ0061_grINTVAR, EQ_ERROR, 1, name );
		arg_int_add( 0 );
	    }
	    else
		arg_var_add( var, name );
	}
	/* Check for a map value */
	else
	{
	    arg_int_add( 0 );		/* Argument slot of null indicator */
	    arg_int_add( frsck_valmap(name) );
	}
    }
    else if (type != fc->fc_type[FRSset] && type != T_DBV)
    {
	/* Types are wrong - send as string constant */
	value = fc->fc_type[FRSset];
	if (var != NULL)
	    er_write( value == T_INT ? E_EQ0061_grINTVAR : E_EQ0067_grSTRVAR, 
		EQ_ERROR, 1, name );
	else
	    er_write( value == T_INT ? E_EQ0059_grINT : E_EQ0066_grSTR, 
		EQ_ERROR, 1, name );
	arg_int_add( 0 );		/* Argument slot of null indicator */
	arg_str_add( ARG_CHAR, name );
    }
    else if (fc->fc_def == FsiFMODE && var == (SYM *)0)
    {
	arg_int_add( 0 );	/* Argument slot for null indicator */
	frs_mode( ERx("SET_FRS"), name );
    }
    else if (var != NULL)
    {
	if (indsym != (SYM *)0 )
	    arg_var_add( indsym, indid );
	else
	    arg_int_add( 0 );
	arg_var_add( var, name );
    }
    else
    {
	arg_int_add( 0 );
	arg_str_add( type == T_INT ? ARG_INT : ARG_CHAR, name );
    }
    if (f->f_mode == FRSload)
	gen_call( IIFRSTATTR );
    else
	gen_call( IISTFRS );
}

/*
+* Procedure:	frs_close
** Purpose:	Close off the frs statement.
** Parameters:
**	None
** Return Values:
-*	None
*/

void
frs_close()
{
    if (frs.f_err)
	return;
    gen_if( G_CLOSE, frs.f_old ? IIINQSET : IIIQSET, C_0, 0 );
}


/*
+* Procedure:	frs_add_const
** Purpose:	Add the 3 special arguments required with IIIQFRS and IISTFRS.
** Parameters:
**	konst	- i4	- Fsi constant for the current frs-constant.
**	objname	- char *- Object-name.
**	objvar	- SYM * - If object-name is var then its sym entry.
**	rownum	- i4	- Row number if direct (usually 0).
**	rowname	- char *- If row was specified via user then its string rep.
**	rowvar	- SYM * - If rowname is var then its sym entry.
** Return Values:
-*	None
** Notes:
**	1. Adds 3 arguments: Fsi flag, object-name and row-number.
**	2. Sometimes row is used by higher level routines to get at the integer
**	   values of keys, control chars etc.
**      3. For WITH clause on LOADTABLE/INSERTROW, there is no generation
**         for row number because IILQsaSetAttrio operates on row currently
**         being loaded.
** Example:
**	"terminal"	- FsiTERMINAL, (char *)0, 0
**	"blink(1)"	- FsiBLINLK, (char *)0, 1
**	"reverse(fld1")	- FsiRVVID, "fld1", 0
*/

static void
frs_add_const( konst, objname, objvar, rownum, rowname, rowvar )
i4	konst;
char	*objname, *rowname;
SYM	*objvar, *rowvar;
i4	rownum;
{
    register FRS__      *f = &frs;

    arg_int_add( konst );
    if (objvar)
	arg_var_add( objvar, objname );
    else
	arg_str_add( ARG_CHAR, objname );

    if (f->f_mode == FRSload)
	return;

    if (rowname)
    {
	if (rowvar)
	    arg_var_add( rowvar, rowname );
	else
	    arg_str_add( ARG_INT, rowname );
    }
    else
	arg_int_add( rownum );
}

/*
+* Procedure:	frs_dump
** Purpose:	Dump global FRS structure.
** Parameters:
**	None
** Return Values:
-*	None
*/
i4
frs_dump()
{
    register	FRS__		*f = &frs;
    register 	FRS_NAME	*fn;
    register 	FILE		*df = eq->eq_dumpfile;
    static	char		*mod[] = {ERx("INQ"), ERx("SET")};
    i4				i;

    SIfprintf( df, ERx("FRS:\n") );
    SIfprintf( df, 
	ERx("  mode = %s, err = %d, old = %d, obj = '%s', args = %d\n"),
	mod[f->f_mode], f->f_err, f->f_old, 
	f->f_obj && f->f_obj->fo_name ? f->f_obj->fo_name : ERx("no-obj"),
	f->f_argcnt);
    fn = &f->f_row;
    SIfprintf( df, ERx("  row.name = '%s', row.var = 0x%p\n"),
		fn->fn_name ? fn->fn_name : ERx("no-row"), fn->fn_var );
    for (i = 0, fn = f->f_pnames; i < f->f_argcnt; i++, fn++)
    {
	SIfprintf( df, ERx("  pnames[%d] name = '%s', var = 0x%p\n"),
		    i, fn->fn_name, fn->fn_var );
    }
    SIfprintf( df, ERx("  const = '%s', consterr = %d\n"),
	f->f_const && f->f_const->fc_name ? f->f_const->fc_name:ERx("no-const"),
 	f->f_consterr );
    SIfprintf( df, ERx("  var.name = '%s', var.type = %d\n"),
		f->f_var.fv_name ? f->f_var.fv_name : ERx("no-var"),
		f->f_var.fv_type );
    SIflush( df );
    return(0);
}


/*
+* Procedure:	frs_mode
** Purpose:	Add argument for mode of object.
** Parameters:
**	stmt	- char *- Statement name (in case of error).
**	mode	- char *- Mode being set (cannot be in a variable).
** Return Values:
-*	None
** Notes:
**	Adds as an argument a one character abbreviation of string.
*/

void
frs_mode( stmt, mode )
char		*stmt;
register char	*mode;
{
    register	char	*cp;
    static	char	buf[2];

    char	*frsck_mode(char *stmt, char *mode);

    cp = frsck_mode( stmt, mode );
    buf[0] = cp != NULL ? *cp : 'f';
    buf[1] = EOS;
    arg_str_add( ARG_CHAR, buf );
}

/*
+* Procedure:	frs_error
** Purpose:	Report error for FRS module.
** Parameters:	ernum 	- i4	 - First argument is an error number, 
**		severity- i4	 - EQ_FATAL/EQ_ERROR/EQ_WARN
**		narg	- i4	 - Number of arguments,
**		e1..e5	- char * - Next (<=) 5 are SIprintf arguments.
-* Returns:	None
**
** Interface to EQUEL error reporting routines.
*/

void
frs_error (ernum, severity, narg, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10)
ER_MSGID	ernum;
i4	severity;
i4	narg;				/* No. of arguments */
PTR	e1, e2, e3, e4, e5, e6, e7, e8, e9, e10;
{
    er_write( ernum, severity, narg, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10 );
}
