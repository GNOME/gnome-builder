/* gbp-devhelp-menu-button.c
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

#define G_LOG_DOMAIN "gbp-devhelp-menu-button"

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "gbp-devhelp-menu-button.h"

struct _GbpDevhelpMenuButton
{
  GtkMenuButton  parent_instance;

  GtkPopover    *popover;
  DhSidebar     *sidebar;
};

G_DEFINE_TYPE (GbpDevhelpMenuButton, gbp_devhelp_menu_button, GTK_TYPE_MENU_BUTTON)

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
                                               "/org/gnome/builder/plugins/devhelp-plugin/gbp-devhelp-menu-button.ui");
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
