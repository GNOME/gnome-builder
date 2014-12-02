/* gb-tab.c
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

#define G_LOG_DOMAIN "tab"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-notebook.h"
#include "gb-tab.h"

struct _GbTabPrivate
{
  GtkWidget *content;
  GtkBox    *controls;
  GtkWidget *footer_box;
  GtkWidget *header_box;

  gchar    *icon_name;
  gchar    *title;

  guint     dirty : 1;
};

enum {
  PROP_0,
  PROP_DIRTY,
  PROP_ICON_NAME,
  PROP_TITLE,
  LAST_PROP
};

enum {
  CLOSE,
  FREEZE_DRAG,
  THAW_DRAG,
  LAST_SIGNAL
};

static void buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_EXTENDED (GbTab, gb_tab, GTK_TYPE_BOX, 0,
                        G_ADD_PRIVATE (GbTab)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                               buildable_init))

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

GtkWidget *
gb_tab_get_header_area (GbTab *tab)
{
  g_return_val_if_fail (GB_IS_TAB (tab), NULL);

  return tab->priv->header_box;
}

GtkWidget *
gb_tab_get_controls (GbTab *tab)
{
  g_return_val_if_fail (GB_IS_TAB (tab), NULL);

  return GTK_WIDGET (tab->priv->controls);
}

GtkWidget *
gb_tab_get_footer_area (GbTab *tab)
{
  g_return_val_if_fail (GB_IS_TAB (tab), NULL);

  return tab->priv->footer_box;
}

GtkWidget *
gb_tab_get_content_area (GbTab *tab)
{
  g_return_val_if_fail (GB_IS_TAB (tab), NULL);

  return tab->priv->content;
}

gboolean
gb_tab_get_dirty (GbTab *tab)
{
  g_return_val_if_fail (GB_IS_TAB (tab), FALSE);

  return tab->priv->dirty;
}

void
gb_tab_set_dirty (GbTab    *tab,
                  gboolean  dirty)
{
  g_return_if_fail (GB_IS_TAB (tab));

  tab->priv->dirty = dirty;
  g_object_notify_by_pspec (G_OBJECT (tab), gParamSpecs [PROP_DIRTY]);
}

void
gb_tab_close (GbTab *tab)
{
  g_return_if_fail (GB_IS_TAB (tab));

  g_signal_emit (tab, gSignals [CLOSE], 0);
}

const gchar *
gb_tab_get_icon_name (GbTab *tab)
{
  g_return_val_if_fail (GB_IS_TAB (tab), NULL);

  return tab->priv->icon_name;
}

void
gb_tab_set_icon_name (GbTab       *tab,
                      const gchar *icon_name)
{
  g_return_if_fail (GB_IS_TAB (tab));

  g_free (tab->priv->icon_name);
  tab->priv->icon_name = g_strdup (icon_name);
  g_object_notify_by_pspec (G_OBJECT (tab), gParamSpecs[PROP_ICON_NAME]);
}

const gchar *
gb_tab_get_title (GbTab *tab)
{
  g_return_val_if_fail (GB_IS_TAB (tab), NULL);

  return tab->priv->title;
}

void
gb_tab_set_title (GbTab       *tab,
                  const gchar *title)
{
  g_return_if_fail (GB_IS_TAB (tab));

  g_free (tab->priv->title);
  tab->priv->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (tab), gParamSpecs[PROP_TITLE]);
}

void
gb_tab_freeze_drag (GbTab *tab)
{
  g_return_if_fail (GB_IS_TAB (tab));

  g_signal_emit (tab, gSignals[FREEZE_DRAG], 0);
}

void
gb_tab_thaw_drag (GbTab *tab)
{
  g_return_if_fail (GB_IS_TAB (tab));

  g_signal_emit (tab, gSignals[THAW_DRAG], 0);
}

static void
gb_tab_finalize (GObject *object)
{
  GbTab *tab = (GbTab *)object;

  g_clear_pointer (&tab->priv->icon_name, g_free);
  g_clear_pointer (&tab->priv->title, g_free);
  g_clear_object (&tab->priv->controls);

  G_OBJECT_CLASS (gb_tab_parent_class)->finalize (object);
}

static void
gb_tab_get_property (GObject    *object,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  GbTab *tab = GB_TAB (object);

  switch (prop_id)
    {
    case PROP_DIRTY:
      g_value_set_boolean (value, gb_tab_get_dirty (tab));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, gb_tab_get_icon_name (tab));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gb_tab_get_title (tab));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_tab_set_property (GObject      *object,
                     guint         prop_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
  GbTab *tab = GB_TAB (object);

  switch (prop_id)
    {
    case PROP_DIRTY:
      gb_tab_set_dirty (tab, g_value_get_boolean (value));
      break;

    case PROP_ICON_NAME:
      gb_tab_set_icon_name (tab, g_value_get_string (value));
      break;

    case PROP_TITLE:
      gb_tab_set_title (tab, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_tab_class_init (GbTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_tab_finalize;
  object_class->get_property = gb_tab_get_property;
  object_class->set_property = gb_tab_set_property;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-tab.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbTab, content);
  gtk_widget_class_bind_template_child_private (widget_class, GbTab, footer_box);
  gtk_widget_class_bind_template_child_private (widget_class, GbTab, header_box);

  gParamSpecs [PROP_DIRTY] =
    g_param_spec_boolean ("dirty",
                         _("Dirty"),
                         _("If the tab has dirty state."),
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DIRTY,
                                   gParamSpecs [PROP_DIRTY]);

  gParamSpecs[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         _("Icon Name"),
                         _("The name of the icon to display."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ICON_NAME,
                                   gParamSpecs[PROP_ICON_NAME]);

  gParamSpecs[PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The title of the tab."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TITLE,
                                   gParamSpecs[PROP_TITLE]);

  gSignals [CLOSE] =
    g_signal_new ("close",
                  GB_TYPE_TAB,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTabClass, close),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gSignals [FREEZE_DRAG] =
    g_signal_new ("freeze-drag",
                  GB_TYPE_TAB,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTabClass, freeze_drag),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gSignals [THAW_DRAG] =
    g_signal_new ("thaw-drag",
                  GB_TYPE_TAB,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTabClass, thaw_drag),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static void
gb_tab_init (GbTab *tab)
{
  tab->priv = gb_tab_get_instance_private (tab);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (tab),
                                  GTK_ORIENTATION_VERTICAL);

  tab->priv->controls =
    g_object_new (GTK_TYPE_BOX,
                  "orientation", GTK_ORIENTATION_HORIZONTAL,
                  "visible", TRUE,
                  NULL);
  g_object_ref_sink (tab->priv->controls);

  gtk_widget_init_template (GTK_WIDGET (tab));
}

static GObject *
gb_tab_get_internal_child (GtkBuildable *buildable,
                           GtkBuilder   *builder,
                           const gchar  *childname)
{
  GbTab *tab = (GbTab *)buildable;

  g_return_val_if_fail (GB_IS_TAB (tab), NULL);

  if (g_strcmp0 (childname, "content") == 0)
    return G_OBJECT (tab->priv->content);
  else if (g_strcmp0 (childname, "header") == 0)
    return G_OBJECT (tab->priv->header_box);
  else if (g_strcmp0 (childname, "footer") == 0)
    return G_OBJECT (tab->priv->footer_box);
  else if (g_strcmp0 (childname, "controls") == 0)
    return G_OBJECT (tab->priv->controls);

  return NULL;
}

static void
buildable_init (GtkBuildableIface *iface)
{
  iface->get_internal_child = gb_tab_get_internal_child;
}
