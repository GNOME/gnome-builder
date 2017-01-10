/* ide-editor-spell-dict.c
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

/* This GObject exists until Gspell handle managing the content of a dict */

#include "ide-editor-spell-dict.h"
#include <enchant.h>
#include <gspell/gspell.h>

#include <ide.h>

struct _IdeEditorSpellDict
{
  GtkBin                parent_instance;

  GspellChecker        *checker;
  EnchantBroker        *broker;
  EnchantDict          *dict;
  const GspellLanguage *language;
};

G_DEFINE_TYPE (IdeEditorSpellDict, ide_editor_spell_dict, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CHECKER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void read_line_cb (GObject *object, GAsyncResult *result, gpointer user_data);

typedef struct
{
  IdeEditorSpellDict  *self;
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

gboolean
ide_editor_spell_dict_add_word_to_personal (IdeEditorSpellDict *self,
                                            const gchar        *word)
{
  g_assert (IDE_IS_EDITOR_SPELL_DICT (self));
  g_assert (!ide_str_empty0 (word));

 if (self->dict != NULL)
    {
      if (enchant_dict_is_added (self->dict, word, -1))
        return FALSE;

      enchant_dict_add (self->dict, word, -1);
      return TRUE;
    }
  else
    {
      g_warning ("No dictionaries loaded");
      return FALSE;
    }
}

gboolean
ide_editor_spell_dict_remove_word_from_personal (IdeEditorSpellDict *self,
                                                 const gchar        *word)
{
  g_assert (IDE_IS_EDITOR_SPELL_DICT (self));
  g_assert (!ide_str_empty0 (word));

  if (self->dict != NULL)
    {
      if (!enchant_dict_is_added (self->dict, word, -1))
        return FALSE;

      enchant_dict_remove (self->dict, word, -1);
      return TRUE;
    }
  else
    {
      g_warning ("No dictionaries loaded");
      return FALSE;
    }
}

gboolean
ide_editor_spell_dict_personal_contains (IdeEditorSpellDict *self,
                                         const gchar        *word)
{
  g_assert (IDE_IS_EDITOR_SPELL_DICT (self));
  g_assert (!ide_str_empty0 (word));

  if (self->dict != NULL)
    {
      return !!enchant_dict_is_added (self->dict, word, -1);
    }
  else
    {
      g_warning ("No dictionaries loaded");
      return FALSE;
    }
}

void
ide_editor_spell_dict_get_words_async (IdeEditorSpellDict  *self,
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
  g_assert (IDE_IS_EDITOR_SPELL_DICT (self));
  g_assert (callback != NULL);

  state = g_slice_new0 (TaskState);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_editor_spell_dict_get_words_async);
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

GPtrArray *
ide_editor_spell_dict_get_words_finish (IdeEditorSpellDict   *self,
                                        GAsyncResult         *result,
                                        GError              **error)
{
  g_assert (IDE_IS_EDITOR_SPELL_DICT (self));
  g_assert (g_task_is_valid (result, self));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_editor_spell_dict_set_dict (IdeEditorSpellDict    *self,
                                const GspellLanguage  *language)
{
  const gchar *lang_code;

  g_assert (IDE_IS_EDITOR_SPELL_DICT (self));

  if (language != NULL)
    {
      lang_code = gspell_language_get_code (language);
      self->dict = enchant_broker_request_dict (self->broker, lang_code);
    }
  else if (self->dict != NULL)
    {
      enchant_broker_free_dict (self->broker, self->dict);
      self->dict = NULL;
    }
}

static void
language_notify_cb (IdeEditorSpellDict  *self,
                    GParamSpec          *pspec,
                    GspellChecker       *checker)
{
  const GspellLanguage *language;

  g_assert (IDE_IS_EDITOR_SPELL_DICT (self));
  g_assert (GSPELL_IS_CHECKER (checker));

  language = gspell_checker_get_language (self->checker);

  if ((self->language == NULL && language != NULL) ||
      (self->language != NULL && language == NULL) ||
      0 != gspell_language_compare (language, self->language))
    {
      self->language = language;
      ide_editor_spell_dict_set_dict (self, language);
    }
}

static void
checker_weak_ref_cb (gpointer data,
                     GObject *where_the_object_was)
{
  IdeEditorSpellDict *self = (IdeEditorSpellDict *)data;

  g_assert (IDE_IS_EDITOR_SPELL_DICT (self));

  self->checker = NULL;
  self->language = NULL;
}

GspellChecker *
ide_editor_spell_dict_get_checker (IdeEditorSpellDict *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SPELL_DICT (self), NULL);

  return self->checker;
}

void
ide_editor_spell_dict_set_checker (IdeEditorSpellDict  *self,
                                   GspellChecker       *checker)
{
  g_return_if_fail (IDE_IS_EDITOR_SPELL_DICT (self));

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

IdeEditorSpellDict *
ide_editor_spell_dict_new (GspellChecker *checker)
{
  return g_object_new (IDE_TYPE_EDITOR_SPELL_DICT,
                       "checker", checker,
                       NULL);
}

static void
ide_editor_spell_dict_finalize (GObject *object)
{
  IdeEditorSpellDict *self = (IdeEditorSpellDict *)object;

  if (self->broker != NULL)
    {
      if (self->dict != NULL)
        enchant_broker_free_dict (self->broker, self->dict);

      enchant_broker_free (self->broker);
    }
}

static void
ide_editor_spell_dict_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeEditorSpellDict *self = IDE_EDITOR_SPELL_DICT (object);

  switch (prop_id)
    {
    case PROP_CHECKER:
      g_value_set_object (value, ide_editor_spell_dict_get_checker (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_spell_dict_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeEditorSpellDict *self = IDE_EDITOR_SPELL_DICT (object);

  switch (prop_id)
    {
    case PROP_CHECKER:
      ide_editor_spell_dict_set_checker (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_spell_dict_class_init (IdeEditorSpellDictClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_editor_spell_dict_finalize;
  object_class->get_property = ide_editor_spell_dict_get_property;
  object_class->set_property = ide_editor_spell_dict_set_property;

  properties [PROP_CHECKER] =
   g_param_spec_object ("checker",
                        "Checker",
                        "Checker",
                        GSPELL_TYPE_CHECKER,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_editor_spell_dict_init (IdeEditorSpellDict *self)
{
  self->broker = enchant_broker_init ();
}
