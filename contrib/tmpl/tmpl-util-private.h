/* tmpl-util-private.h
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

#ifndef TMPL_UTIL_PRIVATE_H
#define TMPL_UTIL_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

#define TMPL_CLEAR_VALUE(v) { if (G_VALUE_TYPE(v)) g_value_unset(v); }

void      tmpl_destroy_in_main_context (GMainContext   *main_context,
                                        gpointer        data,
                                        GDestroyNotify  destroy);
gchar    *tmpl_value_repr              (const GValue   *value);
gboolean  tmpl_value_as_boolean        (const GValue   *value);

G_END_DECLS

#endif /* TMPL_UTIL_PRIVATE_H */
