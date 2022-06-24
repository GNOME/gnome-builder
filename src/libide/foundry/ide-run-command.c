/* ide-run-command.c
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

#define G_LOG_DOMAIN "ide-run-command"

#include "config.h"

#include "ide-foundry-enums.h"
#include "ide-run-command.h"

typedef struct
{
  char *id;
  char *cwd;
  char *display_name;
  char **environ;
  char **argv;
  char **languages;
  int priority;
  IdeRunCommandKind kind;
} IdeRunCommandPrivate;

enum {
  PROP_0,
  PROP_ARGV,
  PROP_CWD,
  PROP_DISPLAY_NAME,
  PROP_ENVIRON,
  PROP_ID,
  PROP_KIND,
  PROP_LANGUAGES,
  PROP_PRIORITY,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeRunCommand, ide_run_command, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static char **
ide_run_command_real_get_arguments (IdeRunCommand      *self,
                                    const char * const *wrapper)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);
  GPtrArray *ar;

  g_assert (IDE_IS_RUN_COMMAND (self));

  if (wrapper == NULL || wrapper[0] == NULL)
    return g_strdupv (priv->argv);

  ar = g_ptr_array_new ();
  for (guint i = 0; wrapper[i]; i++)
    g_ptr_array_add (ar, g_strdup (wrapper[i]));
  if (priv->argv != NULL)
    {
      for (guint i = 0; priv->argv[i]; i++)
        g_ptr_array_add (ar, g_strdup (priv->argv[i]));
    }

  g_ptr_array_add (ar, NULL);

  return (char **)g_ptr_array_free (ar, FALSE);
}

static void
ide_run_command_finalize (GObject *object)
{
  IdeRunCommand *self = (IdeRunCommand *)object;
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->cwd, g_free);
  g_clear_pointer (&priv->display_name, g_free);
  g_clear_pointer (&priv->environ, g_strfreev);
  g_clear_pointer (&priv->argv, g_strfreev);
  g_clear_pointer (&priv->languages, g_strfreev);

  G_OBJECT_CLASS (ide_run_command_parent_class)->finalize (object);
}

static void
ide_run_command_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeRunCommand *self = IDE_RUN_COMMAND (object);

  switch (prop_id)
    {
    case PROP_CWD:
      g_value_set_string (value, ide_run_command_get_cwd (self));
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_run_command_get_display_name (self));
      break;

    case PROP_ARGV:
      g_value_set_boxed (value, ide_run_command_get_argv (self));
      break;

    case PROP_ENVIRON:
      g_value_set_boxed (value, ide_run_command_get_environ (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_run_command_get_id (self));
      break;

    case PROP_KIND:
      g_value_set_enum (value, ide_run_command_get_kind (self));
      break;

    case PROP_LANGUAGES:
      g_value_set_boxed (value, ide_run_command_get_languages (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, ide_run_command_get_priority (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_run_command_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeRunCommand *self = IDE_RUN_COMMAND (object);

  switch (prop_id)
    {
    case PROP_CWD:
      ide_run_command_set_cwd (self, g_value_get_string (value));
      break;

    case PROP_DISPLAY_NAME:
      ide_run_command_set_display_name (self, g_value_get_string (value));
      break;

    case PROP_ARGV:
      ide_run_command_set_argv (self, g_value_get_boxed (value));
      break;

    case PROP_ENVIRON:
      ide_run_command_set_environ (self, g_value_get_boxed (value));
      break;

    case PROP_ID:
      ide_run_command_set_id (self, g_value_get_string (value));
      break;

    case PROP_KIND:
      ide_run_command_set_kind (self, g_value_get_enum (value));
      break;

    case PROP_LANGUAGES:
      ide_run_command_set_languages (self, g_value_get_boxed (value));
      break;

    case PROP_PRIORITY:
      ide_run_command_set_priority (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_run_command_class_init (IdeRunCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_run_command_finalize;
  object_class->get_property = ide_run_command_get_property;
  object_class->set_property = ide_run_command_set_property;

  klass->get_arguments = ide_run_command_real_get_arguments;

  properties [PROP_ARGV] =
    g_param_spec_boxed ("argv", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CWD] =
    g_param_spec_string ("cwd", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENVIRON] =
    g_param_spec_boxed ("environ", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KIND] =
    g_param_spec_enum ("kind", NULL, NULL,
                       IDE_TYPE_RUN_COMMAND_KIND,
                       IDE_RUN_COMMAND_KIND_UNKNOWN,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeRunCommand:languages:
   *
   * Contains the programming languages used.
   *
   * This is to be set by run command providers when they know what languages
   * are used to create the program spawned by the run command. This can be
   * used by debuggers to ensure that a suitable debugger is chosen for a given
   * language used.
   */
  properties [PROP_LANGUAGES] =
    g_param_spec_boxed ("languages", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_run_command_init (IdeRunCommand *self)
{
}

IdeRunCommand *
ide_run_command_new (void)
{
  return g_object_new (IDE_TYPE_RUN_COMMAND, NULL);
}

const char *
ide_run_command_get_id (IdeRunCommand *self)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self), NULL);

  return priv->id;
}

void
ide_run_command_set_id (IdeRunCommand *self,
                        const char    *id)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUN_COMMAND (self));

  if (g_strcmp0 (priv->id, id) != 0)
    {
      g_free (priv->id);
      priv->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

const char *
ide_run_command_get_cwd (IdeRunCommand *self)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self), NULL);

  return priv->cwd;
}

void
ide_run_command_set_cwd (IdeRunCommand *self,
                         const char    *cwd)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUN_COMMAND (self));

  if (g_strcmp0 (priv->cwd, cwd) != 0)
    {
      g_free (priv->cwd);
      priv->cwd = g_strdup (cwd);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CWD]);
    }
}

const char *
ide_run_command_get_display_name (IdeRunCommand *self)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self), NULL);

  return priv->display_name;
}

void
ide_run_command_set_display_name (IdeRunCommand *self,
                                  const char    *display_name)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUN_COMMAND (self));

  if (g_strcmp0 (priv->display_name, display_name) != 0)
    {
      g_free (priv->display_name);
      priv->display_name = g_strdup (display_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
    }
}

const char * const *
ide_run_command_get_argv (IdeRunCommand *self)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self), NULL);

  return (const char * const *)priv->argv;
}

void
ide_run_command_set_argv (IdeRunCommand      *self,
                          const char * const *argv)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUN_COMMAND (self));

  if (argv == (const char * const *)priv->argv)
    return;

  g_strfreev (priv->argv);
  priv->argv = g_strdupv ((char **)argv);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ARGV]);
}

const char * const *
ide_run_command_get_environ (IdeRunCommand *self)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self), NULL);

  return (const char * const *)priv->environ;
}

void
ide_run_command_set_environ (IdeRunCommand      *self,
                             const char * const *environ)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUN_COMMAND (self));

  if (environ == (const char * const *)priv->environ)
    return;

  g_strfreev (priv->environ);
  priv->environ = g_strdupv ((char **)environ);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENVIRON]);
}

int
ide_run_command_get_priority (IdeRunCommand *self)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self), -1);

  return priv->priority;
}

void
ide_run_command_set_priority (IdeRunCommand *self,
                              int            priority)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUN_COMMAND (self));

  if (priority != priv->priority)
    {
      priv->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIORITY]);
    }
}

/**
 * ide_run_command_get_arguments:
 * @self: a #IdeRunCommand
 * @wrapper: (nullable) (array zero-terminated=1): optional wrapper
 *   argument vector for the command, such as "gdb" or "valgrind"
 *
 * Creates an argument vector for the command which contains the
 * wrapper program inserted into the correct position to control
 * the target run command.
 *
 * Some command providers may use this to place @wrapper inside
 * an argument to another program such as
 * "meson test --wrapper='shell command'".
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): A
 *   %NULL-terminated array containing the arguments to execute the program.
 */
char **
ide_run_command_get_arguments (IdeRunCommand      *self,
                               const char * const *wrapper)
{
  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self), NULL);

  return IDE_RUN_COMMAND_GET_CLASS (self)->get_arguments (self, wrapper);
}

IdeRunCommandKind
ide_run_command_get_kind (IdeRunCommand *self)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self), 0);

  return priv->kind;
}

/**
 * ide_run_command_set_kind:
 * @self: a #IdeRunCommand
 *
 * Sets the kind of command.
 *
 * This is useful for #IdeRunCommandProvider that want to specify
 * the type of command that is being provided. Doing so allows tooling
 * in Builder to treat that information specially, such as showing tags
 * next to the row in UI or including it in "Unit Test" browsers.
 */
void
ide_run_command_set_kind (IdeRunCommand     *self,
                          IdeRunCommandKind  kind)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUN_COMMAND (self));
  g_return_if_fail (kind <= IDE_RUN_COMMAND_KIND_USER_DEFINED);

  if (priv->kind != kind)
    {
      priv->kind = kind;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KIND]);
    }
}

const char * const *
ide_run_command_get_languages (IdeRunCommand *self)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self), NULL);

  return (const char * const *)priv->languages;
}

void
ide_run_command_set_languages (IdeRunCommand      *self,
                               const char * const *languages)
{
  IdeRunCommandPrivate *priv = ide_run_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUN_COMMAND (self));

  if (languages == (const char * const *)priv->languages ||
      (languages != NULL &&
       priv->languages != NULL &&
       g_strv_equal ((const char * const *)priv->languages, languages)))
    return;

  g_strfreev (priv->languages);
  priv->languages = g_strdupv ((char **)languages);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGES]);
}
