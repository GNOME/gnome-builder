/* gbp-codeui-rename-dialog.c
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

#define G_LOG_DOMAIN "gbp-codeui-rename-dialog"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>

#include "gbp-codeui-rename-dialog.h"

struct _GbpCodeuiRenameDialog
{
  AdwAlertDialog     parent_instance;

  AdwEntryRow       *old_symbol;
  AdwEntryRow       *new_symbol;

  IdeLocation       *location;
  IdeRenameProvider *provider;
  char              *word;
};

G_DEFINE_FINAL_TYPE (GbpCodeuiRenameDialog, gbp_codeui_rename_dialog, ADW_TYPE_ALERT_DIALOG)

static void
gbp_codeui_rename_dialog_notify_text_cb (GbpCodeuiRenameDialog *self,
                                         GParamSpec            *pspec,
                                         GtkEditable           *editable)
{
  const char *str;
  gboolean enabled;

  g_assert (GBP_IS_CODEUI_RENAME_DIALOG (self));
  g_assert (GTK_IS_EDITABLE (editable));

  str = gtk_editable_get_text (editable);
  enabled = !ide_str_empty0 (str);

  for (const char *iter = str;
       enabled && *iter;
       iter = g_utf8_next_char (iter))
    {
      gunichar ch = g_utf8_get_char (iter);

      if (g_unichar_isspace (ch))
        enabled = FALSE;
    }

  adw_alert_dialog_set_response_enabled (ADW_ALERT_DIALOG (self), "rename", enabled);
}

static void
get_edits_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  IdeRenameProvider *provider = (IdeRenameProvider *)object;
  g_autoptr(GbpCodeuiRenameDialog) self = user_data;
  g_autoptr(GPtrArray) text_edits = NULL;
  g_autoptr(GError) error = NULL;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RENAME_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_CODEUI_RENAME_DIALOG (self));

  /* Get the edits to apply. It would be nice someday to show these
   * to the user interactively with a "refactory" dialog.
   */
  if (!ide_rename_provider_rename_finish (provider, result, &text_edits, &error))
    {
      ide_object_warning (IDE_OBJECT (provider),
                          _("Failed to rename symbol: %s"),
                          error->message);
      IDE_EXIT;
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (text_edits, g_object_unref);

  /* If we fail to get a context, we must have shutdown */
  if (!(context = ide_object_get_context (IDE_OBJECT (provider))))
    IDE_EXIT;

  /* Apply the edits via the buffer manager */
  buffer_manager = ide_buffer_manager_from_context (context);
  ide_buffer_manager_apply_edits_async (buffer_manager,
                                        g_steal_pointer (&text_edits),
                                        NULL, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_codeui_rename_dialog_rename_cb (GbpCodeuiRenameDialog *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_RENAME_DIALOG (self));
  g_assert (IDE_IS_LOCATION (self->location));
  g_assert (IDE_IS_RENAME_PROVIDER (self->provider));

  ide_rename_provider_rename_async (self->provider,
                                    self->location,
                                    gtk_editable_get_text (GTK_EDITABLE (self->new_symbol)),
                                    NULL,
                                    get_edits_cb,
                                    g_object_ref (self));
}

static void
gbp_codeui_rename_dialog_dispose (GObject *object)
{
  GbpCodeuiRenameDialog *self = (GbpCodeuiRenameDialog *)object;

  g_clear_object (&self->location);
  g_clear_object (&self->provider);
  g_clear_pointer (&self->word, g_free);

  G_OBJECT_CLASS (gbp_codeui_rename_dialog_parent_class)->dispose (object);
}

static void
gbp_codeui_rename_dialog_class_init (GbpCodeuiRenameDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_codeui_rename_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/codeui/gbp-codeui-rename-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiRenameDialog, old_symbol);
  gtk_widget_class_bind_template_child (widget_class, GbpCodeuiRenameDialog, new_symbol);
  gtk_widget_class_bind_template_callback (widget_class, gbp_codeui_rename_dialog_rename_cb);
  gtk_widget_class_bind_template_callback (widget_class, gbp_codeui_rename_dialog_notify_text_cb);
}

static void
gbp_codeui_rename_dialog_init (GbpCodeuiRenameDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwDialog *
gbp_codeui_rename_dialog_new (IdeRenameProvider *provider,
                              IdeLocation       *location,
                              const char        *word)
{
  GbpCodeuiRenameDialog *self;

  self = g_object_new (GBP_TYPE_CODEUI_RENAME_DIALOG, NULL);
  self->provider = g_object_ref (provider);
  self->location = g_object_ref (location);
  self->word = g_strdup (word);

  gtk_editable_set_text (GTK_EDITABLE (self->old_symbol), word);
  gtk_editable_set_text (GTK_EDITABLE (self->new_symbol), word);
  gtk_widget_grab_focus (GTK_WIDGET (self->new_symbol));

  return ADW_DIALOG (self);
}
