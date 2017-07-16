/* gbp-spell-editor-addin.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "gbp-spell-editor-addin"

#include <glib/gi18n.h>

#include "gbp-spell-editor-addin.h"

struct _GbpSpellEditorAddin
{
  GObject        parent_instance;

  DzlDockWidget *dock;
};

static void
gbp_spell_editor_addin_load (IdeEditorAddin       *addin,
                             IdeEditorPerspective *editor)
{
  GbpSpellEditorAddin *self = (GbpSpellEditorAddin *)addin;
  IdeLayoutTransientSidebar *sidebar;

  g_assert (GBP_IS_SPELL_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  sidebar = ide_editor_perspective_get_transient_sidebar (editor);

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
}

static void
gbp_spell_editor_addin_unload (IdeEditorAddin       *addin,
                               IdeEditorPerspective *editor)
{
  GbpSpellEditorAddin *self = (GbpSpellEditorAddin *)addin;

  g_assert (GBP_IS_SPELL_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  if (self->dock != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->dock));
}

static void
gbp_spell_editor_addin_view_set (IdeEditorAddin *addin,
                                 IdeLayoutView  *view)
{
  GbpSpellEditorAddin *self = (GbpSpellEditorAddin *)addin;

  g_assert (GBP_IS_SPELL_EDITOR_ADDIN (self));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

  if (!IDE_IS_EDITOR_VIEW (view))
    view = NULL;


}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = gbp_spell_editor_addin_load;
  iface->unload = gbp_spell_editor_addin_unload;
  iface->view_set = gbp_spell_editor_addin_view_set;
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
