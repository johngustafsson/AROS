/*
    Copyright � 1995-2001, The AROS Development Team. All rights reserved.
    $Id$

    Desc: ConvToLower() - Stub for the Language tolower() function.
    Lang: english
*/
#include <exec/types.h>
#include "locale_intern.h"
#include <aros/asmcall.h>

/*****************************************************************************

    NAME */
#include <proto/locale.h>

    AROS_LH2(ULONG, ConvToLower,

/*  SYNOPSIS */
	AROS_LHA(struct Locale *, locale, A0),
	AROS_LHA(ULONG          , character, D0),

/*  LOCATION */
	struct LocaleBase *, LocaleBase, 8, Locale)

/*  FUNCTION
	This function determine if the character supplied is upper case,
	and if it is, the character will be converted to lower case.
	Otherwise, the original character will be returned.

    INPUTS
	locale      - The Locale to use for this conversion.
	character   - The character to convert to lower case.

    RESULT
	The possibly converted character.

    NOTES
	This function requires a full 32-bit character in order to
	support future multi-byte character sets.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY
	27-11-96    digulla automatically created from
    		    locale_lib.fd and clib/locale_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    AROS_LIBBASE_EXT_DECL(struct LocaleBase *,LocaleBase)

    return AROS_CALL1(ULONG, IntL(locale)->il_LanguageFunctions[0],
	AROS_LCA(ULONG, character, D0),
	struct LocaleBase *, LocaleBase);

    AROS_LIBFUNC_EXIT
} /* ConvToLower */
