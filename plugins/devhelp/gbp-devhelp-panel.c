/* gbp-devhelp-panel.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "gbp-devhelp-panel.h"
#include "gbp-devhelp-view.h"

struct _GbpDevhelpPanel
{
  PnlDockWidget  parent_instance;

  DhBookManager *books;
  DhSidebar     *sidebar;
};

G_DEFINE_TYPE (GbpDevhelpPanel, gbp_devhelp_panel, PNL_TYPE_DOCK_WIDGET)

enum {
  PROP_0,
  PROP_BOOK_MANAGER,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
gbp_devhelp_panel_find_view (GtkWidget *widget,
                             gpointer   user_data)
{
  GbpDevhelpView **view = user_data;

  if (*view != NULL)
    return;

  if (GBP_IS_DEVHELP_VIEW (widget))
    *view = GBP_DEVHELP_VIEW (widget);
}

static void
gbp_devhelp_panel_link_selected (GbpDevhelpPanel *self,
                                 DhLink          *link,
                                 DhSidebar       *sidebar)
{
  GbpDevhelpView *view = NULL;
  IdePerspective *perspective;
  IdeWorkbench *workbench;
  gchar *uri;

  g_assert (GBP_IS_DEVHELP_PANEL (self));
  g_assert (link != NULL);
  g_assert (DH_IS_SIDEBAR (sidebar));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  perspective = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (IDE_IS_LAYOUT (perspective));

  ide_perspective_views_foreach (perspective, gbp_devhelp_panel_find_view, &view);

  if (view == NULL)
    {
      view = g_object_new (GBP_TYPE_DEVHELP_VIEW,
                           "visible", TRUE,
                           NULL);
      gtk_container_add (GTK_CONTAINER (perspective), GTK_WIDGET (view));
    }

  uri = dh_link_get_uri (link);
  gbp_devhelp_view_set_uri (view, uri);
  g_free (uri);

  ide_workbench_focus (workbench, GTK_WIDGET (view));
}

void
gbp_devhelp_panel_set_uri (GbpDevhelpPanel *self,
                           const gchar     *uri)
{
  g_return_if_fail (GBP_IS_DEVHELP_PANEL (self));
  g_return_if_fail (uri != NULL);

  dh_sidebar_select_uri (self->sidebar, uri);
}

static void
gbp_devhelp_panel_constructed (GObject *object)
{
  GbpDevhelpPanel *self = (GbpDevhelpPanel *)object;
  GtkWidget *entry;

  G_OBJECT_CLASS (gbp_devhelp_panel_parent_class)->constructed (object);

  g_assert (self->books != NULL);

  self->sidebar = DH_SIDEBAR (dh_sidebar_new (self->books));

  entry = ide_widget_find_child_typed (GTK_WIDGET (self->sidebar), GTK_TYPE_ENTRY);
  if (entry != NULL)
    {
      g_object_set (entry, "margin", 0, NULL);
      gtk_container_set_border_width (GTK_CONTAINER (gtk_widget_get_parent (entry)), 0);
    }

  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->sidebar));
  gtk_widget_show (GTK_WIDGET (self->sidebar));

  g_signal_connect_object (self->sidebar,
                           "link-selected",
                           G_CALLBACK (gbp_devhelp_panel_link_selected),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_devhelp_panel_finalize (GObject *object)
{
  GbpDevhelpPanel *self = (GbpDevhelpPanel *)object;

  g_clear_object (&self->books);

  G_OBJECT_CLASS (gbp_devhelp_panel_parent_class)->finalize (object);
}

static void
gbp_devhelp_panel_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbpDevhelpPanel *self = GBP_DEVHELP_PANEL(object);

  switch (prop_id)
    {
    case PROP_BOOK_MANAGER:
      self->books = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_devhelp_panel_class_init (GbpDevhelpPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_devhelp_panel_constructed;
  object_class->finalize = gbp_devhelp_panel_finalize;
  object_class->set_property = gbp_devhelp_panel_set_property;

  gtk_widget_class_set_css_name (widget_class, "devhelppanel");

  properties [PROP_BOOK_MANAGER] =
    g_param_spec_object ("book-manager",
                         "Book Manager",
                         "Book Manager",
                         DH_TYPE_BOOK_MANAGER,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gbp_devhelp_panel_init (GbpDevhelpPanel *self)
{
  g_object_set (self, "title", _("Documentation"), NULL);
}

void
gbp_devhelp_panel_focus_search (GbpDevhelpPanel *self,
                                const gchar     *keyword)
{
  g_return_if_fail (GBP_IS_DEVHELP_PANEL (self));

  dh_sidebar_set_search_focus (self->sidebar);

  if (keyword)
    dh_sidebar_set_search_string (self->sidebar, keyword);
}
