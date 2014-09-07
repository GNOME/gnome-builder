/* gb-devhelp-tab.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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
#include <webkit2/webkit2.h>

#include "gb-devhelp-tab.h"
#include "gd-tagged-entry.h"

struct _GbDevhelpTabPrivate
{
  WebKitWebView *web_view;
};

enum {
  PROP_0,
  PROP_URI,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDevhelpTab, gb_devhelp_tab, GB_TYPE_TAB)

static GParamSpec *gParamSpecs[LAST_PROP];

static void
gb_devhelp_tab_freeze_drag (GbTab *tab)
{
  GbDevhelpTabPrivate *priv = GB_DEVHELP_TAB (tab)->priv;
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (priv->web_view));
  g_object_set_data_full (G_OBJECT (priv->web_view),
                          "GB_TAB_DRAG_TARGET_LIST",
                          gtk_target_list_ref (target_list),
                          (GDestroyNotify) gtk_target_list_unref);
  gtk_drag_dest_unset (GTK_WIDGET (priv->web_view));
}

static void
gb_devhelp_tab_thaw_drag (GbTab *tab)
{
  GbDevhelpTabPrivate *priv = GB_DEVHELP_TAB (tab)->priv;
  GtkTargetList *target_list;

  target_list = g_object_get_data (G_OBJECT (priv->web_view),
                                   "GB_TAB_DRAG_TARGET_LIST");
  gtk_drag_dest_set (GTK_WIDGET (priv->web_view), 0, 0, 0,
                     (GDK_ACTION_COPY |
                      GDK_ACTION_MOVE |
                      GDK_ACTION_LINK |
                      GDK_ACTION_PRIVATE));
  gtk_drag_dest_set_target_list (GTK_WIDGET (priv->web_view), target_list);
}

static void
gb_devhelp_tab_on_title_changed (GbDevhelpTab  *tab,
                                 GParamSpec    *pspec,
                                 WebKitWebView *web_view)
{
  const gchar *title;

  g_return_if_fail (GB_IS_DEVHELP_TAB (tab));
  g_return_if_fail (WEBKIT_IS_WEB_VIEW (web_view));

  title = webkit_web_view_get_title (web_view);
  gb_tab_set_title (GB_TAB (tab), title);
}

void
gb_devhelp_tab_set_uri (GbDevhelpTab *tab,
                        const gchar  *uri)
{
  g_return_if_fail (GB_IS_DEVHELP_TAB (tab));

  webkit_web_view_load_uri (tab->priv->web_view, uri);
}

static void
gb_devhelp_tab_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_devhelp_tab_parent_class)->finalize (object);
}

static void
gb_devhelp_tab_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbDevhelpTab *tab = GB_DEVHELP_TAB (object);

  switch (prop_id)
    {
    case PROP_URI:
      gb_devhelp_tab_set_uri (tab, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_devhelp_tab_class_init (GbDevhelpTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbTabClass *tab_class = GB_TAB_CLASS (klass);

  object_class->finalize = gb_devhelp_tab_finalize;
  object_class->set_property = gb_devhelp_tab_set_property;

  tab_class->freeze_drag = gb_devhelp_tab_freeze_drag;
  tab_class->thaw_drag = gb_devhelp_tab_thaw_drag;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-devhelp-tab.ui");
  gtk_widget_class_bind_template_child_private (widget_class,
                                                GbDevhelpTab,
                                                web_view);

  gParamSpecs[PROP_URI] =
    g_param_spec_string ("uri",
                         _ ("Uri"),
                         _ ("The uri for the web_view."),
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_URI,
                                   gParamSpecs[PROP_URI]);
}

static void
gb_devhelp_tab_init (GbDevhelpTab *tab)
{
  tab->priv = gb_devhelp_tab_get_instance_private (tab);

  g_type_ensure (GD_TYPE_TAGGED_ENTRY);
  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);

  gtk_widget_init_template (GTK_WIDGET (tab));

  g_signal_connect_object (tab->priv->web_view,
                           "notify::title",
                           G_CALLBACK (gb_devhelp_tab_on_title_changed),
                           tab,
                           G_CONNECT_SWAPPED);
}
