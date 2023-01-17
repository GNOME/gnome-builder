/* ide-file-preview.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-file-preview"

#include "config.h"

#include <gtksourceview/gtksource.h>

#include <libide-gui.h>
#include <libide-io.h>
#include <libide-sourceview.h>

#include "ide-application-private.h"
#include "ide-source-view-private.h"

#include "ide-file-preview.h"

struct _IdeFilePreview
{
  IdeSearchPreview parent_instance;

  GFile *file;
  GtkCssProvider *css_provider;

  GtkSourceView *view;
  GtkSourceBuffer *buffer;
};

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeFilePreview, ide_file_preview, IDE_TYPE_SEARCH_PREVIEW)

static GParamSpec *properties [N_PROPS];

static void
file_progress_cb (goffset  current_num_bytes,
                  goffset  total_num_bytes,
                  gpointer user_data)
{
  IdeFilePreview *self = user_data;
  double progress;

  g_assert (IDE_IS_FILE_PREVIEW (self));

  if (total_num_bytes == 0)
    progress = 1.;
  else
    progress = (double)current_num_bytes / (double)total_num_bytes;

  ide_search_preview_set_progress (IDE_SEARCH_PREVIEW (self), progress);
}

static void
ide_file_preview_load_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GtkSourceFileLoader *loader = (GtkSourceFileLoader *)object;
  g_autoptr(IdeFilePreview) self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_SOURCE_IS_FILE_LOADER (loader));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_FILE_PREVIEW (self));

  if (gtk_source_file_loader_load_finish (loader, result, NULL))
    {
      g_autofree char *name = g_file_get_basename (self->file);
      GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();
      GtkSourceLanguage *l = gtk_source_language_manager_guess_language (lm, name, NULL);

      gtk_source_buffer_set_language (self->buffer, l);
      gtk_source_buffer_set_highlight_syntax (self->buffer, TRUE);
    }
}

static void
ide_file_preview_load (IdeFilePreview *self)
{
  GtkSourceStyleSchemeManager *schemes;
  GtkSourceStyleScheme *scheme;
  g_autoptr(GtkSourceFileLoader) loader = NULL;
  g_autoptr(GtkSourceFile) file = NULL;
  const char *style_scheme_name;
  g_autofree char *path = NULL;
  g_autofree char *title = NULL;
  g_autofree char *subtitle = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_FILE_PREVIEW (self));
  g_assert (G_IS_FILE (self->file));

  /* TODO: This needs to update when changed */
  style_scheme_name = ide_application_get_style_scheme (IDE_APPLICATION_DEFAULT);

  schemes = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (schemes, style_scheme_name);

  gtk_source_buffer_set_style_scheme (self->buffer, scheme);

  path = g_file_get_path (self->file);
  title = g_path_get_basename (path);

  if (g_file_is_native (self->file))
    {
      g_autofree char *dn = g_path_get_dirname (path);
      subtitle = ide_path_collapse (dn);
    }
  else
    {
      g_autoptr(GFile) parent = g_file_get_parent (self->file);
      subtitle = g_file_get_uri (parent);
    }

  ide_search_preview_set_title (IDE_SEARCH_PREVIEW (self), title);
  ide_search_preview_set_subtitle (IDE_SEARCH_PREVIEW (self), subtitle);

  file = gtk_source_file_new ();
  gtk_source_file_set_location (file, self->file);

  loader = gtk_source_file_loader_new (self->buffer, file);
  gtk_source_file_loader_load_async (loader,
                                     G_PRIORITY_DEFAULT,
                                     NULL,
                                     file_progress_cb,
                                     g_object_ref (self),
                                     g_object_unref,
                                     ide_file_preview_load_cb,
                                     g_object_ref (self));
}

static void
ide_file_preview_settings_changed_cb (IdeFilePreview *self,
                                      const char     *key,
                                      GSettings      *settings)
{
  gboolean update_css = FALSE;

  g_assert (IDE_IS_FILE_PREVIEW (self));
  g_assert (G_IS_SETTINGS (settings));

  if (!key || ide_str_equal0 (key, "show-grid-lines"))
    gtk_source_view_set_background_pattern (self->view,
                                            g_settings_get_boolean (settings, "show-grid-lines") ?
                                              GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID :
                                              GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);

  if (!key || ide_str_equal0 (key, "highlight-current-line"))
    gtk_source_view_set_highlight_current_line (self->view,
                                                g_settings_get_boolean (settings, "highlight-current-line"));

  if (!key || ide_str_equal0 (key, "highlight-matching-brackets"))
    gtk_source_buffer_set_highlight_matching_brackets (self->buffer,
                                                       g_settings_get_boolean (settings, "highlight-matching-brackets"));

  if (!key || ide_str_equal0 (key, "show-line-numbers"))
    gtk_source_view_set_show_line_numbers (self->view,
                                           g_settings_get_boolean (settings, "show-line-numbers"));

  if (!key || ide_str_equal0 (key, "line-height"))
    update_css = TRUE;

  if (!key || ide_str_equal0 (key, "font-name"))
    update_css = TRUE;

  if (update_css)
    {
      g_autofree char *css = NULL;
      g_autofree char *font_name = NULL;
      PangoFontDescription *font_desc;
      double line_height;

      line_height = g_settings_get_double (settings, "line-height");
      font_name = g_settings_get_string (settings, "font-name");
      font_desc = pango_font_description_from_string (font_name);

      if ((css = _ide_source_view_generate_css (self->view, font_desc, -3, line_height)))
        gtk_css_provider_load_from_data (self->css_provider, css, -1);

      g_clear_pointer (&font_desc, pango_font_description_free);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
notify_style_scheme_cb (IdeFilePreview *self,
                        GParamSpec     *pspec,
                        IdeApplication *app)
{
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;
  const char *name;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_FILE_PREVIEW (self));
  g_assert (IDE_IS_APPLICATION (app));

  name = ide_application_get_style_scheme (app);
  manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (manager, name);

  gtk_source_buffer_set_style_scheme (self->buffer, scheme);

  IDE_EXIT;
}

static void
ide_file_preview_constructed (GObject *object)
{
  IdeFilePreview *self = (IdeFilePreview *)object;

  IDE_ENTRY;

  G_OBJECT_CLASS (ide_file_preview_parent_class)->constructed (object);

  g_signal_connect_object (IDE_APPLICATION_DEFAULT,
                           "notify::style-scheme",
                           G_CALLBACK (notify_style_scheme_cb),
                           self,
                           G_CONNECT_SWAPPED);

  notify_style_scheme_cb (self, NULL, IDE_APPLICATION_DEFAULT);

  ide_file_preview_settings_changed_cb (self,
                                        NULL,
                                        IDE_APPLICATION_DEFAULT->editor_settings);

  IDE_EXIT;
}

static void
ide_file_preview_dispose (GObject *object)
{
  IdeFilePreview *self = (IdeFilePreview *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->css_provider);

  G_OBJECT_CLASS (ide_file_preview_parent_class)->dispose (object);
}

static void
ide_file_preview_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeFilePreview *self = IDE_FILE_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_preview_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeFilePreview *self = IDE_FILE_PREVIEW (object);

  switch (prop_id)
    {
    case PROP_FILE:
      if ((self->file = g_value_dup_object (value)))
        ide_file_preview_load (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_preview_class_init (IdeFilePreviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_file_preview_constructed;
  object_class->dispose = ide_file_preview_dispose;
  object_class->get_property = ide_file_preview_get_property;
  object_class->set_property = ide_file_preview_set_property;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ide-file-preview.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeFilePreview, buffer);
  gtk_widget_class_bind_template_child (widget_class, IdeFilePreview, view);
}

static void
ide_file_preview_init (IdeFilePreview *self)
{
  GSettings *editor_settings = IDE_APPLICATION_DEFAULT->editor_settings;
  static const char *keys[] = {
    "font-name",
    "highlight-current-line",
    "highlight-matching-brackets",
    "line-height",
    "show-grid-lines",
    "show-line-numbers",
  };

  self->css_provider = gtk_css_provider_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self->view)),
                                  GTK_STYLE_PROVIDER (self->css_provider),
                                  G_MAXINT);

  g_signal_connect_object (editor_settings,
                           "changed",
                           G_CALLBACK (ide_file_preview_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Fetch the key to ensure that changed::key is emitted */
  for (guint i = 0; i < G_N_ELEMENTS (keys); i++)
    g_variant_unref (g_settings_get_value (editor_settings, keys[i]));
}

IdeSearchPreview *
ide_file_preview_new (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (IDE_TYPE_FILE_PREVIEW,
                       "file", file,
                       NULL);
}
