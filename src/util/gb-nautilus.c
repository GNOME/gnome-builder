/* gb-nautilus.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "gb-nautilus.h"

gboolean
gb_nautilus_select_file (GtkWidget *widget,
                         GFile     *file,
                         guint32    user_time)
{
  GdkAppLaunchContext *launch_context;
  GdkDisplay *display;
  GdkScreen *screen;
  GAppInfo *app_info;
  GList *files = NULL;
  gboolean ret;

  /*
   * FIXME:
   *
   * Currently, this will select the file in the parent folder. But it does not
   * work for selecting a folder within a parent folder.
   */

  g_return_val_if_fail (!widget || GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  app_info = g_app_info_get_default_for_type ("inode/directory", FALSE);

  if (widget)
    {
      display = gtk_widget_get_display (widget);
      screen = gtk_widget_get_screen (widget);
    }
  else
    {
      display = gdk_display_get_default ();
      screen = gdk_screen_get_default ();
    }

  launch_context = gdk_display_get_app_launch_context (display);
  gdk_app_launch_context_set_screen (launch_context, screen);
  gdk_app_launch_context_set_timestamp (launch_context, user_time);

  files = g_list_prepend (files, file);

  ret = g_app_info_launch (app_info, files, G_APP_LAUNCH_CONTEXT (launch_context), NULL);

  g_list_free (files);
  g_object_unref (launch_context);
  g_object_unref (app_info);

  return ret;
}
