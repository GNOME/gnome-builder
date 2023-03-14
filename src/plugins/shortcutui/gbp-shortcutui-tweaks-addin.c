/* gbp-shortcutui-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-shortcutui-tweaks-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "ide-shortcut-manager-private.h"
#include "ide-shortcut-observer-private.h"

#include "gbp-shortcutui-dialog.h"
#include "gbp-shortcutui-tweaks-addin.h"

struct _GbpShortcutuiTweaksAddin
{
  IdeTweaksAddin parent_instance;
  GListModel *model;
};

G_DEFINE_FINAL_TYPE (GbpShortcutuiTweaksAddin, gbp_shortcutui_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static void
gbp_shortcutui_tweaks_addin_row_activated_cb (GListModel   *model,
                                              AdwActionRow *row)
{
  g_autoptr(IdeShortcutManager) temp_manager = NULL;
  GbpShortcutuiDialog *dialog;
  IdeContext *context;
  GtkRoot *root;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (ADW_IS_ACTION_ROW (row));

  context = ide_widget_get_context (GTK_WIDGET (row));

  g_assert (!context || IDE_IS_CONTEXT (context));

  root = gtk_widget_get_root (GTK_WIDGET (row));
  dialog = g_object_new (GBP_TYPE_SHORTCUTUI_DIALOG,
                         "default-width", 700,
                         "default-height", 500,
                         "title", _("Keyboard Shortcuts"),
                         "transient-for", root,
                         "modal", TRUE,
                         "context", context,
                         NULL);

  gtk_window_present (GTK_WINDOW (dialog));

  IDE_EXIT;
}

static GtkWidget *
shortcutui_create_shortcuts_cb (GbpShortcutuiTweaksAddin *self,
                                IdeTweaksItem            *instance,
                                IdeTweaksItem            *original)
{
  AdwActionRow *row;
  GtkImage *image;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_ITEM (instance));
  g_assert (IDE_IS_TWEAKS_ITEM (original));

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      "title", _("View and Customize Shortcuts…"),
                      NULL);
  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "go-next-symbolic",
                        NULL);
  adw_action_row_add_suffix (row, GTK_WIDGET (image));

  g_signal_connect_object (row,
                           "activated",
                           G_CALLBACK (gbp_shortcutui_tweaks_addin_row_activated_cb),
                           self->model,
                           G_CONNECT_SWAPPED);

  return GTK_WIDGET (row);
}

static void
gbp_shortcutui_tweaks_addin_load (IdeTweaksAddin *addin,
                                  IdeTweaks      *tweaks)
{
  GbpShortcutuiTweaksAddin *self = (GbpShortcutuiTweaksAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  self->model = g_object_new (IDE_TYPE_SHORTCUT_MANAGER, NULL);

  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/shortcutui/tweaks.ui"));
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), shortcutui_create_shortcuts_cb);

  IDE_TWEAKS_ADDIN_CLASS (gbp_shortcutui_tweaks_addin_parent_class)->load (addin, tweaks);
}

static void
gbp_shortcutui_tweaks_addin_unload (IdeTweaksAddin *addin,
                                    IdeTweaks      *tweaks)
{
  GbpShortcutuiTweaksAddin *self = (GbpShortcutuiTweaksAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  g_clear_object (&self->model);
}

static void
gbp_shortcutui_tweaks_addin_class_init (GbpShortcutuiTweaksAddinClass *klass)
{
  IdeTweaksAddinClass *tweaks_addin_class = IDE_TWEAKS_ADDIN_CLASS (klass);

  tweaks_addin_class->load = gbp_shortcutui_tweaks_addin_load;
  tweaks_addin_class->unload = gbp_shortcutui_tweaks_addin_unload;
}

static void
gbp_shortcutui_tweaks_addin_init (GbpShortcutuiTweaksAddin *self)
{
}
