/* mi2-error.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
 */

#ifndef MI2_ERROR_H
#define MI2_ERROR_H

#include <glib.h>

G_BEGIN_DECLS

#define MI2_ERROR (mi2_error_quark())

typedef enum
{
  MI2_ERROR_UNKNOWN_ERROR,
  MI2_ERROR_EXEC_PENDING,
  MI2_ERROR_INVALID_DATA,
} Mi2Error;

GQuark mi2_error_quark (void);

G_END_DECLS

#endif /* MI2_ERROR_H */
