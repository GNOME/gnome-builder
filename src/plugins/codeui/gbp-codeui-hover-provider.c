/* gbp-codeui-hover-provider.c
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

#define G_LOG_DOMAIN "gbp-codeui-hover-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-gui.h>
#include <libide-sourceview.h>
#include <libide-threading.h>

#include "gbp-codeui-hover-provider.h"

struct _GbpCodeuiHoverProvider
{
  GObject parent_instance;
};

static gboolean
gbp_codeui_hover_provider_populate (GtkSourceHoverProvider  *provider,
                                    GtkSourceHoverContext   *context,
                                    GtkSourceHoverDisplay   *display,
                                    GError                 **error)
{
  GbpCodeuiHoverProvider *self = (GbpCodeuiHoverProvider *)provider;
  g_autoptr(GPtrArray) line_diags = NULL;
  IdeDiagnostics *diagnostics;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GFile *file;
  guint line;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_HOVER_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_HOVER_CONTEXT (context));
  g_assert (GTK_SOURCE_IS_HOVER_DISPLAY (display));

  if (!gtk_source_hover_context_get_iter (context, &iter))
    goto handle_error;

  buffer = gtk_text_iter_get_buffer (&iter);

  if (!IDE_IS_BUFFER (buffer))
    goto handle_error;

  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  line = gtk_text_iter_get_line (&iter);

  if ((diagnostics = ide_buffer_get_diagnostics (IDE_BUFFER (buffer))) &&
      (line_diags = ide_diagnostics_get_diagnostics_at_line (diagnostics, file, line)) &&
      (line_diags->len > 0))
    {
      GtkBox *box;
      GtkLabel *label;
      g_autofree gchar *markup = NULL;

      IDE_PTR_ARRAY_SET_FREE_FUNC (line_diags, g_object_unref);

      box = g_object_new (GTK_TYPE_BOX,
                          "orientation", GTK_ORIENTATION_VERTICAL,
                          NULL);

      markup = g_strdup_printf ("<b>%s</b>:", _("Diagnostics"));

      label = g_object_new (GTK_TYPE_LABEL,
                            "label", markup,
                            "use-markup", TRUE,
                            "xalign", .0f,
                            NULL);
      gtk_box_append (box, GTK_WIDGET (label));

      for (guint i = 0; i < line_diags->len; i++)
        {
          IdeDiagnostic *diag = g_ptr_array_index (line_diags, i);
          g_autoptr(IdeMarkedContent) content = NULL;
          g_autofree gchar *text = ide_diagnostic_get_text_for_display (diag);
          GtkWidget *child;

          content = ide_marked_content_new_from_data (text, strlen (text), ide_diagnostic_get_marked_kind (diag));
          child = ide_marked_view_new (content);
          gtk_box_append (box, child);
        }

      gtk_widget_add_css_class (GTK_WIDGET (box), "hover-display-row");
      gtk_source_hover_display_append (display, GTK_WIDGET (box));

      return TRUE;
    }

handle_error:
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       "Not supported");

  return FALSE;
}

static void
hover_provider_iface_init (GtkSourceHoverProviderInterface *iface)
{
  iface->populate = gbp_codeui_hover_provider_populate;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCodeuiHoverProvider, gbp_codeui_hover_provider, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

static void
gbp_codeui_hover_provider_class_init (GbpCodeuiHoverProviderClass *klass)
{
}

static void
gbp_codeui_hover_provider_init (GbpCodeuiHoverProvider *self)
{
}
