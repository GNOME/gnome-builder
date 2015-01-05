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

#include "gb-preferences-page.h"
#include "gb-log.h"
#include "gb-string.h"

struct _GbPreferencesPagePrivate
{
  GHashTable *widgets;
  gchar      *title;
  gboolean    active;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesPage, gb_preferences_page,
                            GTK_TYPE_BIN)

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

  g_return_val_if_fail (GB_IS_PREFERENCES_PAGE (page), 0);

  if (!keywords || (g_strv_length ((gchar **)keywords) == 0))
    {
      g_hash_table_foreach (page->priv->widgets, (GHFunc)gtk_widget_show, NULL);
      return G_MAXUINT;
    }

  size = g_strv_length ((gchar **)keywords) + 1;
  needle = g_new0 (gchar *, size);

  for (i = 0; keywords [i]; i++)
    needle [i] = g_utf8_strdown (keywords [i], -1);

  g_hash_table_iter_init (&iter, page->priv->widgets);

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
                                             GtkWidget         *first_widget,
                                             ...)
{
  GtkWidget *widget = first_widget;
  va_list args;
  GQuark q;
  gchar *downcase;

  ENTRY;

  g_return_if_fail (GB_IS_PREFERENCES_PAGE (page));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  downcase = g_utf8_strdown (keywords, -1);
  q = g_quark_from_string (downcase);
  g_free (downcase);

  va_start (args, first_widget);
  do
    g_hash_table_insert (page->priv->widgets, widget, GINT_TO_POINTER (q));
  while ((widget = va_arg (args, GtkWidget *)));
  va_end (args);

  EXIT;
}

const gchar *
gb_preferences_page_get_title (GbPreferencesPage *page)
{
  g_return_val_if_fail (GB_IS_PREFERENCES_PAGE (page), NULL);

  return page->priv->title;
}

void
gb_preferences_page_set_title (GbPreferencesPage *page,
                               const gchar       *title)
{
  g_return_if_fail (GB_IS_PREFERENCES_PAGE (page));

  if (!gb_str_equal0 (title, page->priv->title))
    {
      g_free (page->priv->title);
      page->priv->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (page),
                                gParamSpecs [PROP_TITLE]);
    }
}

gboolean
gb_preferences_page_get_active (GbPreferencesPage *page)
{
  g_return_val_if_fail (GB_IS_PREFERENCES_PAGE (page), FALSE);

  return page->priv->active;
}

void
gb_preferences_page_set_active (GbPreferencesPage *page,
                                gboolean	   active)
{
  g_return_if_fail (GB_IS_PREFERENCES_PAGE (page));

  page->priv->active = active;
}

static void
gb_preferences_page_finalize (GObject *object)
{
  GbPreferencesPagePrivate *priv = GB_PREFERENCES_PAGE (object)->priv;

  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->widgets, g_hash_table_unref);

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
  g_object_class_install_property (object_class, PROP_TITLE,
                                   gParamSpecs [PROP_TITLE]);
}

static void
gb_preferences_page_init (GbPreferencesPage *self)
{
  self->priv = gb_preferences_page_get_instance_private (self);
  self->priv->widgets = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                               NULL, NULL);
  /* Make it active by default. If some page has to be disabled
   * let the preferences window make it disabled.
   */
  self->priv->active = TRUE;
}
