/* gbp-devhelp-editor-view-addin.c
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-devhelp-editor-view-addin"

#include <libxml/xmlreader.h>

#include "gbp-devhelp-editor-view-addin.h"

struct _GbpDevhelpEditorViewAddin
{
  GObject         parent_instance;
};

static void
documentation_requested_cb (GbpDevhelpEditorViewAddin *self,
                            const gchar               *word,
                            IdeSourceView             *source_view)
{
  g_assert (GBP_IS_DEVHELP_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  if (!dzl_str_empty0 (word))
    dzl_gtk_widget_action (GTK_WIDGET (source_view), "devhelp", "search",
                           g_variant_new_string (word));
}

static void
gbp_devhelp_editor_view_addin_load (IdeEditorViewAddin *addin,
                                    IdeEditorView      *view)
{
  g_assert (GBP_IS_DEVHELP_EDITOR_VIEW_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  g_signal_connect_object (ide_editor_view_get_view (view),
                           "documentation-requested",
                           G_CALLBACK (documentation_requested_cb),
                           addin,
                           G_CONNECT_SWAPPED);

}

static void
gbp_devhelp_editor_view_addin_unload (IdeEditorViewAddin *addin,
                                      IdeEditorView      *view)
{
  IdeSourceView *source_view;

  g_assert (GBP_IS_DEVHELP_EDITOR_VIEW_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  source_view = ide_editor_view_get_view (view);

  if (source_view != NULL)
    g_signal_handlers_disconnect_by_func (source_view,
                                          G_CALLBACK (documentation_requested_cb),
                                          addin);
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gbp_devhelp_editor_view_addin_load;
  iface->unload = gbp_devhelp_editor_view_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpDevhelpEditorViewAddin,
                         gbp_devhelp_editor_view_addin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN,
                                                editor_view_addin_iface_init))


static void
gbp_devhelp_editor_view_addin_class_init (GbpDevhelpEditorViewAddinClass *klass)
{
}

static void
gbp_devhelp_editor_view_addin_init (GbpDevhelpEditorViewAddin *self)
{
}
