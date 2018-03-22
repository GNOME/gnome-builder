/* gb-command.c
 *
 * Copyright 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-command.h"

G_DEFINE_TYPE (GbCommand, gb_command, G_TYPE_OBJECT)

enum {
  EXECUTE,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

GbCommand *
gb_command_new (void)
{
  return g_object_new (GB_TYPE_COMMAND, NULL);
}

static GbCommandResult *
gb_command_real_execute (GbCommand *command)
{
  return NULL;
}

GbCommandResult *
gb_command_execute (GbCommand *command)
{
  GbCommandResult *ret = NULL;
  g_return_val_if_fail (GB_IS_COMMAND (command), NULL);
  g_signal_emit (command, signals [EXECUTE], 0, &ret);
  return ret;
}

static void
gb_command_class_init (GbCommandClass *klass)
{
  klass->execute = gb_command_real_execute;

  signals [EXECUTE] =
    g_signal_new ("execute",
                  GB_TYPE_COMMAND,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbCommandClass, execute),
                  g_signal_accumulator_first_wins,
                  NULL, NULL,
                  GB_TYPE_COMMAND_RESULT,
                  0);
}

static void
gb_command_init (GbCommand *self)
{
}
