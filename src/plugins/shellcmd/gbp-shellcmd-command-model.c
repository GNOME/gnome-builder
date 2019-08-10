/* gbp-shellcmd-command-model.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>
#include <libide-sourceview.h>
#include <libide-threading.h>

#include "gbp-shellcmd-command.h"
#include "gbp-shellcmd-command-model.h"

struct _GbpShellcmdCommandModel
{
  GObject    parent_instance;
  GPtrArray *items;
  GKeyFile  *keyfile;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpShellcmdCommandModel, gbp_shellcmd_command_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
gbp_shellcmd_command_model_finalize (GObject *object)
{
  GbpShellcmdCommandModel *self = (GbpShellcmdCommandModel *)object;

  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_shellcmd_command_model_parent_class)->finalize (object);
}

static void
gbp_shellcmd_command_model_class_init (GbpShellcmdCommandModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_shellcmd_command_model_finalize;
}

static void
gbp_shellcmd_command_model_init (GbpShellcmdCommandModel *self)
{
  self->items = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_object_unref_and_destroy);
}

GbpShellcmdCommandModel *
gbp_shellcmd_command_model_new (void)
{
  return g_object_new (GBP_TYPE_SHELLCMD_COMMAND_MODEL, NULL);
}

static GType
gbp_shellcmd_command_model_get_item_type (GListModel *model)
{
  return GBP_TYPE_SHELLCMD_COMMAND;
}

static gpointer
gbp_shellcmd_command_model_get_item (GListModel *model,
                                     guint       position)
{
  GbpShellcmdCommandModel *self = GBP_SHELLCMD_COMMAND_MODEL (model);

  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (self));
  g_assert (position < self->items->len);

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static guint
gbp_shellcmd_command_model_get_n_items (GListModel *model)
{
  GbpShellcmdCommandModel *self = GBP_SHELLCMD_COMMAND_MODEL (model);

  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (self));

  return self->items->len;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = gbp_shellcmd_command_model_get_item;
  iface->get_item_type = gbp_shellcmd_command_model_get_item_type;
  iface->get_n_items = gbp_shellcmd_command_model_get_n_items;
}

static gchar *
get_filename (void)
{
  return g_build_filename (g_get_user_config_dir (),
                           ide_get_program_name (),
                           "external-commands",
                           NULL);
}

static void
set_items (GbpShellcmdCommandModel *self,
           GPtrArray               *items)
{
  g_autoptr(GPtrArray) old_items = NULL;

  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (self));
  g_assert (items != NULL);

  old_items = g_steal_pointer (&self->items);
  self->items = g_ptr_array_ref (items);

  if (old_items->len || self->items->len)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_items->len, self->items->len);
}

gboolean
gbp_shellcmd_command_model_load (GbpShellcmdCommandModel  *self,
                                 GCancellable             *cancellable,
                                 GError                  **error)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GPtrArray) items = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) err = NULL;
  g_auto(GStrv) groups = NULL;
  gsize len;

  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND_MODEL (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  path = get_filename ();
  keyfile = g_key_file_new ();
  items = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_object_unref_and_destroy);

  /* Parse keybindings keyfile from storage, but ignore if missing */
  if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_KEEP_COMMENTS, &err))
    {
      if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
          g_error_matches (err, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        return TRUE;

      g_propagate_error (error, g_steal_pointer (&err));
      return FALSE;
    }

  groups = g_key_file_get_groups (keyfile, &len);

  for (guint i = 0; i < len; i++)
    {
      g_autoptr(GbpShellcmdCommand) command = NULL;
      g_autoptr(GError) cmderr = NULL;

      if (!(command = gbp_shellcmd_command_from_key_file (keyfile, groups[i], &cmderr)))
        {
          g_warning ("Failed to parse command from group %s", groups[i]);
          continue;
        }

      g_ptr_array_add (items, g_steal_pointer (&command));
    }

  g_clear_pointer (&self->keyfile, g_key_file_unref);
  self->keyfile = g_steal_pointer (&keyfile);
  set_items (self, items);

  return TRUE;
}

gboolean
gbp_shellcmd_command_model_save (GbpShellcmdCommandModel  *self,
                                 GCancellable             *cancellable,
                                 GError                  **error)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND_MODEL (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  return TRUE;
}

/**
 * gbp_shellcmd_command_model_get_command:
 *
 * Returns: (transfer none) (nullable): an #GbpShellcmdCommand or %NULL
 */
GbpShellcmdCommand *
gbp_shellcmd_command_model_get_command (GbpShellcmdCommandModel *self,
                                        const gchar             *command_id)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND_MODEL (self), NULL);

  for (guint i = 0; i < self->items->len; i++)
    {
      GbpShellcmdCommand *command = g_ptr_array_index (self->items, i);
      const gchar *id = gbp_shellcmd_command_get_id (command);

      if (ide_str_equal0 (id, command_id))
        return command;
    }

  return NULL;
}

void
gbp_shellcmd_command_model_query (GbpShellcmdCommandModel *self,
                                  GPtrArray               *items,
                                  const gchar             *typed_text)
{
  g_autofree gchar *q = NULL;

  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND_MODEL (self));
  g_return_if_fail (items != NULL);
  g_return_if_fail (typed_text != NULL);

  q = g_utf8_casefold (typed_text, -1);

  for (guint i = 0; i < self->items->len; i++)
    {
      GbpShellcmdCommand *command = g_ptr_array_index (self->items, i);
      g_autofree gchar *title = ide_command_get_title (IDE_COMMAND (command));
      const gchar *cmdstr = gbp_shellcmd_command_get_command (command);
      guint prio1 = G_MAXINT;
      guint prio2 = G_MAXINT;

      if (ide_completion_fuzzy_match (title, q, &prio1) ||
          ide_completion_fuzzy_match (cmdstr, q, &prio2))
        {
          GbpShellcmdCommand *copy = gbp_shellcmd_command_copy (command);
          gbp_shellcmd_command_set_priority (copy, MIN (prio1, prio2));
          g_ptr_array_add (items, g_steal_pointer (&copy));
        }
    }
}

void
gbp_shellcmd_command_model_add (GbpShellcmdCommandModel *self,
                                GbpShellcmdCommand      *command)
{
  guint position;

  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND_MODEL (self));
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (command));

  position = self->items->len;
  g_ptr_array_add (self->items, g_object_ref (command));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}
