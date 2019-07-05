/* ide-docs-search-group.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-docs-search-group"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-docs-search-group.h"
#include "ide-docs-search-row.h"

#define DEFAULT_MAX_CHILDREN 3

struct _IdeDocsSearchGroup
{
  GtkBin          parent_instance;

  GtkEventBox    *header;
  GtkLabel       *more;
  GtkLabel       *title;
  GtkBox         *rows;

  GtkGesture     *gesture;
  IdeDocsItem    *items;

  guint           max_items;
  gint            priority;

  guint           expanded : 1;
};

enum {
  PROP_0,
  PROP_MAX_ITEMS,
  PROP_PRIORITY,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_TYPE (IdeDocsSearchGroup, ide_docs_search_group, GTK_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
on_header_button_released_cb (IdeDocsSearchGroup   *self,
                              gint                  n_press,
                              gdouble               x,
                              gdouble               y,
                              GtkGestureMultiPress *gesture)
{
  g_assert (IDE_IS_DOCS_SEARCH_GROUP (self));
  g_assert (GTK_IS_GESTURE_MULTI_PRESS (gesture));

  ide_docs_search_group_toggle (self);
}

static void
ide_docs_search_group_header_realized (IdeDocsSearchGroup *self,
                                       GtkWidget          *widget)
{
  GdkCursor *cursor;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (IDE_IS_DOCS_SEARCH_GROUP (self));

  cursor = gdk_cursor_new_from_name (gtk_widget_get_display (widget), "pointer");
  gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
  g_clear_object (&cursor);
}

static void
ide_docs_search_group_finalize (GObject *object)
{
  IdeDocsSearchGroup *self = (IdeDocsSearchGroup *)object;

  g_clear_object (&self->gesture);
  g_clear_object (&self->items);

  G_OBJECT_CLASS (ide_docs_search_group_parent_class)->finalize (object);
}

static void
ide_docs_search_group_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeDocsSearchGroup *self = IDE_DOCS_SEARCH_GROUP (object);

  switch (prop_id)
    {
    case PROP_MAX_ITEMS:
      g_value_set_uint (value, ide_docs_search_group_get_max_items (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, ide_docs_search_group_get_priority (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_search_group_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeDocsSearchGroup *self = IDE_DOCS_SEARCH_GROUP (object);

  switch (prop_id)
    {
    case PROP_MAX_ITEMS:
      ide_docs_search_group_set_max_items (self, g_value_get_uint (value));
      break;

    case PROP_PRIORITY:
      ide_docs_search_group_set_priority (self, g_value_get_int (value));
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_search_group_class_init (IdeDocsSearchGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_docs_search_group_finalize;
  object_class->get_property = ide_docs_search_group_get_property;
  object_class->set_property = ide_docs_search_group_set_property;

  properties [PROP_MAX_ITEMS] =
    g_param_spec_uint ("max-items",
                       "Max Items",
                       "Max Items",
                       0, G_MAXUINT, DEFAULT_MAX_CHILDREN,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "THe priority of the group",
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title of the group",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "IdeDocsSearchGroup");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-docs/ui/ide-docs-search-group.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchGroup, header);
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchGroup, more);
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchGroup, rows);
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchGroup, title);
}

static void
ide_docs_search_group_init (IdeDocsSearchGroup *self)
{
  self->max_items = DEFAULT_MAX_CHILDREN;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->gesture = gtk_gesture_multi_press_new (GTK_WIDGET (self->header));
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->gesture), 1);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->gesture), FALSE);

  g_signal_connect_object (self->gesture,
                           "released",
                           G_CALLBACK (on_header_button_released_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->header,
                           "realize",
                           G_CALLBACK (ide_docs_search_group_header_realized),
                           self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

GtkWidget *
ide_docs_search_group_new (const gchar *title)
{
  return g_object_new (IDE_TYPE_DOCS_SEARCH_GROUP,
                       "title", title,
                       NULL);
}

const gchar *
ide_docs_search_group_get_title (IdeDocsSearchGroup *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_SEARCH_GROUP (self), NULL);

  return gtk_label_get_label (self->title);
}

void
ide_docs_search_group_add_items (IdeDocsSearchGroup *self,
                                 IdeDocsItem        *parent)
{
  const GList *iter;
  guint n_children;

  g_return_if_fail (IDE_IS_DOCS_SEARCH_GROUP (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (parent));
  n_children = ide_docs_item_get_n_children (parent);
  g_return_if_fail (n_children > 0);

  g_set_object (&self->items, parent);

  if (n_children > self->max_items)
    {
      guint more = n_children - self->max_items;
      g_autofree gchar *str = NULL;
      
      if (self->expanded)
        str = g_strdup (_("Show Fewer"));
      else
        str = g_strdup_printf ("+%u", more);

      gtk_label_set_label (self->more, str);

      if (!self->expanded)
        n_children = self->max_items;
    }
  else
    {
      gtk_label_set_label (self->more, "");
    }

  iter = ide_docs_item_get_children (parent);

  for (guint i = 0; i < n_children; i++)
    {
      IdeDocsItem *child = iter->data;
      GtkWidget *row;

      g_assert (IDE_IS_DOCS_ITEM (child));

      row = ide_docs_search_row_new (child);
      gtk_container_add (GTK_CONTAINER (self->rows), row);
      gtk_widget_show (row);

      iter = iter->next;
    }
}

gint
ide_docs_search_group_get_priority (IdeDocsSearchGroup *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_SEARCH_GROUP (self), 0);

  return self->priority;
}

void
ide_docs_search_group_set_priority (IdeDocsSearchGroup *self,
                                    gint                priority)
{
  g_return_if_fail (IDE_IS_DOCS_SEARCH_GROUP (self));

  if (self->priority != priority)
    {
      self->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIORITY]);
    }
}

void
ide_docs_search_group_toggle (IdeDocsSearchGroup *self)
{
  g_return_if_fail (IDE_IS_DOCS_SEARCH_GROUP (self));

  self->expanded = !self->expanded;

  gtk_container_foreach (GTK_CONTAINER (self->rows),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);
  ide_docs_search_group_add_items (self, self->items);
}

guint
ide_docs_search_group_get_max_items (IdeDocsSearchGroup *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_SEARCH_GROUP (self), 0);

  return self->max_items;
}

void
ide_docs_search_group_set_max_items (IdeDocsSearchGroup *self,
                                     guint               max_items)
{
  g_return_if_fail (IDE_IS_DOCS_SEARCH_GROUP (self));

  if (max_items == 0)
    max_items = DEFAULT_MAX_CHILDREN;

  if (max_items != self->max_items)
    {
      self->max_items = max_items;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAX_ITEMS]);
    }
}
