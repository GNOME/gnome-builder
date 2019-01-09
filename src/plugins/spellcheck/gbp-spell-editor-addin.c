/* gbp-spell-editor-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-spell-editor-addin"

#include <glib/gi18n.h>

#include "gbp-spell-editor-addin.h"
#include "gbp-spell-private.h"

struct _GbpSpellEditorAddin
{
  GObject               parent_instance;

  IdeEditorSurface *editor;

  DzlDockWidget        *dock;
  GbpSpellWidget       *widget;
};

static void
gbp_spell_editor_addin_load (IdeEditorAddin       *addin,
                             IdeEditorSurface *editor)
{
  GbpSpellEditorAddin *self = (GbpSpellEditorAddin *)addin;
  IdeTransientSidebar *sidebar;

  g_assert (GBP_IS_SPELL_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (editor));

  self->editor = editor;

  sidebar = ide_editor_surface_get_transient_sidebar (editor);

  self->dock = g_object_new (DZL_TYPE_DOCK_WIDGET,
                             "title", _("Spelling"),
                             "icon-name", "tools-check-spelling-symbolic",
                             "visible", TRUE,
                             NULL);
  g_signal_connect (self->dock,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->dock);
  gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (self->dock));

  self->widget = g_object_new (GBP_TYPE_SPELL_WIDGET,
                               "visible", TRUE,
                               NULL);
  g_signal_connect (self->widget,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->widget);
  gtk_container_add (GTK_CONTAINER (self->dock), GTK_WIDGET (self->widget));
}

static void
gbp_spell_editor_addin_unload (IdeEditorAddin       *addin,
                               IdeEditorSurface *editor)
{
  GbpSpellEditorAddin *self = (GbpSpellEditorAddin *)addin;

  g_assert (GBP_IS_SPELL_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (editor));

  if (self->dock != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->dock));

  if (self->widget != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->widget));

  self->editor = NULL;
}

static void
gbp_spell_editor_addin_page_set (IdeEditorAddin *addin,
                                 IdePage  *view)
{
  GbpSpellEditorAddin *self = (GbpSpellEditorAddin *)addin;
  IdeEditorPage *current;

  g_assert (GBP_IS_SPELL_EDITOR_ADDIN (self));
  g_assert (!view || IDE_IS_PAGE (view));

  /* If there is currently a view attached, and this is
   * a new view, then we want to unset it so that the
   * panel can be dismissed.
   */

  current = gbp_spell_widget_get_editor (self->widget);

  if (current != NULL)
    {
      if (view == IDE_PAGE (current))
        return;

      gbp_spell_widget_set_editor (self->widget, NULL);

      /* TODO: We need transient sidebar API for this to dismiss our display.
       *       Normally it's done automatically, but if the last view is closed
       *       it can get confused.
       */
      g_object_set (self->editor, "right-visible", FALSE, NULL);
    }
}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = gbp_spell_editor_addin_load;
  iface->unload = gbp_spell_editor_addin_unload;
  iface->page_set = gbp_spell_editor_addin_page_set;
}

G_DEFINE_TYPE_WITH_CODE (GbpSpellEditorAddin, gbp_spell_editor_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_ADDIN, editor_addin_iface_init))

static void
gbp_spell_editor_addin_class_init (GbpSpellEditorAddinClass *klass)
{
}

static void
gbp_spell_editor_addin_init (GbpSpellEditorAddin *self)
{
}

void
_gbp_spell_editor_addin_begin (GbpSpellEditorAddin *self,
                               IdeEditorPage       *view)
{
  IdeTransientSidebar *sidebar;

  g_return_if_fail (GBP_IS_SPELL_EDITOR_ADDIN (self));
  g_return_if_fail (IDE_IS_EDITOR_PAGE (view));

  gbp_spell_widget_set_editor (self->widget, view);

  sidebar = ide_editor_surface_get_transient_sidebar (self->editor);
  ide_transient_sidebar_set_page (sidebar, IDE_PAGE (view));
  ide_transient_sidebar_set_panel (sidebar, GTK_WIDGET (self->dock));

  /* TODO: This needs API via transient sidebar panel */
  g_object_set (self->editor, "right-visible", TRUE, NULL);
}

void
_gbp_spell_editor_addin_cancel (GbpSpellEditorAddin *self,
                                IdeEditorPage       *view)
{
  g_return_if_fail (GBP_IS_SPELL_EDITOR_ADDIN (self));
  g_return_if_fail (IDE_IS_EDITOR_PAGE (view));

  gbp_spell_widget_set_editor (self->widget, NULL);

  /* TODO: This needs API via transient sidebar panel */
  g_object_set (self->editor, "right-visible", FALSE, NULL);
}
