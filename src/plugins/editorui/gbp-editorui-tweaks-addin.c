/* gbp-editorui-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-editorui-tweaks-addin"

#include "config.h"

#include <libide-gui.h>

#include "gbp-editorui-preview.h"
#include "gbp-editorui-scheme-selector.h"
#include "gbp-editorui-tweaks-addin.h"

struct _GbpEditoruiTweaksAddin
{
  IdeTweaksAddin parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpEditoruiTweaksAddin, gbp_editorui_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static GtkWidget *
editorui_create_style_scheme_preview (GbpEditoruiTweaksAddin *self,
                                      IdeTweaksWidget        *widget)
{
  g_assert (GBP_IS_EDITORUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));

  return g_object_new (GBP_TYPE_EDITORUI_PREVIEW,
                       "bottom-margin", 8,
                       "css-classes", IDE_STRV_INIT ("card"),
                       "cursor-visible", FALSE,
                       "left-margin", 12,
                       "monospace", TRUE,
                       "right-margin", 12,
                       "right-margin-position", 30,
                       "top-margin", 8,
                       NULL);
}

static GtkWidget *
editorui_create_style_scheme_selector (GbpEditoruiTweaksAddin *self,
                                       IdeTweaksWidget        *widget)
{
  g_assert (GBP_IS_EDITORUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));

  return g_object_new (GBP_TYPE_EDITORUI_SCHEME_SELECTOR,
                       "margin-top", 18,
                       NULL);
}

static void
gbp_editorui_tweaks_addin_class_init (GbpEditoruiTweaksAddinClass *klass)
{
}

static void
gbp_editorui_tweaks_addin_init (GbpEditoruiTweaksAddin *self)
{
  ide_tweaks_addin_set_resource_path (IDE_TWEAKS_ADDIN (self),
                                      "/plugins/editorui/tweaks.ui");
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self),
                                  editorui_create_style_scheme_preview);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self),
                                  editorui_create_style_scheme_selector);
}
