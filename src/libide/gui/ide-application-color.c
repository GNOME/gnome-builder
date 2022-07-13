/* ide-application-color.c
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

#define G_LOG_DOMAIN "ide-application-color"

#include "config.h"

#include <gtksourceview/gtksource.h>

#include "ide-application.h"
#include "ide-application-private.h"
#include "ide-recoloring-private.h"

static void
add_style_name (GPtrArray   *ar,
                const gchar *base,
                gboolean     dark)
{
  g_ptr_array_add (ar, g_strdup_printf ("%s-%s", base, dark ? "dark" : "light"));
}

static gchar *
find_similar_style_scheme (const gchar *name,
                           gboolean     is_dark_mode)
{
  g_autoptr(GPtrArray) attempts = NULL;
  GtkSourceStyleSchemeManager *mgr;
  const gchar * const *scheme_ids;
  const gchar *dash;

  g_assert (name != NULL);

  attempts = g_ptr_array_new_with_free_func (g_free);

  mgr = gtk_source_style_scheme_manager_get_default ();
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (mgr);

  add_style_name (attempts, name, is_dark_mode);

  if ((dash = strrchr (name, '-')))
    {
      if (g_str_equal (dash, "-light") ||
          g_str_equal (dash, "-dark"))
        {
          g_autofree gchar *base = NULL;

          base = g_strndup (name, dash - name);
          add_style_name (attempts, base, is_dark_mode);

          /* Add the base name last so light/dark matches first */
          g_ptr_array_add (attempts, g_steal_pointer (&base));
        }
    }

  /*
   * Instead of using gtk_source_style_scheme_manager_get_scheme(), we get the
   * IDs and look using case insensitive search so that its more likely we get
   * a match when something is named Dark or Light in the id.
   */

  for (guint i = 0; i < attempts->len; i++)
    {
      const gchar *attempt = g_ptr_array_index (attempts, i);

      for (guint j = 0; scheme_ids[j] != NULL; j++)
        {
          if (strcasecmp (attempt, scheme_ids[j]) == 0)
            return g_strdup (scheme_ids[j]);
        }
    }

  return NULL;
}

static void
_ide_application_update_color (IdeApplication *self)
{
  static gboolean ignore_reentrant = FALSE;
  AdwStyleManager *manager;
  g_autofree char *style_variant = NULL;

  g_assert (IDE_IS_APPLICATION (self));

  if (ignore_reentrant)
    return;

  if (self->settings == NULL)
    return;

  ignore_reentrant = TRUE;

  g_assert (G_IS_SETTINGS (self->settings));

  style_variant = g_settings_get_string (self->settings, "style-variant");
  manager = adw_style_manager_get_default ();

  g_debug ("Style variant changed to %s", style_variant);

  if (g_strcmp0 (style_variant, "default") == 0)
    adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_PREFER_LIGHT);
  else if (g_strcmp0 (style_variant, "dark") == 0)
    adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_DARK);
  else
    adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_LIGHT);

  ignore_reentrant = FALSE;
}

static void
_ide_application_update_style_scheme (IdeApplication *self)
{
  AdwStyleManager *manager;
  g_autoptr(GSettings) editor_settings = NULL;
  g_autofree gchar *old_name = NULL;
  g_autofree gchar *new_name = NULL;

  g_assert (IDE_IS_APPLICATION (self));

  manager = adw_style_manager_get_default ();

  /*
   * Now that we have our color up to date, we need to possibly update the
   * color scheme to match the setting. We always do this (and not just when
   * the style-variant changes) so that we pick up changes at startup.
   *
   * Try to locate a corresponding style-scheme for the light/dark switch
   * based on some naming conventions. If found, switch the current style
   * scheme to match.
   */

  editor_settings = g_settings_new ("org.gnome.builder.editor");
  old_name = g_settings_get_string (editor_settings, "style-scheme-name");
  new_name = find_similar_style_scheme (old_name, adw_style_manager_get_dark (manager));

  if (new_name != NULL)
    g_settings_set_string (editor_settings, "style-scheme-name", new_name);
}

static void
ide_application_color_style_scheme_changed_cb (IdeApplication *self,
                                               const char     *key,
                                               GSettings      *editor_settings)
{
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;
  g_autofree char *style_scheme_name = NULL;
  g_autofree char *css = NULL;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (g_strcmp0 (key, "style-scheme-name") == 0);
  g_assert (G_IS_SETTINGS (editor_settings));

  style_scheme_name = g_settings_get_string (editor_settings, key);
  g_debug ("Style scheme changed to %s", style_scheme_name);

  manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (manager, style_scheme_name);

  if (scheme == NULL)
    return;

  css = _ide_recoloring_generate_css (scheme);
  gtk_css_provider_load_from_data (self->recoloring, css ? css : "", -1);
}

void
_ide_application_init_color (IdeApplication *self)
{
  g_autofree char *style_scheme_name = NULL;

  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (G_IS_SETTINGS (self->settings));

  if (g_getenv ("GTK_THEME") == NULL)
    {
      g_autofree char *style_variant = NULL;

      /* We must read "style-variant" to get changed notifications */
      style_variant = g_settings_get_string (self->settings, "style-variant");
      g_debug ("Initialized with style-variant %s", style_variant);

      g_signal_connect_object (self->settings,
                               "changed::style-variant",
                               G_CALLBACK (_ide_application_update_color),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (adw_style_manager_get_default (),
                               "notify::dark",
                               G_CALLBACK (_ide_application_update_style_scheme),
                               self,
                               G_CONNECT_SWAPPED);
    }

  style_scheme_name = g_settings_get_string (self->editor_settings, "style-scheme-name");
  g_debug ("Initialized with style scheme %s", style_scheme_name);
  g_signal_connect_object (self->editor_settings,
                           "changed::style-scheme-name",
                           G_CALLBACK (ide_application_color_style_scheme_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (self->recoloring),
                                              GTK_STYLE_PROVIDER_PRIORITY_THEME+1);

  _ide_application_update_color (self);
  _ide_application_update_style_scheme (self);
  ide_application_color_style_scheme_changed_cb (self, "style-scheme-name", self->editor_settings);
}

gboolean
ide_application_get_dark (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), FALSE);

  return adw_style_manager_get_dark (adw_style_manager_get_default ());
}
