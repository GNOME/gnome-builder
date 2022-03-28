/* ide-gtk.c
 *
 * Copyright 2015-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-gtk"

#include "config.h"

#include <libide-threading.h>

#include "ide-gtk.h"

void
ide_gtk_window_present (GtkWindow *window)
{
  /* TODO: We need the last event time to do this properly. Until then,
   * we'll just fake some timing info to workaround wayland issues.
   */
  gtk_window_present_with_time (window, g_get_monotonic_time () / 1000L);
}

static void
ide_gtk_show_uri_on_window_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    g_warning ("Subprocess failed: %s", error->message);
}

gboolean
ide_gtk_show_uri_on_window (GtkWindow    *window,
                            const gchar  *uri,
                            gint64        timestamp,
                            GError      **error)
{
  g_return_val_if_fail (!window || GTK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  if (ide_is_flatpak ())
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeSubprocess) subprocess = NULL;

      /* We can't currently trust gtk_show_uri_on_window() because it tries
       * to open our HTML page with Builder inside our current flatpak
       * environment! We need to ensure this is fixed upstream, but it's
       * currently unclear how to do so since we register handles for html.
       */

      launcher = ide_subprocess_launcher_new (0);
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      ide_subprocess_launcher_set_clear_env (launcher, FALSE);
      ide_subprocess_launcher_push_argv (launcher, "xdg-open");
      ide_subprocess_launcher_push_argv (launcher, uri);

      if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, error)))
        return FALSE;

      ide_subprocess_wait_async (subprocess,
                                 NULL,
                                 ide_gtk_show_uri_on_window_cb,
                                 NULL);
    }
  else
    {
      /* XXX: Workaround for wayland timestamp issue */
      gtk_show_uri (window, uri, timestamp / 1000L);
    }

  return TRUE;
}
