/* Copyright (C) 1991, 1992, 1996, 1998 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* SWISH-E Porting  */
#ifdef _WIN32
#  define _SWISH_PORT
#  include "config.h"
#endif
/* End SWISH-E Porting */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef __set_errno
#define __set_errno(x) errno=(x) 
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Generate a unique temporary file name from TEMPLATE.
   The last six characters of TEMPLATE must be "XXXXXX";
   they are replaced with a string that makes the filename unique.
   Returns a file descriptor open on the file for reading and writing.  */
int
mkstemp (Template)
     char *Template;
{
  static const char letters[]
    = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  size_t len;
  size_t i;

  len = strlen (Template);

  // printf("mkstemp: Template = %s\n", Template);
  if (len < 6 || strcmp (&Template[len - 6], "XXXXXX"))
    {
      __set_errno (EINVAL);
      return -1;
    }

  if (sprintf (&Template[len - 5], "%.5u",
	       (unsigned int) getpid () % 100000) != 5)
    /* Inconceivable lossage.  */
    return -1;

  for (i = 0; i < sizeof (letters); ++i)
    {
      int fd;

      Template[len - 6] = letters[i];

      fd = open (Template, O_RDWR|O_BINARY|O_CREAT|O_EXCL, 0600);
      if (fd >= 0)
	return fd;
    }

  /* We return the null string if we can't find a unique file name.  */

  Template[0] = '\0';
  return -1;
}
