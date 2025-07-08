/*
 * gbp-git-editor-page-addin.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-git-editor-page-addin"

#include "config.h"

#include <libide-editor.h>

#include "gbp-git-annotation-provider.h"
#include "gbp-git-editor-page-addin.h"

struct _GbpGitEditorPageAddin
{
  GObject                      parent_instance;

  GtkSourceAnnotations        *annotations;
  GbpGitAnnotationProvider    *annotation_provider;
  IdeSourceView               *view;

  GSettings                   *settings;
};

static void
gbp_git_editor_page_addin_connect (GbpGitEditorPageAddin *self)
{
  IdeBuffer *buffer;

  g_assert (GBP_IS_GIT_EDITOR_PAGE_ADDIN (self));

  if (self->view == NULL)
    return;

  buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view)));

  g_set_object (&self->annotations,
                gtk_source_view_get_annotations (GTK_SOURCE_VIEW (self->view)));

  if (g_settings_get_boolean (self->settings, "inline-blame-enabled"))
    {
      g_set_object (&self->annotation_provider, gbp_git_annotation_provider_new (buffer));

      gtk_source_annotations_add_provider (self->annotations,
                                           GTK_SOURCE_ANNOTATION_PROVIDER (self->annotation_provider));
    }
}

static void
gbp_git_editor_page_addin_disconnect (GbpGitEditorPageAddin *self)
{
  g_assert (GBP_IS_GIT_EDITOR_PAGE_ADDIN (self));

  if (self->annotation_provider != NULL)
    {
      if (self->annotations != NULL)
        {
          gtk_source_annotations_remove_provider (self->annotations,
                                                  GTK_SOURCE_ANNOTATION_PROVIDER (self->annotation_provider));
        }
    }

  g_clear_object (&self->annotation_provider);
}

void
on_inline_blame_setting_changed (GSettings *settings,
                                 gchar     *key,
                                 gpointer   user_data)
{
  GbpGitEditorPageAddin *self = user_data;

  g_assert (GBP_IS_GIT_EDITOR_PAGE_ADDIN (self));

  if (g_settings_get_boolean (self->settings, "inline-blame-enabled"))
    {
      gbp_git_editor_page_addin_connect (self);
    }
  else
    {
      gbp_git_editor_page_addin_disconnect (self);
    }
}

static void
gbp_git_editor_page_addin_load (IdeEditorPageAddin *addin,
                                IdeEditorPage      *page)
{
  GbpGitEditorPageAddin *self = (GbpGitEditorPageAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  self->view = ide_editor_page_get_view (page);
  self->settings = g_settings_new ("org.gnome.builder.git");

  g_signal_connect_object (self->settings,
                           "changed::inline-blame-enabled",
                           G_CALLBACK (on_inline_blame_setting_changed),
                           self,
                           0);

  gbp_git_editor_page_addin_connect (self);

  IDE_EXIT;
}

static void
gbp_git_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                  IdeEditorPage      *page)
{
  GbpGitEditorPageAddin *self = (GbpGitEditorPageAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  gbp_git_editor_page_addin_disconnect (self);

  g_clear_object (&self->settings);
  self->view = NULL;

  IDE_EXIT;
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_git_editor_page_addin_load;
  iface->unload = gbp_git_editor_page_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitEditorPageAddin, gbp_git_editor_page_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
gbp_git_editor_page_addin_dispose (GObject *object)
{
  GbpGitEditorPageAddin *self = (GbpGitEditorPageAddin *)object;

  gbp_git_editor_page_addin_disconnect (self);

  g_clear_object (&self->settings);
  g_clear_object (&self->view);
  g_clear_object (&self->annotations);

  G_OBJECT_CLASS (gbp_git_editor_page_addin_parent_class)->dispose (object);
}

static void
gbp_git_editor_page_addin_class_init (GbpGitEditorPageAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_git_editor_page_addin_dispose;
}

static void
gbp_git_editor_page_addin_init (GbpGitEditorPageAddin *self)
{
}
