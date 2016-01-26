/* tmpl-glib.h
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

#ifndef TMPL_GLIB_H
#define TMPL_GLIB_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define TMPL_GLIB_INSIDE
# include "tmpl-error.h"
# include "tmpl-expr.h"
# include "tmpl-expr-types.h"
# include "tmpl-scope.h"
# include "tmpl-symbol.h"
# include "tmpl-template.h"
# include "tmpl-template-locator.h"
#undef TMPL_GLIB_INSIDE

G_END_DECLS

#endif /* TMPL_GLIB_H */
