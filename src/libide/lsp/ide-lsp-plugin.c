/* ide-lsp-plugin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-lsp-plugin"

#include "config.h"

#include <json-glib/json-glib.h>
#include <libide-search.h>

#include "ide-lsp-diagnostic-provider.h"
#include "ide-lsp-symbol-resolver.h"
#include "ide-lsp-highlighter.h"
#include "ide-lsp-formatter.h"
#include "ide-lsp-hover-provider.h"
#include "ide-lsp-rename-provider.h"
#include "ide-lsp-code-action-provider.h"
#include "ide-lsp-plugin-private.h"
#include "ide-lsp-service.h"

typedef enum _IdeLspPluginFeatures
{
  IDE_LSP_PLUGIN_FEATURES_DIAGNOSTICS     = 1 << 0,
  IDE_LSP_PLUGIN_FEATURES_COMPLETION      = 1 << 1,
  IDE_LSP_PLUGIN_FEATURES_SYMBOL_RESOLVER = 1 << 2,
  IDE_LSP_PLUGIN_FEATURES_HIGHLIGHTER     = 1 << 3,
  IDE_LSP_PLUGIN_FEATURES_FORMATTER       = 1 << 4,
  IDE_LSP_PLUGIN_FEATURES_HOVER           = 1 << 5,
  IDE_LSP_PLUGIN_FEATURES_RENAME          = 1 << 6,
  IDE_LSP_PLUGIN_FEATURES_CODE_ACTION     = 1 << 7,
  IDE_LSP_PLUGIN_FEATURES_SEARCH          = 1 << 8,
  IDE_LSP_PLUGIN_FEATURES_ALL             = ~0,
} IdeLspPluginFeatures;

IdeLspPluginInfo *
ide_lsp_plugin_info_ref (IdeLspPluginInfo *self)
{
  return g_atomic_rc_box_acquire (self);
}

static void
ide_lsp_plugin_info_finalize (gpointer data)
{
  IdeLspPluginInfo *self = data;

  g_clear_pointer (&self->command, g_strfreev);
  g_clear_pointer (&self->languages, g_strfreev);
  g_clear_pointer (&self->default_settings, g_bytes_unref);
  g_clear_pointer (&self->module_name, g_free);
}

void
ide_lsp_plugin_info_unref (IdeLspPluginInfo *info)
{
  g_atomic_rc_box_release_full (info, ide_lsp_plugin_info_finalize);
}

static IdeLspPluginInfo *
ide_lsp_plugin_info_new (void)
{
  return g_atomic_rc_box_new0 (IdeLspPluginInfo);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeLspPluginInfo, ide_lsp_plugin_info_unref)

static GBytes *
load_bytes (const char *path)
{
  if (path == NULL)
    {
      return NULL;
    }
  else if (g_str_has_prefix (path, "resource://"))
    {
      return g_resources_lookup_data (path + strlen("resource://"), 0, NULL);
    }
  else
    {
      g_autoptr(GFile) file = g_file_new_for_path (path);
      return g_file_load_bytes (file, NULL, NULL, NULL);
    }
}

typedef struct _IdeLspPluginService
{
  IdeLspService parent_instance;
} IdeLspPluginService;

typedef struct _IdeLspPluginServiceClass
{
  IdeLspServiceClass  parent_class;
  IdeLspPluginInfo   *info;
} IdeLspPluginServiceClass;

static void
ide_lsp_plugin_service_configure_client (IdeLspService *service,
                                         IdeLspClient  *client)
{
  IdeLspPluginService *self = (IdeLspPluginService *)service;
  IdeLspPluginServiceClass *klass = (IdeLspPluginServiceClass *)((GTypeInstance *)service)->g_class;
  IdeContext *context;

  g_assert (IDE_IS_LSP_SERVICE (self));
  g_assert (IDE_IS_LSP_CLIENT (client));

  if (klass->info->languages != NULL)
    {
      for (guint i = 0; klass->info->languages[i]; i++)
        {
          const char *language = klass->info->languages[i];

          if (!ide_str_empty0 (language))
            ide_lsp_client_add_language (client, language);
        }
    }

  if (!(context = ide_object_get_context (IDE_OBJECT (service))))
    return;

  if (klass->info->default_settings)
    {
      g_autoptr (JsonParser) parser = NULL;
      g_autoptr (GVariant) init_options = NULL;
      g_autoptr (GError) error = NULL;
      const char *data;
      gsize size;
      JsonNode *root;
      JsonObject *root_obj;
      JsonObject *plugin_obj;
      JsonNode *init_node;

      data = g_bytes_get_data (klass->info->default_settings, &size);

      parser = json_parser_new ();
      if (!json_parser_load_from_data (parser, data, size, &error))
        {
          g_debug ("Could not parse %s settings.json: %s",
                   klass->info->module_name, error->message);
          return;
        }

      if (!(root = json_parser_get_root (parser)) ||
          !JSON_NODE_HOLDS_OBJECT (root) ||
          !(root_obj = json_node_get_object (root)) ||
          !(plugin_obj = json_object_get_object_member (root_obj, klass->info->module_name)) ||
          !json_object_has_member (plugin_obj, "initializationOptions"))
        {
          g_debug ("settings.json not valid for %s",
                   klass->info->module_name);
          return;
        }

      init_node = json_object_get_member (plugin_obj, "initializationOptions");
      init_options = json_gvariant_deserialize (init_node, NULL, &error);

      if (!init_options)
        {
          g_debug ("Could not deserialize %s initializationOptions: %s",
                   klass->info->module_name, error->message);
          return;
        }

      ide_lsp_client_set_initialization_options (client, g_steal_pointer (&init_options));
    }
}

static void
ide_lsp_plugin_service_prepare_run_context (IdeLspService *service,
                                            IdePipeline   *pipeline,
                                            IdeRunContext *run_context)
{
  IdeLspPluginServiceClass *klass;

  g_assert (IDE_IS_LSP_SERVICE (service));
  g_assert (!pipeline || IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  klass = (IdeLspPluginServiceClass *)G_OBJECT_GET_CLASS (service);

  if (klass->info->command[0] && klass->info->command[1])
    ide_run_context_append_args (run_context, (const char * const *)&klass->info->command[1]);
}

static void
ide_lsp_plugin_service_class_init (IdeLspPluginServiceClass *klass,
                                   IdeLspPluginInfo         *info)
{
  IdeLspServiceClass *service_class = IDE_LSP_SERVICE_CLASS (klass);

  klass->info = info;

  service_class->configure_client = ide_lsp_plugin_service_configure_client;
  service_class->prepare_run_context = ide_lsp_plugin_service_prepare_run_context;
}

static void
ide_lsp_plugin_service_init (IdeLspPluginService      *self,
                             IdeLspPluginServiceClass *klass)
{
  ide_lsp_service_set_program (IDE_LSP_SERVICE (self), klass->info->command[0]);
}

static GType
ide_lsp_plugin_register_service_gtype (IdeLspPluginInfo *info)
{
  g_autofree char *type_name = g_strconcat (info->module_name, "+IdeLspPluginService", NULL);
  GTypeInfo type_info =
  {
    sizeof (IdeLspPluginServiceClass),
    NULL,
    NULL,
    (GClassInitFunc)ide_lsp_plugin_service_class_init,
    NULL,
    ide_lsp_plugin_info_ref (info),
    sizeof (IdeLspPluginService),
    0,
    (GInstanceInitFunc)ide_lsp_plugin_service_init,
    NULL,
  };
  return g_type_register_static (IDE_TYPE_LSP_SERVICE, type_name, &type_info, G_TYPE_FLAG_FINAL);
}

static void
ide_lsp_plugin_register (PeasObjectModule     *object_module,
                         IdeLspPluginFeatures  features,
                         const char * const   *command,
                         const char * const   *languages,
                         GBytes               *default_settings)
{
  g_autoptr(IdeLspPluginInfo) info = NULL;

  g_return_if_fail (PEAS_IS_OBJECT_MODULE (object_module));

  info = ide_lsp_plugin_info_new ();
  info->module_name = g_strdup (peas_object_module_get_module_name (object_module));
  info->command = g_strdupv ((char **)command);
  info->languages = g_strdupv ((char **)languages);
  info->default_settings = default_settings ? g_bytes_ref (default_settings) : NULL;
  info->service_type = ide_lsp_plugin_register_service_gtype (info);

  g_log (info->module_name,
         G_LOG_LEVEL_DEBUG,
         "Registered type %s",
         g_type_name (info->service_type));

  if ((features & IDE_LSP_PLUGIN_FEATURES_DIAGNOSTICS) != 0)
    peas_object_module_register_extension_factory (object_module,
                                                   IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                                   (PeasFactoryFunc)ide_lsp_plugin_create_diagnostic_provider,
                                                   ide_lsp_plugin_info_ref (info),
                                                   (GDestroyNotify)ide_lsp_plugin_info_unref);

  if ((features & IDE_LSP_PLUGIN_FEATURES_COMPLETION) != 0)
    peas_object_module_register_extension_factory (object_module,
                                                   GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                                   (PeasFactoryFunc)ide_lsp_plugin_create_completion_provider,
                                                   ide_lsp_plugin_info_ref (info),
                                                   (GDestroyNotify)ide_lsp_plugin_info_unref);

  if ((features & IDE_LSP_PLUGIN_FEATURES_SYMBOL_RESOLVER) != 0)
    peas_object_module_register_extension_factory (object_module,
                                                   IDE_TYPE_SYMBOL_RESOLVER,
                                                   (PeasFactoryFunc)ide_lsp_plugin_create_symbol_resolver,
                                                   ide_lsp_plugin_info_ref (info),
                                                   (GDestroyNotify)ide_lsp_plugin_info_unref);

  if ((features & IDE_LSP_PLUGIN_FEATURES_HIGHLIGHTER) != 0)
    peas_object_module_register_extension_factory (object_module,
                                                   IDE_TYPE_HIGHLIGHTER,
                                                   (PeasFactoryFunc)ide_lsp_plugin_create_highlighter,
                                                   ide_lsp_plugin_info_ref (info),
                                                   (GDestroyNotify)ide_lsp_plugin_info_unref);

  if ((features & IDE_LSP_PLUGIN_FEATURES_FORMATTER) != 0)
    peas_object_module_register_extension_factory (object_module,
                                                   IDE_TYPE_FORMATTER,
                                                   (PeasFactoryFunc)ide_lsp_plugin_create_formatter,
                                                   ide_lsp_plugin_info_ref (info),
                                                   (GDestroyNotify)ide_lsp_plugin_info_unref);

  if ((features & IDE_LSP_PLUGIN_FEATURES_HOVER) != 0)
    peas_object_module_register_extension_factory (object_module,
                                                   GTK_SOURCE_TYPE_HOVER_PROVIDER,
                                                   (PeasFactoryFunc)ide_lsp_plugin_create_hover_provider,
                                                   ide_lsp_plugin_info_ref (info),
                                                   (GDestroyNotify)ide_lsp_plugin_info_unref);

  if ((features & IDE_LSP_PLUGIN_FEATURES_RENAME) != 0)
    peas_object_module_register_extension_factory (object_module,
                                                   IDE_TYPE_RENAME_PROVIDER,
                                                   (PeasFactoryFunc)ide_lsp_plugin_create_rename_provider,
                                                   ide_lsp_plugin_info_ref (info),
                                                   (GDestroyNotify)ide_lsp_plugin_info_unref);

  if ((features & IDE_LSP_PLUGIN_FEATURES_CODE_ACTION) != 0)
    peas_object_module_register_extension_factory (object_module,
                                                   IDE_TYPE_CODE_ACTION_PROVIDER,
                                                   (PeasFactoryFunc)ide_lsp_plugin_create_code_action_provider,
                                                   ide_lsp_plugin_info_ref (info),
                                                   (GDestroyNotify)ide_lsp_plugin_info_unref);

  if ((features & IDE_LSP_PLUGIN_FEATURES_SEARCH) != 0)
    peas_object_module_register_extension_factory (object_module,
                                                   IDE_TYPE_SEARCH_PROVIDER,
                                                   (PeasFactoryFunc)ide_lsp_plugin_create_search_provider,
                                                   ide_lsp_plugin_info_ref (info),
                                                   (GDestroyNotify)ide_lsp_plugin_info_unref);
}

static inline gboolean
has_metadata (PeasPluginInfo *plugin_info,
              const char     *key)
{
  const char *str = peas_plugin_info_get_external_data (plugin_info, key);
  return !ide_str_empty0 (str);
}

void
ide_lsp_plugin_register_types (PeasObjectModule *object_module)
{
  g_autofree char *x_lsp_languages = NULL;
  g_autofree char *settings_path = NULL;
  g_autoptr(GBytes) default_settings = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;
  g_auto(GStrv) languages = NULL;
  IdeLspPluginFeatures features = 0;
  PeasPluginInfo *plugin_info;
  PeasEngine *engine;
  const char *data_dir;
  const char *module_name;
  const char *x_lsp_command;
  const char *x_lsp_settings;
  int argc;

  g_return_if_fail (PEAS_IS_OBJECT_MODULE (object_module));

  engine = peas_engine_get_default ();
  module_name = peas_object_module_get_module_name (object_module);
  plugin_info = peas_engine_get_plugin_info (engine, module_name);
  x_lsp_command = peas_plugin_info_get_external_data (plugin_info, "LSP-Command");
  x_lsp_languages = g_strdup (peas_plugin_info_get_external_data (plugin_info, "LSP-Languages"));
  x_lsp_settings = peas_plugin_info_get_external_data (plugin_info, "LSP-Settings");
  data_dir = peas_plugin_info_get_data_dir (plugin_info);

  if (x_lsp_command == NULL)
    {
      g_critical ("Plugin %s missing X-LSP-Command=", module_name);
      return;
    }

  if (x_lsp_languages == NULL)
    {
      g_critical ("Plugin %s missing X-LSP-Languages=", module_name);
      return;
    }

  languages = g_strsplit (g_strdelimit (x_lsp_languages, ",", ';'), ";", 0);

  if (!g_shell_parse_argv (x_lsp_command, &argc, &argv, &error))
    {
      g_critical ("Plugin %s provides invalid X-LSP-Command=%s: %s",
                  module_name, x_lsp_command, error->message);
      return;
    }

  if (x_lsp_settings == NULL)
    x_lsp_settings = "settings.json";

  settings_path = g_build_filename (data_dir, x_lsp_settings, NULL);
  default_settings = load_bytes (settings_path);

  /* Figure out what features this LSP supports based on the X-* metadata
   * values. We require that they are set in the .plugin or they will not
   * have dynamic subtypes created.
   */
  if (has_metadata (plugin_info, "Code-Action-Languages"))
    features |= IDE_LSP_PLUGIN_FEATURES_CODE_ACTION;
  if (has_metadata (plugin_info, "Completion-Provider-Languages"))
    features |= IDE_LSP_PLUGIN_FEATURES_COMPLETION;
  if (has_metadata (plugin_info, "Diagnostic-Provider-Languages"))
    features |= IDE_LSP_PLUGIN_FEATURES_DIAGNOSTICS;
  if (has_metadata (plugin_info, "Formatter-Languages"))
    features |= IDE_LSP_PLUGIN_FEATURES_FORMATTER;
  if (has_metadata (plugin_info, "Highlighter-Languages"))
    features |= IDE_LSP_PLUGIN_FEATURES_HIGHLIGHTER;
  if (has_metadata (plugin_info, "Hover-Provider-Languages"))
    features |= IDE_LSP_PLUGIN_FEATURES_HOVER;
  if (has_metadata (plugin_info, "Rename-Provider-Languages"))
    features |= IDE_LSP_PLUGIN_FEATURES_RENAME;
  if (has_metadata (plugin_info, "Symbol-Resolver-Languages"))
    features |= IDE_LSP_PLUGIN_FEATURES_SYMBOL_RESOLVER;

  /* Always turn on search, and we should dynamically disable it if the client
   * does not support it's capabilities (workspace/symbol currently). This is
   * lazy bound to client creation, so it only has a client if the LSP client
   * is created through some other means.
   */
  features |= IDE_LSP_PLUGIN_FEATURES_SEARCH;

  if (features == 0)
    g_warning ("LSP plugin %s contains no requested LSP features. "
               "Make sure you've set X-Diagnostic-Provider-Lanaguages and other metadata.",
               module_name);

  ide_lsp_plugin_register (object_module,
                           features,
                           (const char * const *)argv,
                           (const char * const *)languages,
                           default_settings);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
void
ide_lsp_plugin_remove_plugin_info_param (guint      *n_parameters,
                                         GParameter *parameters)
{
  static GType plugin_info_type;
  const GParameter *param;

  if (*n_parameters == 0)
    return;

  if G_UNLIKELY (plugin_info_type == G_TYPE_INVALID)
    plugin_info_type = PEAS_TYPE_PLUGIN_INFO;

  param = &parameters[(*n_parameters) - 1];

  if (G_VALUE_TYPE (&param->value) == plugin_info_type &&
      strcmp (param->name, "plugin-info") == 0)
    (*n_parameters)--;
}
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
