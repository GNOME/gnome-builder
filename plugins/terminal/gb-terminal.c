/* gb-terminal.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#include "gb-terminal.h"

struct _GbTerminal
{
  VteTerminal parent;
};

struct _GbTerminalClass
{
  VteTerminalClass parent;
};

G_DEFINE_TYPE (GbTerminal, gb_terminal, VTE_TYPE_TERMINAL)

static void
gb_terminal_class_init (GbTerminalClass *klass)
{
  GtkBindingSet *binding_set;

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_c,
                                GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                "copy-clipboard",
                                0);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_v,
                                GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                "paste-clipboard",
                                0);
}

static void
gb_terminal_init (GbTerminal *self)
{
}
