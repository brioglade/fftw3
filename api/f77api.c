/*
 * Copyright (c) 2003, 2006 Matteo Frigo
 * Copyright (c) 2003, 2006 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "api.h"
#include "dft.h"
#include "rdft.h"

/* if F77_FUNC is not defined, then we don't know how to mangle identifiers
   for the Fortran linker, and we must omit the f77 API. */
#if defined(F77_FUNC) || defined(WINDOWS_F77_MANGLING)

/*-----------------------------------------------------------------------*/
/* some internal functions used by the f77 api */

/* in fortran, the natural array ordering is column-major, which
   corresponds to reversing the dimensions relative to C's row-major */
static int *reverse_n(int rnk, const int *n)
{
     int *nrev;
     int i;
     A(FINITE_RNK(rnk));
     nrev = (int *) MALLOC(sizeof(int) * rnk, PROBLEMS);
     for (i = 0; i < rnk; ++i)
          nrev[rnk - i - 1] = n[i];
     return nrev;
}

/* f77 doesn't have data structures, so we have to pass iodims as
   parallel arrays */
static X(iodim) *make_dims(int rnk, const int *n,
			   const int *is, const int *os)
{
     X(iodim) *dims;
     int i;
     A(FINITE_RNK(rnk));
     dims = (X(iodim) *) MALLOC(sizeof(X(iodim)) * rnk, PROBLEMS);
     for (i = 0; i < rnk; ++i) {
          dims[i].n = n[i];
          dims[i].is = is[i];
          dims[i].os = os[i];
     }
     return dims;
}

typedef struct {
     void (*f77_write_char)(char *, void *);
     void *data;
} write_char_data;

static void write_char(char c, void *d)
{
     write_char_data *ad = (write_char_data *) d;
     ad->f77_write_char(&c, ad->data);
}

typedef struct {
     void (*f77_read_char)(int *, void *);
     void *data;
} read_char_data;

static int read_char(void *d)
{
     read_char_data *ed = (read_char_data *) d;
     int c;
     ed->f77_read_char(&c, ed->data);
     return (c < 0 ? EOF : c);
}

static X(r2r_kind) *ints2kinds(int rnk, const int *ik)
{
     if (!FINITE_RNK(rnk) || rnk == 0)
	  return 0;
     else {
	  int i;
	  X(r2r_kind) *k;

	  k = (X(r2r_kind) *) MALLOC(sizeof(X(r2r_kind)) * rnk, PROBLEMS);
	  /* reverse order for Fortran -> C */
	  for (i = 0; i < rnk; ++i)
	       k[i] = (X(r2r_kind)) ik[rnk - 1 - i];
	  return k;
     }
}

/*-----------------------------------------------------------------------*/

#include "x77.h"

#define F77(a, A) F77x(x77(a), X77(A))

/* If F77_FUNC is not defined and the user didn't explicitly specify
   --disable-fortran, then make our best guess at default wrappers
   (since F77_FUNC_EQUIV should not be defined in this case, we
    will use both double-underscored g77 wrappers and single- or
    non-underscored wrappers).  This saves us from dealing with
    complaints in the cases where the user failed to specify
    an F77 compiler or wrapper detection failed for some reason. */
#if !defined(F77_FUNC) && !defined(DISABLE_FORTRAN)
#  if (defined(_WIN32) || defined(__WIN32__)) && !defined(WINDOWS_F77_MANGLING)
#    define WINDOWS_F77_MANGLING 1
#  endif
#  if defined(_AIX) || defined(__hpux) || defined(hpux)
#    define F77_FUNC(a, A) a
#  elif defined(CRAY) || defined(_CRAY) || defined(_UNICOS)
#    define F77_FUNC(a, A) A
#  else
#    define F77_FUNC(a, A) a ## _
#  endif
#  define F77_FUNC_(a, A) a ## __
#endif

#ifndef WINDOWS_F77_MANGLING

#if defined(F77_FUNC)
#  define F77x(a, A) F77_FUNC(a, A)
#  include "f77funcs.h"
#endif

/* If identifiers with underscores are mangled differently than those
   without underscores, then we include *both* mangling versions.  The
   reason is that the only Fortran compiler that does such differing
   mangling is currently g77 (which adds an extra underscore to names
   with underscores), whereas other compilers running on the same
   machine are likely to use non-underscored mangling.  (I'm sick
   of users complaining that FFTW works with g77 but not with e.g.
   pgf77 or ifc on the same machine.)  Note that all FFTW identifiers
   contain underscores, and configure picks g77 by default. */
#if defined(F77_FUNC_) && !defined(F77_FUNC_EQUIV)
#  undef F77x
#  define F77x(a, A) F77_FUNC_(a, A)
#  include "f77funcs.h"
#endif

#else /* WINDOWS_F77_MANGLING */

/* Various mangling conventions common (?) under Windows. */

/* g77 */
#  define WINDOWS_F77_FUNC(a, A) a ## __
#  define F77x(a, A) WINDOWS_F77_FUNC(a, A)
#  include "f77funcs.h"

/* Intel, etc. */
#  undef WINDOWS_F77_FUNC
#  define WINDOWS_F77_FUNC(a, A) a ## _
#  include "f77funcs.h"

/* Digital/Compaq/HP Visual Fortran, Intel Fortran.  stdcall attribute
   is apparently required to adjust for calling conventions (callee
   pops stack in stdcall).  See also:
       http://msdn.microsoft.com/library/en-us/vccore98/html/_core_mixed.2d.language_programming.3a_.overview.asp
*/
#  undef WINDOWS_F77_FUNC
#  if defined(__GNUC__)
#    define WINDOWS_F77_FUNC(a, A) __attribute__((stdcall)) A
#  elif defined(_MSC_VER) || defined(_ICC) || defined(_STDCALL_SUPPORTED)
#    define WINDOWS_F77_FUNC(a, A) __stdcall A
#  else
#    define WINDOWS_F77_FUNC(a, A) A /* oh well */
#  endif
#  include "f77funcs.h"

#endif /* WINDOWS_F77_MANGLING */

#endif				/* F77_FUNC */
