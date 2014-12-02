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

#define G_LOG_DOMAIN "devhelp-tab"

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

#include "gb-devhelp-tab.h"
#include "gb-log.h"

struct _GbDevhelpTabPrivate
{
  DhAssistantView *assistant_view;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDevhelpTab, gb_devhelp_tab, GB_TYPE_TAB)

enum {
  PROP_0,
  LAST_PROP
};

static DhBookManager *gBookManager;

#if 0
static GParamSpec *gParamSpecs [LAST_PROP];
#endif

GbDevhelpTab *
gb_devhelp_tab_new (void)
{
  return g_object_new (GB_TYPE_DEVHELP_TAB, NULL);
}

void
gb_devhelp_tab_jump_to_keyword (GbDevhelpTab *tab,
                                const gchar  *keyword)
{
  gchar *title;

  ENTRY;

  g_return_if_fail (GB_IS_DEVHELP_TAB (tab));
  g_return_if_fail (keyword);

  dh_assistant_view_search (tab->priv->assistant_view, keyword);

  title = g_strdup_printf (_("Documentation (%s)"), keyword);
  gb_tab_set_title (GB_TAB (tab), title);
  g_free (title);

  EXIT;
}

static void
gb_devhelp_tab_constructed (GObject *object)
{
  GbDevhelpTabPrivate *priv = GB_DEVHELP_TAB (object)->priv;

  G_OBJECT_CLASS (gb_devhelp_tab_parent_class)->constructed (object);

  dh_assistant_view_set_book_manager (priv->assistant_view, gBookManager);
}

static void
gb_devhelp_tab_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_devhelp_tab_parent_class)->finalize (object);
}

static void
gb_devhelp_tab_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
#if 0
  GbDevhelpTab *self = GB_DEVHELP_TAB (object);
#endif

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_devhelp_tab_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
#if 0
  GbDevhelpTab *self = GB_DEVHELP_TAB (object);
#endif

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_devhelp_tab_class_init (GbDevhelpTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_devhelp_tab_constructed;
  object_class->finalize = gb_devhelp_tab_finalize;
  object_class->get_property = gb_devhelp_tab_get_property;
  object_class->set_property = gb_devhelp_tab_set_property;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-devhelp-tab.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbDevhelpTab, assistant_view);

  /* TODO:
   *
   * This type of stuff should be loaded during init just in case we ever
   * reach the point of having to have a "splash screen".  Ugh, just the
   * thought of it.
   */
  gBookManager = dh_book_manager_new ();
  dh_book_manager_populate (gBookManager);
}

static void
gb_devhelp_tab_init (GbDevhelpTab *self)
{
  self->priv = gb_devhelp_tab_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
