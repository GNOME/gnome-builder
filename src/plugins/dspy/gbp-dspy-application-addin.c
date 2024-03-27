/* gbp-dspy-application-addin.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-dspy-application-addin"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-dspy-application-addin.h"

struct _GbpDspyApplicationAddin
{
  GObject parent_instance;
};

static GAppInfo *
find_app_info (void)
{
  const char * const *alternates = IDE_STRV_INIT ("org.gnome.dspy.desktop",
                                                  "org.gnome.dspy.devel.desktop",
                                                  "org.gnome.Builder.dspy.desktop",
                                                  "org.gnome.Builder.devel.dspy.desktop");
  const char *preferred;
  g_autolist(GAppInfo) all = NULL;
  GAppInfo *found = NULL;

#ifdef DEVELOPMENT_BUILD
  if (ide_is_flatpak ())
    preferred = "org.gnome.Builder.Devel.dspy.desktop";
  else
    preferred = "org.gnome.dspy.desktop";
#else
  if (ide_is_flatpak ())
    preferred = "org.gnome.Builder.dspy.desktop";
  else
    preferred = "org.gnome.dspy.desktop";
#endif

  all = g_app_info_get_all ();

  for (const GList *iter = all; iter; iter = iter->next)
    {
      GAppInfo *app_info = iter->data;
      const char *app_id = g_app_info_get_id (app_info);

      if (ide_str_equal0 (app_id, preferred))
        {
          found = app_info;
          break;
        }
    }

  if (!found)
    {
      for (const GList *iter = all; iter; iter = iter->next)
        {
          GAppInfo *app_info = iter->data;
          const char *app_id = g_app_info_get_id (app_info);

          if (g_strv_contains (alternates, app_id))
            {
              found = app_info;
              break;
            }
        }
    }

  return found ? g_object_ref (found) : NULL;
}

static void
dspy_action_cb (GSimpleAction *action,
                GVariant      *param,
                gpointer       user_data)
{
  g_autoptr(GdkAppLaunchContext) context = NULL;
  g_autoptr(GAppInfo) app_info = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DSPY_APPLICATION_ADDIN (user_data));

  if (!(app_info = find_app_info ()))
    return;

  context = gdk_display_get_app_launch_context (gdk_display_get_default ());
  if (!g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (context), &error))
    g_warning ("Failed to launch d-spy: %s", error->message);
}

static GActionEntry actions[] = {
  { "dspy", dspy_action_cb },
};

static void
gbp_dspy_application_addin_load (IdeApplicationAddin *addin,
                                 IdeApplication      *application)
{
  g_autoptr(GAppInfo) app_info = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  app_info = find_app_info ();

  if (app_info != NULL)
    g_action_map_add_action_entries (G_ACTION_MAP (application),
                                     actions,
                                     G_N_ELEMENTS (actions),
                                     addin);
}

static void
gbp_dspy_application_addin_unload (IdeApplicationAddin *addin,
                                   IdeApplication      *application)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (application), actions[i].name);
}

static void
app_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_dspy_application_addin_load;
  iface->unload = gbp_dspy_application_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpDspyApplicationAddin, gbp_dspy_application_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, app_addin_iface_init))

static void
gbp_dspy_application_addin_class_init (GbpDspyApplicationAddinClass *klass)
{
}

static void
gbp_dspy_application_addin_init (GbpDspyApplicationAddin *self)
{
}
