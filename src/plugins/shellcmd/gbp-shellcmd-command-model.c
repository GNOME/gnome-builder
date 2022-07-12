/* gbp-shellcmd-command-model.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-command-model"

#include "config.h"

#include "gbp-shellcmd-command-model.h"
#include "gbp-shellcmd-run-command.h"

#define SHELLCMD_SETTINGS_BASE "/org/gnome/builder/shellcmd/"

struct _GbpShellcmdCommandModel
{
  GObject      parent_instance;
  GSettings   *settings;
  char        *key;
  GHashTable  *id_to_command;
  char       **ids;
  guint        n_items;
};

enum {
  PROP_0,
  PROP_KEY,
  PROP_SETTINGS,
  PROP_N_ITEMS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gpointer
gbp_shellcmd_command_model_get_item (GListModel *model,
                                     guint       position)
{
  GbpShellcmdCommandModel *self = (GbpShellcmdCommandModel *)model;
  GbpShellcmdRunCommand *command;
  const char *id;

  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (self));

  if (position >= self->n_items)
    return NULL;

  id = self->ids[position];
  command = g_hash_table_lookup (self->id_to_command, id);

  if (command == NULL)
    {
      g_autofree char *base_path = NULL;
      g_autofree char *settings_path = NULL;

      g_object_get (self->settings,
                    "path", &base_path,
                    NULL);
      settings_path = g_strconcat (base_path, id, "/", NULL);
      command = gbp_shellcmd_run_command_new (settings_path);
      g_hash_table_insert (self->id_to_command, g_strdup (id), command);
    }

  return g_object_ref (command);
}

static guint
gbp_shellcmd_command_model_get_n_items (GListModel *model)
{
  return GBP_SHELLCMD_COMMAND_MODEL (model)->n_items;
}

static GType
gbp_shellcmd_command_model_get_item_type (GListModel *model)
{
  return IDE_TYPE_RUN_COMMAND;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = gbp_shellcmd_command_model_get_n_items;
  iface->get_item = gbp_shellcmd_command_model_get_item;
  iface->get_item_type = gbp_shellcmd_command_model_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpShellcmdCommandModel, gbp_shellcmd_command_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
gbp_shellcmd_command_model_replace (GbpShellcmdCommandModel  *self,
                                    char                    **commands)
{
  g_auto(GStrv) old_ids = NULL;
  guint old_len;

  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (self));
  g_assert (self->ids != NULL);
  g_assert (commands != NULL);

  if (g_strv_equal ((const char * const *)self->ids,
                    (const char * const *)commands))
    {
      g_strfreev (commands);
      return;
    }

  old_ids = g_steal_pointer (&self->ids);
  old_len = self->n_items;

  self->ids = g_steal_pointer (&commands);
  self->n_items = g_strv_length (self->ids);

  g_assert (g_strv_length (old_ids) == old_len);
  g_assert (g_strv_length (self->ids) == self->n_items);
  g_assert (g_hash_table_size (self->id_to_command) <= old_len);

  for (guint i = 0; old_ids[i]; i++)
    {
      if (!g_strv_contains ((const char * const *)self->ids, old_ids[i]))
        g_hash_table_remove (self->id_to_command, old_ids[i]);
    }

  g_assert (g_hash_table_size (self->id_to_command) <= self->n_items);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->n_items);

  if (old_len != self->n_items)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ITEMS]);
}

static void
gbp_shellcmd_command_model_settings_changed_cb (GbpShellcmdCommandModel *self,
                                                const char              *key,
                                                GSettings               *settings)
{
  g_auto(GStrv) commands = NULL;

  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (self));
  g_assert (ide_str_equal0 (key, self->key));
  g_assert (G_IS_SETTINGS (settings));

  commands = g_settings_get_strv (settings, self->key);

  gbp_shellcmd_command_model_replace (self, g_steal_pointer (&commands));
}

static void
gbp_shellcmd_command_model_constructed (GObject *object)
{
  GbpShellcmdCommandModel *self = (GbpShellcmdCommandModel *)object;
  g_autofree char *signal_name = NULL;

  G_OBJECT_CLASS (gbp_shellcmd_command_model_parent_class)->constructed (object);

  g_assert (self->key != NULL);
  g_assert (G_IS_SETTINGS (self->settings));

  self->ids = g_settings_get_strv (self->settings, self->key);
  self->n_items = g_strv_length (self->ids);

  signal_name = g_strconcat ("changed::", self->key, NULL);
  g_signal_connect_object (self->settings,
                           signal_name,
                           G_CALLBACK (gbp_shellcmd_command_model_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_shellcmd_command_model_dispose (GObject *object)
{
  GbpShellcmdCommandModel *self = (GbpShellcmdCommandModel *)object;

  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->id_to_command, g_hash_table_unref);
  g_clear_pointer (&self->ids, g_strfreev);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gbp_shellcmd_command_model_parent_class)->dispose (object);
}

static void
gbp_shellcmd_command_model_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbpShellcmdCommandModel *self = GBP_SHELLCMD_COMMAND_MODEL (object);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, self->key);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, self->n_items);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_command_model_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbpShellcmdCommandModel *self = GBP_SHELLCMD_COMMAND_MODEL (object);

  switch (prop_id)
    {
    case PROP_KEY:
      self->key = g_value_dup_string (value);
      break;

    case PROP_SETTINGS:
      self->settings = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shellcmd_command_model_class_init (GbpShellcmdCommandModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_shellcmd_command_model_constructed;
  object_class->dispose = gbp_shellcmd_command_model_dispose;
  object_class->get_property = gbp_shellcmd_command_model_get_property;
  object_class->set_property = gbp_shellcmd_command_model_set_property;

  properties [PROP_KEY] =
    g_param_spec_string ("key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         G_TYPE_SETTINGS,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_shellcmd_command_model_init (GbpShellcmdCommandModel *self)
{
  self->id_to_command = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

GbpShellcmdCommandModel *
gbp_shellcmd_command_model_new (GSettings  *settings,
                                const char *key)
{
  g_autoptr(GSettingsSchema) schema = NULL;

  g_return_val_if_fail (G_SETTINGS (settings), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  g_object_get (settings,
                "settings-schema", &schema,
                NULL);

  g_return_val_if_fail (schema != NULL, NULL);
  g_return_val_if_fail (g_settings_schema_has_key (schema, key), NULL);

  return g_object_new (GBP_TYPE_SHELLCMD_COMMAND_MODEL,
                       "settings", settings,
                       "key", key,
                       NULL);
}

GbpShellcmdCommandModel *
gbp_shellcmd_command_model_new_for_app (void)
{
  g_autoptr(GSettings) settings = NULL;

  settings = g_settings_new_with_path ("org.gnome.builder.shellcmd", SHELLCMD_SETTINGS_BASE);
  return gbp_shellcmd_command_model_new (settings, "run-commands");
}

GbpShellcmdCommandModel *
gbp_shellcmd_command_model_new_for_project (IdeContext *context)
{
  g_autofree char *project_id = NULL;
  g_autofree char *project_settings_path = NULL;
  g_autoptr(GSettings) settings = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  project_id = ide_context_dup_project_id (context);
  project_settings_path = g_strconcat (SHELLCMD_SETTINGS_BASE"projects/", project_id, "/", NULL);
  settings = g_settings_new_with_path ("org.gnome.builder.shellcmd", project_settings_path);

  return gbp_shellcmd_command_model_new (settings, "run-commands");
}

GbpShellcmdRunCommand *
gbp_shellcmd_run_command_create (IdeContext *context)
{
  g_autofree char *uuid = NULL;
  g_autofree char *project_id = NULL;
  g_autofree char *settings_path = NULL;
  g_autofree char *parent_path = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_auto(GStrv) strv = NULL;

  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);

  uuid = g_uuid_string_random ();
  if (context != NULL)
    project_id = ide_context_dup_project_id (context);

  if (project_id == NULL)
    parent_path = g_strdup (SHELLCMD_SETTINGS_BASE);
  else
    parent_path = g_strconcat (SHELLCMD_SETTINGS_BASE"projects/", project_id, "/", NULL);

  settings_path = g_strconcat (parent_path, uuid, "/", NULL);
  settings = g_settings_new_with_path ("org.gnome.builder.shellcmd", parent_path);
  strv = g_settings_get_strv (settings, "run-commands");

  builder = g_strv_builder_new ();
  g_strv_builder_addv (builder, (const char **)strv);
  g_strv_builder_add (builder, uuid);

  g_clear_pointer (&strv, g_strfreev);
  strv = g_strv_builder_end (builder);

  g_settings_set_strv (settings, "run-commands", (const char * const *)strv);

  return gbp_shellcmd_run_command_new (settings_path);
}
