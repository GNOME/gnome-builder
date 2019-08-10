/* ide-application-actions.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-application-addins"
#define DOCS_URI "https://builder.readthedocs.io"

#include "config.h"

#include <glib/gi18n.h>
#include <ide-build-ident.h>
#include <libide-projects.h>

#include "ide-application.h"
#include "ide-application-credits.h"
#include "ide-application-private.h"
#include "ide-gui-global.h"
#include "ide-preferences-window.h"
#include "ide-shortcuts-window-private.h"

static void
ide_application_actions_preferences (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  IdeApplication *self = user_data;
  GtkWindow *toplevel = NULL;
  GtkWindow *window;
  GList *windows;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_APPLICATION (self));

  /* Locate a toplevel for a transient-for property, or a previous
   * preferences window to display.
   */
  windows = gtk_application_get_windows (GTK_APPLICATION (self));
  for (; windows != NULL; windows = windows->next)
    {
      GtkWindow *win = windows->data;

      if (IDE_IS_PREFERENCES_WINDOW (win))
        {
          ide_gtk_window_present (win);
          return;
        }

      if (toplevel == NULL && IDE_IS_WORKBENCH (win))
        toplevel = win;
    }

  /* Create a new window for preferences, with enough space for
   * 2 columns of preferences. The window manager will automatically
   * maximize the window if necessary.
   */
  window = g_object_new (IDE_TYPE_PREFERENCES_WINDOW,
                         "transient-for", toplevel,
                         "default-width", 1300,
                         "default-height", 800,
                         "title", _("Builder — Preferences"),
                         "window-position", GTK_WIN_POS_CENTER_ON_PARENT,
                         NULL);
  gtk_application_add_window (GTK_APPLICATION (self), window);
  ide_gtk_window_present (window);

  IDE_EXIT;
}

static void
ide_application_actions_quit (GSimpleAction *action,
                              GVariant      *param,
                              gpointer       user_data)
{
  IdeApplication *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  /* TODO: Ask all workbenches to cleanup */

  g_application_quit (G_APPLICATION (self));

  IDE_EXIT;
}

static void
ide_application_actions_about (GSimpleAction *action,
                               GVariant      *param,
                               gpointer       user_data)
{
  IdeApplication *self = user_data;
  g_autoptr(GString) version = NULL;
  GtkDialog *dialog;
  GtkWindow *parent = NULL;
  GList *iter;
  GList *windows;

  g_assert (IDE_IS_APPLICATION (self));

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (iter = windows; iter; iter = iter->next)
    {
      if (IDE_IS_WORKSPACE (iter->data))
        {
          parent = iter->data;
          break;
        }
    }

  version = g_string_new (NULL);

  if (!g_str_equal (IDE_BUILD_TYPE, "release"))
    g_string_append (version, IDE_BUILD_IDENTIFIER);
  else
    g_string_append (version, PACKAGE_VERSION);

  if (g_strcmp0 (IDE_BUILD_CHANNEL, "other") != 0)
    g_string_append (version, "\n" IDE_BUILD_CHANNEL);

  dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         "artists", ide_application_credits_artists,
                         "authors", ide_application_credits_authors,
                         "comments", _("An IDE for GNOME"),
                         "copyright", "© 2014–2019 Christian Hergert, et al.",
                         "documenters", ide_application_credits_documenters,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", "org.gnome.Builder",
                         "modal", TRUE,
                         "program-name", _("GNOME Builder"),
                         "transient-for", parent,
                         "translator-credits", _("translator-credits"),
                         "use-header-bar", TRUE,
                         "version", version->str,
                         "website", "https://wiki.gnome.org/Apps/Builder",
                         "website-label", _("Learn more about GNOME Builder"),
                         NULL);
  gtk_about_dialog_add_credit_section (GTK_ABOUT_DIALOG (dialog),
                                       _("Funded By"),
                                       ide_application_credits_funders);

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  ide_gtk_window_present (GTK_WINDOW (dialog));
}

static void
ide_application_actions_help_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GNetworkMonitor *monitor = (GNetworkMonitor *)object;
  g_autoptr(IdeApplication) self = user_data;
  GtkWindow *focused_window;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  focused_window = gtk_application_get_active_window (GTK_APPLICATION (self));

  /*
   * If we can reach the documentation website, prefer showing up-to-date
   * documentation from the website.
   */
  if (g_network_monitor_can_reach_finish (monitor, result, NULL))
    {
      g_autoptr(GError) error = NULL;

      g_debug ("Can reach documentation site, opening online");
      if (!ide_gtk_show_uri_on_window (focused_window, DOCS_URI, g_get_monotonic_time (), &error))
        g_warning ("Failed to display documentation: %s", error->message);

      IDE_EXIT;
    }

  g_debug ("Cannot reach online documentation, trying locally");

  /*
   * We failed to reach the online site for some reason (offline, transient error, etc),
   * so instead try to load the local documentation.
   */
  if (g_file_test (PACKAGE_DOCDIR"/en/index.html", G_FILE_TEST_IS_REGULAR))
    {
      g_autofree gchar *file_base = NULL;
      g_autofree gchar *uri = NULL;
      g_autoptr(GError) error = NULL;

      if (ide_is_flatpak ())
        file_base = ide_get_relocatable_path ("/share/doc/gnome-builder");
      else
        file_base = g_strdup (PACKAGE_DOCDIR);

      uri = g_strdup_printf ("file://%s/en/index.html", file_base);

      g_debug ("Documentation URI: %s", uri);

      if (!ide_gtk_show_uri_on_window (focused_window, uri, g_get_monotonic_time (), &error))
        g_warning ("Failed to load documentation: %s", error->message);

      IDE_EXIT;
    }

  g_debug ("No locally installed documentation to display");

  IDE_EXIT;
}

static void
ide_application_actions_help (GSimpleAction *action,
                              GVariant      *param,
                              gpointer       user_data)
{
  IdeApplication *self = user_data;
  g_autoptr(GSocketConnectable) network_address = NULL;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_APPLICATION (self));

  /*
   * Check for access to the internet. Sadly, we cannot use
   * g_network_monitor_get_network_available() because that does not seem to
   * act correctly on some systems (Ubuntu appears to be one example). So
   * instead, we can asynchronously check if we can reach the peer first.
   */
  network_address = g_network_address_parse_uri (DOCS_URI, 443, NULL);
  g_network_monitor_can_reach_async (g_network_monitor_get_default (),
                                     network_address,
                                     NULL,
                                     ide_application_actions_help_cb,
                                     g_object_ref (self));

  IDE_EXIT;
}

static void
ide_application_actions_shortcuts (GSimpleAction *action,
                                   GVariant      *variant,
                                   gpointer       user_data)
{
  IdeApplication *self = user_data;
  GtkWindow *window;
  GtkWindow *parent = NULL;
  GList *list;

  g_assert (IDE_IS_APPLICATION (self));

  list = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; list; list = list->next)
    {
      window = list->data;

      if (IDE_IS_SHORTCUTS_WINDOW (window))
        {
          ide_gtk_window_present (window);
          return;
        }

      if (IDE_IS_WORKBENCH (window))
        {
          parent = window;
          break;
        }
    }

  window = g_object_new (IDE_TYPE_SHORTCUTS_WINDOW,
                         "application", self,
                         "window-position", GTK_WIN_POS_CENTER,
                         "transient-for", parent,
                         NULL);

  ide_gtk_window_present (GTK_WINDOW (window));
}

static void
ide_application_actions_nighthack (GSimpleAction *action,
                                   GVariant      *variant,
                                   gpointer       user_data)
{
  g_autoptr(GSettings) settings = NULL;

  g_object_set (gtk_settings_get_default (),
                "gtk-application-prefer-dark-theme", TRUE,
                NULL);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_set_string (settings, "style-scheme-name", "builder-dark");
}

static void
ide_application_actions_dayhack (GSimpleAction *action,
                                 GVariant      *variant,
                                 gpointer       user_data)
{
  g_autoptr(GSettings) settings = NULL;

  g_object_set (gtk_settings_get_default (),
                "gtk-application-prefer-dark-theme", FALSE,
                NULL);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_set_string (settings, "style-scheme-name", "builder");
}

static void
ide_application_actions_load_project (GSimpleAction *action,
                                      GVariant      *args,
                                      gpointer       user_data)
{
  IdeApplication *self = user_data;
  g_autoptr(IdeProjectInfo) project_info = NULL;
  g_autofree gchar *filename = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *scheme = NULL;

  g_assert (IDE_IS_APPLICATION (self));

  g_variant_get (args, "s", &filename);

  if ((scheme = g_uri_parse_scheme (filename)))
    file = g_file_new_for_uri (filename);
  else
    file = g_file_new_for_path (filename);

  project_info = ide_project_info_new ();
  ide_project_info_set_file (project_info, file);

  ide_application_open_project_async (self,
                                      project_info,
                                      G_TYPE_INVALID,
                                      NULL, NULL, NULL);
}

static gint
type_compare (gconstpointer a,
              gconstpointer b)
{
  GType *ta = (GType *)a;
  GType *tb = (GType *)b;

  return g_type_get_instance_count (*ta) - g_type_get_instance_count (*tb);
}

static void
ide_application_actions_stats (GSimpleAction *action,
                               GVariant *args,
                               gpointer user_data)
{
  guint n_types = 0;
  g_autofree GType *types = g_type_children (G_TYPE_OBJECT, &n_types);
  GtkScrolledWindow *scroller;
  GtkTextBuffer *buffer;
  GtkTextView *text_view;
  GtkWindow *window;
  gboolean found = FALSE;

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 1000,
                         "default-height", 600,
                         "title", "about:types",
                         NULL);
  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "visible", TRUE,
                           NULL);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (scroller));
  text_view = g_object_new (GTK_TYPE_TEXT_VIEW,
                            "editable", FALSE,
                            "monospace", TRUE,
                            "visible", TRUE,
                            NULL);
  gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (text_view));
  buffer = gtk_text_view_get_buffer (text_view);

  gtk_text_buffer_insert_at_cursor (buffer, "Count | Type\n", -1);
  gtk_text_buffer_insert_at_cursor (buffer, "======+======\n", -1);

  qsort (types, n_types, sizeof (GType), type_compare);

  for (guint i = 0; i < n_types; i++)
    {
      gint count = g_type_get_instance_count (types[i]);

      if (count)
        {
          gchar str[12];

          found = TRUE;

          g_snprintf (str, sizeof str, "%6d", count);
          gtk_text_buffer_insert_at_cursor (buffer, str, -1);
          gtk_text_buffer_insert_at_cursor (buffer, " ", -1);
          gtk_text_buffer_insert_at_cursor (buffer, g_type_name (types[i]), -1);
          gtk_text_buffer_insert_at_cursor (buffer, "\n", -1);
        }
    }

  if (!found)
    gtk_text_buffer_insert_at_cursor (buffer, "No stats were found, was GOBJECT_DEBUG=instance-count set?", -1);

  ide_gtk_window_present (window);
}

static const GActionEntry IdeApplicationActions[] = {
  { "about:types",  ide_application_actions_stats },
  { "about",        ide_application_actions_about },
  { "dayhack",      ide_application_actions_dayhack },
  { "nighthack",    ide_application_actions_nighthack },
  { "load-project", ide_application_actions_load_project, "s"},
  { "preferences",  ide_application_actions_preferences },
  { "quit",         ide_application_actions_quit },
  { "shortcuts",    ide_application_actions_shortcuts },
  { "help",         ide_application_actions_help },
};

void
_ide_application_init_actions (IdeApplication *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   IdeApplicationActions,
                                   G_N_ELEMENTS (IdeApplicationActions),
                                   self);
}
