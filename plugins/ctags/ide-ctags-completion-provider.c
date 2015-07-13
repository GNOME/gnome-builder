/* ide-ctags-completion-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-ctags-completion-provider"

#include <glib/gi18n.h>

#include "ide-completion-provider.h"
#include "ide-context.h"
#include "ide-ctags-completion-item.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-service.h"
#include "ide-debug.h"
#include "ide-macros.h"

struct _IdeCtagsCompletionProvider
{
  IdeObject      parent_instance;

  GSettings     *settings;
  GPtrArray     *indexes;
  GHashTable    *icons;

  gint           minimum_word_size;
};

static void provider_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCtagsCompletionProvider,
                                ide_ctags_completion_provider,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, provider_iface_init)
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

void
ide_ctags_completion_provider_add_index (IdeCtagsCompletionProvider *self,
                                         IdeCtagsIndex              *index)
{
  GFile *file;
  gsize i;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_return_if_fail (!index || IDE_IS_CTAGS_INDEX (index));
  g_return_if_fail (self->indexes != NULL);

  file = ide_ctags_index_get_file (index);

  for (i = 0; i < self->indexes->len; i++)
    {
      IdeCtagsIndex *item = g_ptr_array_index (self->indexes, i);
      GFile *item_file = ide_ctags_index_get_file (item);

      if (g_file_equal (item_file, file))
        {
          g_ptr_array_remove_index_fast (self->indexes, i);
          g_ptr_array_add (self->indexes, g_object_ref (index));

          IDE_EXIT;
        }
    }

  g_ptr_array_add (self->indexes, g_object_ref (index));

  IDE_EXIT;
}

static void
theme_changed_cb (IdeCtagsCompletionProvider *self,
                  GParamSpec                 *pspec,
                  GtkSettings                *settings)
{
  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (self->icons != NULL);

  g_hash_table_remove_all (self->icons);
}

static void
ide_ctags_completion_provider_constructed (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;
  IdeContext *context;
  IdeCtagsService *service;

  G_OBJECT_CLASS (ide_ctags_completion_provider_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_context_get_service_typed (context, IDE_TYPE_CTAGS_SERVICE);
  ide_ctags_service_register_completion (service, self);
}

static void
ide_ctags_completion_provider_dispose (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;
  IdeContext *context;
  IdeCtagsService *service;

  if ((context = ide_object_get_context (IDE_OBJECT (self))) &&
      (service = ide_context_get_service_typed (context, IDE_TYPE_CTAGS_SERVICE)))
    ide_ctags_service_unregister_completion (service, self);

  G_OBJECT_CLASS (ide_ctags_completion_provider_parent_class)->dispose (object);
}

static void
ide_ctags_completion_provider_finalize (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;

  g_clear_pointer (&self->indexes, g_ptr_array_unref);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_ctags_completion_provider_parent_class)->finalize (object);
}

static void
ide_ctags_completion_provider_class_init (IdeCtagsCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_ctags_completion_provider_constructed;
  object_class->dispose = ide_ctags_completion_provider_dispose;
  object_class->finalize = ide_ctags_completion_provider_finalize;
}

static void
ide_ctags_completion_provider_class_finalize (IdeCtagsCompletionProviderClass *klass)
{
}

static void
ide_ctags_completion_provider_init (IdeCtagsCompletionProvider *self)
{
  GtkSettings *settings;

  self->minimum_word_size = 3;
  self->indexes = g_ptr_array_new_with_free_func (g_object_unref);
  self->settings = g_settings_new ("org.gnome.builder.code-insight");
  self->icons = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  settings = gtk_settings_get_default ();

  g_signal_connect_object (settings,
                           "notify::gtk-theme-name",
                           G_CALLBACK (theme_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (settings,
                           "notify::gtk-application-prefer-dark-theme",
                           G_CALLBACK (theme_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static GdkPixbuf *
load_pixbuf (IdeCtagsCompletionProvider *self,
             GtkSourceCompletionContext *context,
             const gchar                *icon_name,
             guint                       size)
{
  GtkSourceCompletion *completion = NULL;
  GtkSourceCompletionInfo *window;
  GtkStyleContext *style_context;
  GtkIconTheme *icon_theme;
  GtkIconInfo *icon_info;
  GdkPixbuf *ret = NULL;
  gboolean was_symbolic;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  g_object_get (context, "completion", &completion, NULL);
  window = gtk_source_completion_get_info_window (completion);
  style_context = gtk_widget_get_style_context (GTK_WIDGET (window));
  icon_theme = gtk_icon_theme_get_default ();
  icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, size, 0);
  if (icon_info != NULL)
    ret = gtk_icon_info_load_symbolic_for_context (icon_info, style_context, &was_symbolic, NULL);
  g_clear_object (&completion);
  g_clear_object (&icon_info);
  if (ret != NULL)
    g_hash_table_insert (self->icons, g_strdup (icon_name), ret);

  return ret;
}

static GdkPixbuf *
get_pixbuf (IdeCtagsCompletionProvider *self,
            GtkSourceCompletionContext *context,
            const IdeCtagsIndexEntry   *entry)
{
  const gchar *icon_name = NULL;
  GdkPixbuf *pixbuf;

  switch (entry->kind)
    {
    case IDE_CTAGS_INDEX_ENTRY_CLASS_NAME:
      icon_name = "lang-clang-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATOR:
      icon_name = "lang-enum-value-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATION_NAME:
      icon_name = "lang-enum-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_PROTOTYPE:
    case IDE_CTAGS_INDEX_ENTRY_FUNCTION:
      icon_name = "lang-function-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_FILE_NAME:
      icon_name = "text-x-generic-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_MEMBER:
      icon_name = "lang-struct-field-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_UNION:
      icon_name = "lang-union-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_TYPEDEF:
      icon_name = "lang-typedef-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_STRUCTURE:
      icon_name = "lang-struct-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_VARIABLE:
      icon_name = "lang-variable-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_DEFINE:
      icon_name = "lang-define-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_ANCHOR:
    default:
      return NULL;
    }

  pixbuf = g_hash_table_lookup (self->icons, icon_name);
  if (!pixbuf)
    pixbuf = load_pixbuf (self, context, icon_name, 16);

  return pixbuf;
}

static gchar *
ide_ctags_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup (_("CTags"));
}

static inline gboolean
is_symbol_char (gunichar ch)
{
  switch (ch)
    {
    case '_':
      return TRUE;

    default:
      return g_unichar_isalnum (ch);
    }
}

static gchar *
get_word_to_cursor (const GtkTextIter *location)
{
  GtkTextIter iter = *location;
  GtkTextIter end = *location;

  if (!gtk_text_iter_backward_char (&end))
    return NULL;

  while (gtk_text_iter_backward_char (&iter))
    {
      gunichar ch;

      ch = gtk_text_iter_get_char (&iter);

      if (!is_symbol_char (ch))
        break;
    }

  if (!is_symbol_char (gtk_text_iter_get_char (&iter)))
    gtk_text_iter_forward_char (&iter);

  if (gtk_text_iter_compare (&iter, &end) >= 0)
    return NULL;

  return gtk_text_iter_get_slice (&iter, location);
}

static gint
sort_wrapper (gconstpointer a,
              gconstpointer b)
{
  IdeCtagsIndexEntry * const *enta = a;
  IdeCtagsIndexEntry * const *entb = b;

  return ide_ctags_index_entry_compare (*enta, *entb);
}

static const gchar * const *
get_allowed_suffixes (GtkSourceBuffer *buffer)
{
  static const gchar *c_languages[] = { ".c", ".h",
                                        ".cc", ".hh",
                                        ".cpp", ".hpp",
                                        ".cxx", ".hxx",
                                        NULL };
  static const gchar *vala_languages[] = { ".vala", NULL };
  static const gchar *python_languages[] = { ".py", NULL };
  static const gchar *js_languages[] = { ".js", NULL };
  static const gchar *html_languages[] = { ".html",
                                           ".htm",
                                           ".tmpl",
                                           ".css",
                                           ".js",
                                           NULL };
  GtkSourceLanguage *language;
  const gchar *lang_id;

  language = gtk_source_buffer_get_language (buffer);
  if (!language)
    return NULL;

  lang_id = gtk_source_language_get_id (language);

  /*
   * NOTE:
   *
   * This seems like the type of thing that should be provided as a property
   * to the ctags provider. However, I'm trying to only have one provider
   * in process for now, so we hard code things here.
   *
   * If we decide to load multiple providers (that all sync with the ctags
   * service), then we can put this in IdeLanguage:get_completion_providers()
   * vfunc overrides.
   */

  if (ide_str_equal0 (lang_id, "c") || ide_str_equal0 (lang_id, "chdr") || ide_str_equal0 (lang_id, "cpp"))
    return c_languages;
  else if (ide_str_equal0 (lang_id, "vala"))
    return vala_languages;
  else if (ide_str_equal0 (lang_id, "python"))
    return python_languages;
  else if (ide_str_equal0 (lang_id, "js"))
    return js_languages;
  else if (ide_str_equal0 (lang_id, "html"))
    return html_languages;
  else
    return NULL;
}

static gboolean
is_allowed (const IdeCtagsIndexEntry *entry,
            const gchar * const      *allowed)
{
  if (allowed)
    {
      const gchar *dotptr = strrchr (entry->path, '.');
      gsize i;

      for (i = 0; allowed [i]; i++)
        if (ide_str_equal0 (dotptr, allowed [i]))
          return TRUE;
    }

  return FALSE;
}

static inline gboolean
too_similar (const IdeCtagsIndexEntry *a,
             const IdeCtagsIndexEntry *b)
{
  if (a->kind == b->kind)
    {
      if (ide_str_equal0 (a->name, b->name))
        return TRUE;
    }

  return FALSE;
}

static void
ide_ctags_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)provider;
  g_autofree gchar *word = NULL;
  const IdeCtagsIndexEntry *entries;
  const gchar * const *allowed;
  g_autoptr(GPtrArray) ar = NULL;
  IdeCtagsIndexEntry *last = NULL;
  GtkSourceBuffer *buffer;
  gsize n_entries;
  GtkTextIter iter;
  GList *list = NULL;
  gsize i;
  gsize j;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (self->indexes->len == 0)
    IDE_GOTO (failure);

  if (!g_settings_get_boolean (self->settings, "ctags-autocompletion"))
    IDE_GOTO (failure);

  if (!gtk_source_completion_context_get_iter (context, &iter))
    IDE_GOTO (failure);

  buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (&iter));
  allowed = get_allowed_suffixes (buffer);

  word = get_word_to_cursor (&iter);
  if (ide_str_empty0 (word) || strlen (word) < self->minimum_word_size)
    IDE_GOTO (failure);

  if (strlen (word) < 3)
    IDE_GOTO (failure);

  ar = g_ptr_array_new ();

  IDE_TRACE_MSG ("Searching for %s", word);

  for (j = 0; j < self->indexes->len; j++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (self->indexes, j);

      entries = ide_ctags_index_lookup_prefix (index, word, &n_entries);
      if ((entries == NULL) || (n_entries == 0))
        continue;

      for (i = 0; i < n_entries; i++)
        {
          const IdeCtagsIndexEntry *entry = &entries [i];

          if (is_allowed (entry, allowed))
            g_ptr_array_add (ar, (gpointer)entry);
        }
    }

  g_ptr_array_sort (ar, sort_wrapper);

  for (i = ar->len; i > 0; i--)
    {
      GtkSourceCompletionProposal *item;
      IdeCtagsIndexEntry *entry = g_ptr_array_index (ar, i - 1);

      /*
       * NOTE:
       *
       * We walk backwards in this ptrarray so that we can use g_list_prepend() for O(1) access.
       * I think everyone agrees that using GList for passing completion data around was not
       * a great choice, but it is what we have to work with.
       */

      /*
       * Ignore this item if the previous one looks really similar.
       * We take the first item instead of the last since the first item (when walking backwards)
       * tends to be more likely to be the one we care about (based on lexicographical
       * ordering. For example, something in "gtk-2.0" is less useful than "gtk-3.0".
       *
       * This is done here instead of during our initial object creation so that
       * we can merge items between different indexes. It often happens that the
       * same headers are included in multiple tags files.
       */
      if ((last != NULL) && too_similar (entry, last))
        continue;

      /*
       * NOTE:
       *
       * Autocompletion is very performance sensitive code. The smallest amount of
       * extra work has a very negative impact on interactivity. We are trying to
       * avoid a couple things here based on how completion works.
       *
       * 1) Avoiding referencing or copying things.
       *    Since the provider will always outlive the completion item, we use
       *    borrowed references for as much as we can.
       * 2) We delay the work of looking up icons until they are requested.
       *    No sense in doing that work before hand.
       */
      item = ide_ctags_completion_item_new (entry, self, context);
      list = g_list_prepend (list, item);

      last = entry;
    }

failure:
  gtk_source_completion_context_add_proposals (context, provider, list, TRUE);
  g_list_free_full (list, g_object_unref);

  IDE_EXIT;
}

GdkPixbuf *
ide_ctags_completion_provider_get_proposal_icon (IdeCtagsCompletionProvider *self,
                                                 GtkSourceCompletionContext *context,
                                                 const IdeCtagsIndexEntry   *entry)
{
  g_return_val_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (self), NULL);

  return get_pixbuf (self, context, entry);
}

static gint
ide_ctags_completion_provider_get_priority (GtkSourceCompletionProvider *provider)
{
  return IDE_CTAGS_COMPLETION_PROVIDER_PRIORITY;
}

static void
provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->get_name = ide_ctags_completion_provider_get_name;
  iface->populate = ide_ctags_completion_provider_populate;
  iface->get_priority = ide_ctags_completion_provider_get_priority;
}

void
_ide_ctags_completion_provider_register_type (GTypeModule *module)
{
  ide_ctags_completion_provider_register_type (module);
}
