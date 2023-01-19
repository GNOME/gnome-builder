/* code-line-reader.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include <string.h>

#include "code-line-reader.h"

void
code_line_reader_init (CodeLineReader *reader,
                       char           *contents,
                       gssize          length)
{
  g_assert (reader);

  if (length < 0)
    length = strlen (contents);

  if (contents != NULL)
    {
      reader->contents = contents;
      reader->length = length;
      reader->pos = 0;
    }
  else
    {
      reader->contents = NULL;
      reader->length = 0;
      reader->pos = 0;
    }
}

/**
 * code_line_reader_next:
 * @reader: the #CodeLineReader
 * @length: a location for the length of the line in bytes.
 *
 * Moves forward to the beginning of the next line in the buffer. No changes to the buffer
 * are made, and the result is a pointer within the string passed as @contents in
 * code_line_reader_init(). Since the line most likely will not be terminated with a NULL byte,
 * you must provcode @length to determine the length of the line.
 *
 * Returns: (transfer none): The beginning of the line within the buffer.
 */
char *
code_line_reader_next (CodeLineReader *reader,
                       gsize          *length)
{
  char *ret = NULL;

  g_assert (reader);
  g_assert (length != NULL);

  if ((reader->contents == NULL) || (reader->pos >= reader->length))
    {
      *length = 0;
      return NULL;
    }

  ret = &reader->contents [reader->pos];

  for (; reader->pos < reader->length; reader->pos++)
    {
      if (reader->contents [reader->pos] == '\n')
        {
          *length = &reader->contents [reader->pos] - ret;
          /* Ignore the \r in \r\n if provcoded */
          if (*length > 0 && reader->pos > 0 && reader->contents [reader->pos - 1] == '\r')
            (*length)--;
          reader->pos++;
          return ret;
        }
    }

  *length = &reader->contents [reader->pos] - ret;

  return ret;
}
