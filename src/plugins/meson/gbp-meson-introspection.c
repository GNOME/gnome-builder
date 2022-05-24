/* gbp-meson-introspection.c
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

#define G_LOG_DOMAIN "gbp-meson-introspection"

#include "config.h"

#include <json-glib/json-glib.h>

#include <libide-core.h>
#include <libide-threading.h>

#include "gbp-meson-introspection.h"

struct _GbpMesonIntrospection
{
  GObject parent_instance;

  char *descriptive_name;
  char *subproject_dir;
  char *version;

  guint loaded : 1;
};

G_DEFINE_FINAL_TYPE (GbpMesonIntrospection, gbp_meson_introspection, G_TYPE_OBJECT)

static void
gbp_meson_introspection_dispose (GObject *object)
{
  GbpMesonIntrospection *self = (GbpMesonIntrospection *)object;

  g_clear_pointer (&self->descriptive_name, g_free);
  g_clear_pointer (&self->subproject_dir, g_free);
  g_clear_pointer (&self->version, g_free);

  G_OBJECT_CLASS (gbp_meson_introspection_parent_class)->dispose (object);
}

static void
gbp_meson_introspection_class_init (GbpMesonIntrospectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_meson_introspection_dispose;
}

static void
gbp_meson_introspection_init (GbpMesonIntrospection *self)
{
}

static gboolean
get_string_member (JsonObject  *object,
                   const char  *member,
                   char       **location)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);
  g_assert (*location == NULL);

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_VALUE (node))
    {
      *location = g_strdup (json_node_get_string (node));
      return TRUE;
    }

  return FALSE;
}

static gboolean
get_strv_member (JsonObject   *object,
                 const char   *member,
                 char       ***location)
{
  JsonNode *node;
  JsonArray *ar;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);
  g_assert (*location == NULL);

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_ARRAY (node) &&
      (ar = json_node_get_array (node)))
    {
      GPtrArray *strv = g_ptr_array_new ();
      guint n_items = json_array_get_length (ar);

      for (guint i = 0; i < n_items; i++)
        {
          JsonNode *ele = json_array_get_element (ar, i);
          const char *str;

          if (JSON_NODE_HOLDS_VALUE (ele) &&
              (str = json_node_get_string (ele)))
            g_ptr_array_add (strv, g_strdup (str));
        }

      g_ptr_array_add (strv, NULL);

      *location = (char **)(gpointer)g_ptr_array_free (strv, FALSE);

      return TRUE;
    }

  return FALSE;
}

static gboolean
get_environ_member (JsonObject   *object,
                    const char   *member,
                    char       ***location)
{
  JsonNode *node;
  JsonObject *envobj;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);
  g_assert (*location == NULL);

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_OBJECT (node) &&
      (envobj = json_node_get_object (node)))
    {
      JsonObjectIter iter;
      const char *key;
      JsonNode *value_node;

      json_object_iter_init (&iter, envobj);
      while (json_object_iter_next (&iter, &key, &value_node))
        {
          const char *value;

          if (!JSON_NODE_HOLDS_VALUE (value_node) ||
              !(value = json_node_get_string (value_node)))
            continue;

          *location = g_environ_setenv (*location, key, value, TRUE);
        }

      return TRUE;
    }

  return FALSE;
}

static void
gbp_meson_introspection_load_buildoptions (GbpMesonIntrospection *self,
                                           JsonArray             *buildoptions)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (buildoptions != NULL);

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_projectinfo (GbpMesonIntrospection *self,
                                          JsonObject            *projectinfo)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (projectinfo != NULL);

  get_string_member (projectinfo, "version", &self->version);
  get_string_member (projectinfo, "descriptive_name", &self->descriptive_name);
  get_string_member (projectinfo, "subproject_dir", &self->subproject_dir);

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_test (GbpMesonIntrospection *self,
                                   JsonObject            *test)
{
  g_auto(GStrv) cmd = NULL;
  g_auto(GStrv) env = NULL;
  g_auto(GStrv) suite = NULL;
  g_autofree char *name = NULL;
  g_autofree char *workdir = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (test != NULL);

  get_strv_member (test, "cmd", &cmd);
  get_strv_member (test, "suite", &suite);
  get_environ_member (test, "env", &env);
  get_string_member (test, "name", &name);
  get_string_member (test, "workdir", &workdir);

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_tests (GbpMesonIntrospection *self,
                                    JsonArray             *tests)
{
  guint n_items;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (tests != NULL);

  n_items = json_array_get_length (tests);

  for (guint i = 0; i < n_items; i++)
    {
      JsonNode *node = json_array_get_element (tests, i);
      JsonObject *obj;

      if (node != NULL &&
          JSON_NODE_HOLDS_OBJECT (node) &&
          (obj = json_node_get_object (node)))
        gbp_meson_introspection_load_test (self, obj);
    }

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_benchmarks (GbpMesonIntrospection *self,
                                         JsonArray             *benchmarks)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (benchmarks != NULL);

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_installed (GbpMesonIntrospection *self,
                                        JsonObject            *installed)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (installed != NULL);

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_file_worker (IdeTask      *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable)
{
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  GbpMesonIntrospection *self = source_object;
  const char *filename = task_data;
  JsonObject *obj;
  JsonNode *root;
  JsonNode *member;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (filename != NULL);

  g_debug ("Loading meson introspection from %s", filename);

  parser = json_parser_new ();

  if (!json_parser_load_from_mapped_file (parser, filename, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_OBJECT (root) ||
      !(obj = json_node_get_object (root)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Root json node is not an object");
      IDE_EXIT;
    }

  if (json_object_has_member (obj, "buildoptions") &&
      (member = json_object_get_member (obj, "buildoptions")) &&
      JSON_NODE_HOLDS_ARRAY (member))
    gbp_meson_introspection_load_buildoptions (self, json_node_get_array (member));

  if (json_object_has_member (obj, "projectinfo") &&
      (member = json_object_get_member (obj, "projectinfo")) &&
      JSON_NODE_HOLDS_OBJECT (member))
    gbp_meson_introspection_load_projectinfo (self, json_node_get_object (member));

  if (json_object_has_member (obj, "tests") &&
      (member = json_object_get_member (obj, "tests")) &&
      JSON_NODE_HOLDS_ARRAY (member))
    gbp_meson_introspection_load_tests (self, json_node_get_array (member));

  if (json_object_has_member (obj, "benchmarks") &&
      (member = json_object_get_member (obj, "benchmarks")) &&
      JSON_NODE_HOLDS_ARRAY (member))
    gbp_meson_introspection_load_benchmarks (self, json_node_get_array (member));

  if (json_object_has_member (obj, "installed") &&
      (member = json_object_get_member (obj, "installed")) &&
      JSON_NODE_HOLDS_OBJECT (member))
    gbp_meson_introspection_load_installed (self, json_node_get_object (member));

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

GbpMesonIntrospection *
gbp_meson_introspection_new (void)
{
  return g_object_new (GBP_TYPE_MESON_INTROSPECTION, NULL);
}

void
gbp_meson_introspection_load_file_async (GbpMesonIntrospection *self,
                                         const char            *path,
                                         GCancellable          *cancellable,
                                         GAsyncReadyCallback    callback,
                                         gpointer               user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_MESON_INTROSPECTION (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (task, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_introspection_load_file_async);

  if (self->loaded)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Already initialized");
      IDE_EXIT;
    }

  self->loaded = TRUE;

  ide_task_set_task_data (task, g_strdup (path), g_free);
  ide_task_run_in_thread (task, gbp_meson_introspection_load_file_worker);

  IDE_EXIT;
}

gboolean
gbp_meson_introspection_load_file_finish (GbpMesonIntrospection *self,
                                          GAsyncResult *result,
                                          GError **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_MESON_INTROSPECTION (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}
