/* ide-subprocess-launcher.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include <string.h>

#include "ide-debug.h"
#include "ide-environment.h"
#include "ide-environment-variable.h"
#include "ide-macros.h"
#include "ide-subprocess-launcher.h"

typedef struct
{
  GSubprocessFlags  flags;
  guint             freeze_check : 1;

  GPtrArray        *argv;
  gchar            *cwd;
  GPtrArray        *environ;
} IdeSubprocessLauncherPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSubprocessLauncher, ide_subprocess_launcher, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CWD,
  PROP_ENVIRON,
  PROP_FLAGS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeSubprocessLauncher *
ide_subprocess_launcher_new (GSubprocessFlags flags)
{
  return g_object_new (IDE_TYPE_SUBPROCESS_LAUNCHER,
                       "flags", flags,
                       NULL);
}

static void
ide_subprocess_launcher_spawn_worker (GTask        *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable)
{
  IdeSubprocessLauncher *self = source_object;
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  GSubprocess *ret;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *str = NULL;
    str = g_strjoinv (" ", (gchar **)priv->argv->pdata);
    IDE_TRACE_MSG ("Launching '%s'", str);
  }
#endif

  launcher = g_subprocess_launcher_new (priv->flags);
  g_subprocess_launcher_set_cwd (launcher, priv->cwd);
  if (priv->environ->len > 1)
    g_subprocess_launcher_set_environ (launcher, (gchar **)priv->environ->pdata);
  ret = g_subprocess_launcher_spawnv (launcher,
                                      (const gchar * const *)priv->argv->pdata,
                                      &error);

  if (ret == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, ret, g_object_unref);
}

static GSubprocess *
ide_subprocess_launcher_real_spawn_sync (IdeSubprocessLauncher  *self,
                                         GCancellable           *cancellable,
                                         GError                **error)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  priv->freeze_check = TRUE;

  task = g_task_new (self, cancellable, NULL, NULL);
  g_task_run_in_thread_sync (task, ide_subprocess_launcher_spawn_worker);

  return g_task_propagate_pointer (task, error);
}

static void
ide_subprocess_launcher_real_spawn_async (IdeSubprocessLauncher *self,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  priv->freeze_check = TRUE;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, ide_subprocess_launcher_spawn_worker);
}

static GSubprocess *
ide_subprocess_launcher_real_spawn_finish (IdeSubprocessLauncher  *self,
                                           GAsyncResult           *result,
                                           GError                **error)
{
  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_subprocess_launcher_finalize (GObject *object)
{
  IdeSubprocessLauncher *self = (IdeSubprocessLauncher *)object;
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_clear_pointer (&priv->argv, g_ptr_array_unref);
  g_clear_pointer (&priv->cwd, g_free);
  g_clear_pointer (&priv->environ, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_subprocess_launcher_parent_class)->finalize (object);
}

static void
ide_subprocess_launcher_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeSubprocessLauncher *self = IDE_SUBPROCESS_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_CWD:
      g_value_set_string (value, ide_subprocess_launcher_get_cwd (self));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, ide_subprocess_launcher_get_flags (self));
      break;

    case PROP_ENVIRON:
      g_value_set_boxed (value, ide_subprocess_launcher_get_environ (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_subprocess_launcher_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeSubprocessLauncher *self = IDE_SUBPROCESS_LAUNCHER (object);

  switch (prop_id)
    {
    case PROP_CWD:
      ide_subprocess_launcher_set_cwd (self, g_value_get_string (value));
      break;

    case PROP_FLAGS:
      ide_subprocess_launcher_set_flags (self, g_value_get_flags (value));
      break;

    case PROP_ENVIRON:
      ide_subprocess_launcher_set_environ (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_subprocess_launcher_class_init (IdeSubprocessLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_subprocess_launcher_finalize;
  object_class->get_property = ide_subprocess_launcher_get_property;
  object_class->set_property = ide_subprocess_launcher_set_property;

  klass->spawn_sync = ide_subprocess_launcher_real_spawn_sync;
  klass->spawn_async = ide_subprocess_launcher_real_spawn_async;
  klass->spawn_finish = ide_subprocess_launcher_real_spawn_finish;

  properties [PROP_CWD] =
    g_param_spec_string ("cwd",
                         "Current Working Directory",
                         "Current Working Directory",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        "Flags",
                        "Flags",
                        G_TYPE_SUBPROCESS_FLAGS,
                        G_SUBPROCESS_FLAGS_NONE,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENVIRON] =
    g_param_spec_boxed ("environ",
                        "Environ",
                        "Environ",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_subprocess_launcher_init (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  priv->environ = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (priv->environ, NULL);

  priv->argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (priv->argv, NULL);

  priv->cwd = g_strdup (".");
}

void
ide_subprocess_launcher_set_flags (IdeSubprocessLauncher *self,
                                   GSubprocessFlags       flags)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (priv->freeze_check)
    {
      g_warning ("process launcher is already frozen");
      return;
    }

  if (flags != priv->flags)
    {
      priv->flags = flags;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FLAGS]);
    }
}

GSubprocessFlags
ide_subprocess_launcher_get_flags (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), 0);

  return priv->flags;
}

const gchar * const *
ide_subprocess_launcher_get_environ (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  return (const gchar * const *)priv->environ->pdata;
}

void
ide_subprocess_launcher_set_environ (IdeSubprocessLauncher *self,
                                     const gchar * const   *environ_)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  guint i;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (priv->freeze_check)
    {
      g_warning ("process launcher is already frozen");
      return;
    }

  g_ptr_array_remove_range (priv->environ, 0, priv->environ->len);

  if (environ_ != NULL)
    {
      for (i = 0; environ_ [i]; i++)
        g_ptr_array_add (priv->environ, g_strdup (environ_ [i]));
    }

  g_ptr_array_add (priv->environ, NULL);
}

void
ide_subprocess_launcher_setenv (IdeSubprocessLauncher *self,
                                const gchar           *key,
                                const gchar           *value,
                                gboolean               replace)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);
  gchar *str;
  guint i;

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (key != NULL);

  if (priv->freeze_check)
    {
      g_warning ("process launcher is already frozen");
      return;
    }

  if (value == NULL)
    value = "";

  for (i = 0; i < priv->environ->len; i++)
    {
      gchar *item_key = g_ptr_array_index (priv->environ, i);
      const gchar *eq;

      if (item_key == NULL)
        break;

      if (NULL == (eq = strchr (item_key, '=')))
        continue;

      if (strncmp (item_key, key, eq - item_key) == 0)
        {
          if (replace)
            {
              g_free (item_key);
              g_ptr_array_index (priv->environ, i) = g_strdup_printf ("%s=%s", key, value);
            }
          return;
        }
    }

  str = g_strdup_printf ("%s=%s", key, value);
  g_ptr_array_index (priv->environ, priv->environ->len - 1) = str;
  g_ptr_array_add (priv->environ, NULL);
}

void
ide_subprocess_launcher_push_argv (IdeSubprocessLauncher *self,
                                   const gchar           *argv)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (argv != NULL);

  if (priv->freeze_check)
    {
      g_warning ("process launcher is already frozen");
      return;
    }

  g_ptr_array_index (priv->argv, priv->argv->len - 1) = g_strdup (argv);
  g_ptr_array_add (priv->argv, NULL);
}

void
ide_subprocess_launcher_spawn_async (IdeSubprocessLauncher *self,
                                     GCancellable          *cancellable,
                                     GAsyncReadyCallback    callback,
                                     gpointer               user_data)
{
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SUBPROCESS_LAUNCHER_GET_CLASS (self)->spawn_async (self, cancellable, callback, user_data);
}

/**
 * ide_subprocess_launcher_spawn_finish:
 *
 * Complete a request to asynchronously spawn a process.
 *
 * Returns: (transfer full): A #GSubprocess or %NULL upon error.
 */
GSubprocess *
ide_subprocess_launcher_spawn_finish (IdeSubprocessLauncher  *self,
                                      GAsyncResult           *result,
                                      GError                **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return IDE_SUBPROCESS_LAUNCHER_GET_CLASS (self)->spawn_finish (self, result, error);
}

/**
 * ide_subprocess_launcher_spawn_sync:
 *
 * Synchronously spawn a process using the internal state.
 *
 * Returns: (transfer full): A #GSubprocess or %NULL upon error.
 */
GSubprocess *
ide_subprocess_launcher_spawn_sync (IdeSubprocessLauncher  *self,
                                    GCancellable           *cancellable,
                                    GError                **error)
{
  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  return IDE_SUBPROCESS_LAUNCHER_GET_CLASS (self)->spawn_sync (self, cancellable, error);
}

void
ide_subprocess_launcher_set_cwd (IdeSubprocessLauncher *self,
                                 const gchar           *cwd)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));

  if (ide_str_empty0 (cwd))
    cwd = ".";

  if (!ide_str_equal0 (priv->cwd, cwd))
    {
      g_free (priv->cwd);
      priv->cwd = g_strdup (cwd);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CWD]);
    }
}

const gchar *
ide_subprocess_launcher_get_cwd (IdeSubprocessLauncher *self)
{
  IdeSubprocessLauncherPrivate *priv = ide_subprocess_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self), NULL);

  return priv->cwd;
}

void
ide_subprocess_launcher_overlay_environment (IdeSubprocessLauncher *self,
                                             IdeEnvironment        *environment)
{
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (!environment || IDE_IS_ENVIRONMENT (environment));

  if (environment != NULL)
    {
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (environment));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(IdeEnvironmentVariable) var = NULL;
          const gchar *key;
          const gchar *value;

          var = g_list_model_get_item (G_LIST_MODEL (environment), i);
          key = ide_environment_variable_get_key (var);
          value = ide_environment_variable_get_value (var);

          if (!ide_str_empty0 (key))
            ide_subprocess_launcher_setenv (self, key, value ?: "", TRUE);
        }
    }
}

void
ide_subprocess_launcher_push_args (IdeSubprocessLauncher *self,
                                   const gchar * const   *args)
{
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (self));
  g_return_if_fail (args != NULL);

  for (guint i = 0; args [i] != NULL; i++)
    ide_subprocess_launcher_push_argv (self, args [i]);
}
