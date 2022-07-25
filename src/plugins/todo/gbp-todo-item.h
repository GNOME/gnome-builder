/* gbp-todo-item.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GBP_TYPE_TODO_ITEM (gbp_todo_item_get_type())

G_DECLARE_FINAL_TYPE (GbpTodoItem, gbp_todo_item, GBP, TODO_ITEM, GObject)

struct _GbpTodoItem
{
  GObject     parent_instance;
  GBytes     *bytes;
  const char *path;
  guint       lineno;
  const char *lines[5];
};

GbpTodoItem *gbp_todo_item_new        (GBytes       *bytes);
const gchar *gbp_todo_item_get_path   (GbpTodoItem  *self);
void         gbp_todo_item_set_path   (GbpTodoItem  *self,
                                       const gchar  *path);
void         gbp_todo_item_set_lineno (GbpTodoItem  *self,
                                       guint         lineno);
guint        gbp_todo_item_get_lineno (GbpTodoItem  *self);
void         gbp_todo_item_add_line   (GbpTodoItem  *self,
                                       const gchar  *line);
const gchar *gbp_todo_item_get_line   (GbpTodoItem  *self,
                                       guint         nth);

G_END_DECLS
