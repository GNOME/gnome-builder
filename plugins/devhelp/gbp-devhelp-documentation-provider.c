/* gbp-devhelp-documentation-provider.c
 *
 * Copyright (C) 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#define G_LOG_DOMAIN "devhelp-documentation-provider"

#include <ide.h>
#include <devhelp/devhelp.h>

#include "gbp-devhelp-documentation-provider.h"

struct _GbpDevhelpDocumentationProvider
{
  GObject                       parent_instance;

  IdeDocumentation             *documentation;
  IdeDocumentationContext       context;
  IdeDocumentationProposal     *proposal;
  DhKeywordModel               *keyword_model;
  gchar                        *uri;

};

static void gbp_devhelp_documentation_provider_interface_init (IdeDocumentationProviderInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpDevhelpDocumentationProvider,
                        gbp_devhelp_documentation_provider,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_DOCUMENTATION_PROVIDER,
                                               gbp_devhelp_documentation_provider_interface_init))

enum
{
  START_HEADER,
  END_HEADER,
  END_TEXT,

  REMOVE_TAG_HEADER,
  REMOVE_TAG_TEXT,
  REMOVE_MULTI_SPACES,
  NEW_LINE,
  NEW_PARAGRAPH,
  MAKE_BOLD_START,
  MAKE_BOLD_END,
  MAKE_BOLD_START_NEW_LINE,
  MAKE_BOLD_END_NEW_LINE,
  MAKE_POINT_NEW_LINE,
  INFORMAL_EXAMPLE,
  INFORMAL_EXAMPLE_END,
  CLEAN_UP,

  N_REGEXES
};

static GRegex *regexes [N_REGEXES];

static gchar*
regex_replace_line (GRegex      *regex,
                    gchar       *line,
                    const gchar *replace)
{
  gchar *tmp_line;

  tmp_line = g_regex_replace (regex, line, -1, 0, replace, 0, NULL);
  g_free (line);
  return tmp_line;
}

static gboolean
xml_parse (GbpDevhelpDocumentationProvider *self,
           gchar                           *file_name,
           gchar                           *func_name,
           IdeDocumentationInfo            *info)
{
  GFileInputStream *file_stream;
  GDataInputStream *data_stream;
  g_autoptr(GString) header = g_string_new (NULL);
  g_autoptr(GString) text = g_string_new (NULL);
  g_autoptr(GError) error_file = NULL;
  g_autoptr(GError) error_stream = NULL;
  GRegex *start_text;

  const gchar *regex_char;
  gboolean informal_example_bool = FALSE;
  gboolean found_tag = FALSE;
  gboolean text_tag = FALSE;

  file_stream = g_file_read (g_file_new_for_uri (file_name), NULL, &error_file);
  if (file_stream == NULL)
    return FALSE;

  regex_char = g_strdup_printf ("name=\"%s\"", func_name);
  start_text = g_regex_new (regex_char, 0, 0, NULL);

  data_stream = g_data_input_stream_new (G_INPUT_STREAM (file_stream));

  while (TRUE)
    {
      g_autofree gchar *line = NULL;
      line = g_data_input_stream_read_line_utf8 (data_stream, NULL, NULL, &error_stream);
      if (line == NULL)
        return FALSE;
      if (!found_tag)
        {
          if (g_regex_match (start_text, line, 0, NULL))
            found_tag = TRUE;
        }
      if (found_tag && !text_tag)
        {
          line = regex_replace_line (regexes[START_HEADER], line, "<tt>");
          line = regex_replace_line (regexes[REMOVE_TAG_HEADER], line, "");
          line = regex_replace_line (regexes[MAKE_BOLD_START], line, "<b>");
          line = regex_replace_line (regexes[MAKE_BOLD_END], line, "</b>");
          line = regex_replace_line (regexes[NEW_LINE], line, "\n");

          if (g_regex_match (regexes[REMOVE_MULTI_SPACES], line, 0, NULL))
            continue;

          if (g_regex_match (regexes[END_HEADER], line, 0, NULL))
            {
              line = regex_replace_line (regexes[END_HEADER], line, "</tt>");
              g_string_append (header, line);
              text_tag = TRUE;
              continue;
            }
          line = regex_replace_line (regexes[CLEAN_UP], line, "\n");
          g_string_append_printf (header, "%s\n", line);
        }
      if (text_tag)
        {
          if (g_regex_match (regexes[INFORMAL_EXAMPLE], line, 0, NULL))
            {
              informal_example_bool = TRUE;
              continue;
            }
          if (g_regex_match (regexes[INFORMAL_EXAMPLE_END], line, 0, NULL))
            {
              informal_example_bool = FALSE;
              continue;
            }
          line = regex_replace_line (regexes[NEW_PARAGRAPH], line, "\t");
          line = regex_replace_line (regexes[REMOVE_TAG_TEXT], line, "");
          line = regex_replace_line (regexes[MAKE_BOLD_START], line, "<b>");
          line = regex_replace_line (regexes[MAKE_BOLD_END], line, "</b>");
          line = regex_replace_line (regexes[MAKE_BOLD_START_NEW_LINE], line, "\n<b>");
          line = regex_replace_line (regexes[MAKE_BOLD_END_NEW_LINE], line, "</b>\n");
          line = regex_replace_line (regexes[MAKE_POINT_NEW_LINE], line, " - ");

          if (g_regex_match (regexes[REMOVE_MULTI_SPACES], line, 0, NULL))
            continue;

          line = regex_replace_line (regexes[NEW_LINE], line, "\n");

          if (g_regex_match (regexes[END_TEXT], line, 0, NULL))
            break;

          line = regex_replace_line (regexes[CLEAN_UP], line, "\n");

          if (informal_example_bool)
            g_string_append_printf (text, "\n<tt>%s</tt>", line);
          else
            g_string_append_printf (text, "%s ", line);
        }
    }

  self->proposal = ide_documentation_proposal_new (self->uri);
  ide_documentation_proposal_set_header (self->proposal, header->str);
  ide_documentation_proposal_set_text (self->proposal, text->str);

  g_string_free (g_steal_pointer (&header), TRUE);
  g_string_free (g_steal_pointer (&text), TRUE);

  return TRUE;
}

static gboolean
get_devhelp_book (GbpDevhelpDocumentationProvider *self,
                  IdeDocumentationInfo            *info)
{
  DhLink *link;

  if (ide_documentation_info_get_input (info) == NULL)
    return FALSE;

  link = dh_keyword_model_filter (self->keyword_model, ide_documentation_info_get_input (info), NULL, NULL);
  if (link == NULL)
    return FALSE;

  g_free (self->uri);
  self->uri = dh_link_get_uri (link);

  return TRUE;
}

gchar *
gbp_devhelp_documentation_provider_get_name (IdeDocumentationProvider *provider)
{
  return g_strdup ("Devhelp");
}

gboolean
start_get_info (IdeDocumentationProvider *provider,
                IdeDocumentationInfo     *info)
{
  GbpDevhelpDocumentationProvider *self = (GbpDevhelpDocumentationProvider *)provider;
  g_auto(GStrv) tokens = NULL;

  g_assert (GBP_IS_DEVHELP_DOCUMENTATION_PROVIDER (self));

  if (!get_devhelp_book (self, info))
    return FALSE;

  tokens = g_strsplit (self->uri, "#", -1 );
  g_clear_pointer (&self->uri, g_free);

  if (tokens == NULL || g_strv_length (tokens) != 2)
    return FALSE;

  return xml_parse (self, tokens[0], tokens[1], info);
}

void
gbp_devhelp_documentation_provider_get_info (IdeDocumentationProvider *provider,
                                             IdeDocumentationInfo     *info)
{
  GbpDevhelpDocumentationProvider *self = (GbpDevhelpDocumentationProvider *)provider;
  gboolean parse_succ;

  g_assert (GBP_IS_DEVHELP_DOCUMENTATION_PROVIDER (self));

  parse_succ = start_get_info (provider, info);

  if (parse_succ)
    {
      if (G_UNLIKELY (self->proposal == NULL))
          return;

      ide_documentation_info_take_proposal (info, g_steal_pointer (&self->proposal));
    }
}

IdeDocumentationContext
gbp_devhelp_documentation_provider_get_context (IdeDocumentationProvider *provider)
{
  GbpDevhelpDocumentationProvider *self = (GbpDevhelpDocumentationProvider *)provider;

  return self->context;
}

static void
gbp_devhelp_documentation_provider_constructed (GObject *object)
{
  GbpDevhelpDocumentationProvider *self = (GbpDevhelpDocumentationProvider *)object;
  IdeContext *context;

  context = ide_object_get_context (IDE_OBJECT (self));
  self->documentation = ide_context_get_documentation (context);
  self->keyword_model = dh_keyword_model_new ();
  self->context = IDE_DOCUMENTATION_CONTEXT_CARD_C;
}

static void
gbp_devhelp_documentation_provider_finalize (GObject *object)
{
  GbpDevhelpDocumentationProvider *self = (GbpDevhelpDocumentationProvider *)object;

  g_clear_object (&self->keyword_model);

  G_OBJECT_CLASS (gbp_devhelp_documentation_provider_parent_class)->finalize (object);
}

static void
gbp_devhelp_documentation_provider_class_init (GbpDevhelpDocumentationProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_devhelp_documentation_provider_finalize;
  object_class->constructed = gbp_devhelp_documentation_provider_constructed;

  regexes[START_HEADER] = g_regex_new (".*<pre.*?>", 0, 0, NULL);
  regexes[END_HEADER] = g_regex_new ("</pre.*", 0, 0, NULL);
  regexes[END_TEXT] = g_regex_new ("<hr>", 0, 0, NULL);

  regexes[REMOVE_TAG_HEADER] = g_regex_new ("<p.*?>|</?[ace].*?>|</?ta.*?>|<h3.*/h3>", 0, 0, NULL);
  regexes[REMOVE_TAG_TEXT] = g_regex_new ("<p.*?>|</?[cdelsu].*?>|</?t[dba].*?>|</?ac.*?>|</?pre.*?>|\\s*</?td.*?>", 0, 0, NULL);
  regexes[REMOVE_MULTI_SPACES] = g_regex_new ("^\\s*$|^[\\d|\\s]*$", 0, 0, NULL);
  regexes[NEW_LINE] = g_regex_new ("</tr>|</p>", 0, 0, NULL);
  regexes[NEW_PARAGRAPH] = g_regex_new ("</p></td>", 0, 0, NULL);
  regexes[MAKE_BOLD_START] = g_regex_new ("<a.*?>|<span.*?>", 0, 0, NULL);
  regexes[MAKE_BOLD_END] = g_regex_new ("</a>|</span>", 0, 0, NULL);
  regexes[MAKE_BOLD_START_NEW_LINE] = g_regex_new ("<h4.*?>", 0, 0, NULL);
  regexes[MAKE_BOLD_END_NEW_LINE] = g_regex_new ("</h4>", 0, 0, NULL);
  regexes[MAKE_POINT_NEW_LINE] = g_regex_new ("<li.*?>|<tr>", 0, 0, NULL);
  regexes[INFORMAL_EXAMPLE] = g_regex_new ("<div class=\"informalexample\">", 0, 0, NULL);
  regexes[INFORMAL_EXAMPLE_END] = g_regex_new ("</div>", 0, 0, NULL);
  regexes[CLEAN_UP] = g_regex_new ("</?[acdehlpsu].*?>|</?td.*?>|</?ta.*?>|</?tb.*?>", 0, 0, NULL);

#ifdef IDE_ENABLE_TRACE
  for (guint i = 0; i < N_REGEXES; i++)
    {
      if (regexes[i] == NULL)
        g_error ("Failed to create regex %d", i);
    }
#endif
}

static void
gbp_devhelp_documentation_provider_interface_init (IdeDocumentationProviderInterface *iface)
{
  iface->get_name = gbp_devhelp_documentation_provider_get_name;
  iface->get_info = gbp_devhelp_documentation_provider_get_info;
  iface->get_context = gbp_devhelp_documentation_provider_get_context;
}

static void
gbp_devhelp_documentation_provider_init (GbpDevhelpDocumentationProvider *self)
{
}
