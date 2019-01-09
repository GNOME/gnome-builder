/* gbp-omni-gutter-editor-page-addin.c
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

#define G_LOG_DOMAIN "gbp-omni-gutter-editor-page-addin"

#include <libide-editor.h>

#include "gbp-omni-gutter-editor-page-addin.h"
#include "gbp-omni-gutter-renderer.h"

struct _GbpOmniGutterEditorPageAddin
{
  GObject parent_instance;
};

static void
gbp_omni_gutter_editor_page_addin_load (IdeEditorPageAddin *addin,
                                        IdeEditorPage      *page)
{
  GbpOmniGutterRenderer *gutter;
  IdeSourceView *view;

  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  view = ide_editor_page_get_view (page);
  gutter = gbp_omni_gutter_renderer_new ();
  ide_source_view_set_gutter (view, IDE_GUTTER (gutter));
}

static void
gbp_omni_gutter_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                          IdeEditorPage      *page)
{
  IdeSourceView *view;

  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  view = ide_editor_page_get_view (page);
  ide_source_view_set_gutter (view, NULL);
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_omni_gutter_editor_page_addin_load;
  iface->unload = gbp_omni_gutter_editor_page_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpOmniGutterEditorPageAddin, gbp_omni_gutter_editor_page_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
gbp_omni_gutter_editor_page_addin_class_init (GbpOmniGutterEditorPageAddinClass *klass)
{
}

static void
gbp_omni_gutter_editor_page_addin_init (GbpOmniGutterEditorPageAddin *self)
{
}
