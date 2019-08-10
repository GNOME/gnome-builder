/* gbp-shellcmd-list.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-list"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-shellcmd-command.h"
#include "gbp-shellcmd-command-row.h"
#include "gbp-shellcmd-list.h"

struct _GbpShellcmdList
{
  GtkFrame                 parent_instance;

  GtkListBox              *list;
  GtkBox                  *box;
  GtkListBoxRow           *add_row;

  GbpShellcmdCommandModel *model;
};

enum {
  COMMAND_SELECTED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

G_DEFINE_TYPE (GbpShellcmdList, gbp_shellcmd_list, GTK_TYPE_FRAME)

static void
gbp_shellcmd_list_class_init (GbpShellcmdListClass *klass)
{
  signals [COMMAND_SELECTED] =
    g_signal_new ("command-selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GBP_TYPE_SHELLCMD_COMMAND);
}

static void
gbp_shellcmd_list_init (GbpShellcmdList *self)
{
}

static void
on_row_activated_cb (GbpShellcmdList       *self,
                     GbpShellcmdCommandRow *row,
                     GtkListBox            *list_box)
{
  GbpShellcmdCommand *command = NULL;

  g_assert (GBP_IS_SHELLCMD_LIST (self));
  g_assert (!row || GBP_IS_SHELLCMD_COMMAND_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (row != NULL)
    command = gbp_shellcmd_command_row_get_command (row);

  g_signal_emit (self, signals [COMMAND_SELECTED], 0, command);
}

static GtkWidget *
create_row_func (gpointer item,
                 gpointer user_data)
{
  return gbp_shellcmd_command_row_new (item);
}

static void
on_add_new_row_cb (GbpShellcmdList *self,
                   GtkListBoxRow   *row,
                   GtkListBox      *list_box)
{
  g_autoptr(GbpShellcmdCommand) command = NULL;
  g_autofree gchar *id = NULL;
  guint nth;

  g_assert (GBP_IS_SHELLCMD_LIST (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (self->model == NULL)
    return;

  id = g_uuid_string_random ();
  nth = g_list_model_get_n_items (G_LIST_MODEL (self->model));

  command = g_object_new (GBP_TYPE_SHELLCMD_COMMAND,
                          "id", id,
                          "title", _("New command"),
                          "command", "",
                          NULL);
  gbp_shellcmd_command_model_add (self->model, command);

  /* Now select the new row */
  row = gtk_list_box_get_row_at_index (self->list, nth);
  gtk_list_box_select_row (self->list, row);
}

GtkWidget *
gbp_shellcmd_list_new (GbpShellcmdCommandModel *model)
{
  GbpShellcmdList *self;
  GtkWidget *list2;
  GtkWidget *placeholder;

  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND_MODEL (model), NULL);

  placeholder = g_object_new (GTK_TYPE_LABEL,
                              "margin", 12,
                              "label", _("Click + to add an external command"),
                              "visible", TRUE,
                              NULL);

  self = g_object_new (GBP_TYPE_SHELLCMD_LIST,
                       "shadow-type", GTK_SHADOW_IN,
                       "visible", TRUE,
                       NULL);
  self->model = g_object_ref (model);

  self->box = g_object_new (GTK_TYPE_BOX,
                            "orientation", GTK_ORIENTATION_VERTICAL,
                            "visible", TRUE,
                            NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->box));

  self->list = g_object_new (GTK_TYPE_LIST_BOX,
                             "selection-mode", GTK_SELECTION_NONE,
                             "visible", TRUE,
                             NULL);
  gtk_list_box_set_placeholder (self->list, placeholder);
  gtk_list_box_bind_model (self->list,
                           G_LIST_MODEL (model),
                           create_row_func, NULL, NULL);
  g_signal_connect_object (self->list,
                           "row-activated",
                           G_CALLBACK (on_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (self->list));

  list2 = g_object_new (GTK_TYPE_LIST_BOX,
                        "selection-mode", GTK_SELECTION_NONE,
                        "visible", TRUE,
                        NULL);
  g_signal_connect_object (list2,
                           "row-activated",
                           G_CALLBACK (on_add_new_row_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (list2));

  self->add_row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                                "child", g_object_new (GTK_TYPE_IMAGE,
                                                       "icon-name", "list-add-symbolic",
                                                       "visible", TRUE,
                                                       NULL),
                                "visible", TRUE,
                                NULL);
  gtk_container_add (GTK_CONTAINER (list2), GTK_WIDGET (self->add_row));

  return GTK_WIDGET (g_steal_pointer (&self));
}
