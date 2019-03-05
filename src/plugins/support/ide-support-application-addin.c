/* ide-support-application-addin.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>
#include <libide-gui.h>

#include "ide-support.h"
#include "ide-support-application-addin.h"

struct _IdeSupportApplicationAddin
{
  GObject parent_instance;
};

static void application_addin_iface_init (IdeApplicationAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeSupportApplicationAddin,
                        ide_support_application_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN,
                                               application_addin_iface_init))

static void
ide_support_application_addin_class_init (IdeSupportApplicationAddinClass *klass)
{
}

static void
ide_support_application_addin_init (IdeSupportApplicationAddin *addin)
{
}

static void
generate_support_activate (GSimpleAction              *action,
                           GVariant                   *variant,
                           IdeSupportApplicationAddin *self)
{
  g_autoptr(GFile) file = NULL;
  GtkWidget *dialog;
  gchar *text = NULL;
  GList *windows;
  GError *error = NULL;
  gchar *str = NULL;
  gchar *log_path = NULL;
  gchar *name = NULL;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_SUPPORT_APPLICATION_ADDIN (self));

  name = g_strdup_printf ("gnome-builder-%u.log", (int)getpid ());
  log_path = g_build_filename (g_get_home_dir (), name, NULL);
  g_free (name);

  file = g_file_new_for_path (log_path);

  windows = gtk_application_get_windows (GTK_APPLICATION (IDE_APPLICATION_DEFAULT));

  str = ide_get_support_log ();

  if (!g_file_set_contents (log_path, str, -1, &error))
    {
      g_printerr ("%s\n", error->message);
      goto cleanup;
    }

  text = g_strdup_printf (_("The support log file has been written to “%s”. "
                            "Please provide this file as an attachment on "
                            "your bug report or support request."),
                            log_path);

  g_message ("%s", text);

  dialog = gtk_message_dialog_new (windows ? windows->data : NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "%s", text);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  ide_gtk_window_present (GTK_WINDOW (dialog));

  dzl_file_manager_show (file, NULL);

cleanup:
  g_free (text);
  g_clear_error (&error);
  g_free (str);
  g_free (log_path);
}

static void
ide_support_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  GSimpleAction *action;

  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  action = g_simple_action_new ("generate-support", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (generate_support_activate), addin);
  g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));
}

static void
ide_support_application_addin_unload (IdeApplicationAddin *addin,
                                      IdeApplication      *application)
{
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  g_action_map_remove_action (G_ACTION_MAP (application), "generate-support");
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = ide_support_application_addin_load;
  iface->unload = ide_support_application_addin_unload;
}
