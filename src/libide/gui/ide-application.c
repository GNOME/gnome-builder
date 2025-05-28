/* ide-application.c
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-application"

#include "config.h"

#ifdef __linux__
# include <sys/prctl.h>
#endif

#include <glib/gi18n.h>

#include <libpeas.h>

#include "ide-language-defaults.h"

#include "ide-application.h"
#include "ide-application-addin.h"
#include "ide-application-private.h"
#include "ide-gui-global.h"
#include "ide-primary-workspace.h"
#include "ide-shortcut-manager-private.h"

typedef struct
{
  IdeApplication  *self;
  GFile          **files;
  gint             n_files;
  const gchar     *hint;
} OpenData;

G_DEFINE_FINAL_TYPE (IdeApplication, ide_application, ADW_TYPE_APPLICATION)

enum {
  PROP_0,
  PROP_STYLE_SCHEME,
  PROP_SYSTEM_FONT,
  PROP_SYSTEM_FONT_NAME,
  N_PROPS
};

enum {
  SHOW_HELP,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
ide_application_add_platform_data (GApplication    *app,
                                   GVariantBuilder *builder)
{
  IdeApplication *self = (IdeApplication *)app;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (self->argv != NULL);

  G_APPLICATION_CLASS (ide_application_parent_class)->add_platform_data (app, builder);

  g_variant_builder_add (builder,
                         "{sv}",
                         "gnome-builder-version",
                         g_variant_new_string (IDE_VERSION_S));
  g_variant_builder_add (builder,
                         "{sv}",
                         "argv",
                         g_variant_new_strv ((const gchar * const *)self->argv, -1));
}

static gint
ide_application_command_line (GApplication            *app,
                              GApplicationCommandLine *cmdline)
{
  IdeApplication *self = (IdeApplication *)app;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  /* Allow plugins to handle command-line */
  _ide_application_command_line (self, cmdline);

  return G_APPLICATION_CLASS (ide_application_parent_class)->command_line (app, cmdline);
}

static gboolean
ide_application_local_command_line (GApplication   *app,
                                    gchar        ***arguments,
                                    gint           *exit_status)
{
  IdeApplication *self = (IdeApplication *)app;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (arguments != NULL);
  g_assert (exit_status != NULL);
  g_assert (self->argv == NULL);

  /* Save these for later, to use by cmdline addins */
  self->argv = g_strdupv (*arguments);

  return G_APPLICATION_CLASS (ide_application_parent_class)->local_command_line (app, arguments, exit_status);
}

G_GNUC_NULL_TERMINATED
static gboolean
ide_application_load_all_typelibs (GError **error, ...)
{
  g_autoptr(GString) msg = g_string_new (NULL);
  const char *typelib;
  gboolean had_failure = FALSE;
  va_list args;

  va_start (args, error);
  while ((typelib = va_arg (args, const char *)))
    {
      const char *version = va_arg (args, const char *);
      g_autoptr(GError) local_error = NULL;

      if (!gi_repository_require (ide_get_gir_repository (), typelib, version, 0, &local_error))
        {
          if (msg->len)
            g_string_append (msg, "; ");
          g_string_append (msg, local_error->message);
          had_failure = TRUE;
        }
    }
  va_end (args);

  if (had_failure)
    {
      g_set_error_literal (error,
                           GI_REPOSITORY_ERROR,
                           GI_REPOSITORY_ERROR_TYPELIB_NOT_FOUND,
                           msg->str);
      return FALSE;
    }

  return TRUE;
}

static void
ide_application_load_typelibs (IdeApplication *self)
{
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));

  gi_repository_prepend_search_path (ide_get_gir_repository (),
                                     PACKAGE_LIBDIR"/gnome-builder/girepository-1.0");

  /* Ensure that we have all our required GObject Introspection packages
   * loaded so that plugins don't need to require_version() as that is
   * tedious and annoying to keep up to date.
   *
   * If we can't load any of our dependent packages, then fail to load
   * python3 plugins altogether to avoid loading anything improper into
   * the process space.
   */
  if (!ide_application_load_all_typelibs (&error,
                                          "Gio", "2.0",
                                          "GLib", "2.0",
                                          "Gtk", "4.0",
                                          "GtkSource", "5",
                                          "Jsonrpc", "1.0",
                                          "Template", "1.0",
                                          "Vte", "3.91",
#ifdef HAVE_WEBKIT
                                          PACKAGE_WEBKIT_GIR_NAME, PACKAGE_WEBKIT_GIR_VERSION,
#endif
                                          "Ide", PACKAGE_ABI_S,
                                          NULL))
    g_critical ("Cannot enable GJS plugins: %s", error->message);
  else
    self->loaded_typelibs = TRUE;

  IDE_EXIT;
}

static void
ide_application_startup (GApplication *app)
{
  IdeApplication *self = (IdeApplication *)app;
  g_autofree gchar *style_path = NULL;
  GtkSourceStyleSchemeManager *styles;
  GtkSourceLanguageManager *langs;
  GtkIconTheme *icon_theme;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));

  G_APPLICATION_CLASS (ide_application_parent_class)->startup (app);

  /* Setup access to private icons dir */
  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  gtk_icon_theme_add_search_path (icon_theme, PACKAGE_ICONDIR);

  /* Add custom style locations for gtksourceview schemes */
  styles = gtk_source_style_scheme_manager_get_default ();
  style_path = g_build_filename (g_get_home_dir (), ".local", "share", "gtksourceview-5", "styles", NULL);
  gtk_source_style_scheme_manager_append_search_path (styles, style_path);
  gtk_source_style_scheme_manager_append_search_path (styles, "resource:///org/gnome/builder/gtksourceview/styles/");

  /* Add custom locations for language specs */
  langs = gtk_source_language_manager_get_default ();
  gtk_source_language_manager_append_search_path (langs, "resource:///org/gnome/builder/gtksourceview/language-specs/");

  /* Setup access to portal settings */
  _ide_application_init_settings (self);

  /* Load color settings (Night Light, Dark Mode, etc) */
  _ide_application_init_color (self);

  /* And now we can load the rest of our plugins for startup. */
  _ide_application_load_plugins (self);

  /* Load language defaults into gsettings */
  ide_language_defaults_init_async (NULL, NULL, NULL);

  /* Queue loading of the Network Monitor early to help ensure we
   * get reliable data quickly.
   */
  (void) ide_application_has_network (self);
}

static void
ide_application_shutdown (GApplication *app)
{
  IdeApplication *self = (IdeApplication *)app;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));

  _ide_application_unload_addins (self);

  g_clear_pointer (&self->plugin_settings, g_hash_table_unref);
  g_clear_object (&self->addins);
  g_clear_object (&self->settings);

  G_APPLICATION_CLASS (ide_application_parent_class)->shutdown (app);
}

static void
ide_application_activate_cb (PeasExtensionSet *set,
                             PeasPluginInfo   *plugin_info,
                             GObject    *exten,
                             gpointer          user_data)
{
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (exten));
  g_assert (IDE_IS_APPLICATION (user_data));

  ide_application_addin_activate (IDE_APPLICATION_ADDIN (exten), user_data);
}

static void
ide_application_activate (GApplication *app)
{
  IdeApplication *self = (IdeApplication *)app;
  GtkWindow *window;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));

  if ((window = gtk_application_get_active_window (GTK_APPLICATION (self))))
    gtk_window_present (GTK_WINDOW (window));

  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins,
                                ide_application_activate_cb,
                                self);

  IDE_EXIT;
}

static void
ide_application_open_cb (PeasExtensionSet *set,
                         PeasPluginInfo   *plugin_info,
                         GObject    *exten,
                         gpointer          user_data)
{
  IdeApplicationAddin *app_addin = (IdeApplicationAddin*) exten;
  OpenData *data = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (app_addin));
  g_assert (data != NULL);
  g_assert (IDE_IS_APPLICATION (data->self));
  g_assert (data->files != NULL);

  ide_application_addin_open (app_addin, data->self, data->files, data->n_files, data->hint);
}

static void
ide_application_open (GApplication  *app,
                      GFile        **files,
                      gint           n_files,
                      const gchar   *hint)
{
  IdeApplication *self = (IdeApplication*)app;
  OpenData data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (files);
  g_assert (n_files > 0);
  g_assert (hint);

  data.self = self;
  data.files = files;
  data.n_files = n_files;
  data.hint = hint;

  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins, ide_application_open_cb, &data);

  IDE_EXIT;
}

static GtkCssProvider *
get_css_provider (IdeApplication *self,
                  const char     *key)
{
  GtkCssProvider *ret;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (key != NULL);

  if (!(ret = g_hash_table_lookup (self->css_providers, key)))
    {
      ret = gtk_css_provider_new ();
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (ret),
                                                  GTK_STYLE_PROVIDER_PRIORITY_USER-1);
      g_hash_table_insert (self->css_providers, g_strdup (key), ret);
    }

  return ret;
}

void
_ide_application_add_resources (IdeApplication *self,
                                const char     *resource_path)
{
  g_autoptr(GError) error = NULL;
  g_autofree gchar *menu_path = NULL;
  g_autofree gchar *css_path = NULL;
  guint merge_id;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (resource_path != NULL);

  /* We use interned strings for hash table keys */
  resource_path = g_intern_string (resource_path);

  /*
   * If the resource path has a gtk/menus.ui file, we want to auto-load and
   * merge the menus.
   */
  menu_path = g_build_filename (resource_path, "gtk", "menus.ui", NULL);

  if (g_str_has_prefix (menu_path, "resource://"))
    merge_id = ide_menu_manager_add_resource (self->menu_manager, menu_path, &error);
  else
    merge_id = ide_menu_manager_add_filename (self->menu_manager, menu_path, &error);

  if (merge_id != 0)
    g_hash_table_insert (self->menu_merge_ids, (gchar *)resource_path, GUINT_TO_POINTER (merge_id));

  if (error != NULL &&
      !(g_error_matches (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND) ||
        g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)))
    g_warning ("%s", error->message);
  g_clear_error (&error);

  if (g_str_has_prefix (resource_path, "resource://"))
    {
      g_autoptr(GBytes) bytes = NULL;

      css_path = g_build_filename (resource_path + strlen ("resource://"), "style.css", NULL);
      bytes = g_resources_lookup_data (css_path, 0, NULL);

      if (bytes != NULL)
        {
          GtkCssProvider *provider = get_css_provider (self, resource_path);
          g_debug ("Loading CSS from resource path %s", css_path);
          gtk_css_provider_load_from_resource (provider, css_path);
        }
    }
  else
    {
      css_path = g_build_filename (resource_path, "style.css", NULL);

      if (g_file_test (css_path, G_FILE_TEST_IS_REGULAR))
        {
          GtkCssProvider *provider = get_css_provider (self, resource_path);
          g_debug ("Loading CSS from file path %s", css_path);
          gtk_css_provider_load_from_path (provider, css_path);
        }
    }

  ide_shortcut_manager_add_resources (resource_path);
}

void
_ide_application_remove_resources (IdeApplication *self,
                                   const char     *resource_path)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (resource_path != NULL);

  /* Unmerge menus, keybindings, etc */
  g_warning ("TODO: implement resource unloading for plugins: %s", resource_path);
}

static gboolean
ide_application_show_help_external (IdeApplication *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_APPLICATION (self));

  gtk_show_uri (NULL,
                "https://builder.readthedocs.io",
                GDK_CURRENT_TIME);

  IDE_RETURN (TRUE);
}

static void
ide_application_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeApplication *self = IDE_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_STYLE_SCHEME:
      g_value_set_string (value, ide_application_get_style_scheme (self));
      break;

    case PROP_SYSTEM_FONT_NAME:
      g_value_set_string (value, ide_application_get_system_font_name (self));
      break;

    case PROP_SYSTEM_FONT: {
      const char *system_font_name = ide_application_get_system_font_name (self);
      g_value_take_boxed (value, pango_font_description_from_string (system_font_name));
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_application_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeApplication *self = IDE_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_STYLE_SCHEME:
      ide_application_set_style_scheme (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_application_dispose (GObject *object)
{
  IdeApplication *self = (IdeApplication *)object;

  /* We don't necessarily get startup/shutdown called when we are
   * the remote process, so ensure they get cleared here rather than
   * in ::shutdown.
   */
  g_clear_pointer (&self->started_at, g_date_time_unref);
  g_clear_pointer (&self->workbenches, g_ptr_array_unref);
  g_clear_pointer (&self->plugin_settings, g_hash_table_unref);
  g_clear_pointer (&self->plugin_gresources, g_hash_table_unref);
  g_clear_pointer (&self->css_providers, g_hash_table_unref);
  g_clear_pointer (&self->argv, g_strfreev);
  g_clear_pointer (&self->menu_merge_ids, g_hash_table_unref);
  g_clear_pointer (&self->system_font_name, g_free);
  g_clear_object (&self->recoloring);
  g_clear_object (&self->addins);
  g_clear_object (&self->editor_settings);
  g_clear_object (&self->settings);
  g_clear_object (&self->network_monitor);
  g_clear_object (&self->menu_manager);

  G_OBJECT_CLASS (ide_application_parent_class)->dispose (object);
}

static void
ide_application_class_init (IdeApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = ide_application_dispose;
  object_class->get_property = ide_application_get_property;
  object_class->set_property = ide_application_set_property;

  app_class->activate = ide_application_activate;
  app_class->open = ide_application_open;
  app_class->add_platform_data = ide_application_add_platform_data;
  app_class->command_line = ide_application_command_line;
  app_class->local_command_line = ide_application_local_command_line;
  app_class->startup = ide_application_startup;
  app_class->shutdown = ide_application_shutdown;

  properties[PROP_STYLE_SCHEME] =
    g_param_spec_string ("style-scheme",
                         "Style Scheme",
                         "The style scheme for the editor",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SYSTEM_FONT] =
    g_param_spec_boxed ("system-font",
                        "System Font",
                        "System Font",
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SYSTEM_FONT_NAME] =
    g_param_spec_string ("system-font-name",
                         "System Font Name",
                         "System Font Name",
                         "Monospace 11",
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [SHOW_HELP] =
    g_signal_new_class_handler ("show-help",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_application_show_help_external),
                                g_signal_accumulator_true_handled, NULL,
                                NULL,
                                G_TYPE_BOOLEAN, 0);
}

static void
ide_application_init (IdeApplication *self)
{
  self->system_font_name = g_strdup ("Monospace 11");
  self->menu_merge_ids = g_hash_table_new (g_str_hash, g_str_equal);
  self->menu_manager = ide_menu_manager_new ();
  self->started_at = g_date_time_new_now_local ();
  self->workspace_type = IDE_TYPE_PRIMARY_WORKSPACE;
  self->workbenches = g_ptr_array_new_with_free_func (g_object_unref);
  self->settings = g_settings_new ("org.gnome.builder");
  self->editor_settings = g_settings_new ("org.gnome.builder.editor");
  self->plugin_gresources = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                   (GDestroyNotify)g_resource_unref);
  self->css_providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref );
  self->recoloring = gtk_css_provider_new ();

  g_application_set_default (G_APPLICATION (self));
  gtk_window_set_default_icon_name (ide_get_application_id ());

  /* Make sure we've loaded typelibs into process for early access */
  ide_application_load_typelibs (self);

  /* Ensure our core data is loaded early. */
  _ide_application_add_resources (self, "resource:///org/gnome/libide-gtk/");
  _ide_application_add_resources (self, "resource:///org/gnome/libide-tweaks/");
  _ide_application_add_resources (self, "resource:///org/gnome/libide-sourceview/");
  _ide_application_add_resources (self, "resource:///org/gnome/libide-gui/");
  _ide_application_add_resources (self, "resource:///org/gnome/libide-greeter/");
  _ide_application_add_resources (self, "resource:///org/gnome/libide-editor/");
  _ide_application_add_resources (self, "resource:///org/gnome/libide-terminal/");

  /* Make sure our GAction are available */
  _ide_application_init_actions (self);
}

IdeApplication *
_ide_application_new (gboolean standalone)
{
  GApplicationFlags flags = G_APPLICATION_HANDLES_COMMAND_LINE | G_APPLICATION_HANDLES_OPEN;
  IdeApplication *self;

  if (standalone)
    flags |= G_APPLICATION_NON_UNIQUE;

  self = g_object_new (IDE_TYPE_APPLICATION,
                       "application-id", ide_get_application_id (),
                       "flags", flags,
                       "resource-base-path", "/org/gnome/builder",
                       NULL);

  /* Load plugins indicating they support startup features */
  _ide_application_load_plugins_for_startup (self);

  /* Now that early plugins are loaded, we can activate app addins. We'll
   * load additional plugins later after post-early stage startup
   */
  _ide_application_load_addins (self);

  /* Register command-line options, possibly from plugins. */
  _ide_application_add_option_entries (self);

  return g_steal_pointer (&self);
}

static void
ide_application_add_workbench_cb (PeasExtensionSet *set,
                                  PeasPluginInfo   *plugin_info,
                                  GObject    *exten,
                                  gpointer          user_data)
{
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (exten));
  g_assert (IDE_IS_WORKBENCH (user_data));

  ide_application_addin_workbench_added (IDE_APPLICATION_ADDIN (exten), user_data);
}

void
ide_application_add_workbench (IdeApplication *self,
                               IdeWorkbench   *workbench)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  g_ptr_array_add (self->workbenches, g_object_ref (workbench));

  peas_extension_set_foreach (self->addins,
                              ide_application_add_workbench_cb,
                              workbench);
}

static void
ide_application_remove_workbench_cb (PeasExtensionSet *set,
                                     PeasPluginInfo   *plugin_info,
                                     GObject    *exten,
                                     gpointer          user_data)
{
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (exten));
  g_assert (IDE_IS_WORKBENCH (user_data));

  ide_application_addin_workbench_removed (IDE_APPLICATION_ADDIN (exten), user_data);
}

void
ide_application_remove_workbench (IdeApplication *self,
                                  IdeWorkbench   *workbench)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  peas_extension_set_foreach (self->addins,
                              ide_application_remove_workbench_cb,
                              workbench);

  g_ptr_array_remove (self->workbenches, workbench);
}

/**
 * ide_application_foreach_workbench:
 * @self: an #IdeApplication
 * @callback: (scope call): a #GFunc callback
 * @user_data: user data for @callback
 *
 * Calls @callback for each of the registered workbenches.
 */
void
ide_application_foreach_workbench (IdeApplication *self,
                                   GFunc           callback,
                                   gpointer        user_data)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (callback != NULL);

  for (guint i = self->workbenches->len; i > 0; i--)
    {
      IdeWorkbench *workbench = g_ptr_array_index (self->workbenches, i - 1);

      callback (workbench, user_data);
    }
}

/**
 * ide_application_set_workspace_type:
 * @self: a #IdeApplication
 *
 * Sets the #GType of an #IdeWorkspace that should be used when creating the
 * next workspace upon handling files from command-line arguments. This is
 * reset after the files are opened and is generally only useful from
 * #IdeApplicationAddin's who need to alter the default workspace.
 */
void
ide_application_set_workspace_type (IdeApplication *self,
                                    GType           workspace_type)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (g_type_is_a (workspace_type, IDE_TYPE_WORKSPACE));

  self->workspace_type = workspace_type;
}

static void
ide_application_network_changed_cb (IdeApplication  *self,
                                    gboolean         available,
                                    GNetworkMonitor *monitor)
{
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_NETWORK_MONITOR (monitor));

  g_debug ("Network available has changed to %d", available);

  self->has_network = !!available;
}

/**
 * ide_application_has_network:
 * @self: (nullable): a #IdeApplication
 *
 * This is a helper that uses an internal #GNetworkMonitor to track if we
 * have access to the network. It works around some issues we've seen in
 * the wild that make determining if we have network access difficult.
 *
 * Returns: %TRUE if we think there is network access.
 */
gboolean
ide_application_has_network (IdeApplication *self)
{
  g_return_val_if_fail (!self || IDE_IS_APPLICATION (self), FALSE);

  if (self == NULL)
    self = IDE_APPLICATION_DEFAULT;

  if (self->network_monitor == NULL)
    {
      self->network_monitor = g_object_ref (g_network_monitor_get_default ());

      g_signal_connect_object (self->network_monitor,
                               "network-changed",
                               G_CALLBACK (ide_application_network_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

      self->has_network = g_network_monitor_get_network_available (self->network_monitor);

      /*
       * FIXME: Ignore the network portal initially for now.
       *
       * See https://gitlab.gnome.org/GNOME/glib/merge_requests/227 for more
       * information about when this is fixed. However, even with that in
       * place we still have issues with our initial state.
       *
       * See Also: https://gitlab.gnome.org/GNOME/glib/-/issues/1718
       */
      if (!self->has_network && ide_is_flatpak ())
        self->has_network = TRUE;
    }

  return self->has_network;
}

/**
 * ide_application_get_started_at:
 * @self: a #IdeApplication
 *
 * Gets the time the application was started.
 *
 * Returns: (transfer none): a #GDateTime
 */
GDateTime *
ide_application_get_started_at (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  return self->started_at;
}

/**
 * ide_application_find_workbench_for_file:
 * @self: a #IdeApplication
 * @file: a #GFile
 *
 * Looks for the workbench that is the closest match to @file.
 *
 * If no workbench is the root of @file, then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkbench or %NULL
 */
IdeWorkbench *
ide_application_find_workbench_for_file (IdeApplication *self,
                                         GFile          *file)
{
  g_autofree gchar *suffix = NULL;
  IdeWorkbench *match = NULL;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  for (guint i = 0; i < self->workbenches->len; i++)
    {
      IdeWorkbench *workbench = g_ptr_array_index (self->workbenches, 0);
      IdeContext *context = ide_workbench_get_context (workbench);
      g_autoptr(GFile) workdir = ide_context_ref_workdir (context);

      if (!ide_workbench_has_project (workbench))
        continue;

      if (g_file_has_prefix (file, workdir))
        {
          g_autofree gchar *relative = g_file_get_relative_path (workdir, file);

          if (!suffix || strlen (relative) < strlen (suffix))
            {
              match = workbench;
              g_free (suffix);
              suffix = g_steal_pointer (&relative);
            }
        }
    }

  /* TODO: If a file is installed, but was installed by a workspace that
   *       we have open, we want to switch to that file instead of the
   *       installed version. For example, something installed to
   *       /app/include/libpeas-2/peas-engine.h should really open
   *       peas-engine.h from the project. This will require querying
   *       the pipeline/build-system for installed files to reverse-map the
   *       filename.
   */

  return match;
}

void
ide_application_set_command_line_handled (IdeApplication          *self,
                                          GApplicationCommandLine *cmdline,
                                          gboolean                 handled)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  return g_object_set_data (G_OBJECT (cmdline), "COMMAND_LINE_HANDLED", GINT_TO_POINTER (!!handled));
}

gboolean
ide_application_get_command_line_handled (IdeApplication          *self,
                                          GApplicationCommandLine *cmdline)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), FALSE);
  g_return_val_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline), FALSE);

  return !!g_object_get_data (G_OBJECT (cmdline), "COMMAND_LINE_HANDLED");
}

/**
 * ide_application_find_addin_by_module_name:
 * @self: a #IdeApplication
 * @module_name: the name of the plugin module
 *
 * Finds a loaded #IdeApplicationAddin within @self that was part of
 * the plugin matching @module_name.
 *
 * Returns: (transfer none) (type IdeApplicationAddin) (nullable): an
 *   #IdeApplicationAddin or %NULL.
 */
gpointer
ide_application_find_addin_by_module_name (IdeApplication *self,
                                           const gchar    *module_name)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;

  if (self == NULL)
    self = IDE_APPLICATION_DEFAULT;

  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  if (self->addins == NULL)
    return NULL;

  engine = peas_engine_get_default ();
  plugin_info = peas_engine_get_plugin_info (engine, module_name);

  if (plugin_info == NULL)
    return NULL;

  return peas_extension_set_get_extension (self->addins, plugin_info);
}

/**
 * ide_application_get_menu_by_id:
 * @self: a #IdeApplication
 * @menu_id: (nullable): the menu identifier
 *
 * Gets the merged menu by it's identifier.
 *
 * Returns: (transfer none) (nullable): a #GMenu or %NULL if @menu_id is %NULL
 */
GMenu *
ide_application_get_menu_by_id (IdeApplication *self,
                                const char     *menu_id)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  if (menu_id == NULL)
    return NULL;

  return ide_menu_manager_get_menu_by_id (self->menu_manager, menu_id);
}

const char *
ide_application_get_system_font_name (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  return self->system_font_name;
}

static GFile *
get_user_style_file (GFile *file)
{
  static GFile *style_dir;
  g_autofree char *basename = NULL;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if G_UNLIKELY (style_dir == NULL)
    {
      if (ide_is_flatpak ())
        style_dir = g_file_new_build_filename (g_get_home_dir (),
                                               ".local",
                                               "share",
                                               "gtksourceview-5",
                                               "styles",
                                               NULL);
      else
        style_dir = g_file_new_build_filename (g_get_user_data_dir (),
                                               "gtksourceview-5",
                                               "styles",
                                               NULL);
    }

  basename = g_file_get_basename (file);

  /* Style schemes must have .xml suffix to be picked up
   * by GtkSourceView. See GNOME/gnome-builder#1999.
   */
  if (!g_str_has_suffix (basename, ".xml"))
    {
      g_autofree char *tmp = g_steal_pointer (&basename);
      basename = g_strdup_printf ("%s.xml", tmp);
    }

  return g_file_get_child (style_dir, basename);
}

static void
ide_application_install_schemes_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFile) dst = NULL;
  GPtrArray *ar;
  GFile *src;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  ar = g_task_get_task_data (task);

  g_assert (ar != NULL);
  g_assert (ar->len > 0);
  g_assert (G_IS_FILE (g_ptr_array_index (ar, ar->len-1)));

  g_ptr_array_remove_index (ar, ar->len-1);

  if (!g_file_copy_finish (file, result, &error))
    g_warning ("Failed to copy file: %s", error->message);

  if (ar->len == 0)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  src = g_ptr_array_index (ar, ar->len-1);
  dst = get_user_style_file (src);

  g_file_copy_async (src, dst,
                     G_FILE_COPY_OVERWRITE | G_FILE_COPY_BACKUP,
                     G_PRIORITY_LOW,
                     g_task_get_cancellable (task),
                     NULL, NULL,
                     ide_application_install_schemes_cb,
                     g_object_ref (task));

  IDE_EXIT;
}

void
ide_application_install_schemes_async (IdeApplication       *self,
                                       GFile               **files,
                                       guint                 n_files,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data)
{
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) dst = NULL;
  g_autoptr(GFile) dir = NULL;
  GFile *src;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (files != NULL);
  g_return_if_fail (n_files > 0);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  ar = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 0; i < n_files; i++)
    g_ptr_array_add (ar, g_object_ref (files[i]));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_application_install_schemes_async);
  g_task_set_task_data (task, g_ptr_array_ref (ar), (GDestroyNotify) g_ptr_array_unref);

  src = g_ptr_array_index (ar, ar->len-1);
  dst = get_user_style_file (src);
  dir = g_file_get_parent (dst);

  if (!g_file_query_exists (dir, NULL) &&
      !g_file_make_directory_with_parents (dir, cancellable, &error))
    {
      g_warning ("Failed to create directory for style scheme: %s", error->message);
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_file_copy_async (src, dst,
                     G_FILE_COPY_OVERWRITE | G_FILE_COPY_BACKUP,
                     G_PRIORITY_LOW,
                     cancellable,
                     NULL, NULL,
                     ide_application_install_schemes_cb,
                     g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_application_install_schemes_finish (IdeApplication  *self,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * ide_application_find_project_workbench:
 * @self: a #IdeApplication
 * @project_info: an #IdeProjectInfo
 *
 * Finds a workbench that has @project_info loaded.
 *
 * If no workbench could be found, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkbench or %NULL
 *
 * Since: 44
 */
IdeWorkbench *
ide_application_find_project_workbench (IdeApplication *self,
                                        IdeProjectInfo *project_info)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (project_info), NULL);

  for (guint i = 0; i < self->workbenches->len; i++)
    {
      IdeWorkbench *workbench = g_ptr_array_index (self->workbenches, i);
      IdeProjectInfo *workbench_project_info = ide_workbench_get_project_info (workbench);

      if (workbench_project_info != NULL &&
          ide_project_info_equal (workbench_project_info, project_info))
        return workbench;
    }

  return NULL;
}

gboolean
ide_application_control_is_pressed (IdeApplication *self)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *keyboard;
  GdkModifierType modifiers;

  if (self == NULL)
    self = IDE_APPLICATION_DEFAULT;

  g_return_val_if_fail (IDE_IS_APPLICATION (self), FALSE);

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);
  keyboard = gdk_seat_get_keyboard (seat);
  modifiers = gdk_device_get_modifier_state (keyboard) & gtk_accelerator_get_default_mod_mask ();

  return !!(modifiers & GDK_CONTROL_MASK);
}

/**
 * ide_application_get_active_workbench:
 * @self: a #IdeApplication
 *
 * Gets the active workbench, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkbench or %NULL
 */
IdeWorkbench *
ide_application_get_active_workbench (IdeApplication *self)
{
  GtkWindow *window;

  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  if (!(window = gtk_application_get_active_window (GTK_APPLICATION (self))))
    return NULL;

  do
    {
      IdeWorkbench *workbench;

      if (IDE_IS_WORKSPACE (window))
        return ide_workspace_get_workbench (IDE_WORKSPACE (window));

      if ((workbench = ide_workbench_from_widget (GTK_WIDGET (window))))
        return workbench;
    }
  while ((window = gtk_window_get_transient_for (window)));

  return NULL;
}
