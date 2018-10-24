/* gbp-grep-dialog.c
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "gbp-grep-dialog"

#include <ide.h>

#include "gbp-grep-dialog.h"

struct _GbpGrepDialog
{
  GtkDialog parent_instance;
};

G_DEFINE_TYPE (GbpGrepDialog, gbp_grep_dialog, GTK_TYPE_DIALOG)

static void
gbp_grep_dialog_class_init (GbpGrepDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/grep/gbp-grep-dialog.ui");
}

static void
gbp_grep_dialog_init (GbpGrepDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
