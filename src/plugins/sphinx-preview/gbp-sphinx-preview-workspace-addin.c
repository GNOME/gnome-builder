/* gbp-sphinx-preview-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-sphinx-preview-workspace-addin"

#include "config.h"

#include <libide-code.h>
#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-webkit.h>

#include "gbp-rst-html-generator.h"
#include "gbp-sphinx-compiler.h"
#include "gbp-sphinx-html-generator.h"
#include "gbp-sphinx-preview-workspace-addin.h"

struct _GbpSphinxPreviewWorkspaceAddin
{
  GObject        parent_instance;
  IdeWorkspace  *workspace;
  GSignalGroup  *buffer_signals;
  IdeEditorPage *editor_page;
  GHashTable    *compilers;
};

static void live_preview_action (GbpSphinxPreviewWorkspaceAddin *self,
                                 GVariant                       *params);

IDE_DEFINE_ACTION_GROUP (GbpSphinxPreviewWorkspaceAddin, gbp_sphinx_preview_workspace_addin, {
  { "preview", live_preview_action },
})

static void
gbp_sphinx_preview_workspace_addin_set_language (GbpSphinxPreviewWorkspaceAddin *self,
                                                 const char                     *language_id)
{
  gboolean enabled;

  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));

  IDE_TRACE_MSG ("Switching language-id to %s", language_id ? language_id : "NULL");

  enabled = ide_str_equal0 (language_id, "rst");
  gbp_sphinx_preview_workspace_addin_set_action_enabled (self, "preview", enabled);
}

static void
gbp_sphinx_preview_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                                 IdePage           *page)
{
  GbpSphinxPreviewWorkspaceAddin *self = (GbpSphinxPreviewWorkspaceAddin *)addin;
  IdeBuffer *buffer = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  self->editor_page = NULL;

  /* Make sure the page is an editor page and a local file, as that
   * is the only kind of files we can process with sphinx.
   */
  if (IDE_IS_EDITOR_PAGE (page))
    {
      GFile *file = ide_editor_page_get_file (IDE_EDITOR_PAGE (page));

      if (g_file_is_native (file))
        {
          self->editor_page = IDE_EDITOR_PAGE (page);
          buffer = ide_editor_page_get_buffer (self->editor_page);
        }
    }

  g_signal_group_set_target (self->buffer_signals, buffer);

  IDE_EXIT;
}

static void
gbp_sphinx_preview_workspace_addin_notify_language_id (GbpSphinxPreviewWorkspaceAddin *self,
                                                       GParamSpec                     *pspec,
                                                       IdeBuffer                      *buffer)
{
  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gbp_sphinx_preview_workspace_addin_set_language (self, ide_buffer_get_language_id (buffer));
}

static void
gbp_sphinx_preview_workspace_addin_bind (GbpSphinxPreviewWorkspaceAddin *self,
                                         IdeBuffer                      *buffer,
                                         GSignalGroup                   *signal_group)
{
  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_SIGNAL_GROUP (signal_group));

  gbp_sphinx_preview_workspace_addin_set_language (self, ide_buffer_get_language_id (buffer));
}

static void
gbp_sphinx_preview_workspace_addin_unbind (GbpSphinxPreviewWorkspaceAddin *self,
                                           GSignalGroup                   *signal_group)
{
  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (G_IS_SIGNAL_GROUP (signal_group));

  gbp_sphinx_preview_workspace_addin_set_language (self, NULL);
}

static void
gbp_sphinx_preview_workspace_addin_load (IdeWorkspaceAddin *addin,
                                         IdeWorkspace      *workspace)
{
  GbpSphinxPreviewWorkspaceAddin *self = (GbpSphinxPreviewWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;

  self->compilers = g_hash_table_new_full (g_file_hash,
                                           (GEqualFunc)g_file_equal,
                                           g_object_unref,
                                           g_object_unref);

  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);
  g_signal_connect_object (self->buffer_signals,
                           "bind",
                           G_CALLBACK (gbp_sphinx_preview_workspace_addin_bind),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->buffer_signals,
                           "unbind",
                           G_CALLBACK (gbp_sphinx_preview_workspace_addin_unbind),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->buffer_signals,
                                 "notify::language-id",
                                 G_CALLBACK (gbp_sphinx_preview_workspace_addin_notify_language_id),
                                 self,
                                 G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
gbp_sphinx_preview_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                           IdeWorkspace      *workspace)
{
  GbpSphinxPreviewWorkspaceAddin *self = (GbpSphinxPreviewWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  g_clear_pointer (&self->compilers, g_hash_table_unref);
  g_clear_object (&self->buffer_signals);

  self->editor_page = NULL;
  self->workspace = NULL;

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_sphinx_preview_workspace_addin_load;
  iface->unload = gbp_sphinx_preview_workspace_addin_unload;
  iface->page_changed = gbp_sphinx_preview_workspace_addin_page_changed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSphinxPreviewWorkspaceAddin, gbp_sphinx_preview_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_sphinx_preview_workspace_addin_init_action_group))

static void
gbp_sphinx_preview_workspace_addin_class_init (GbpSphinxPreviewWorkspaceAddinClass *klass)
{
}

static void
gbp_sphinx_preview_workspace_addin_init (GbpSphinxPreviewWorkspaceAddin *self)
{
  gbp_sphinx_preview_workspace_addin_set_action_enabled (self, "preview", FALSE);
}

static IdePage *
open_sphinx_preview (GbpSphinxPreviewWorkspaceAddin *self,
                     IdeBuffer                      *buffer,
                     GFile                          *conf_py)
{
  g_autoptr(IdeHtmlGenerator) generator = NULL;
  GbpSphinxCompiler *compiler;
  IdeWebkitPage *page;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (conf_py));
  g_assert (self->compilers != NULL);

  if (!(compiler = g_hash_table_lookup (self->compilers, conf_py)))
    {
      compiler = gbp_sphinx_compiler_new (conf_py);
      g_hash_table_insert (self->compilers, g_file_dup (conf_py), compiler);
    }

  generator = g_object_new (GBP_TYPE_SPHINX_HTML_GENERATOR,
                            "buffer", buffer,
                            "compiler", compiler,
                            NULL);
  page = ide_webkit_page_new_for_generator (generator);

  IDE_RETURN (IDE_PAGE (page));
}

static IdePage *
open_rst_preview (GbpSphinxPreviewWorkspaceAddin *self,
                  IdeBuffer                      *buffer)
{
  g_autoptr(IdeHtmlGenerator) generator = NULL;
  IdeWebkitPage *page;

  IDE_ENTRY;

  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  generator = g_object_new (GBP_TYPE_RST_HTML_GENERATOR,
                            "buffer", buffer,
                            NULL);
  page = ide_webkit_page_new_for_generator (generator);

  IDE_RETURN (IDE_PAGE (page));
}

static void
live_preview_action (GbpSphinxPreviewWorkspaceAddin *self,
                     GVariant                       *params)
{
  g_autoptr(PanelPosition) position = NULL;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) parent = NULL;
  IdeContext *context;
  IdePage *page = NULL;
  GFile *file;
  guint column;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SPHINX_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_EDITOR_PAGE (self->editor_page));

  context = ide_workspace_get_context (self->workspace);
  workdir = ide_context_ref_workdir (context);
  file = ide_editor_page_get_file (self->editor_page);
  parent = g_file_get_parent (file);
  buffer = g_signal_group_dup_target (self->buffer_signals);
  position = ide_page_get_position (IDE_PAGE (self->editor_page));

  if (!ide_panel_position_get_column (position, &column))
    column = 0;

  panel_position_set_column (position, column + 1);
  panel_position_set_depth (position, 0);

  while (parent != NULL &&
         (g_file_equal (workdir, parent) || g_file_has_prefix (parent, workdir)))
    {
      g_autoptr(GFile) conf_py = g_file_get_child (parent, "conf.py");
      g_autoptr(GFile) old_parent = NULL;

      if (g_file_query_exists (conf_py, NULL))
        {
          /* Found our top-level sphinx directory */
          page = open_sphinx_preview (self, buffer, conf_py);
          break;
        }

      old_parent = parent;
      parent = g_file_get_parent (old_parent);
    }

  if (page == NULL)
    page = open_rst_preview (self, buffer);

  ide_workspace_add_page (self->workspace, page, position);
  panel_widget_raise (PANEL_WIDGET (page));

  IDE_EXIT;
}
