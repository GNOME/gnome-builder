/* ide-tweaks-directory.c
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

#define G_LOG_DOMAIN "ide-tweaks-directory"

#include "config.h"

#include <glib/gi18n.h>

#include <adwaita.h>

#include <libide-io.h>

#include "ide-tweaks-directory.h"

struct _IdeTweaksDirectory
{
  IdeTweaksWidget parent_instance;
  char *title;
  char *subtitle;
  guint is_directory : 1;
};

G_DEFINE_FINAL_TYPE (IdeTweaksDirectory, ide_tweaks_directory, IDE_TYPE_TWEAKS_WIDGET)

enum {
  PROP_0,
  PROP_IS_DIRECTORY,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
get_path_transform (const GValue *from_value,
                    GValue       *to_value,
                    gpointer      user_data)
{
  const char *str = g_value_get_string (from_value);

  if (str && g_path_is_absolute (str))
    g_value_take_string (to_value, ide_path_collapse (str));
  else
    g_value_set_string (to_value, str);

  return TRUE;
}

static gboolean
set_path_transform (const GValue *from_value,
                    GValue       *to_value,
                    gpointer      user_data)
{
  const char *str = g_value_get_string (from_value);

  if (str && !g_path_is_absolute (str))
    g_value_take_string (to_value, ide_path_expand (str));
  else
    g_value_set_string (to_value, str);

  return TRUE;
}

static void
select_folder_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GtkFileDialog *dialog = (GtkFileDialog *)object;
  g_autoptr(IdeTweaksDirectory) info = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  GtkWidget *entry;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TWEAKS_DIRECTORY (info));

  entry = g_object_get_data (G_OBJECT (dialog), "ENTRY");

  g_assert (entry != NULL);
  g_assert (GTK_IS_WIDGET (entry));

  if (!(file = gtk_file_dialog_select_folder_finish (dialog, result, &error)))
    return;

  if (g_file_is_native (file))
    {
      g_autofree char *path = g_file_get_path (file);
      g_autofree char *collapsed = ide_path_collapse (path);

      gtk_editable_set_text (GTK_EDITABLE (entry), collapsed);
    }
}

static void
open_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
  GtkFileDialog *dialog = (GtkFileDialog *)object;
  g_autoptr(IdeTweaksDirectory) info = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  GtkWidget *entry;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TWEAKS_DIRECTORY (info));

  entry = g_object_get_data (G_OBJECT (dialog), "ENTRY");

  g_assert (entry != NULL);
  g_assert (GTK_IS_WIDGET (entry));

  if (!(file = gtk_file_dialog_open_finish (dialog, result, &error)))
    return;

  if (g_file_is_native (file))
    {
      g_autofree char *path = g_file_get_path (file);
      g_autofree char *collapsed = ide_path_collapse (path);

      gtk_editable_set_text (GTK_EDITABLE (entry), collapsed);
    }
}

static void
on_button_clicked_cb (GtkButton          *button,
                      IdeTweaksDirectory *info)
{
  g_autoptr(GtkFileDialog) dialog = NULL;
  g_autoptr(GFile) folder = NULL;
  g_autofree char *path = NULL;
  g_autofree char *expanded = NULL;
  IdeTweaksBinding *binding;
  GtkWidget *row;
  GtkRoot *root;

  g_assert (GTK_IS_BUTTON (button));
  g_assert (IDE_IS_TWEAKS_DIRECTORY (info));

  if ((binding = ide_tweaks_widget_get_binding (IDE_TWEAKS_WIDGET (info))))
    path = ide_tweaks_binding_dup_string (binding);

  if (path != NULL)
    {
      expanded = ide_path_expand (path);
      folder = g_file_new_for_path (expanded);
    }

  root = gtk_widget_get_root (GTK_WIDGET (button));
  row = gtk_widget_get_ancestor (GTK_WIDGET (button), ADW_TYPE_ENTRY_ROW);

  dialog = gtk_file_dialog_new ();
  /* FIXME? Shouldn't title come from a tweak property? */
  gtk_file_dialog_set_title (dialog, _("Projects Directory"));
  gtk_file_dialog_set_accept_label (dialog, _("Select"));
  if (folder != NULL)
    gtk_file_dialog_set_initial_file (dialog, folder);
  g_object_set_data (G_OBJECT (dialog), "ENTRY", row);

  if (info->is_directory)
    gtk_file_dialog_select_folder (dialog,
                                   GTK_WINDOW (root),
                                   NULL,
                                   select_folder_cb,
                                   g_object_ref (info));
  else
    gtk_file_dialog_open (dialog,
                          GTK_WINDOW (root),
                          NULL,
                          open_cb,
                          g_object_ref (info));
}

static GtkWidget *
ide_tweaks_directory_create_for_item (IdeTweaksWidget *widget,
                                      IdeTweaksItem   *item)
{
  IdeTweaksDirectory *info = (IdeTweaksDirectory *)item;
  IdeTweaksBinding *binding;
  AdwEntryRow *row;
  GtkListBox *list;
  GtkButton *button;
  GtkBox *box;

  g_assert (IDE_IS_TWEAKS_DIRECTORY (info));

  if (!(binding = ide_tweaks_widget_get_binding (IDE_TWEAKS_WIDGET (info))))
    return NULL;

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "spacing", 6,
                      NULL);
  button = g_object_new (GTK_TYPE_BUTTON,
                         "css-classes", IDE_STRV_INIT ("flat"),
                         "icon-name", "folder-symbolic",
                         "valign", GTK_ALIGN_CENTER,
                         NULL);
  g_signal_connect_object (button,
                           "clicked",
                           G_CALLBACK (on_button_clicked_cb),
                           info,
                           0);
  list = g_object_new (GTK_TYPE_LIST_BOX,
                       "css-classes", IDE_STRV_INIT ("boxed-list"),
                       "selection-mode", GTK_SELECTION_NONE,
                       NULL);
  row = g_object_new (ADW_TYPE_ENTRY_ROW,
                      "title", info->title,
                      NULL);
  adw_entry_row_add_suffix (row, GTK_WIDGET (button));
  gtk_box_append (box, GTK_WIDGET (list));
  gtk_list_box_append (list, GTK_WIDGET (row));

  if (info->subtitle)
    {
      GtkLabel *label;

      label = g_object_new (GTK_TYPE_LABEL,
                            "css-classes", IDE_STRV_INIT ("caption", "dim-label"),
                            "label", info->subtitle,
                            "wrap", TRUE,
                            "xalign", .0f,
                            "margin-top", 6,
                            NULL);
      gtk_box_append (box, GTK_WIDGET (label));
    }

  ide_tweaks_binding_bind_with_transform (binding,
                                          row, "text",
                                          get_path_transform,
                                          set_path_transform,
                                          NULL, NULL);

  return GTK_WIDGET (box);
}

static void
ide_tweaks_directory_dispose (GObject *object)
{
  IdeTweaksDirectory *self = (IdeTweaksDirectory *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);

  G_OBJECT_CLASS (ide_tweaks_directory_parent_class)->dispose (object);
}

static void
ide_tweaks_directory_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeTweaksDirectory *self = IDE_TWEAKS_DIRECTORY (object);

  switch (prop_id)
    {
    case PROP_IS_DIRECTORY:
      g_value_set_boolean (value, ide_tweaks_directory_get_is_directory (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_directory_get_title (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_tweaks_directory_get_subtitle (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_directory_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeTweaksDirectory *self = IDE_TWEAKS_DIRECTORY (object);

  switch (prop_id)
    {
    case PROP_IS_DIRECTORY:
      ide_tweaks_directory_set_is_directory (self, g_value_get_boolean (value));
      break;

    case PROP_TITLE:
      ide_tweaks_directory_set_title (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      ide_tweaks_directory_set_subtitle (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_directory_class_init (IdeTweaksDirectoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksWidgetClass *widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_directory_dispose;
  object_class->get_property = ide_tweaks_directory_get_property;
  object_class->set_property = ide_tweaks_directory_set_property;

  widget_class->create_for_item = ide_tweaks_directory_create_for_item;

  properties [PROP_IS_DIRECTORY] =
    g_param_spec_boolean ("is-directory", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_directory_init (IdeTweaksDirectory *self)
{
  self->is_directory = TRUE;
}

IdeTweaksDirectory *
ide_tweaks_directory_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_DIRECTORY, NULL);
}

gboolean
ide_tweaks_directory_get_is_directory (IdeTweaksDirectory *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_DIRECTORY (self), FALSE);

  return self->is_directory;
}

void
ide_tweaks_directory_set_is_directory (IdeTweaksDirectory *self,
                                       gboolean            is_directory)
{
  g_return_if_fail (IDE_IS_TWEAKS_DIRECTORY (self));

  is_directory = !!is_directory;

  if (is_directory != self->is_directory)
    {
      self->is_directory = is_directory;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_DIRECTORY]);
    }
}

const char *
ide_tweaks_directory_get_title (IdeTweaksDirectory *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_DIRECTORY (self), NULL);

  return self->title;
}

void
ide_tweaks_directory_set_title (IdeTweaksDirectory *self,
                                const char         *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_DIRECTORY (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

const char *
ide_tweaks_directory_get_subtitle (IdeTweaksDirectory *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_DIRECTORY (self), NULL);

  return self->subtitle;
}

void
ide_tweaks_directory_set_subtitle (IdeTweaksDirectory *self,
                                   const char         *subtitle)
{
  g_return_if_fail (IDE_IS_TWEAKS_DIRECTORY (self));

  if (g_set_str (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}
