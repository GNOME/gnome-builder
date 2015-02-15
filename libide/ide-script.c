/* ide-script.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-script.h"

G_DEFINE_ABSTRACT_TYPE (IdeScript, ide_script, IDE_TYPE_OBJECT)

enum {
  LOAD,
  UNLOAD,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

void
ide_script_load (IdeScript *script)
{
  g_return_if_fail (IDE_IS_SCRIPT (script));

  g_signal_emit (script, gSignals [LOAD], 0);
}

void
ide_script_unload (IdeScript *script)
{
  g_return_if_fail (IDE_IS_SCRIPT (script));

  g_signal_emit (script, gSignals [UNLOAD], 0);
}

static void
ide_script_class_init (IdeScriptClass *klass)
{
  gSignals [LOAD] =
    g_signal_new ("load",
                  IDE_TYPE_SCRIPT,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeScriptClass, load),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  0);

  gSignals [UNLOAD] =
    g_signal_new ("unload",
                  IDE_TYPE_SCRIPT,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeScriptClass, unload),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  0);
}

static void
ide_script_init (IdeScript *self)
{
}
