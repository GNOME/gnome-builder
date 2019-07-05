/* ide-docs-search-row.c
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

#define G_LOG_DOMAIN "ide-docs-search-row"

#include "config.h"

#include "ide-docs-search-row.h"

#define DEFAULT_MAX_CHILDREN 3

struct _IdeDocsSearchRow
{
  DzlListBoxRow parent_instance;

  IdeDocsItem *item;

  /* Template Widgets */
  GtkLabel *label;
  GtkImage *image;
};

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

G_DEFINE_TYPE (IdeDocsSearchRow, ide_docs_search_row, DZL_TYPE_LIST_BOX_ROW)

static GParamSpec *properties [N_PROPS];

static void
ide_docs_search_row_set_item (IdeDocsSearchRow *self,
                              IdeDocsItem      *item)
{
  g_autofree gchar *with_size = NULL;
  GtkStyleContext *style_context;
  const gchar *icon_name;
  const gchar *title;
  IdeDocsItemKind kind;
  gboolean use_markup;

  g_return_if_fail (IDE_IS_DOCS_SEARCH_ROW (self));
  g_return_if_fail (!item || IDE_IS_DOCS_ITEM (item));

  g_set_object (&self->item, item);

  if (item == NULL)
    return;

  kind = ide_docs_item_get_kind (self->item);

  switch (kind)
    {
    case IDE_DOCS_ITEM_KIND_FUNCTION:
      icon_name = "lang-function-symbolic";
      break;

    case IDE_DOCS_ITEM_KIND_METHOD:
      icon_name = "lang-method-symbolic";
      break;

    case IDE_DOCS_ITEM_KIND_CLASS:
      icon_name = "lang-class-symbolic";
      break;

    case IDE_DOCS_ITEM_KIND_ENUM:
      icon_name = "lang-enum-symbolic";
      break;

    case IDE_DOCS_ITEM_KIND_CONSTANT:
      icon_name = "lang-enum-value-symbolic";
      break;

    case IDE_DOCS_ITEM_KIND_MACRO:
      icon_name = "lang-define-symbolic";
      break;

    case IDE_DOCS_ITEM_KIND_STRUCT:
      icon_name = "lang-struct-symbolic";
      break;

    case IDE_DOCS_ITEM_KIND_UNION:
      icon_name = "lang-union-symbolic";
      break;

    case IDE_DOCS_ITEM_KIND_PROPERTY:
      icon_name = "lang-variable-symbolic";
      break;

    case IDE_DOCS_ITEM_KIND_BOOK:
    case IDE_DOCS_ITEM_KIND_CHAPTER:
    case IDE_DOCS_ITEM_KIND_COLLECTION:
    case IDE_DOCS_ITEM_KIND_MEMBER:
    case IDE_DOCS_ITEM_KIND_NONE:
    case IDE_DOCS_ITEM_KIND_SIGNAL:
    default:
      icon_name = NULL;
      break;
    }

  gtk_label_set_use_markup (self->label, FALSE);

  if ((title = ide_docs_item_get_display_name (self->item)))
    use_markup = TRUE;
  else
    title = ide_docs_item_get_title (self->item);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (kind == IDE_DOCS_ITEM_KIND_BOOK || ide_docs_item_has_child (item))
    {
      guint n_children = ide_docs_item_get_n_children (item);

      gtk_style_context_add_class (style_context, "header");

      if (n_children > DEFAULT_MAX_CHILDREN)
        title = with_size = g_strdup_printf ("%s     +%u", title, n_children - DEFAULT_MAX_CHILDREN);
    }
  else
    {
      gtk_style_context_remove_class (style_context, "header");
    }

  g_object_set (self->image, "icon-name", icon_name, NULL);

  gtk_label_set_label (self->label, title);
  gtk_label_set_use_markup (self->label, use_markup);
}

static void
ide_docs_search_row_finalize (GObject *object)
{
  IdeDocsSearchRow *self = (IdeDocsSearchRow *)object;

  g_clear_object (&self->item);

  G_OBJECT_CLASS (ide_docs_search_row_parent_class)->finalize (object);
}

static void
ide_docs_search_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeDocsSearchRow *self = IDE_DOCS_SEARCH_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_search_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeDocsSearchRow *self = IDE_DOCS_SEARCH_ROW (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      ide_docs_search_row_set_item (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_search_row_class_init (IdeDocsSearchRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_docs_search_row_finalize;
  object_class->get_property = ide_docs_search_row_get_property;
  object_class->set_property = ide_docs_search_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-docs/ui/ide-docs-search-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchRow, image);
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchRow, label);

  properties [PROP_ITEM] =
    g_param_spec_object ("item",
                         "Item",
                         "The item to display",
                         IDE_TYPE_DOCS_ITEM,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_docs_search_row_init (IdeDocsSearchRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_docs_search_row_new (IdeDocsItem *item)
{
  g_return_val_if_fail (IDE_IS_DOCS_ITEM (item), NULL);

  return g_object_new (IDE_TYPE_DOCS_SEARCH_ROW,
                       "item", item,
                       NULL);
}

IdeDocsItem *
ide_docs_search_row_get_item (IdeDocsSearchRow *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_SEARCH_ROW (self), NULL);

  return self->item;
}
