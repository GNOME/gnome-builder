/* ide-terminal.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#pragma once

#if !defined (IDE_TERMINAL_INSIDE) && !defined (IDE_TERMINAL_COMPILATION)
# error "Only <libide-terminal.h> can be included directly."
#endif

#include <vte/vte.h>
#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL (ide_terminal_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdeTerminal, ide_terminal, IDE, TERMINAL, VteTerminal)

struct _IdeTerminalClass
{
  VteTerminalClass parent_class;

  void     (*populate_popup)      (IdeTerminal *self,
                                   GtkWidget   *widget);
  void     (*select_all)          (IdeTerminal *self,
                                   gboolean     all);
  void     (*search_reveal)       (IdeTerminal *self);
  gboolean (*open_link)           (IdeTerminal *self);
  gboolean (*copy_link_address)   (IdeTerminal *self);

  /*< private >*/
  gpointer padding[16];
};

IDE_AVAILABLE_IN_3_32
GtkWidget *ide_terminal_new (void);

G_END_DECLS
