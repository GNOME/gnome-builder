/* gbp-flatpak-util.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-util"

#include <string.h>
#include <libide-foundry.h>
#include <libide-vcs.h>
#include <yaml.h>

#include "gbp-flatpak-client.h"
#include "gbp-flatpak-util.h"

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (yaml_parser_t, yaml_parser_delete)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (yaml_document_t, yaml_document_delete)

gchar *
gbp_flatpak_get_repo_dir (IdeContext *context)
{
  return ide_context_cache_filename (context, "flatpak", "repo", NULL);
}

gchar *
gbp_flatpak_get_staging_dir (IdePipeline *pipeline)
{
  g_autofree char *branch = NULL;
  g_autofree char *name = NULL;
  g_autofree char *arch = NULL;
  g_autoptr (IdeTriplet) triplet = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeVcs) vcs = NULL;
  g_autoptr(IdeToolchain) toolchain = NULL;

  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_ref_context (IDE_OBJECT (pipeline));
  vcs = ide_vcs_ref_from_context (context);
  branch = ide_vcs_get_branch_name (vcs);
  arch = ide_pipeline_dup_arch (pipeline);
  name = g_strdup_printf ("%s-%s", arch, branch);

  g_strdelimit (name, G_DIR_SEPARATOR_S, '-');

  return ide_context_cache_filename (context, "flatpak", "staging", name, NULL);
}

gboolean
gbp_flatpak_is_ignored (const gchar *name)
{
  if (name == NULL)
    return TRUE;

  return g_str_has_suffix (name, ".Locale") ||
         g_str_has_suffix (name, ".Debug") ||
         g_str_has_suffix (name, ".Docs") ||
         g_str_has_suffix (name, ".Sources") ||
         g_str_has_suffix (name, ".Var") ||
         g_str_has_prefix (name, "org.gtk.Gtk3theme.") ||
         strstr (name, ".GL.nvidia") != NULL ||
         strstr (name, ".GL32.nvidia") != NULL ||
         strstr (name, ".VAAPI") != NULL ||
         strstr (name, ".Icontheme") != NULL ||
         strstr (name, ".Extension") != NULL ||
         strstr (name, ".Gtk3theme") != NULL ||
         strstr (name, ".KStyle") != NULL ||
         strstr (name, ".PlatformTheme") != NULL ||
         strstr (name, ".openh264") != NULL;
}

static gboolean
_gbp_flatpak_split_id (const gchar  *str,
                       gchar       **id,
                       gchar       **arch,
                       gchar       **branch)
{
  g_auto(GStrv) parts = g_strsplit (str, "/", 0);
  guint i = 0;

  if (id)
    *id = NULL;

  if (arch)
    *arch = NULL;

  if (branch)
    *branch = NULL;

  if (parts[i] != NULL)
    {
      if (id != NULL)
        *id = g_strdup (parts[i]);
    }
  else
    {
      /* we require at least a runtime/app ID */
      return FALSE;
    }

  i++;

  if (parts[i] != NULL)
    {
      if (arch != NULL)
        *arch = g_strdup (parts[i]);
    }
  else
    return TRUE;

  i++;

  if (parts[i] != NULL)
    {
      if (branch != NULL && !ide_str_empty0 (parts[i]))
        *branch = g_strdup (parts[i]);
    }

  return TRUE;
}

gboolean
gbp_flatpak_split_id (const gchar  *str,
                      gchar       **id,
                      gchar       **arch,
                      gchar       **branch)
{
  if (g_str_has_prefix (str, "runtime/"))
    str += strlen ("runtime/");
  else if (g_str_has_prefix (str, "app/"))
    str += strlen ("app/");

  return _gbp_flatpak_split_id (str, id, arch, branch);
}

static char *
_gbp_flatpak_get_default_arch (void)
{
  /* Just statically determine the most common ones */
#if defined(__x86_64__) || defined(_M_X64)
  return g_strdup ("x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
  return g_strdup ("aarch64");
#else
  GbpFlatpakClient *client = gbp_flatpak_client_get_default ();
  IpcFlatpakService *service = gbp_flatpak_client_get_service (client, NULL, NULL);

  if (service != NULL)
    return g_strdup (ipc_flatpak_service_get_default_arch (service));

  return ide_get_system_arch ();
#endif
}

const char *
gbp_flatpak_get_default_arch (void)
{
  static char *default_arch;

  if (default_arch == NULL)
    default_arch = _gbp_flatpak_get_default_arch ();

  return default_arch;
}

void
gbp_flatpak_set_config_dir (IdeRunContext *run_context)
{
  g_autofree char *path = ide_dup_default_cache_dir ();
  g_autofree char *flatpak_etc = g_build_filename (path, "flatpak", "etc", NULL);

  ide_run_context_setenv (run_context, "FLATPAK_CONFIG_DIR", flatpak_etc);
}

static char *
_gbp_flatpak_get_a11y_bus (void)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;
  g_autofree char *a11y_bus = NULL;
  g_autoptr(GVariant) variant = NULL;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_push_args (launcher,
                                     IDE_STRV_INIT ("gdbus",
                                                    "call",
                                                    "--session",
                                                    "--dest=org.a11y.Bus",
                                                    "--object-path=/org/a11y/bus",
                                                    "--method=org.a11y.Bus.GetAddress"));
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    goto handle_error;

  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, &error))
    goto handle_error;

  if (!(variant = g_variant_parse (G_VARIANT_TYPE ("(s)"), stdout_buf, NULL, NULL, &error)))
    goto handle_error;

  g_variant_take_ref (variant);
  g_variant_get (variant, "(s)", &a11y_bus, NULL);

  g_debug ("Accessibility bus discovered at %s", a11y_bus);

  return g_steal_pointer (&a11y_bus);

handle_error:
  g_critical ("Failed to detect a11y bus on host: %s", error->message);

  return NULL;
}

const char *
gbp_flatpak_get_a11y_bus (const char **out_unix_path,
                          const char **out_address_suffix)
{
  static char *a11y_bus;
  static char *a11y_bus_path;
  static const char *a11y_bus_suffix;
  static gsize initialized;

  if (g_once_init_enter (&initialized))
    {
      char *address = _gbp_flatpak_get_a11y_bus ();

      if (address != NULL && g_str_has_prefix (address, "unix:path="))
        {
          const char *skip = address + strlen ("unix:path=");

          if ((a11y_bus_suffix = strchr (skip, ',')))
            a11y_bus_path = g_strndup (skip, a11y_bus_suffix - skip);
          else
            a11y_bus_path = g_strdup (skip);
        }

      a11y_bus = address;

      g_once_init_leave (&initialized, TRUE);
    }

#if 0
  g_print ("a11y_bus=%s\n", a11y_bus);
  g_print ("a11y_bus_path=%s\n", a11y_bus_path);
  g_print ("a11y_bus_suffix=%s\n", a11y_bus_suffix);
#endif

  if (out_unix_path != NULL)
    *out_unix_path = a11y_bus_path;

  if (out_address_suffix != NULL)
    *out_address_suffix = a11y_bus_suffix;

  return a11y_bus;
}


static JsonNode *
parse_yaml_node_to_json (yaml_document_t *doc,
                         yaml_node_t     *node)
{
  JsonNode *json = json_node_alloc ();
  const char *scalar = NULL;
  g_autoptr(JsonArray) array = NULL;
  g_autoptr(JsonObject) object = NULL;
  yaml_node_item_t *item = NULL;
  yaml_node_pair_t *pair = NULL;

  g_assert (doc);
  g_assert (node);

  switch (node->type)
    {
    case YAML_NO_NODE:
      json_node_init_null (json);
      break;
    case YAML_SCALAR_NODE:
      scalar = (gchar *) node->data.scalar.value;
      if (node->data.scalar.style == YAML_PLAIN_SCALAR_STYLE)
        {
          if (strcmp (scalar, "true") == 0)
            {
              json_node_init_boolean (json, TRUE);
              break;
            }
          else if (strcmp (scalar, "false") == 0)
            {
              json_node_init_boolean (json, FALSE);
              break;
            }
          else if (strcmp (scalar, "null") == 0)
            {
              json_node_init_null (json);
              break;
            }

          if (*scalar != '\0')
            {
              gchar *endptr;
              gint64 num = g_ascii_strtoll (scalar, &endptr, 10);
              if (*endptr == '\0')
                {
                  json_node_init_int (json, num);
                  break;
                }
              // Make sure that N.N, N., and .N (where N is a digit) are picked up as numbers.
              else if (*endptr == '.' && (endptr != scalar || endptr[1] != '\0'))
                {
                  g_ascii_strtoll (endptr + 1, &endptr, 10);
                  if (*endptr == '\0')
                    g_warning ("%zu:%zu: '%s' will be parsed as a number by many YAML parsers",
                               node->start_mark.line + 1, node->start_mark.column + 1, scalar);
                }
            }
        }

      json_node_init_string (json, scalar);
      break;
    case YAML_SEQUENCE_NODE:
      array = json_array_new ();
      for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++)
        {
          yaml_node_t *child = yaml_document_get_node (doc, *item);
          if (child != NULL)
            json_array_add_element (array, parse_yaml_node_to_json (doc, child));
        }

      json_node_init_array (json, array);
      break;
    case YAML_MAPPING_NODE:
      object = json_object_new ();
      for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++)
        {
          yaml_node_t *key = yaml_document_get_node (doc, pair->key);
          yaml_node_t *value = yaml_document_get_node (doc, pair->value);

          g_warn_if_fail (key->type == YAML_SCALAR_NODE);
          json_object_set_member (object, (gchar *) key->data.scalar.value,
                                  parse_yaml_node_to_json (doc, value));
        }

      json_node_init_object (json, object);
      break;
    default:
      g_assert_not_reached ();
    }

  return json;
}

JsonNode *
gbp_flatpak_yaml_to_json (const gchar  *contents,
                          gsize         len,
                          GError      **error)
{
  g_auto(yaml_parser_t) parser = {0};
  g_auto(yaml_document_t) doc = {{0}};
  yaml_node_t *root;

  if (!yaml_parser_initialize (&parser))
    g_error ("yaml_parser_initialize is out of memory.");

  yaml_parser_set_input_string (&parser, (yaml_char_t *) contents, len);

  if (!yaml_parser_load (&parser, &doc))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "%zu:%zu: %s", parser.problem_mark.line + 1,
                   parser.problem_mark.column + 1, parser.problem);
      return NULL;
    }

  root = yaml_document_get_root_node (&doc);
  if (root == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Document has no root node.");
      return NULL;
    }

  return parse_yaml_node_to_json (&doc, root);
}
