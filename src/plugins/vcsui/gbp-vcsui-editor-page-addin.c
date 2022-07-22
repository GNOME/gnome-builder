/* gbp-vcsui-editor-page-addin.c
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

#define G_LOG_DOMAIN "gbp-vcsui-editor-page-addin"

#include "config.h"

#include <libide-editor.h>

#include "gbp-vcsui-editor-page-addin.h"

struct _GbpVcsuiEditorPageAddin
{
  GObject parent_instance;
};

static void
on_push_snippet_cb (GbpVcsuiEditorPageAddin *self,
                    GtkSourceSnippet        *snippet,
                    GtkTextIter             *iter,
                    IdeSourceView           *source_view)
{
  g_autoptr(IdeVcsConfig) vcs_config = NULL;
  g_autoptr(IdeContext) ide_context = NULL;
  GtkSourceSnippetContext *context;
  IdeBuffer *buffer;
  IdeVcs *vcs;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_EDITOR_PAGE_ADDIN (self));
  g_assert (GTK_SOURCE_IS_SNIPPET (snippet));
  g_assert (iter != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view)));
  ide_context = ide_buffer_ref_context (buffer);
  context = gtk_source_snippet_get_context (snippet);

  if ((vcs = ide_vcs_from_context (ide_context)) &&
       (vcs_config = ide_vcs_get_config (vcs)))
    {
      GValue value = G_VALUE_INIT;

      g_value_init (&value, G_TYPE_STRING);

      ide_vcs_config_get_config (vcs_config, IDE_VCS_CONFIG_FULL_NAME, &value);

      if (!ide_str_empty0 (g_value_get_string (&value)))
        {
          gtk_source_snippet_context_set_variable (context, "NAME", g_value_get_string (&value));
          gtk_source_snippet_context_set_variable (context, "author", g_value_get_string (&value));
          gtk_source_snippet_context_set_variable (context, "fullname", g_value_get_string (&value));
          gtk_source_snippet_context_set_variable (context, "username", g_value_get_string (&value));
        }

      g_value_reset (&value);

      ide_vcs_config_get_config (vcs_config, IDE_VCS_CONFIG_EMAIL, &value);

      if (!ide_str_empty0 (g_value_get_string (&value)))
        {
          gtk_source_snippet_context_set_variable (context, "email", g_value_get_string (&value));
          gtk_source_snippet_context_set_variable (context, "EMAIL", g_value_get_string (&value));
        }

      g_value_unset (&value);
    }

  IDE_EXIT;
}

static void
gbp_vcsui_editor_page_addin_load (IdeEditorPageAddin *addin,
                                  IdeEditorPage      *page)
{
  IdeSourceView *source_view;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  source_view = ide_editor_page_get_view (page);

  g_signal_connect_object (source_view,
                           "push-snippet",
                           G_CALLBACK (on_push_snippet_cb),
                           addin,
                           G_CONNECT_SWAPPED);
}

static void
gbp_vcsui_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                    IdeEditorPage      *page)
{
  IdeSourceView *source_view;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  source_view = ide_editor_page_get_view (page);

  g_signal_handlers_disconnect_by_func (source_view,
                                        G_CALLBACK (on_push_snippet_cb),
                                        addin);
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_vcsui_editor_page_addin_load;
  iface->unload = gbp_vcsui_editor_page_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVcsuiEditorPageAddin, gbp_vcsui_editor_page_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
gbp_vcsui_editor_page_addin_class_init (GbpVcsuiEditorPageAddinClass *klass)
{
}

static void
gbp_vcsui_editor_page_addin_init (GbpVcsuiEditorPageAddin *self)
{
}
