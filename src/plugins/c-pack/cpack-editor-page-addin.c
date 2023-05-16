/* cpack-editor-page-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "cpack-editor-page-addin"

#include "config.h"

#include <libide-editor.h>

#include "cpack-editor-page-addin.h"
#include "hdr-format.h"

struct _CpackEditorPageAddin
{
  GObject             parent_instance;
  GSimpleActionGroup *actions;
};

static void
format_decls_cb (GSimpleAction *action,
                 GVariant      *param,
                 gpointer       user_data)
{
  IdeEditorPage *view = user_data;
  g_autofree gchar *input = NULL;
  g_autofree gchar *output = NULL;
  IdeBuffer *buffer;
  GtkTextIter begin, end;

  g_assert (IDE_IS_EDITOR_PAGE (view));

  buffer = ide_editor_page_get_buffer (view);

  /* We require a selection */
  if (!gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end))
    return;

  input = gtk_text_iter_get_slice (&begin, &end);
  output = hdr_format_string (input, -1);

  if (output != NULL)
    {
      gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
      gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
      gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, output, -1);
      gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
    }

  gtk_widget_grab_focus (GTK_WIDGET (view));
}

static GActionEntry entries[] = {
  { "format", format_decls_cb },
};

static void
cpack_editor_page_addin_load (IdeEditorPageAddin *addin,
                              IdeEditorPage      *page)
{
  CpackEditorPageAddin *self = (CpackEditorPageAddin *)addin;

  g_assert (CPACK_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  self->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   page);
}

static void
cpack_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                IdeEditorPage      *page)
{
  CpackEditorPageAddin *self = (CpackEditorPageAddin *)addin;

  g_assert (CPACK_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  g_clear_object (&self->actions);
}

static GActionGroup *
cpack_editor_page_addin_ref_action_group (IdeEditorPageAddin *addin)
{
  return g_object_ref (G_ACTION_GROUP (CPACK_EDITOR_PAGE_ADDIN (addin)->actions));
}

static void
iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = cpack_editor_page_addin_load;
  iface->unload = cpack_editor_page_addin_unload;
  iface->ref_action_group = cpack_editor_page_addin_ref_action_group;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (CpackEditorPageAddin, cpack_editor_page_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, iface_init))

static void
cpack_editor_page_addin_class_init (CpackEditorPageAddinClass *klass)
{
}

static void
cpack_editor_page_addin_init (CpackEditorPageAddin *self)
{
}
