/*
 * folding-editor-page-addin.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <libide-editor.h>

#include "folding-editor-page-addin.h"
#include "folding-gutter-renderer.h"

struct _FoldingEditorPageAddin
{
  GObject                parent_instance;
  IdeEditorPage         *page;
  FoldingGutterRenderer *gutter_renderer;
};

static void
folding_editor_page_addin_load (IdeEditorPageAddin *addin,
                                IdeEditorPage      *page)
{
  FoldingEditorPageAddin *self = (FoldingEditorPageAddin *)addin;
  GtkSourceGutterRenderer *gutter_renderer;
  GtkSourceGutter *gutter;
  IdeSourceView *view;

  g_assert (FOLDING_IS_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  self->page = page;
  self->gutter_renderer = folding_gutter_renderer_new ();

  view = ide_editor_page_get_view (page);
  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (view), GTK_TEXT_WINDOW_LEFT);
  gutter_renderer = GTK_SOURCE_GUTTER_RENDERER (self->gutter_renderer);

  gtk_source_gutter_insert (gutter, gutter_renderer, 10000);
}

static void
folding_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                  IdeEditorPage      *page)
{
  FoldingEditorPageAddin *self = (FoldingEditorPageAddin *)addin;
  GtkSourceGutterRenderer *gutter_renderer;
  GtkSourceGutter *gutter;
  IdeSourceView *view;

  g_assert (FOLDING_IS_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  view = ide_editor_page_get_view (page);
  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (view), GTK_TEXT_WINDOW_LEFT);
  gutter_renderer = GTK_SOURCE_GUTTER_RENDERER (self->gutter_renderer);

  gtk_source_gutter_remove (gutter, gutter_renderer);

  self->gutter_renderer = NULL;
  self->page = NULL;
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = folding_editor_page_addin_load;
  iface->unload = folding_editor_page_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoldingEditorPageAddin, folding_editor_page_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
folding_editor_page_addin_class_init (FoldingEditorPageAddinClass *klass)
{
}

static void
folding_editor_page_addin_init (FoldingEditorPageAddin *self)
{
}
