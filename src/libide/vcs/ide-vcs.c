/* ide-vcs.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "ide-vcs"

#include "ide-context.h"

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-change-monitor.h"
#include "vcs/ide-vcs.h"

G_DEFINE_INTERFACE (IdeVcs, ide_vcs, IDE_TYPE_OBJECT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];
static GPtrArray *ignored;

void
ide_vcs_register_ignored (const gchar *pattern)
{
  if (ignored == NULL)
    ignored = g_ptr_array_new ();

  g_ptr_array_add (ignored, g_pattern_spec_new (pattern));
}

static void
ide_vcs_default_init (IdeVcsInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "Context",
                                                            "Context",
                                                            IDE_TYPE_CONTEXT,
                                                            (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_string ("branch-name",
                                                            "Branch Name",
                                                            "The current name of the branch",
                                                            NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("working-directory",
                                                            "Working Directory",
                                                            "The working directory for the VCS",
                                                            G_TYPE_FILE,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  /**
   * IdeVcs::changed:
   *
   * The "changed" signal should be emitted when the VCS has detected a change
   * to the underlying VCS storage. This can be used by consumers to reload
   * their respective data structures.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeVcsInterface, changed),
                  NULL, NULL, NULL, G_TYPE_NONE, 0);
}

gboolean
ide_vcs_is_ignored (IdeVcs  *self,
                    GFile   *file,
                    GError **error)
{
  g_return_val_if_fail (IDE_IS_VCS (self), FALSE);

  if G_LIKELY (ignored != NULL)
    {
      g_autofree gchar *name = g_file_get_basename (file);
      guint len = strlen (name);
      g_autofree gchar *reversed = g_utf8_strreverse (name, len);

      for (guint i = 0; i < ignored->len; i++)
        {
          GPatternSpec *pattern_spec = g_ptr_array_index (ignored, i);

          if (g_pattern_match (pattern_spec, len, name, reversed))
            return TRUE;
        }
    }

  if (IDE_VCS_GET_IFACE (self)->is_ignored)
    return IDE_VCS_GET_IFACE (self)->is_ignored (self, file, error);

  return FALSE;
}

gint
ide_vcs_get_priority (IdeVcs *self)
{
  gint ret = 0;

  g_return_val_if_fail (IDE_IS_VCS (self), 0);

  if (IDE_VCS_GET_IFACE (self)->get_priority)
    ret = IDE_VCS_GET_IFACE (self)->get_priority (self);

  return ret;
}

/**
 * ide_vcs_get_working_directory:
 * @self: An #IdeVcs.
 *
 * Retrieves the working directory for the context. This is the root of where
 * the project files exist.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
ide_vcs_get_working_directory (IdeVcs *self)
{
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_working_directory)
    return IDE_VCS_GET_IFACE (self)->get_working_directory (self);

  return NULL;
}

/**
 * ide_vcs_get_buffer_change_monitor:
 *
 * Gets an #IdeBufferChangeMonitor for the buffer provided. If the #IdeVcs implementation does not
 * support change monitoring, or cannot for the current file, then %NULL is returned.
 *
 * Returns: (transfer full) (nullable): An #IdeBufferChangeMonitor or %NULL.
 */
IdeBufferChangeMonitor *
ide_vcs_get_buffer_change_monitor (IdeVcs    *self,
                                   IdeBuffer *buffer)
{
  IdeBufferChangeMonitor *ret = NULL;

  g_return_val_if_fail (IDE_IS_VCS (self), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_buffer_change_monitor)
    ret = IDE_VCS_GET_IFACE (self)->get_buffer_change_monitor (self, buffer);

  g_return_val_if_fail (!ret || IDE_IS_BUFFER_CHANGE_MONITOR (ret), NULL);

  return ret;
}

static gint
sort_by_priority (gconstpointer a,
                  gconstpointer b,
                  gpointer      user_data)
{
  IdeVcs *vcs_a = *(IdeVcs **)a;
  IdeVcs *vcs_b = *(IdeVcs **)b;

  return ide_vcs_get_priority (vcs_a) - ide_vcs_get_priority (vcs_b);
}

void
ide_vcs_new_async (IdeContext           *context,
                   int                   io_priority,
                   GCancellable         *cancellable,
                   GAsyncReadyCallback   callback,
                   gpointer              user_data)
{
  ide_object_new_for_extension_async (IDE_TYPE_VCS,
                                      sort_by_priority,
                                      NULL,
                                      io_priority,
                                      cancellable,
                                      callback,
                                      user_data,
                                      "context", context,
                                      NULL);
}

/**
 * ide_vcs_new_finish:
 *
 * Completes a call to ide_vcs_new_async().
 *
 * Returns: (transfer full): An #IdeVcs.
 */
IdeVcs *
ide_vcs_new_finish (GAsyncResult  *result,
                    GError       **error)
{
  IdeObject *ret;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = ide_object_new_finish (result, error);

  return IDE_VCS (ret);
}

void
ide_vcs_emit_changed (IdeVcs *self)
{
  g_return_if_fail (IDE_IS_VCS (self));

  g_signal_emit (self, signals [CHANGED], 0);
}

/**
 * ide_vcs_get_config:
 *
 * Retrieves an #IdeVcsConfig for the #IdeVcs provided. If the #IdeVcs implementation does not
 * support access to configuration, then %NULL is returned.
 *
 * Returns: (transfer full) (nullable): An #IdeVcsConfig or %NULL.
 */
IdeVcsConfig *
ide_vcs_get_config (IdeVcs *self)
{
  IdeVcsConfig *ret = NULL;

  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_config)
    ret = IDE_VCS_GET_IFACE (self)->get_config (self);

  g_return_val_if_fail (!ret || IDE_IS_VCS_CONFIG (ret), NULL);

  return  ret;
}

/**
 * ide_vcs_get_branch_name:
 *
 * Retrieves the name of the branch in the current working directory.
 *
 * Returns: (transfer full): A string containing the branch name.
 */
gchar *
ide_vcs_get_branch_name (IdeVcs *self)
{
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_branch_name)
    return IDE_VCS_GET_IFACE (self)->get_branch_name (self);

  return g_strdup ("primary");
}
