/* ide-editor-dict-widget.c
 *
 * Copyright (C) 2016 Sébastien Lafargue <slafargue@gnome.org>
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

#include "ide-editor-dict-widget.h"
#include <gspell/gspell.h>

#include <ide.h>

struct _IdeEditorDictWidget
{
  GtkBin                parent_instance;

  GspellChecker        *checker;
  const GspellLanguage *language;
  GPtrArray            *words_array;
  GCancellable         *cancellable;

  GtkWidget            *word_entry;
  GtkWidget            *add_button;
  GtkWidget            *words_list;
  GtkLabel             *add_word_label;
  GtkLabel             *language_label;
};

G_DEFINE_TYPE (IdeEditorDictWidget, ide_editor_dict_widget, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_CHECKER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void read_line_cb (GObject *object, GAsyncResult *result, gpointer user_data);

typedef struct
{
  IdeEditorDictWidget *self;
  GFile               *file;
  GDataInputStream    *data_stream;
  GPtrArray           *ar;
} TaskState;

static void
task_state_free (gpointer data)
{
  TaskState *state = (TaskState *)data;

  g_assert (state != NULL);

  g_clear_object (&state->file);
  g_ptr_array_unref (state->ar);

  g_slice_free (TaskState, state);
}

static void
read_line_async (GTask *task)
{
  TaskState *state;

  state = g_task_get_task_data (task);
  g_data_input_stream_read_line_async (state->data_stream,
                                       g_task_get_priority (task),
                                       g_task_get_cancellable (task),
                                       read_line_cb,
                                       task);
}

static void
read_line_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  g_autoptr (GTask) task = (GTask *)user_data;
  g_autoptr(GError) error = NULL;
  TaskState *state;
  gchar *word;
  gsize len;

  if (g_task_return_error_if_cancelled (task))
    return;

  state = g_task_get_task_data (task);
  if (NULL == (word = g_data_input_stream_read_line_finish_utf8 (state->data_stream,
                                                                 result,
                                                                 &len,
                                                                 &error)))
    {
      if (error != NULL)
        {
          /* TODO: check invalid utf8 string to skip it */
          g_task_return_error (task, g_steal_pointer (&error));
        }
      else
        g_task_return_pointer (task, state->ar, (GDestroyNotify)g_ptr_array_unref);
    }
  else
    {
      if (len > 0)
        g_ptr_array_add (state->ar, word);

      read_line_async (g_steal_pointer (&task));
    }
}

static void
open_file_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  g_autoptr (GTask) task = (GTask *)user_data;
  g_autoptr(GError) error = NULL;
  GFileInputStream *stream;
  TaskState *state;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (g_task_return_error_if_cancelled (task))
    return;

  state = g_task_get_task_data (task);
  if (NULL == (stream = g_file_read_finish (state->file, result, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  state->data_stream = g_data_input_stream_new (G_INPUT_STREAM (stream));
  read_line_async (g_steal_pointer (&task));
}

static void
ide_editor_dict_widget_get_words_async (IdeEditorDictWidget *self,
                                        GAsyncReadyCallback  callback,
                                        GCancellable        *cancellable,
                                        gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *dict_filename = NULL;
  TaskState *state;
  gint priority;

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));
  g_assert (callback != NULL);

  state = g_slice_new0 (TaskState);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_editor_dict_widget_get_words_async);
  g_task_set_task_data (task, state, (GDestroyNotify)task_state_free);

  dict_filename = g_strconcat (gspell_language_get_code (self->language), ".dic", NULL);
  path = g_build_filename (g_get_user_config_dir (), "enchant", dict_filename, NULL);
  state->self = self;
  state->ar = g_ptr_array_new_with_free_func (g_free);
  state->file = g_file_new_for_path (path);

  priority = g_task_get_priority (task);
  g_file_read_async (state->file,
                     priority,
                     cancellable,
                     open_file_cb,
                     g_steal_pointer (&task));
}

static GPtrArray *
ide_editor_dict_widget_get_words_finish (IdeEditorDictWidget  *self,
                                         GAsyncResult         *result,
                                         GError              **error)
{
  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));
  g_assert (g_task_is_valid (result, self));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
close_button_clicked_cb (IdeEditorDictWidget *self,
                         GtkButton           *button)
{
  GtkWidget *row;
  gchar *word;

  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  if (NULL != (row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW)))
    {
      word = g_object_get_data (G_OBJECT (row), "word");
      gspell_checker_remove_word_from_personal (self->checker, word, -1);
      gtk_container_remove (GTK_CONTAINER (self->words_list), row);
    }
}

static GtkWidget *
create_word_row (IdeEditorDictWidget *self,
                 const gchar         *word)
{
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *button;

  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));
  g_assert (!ide_str_empty0 (word));

  label = g_object_new (GTK_TYPE_LABEL,
                       "label", word,
                       "halign", GTK_ALIGN_START,
                       NULL);

  button = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
  g_signal_connect_swapped (button,
                            "clicked",
                            G_CALLBACK (close_button_clicked_cb),
                            self);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);

  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  g_object_set_data_full (G_OBJECT (row), "word", g_strdup (word), g_free);

  gtk_widget_show_all (row);

  return row;
}

static void
clean_listbox (IdeEditorDictWidget *self)
{
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (self->words_list));
  for (GList *l = children; l != NULL; l = g_list_next (l))
    gtk_widget_destroy (GTK_WIDGET (l->data));
}

static void
fill_listbox (IdeEditorDictWidget *self,
              GPtrArray           *words_array)
{
  gsize len;
  const gchar *word;
  GtkWidget *item;

  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));

  clean_listbox (self);

  len = self->words_array->len;
  for (gint i = 0; i < len; ++i)
    {
      word = g_ptr_array_index (self->words_array, i);
      item = create_word_row (self, word);
      gtk_list_box_insert (GTK_LIST_BOX (self->words_list), item, -1);
    }
}

static void
ide_editor_dict_widget_get_words_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeEditorDictWidget  *self = (IdeEditorDictWidget  *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (NULL == (self->words_array = ide_editor_dict_widget_get_words_finish (self, result, &error)))
    {
      printf ("error: %s\n", error->message);
      return;
    }

  fill_listbox (self, self->words_array);
  g_clear_pointer (&self->words_array, g_ptr_array_unref);
}

static void
add_dict_set_sensitivity (IdeEditorDictWidget *self,
                          gboolean             sensitivity)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), sensitivity);
  gtk_widget_set_sensitive (GTK_WIDGET (self->words_list), sensitivity);
}

static void
language_notify_cb (IdeEditorDictWidget *self,
                    GParamSpec          *pspec,
                    GspellChecker       *checker)
{
  const GspellLanguage *language;
  GCancellable *cancellable;

  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));
  g_assert (GSPELL_IS_CHECKER (checker));

  language = gspell_checker_get_language (self->checker);

  if ((self->language == NULL && language != NULL) ||
      (self->language != NULL && language == NULL) ||
      0 != gspell_language_compare (language, self->language))
    {
      self->language = language;
      g_clear_pointer (&self->words_array, g_ptr_array_unref);

      if (language == NULL)
        {
          clean_listbox (self);
          return;
        }

      add_dict_set_sensitivity (self, TRUE);
      cancellable = g_cancellable_new ();
      ide_editor_dict_widget_get_words_async (self,
                                              ide_editor_dict_widget_get_words_cb,
                                              cancellable,
                                              NULL);
    }
  else
    add_dict_set_sensitivity (self, FALSE);
}

static void
word_entry_text_notify_cb (IdeEditorDictWidget *self,
                           GParamSpec          *pspec,
                           GtkEntry            *word_entry)
{
  const gchar *word;

  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));
  g_assert (GTK_IS_ENTRY (word_entry));

  word = gtk_entry_get_text (GTK_ENTRY (self->word_entry));
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), !ide_str_empty0 (word));
}

static void
checker_weak_ref_cb (gpointer data,
                     GObject *where_the_object_was)
{
  IdeEditorDictWidget *self = (IdeEditorDictWidget *)data;

  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));

  g_clear_pointer (&self->words_array, g_ptr_array_unref);
  clean_listbox (self);
  self->checker = NULL;
  self->language = NULL;
}

GspellChecker *
ide_editor_dict_widget_get_checker (IdeEditorDictWidget *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_DICT_WIDGET (self), NULL);

  return self->checker;
}

gint
ide_editor_dict_get_label_max_width (IdeEditorDictWidget *self)
{
  gint add_word_label_width;

  g_return_val_if_fail (IDE_IS_EDITOR_DICT_WIDGET (self), 0);

  gtk_widget_get_preferred_width (GTK_WIDGET (self->add_word_label), NULL, &add_word_label_width);

  return add_word_label_width;
}

void
ide_editor_dict_set_label_width (IdeEditorDictWidget *self,
                                 gint                 width)
{
  g_return_if_fail (IDE_IS_EDITOR_DICT_WIDGET (self));

  gtk_widget_set_size_request (GTK_WIDGET (self->add_word_label), width, -1);
  /* The other labels are in a grid column soo their size will be automatically set */
}

void
ide_editor_dict_widget_set_checker (IdeEditorDictWidget *self,
                                    GspellChecker       *checker)
{
  g_return_if_fail (IDE_IS_EDITOR_DICT_WIDGET (self));

  if (self->checker != checker)
    {
      if (self->checker != NULL)
        g_object_weak_unref (G_OBJECT (self->checker), checker_weak_ref_cb, self);

      if (checker == NULL)
        {
          checker_weak_ref_cb (self, NULL);
          return;
        }

      self->checker = checker;
      g_object_weak_ref (G_OBJECT (self->checker), checker_weak_ref_cb, self);
      g_signal_connect_object (self->checker,
                               "notify::language",
                               G_CALLBACK (language_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);

      language_notify_cb (self, NULL, self->checker);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHECKER]);
    }
}

static gboolean
check_dict_available (IdeEditorDictWidget *self)
{
  return (self->checker != NULL && self->language != NULL);
}

static void
add_button_clicked_cb (IdeEditorDictWidget *self,
                       GtkButton           *button)
{
  const gchar *word;
  GtkWidget *item;

  g_assert (IDE_IS_EDITOR_DICT_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  word = gtk_entry_get_text (GTK_ENTRY (self->word_entry));
  /* TODO: check if word already in dict */
  if (check_dict_available (self) && !ide_str_empty0 (word))
    {
      item = create_word_row (self, word);
      gtk_list_box_insert (GTK_LIST_BOX (self->words_list), item, 0);
      gspell_checker_add_word_to_personal (self->checker, word, -1);

      gtk_entry_set_text (GTK_ENTRY (self->word_entry), "");
      gtk_widget_grab_focus (self->word_entry);
    }
}

IdeEditorDictWidget *
ide_editor_dict_widget_new (GspellChecker *checker)
{
  return g_object_new (IDE_TYPE_EDITOR_DICT_WIDGET,
                       "checker", checker,
                       NULL);
}

static void
ide_editor_dict_widget_finalize (GObject *object)
{
  IdeEditorDictWidget *self = (IdeEditorDictWidget *)object;

  if (self->words_array != NULL)
    g_ptr_array_unref (self->words_array);

  G_OBJECT_CLASS (ide_editor_dict_widget_parent_class)->finalize (object);
}

static void
ide_editor_dict_widget_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeEditorDictWidget *self = IDE_EDITOR_DICT_WIDGET (object);

  switch (prop_id)
    {
    case PROP_CHECKER:
      g_value_set_object (value, ide_editor_dict_widget_get_checker (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_dict_widget_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeEditorDictWidget *self = IDE_EDITOR_DICT_WIDGET (object);

  switch (prop_id)
    {
    case PROP_CHECKER:
      ide_editor_dict_widget_set_checker (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_dict_widget_class_init (IdeEditorDictWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_editor_dict_widget_finalize;
  object_class->get_property = ide_editor_dict_widget_get_property;
  object_class->set_property = ide_editor_dict_widget_set_property;

  properties [PROP_CHECKER] =
   g_param_spec_object ("checker",
                        "Checker",
                        "Checker",
                        GSPELL_TYPE_CHECKER,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-dict-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorDictWidget, add_word_label);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorDictWidget, word_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorDictWidget, add_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorDictWidget, words_list);
}

static void
ide_editor_dict_widget_init (IdeEditorDictWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (self->add_button,
                            "clicked",
                            G_CALLBACK (add_button_clicked_cb),
                            self);

  g_signal_connect_swapped (self->word_entry,
                            "notify::text",
                            G_CALLBACK (word_entry_text_notify_cb),
                            self);
}
