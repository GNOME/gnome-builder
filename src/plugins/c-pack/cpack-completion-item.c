/* cpack-completion-item.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "cpack-completion-item"

#include "config.h"

#include <libide-sourceview.h>

#include "cpack-completion-item.h"

G_DEFINE_FINAL_TYPE_WITH_CODE (CpackCompletionItem, cpack_completion_item, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
cpack_completion_item_finalize (GObject *object)
{
  CpackCompletionItem *self = (CpackCompletionItem *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (cpack_completion_item_parent_class)->finalize (object);
}

static void
cpack_completion_item_class_init (CpackCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cpack_completion_item_finalize;
}

static void
cpack_completion_item_init (CpackCompletionItem *self)
{
}

CpackCompletionItem *
cpack_completion_item_new (const gchar *word)
{
  CpackCompletionItem *self;

  self = g_object_new (CPACK_TYPE_COMPLETION_ITEM, NULL);
  self->name = g_strdup (word);

  return self;
}

void
cpack_completion_item_display (CpackCompletionItem     *self,
                               GtkSourceCompletionCell *cell,
                               const char              *typed_text)
{
  GtkSourceCompletionColumn column;

  g_return_if_fail (CPACK_IS_COMPLETION_ITEM (self));
  g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (cell));

  column = gtk_source_completion_cell_get_column (cell);

  switch (column)
    {
    case GTK_SOURCE_COMPLETION_COLUMN_ICON:
      gtk_source_completion_cell_set_icon_name (cell, "lang-include-symbolic");
      break;

    case GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT:
      {
        PangoAttrList *attrs;

        attrs = gtk_source_completion_fuzzy_highlight (self->name, typed_text);
        gtk_source_completion_cell_set_text_with_attributes (cell, self->name, attrs);
        pango_attr_list_unref (attrs);

        break;
      }

    case GTK_SOURCE_COMPLETION_COLUMN_COMMENT:
    case GTK_SOURCE_COMPLETION_COLUMN_DETAILS:
    case GTK_SOURCE_COMPLETION_COLUMN_BEFORE:
    case GTK_SOURCE_COMPLETION_COLUMN_AFTER:
    default:
      gtk_source_completion_cell_set_text (cell, NULL);
      break;
    }

}
