/* gbp-markdown-preview-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-markdown-preview-workspace-addin"

#include "config.h"

#include <libide-code.h>
#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-webkit.h>

#include "gbp-markdown-html-generator.h"
#include "gbp-markdown-preview-workspace-addin.h"

struct _GbpMarkdownPreviewWorkspaceAddin
{
  GObject        parent_instance;
  IdeWorkspace  *workspace;
  GSignalGroup  *buffer_signals;
  IdeEditorPage *editor_page;
};

static void live_preview_action (GbpMarkdownPreviewWorkspaceAddin *self,
                                 GVariant                     *params);

IDE_DEFINE_ACTION_GROUP (GbpMarkdownPreviewWorkspaceAddin, gbp_markdown_preview_workspace_addin, {
  { "preview", live_preview_action },
})

static void
gbp_markdown_preview_workspace_addin_set_language (GbpMarkdownPreviewWorkspaceAddin *self,
                                               const char                   *language_id)
{
  g_assert (GBP_IS_MARKDOWN_PREVIEW_WORKSPACE_ADDIN (self));

  IDE_TRACE_MSG ("Switching language-id to %s", language_id ? language_id : "NULL");
  gbp_markdown_preview_workspace_addin_set_action_enabled (self,
                                                           "preview",
                                                           ide_str_equal0 ("markdown", language_id));
}

static void
gbp_markdown_preview_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                                   IdePage           *page)
{
  GbpMarkdownPreviewWorkspaceAddin *self = (GbpMarkdownPreviewWorkspaceAddin *)addin;
  IdeBuffer *buffer = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MARKDOWN_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  self->editor_page = NULL;

  if (IDE_IS_EDITOR_PAGE (page))
    {
      self->editor_page = IDE_EDITOR_PAGE (page);
      buffer = ide_editor_page_get_buffer (self->editor_page);
    }

  g_signal_group_set_target (self->buffer_signals, buffer);

  IDE_EXIT;
}

static void
gbp_markdown_preview_workspace_addin_notify_language_id (GbpMarkdownPreviewWorkspaceAddin *self,
                                                         GParamSpec                       *pspec,
                                                         IdeBuffer                        *buffer)
{
  g_assert (GBP_IS_MARKDOWN_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gbp_markdown_preview_workspace_addin_set_language (self, ide_buffer_get_language_id (buffer));
}

static void
gbp_markdown_preview_workspace_addin_bind (GbpMarkdownPreviewWorkspaceAddin *self,
                                           IdeBuffer                        *buffer,
                                           GSignalGroup                     *signal_group)
{
  g_assert (GBP_IS_MARKDOWN_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_SIGNAL_GROUP (signal_group));

  gbp_markdown_preview_workspace_addin_set_language (self, ide_buffer_get_language_id (buffer));
}

static void
gbp_markdown_preview_workspace_addin_unbind (GbpMarkdownPreviewWorkspaceAddin *self,
                                             GSignalGroup                     *signal_group)
{
  g_assert (GBP_IS_MARKDOWN_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (G_IS_SIGNAL_GROUP (signal_group));

  gbp_markdown_preview_workspace_addin_set_language (self, NULL);
}

static void
gbp_markdown_preview_workspace_addin_load (IdeWorkspaceAddin *addin,
                                           IdeWorkspace      *workspace)
{
  GbpMarkdownPreviewWorkspaceAddin *self = (GbpMarkdownPreviewWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_MARKDOWN_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;

  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);
  g_signal_connect_object (self->buffer_signals,
                           "bind",
                           G_CALLBACK (gbp_markdown_preview_workspace_addin_bind),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->buffer_signals,
                           "unbind",
                           G_CALLBACK (gbp_markdown_preview_workspace_addin_unbind),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->buffer_signals,
                                 "notify::language-id",
                                 G_CALLBACK (gbp_markdown_preview_workspace_addin_notify_language_id),
                                 self,
                                 G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
gbp_markdown_preview_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                             IdeWorkspace      *workspace)
{
  GbpMarkdownPreviewWorkspaceAddin *self = (GbpMarkdownPreviewWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MARKDOWN_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  g_clear_object (&self->buffer_signals);

  self->editor_page = NULL;
  self->workspace = NULL;

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_markdown_preview_workspace_addin_load;
  iface->unload = gbp_markdown_preview_workspace_addin_unload;
  iface->page_changed = gbp_markdown_preview_workspace_addin_page_changed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMarkdownPreviewWorkspaceAddin, gbp_markdown_preview_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_markdown_preview_workspace_addin_init_action_group))

static void
gbp_markdown_preview_workspace_addin_class_init (GbpMarkdownPreviewWorkspaceAddinClass *klass)
{
}

static void
gbp_markdown_preview_workspace_addin_init (GbpMarkdownPreviewWorkspaceAddin *self)
{
  gbp_markdown_preview_workspace_addin_set_action_enabled (self, "preview", FALSE);
}

static void
live_preview_action (GbpMarkdownPreviewWorkspaceAddin *self,
                     GVariant                     *params)
{
  g_autoptr(IdeHtmlGenerator) generator = NULL;
  g_autoptr(PanelPosition) position = NULL;
  g_autoptr(IdeBuffer) buffer = NULL;
  IdeWebkitPage *page;
  guint column = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MARKDOWN_PREVIEW_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_EDITOR_PAGE (self->editor_page));

  buffer = g_signal_group_dup_target (self->buffer_signals);
  generator = g_object_new (GBP_TYPE_MARKDOWN_HTML_GENERATOR,
                            "buffer", buffer,
                            NULL);
  page = ide_webkit_page_new_for_generator (generator);
  position = ide_page_get_position (IDE_PAGE (self->editor_page));

  if (!ide_panel_position_get_column (position, &column))
    column = 0;

  panel_position_set_column (position, column + 1);
  panel_position_set_depth (position, 0);

  ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);
  panel_widget_raise (PANEL_WIDGET (page));

  IDE_EXIT;
}
