/* gbp-symbol-menu-button.c
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

#define G_LOG_DOMAIN "gbp-symbol-menu-button.h"

#include "gbp-symbol-menu-button.h"
#include "gbp-symbol-tree-builder.h"

struct _GbpSymbolMenuButton
{
  GtkMenuButton  parent_instance;

  /* Owned references */
  IdeSymbolTree  *symbol_tree;

  /* Template references */
  DzlTree        *tree;
  DzlTreeBuilder *tree_builder;
  GtkWidget      *popover;
};

enum {
  PROP_0,
  PROP_SYMBOL_TREE,
  N_PROPS
};

G_DEFINE_TYPE (GbpSymbolMenuButton, gbp_symbol_menu_button, GTK_TYPE_MENU_BUTTON)

static GParamSpec *properties [N_PROPS];

static void
gbp_symbol_menu_button_destroy (GtkWidget *widget)
{
  GbpSymbolMenuButton *self = (GbpSymbolMenuButton *)widget;

  if (self->tree != NULL)
    dzl_tree_set_root (self->tree, NULL);

  g_clear_object (&self->symbol_tree);

  GTK_WIDGET_CLASS (gbp_symbol_menu_button_parent_class)->destroy (widget);
}

static void
gbp_symbol_menu_button_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpSymbolMenuButton *self = GBP_SYMBOL_MENU_BUTTON (object);

  switch (prop_id)
    {
    case PROP_SYMBOL_TREE:
      g_value_set_object (value, gbp_symbol_menu_button_get_symbol_tree (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_menu_button_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpSymbolMenuButton *self = GBP_SYMBOL_MENU_BUTTON (object);

  switch (prop_id)
    {
    case PROP_SYMBOL_TREE:
      gbp_symbol_menu_button_set_symbol_tree (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_menu_button_class_init (GbpSymbolMenuButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gbp_symbol_menu_button_get_property;
  object_class->set_property = gbp_symbol_menu_button_set_property;

  widget_class->destroy = gbp_symbol_menu_button_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/symbol-tree-plugin/gbp-symbol-menu-button.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpSymbolMenuButton, popover);
  gtk_widget_class_bind_template_child (widget_class, GbpSymbolMenuButton, tree);
  gtk_widget_class_bind_template_child (widget_class, GbpSymbolMenuButton, tree_builder);

  properties [PROP_SYMBOL_TREE] =
    g_param_spec_object ("symbol-tree",
                         "Symbol Tree",
                         "The symbol tree to be visualized",
                         IDE_TYPE_SYMBOL_TREE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  g_type_ensure (GBP_TYPE_SYMBOL_TREE_BUILDER);
}

static void
gbp_symbol_menu_button_init (GbpSymbolMenuButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gbp_symbol_menu_button_get_symbol_tree:
 * @self: a #GbpSymbolMenuButton
 *
 * Gets the #IdeSymbolTree displayed by the popover.
 *
 * Returns: (transfer none) (nullable): An #IdeSymbolTree or %NULL
 *
 * Since: 3.26
 */
IdeSymbolTree *
gbp_symbol_menu_button_get_symbol_tree (GbpSymbolMenuButton *self)
{
  g_return_val_if_fail (GBP_IS_SYMBOL_MENU_BUTTON (self), NULL);

  return self->symbol_tree;
}

/**
 * gbp_symbol_menu_button_set_symbol_tree:
 * @self: a #GbpSymbolMenuButton
 *
 * Sets the symbol tree to be displayed by the popover.
 *
 * Since: 3.26
 */
void
gbp_symbol_menu_button_set_symbol_tree (GbpSymbolMenuButton *self,
                                        IdeSymbolTree       *symbol_tree)
{
  g_return_if_fail (GBP_IS_SYMBOL_MENU_BUTTON (self));
  g_return_if_fail (!symbol_tree || IDE_IS_SYMBOL_TREE (symbol_tree));

  if (g_set_object (&self->symbol_tree, symbol_tree))
    {
      DzlTreeNode *root = dzl_tree_node_new ();

      if (symbol_tree != NULL)
        dzl_tree_node_set_item (root, G_OBJECT (symbol_tree));
      dzl_tree_set_root (self->tree, root);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SYMBOL_TREE]);
    }
}
