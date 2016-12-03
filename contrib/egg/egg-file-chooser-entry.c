/* egg-file-chooser-entry.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "egg-file-chooser-entry"

#include <glib/gi18n.h>

#include "egg-file-chooser-entry.h"

typedef struct
{
  GtkEntry  *entry;
  GtkButton *button;

  GtkFileChooserDialog *dialog;
  GtkFileFilter *filter;
  GFile *file;

  GtkFileChooserAction action;

  guint local_only : 1;
  guint create_folders : 1;
  guint do_overwrite_confirmation : 1;
  guint select_multiple : 1;
  guint show_hidden : 1;
} EggFileChooserEntryPrivate;

enum {
  PROP_0,
  PROP_ACTION,
  PROP_CREATE_FOLDERS,
  PROP_DO_OVERWRITE_CONFIRMATION,
  PROP_FILE,
  PROP_FILTER,
  PROP_LOCAL_ONLY,
  PROP_SHOW_HIDDEN,
  PROP_MAX_WIDTH_CHARS,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

G_DEFINE_TYPE_EXTENDED (EggFileChooserEntry,
                        egg_file_chooser_entry,
                        GTK_TYPE_BIN,
                        0,
                        G_ADD_PRIVATE (EggFileChooserEntry))

static void
egg_file_chooser_entry_sync_to_dialog (EggFileChooserEntry *self)
{
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);
  GtkWidget *toplevel;
  GtkWidget *default_widget;

  g_assert (EGG_IS_FILE_CHOOSER_ENTRY (self));

  if (priv->dialog == NULL)
    return;

  g_object_set (priv->dialog,
                "action", priv->action,
                "create-folders", priv->create_folders,
                "do-overwrite-confirmation", priv->do_overwrite_confirmation,
                "local-only", priv->local_only,
                "show-hidden", priv->show_hidden,
                "filter", priv->filter,
                NULL);

  if (priv->file != NULL)
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (priv->dialog), priv->file, NULL);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

  if (GTK_IS_WINDOW (toplevel))
    gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), GTK_WINDOW (toplevel));

  default_widget = gtk_dialog_get_widget_for_response (GTK_DIALOG (priv->dialog),
                                                       GTK_RESPONSE_OK);

  switch (priv->action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      gtk_button_set_label (GTK_BUTTON (default_widget), _("Open"));
      break;

    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      gtk_button_set_label (GTK_BUTTON (default_widget), _("Select"));
      break;

    case GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER:
      gtk_button_set_label (GTK_BUTTON (default_widget), _("Create"));
      break;

    case GTK_FILE_CHOOSER_ACTION_SAVE:
      gtk_button_set_label (GTK_BUTTON (default_widget), _("Save"));
      break;

    default:
      break;
    }
}

static gboolean
egg_file_chooser_entry_dialog_delete_event (EggFileChooserEntry  *self,
                                            GdkEvent             *event,
                                            GtkFileChooserDialog *dialog)
{
  g_assert (EGG_IS_FILE_CHOOSER_ENTRY (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return GDK_EVENT_PROPAGATE;

  gtk_widget_hide (GTK_WIDGET (dialog));

  return GDK_EVENT_STOP;
}

static void
egg_file_chooser_entry_dialog_response (EggFileChooserEntry  *self,
                                        gint                  response_id,
                                        GtkFileChooserDialog *dialog)
{
  g_autoptr(GFile) file = NULL;

  g_assert (EGG_IS_FILE_CHOOSER_ENTRY (self));
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  if (response_id == GTK_RESPONSE_CANCEL)
    {
      gtk_widget_hide (GTK_WIDGET (dialog));
      return;
    }

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

  if (file != NULL)
    egg_file_chooser_entry_set_file (self, file);

  gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
egg_file_chooser_entry_button_clicked (EggFileChooserEntry *self,
                                       GtkButton           *button)
{
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);

  g_assert (EGG_IS_FILE_CHOOSER_ENTRY (self));
  g_assert (GTK_IS_BUTTON (button));

  egg_file_chooser_entry_sync_to_dialog (self);

  if (priv->dialog != NULL)
    gtk_window_present (GTK_WINDOW (priv->dialog));
}

static GFile *
file_expand (const gchar *path)
{
  g_autofree gchar *relative = NULL;
  g_autofree gchar *scheme = NULL;

  if (path == NULL)
    return g_file_new_for_path (g_get_home_dir ());

  scheme = g_uri_parse_scheme (path);
  if (scheme != NULL)
    return g_file_new_for_uri (path);

  if (g_path_is_absolute (path))
    return g_file_new_for_path (path);

  relative = g_build_filename (g_get_home_dir (),
                               path[0] == '~' ? &path[1] : path,
                               NULL);

  return g_file_new_for_path (relative);
}

static void
egg_file_chooser_entry_changed (EggFileChooserEntry *self,
                                GtkEntry            *entry)
{
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);
  g_autoptr(GFile) file = NULL;

  g_assert (EGG_IS_FILE_CHOOSER_ENTRY (self));
  g_assert (GTK_IS_ENTRY (entry));

  file = file_expand (gtk_entry_get_text (entry));
  g_set_object (&priv->file, file);
}

static void
egg_file_chooser_entry_destroy (GtkWidget *widget)
{
  EggFileChooserEntry *self = (EggFileChooserEntry *)widget;
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);

  if (priv->dialog != NULL)
    gtk_widget_destroy (GTK_WIDGET (priv->dialog));

  GTK_WIDGET_CLASS (egg_file_chooser_entry_parent_class)->destroy (widget);
}

static void
egg_file_chooser_entry_finalize (GObject *object)
{
  EggFileChooserEntry *self = (EggFileChooserEntry *)object;
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);

  g_clear_object (&priv->file);
  g_clear_object (&priv->filter);

  G_OBJECT_CLASS (egg_file_chooser_entry_parent_class)->finalize (object);
}

static void
egg_file_chooser_entry_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  EggFileChooserEntry *self = EGG_FILE_CHOOSER_ENTRY (object);
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ACTION:
      g_value_set_enum (value, priv->action);
      break;

    case PROP_LOCAL_ONLY:
      g_value_set_boolean (value, priv->local_only);
      break;

    case PROP_CREATE_FOLDERS:
      g_value_set_boolean (value, priv->create_folders);
      break;

    case PROP_DO_OVERWRITE_CONFIRMATION:
      g_value_set_boolean (value, priv->do_overwrite_confirmation);
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, priv->show_hidden);
      break;

    case PROP_FILTER:
      g_value_set_object (value, priv->filter);
      break;

    case PROP_FILE:
      g_value_take_object (value, egg_file_chooser_entry_get_file (self));
      break;

    case PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, gtk_entry_get_max_width_chars (priv->entry));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_window_get_title (GTK_WINDOW (priv->dialog)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_file_chooser_entry_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EggFileChooserEntry *self = EGG_FILE_CHOOSER_ENTRY (object);
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ACTION:
      priv->action = g_value_get_enum (value);
      break;

    case PROP_LOCAL_ONLY:
      priv->local_only = g_value_get_boolean (value);
      break;

    case PROP_CREATE_FOLDERS:
      priv->create_folders= g_value_get_boolean (value);
      break;

    case PROP_DO_OVERWRITE_CONFIRMATION:
      priv->do_overwrite_confirmation = g_value_get_boolean (value);
      break;

    case PROP_SHOW_HIDDEN:
      priv->show_hidden = g_value_get_boolean (value);
      break;

    case PROP_FILTER:
      g_clear_object (&priv->filter);
      priv->filter = g_value_dup_object (value);
      break;

    case PROP_FILE:
      egg_file_chooser_entry_set_file (self, g_value_get_object (value));
      break;

    case PROP_MAX_WIDTH_CHARS:
      gtk_entry_set_max_width_chars (priv->entry, g_value_get_int (value));
      break;

    case PROP_TITLE:
      gtk_window_set_title (GTK_WINDOW (priv->dialog), g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }

  egg_file_chooser_entry_sync_to_dialog (self);
}

static void
egg_file_chooser_entry_class_init (EggFileChooserEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = egg_file_chooser_entry_finalize;
  object_class->get_property = egg_file_chooser_entry_get_property;
  object_class->set_property = egg_file_chooser_entry_set_property;

  widget_class->destroy = egg_file_chooser_entry_destroy;

  properties [PROP_ACTION] =
    g_param_spec_enum ("action",
                       NULL,
                       NULL,
                       GTK_TYPE_FILE_CHOOSER_ACTION,
                       GTK_FILE_CHOOSER_ACTION_OPEN,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CREATE_FOLDERS] =
    g_param_spec_boolean ("create-folders",
                          NULL,
                          NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DO_OVERWRITE_CONFIRMATION] =
    g_param_spec_boolean ("do-overwrite-confirmation",
                          NULL,
                          NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LOCAL_ONLY] =
    g_param_spec_boolean ("local-only",
                          NULL,
                          NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_HIDDEN] =
    g_param_spec_boolean ("show-hidden",
                          NULL,
                          NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILTER] =
    g_param_spec_object ("filter",
                         NULL,
                         NULL,
                         GTK_TYPE_FILE_FILTER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         NULL,
                         NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MAX_WIDTH_CHARS] =
    g_param_spec_int ("max-width-chars",
                      NULL,
                      NULL,
                      -1,
                      G_MAXINT,
                      -1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
egg_file_chooser_entry_init (EggFileChooserEntry *self)
{
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);
  GtkWidget *hbox;

  hbox = g_object_new (GTK_TYPE_BOX,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "visible", TRUE,
                       NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (hbox), "linked");
  gtk_container_add (GTK_CONTAINER (self), hbox);

  priv->entry = g_object_new (GTK_TYPE_ENTRY,
                              "visible", TRUE,
                              NULL);
  g_signal_connect (priv->entry,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &priv->entry);
  g_signal_connect_object (priv->entry,
                           "changed",
                           G_CALLBACK (egg_file_chooser_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_container_add_with_properties (GTK_CONTAINER (hbox), GTK_WIDGET (priv->entry),
                                     "expand", TRUE,
                                     NULL);

  priv->button = g_object_new (GTK_TYPE_BUTTON,
                               "label", _("Browse…"),
                               "visible", TRUE,
                               NULL);
  g_signal_connect_object (priv->button,
                           "clicked",
                           G_CALLBACK (egg_file_chooser_entry_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect (priv->button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &priv->button);
  gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (priv->button));

  priv->dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                               "local-only", TRUE,
                               "modal", TRUE,
                               NULL);
  g_signal_connect_object (priv->dialog,
                           "delete-event",
                           G_CALLBACK (egg_file_chooser_entry_dialog_delete_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->dialog,
                           "response",
                           G_CALLBACK (egg_file_chooser_entry_dialog_response),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect (priv->dialog,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &priv->dialog);
  gtk_dialog_add_buttons (GTK_DIALOG (priv->dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Open"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (priv->dialog), GTK_RESPONSE_OK);
}

static gchar *
file_collapse (GFile *file)
{
  gchar *path = NULL;

  g_assert (!file || G_IS_FILE (file));

  if (file == NULL)
    return g_strdup ("");

  if (!g_file_is_native (file))
    return g_file_get_uri (file);

  path = g_file_get_path (file);

  if (path == NULL)
    return g_strdup ("");

  if (!g_path_is_absolute (path))
    {
      g_autofree gchar *freeme = path;

      path = g_build_filename (g_get_home_dir (), freeme, NULL);
    }

  if (g_str_has_prefix (path, g_get_home_dir ()))
    {
      g_autofree gchar *freeme = path;

      path = g_build_filename ("~",
                               freeme + strlen (g_get_home_dir ()),
                               NULL);
    }

  return path;
}

/**
 * egg_file_chooser_entry_get_file:
 *
 * Returns the currently selected file or %NULL if there is no selection.
 *
 * Returns: (nullable) (transfer full): A #GFile or %NULL.
 */
GFile *
egg_file_chooser_entry_get_file (EggFileChooserEntry *self)
{
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_FILE_CHOOSER_ENTRY (self), NULL);

  return priv->file ? g_object_ref (priv->file) : NULL;
}

void
egg_file_chooser_entry_set_file (EggFileChooserEntry *self,
                                 GFile               *file)
{
  EggFileChooserEntryPrivate *priv = egg_file_chooser_entry_get_instance_private (self);
  g_autofree gchar *collapsed = NULL;

  g_return_if_fail (EGG_IS_FILE_CHOOSER_ENTRY (self));

  if (priv->file == file || (priv->file && file && g_file_equal (priv->file, file)))
    return;

  if (file != NULL)
    g_object_ref (file);

  g_clear_object (&priv->file);
  priv->file = file;

  collapsed = file_collapse (file);
  gtk_entry_set_text (priv->entry, collapsed);
}
