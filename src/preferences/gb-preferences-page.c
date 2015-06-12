/* gb-preferences-page.c
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

#define G_LOG_DOMAIN "prefs-page"

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-preferences-page.h"
#include "gb-preferences-switch.h"
#include "gb-string.h"

typedef struct
{
  GHashTable *widgets;
  GtkBox     *controls;
  gchar      *title;
  gchar      *default_title;
} GbPreferencesPagePrivate;

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_EXTENDED (GbPreferencesPage,
                        gb_preferences_page,
                        GTK_TYPE_BIN,
                        0,
                        G_ADD_PRIVATE (GbPreferencesPage)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                               buildable_iface_init))

enum {
  PROP_0,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static gboolean
gb_preferences_page_match (const gchar *needle,
                           const gchar *haystack)
{
  return !!strstr (haystack, needle);
}

guint
gb_preferences_page_set_keywords (GbPreferencesPage   *page,
                                  const gchar * const *keywords)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  gchar **needle;
  gsize size;
  guint count = 0;
  guint i;
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (page);

  g_return_val_if_fail (GB_IS_PREFERENCES_PAGE (page), 0);

  if (!keywords || (g_strv_length ((gchar **)keywords) == 0))
    {
      g_hash_table_foreach (priv->widgets, (GHFunc)gtk_widget_show, NULL);
      return G_MAXUINT;
    }

  size = g_strv_length ((gchar **)keywords) + 1;
  needle = g_new0 (gchar *, size);

  for (i = 0; keywords [i]; i++)
    needle [i] = g_utf8_strdown (keywords [i], -1);

  g_hash_table_iter_init (&iter, priv->widgets);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *haystack;
      GtkWidget *widget = key;
      gboolean visible = FALSE;
      GQuark q = GPOINTER_TO_INT (value);

      haystack = g_quark_to_string (q);

      for (i = 0; keywords [i]; i++)
        {
          if (gb_preferences_page_match (needle [i], haystack))
            {
              count++;
              visible = TRUE;
              break;
            }
        }

      gtk_widget_set_visible (widget, visible);
    }

  g_strfreev (needle);

  return count;
}

void
gb_preferences_page_set_keywords_for_widget (GbPreferencesPage *page,
                                             const gchar       *keywords,
                                             gpointer           first_widget,
                                             ...)
{
  GtkWidget *widget = first_widget;
  va_list args;
  GQuark q;
  gchar *downcase;
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (page);

  g_return_if_fail (GB_IS_PREFERENCES_PAGE (page));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  downcase = g_utf8_strdown (keywords, -1);
  q = g_quark_from_string (downcase);
  g_free (downcase);

  va_start (args, first_widget);
  do
    g_hash_table_insert (priv->widgets, widget, GINT_TO_POINTER (q));
  while ((widget = va_arg (args, GtkWidget *)));
  va_end (args);
}

/**
 * gb_preferences_page_get_controls:
 * @self: A #GbPreferencesPage.
 *
 * Gets the controls for the preferences page.
 *
 * Returns: (transfer none) (nullable): A #GtkWidget.
 */
GtkWidget *
gb_preferences_page_get_controls (GbPreferencesPage *page)
{
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (page);

  g_return_val_if_fail (GB_IS_PREFERENCES_PAGE (page), NULL);

  return GTK_WIDGET (priv->controls);
}

void
gb_preferences_page_set_title (GbPreferencesPage *page,
                               const gchar       *title)
{
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (page);

  g_return_if_fail (GB_IS_PREFERENCES_PAGE (page));

  if (!gb_str_equal0 (title, priv->title))
    {
      g_free (priv->title);
      priv->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (page),
                                gParamSpecs [PROP_TITLE]);
    }
}

void
gb_preferences_page_reset_title (GbPreferencesPage *page)
{
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (page);

  g_return_if_fail (GB_IS_PREFERENCES_PAGE (page));

  gb_preferences_page_set_title (page, priv->default_title);
}

void
gb_preferences_page_clear_search (GbPreferencesPage *page)
{
  g_return_if_fail (GB_IS_PREFERENCES_PAGE (page));

  if (GB_PREFERENCES_PAGE_GET_CLASS (page)->clear_search)
    return GB_PREFERENCES_PAGE_GET_CLASS (page)->clear_search (page);
}

static const gchar *
gb_preferences_page_get_title (GbPreferencesPage *page)
{
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (page);

  g_return_val_if_fail (GB_IS_PREFERENCES_PAGE (page), NULL);

  return priv->title;
}

static void
gb_preferences_page_constructed (GObject *object)
{
  GbPreferencesPage *self = GB_PREFERENCES_PAGE (object);
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (self);

  g_object_get (object, "title", &priv->default_title, NULL);

  G_OBJECT_CLASS (gb_preferences_page_parent_class)->constructed (object);
}

static void
gb_preferences_page_finalize (GObject *object)
{
  GbPreferencesPage *self = GB_PREFERENCES_PAGE (object);
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->default_title, g_free);
  g_clear_pointer (&priv->widgets, g_hash_table_unref);
  g_clear_object  (&priv->controls);

  G_OBJECT_CLASS (gb_preferences_page_parent_class)->finalize (object);
}

static void
gb_preferences_page_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbPreferencesPage *self = GB_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gb_preferences_page_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_preferences_page_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbPreferencesPage *self = GB_PREFERENCES_PAGE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gb_preferences_page_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_preferences_page_class_init (GbPreferencesPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_preferences_page_constructed;
  object_class->finalize = gb_preferences_page_finalize;
  object_class->get_property = gb_preferences_page_get_property;
  object_class->set_property = gb_preferences_page_set_property;

  gParamSpecs [PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The title for the preferences page."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  g_type_ensure (GB_TYPE_PREFERENCES_SWITCH);
}

static void
gb_preferences_page_init (GbPreferencesPage *self)
{
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (self);
  GtkBox *controls;

  priv->widgets = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

  controls = g_object_new (GTK_TYPE_BOX,
                           "orientation", GTK_ORIENTATION_HORIZONTAL,
                           "visible", TRUE,
                           NULL);
  priv->controls = g_object_ref_sink (controls);
}

static GObject *
gb_preferences_page_get_internal_child (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        const gchar  *childname)
{
  GbPreferencesPage *self = (GbPreferencesPage *)buildable;
  GbPreferencesPagePrivate *priv = gb_preferences_page_get_instance_private (self);

  g_assert (GB_IS_PREFERENCES_PAGE (self));

  if (g_strcmp0 (childname, "controls") == 0)
    return G_OBJECT (priv->controls);

  return NULL;
}


static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->get_internal_child = gb_preferences_page_get_internal_child;
}
