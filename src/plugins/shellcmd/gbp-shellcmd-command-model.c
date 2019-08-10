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

#include <glib/gstdio.h>
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

  guint      queue_save;

  guint      keybindings_changed : 1;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpShellcmdCommandModel, gbp_shellcmd_command_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  KEYBINDINGS_CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static gboolean
gbp_shellcmd_command_model_queue_save_cb (gpointer data)
{
  GbpShellcmdCommandModel *self = data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (self));

  self->queue_save = 0;

  if (!gbp_shellcmd_command_model_save (self, NULL, &error))
    g_warning ("Failed to save external-commands: %s", error->message);

  /* Now ask everything to reload (as we might have new keybindings) */
  if (self->keybindings_changed)
    {
      g_signal_emit (self, signals [KEYBINDINGS_CHANGED], 0);
      self->keybindings_changed = FALSE;
    }

  return G_SOURCE_REMOVE;
}

static void
gbp_shellcmd_command_model_queue_save (GbpShellcmdCommandModel *self)
{
  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (self));

  g_object_ref (self);

  if (self->queue_save != 0)
    g_source_remove (self->queue_save);

  self->queue_save =
    g_timeout_add_seconds_full (G_PRIORITY_HIGH,
                                1,
                                gbp_shellcmd_command_model_queue_save_cb,
                                g_object_ref (self),
                                g_object_unref);

  g_object_unref (self);
}

static void
on_command_changed_cb (GbpShellcmdCommandModel *self,
                       GbpShellcmdCommand      *command)
{
  g_assert (GBP_SHELLCMD_COMMAND_MODEL (self));
  g_assert (GBP_SHELLCMD_COMMAND (command));

  gbp_shellcmd_command_model_queue_save (self);
}

static void
on_command_shortcut_changed_cb (GbpShellcmdCommandModel *self,
                                GParamSpec              *pspec,
                                GbpShellcmdCommand      *command)
{
  g_assert (GBP_SHELLCMD_COMMAND_MODEL (self));
  g_assert (GBP_SHELLCMD_COMMAND (command));

  self->keybindings_changed = TRUE;
}

static void
gbp_shellcmd_command_model_finalize (GObject *object)
{
  GbpShellcmdCommandModel *self = (GbpShellcmdCommandModel *)object;

  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_pointer (&self->keyfile, g_key_file_free);

  G_OBJECT_CLASS (gbp_shellcmd_command_model_parent_class)->finalize (object);
}

static void
gbp_shellcmd_command_model_class_init (GbpShellcmdCommandModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_shellcmd_command_model_finalize;

  signals [KEYBINDINGS_CHANGED] =
    g_signal_new ("keybindings-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gbp_shellcmd_command_model_init (GbpShellcmdCommandModel *self)
{
  self->items = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_object_unref_and_destroy);
  self->keyfile = g_key_file_new ();
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

  for (guint i = 0; i < items->len; i++)
    {
      GbpShellcmdCommand *command = g_ptr_array_index (items, i);

      g_signal_connect_object (command,
                               "changed",
                               G_CALLBACK (on_command_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (command,
                               "notify::shortcut",
                               G_CALLBACK (on_command_shortcut_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
    }

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
  g_autofree gchar *path = NULL;
  g_auto(GStrv) groups = NULL;
  gsize n_groups = 0;

  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND_MODEL (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (self->keyfile != NULL, FALSE);

  path = get_filename ();

  for (guint i = 0; i < self->items->len; i++)
    {
      GbpShellcmdCommand *command = g_ptr_array_index (self->items, i);
      gbp_shellcmd_command_to_key_file (command, self->keyfile);
    }

  groups = g_key_file_get_groups (self->keyfile, &n_groups);

  if (n_groups == 0)
    {
      g_unlink (path);
      return TRUE;
    }

  return g_key_file_save_to_file (self->keyfile, path, error);
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

  g_signal_connect_object (command,
                           "changed",
                           G_CALLBACK (on_command_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->keybindings_changed = TRUE;

  position = self->items->len;
  g_ptr_array_add (self->items, g_object_ref (command));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  gbp_shellcmd_command_model_queue_save (self);
}

void
gbp_shellcmd_command_model_remove (GbpShellcmdCommandModel *self,
                                   GbpShellcmdCommand      *command)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND_MODEL (self));
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND (command));

  for (guint i = 0; i < self->items->len; i++)
    {
      GbpShellcmdCommand *ele = g_ptr_array_index (self->items, i);

      if (ele == command)
        {
          const gchar *id = gbp_shellcmd_command_get_id (ele);

          self->keybindings_changed = TRUE;

          if (id != NULL)
            g_key_file_remove_group (self->keyfile, id, NULL);

          g_signal_handlers_disconnect_by_func (command,
                                                G_CALLBACK (on_command_changed_cb),
                                                self);

          g_signal_handlers_disconnect_by_func (command,
                                                G_CALLBACK (on_command_shortcut_changed_cb),
                                                self);

          g_ptr_array_remove_index (self->items, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          gbp_shellcmd_command_model_queue_save (self);

          break;
        }
    }
}
