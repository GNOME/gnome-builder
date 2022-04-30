/* gbp-buildui-editor-page-addin.c
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

#define G_LOG_DOMAIN "gbp-buildui-editor-page-addin"

#include <libide-editor.h>
#include <libide-foundry.h>

#include "gbp-buildui-editor-page-addin.h"

struct _GbpBuilduiEditorPageAddin
{
  GObject parent_instance;
};

static gboolean
parse_version (const gchar *str,
               gint        *major,
               gint        *minor)
{
  g_autofree gchar *copy = g_strdup (str);
  const gchar *begin = str;
  gchar *ptr;

  ptr = copy;
  *major = 0;
  *minor = 0;

  if ((ptr = strchr (ptr, '.')))
    {
      *ptr = 0;

      *major = g_ascii_strtoll (begin, NULL, 10);
      begin = ptr + 1;

      if ((ptr = strchr (ptr, '.')))
        *ptr = 0;

      *minor = g_ascii_strtoll (begin, NULL, 10);

      return TRUE;
    }

  return FALSE;
}

static void
on_push_snippet_cb (GbpBuilduiEditorPageAddin *self,
                    GtkSourceSnippet          *snippet,
                    GtkTextIter               *iter,
                    IdeSourceView             *view)
{
  g_autoptr(IdeContext) context = NULL;
  g_autofree gchar *project_version = NULL;
  GtkSourceSnippetContext *snippet_context;
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_SOURCE_IS_SNIPPET (snippet));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  context = ide_buffer_ref_context (IDE_BUFFER (buffer));

  if (ide_context_has_project (context))
    {
      IdeBuildSystem *build_system = ide_build_system_from_context (context);

      if (build_system != NULL)
        {
          if ((project_version = ide_build_system_get_project_version (build_system)))
            {
              gint major = 0;
              gint minor = 0;

              if (parse_version (project_version, &major, &minor))
                {
                  /* TODO: We might need to come up with a way to make this
                   *       a project setting, but this seems like a reasonable
                   *       default for how we do semantif versioning in GNOME.
                   */
                  if (minor % 2 == 1)
                    minor++;

                  g_free (project_version);
                  project_version = g_strdup_printf ("%d.%d", major, minor);
                }
            }
        }
    }

  snippet_context = gtk_source_snippet_get_context (snippet);
  gtk_source_snippet_context_set_variable (snippet_context,
                                           "project_version",
                                           project_version ?: "");
}

static void
gbp_buildui_editor_page_addin_load (IdeEditorPageAddin *addin,
                                    IdeEditorPage      *page)
{
  IdeSourceView *view;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  view = ide_editor_page_get_view (page);

  g_signal_connect_object (view,
                           "push-snippet",
                           G_CALLBACK (on_push_snippet_cb),
                           addin,
                           G_CONNECT_SWAPPED);
}

static void
gbp_buildui_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                      IdeEditorPage      *page)
{
  IdeSourceView *view;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  view = ide_editor_page_get_view (page);

  g_signal_handlers_disconnect_by_func (view,
                                        G_CALLBACK (on_push_snippet_cb),
                                        addin);
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_buildui_editor_page_addin_load;
  iface->unload = gbp_buildui_editor_page_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpBuilduiEditorPageAddin, gbp_buildui_editor_page_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
gbp_buildui_editor_page_addin_class_init (GbpBuilduiEditorPageAddinClass *klass)
{
}

static void
gbp_buildui_editor_page_addin_init (GbpBuilduiEditorPageAddin *self)
{
}
