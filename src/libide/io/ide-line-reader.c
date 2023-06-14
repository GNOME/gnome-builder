/* ide-line-reader.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-line-reader"

#include "config.h"

#include <string.h>

#include "ide-line-reader.h"

void
ide_line_reader_init (IdeLineReader *reader,
                      gchar         *contents,
                      gssize         length)
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
 * ide_line_reader_next:
 * @reader: the #IdeLineReader
 * @length: a location for the length of the line in bytes.
 *
 * Moves forward to the beginning of the next line in the buffer. No changes to the buffer
 * are made, and the result is a pointer within the string passed as @contents in
 * ide_line_reader_init(). Since the line most likely will not be terminated with a NULL byte,
 * you must provide @length to determine the length of the line.
 *
 * Returns: (transfer none): The beginning of the line within the buffer.
 */
gchar *
ide_line_reader_next (IdeLineReader *reader,
                      gsize         *length)
{
  gchar *ret = NULL;

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
          /* Ignore the \r in \r\n if provided */
          if (*length > 0 && reader->pos > 0 && reader->contents [reader->pos - 1] == '\r')
            (*length)--;
          reader->pos++;
          return ret;
        }
    }

  *length = &reader->contents [reader->pos] - ret;

  return ret;
}
