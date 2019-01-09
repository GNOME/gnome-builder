/* gbp-devhelp-menu-button.c
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

#define G_LOG_DOMAIN "gbp-devhelp-menu-button"

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>
#include <libide-editor.h>

#include "gbp-devhelp-menu-button.h"

struct _GbpDevhelpMenuButton
{
  GtkMenuButton  parent_instance;

  GtkPopover    *popover;
  DhSidebar     *sidebar;
};

G_DEFINE_TYPE (GbpDevhelpMenuButton, gbp_devhelp_menu_button, GTK_TYPE_MENU_BUTTON)

static void
gbp_devhelp_menu_button_pixbuf_data_func (GtkCellLayout   *cell_layout,
                                          GtkCellRenderer *cell,
                                          GtkTreeModel    *tree_model,
                                          GtkTreeIter     *iter,
                                          gpointer         data)
{
  const gchar *icon_name = NULL;
  DhLink *link = NULL;

  g_assert (GTK_IS_CELL_LAYOUT (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_PIXBUF (cell));
  g_assert (GTK_IS_TREE_MODEL (tree_model));
  g_assert (iter != NULL);

  /* link is a G_TYPE_POINTER in the model */
  gtk_tree_model_get (tree_model, iter,
                      DH_KEYWORD_MODEL_COL_LINK, &link,
                      -1);

  if (link != NULL)
    {
      DhLinkType link_type;

      link_type = dh_link_get_link_type (link);

      switch (link_type)
        {
        case DH_LINK_TYPE_PROPERTY:
          icon_name = "lang-struct-field-symbolic";
          break;

        case DH_LINK_TYPE_FUNCTION:
        case DH_LINK_TYPE_SIGNAL:
          icon_name = "lang-function-symbolic";
          break;

        case DH_LINK_TYPE_STRUCT:
          icon_name = "lang-struct-symbolic";
          break;

        case DH_LINK_TYPE_MACRO:
          icon_name = "lang-define-symbolic";
          break;

        case DH_LINK_TYPE_ENUM:
          icon_name = "lang-enum-value-symbolic";
          break;

        case DH_LINK_TYPE_TYPEDEF:
          icon_name = "lang-typedef-symbolic";
          break;

        case DH_LINK_TYPE_BOOK:
        case DH_LINK_TYPE_PAGE:
          icon_name = "org.gnome.Devhelp-symbolic";
          break;

        case DH_LINK_TYPE_KEYWORD:
        default:
          break;
        }
    }

  g_object_set (cell, "icon-name", icon_name, NULL);

  g_clear_pointer (&link, dh_link_unref);
}

static void
find_hitlist_tree_view_cb (GtkWidget *widget,
                           gpointer   user_data)
{
  GtkTreeView **ret = user_data;

  if (*ret != NULL)
    return;

  if (GTK_IS_SCROLLED_WINDOW (widget))
    {
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));

      if (DH_IS_BOOK_TREE (child))
        return;

      if (GTK_IS_TREE_VIEW (child))
        *ret = GTK_TREE_VIEW (child);
    }
}

static GtkTreeView *
find_hitlist_tree_view (DhSidebar *sidebar)
{
  GtkTreeView *ret = NULL;

  g_assert (DH_IS_SIDEBAR (sidebar));

  gtk_container_foreach (GTK_CONTAINER (sidebar),
                         find_hitlist_tree_view_cb,
                         &ret);

  return ret;
}

static void
monkey_patch_devhelp (GbpDevhelpMenuButton *self)
{
  GtkTreeView *tree_view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkTreeModel *model;
  GType column_type;
  GtkSearchEntry *search;

  g_assert (GBP_IS_DEVHELP_MENU_BUTTON (self));

  /*
   * The goal here is to dive into the sidebar and find the treeview.
   * Then we want to get the text column and prepend our own pixbuf
   * renderer to that line. Then with our own cell_data_func, we can
   * render the proper symbolic icon (matching Builder's style) to
   * the link based on the link type.
   */

  if (NULL == (tree_view = find_hitlist_tree_view (self->sidebar)))
    {
      g_warning ("Failed to find sidebar treeview, cannot monkey patch");
      return;
    }

  model = gtk_tree_view_get_model (tree_view);
  column_type = gtk_tree_model_get_column_type (model, DH_KEYWORD_MODEL_COL_LINK);

  if (column_type != DH_TYPE_LINK)
    {
      g_warning ("Link type %s does not match expectation",
                 g_type_name (column_type));
      return;
    }

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "visible", TRUE,
                         NULL);
  gtk_tree_view_insert_column (tree_view, column, 0);

  cell = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
                       "width", 22,
                       "height", 16,
                       "visible", TRUE,
                       "xpad", 6,
                       NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, FALSE);
  gtk_cell_layout_reorder (GTK_CELL_LAYOUT (column), cell, 0);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), cell,
                                      gbp_devhelp_menu_button_pixbuf_data_func,
                                      NULL, NULL);

  /*
   * Now find the GtkSearchEntry and adjust the margins on it to match
   * our style and align the search icon with our icon cell renderer.
   */
  search = dzl_gtk_widget_find_child_typed (GTK_WIDGET (self->sidebar), GTK_TYPE_SEARCH_ENTRY);
  if (search != NULL)
    g_object_set (search,
                  "margin-top", 0,
                  "margin-end", 0,
                  "margin-start", 0,
                  "margin-bottom", 6,
                  NULL);
}

static void
gbp_devhelp_menu_button_link_selected (GbpDevhelpMenuButton *self,
                                       DhLink               *link,
                                       DhSidebar            *sidebar)
{
  g_autofree gchar *uri = NULL;

  g_assert (GBP_IS_DEVHELP_MENU_BUTTON (self));
  g_assert (link != NULL);
  g_assert (DH_IS_SIDEBAR (sidebar));

  uri = dh_link_get_uri (link);

  dzl_gtk_widget_action (GTK_WIDGET (self),
                         "devhelp", "navigate-to",
                         g_variant_new_string (uri));
}

static void
gbp_devhelp_menu_button_class_init (GbpDevhelpMenuButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/plugins/devhelp/gbp-devhelp-menu-button.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpMenuButton, popover);
  gtk_widget_class_bind_template_child (widget_class, GbpDevhelpMenuButton, sidebar);

  g_type_ensure (DH_TYPE_SIDEBAR);
}

static void
gbp_devhelp_menu_button_init (GbpDevhelpMenuButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (self->sidebar,
                            "link-selected",
                            G_CALLBACK (gbp_devhelp_menu_button_link_selected),
                            self);

  monkey_patch_devhelp (self);
}

void
gbp_devhelp_menu_button_search (GbpDevhelpMenuButton *self,
                                const gchar          *keyword)
{
  g_return_if_fail (GBP_IS_DEVHELP_MENU_BUTTON (self));

  gtk_popover_popdown (GTK_POPOVER (self->popover));
  dh_sidebar_set_search_string (self->sidebar, keyword);
  dh_sidebar_set_search_focus (self->sidebar);
}
