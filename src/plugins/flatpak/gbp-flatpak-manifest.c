/* gbp-flatpak-manifest.c
 *
 * Copyright 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-manifest"

#include <flatpak/flatpak.h>
#include <json-glib/json-glib.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-util.h"

struct _GbpFlatpakManifest
{
  IdeConfig  parent_instance;

  GFile            *file;
  GFileMonitor     *file_monitor;

  JsonNode         *root;

  /* These are related to the toplevel object, which are project-wide
   * configuration options.
   */
  gchar           **build_args;
  gchar            *command;
  gchar           **finish_args;
  gchar            *runtime;
  gchar            *runtime_version;
  gchar            *sdk;
  gchar           **sdk_extensions;

  /*
   * These are related to the primary module, which is the module that
   * we believe that the user opened as the project.
   */
  JsonObject       *primary;
  gchar            *primary_module;
  gchar           **config_opts;
};

static void initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakManifest, gbp_flatpak_manifest, IDE_TYPE_CONFIG,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

enum {
  NEEDS_RELOAD,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];
static GRegex *app_id_regex;

static gboolean
is_valid_app_id (const gchar *str)
{
  return g_regex_match (app_id_regex, str, 0, NULL);
}

static gboolean
validate_properties (GbpFlatpakManifest  *self,
                     GError             **error)
{
  g_autofree gchar *runtime_id = NULL;
  const gchar *name, *arch, *branch;

  if (self->runtime == NULL ||
      self->command == NULL ||
      self->primary == NULL ||
      self->primary_module == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Does not appear to be a valid manifest");
      return FALSE;
    }

  name = self->runtime;
  arch = flatpak_get_default_arch ();
  branch = "master";
  if (self->runtime_version != NULL)
    branch = self->runtime_version;

  runtime_id = g_strdup_printf ("flatpak:%s/%s/%s", name, arch, branch);
  ide_config_set_runtime_id (IDE_CONFIG (self), runtime_id);

  return TRUE;
}

static gboolean
discover_string_field (JsonObject   *object,
                       const gchar  *key,
                       gchar       **location)
{
  JsonNode *node;

  g_assert (key != NULL);
  g_assert (location != NULL);

  if (object != NULL &&
      json_object_has_member (object, key) &&
      (node = json_object_get_member (object, key)) &&
      JSON_NODE_HOLDS_VALUE (node))
    {
      *location = g_strdup (json_node_get_string (node));
      return TRUE;
    }

  *location = NULL;

  return FALSE;
}

static gboolean
discover_strv_field (JsonObject    *object,
                     const gchar   *key,
                     gchar       ***location)
{
  JsonNode *node;

  g_assert (key != NULL);
  g_assert (location != NULL);

  if (object != NULL &&
      json_object_has_member (object, key) &&
      (node = json_object_get_member (object, key)) &&
      JSON_NODE_HOLDS_ARRAY (node))
    {
      g_autoptr(GPtrArray) ar = g_ptr_array_new_with_free_func (g_free);
      JsonArray *container = json_node_get_array (node);
      guint n_elements = json_array_get_length (container);

      for (guint i = 0; i < n_elements; i++)
        {
          const gchar *str = json_array_get_string_element (container, i);
          g_ptr_array_add (ar, g_strdup (str));
        }

      g_ptr_array_add (ar, NULL);

      *location = (gchar **)g_ptr_array_free (g_steal_pointer (&ar), FALSE);

      return TRUE;
    }

  return FALSE;
}

static gboolean
discover_strv_as_quoted (JsonObject   *object,
                         const gchar  *key,
                         gchar       **location)
{
  JsonNode *node;

  g_assert (key != NULL);
  g_assert (location != NULL);

  if (object != NULL &&
      json_object_has_member (object, key) &&
      (node = json_object_get_member (object, key)) &&
      JSON_NODE_HOLDS_ARRAY (node))
    {
      g_autoptr(GPtrArray) ar = g_ptr_array_new_with_free_func (g_free);
      JsonArray *container = json_node_get_array (node);
      guint n_elements = json_array_get_length (container);

      for (guint i = 0; i < n_elements; i++)
        {
          const gchar *str = json_array_get_string_element (container, i);
          g_ptr_array_add (ar, g_shell_quote (str));
        }

      g_ptr_array_add (ar, NULL);
      *location = g_strjoinv (" ", (gchar **)(gpointer)ar->pdata);
      return TRUE;
    }

  return FALSE;
}

static void
discover_environ (GbpFlatpakManifest *self,
                  JsonObject         *root)
{
  IdeEnvironment *env;
  JsonObject *build_options;
  JsonObject *obj;
  const gchar *str;

  g_assert (IDE_IS_CONFIG (self));

  if (!json_object_has_member (root, "build-options"))
    return;

  if (!(build_options = json_object_get_object_member (root, "build-options")))
    return;

  env = ide_config_get_environment (IDE_CONFIG (self));

  if (json_object_has_member (build_options, "env") &&
      (obj = json_object_get_object_member (build_options, "env")))
    {
      JsonObjectIter iter;
      const gchar *key;
      JsonNode *value;

      json_object_iter_init (&iter, obj);
      while (json_object_iter_next (&iter, &key, &value))
        {
          if (JSON_NODE_HOLDS_VALUE (value))
            ide_environment_setenv (env, key, json_node_get_string (value));
        }
    }

  if (json_object_has_member (build_options, "cflags") &&
      (str = json_object_get_string_member (build_options, "cflags")))
    ide_environment_setenv (env, "CFLAGS", str);

  if (json_object_has_member (build_options, "cxxflags") &&
      (str = json_object_get_string_member (build_options, "cxxflags")))
    ide_environment_setenv (env, "CXXFLAGS", str);

  if (json_object_has_member (build_options, "append-path") &&
      (str = json_object_get_string_member (build_options, "append-path")))
    ide_config_set_append_path (IDE_CONFIG (self), str);
}

static JsonObject *
discover_primary_module (GbpFlatpakManifest  *self,
                         JsonObject          *parent,
                         const gchar         *dir_name,
                         gboolean             is_root,
                         GError             **error)
{
  JsonArray *ar;
  JsonNode *modules;
  guint n_elements;

  g_assert (GBP_IS_FLATPAK_MANIFEST (self));
  g_assert (parent != NULL);
  g_assert (dir_name != NULL);

  if (!json_object_has_member (parent, "modules") ||
      !(modules = json_object_get_member (parent, "modules")) ||
      !JSON_NODE_HOLDS_ARRAY (modules) ||
      !(ar = json_node_get_array (modules)))
    goto no_match;

  n_elements = json_array_get_length (ar);

  for (guint i = n_elements; i > 0; i--)
    {
      JsonNode *element = json_array_get_element (ar, i - 1);
      const gchar *name;
      JsonObject *obj;

      if (!JSON_NODE_HOLDS_OBJECT (element) ||
          !(obj = json_node_get_object (element)) ||
          !(name = json_object_get_string_member (obj, "name")))
        continue;

      if (dzl_str_equal0 (name, dir_name))
        {
          self->primary_module = g_strdup (name);
          return obj;
        }

      if (json_object_has_member (obj, "modules"))
        {
          JsonObject *subobj;

          if ((subobj = discover_primary_module (self, obj, dir_name, FALSE, NULL)))
            return subobj;
        }
    }

  if (is_root)
    {
      for (guint i = n_elements; i > 0; i--)
        {
          JsonNode *element = json_array_get_element (ar, i - 1);
          const gchar *name;
          JsonObject *obj;

          if (!JSON_NODE_HOLDS_OBJECT (element) ||
              !(obj = json_node_get_object (element)) ||
              !(name = json_object_get_string_member (obj, "name")))
            continue;

          self->primary_module = g_strdup (name);
          return obj;
        }
    }

no_match:
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to locate primary module in modules");

  return NULL;
}

static gboolean
gbp_flatpak_manifest_initable_init (GInitable     *initable,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GbpFlatpakManifest *self = (GbpFlatpakManifest *)initable;
  g_autofree gchar *app_id = NULL;
  g_autofree gchar *contents = NULL;
  g_autofree gchar *dir_name = NULL;
  g_autofree gchar *display_name = NULL;
  g_autofree gchar *run_args = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_auto(GStrv) build_commands = NULL;
  g_auto(GStrv) post_install = NULL;
  const gchar *app_id_field = "app-id";
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) workdir = NULL;
  JsonObject *root_obj;
  JsonObject *primary;
  JsonNode *root;
  gsize len = 0;

  g_assert (GBP_IS_FLATPAK_MANIFEST (self));
  g_assert (G_IS_FILE (self->file));
  g_assert (self->root == NULL);

  if (!g_file_load_contents (self->file, cancellable, &contents, &len, NULL, error))
    return FALSE;

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, contents, len, error))
    return FALSE;

  root = json_parser_get_root (parser);

  if (root == NULL || !JSON_NODE_HOLDS_OBJECT (root))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Root object is not an object");
      return FALSE;
    }

  display_name = g_file_get_basename (self->file);
  ide_config_set_display_name (IDE_CONFIG (self), display_name);

  context = ide_object_ref_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);
  dir_name = g_file_get_basename (workdir);
  root_obj = json_node_get_object (root);

  ide_config_set_build_commands_dir (IDE_CONFIG (self), workdir);

  if (!(primary = discover_primary_module (self, root_obj, dir_name, TRUE, error)))
    return FALSE;

  /* Some flatpak manifests have "id" instead of "app-id", such
   * as some KDE applications we've seen in the wild.
   */
  if (!json_object_has_member (root_obj, "app-id") &&
      json_object_has_member (root_obj, "id"))
    app_id_field = "id";

  if (!discover_string_field (root_obj, app_id_field, &app_id) || !is_valid_app_id (app_id))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "File does not appear to have a valid app-id");
      return FALSE;
    }

  ide_config_set_app_id (IDE_CONFIG (self), app_id);

  discover_string_field (root_obj, "runtime", &self->runtime);
  discover_string_field (root_obj, "runtime-version", &self->runtime_version);
  discover_string_field (root_obj, "sdk", &self->sdk);
  discover_string_field (root_obj, "command", &self->command);
  discover_strv_field (root_obj, "build-args", &self->build_args);
  discover_strv_field (root_obj, "finish-args", &self->finish_args);
  discover_strv_field (root_obj, "sdk-extensions", &self->sdk_extensions);

  if (discover_strv_as_quoted (root_obj, "x-run-args", &run_args))
    ide_config_set_run_opts (IDE_CONFIG (self), run_args);

  if (discover_strv_field (primary, "config-opts", &self->config_opts))
    {
      g_autoptr(GString) gstr = g_string_new (NULL);

      for (guint i = 0; self->config_opts[i]; i++)
        {
          const gchar *opt = self->config_opts[i];

          if (i > 0)
            g_string_append_c (gstr, ' ');

          if (strchr (opt, '\'') || strchr (opt, '"'))
            {
              g_autofree gchar *quoted = g_shell_quote (opt);
              g_string_append (gstr, quoted);
            }
          else
            g_string_append (gstr, opt);
        }

      ide_config_set_config_opts (IDE_CONFIG (self), gstr->str);
    }

  if (discover_strv_field (primary, "build-commands", &build_commands))
    ide_config_set_build_commands (IDE_CONFIG (self),
                                          (const gchar * const *)build_commands);

  if (discover_strv_field (primary, "post-install", &post_install))
    ide_config_set_post_install_commands (IDE_CONFIG (self),
                                                 (const gchar * const *)post_install);

  if (json_object_has_member (primary, "builddir") &&
      json_object_get_boolean_member (primary, "builddir"))
    ide_config_set_locality (IDE_CONFIG (self), IDE_BUILD_LOCALITY_OUT_OF_TREE);
  else
    ide_config_set_locality (IDE_CONFIG (self), IDE_BUILD_LOCALITY_IN_TREE);

  discover_environ (self, root_obj);

  self->root = json_node_ref (root);
  self->primary = json_object_ref (primary);

  if (!validate_properties (self, error))
    return FALSE;

  ide_config_set_dirty (IDE_CONFIG (self), FALSE);

  return TRUE;
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = gbp_flatpak_manifest_initable_init;
}

static gboolean
gbp_flatpak_manifest_supports_runtime (IdeConfig *config,
                                       IdeRuntime       *runtime)
{
  g_assert (GBP_IS_FLATPAK_MANIFEST (config));
  g_assert (IDE_IS_RUNTIME (runtime));

  return GBP_IS_FLATPAK_RUNTIME (runtime);
}

static void
gbp_flatpak_manifest_file_changed (GbpFlatpakManifest *self,
                                   GFile              *file,
                                   GFile              *other_file,
                                   GFileMonitorEvent   event,
                                   GFileMonitor       *file_monitor)
{
  g_assert (GBP_IS_FLATPAK_MANIFEST (self));
  g_assert (G_IS_FILE_MONITOR (file_monitor));

  if (event == G_FILE_MONITOR_EVENT_CHANGED || event == G_FILE_MONITOR_EVENT_CREATED)
    g_signal_emit (self, signals [NEEDS_RELOAD], 0);
}

static void
gbp_flatpak_manifest_block_monitor (GbpFlatpakManifest *self)
{
  g_assert (GBP_IS_FLATPAK_MANIFEST (self));

  g_signal_handlers_block_matched (self->file_monitor,
                                   G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                   g_signal_lookup ("changed", G_TYPE_FILE_MONITOR),
                                   0,
                                   NULL,
                                   gbp_flatpak_manifest_file_changed,
                                   self);
}

static void
gbp_flatpak_manifest_unblock_monitor (GbpFlatpakManifest *self)
{
  g_assert (GBP_IS_FLATPAK_MANIFEST (self));

  g_signal_handlers_unblock_matched (self->file_monitor,
                                     G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                                     g_signal_lookup ("changed", G_TYPE_FILE_MONITOR),
                                     0,
                                     NULL,
                                     gbp_flatpak_manifest_file_changed,
                                     self);
}

static void
gbp_flatpak_manifest_set_file (GbpFlatpakManifest *self,
                               GFile              *file)
{
  g_assert (GBP_IS_FLATPAK_MANIFEST (self));
  g_assert (self->file == NULL);
  g_assert (self->file_monitor == NULL);
  g_assert (!file || G_IS_FILE (file));

  if (file == NULL)
    {
      g_critical ("GbpFlatpakManifest:file is required upon construction");
      return;
    }

  g_set_object (&self->file, file);

  self->file_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

  g_signal_connect_object (self->file_monitor,
                           "changed",
                           G_CALLBACK (gbp_flatpak_manifest_file_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static IdeRuntime *
find_extension (GbpFlatpakManifest *self,
                const gchar        *name)
{
  g_autoptr(FlatpakInstalledRef) ref = NULL;
  GbpFlatpakApplicationAddin *addin;
  GbpFlatpakRuntime *ret = NULL;

  g_assert (GBP_IS_FLATPAK_MANIFEST (self));
  g_assert (name != NULL);

  /* TODO: This doesn't allow pinning to the right version
   * of extension because we need to know the right parent
   * version of the extension.
   */
  addin = gbp_flatpak_application_addin_get_default ();
  ref = gbp_flatpak_application_addin_find_extension (addin, name);

  if (ref != NULL)
    ret = gbp_flatpak_runtime_new (ref, TRUE, NULL, NULL);

  return IDE_RUNTIME (g_steal_pointer (&ret));
}

static GPtrArray *
gbp_flatpak_manifest_get_extensions (IdeConfig *config)
{
  GbpFlatpakManifest *self = (GbpFlatpakManifest *)config;
  GPtrArray *ret;

  g_assert (GBP_IS_FLATPAK_MANIFEST (self));

  ret = g_ptr_array_new ();

  if (self->sdk_extensions != NULL)
    {
      for (guint i = 0; self->sdk_extensions[i]; i++)
        {
          IdeRuntime *found = find_extension (self, self->sdk_extensions[i]);

          if (found)
            g_ptr_array_add (ret, g_steal_pointer (&found));
        }
    }

  return g_steal_pointer (&ret);
}

static void
gbp_flatpak_manifest_finalize (GObject *object)
{
  GbpFlatpakManifest *self = (GbpFlatpakManifest *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->file_monitor);

  g_clear_pointer (&self->root, json_node_unref);

  g_clear_pointer (&self->build_args, g_strfreev);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->finish_args, g_strfreev);
  g_clear_pointer (&self->runtime, g_free);
  g_clear_pointer (&self->runtime_version, g_free);
  g_clear_pointer (&self->sdk, g_free);
  g_clear_pointer (&self->sdk_extensions, g_strfreev);

  g_clear_pointer (&self->primary, json_object_unref);
  g_clear_pointer (&self->primary_module, g_free);
  g_clear_pointer (&self->config_opts, g_strfreev);

  G_OBJECT_CLASS (gbp_flatpak_manifest_parent_class)->finalize (object);
}

static void
gbp_flatpak_manifest_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpFlatpakManifest *self = GBP_FLATPAK_MANIFEST (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, gbp_flatpak_manifest_get_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_flatpak_manifest_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpFlatpakManifest *self = GBP_FLATPAK_MANIFEST (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gbp_flatpak_manifest_set_file (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_flatpak_manifest_class_init (GbpFlatpakManifestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeConfigClass *config_class = IDE_CONFIG_CLASS (klass);

  object_class->finalize = gbp_flatpak_manifest_finalize;
  object_class->get_property = gbp_flatpak_manifest_get_property;
  object_class->set_property = gbp_flatpak_manifest_set_property;

  config_class->get_extensions = gbp_flatpak_manifest_get_extensions;
  config_class->supports_runtime = gbp_flatpak_manifest_supports_runtime;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file containing the manifest",
                         G_TYPE_FILE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [NEEDS_RELOAD] =
    g_signal_new ("needs-reload",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /* This regex is based on https://wiki.gnome.org/HowDoI/ChooseApplicationID */
  app_id_regex = g_regex_new ("^[[:alnum:]-_]+\\.[[:alnum:]-_]+(\\.[[:alnum:]-_]+)*$",
                              G_REGEX_OPTIMIZE, 0, NULL);
}

static void
gbp_flatpak_manifest_init (GbpFlatpakManifest *self)
{
  ide_config_set_prefix (IDE_CONFIG (self), "/app");
}

GbpFlatpakManifest *
gbp_flatpak_manifest_new (GFile       *file,
                          const gchar *id)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  return g_object_new (GBP_TYPE_FLATPAK_MANIFEST,
                       "id", id,
                       "file", file,
                       NULL);
}

/**
 * gbp_flatpak_manifest_get_file:
 *
 * Gets the #GFile for the manifest.
 *
 * Returns: (transfer none): a #GFile
 */
GFile *
gbp_flatpak_manifest_get_file (GbpFlatpakManifest *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), NULL);

  return self->file;
}

/**
 * gbp_flatpak_manifest_get_primary_module:
 *
 * Gets the name of the primary module, which is usually the last
 * module of manifest.
 */
const gchar *
gbp_flatpak_manifest_get_primary_module (GbpFlatpakManifest *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), NULL);

  return self->primary_module;
}

/**
 * gbp_flatpak_manifest_get_command:
 *
 * Gets the "command" specified in the manifest.
 */
const gchar *
gbp_flatpak_manifest_get_command (GbpFlatpakManifest *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), NULL);

  return self->command;
}

/**
 * gbp_flatpak_manifest_get_build_args:
 *
 * Gets the "build-args" from the manifest as a string array.
 */
const gchar * const *
gbp_flatpak_manifest_get_build_args (GbpFlatpakManifest *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), NULL);

  return (const gchar * const *)self->build_args;
}

/**
 * gbp_flatpak_manifest_get_finish_args:
 *
 * Gets the "finish-args" from the manifest as a string array.
 */
const gchar * const *
gbp_flatpak_manifest_get_finish_args (GbpFlatpakManifest *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), NULL);

  return (const gchar * const *)self->finish_args;
}

const gchar *
gbp_flatpak_manifest_get_sdk (GbpFlatpakManifest *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), NULL);

  return self->sdk;
}

/**
 * gbp_flatpak_manifest_get_sdk_extensions:
 *
 * Gets the "sdk-extensions" from the manifest as a string array.
 */
const gchar * const *
gbp_flatpak_manifest_get_sdk_extensions (GbpFlatpakManifest *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), NULL);

  return (const gchar * const *)self->sdk_extensions;
}

/**
 * gbp_flatpak_manifest_get_path:
 *
 * Gets the path for the manifest. This is equivalent to calling
 * g_file_get_path() with the result of gbp_flatpak_manifest_get_file().
 */
gchar *
gbp_flatpak_manifest_get_path (GbpFlatpakManifest *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), NULL);

  return g_file_get_path (self->file);
}

static void
apply_changes_to_tree (GbpFlatpakManifest *self)
{
  IdeEnvironment *env;
  const gchar *app_id;
  const gchar *config_opts;
  const gchar *runtime_id;
  JsonObject *obj;
  JsonObject *build_options;
  JsonObject *env_obj;
  guint n_items;

  g_assert (GBP_IS_FLATPAK_MANIFEST (self));
  g_assert (self->root != NULL);
  g_assert (JSON_NODE_HOLDS_OBJECT (self->root));

  obj = json_node_get_object (self->root);

  if ((runtime_id = ide_config_get_runtime_id (IDE_CONFIG (self))))
    {
      g_autofree gchar *id = NULL;
      g_autofree gchar *arch = NULL;
      g_autofree gchar *branch = NULL;

      if (g_str_has_prefix (runtime_id, "flatpak:"))
        runtime_id += strlen ("flatpak:");

      if (gbp_flatpak_split_id (runtime_id, &id, &arch, &branch))
        {
          json_object_set_string_member (obj, "runtime", id);
          json_object_set_string_member (obj, "runtime-version", branch);
        }
    }

  if ((app_id = ide_config_get_app_id (IDE_CONFIG (self))))
  {
    /* Be friendly to old? style "id" fields */
    if (json_object_has_member (obj, "id"))
      json_object_set_string_member (obj, "id", app_id);
    else
      json_object_set_string_member (obj, "app-id", app_id);
  }

  if (!json_object_has_member (obj, "build-options"))
    json_object_set_object_member (obj, "build-options", json_object_new ());
  build_options = json_object_get_object_member (obj, "build-options");

  env_obj = json_object_new ();
  json_object_set_object_member (build_options, "env", env_obj);

  env = ide_config_get_environment (IDE_CONFIG (self));
  n_items = g_list_model_get_n_items (G_LIST_MODEL (env));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeEnvironmentVariable) var = g_list_model_get_item (G_LIST_MODEL (env), i);
      const gchar *key;
      const gchar *value;

      g_assert (IDE_IS_ENVIRONMENT_VARIABLE (var));

      key = ide_environment_variable_get_key (var);
      value = ide_environment_variable_get_value (var);

      if (dzl_str_equal0 (key, "CFLAGS"))
        json_object_set_string_member (build_options, "cflags", value);
      else if (dzl_str_equal0 (key, "CXXFLAGS"))
        json_object_set_string_member (build_options, "cxxflags", value);
      else
        json_object_set_string_member (env_obj, key, value);
    }

  if (ide_config_get_locality (IDE_CONFIG (self)) == IDE_BUILD_LOCALITY_OUT_OF_TREE)
    json_object_set_boolean_member (self->primary, "builddir", TRUE);
  else if (json_object_has_member (self->primary, "builddir"))
    json_object_remove_member (self->primary, "builddir");

  if (!(config_opts = ide_config_get_config_opts (IDE_CONFIG (self))))
    {
      if (json_object_has_member (self->primary, "config-opts"))
        json_object_remove_member (self->primary, "config-opts");
    }
  else
    {
      g_auto(GStrv) argv = NULL;
      gint argc;

      if (g_shell_parse_argv (config_opts, &argc, &argv, NULL))
        {
          g_autoptr(JsonArray) ar = json_array_new ();

          for (guint i = 0; argv[i] != NULL; i++)
            json_array_add_string_element (ar, argv[i]);

          json_object_set_array_member (self->primary, "config-opts", g_steal_pointer (&ar));
        }

    }

}

static void
gbp_flatpak_manifest_save_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  GbpFlatpakManifest *self;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = ide_task_get_source_object (task);
  ide_config_set_dirty (IDE_CONFIG (self), FALSE);
  ide_task_return_boolean (task, TRUE);

  gbp_flatpak_manifest_unblock_monitor (self);

  IDE_EXIT;
}

void
gbp_flatpak_manifest_save_async (GbpFlatpakManifest  *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(JsonGenerator) generator = NULL;
  g_autofree gchar *data = NULL;
  gsize len;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_FLATPAK_MANIFEST (self));
  g_return_if_fail (G_IS_FILE (self->file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_manifest_save_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (self->root == NULL || self->primary == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to save, missing JSON node");
      return;
    }

  /*
   * First apply our changes to the saved JsonNode while we are in the
   * main loop to avoid proxying structures to another thread (and the
   * mutability issues that would arrise from that).
   */
  apply_changes_to_tree (self);

  /*
   * Now that we have an updated JsonNode tree, convert that to a
   * pretty-printed JSON document stream. We are destructive here (in that
   * we lose extended-JSON comments. But that is outside the scope of our
   * support and needs to be dealt with at a lower layer.
   */
  generator = json_generator_new ();
  json_generator_set_pretty (generator, TRUE);
  json_generator_set_indent (generator, 4);
  json_generator_set_indent_char (generator, ' ');
  json_generator_set_root (generator, self->root);
  data = json_generator_to_data (generator, &len);

  /*
   * Since we're writing this as a series of bytes, and not a utf8 string
   * (even though it is), we can steal the final \0 byte to add a trailing
   * newline for the file.
   */
  data[len] = '\n';
  bytes = g_bytes_new_take (g_steal_pointer (&data), len + 1);

  gbp_flatpak_manifest_block_monitor (self);

  /*
   * Now that we have a buffer containing the UTF-8 encoded formatted
   * JSON, we can asynchronously write that content to disk without hvaing
   * to access any of our Json nodes (which are main-thread only).
   */

  g_file_replace_contents_bytes_async (self->file,
                                       bytes,
                                       NULL,
                                       TRUE,
                                       G_FILE_CREATE_REPLACE_DESTINATION,
                                       cancellable,
                                       gbp_flatpak_manifest_save_cb,
                                       g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
gbp_flatpak_manifest_save_finish (GbpFlatpakManifest  *self,
                                  GAsyncResult        *result,
                                  GError             **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

gchar **
gbp_flatpak_manifest_get_runtimes (GbpFlatpakManifest *self,
                                   const gchar        *for_arch)
{
  const gchar *runtime_version = "master";
  const gchar *sdk;
  GPtrArray *ar;

  g_return_val_if_fail (GBP_IS_FLATPAK_MANIFEST (self), NULL);

  ar = g_ptr_array_new ();

  if (for_arch == NULL)
    for_arch = flatpak_get_default_arch ();

  if (self->runtime_version != NULL)
    runtime_version = self->runtime_version;

  if (self->sdk == NULL)
    sdk = self->runtime;
  else
    sdk = self->sdk;

  /* First discover the runtime needed for building */
  g_ptr_array_add (ar, g_strdup_printf ("%s/%s/%s", sdk, for_arch, runtime_version));

#if 0
  /* XXX: Ignore docs for now, since they aren't reliable */
  /* Now add the documentation SDK so the user can get docs */
  g_ptr_array_add (ar, g_strdup_printf ("%s.Docs/%s/%s", sdk, for_arch, runtime_version));
#endif

  /* Now discover the runtime needed for running */
  g_ptr_array_add (ar, g_strdup_printf ("%s/%s/%s", self->runtime, for_arch, runtime_version));

  /* Now discover any necessary SDK extensions */
  if (self->sdk_extensions != NULL)
    {
      for (guint i = 0; self->sdk_extensions[i]; i++)
        g_ptr_array_add (ar, g_strdup_printf ("%s/%s/", self->sdk_extensions[i], for_arch));
    }

  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}
