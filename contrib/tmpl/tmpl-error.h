/* gis-error.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (TMPL_GLIB_INSIDE) && !defined (TMPL_GLIB_COMPILATION)
# error "Only <tmpl-glib.h> can be included directly."
#endif

#ifndef TMPL_ERROR_H
#define TMPL_ERROR_H

#include <glib.h>

G_BEGIN_DECLS

#define TMPL_ERROR (tmpl_error_quark())

typedef enum
{
  TMPL_ERROR_INVALID_STATE = 1,
  TMPL_ERROR_TEMPLATE_NOT_FOUND,
  TMPL_ERROR_CIRCULAR_INCLUDE,
  TMPL_ERROR_SYNTAX_ERROR,
  TMPL_ERROR_LEXER_FAILURE,
  TMPL_ERROR_TYPE_MISMATCH,
  TMPL_ERROR_INVALID_OP_CODE,
  TMPL_ERROR_DIVIDE_BY_ZERO,
  TMPL_ERROR_MISSING_SYMBOL,
  TMPL_ERROR_SYMBOL_REDEFINED,
  TMPL_ERROR_NOT_AN_OBJECT,
  TMPL_ERROR_NULL_POINTER,
  TMPL_ERROR_NO_SUCH_PROPERTY,
  TMPL_ERROR_GI_FAILURE,
  TMPL_ERROR_RUNTIME_ERROR,
  TMPL_ERROR_NOT_IMPLEMENTED,
  TMPL_ERROR_NOT_A_VALUE,
  TMPL_ERROR_NOT_A_FUNCTION,
} TmplError;

GQuark tmpl_error_quark (void);

G_END_DECLS

#endif /* TMPL_ERROR_H */
