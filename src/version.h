/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: version.h,v 1.7 2004/09/20 03:25:49 kcwu Exp $
 *
 */

/*
 * Public version (release) numbers
 *
 * Update this number for every new release of libtabe.
 */

#define RELEASE_VER	"0.2.6"

/*
 * Internal interface numbers (quotted from `info libtool')
 *
 *   1. Start with version information of `0:0:0' for each libtool library.
 *
 *   2. Update the version information only immediately before a public
 *      release of your software.  More frequent updates are unnecessary,
 *      and only guarantee that the current interface number gets larger
 *      faster.
 *
 *   3. If the library source code has changed at all since the last
 *      update, then increment REVISION (`C:R:A' becomes `C:r+1:A').
 *
 *   4. If any interfaces have been added, removed, or changed since the
 *      last update, increment CURRENT, and set REVISION to 0.
 *
 *   5. If any interfaces have been added since the last public release,
 *      then increment AGE.
 *
 *   6. If any interfaces have been removed since the last public release,
 *      then set AGE to 0.
 *
 *   7. Everytime when AGE is set to 0, one may need to decrease the
 *      CURRENT to CURRENT-AGE+1 (the value before it is set to zero).
 *				-- suggested by T.H.Hsieh --
 */

#define CURRENT_VER	3
#define REVISION_VER	0
#define AGE_VER		1

