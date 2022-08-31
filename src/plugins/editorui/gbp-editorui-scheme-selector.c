/* gbp-editorui-scheme-selector.c
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

#define G_LOG_DOMAIN "gbp-editorui-scheme-selector"

#include "config.h"

#include <libide-gui.h>
#include <libide-sourceview.h>
#include <libide-threading.h>

#include "gbp-editorui-scheme-selector.h"

struct _GbpEditoruiSchemeSelector
{
  GtkWidget   parent_instance;

  GtkFlowBox *flow_box;
  GSettings  *settings;

  guint       disposed : 1;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpEditoruiSchemeSelector, gbp_editorui_scheme_selector, GTK_TYPE_WIDGET)

typedef struct
{
  const char           *id;
  const char           *sort_key;
  GtkSourceStyleScheme *scheme;
  guint                 has_alt : 1;
  guint                 is_dark : 1;
} SchemeInfo;

static int
sort_schemes_cb (gconstpointer a,
                 gconstpointer b)
{
  const SchemeInfo *info_a = a;
  const SchemeInfo *info_b = b;

  /* Light schemes first */
  if (!info_a->is_dark && info_b->is_dark)
    return -1;
  else if (info_a->is_dark && !info_b->is_dark)
    return 1;

  /* Items with variants first */
  if (info_a->has_alt && !info_b->has_alt)
    return -1;
  else if (!info_a->has_alt && info_b->has_alt)
    return 1;

  return g_utf8_collate (info_a->sort_key, info_b->sort_key);
}

static void
update_style_scheme_selection (GtkFlowBox *flow_box)
{
  const char *id;

  g_assert (GTK_IS_FLOW_BOX (flow_box));

  id = ide_application_get_style_scheme (IDE_APPLICATION_DEFAULT);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (flow_box));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkFlowBoxChild *intermediate = GTK_FLOW_BOX_CHILD (child);
      GtkSourceStyleSchemePreview *preview = GTK_SOURCE_STYLE_SCHEME_PREVIEW (gtk_flow_box_child_get_child (intermediate));
      GtkSourceStyleScheme *scheme = gtk_source_style_scheme_preview_get_scheme (preview);
      const char *scheme_id = gtk_source_style_scheme_get_id (scheme);
      gboolean selected = g_strcmp0 (scheme_id, id) == 0;

      gtk_source_style_scheme_preview_set_selected (preview, selected);
    }
}

static void
update_style_schemes (GtkFlowBox *flow_box)
{
  GtkSourceStyleSchemeManager *sm;
  const char * const *scheme_ids;
  g_autoptr(GArray) schemes = NULL;
  const char *current_scheme;
  gboolean is_dark;
  guint j = 0;
  GtkWidget *child;

  g_assert (GTK_IS_FLOW_BOX (flow_box));

  schemes = g_array_new (FALSE, FALSE, sizeof (SchemeInfo));
  is_dark = adw_style_manager_get_dark (adw_style_manager_get_default ());
  current_scheme = ide_application_get_style_scheme (IDE_APPLICATION_DEFAULT);

  /* Populate schemes for preferences */
  sm = gtk_source_style_scheme_manager_get_default ();
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (sm);

  for (guint i = 0; scheme_ids[i]; i++)
    {
      SchemeInfo info;

      /* Ignore our printing scheme */
      if (g_strcmp0 (scheme_ids[i], "printing") == 0)
        continue;

      info.scheme = gtk_source_style_scheme_manager_get_scheme (sm, scheme_ids[i]);
      info.id = gtk_source_style_scheme_get_id (info.scheme);
      info.sort_key = gtk_source_style_scheme_get_name (info.scheme);
      info.has_alt = FALSE;
      info.is_dark = FALSE;

      if (ide_source_style_scheme_is_dark (info.scheme))
        {
          GtkSourceStyleScheme *alt = ide_source_style_scheme_get_variant (info.scheme, "light");

          g_assert (GTK_SOURCE_IS_STYLE_SCHEME (alt));

          if (alt != info.scheme)
            {
              info.sort_key = gtk_source_style_scheme_get_id (alt);
              info.has_alt = TRUE;
            }

          info.is_dark = TRUE;
        }
      else
        {
          GtkSourceStyleScheme *alt = ide_source_style_scheme_get_variant (info.scheme, "dark");

          g_assert (GTK_SOURCE_IS_STYLE_SCHEME (alt));

          if (alt != info.scheme)
            info.has_alt = TRUE;
        }

      g_array_append_val (schemes, info);
    }

  g_array_sort (schemes, sort_schemes_cb);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (flow_box))))
    gtk_flow_box_remove (flow_box, child);

  for (guint i = 0; i < schemes->len; i++)
    {
      const SchemeInfo *info = &g_array_index (schemes, SchemeInfo, i);
      GtkWidget *preview;

      /* Ignore if not matching light/dark variant for app, unless it is
       * the current scheme and it has no alternate.
       */
      if (is_dark != ide_source_style_scheme_is_dark (info->scheme) &&
          (g_strcmp0 (info->id, current_scheme) != 0 || info->has_alt))
        continue;

      preview = gtk_source_style_scheme_preview_new (info->scheme);
      gtk_actionable_set_action_name (GTK_ACTIONABLE (preview), "app.style-scheme-name");
      gtk_actionable_set_action_target (GTK_ACTIONABLE (preview), "s", info->id);
      gtk_flow_box_insert (flow_box, preview, -1);

      j++;
    }

  update_style_scheme_selection (flow_box);
}

static void
style_scheme_activated_cb (GbpEditoruiSchemeSelector *self,
                           GtkFlowBoxChild           *child,
                           GtkFlowBox                *flow_box)
{
  GtkWidget *preview;

  g_assert (GBP_IS_EDITORUI_SCHEME_SELECTOR (self));
  g_assert (GTK_IS_FLOW_BOX_CHILD (child));
  g_assert (GTK_IS_FLOW_BOX (flow_box));

  if ((preview = gtk_flow_box_child_get_child (child)))
    gtk_widget_activate (preview);
}

static gboolean
can_install_scheme (GtkSourceStyleSchemeManager *manager,
                    const char * const          *scheme_ids,
                    GFile                       *file)
{
  g_autofree char *uri = NULL;
  const char *path;

  g_assert (GTK_SOURCE_IS_STYLE_SCHEME_MANAGER (manager));
  g_assert (G_IS_FILE (file));

  uri = g_file_get_uri (file);

  /* Don't allow resources, which would be weird anyway */
  if (g_str_has_prefix (uri, "resource://"))
    return FALSE;

  /* Make sure it's in the form of name.xml as we will require
   * that elsewhere anyway.
   */
  if (!g_str_has_suffix (uri, ".xml"))
    return FALSE;

  /* Not a native file, so likely not already installed */
  if (!g_file_is_native (file))
    return TRUE;

  path = g_file_peek_path (file);
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);
  for (guint i = 0; scheme_ids[i] != NULL; i++)
    {
      GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_ids[i]);
      const char *filename = gtk_source_style_scheme_get_filename (scheme);

      /* If we have already loaded this scheme, then ignore it */
      if (g_strcmp0 (filename, path) == 0)
        return FALSE;
    }

  return TRUE;
}

static void
schemes_installed_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr(GbpEditoruiSchemeSelector) self = user_data;
  GtkSourceStyleSchemeManager *manager;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_EDITORUI_SCHEME_SELECTOR (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_application_install_schemes_finish (IDE_APPLICATION_DEFAULT, result, &error))
    g_critical ("Failed to install schemes: %s", error->message);

  manager = gtk_source_style_scheme_manager_get_default ();
  gtk_source_style_scheme_manager_force_rescan (manager);

  if (!self->disposed)
    update_style_schemes (self->flow_box);
}

static gboolean
gbp_editorui_scheme_selector_drop_scheme_cb (GbpEditoruiSchemeSelector *self,
                                             const GValue              *value,
                                             double                     x,
                                             double                     y,
                                             GtkDropTarget             *drop_target)
{
  g_assert (GBP_IS_EDITORUI_SCHEME_SELECTOR (self));
  g_assert (GTK_IS_DROP_TARGET (drop_target));

  if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
      GSList *list = g_value_get_boxed (value);
      g_autoptr(GPtrArray) to_install = NULL;
      GtkSourceStyleSchemeManager *manager;
      const char * const *scheme_ids;

      if (list == NULL)
        return FALSE;

      manager = gtk_source_style_scheme_manager_get_default ();
      scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);
      to_install = g_ptr_array_new_with_free_func (g_object_unref);

      for (const GSList *iter = list; iter; iter = iter->next)
        {
          GFile *file = iter->data;

          if (can_install_scheme (manager, scheme_ids, file))
            g_ptr_array_add (to_install, g_object_ref (file));
        }

      if (to_install->len == 0)
        return FALSE;

      ide_application_install_schemes_async (IDE_APPLICATION_DEFAULT,
                                             (GFile **)(gpointer)to_install->pdata,
                                             to_install->len,
                                             NULL,
                                             schemes_installed_cb,
                                             g_object_ref (self));

      return TRUE;
    }

  return FALSE;
}

static void
gbp_editorui_scheme_selector_constructed (GObject *object)
{
  GbpEditoruiSchemeSelector *self = (GbpEditoruiSchemeSelector *)object;

  G_OBJECT_CLASS (gbp_editorui_scheme_selector_parent_class)->constructed (object);

  update_style_schemes (self->flow_box);
}

static void
gbp_editorui_scheme_selector_dispose (GObject *object)
{
  GbpEditoruiSchemeSelector *self = (GbpEditoruiSchemeSelector *)object;

  self->disposed = TRUE;

  g_clear_pointer ((GtkWidget **)&self->flow_box, gtk_widget_unparent);

  G_OBJECT_CLASS (gbp_editorui_scheme_selector_parent_class)->dispose (object);
}

static void
gbp_editorui_scheme_selector_class_init (GbpEditoruiSchemeSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_editorui_scheme_selector_constructed;
  object_class->dispose = gbp_editorui_scheme_selector_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/editorui/gbp-editorui-scheme-selector.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpEditoruiSchemeSelector, flow_box);
  gtk_widget_class_bind_template_child (widget_class, GbpEditoruiSchemeSelector, settings);
  gtk_widget_class_bind_template_callback (widget_class, style_scheme_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_style_scheme_selection);
}

static void
gbp_editorui_scheme_selector_init (GbpEditoruiSchemeSelector *self)
{
  GtkDropTarget *drop_target;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (IDE_APPLICATION_DEFAULT,
                           "notify::style-scheme",
                           G_CALLBACK (update_style_scheme_selection),
                           self->flow_box,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (adw_style_manager_get_default (),
                           "notify::dark",
                           G_CALLBACK (update_style_schemes),
                           self->flow_box,
                           G_CONNECT_SWAPPED);

  /* Ensure we've queried style-scheme-name once */
  g_variant_unref (g_settings_get_value (self->settings, "style-scheme-name"));

  /* Setup Drag-n-Drop onto the selector to install schemes */
  drop_target = gtk_drop_target_new (GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
  g_signal_connect_object (drop_target,
                           "drop",
                           G_CALLBACK (gbp_editorui_scheme_selector_drop_scheme_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));
}
