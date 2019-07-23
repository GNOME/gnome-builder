/* ide-docs-pane-row.c
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

#define G_LOG_DOMAIN "ide-docs-pane-row"

#include "ide-docs-pane-row.h"

struct _IdeDocsPaneRow
{
  GtkListBoxRow parent_instance;

  IdeDocsItem *item;

  /* Template Widgets */
  GtkLabel *title;
};

G_DEFINE_TYPE (IdeDocsPaneRow, ide_docs_pane_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_docs_pane_row_new:
 *
 * Create a new #IdeDocsPaneRow.
 *
 * Returns: (transfer full): a newly created #IdeDocsPaneRow
 */
GtkWidget *
ide_docs_pane_row_new (IdeDocsItem *item)
{
  g_return_val_if_fail (IDE_IS_DOCS_ITEM (item), NULL);

  return g_object_new (IDE_TYPE_DOCS_PANE_ROW,
                       "item", item,
                       NULL);
}

static void
ide_docs_pane_row_set_item (IdeDocsPaneRow *self,
                            IdeDocsItem    *item)
{
  const gchar *title;

  g_return_if_fail (IDE_IS_DOCS_PANE_ROW (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (item));

  if (!g_set_object (&self->item, item))
    return;

  title = ide_docs_item_get_title (item);
  gtk_label_set_label (self->title, title);
}

IdeDocsItem *
ide_docs_pane_row_get_item (IdeDocsPaneRow *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_PANE_ROW (self), NULL);

  return self->item;
}

static void
ide_docs_pane_row_finalize (GObject *object)
{
  IdeDocsPaneRow *self = (IdeDocsPaneRow *)object;

  g_clear_object (&self->item);

  G_OBJECT_CLASS (ide_docs_pane_row_parent_class)->finalize (object);
}

static void
ide_docs_pane_row_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeDocsPaneRow *self = IDE_DOCS_PANE_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, ide_docs_pane_row_get_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_pane_row_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeDocsPaneRow *self = IDE_DOCS_PANE_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      ide_docs_pane_row_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_pane_row_class_init (IdeDocsPaneRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_docs_pane_row_finalize;
  object_class->get_property = ide_docs_pane_row_get_property;
  object_class->set_property = ide_docs_pane_row_set_property;

  properties [PROP_ITEM] =
    g_param_spec_object ("item",
                         "Item",
                         "The item to be displayed",
                         IDE_TYPE_DOCS_ITEM,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-docs/ui/ide-docs-pane-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDocsPaneRow, title);
}

static void
ide_docs_pane_row_init (IdeDocsPaneRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
