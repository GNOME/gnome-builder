/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright 2008 Richard Hughes <richard@hughsie.com>
 * Copyright 2015 Kalev Lember <klember@redhat.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <string.h>
#include <glib.h>

#include "gs-markdown-private.h"

/*******************************************************************************
 *
 * This is a simple Markdown parser.
 * It can output to Pango, HTML or plain text. The following limitations are
 * already known, and properly deliberate:
 *
 * - No code section support
 * - No ordered list support
 * - No blockquote section support
 * - No image support
 * - No links or email support
 * - No backslash escapes support
 * - No HTML escaping support
 * - Auto-escapes certain word patterns, like http://
 *
 * It does support the rest of the standard pretty well, although it's not
 * been run against any conformance tests. The parsing is single pass, with
 * a simple enumerated interpretor mode and a single line back-memory.
 *
 *
 * Since: 3.32
 ******************************************************************************/

typedef enum {
	GS_MARKDOWN_MODE_BLANK,
	GS_MARKDOWN_MODE_RULE,
	GS_MARKDOWN_MODE_BULLETT,
	GS_MARKDOWN_MODE_PARA,
	GS_MARKDOWN_MODE_H1,
	GS_MARKDOWN_MODE_H2,
	GS_MARKDOWN_MODE_UNKNOWN
} GsMarkdownMode;

typedef struct {
	const gchar *em_start;
	const gchar *em_end;
	const gchar *strong_start;
	const gchar *strong_end;
	const gchar *code_start;
	const gchar *code_end;
	const gchar *h1_start;
	const gchar *h1_end;
	const gchar *h2_start;
	const gchar *h2_end;
	const gchar *bullet_start;
	const gchar *bullet_end;
	const gchar *rule;
} GsMarkdownTags;

struct _GsMarkdown {
	GObject			 parent_instance;

	GsMarkdownMode		 mode;
	GsMarkdownTags		 tags;
	GsMarkdownOutputKind	 output;
	gint			 max_lines;
	gint			 line_count;
	gboolean		 smart_quoting;
	gboolean		 escape;
	gboolean		 autocode;
	gboolean		 autolinkify;
	GString			*pending;
	GString			*processed;
};

G_DEFINE_TYPE (GsMarkdown, gs_markdown, G_TYPE_OBJECT)

/*
 * gs_markdown_to_text_line_is_rule:
 *
 * Horizontal rules are created by placing three or more hyphens, asterisks,
 * or underscores on a line by themselves.
 * You may use spaces between the hyphens or asterisks.
 **/
static gboolean
gs_markdown_to_text_line_is_rule (const gchar *line)
{
	guint i;
	guint len;
	guint count = 0;
	g_autofree gchar *copy = NULL;

	len = (guint) strlen (line);
	if (len == 0)
		return FALSE;

	/* replace non-rule chars with ~ */
	copy = g_strdup (line);
	g_strcanon (copy, "-*_ ", '~');
	for (i = 0; i < len; i++) {
		if (copy[i] == '~')
			return FALSE;
		if (copy[i] != ' ')
			count++;
	}

	/* if we matched, return true */
	if (count >= 3)
		return TRUE;
	return FALSE;
}

static gboolean
gs_markdown_to_text_line_is_bullet (const gchar *line)
{
	return (g_str_has_prefix (line, "- ") ||
		g_str_has_prefix (line, "* ") ||
		g_str_has_prefix (line, "+ ") ||
		g_str_has_prefix (line, " - ") ||
		g_str_has_prefix (line, " * ") ||
		g_str_has_prefix (line, " + "));
}

static gboolean
gs_markdown_to_text_line_is_header1 (const gchar *line)
{
	return g_str_has_prefix (line, "# ");
}

static gboolean
gs_markdown_to_text_line_is_header2 (const gchar *line)
{
	return g_str_has_prefix (line, "## ");
}

static gboolean
gs_markdown_to_text_line_is_header1_type2 (const gchar *line)
{
	return g_str_has_prefix (line, "===");
}

static gboolean
gs_markdown_to_text_line_is_header2_type2 (const gchar *line)
{
	return g_str_has_prefix (line, "---");
}

#if 0
static gboolean
gs_markdown_to_text_line_is_code (const gchar *line)
{
	return (g_str_has_prefix (line, "    ") ||
		g_str_has_prefix (line, "\t"));
}

static gboolean
gs_markdown_to_text_line_is_blockquote (const gchar *line)
{
	return (g_str_has_prefix (line, "> "));
}
#endif

static gboolean
gs_markdown_to_text_line_is_blank (const gchar *line)
{
	guint i;
	guint len;

	/* a line with no characters is blank by definition */
	len = (guint) strlen (line);
	if (len == 0)
		return TRUE;

	/* find if there are only space chars */
	for (i = 0; i < len; i++) {
		if (line[i] != ' ' && line[i] != '\t')
			return FALSE;
	}

	/* if we matched, return true */
	return TRUE;
}

static gchar *
gs_markdown_replace (const gchar *haystack,
		     const gchar *needle,
		     const gchar *replace)
{
	g_auto(GStrv) split = NULL;
	split = g_strsplit (haystack, needle, -1);
	return g_strjoinv (replace, split);
}

static gchar *
gs_markdown_strstr_spaces (const gchar *haystack, const gchar *needle)
{
	gchar *found;
	const gchar *haystack_new = haystack;

retry:
	/* don't find if surrounded by spaces */
	found = strstr (haystack_new, needle);
	if (found == NULL)
		return NULL;

	/* start of the string, always valid */
	if (found == haystack)
		return found;

	/* end of the string, always valid */
	if ((*(found-1) == ' ' && *(found+1) == ' ') ||
	    (*(found-1) != ' ' && *(found+1) != ' ')) {
		haystack_new = found+1;
		goto retry;
	}
	return found;
}

static gchar *
gs_markdown_to_text_line_formatter (const gchar *line,
				    const gchar *formatter,
				    const gchar *left,
				    const gchar *right)
{
	guint len;
	gchar *str1;
	gchar *str2;
	gchar *start = NULL;
	gchar *middle = NULL;
	gchar *end = NULL;
	g_autofree gchar *copy = NULL;

	/* needed to know for shifts */
	len = (guint) strlen (formatter);
	if (len == 0)
		return NULL;

	/* find sections */
	copy = g_strdup (line);
	str1 = gs_markdown_strstr_spaces (copy, formatter);
	if (str1 != NULL) {
		*str1 = '\0';
		str2 = gs_markdown_strstr_spaces (str1+len, formatter);
		if (str2 != NULL) {
			*str2 = '\0';
			middle = str1 + len;
			start = copy;
			end = str2 + len;
		}
	}

	/* if we found, replace and keep looking for the same string */
	if (start != NULL && middle != NULL && end != NULL) {
		g_autofree gchar *temp = NULL;
		temp = g_strdup_printf ("%s%s%s%s%s", start, left, middle, right, end);
		/* recursive */
		return gs_markdown_to_text_line_formatter (temp, formatter, left, right);
	}

	/* not found, keep return as-is */
	return g_strdup (line);
}

static gchar *
gs_markdown_to_text_line_format_sections (GsMarkdown *self, const gchar *line)
{
	gchar *data = g_strdup (line);
	gchar *temp;

	/* bold1 */
	temp = data;
	data = gs_markdown_to_text_line_formatter (temp, "**",
						   self->tags.strong_start,
						   self->tags.strong_end);
	g_free (temp);

	/* bold2 */
	temp = data;
	data = gs_markdown_to_text_line_formatter (temp, "__",
						   self->tags.strong_start,
						   self->tags.strong_end);
	g_free (temp);

	/* italic1 */
	temp = data;
	data = gs_markdown_to_text_line_formatter (temp, "*",
						   self->tags.em_start,
						   self->tags.em_end);
	g_free (temp);

	/* italic2 */
	temp = data;
	data = gs_markdown_to_text_line_formatter (temp, "_",
						   self->tags.em_start,
						   self->tags.em_end);
	g_free (temp);

	/* em-dash */
	temp = data;
	data = gs_markdown_replace (temp, " -- ", " — ");
	g_free (temp);

	/* smart quoting */
	if (self->smart_quoting) {
		temp = data;
		data = gs_markdown_to_text_line_formatter (temp, "\"", "“", "”");
		g_free (temp);

		temp = data;
		data = gs_markdown_to_text_line_formatter (temp, "'", "‘", "’");
		g_free (temp);
	}

	return data;
}

static gchar *
gs_markdown_to_text_line_format (GsMarkdown *self, const gchar *line)
{
	GString *string;
	gboolean mode = FALSE;
	gchar *text;
	guint i;
	g_auto(GStrv) codes = NULL;

	/* optimise the trivial case where we don't have any code tags */
	text = strstr (line, "`");
	if (text == NULL)
		return gs_markdown_to_text_line_format_sections (self, line);

	/* we want to parse the code sections without formatting */
	codes = g_strsplit (line, "`", -1);
	string = g_string_new ("");
	for (i = 0; codes[i] != NULL; i++) {
		if (!mode) {
			text = gs_markdown_to_text_line_format_sections (self, codes[i]);
			g_string_append (string, text);
			g_free (text);
			mode = TRUE;
		} else {
			/* just append without formatting */
			g_string_append (string, self->tags.code_start);
			g_string_append (string, codes[i]);
			g_string_append (string, self->tags.code_end);
			mode = FALSE;
		}
	}
	return g_string_free (string, FALSE);
}

static gboolean
gs_markdown_add_pending (GsMarkdown *self, const gchar *line)
{
	g_autofree gchar *copy = NULL;

	/* would put us over the limit */
	if (self->max_lines > 0 && self->line_count >= self->max_lines)
		return FALSE;

	copy = g_strdup (line);

	/* strip leading and trailing spaces */
	g_strstrip (copy);

	/* append */
	g_string_append_printf (self->pending, "%s ", copy);
	return TRUE;
}

static gboolean
gs_markdown_add_pending_header (GsMarkdown *self, const gchar *line)
{
	g_autofree gchar *copy = NULL;

	/* strip trailing # */
	copy = g_strdup (line);
	g_strdelimit (copy, "#", ' ');
	return gs_markdown_add_pending (self, copy);
}

static guint
gs_markdown_count_chars_in_word (const gchar *text, gchar find)
{
	guint i;
	guint len;
	guint count = 0;

	/* get length */
	len = (guint) strlen (text);
	if (len == 0)
		return 0;

	/* find matching chars */
	for (i = 0; i < len; i++) {
		if (text[i] == find)
			count++;
	}
	return count;
}

static gboolean
gs_markdown_word_is_code (const gchar *text)
{
	/* already code */
	if (g_str_has_prefix (text, "`"))
		return FALSE;
	if (g_str_has_suffix (text, "`"))
		return FALSE;

	/* paths */
	if (g_str_has_prefix (text, "/"))
		return TRUE;

	/* bugzillas */
	if (g_str_has_prefix (text, "#"))
		return TRUE;

	/* patch files */
	if (g_strrstr (text, ".patch") != NULL)
		return TRUE;
	if (g_strrstr (text, ".diff") != NULL)
		return TRUE;

	/* function names */
	if (g_strrstr (text, "()") != NULL)
		return TRUE;

	/* email addresses */
	if (g_strrstr (text, "@") != NULL)
		return TRUE;

	/* compiler defines */
	if (text[0] != '_' &&
	    gs_markdown_count_chars_in_word (text, '_') > 1)
		return TRUE;

	/* nothing special */
	return FALSE;
}

static gchar *
gs_markdown_word_auto_format_code (const gchar *text)
{
	guint i;
	gchar *temp;
	gboolean ret = FALSE;
	g_auto(GStrv) words = NULL;

	/* split sentence up with space */
	words = g_strsplit (text, " ", -1);

	/* search each word */
	for (i = 0; words[i] != NULL; i++) {
		if (gs_markdown_word_is_code (words[i])) {
			temp = g_strdup_printf ("`%s`", words[i]);
			g_free (words[i]);
			words[i] = temp;
			ret = TRUE;
		}
	}

	/* no replacements, so just return a copy */
	if (!ret)
		return g_strdup (text);

	/* join the array back into a string */
	return g_strjoinv (" ", words);
}

static gboolean
gs_markdown_word_is_url (const gchar *text)
{
	if (g_str_has_prefix (text, "http://"))
		return TRUE;
	if (g_str_has_prefix (text, "https://"))
		return TRUE;
	if (g_str_has_prefix (text, "ftp://"))
		return TRUE;
	return FALSE;
}

static gchar *
gs_markdown_word_auto_format_urls (const gchar *text)
{
	guint i;
	gchar *temp;
	gboolean ret = FALSE;
	g_auto(GStrv) words = NULL;

	/* split sentence up with space */
	words = g_strsplit (text, " ", -1);

	/* search each word */
	for (i = 0; words[i] != NULL; i++) {
		if (gs_markdown_word_is_url (words[i])) {
			temp = g_strdup_printf ("<a href=\"%s\">%s</a>",
						words[i], words[i]);
			g_free (words[i]);
			words[i] = temp;
			ret = TRUE;
		}
	}

	/* no replacements, so just return a copy */
	if (!ret)
		return g_strdup (text);

	/* join the array back into a string */
	return g_strjoinv (" ", words);
}

static void
gs_markdown_flush_pending (GsMarkdown *self)
{
	g_autofree gchar *copy = NULL;
	g_autofree gchar *temp = NULL;

	/* no data yet */
	if (self->mode == GS_MARKDOWN_MODE_UNKNOWN)
		return;

	/* remove trailing spaces */
	while (g_str_has_suffix (self->pending->str, " "))
		g_string_set_size (self->pending, self->pending->len - 1);

	/* pango requires escaping */
	copy = g_strdup (self->pending->str);
	if (!self->escape && self->output == GS_MARKDOWN_OUTPUT_PANGO) {
		g_strdelimit (copy, "<", '(');
		g_strdelimit (copy, ">", ')');
		g_strdelimit (copy, "&", '+');
	}

	/* check words for code */
	if (self->autocode &&
	    (self->mode == GS_MARKDOWN_MODE_PARA ||
	     self->mode == GS_MARKDOWN_MODE_BULLETT)) {
		temp = gs_markdown_word_auto_format_code (copy);
		g_free (copy);
		copy = temp;
	}

	/* escape */
	if (self->escape) {
		temp = g_markup_escape_text (copy, -1);
		g_free (copy);
		copy = temp;
	}

	/* check words for URLS */
	if (self->autolinkify &&
	    self->output == GS_MARKDOWN_OUTPUT_PANGO &&
	    (self->mode == GS_MARKDOWN_MODE_PARA ||
	     self->mode == GS_MARKDOWN_MODE_BULLETT)) {
		temp = gs_markdown_word_auto_format_urls (copy);
		g_free (copy);
		copy = temp;
	}

	/* do formatting */
	temp = gs_markdown_to_text_line_format (self, copy);
	if (self->mode == GS_MARKDOWN_MODE_BULLETT) {
		g_string_append_printf (self->processed, "%s%s%s\n",
					self->tags.bullet_start,
					temp,
					self->tags.bullet_end);
		self->line_count++;
	} else if (self->mode == GS_MARKDOWN_MODE_H1) {
		g_string_append_printf (self->processed, "%s%s%s\n",
					self->tags.h1_start,
					temp,
					self->tags.h1_end);
	} else if (self->mode == GS_MARKDOWN_MODE_H2) {
		g_string_append_printf (self->processed, "%s%s%s\n",
					self->tags.h2_start,
					temp,
					self->tags.h2_end);
	} else if (self->mode == GS_MARKDOWN_MODE_PARA ||
		   self->mode == GS_MARKDOWN_MODE_RULE) {
		g_string_append_printf (self->processed, "%s\n", temp);
		self->line_count++;
	}

	/* clear */
	g_string_truncate (self->pending, 0);
}

static gboolean
gs_markdown_to_text_line_process (GsMarkdown *self, const gchar *line)
{
	gboolean ret;

	/* blank */
	ret = gs_markdown_to_text_line_is_blank (line);
	if (ret) {
		gs_markdown_flush_pending (self);
		/* a new line after a list is the end of list, not a gap */
		if (self->mode != GS_MARKDOWN_MODE_BULLETT)
			ret = gs_markdown_add_pending (self, "\n");
		self->mode = GS_MARKDOWN_MODE_BLANK;
		goto out;
	}

	/* header1_type2 */
	ret = gs_markdown_to_text_line_is_header1_type2 (line);
	if (ret) {
		if (self->mode == GS_MARKDOWN_MODE_PARA)
			self->mode = GS_MARKDOWN_MODE_H1;
		goto out;
	}

	/* header2_type2 */
	ret = gs_markdown_to_text_line_is_header2_type2 (line);
	if (ret) {
		if (self->mode == GS_MARKDOWN_MODE_PARA)
			self->mode = GS_MARKDOWN_MODE_H2;
		goto out;
	}

	/* rule */
	ret = gs_markdown_to_text_line_is_rule (line);
	if (ret) {
		gs_markdown_flush_pending (self);
		self->mode = GS_MARKDOWN_MODE_RULE;
		ret = gs_markdown_add_pending (self, self->tags.rule);
		goto out;
	}

	/* bullet */
	ret = gs_markdown_to_text_line_is_bullet (line);
	if (ret) {
		gs_markdown_flush_pending (self);
		self->mode = GS_MARKDOWN_MODE_BULLETT;
		ret = gs_markdown_add_pending (self, &line[2]);
		goto out;
	}

	/* header1 */
	ret = gs_markdown_to_text_line_is_header1 (line);
	if (ret) {
		gs_markdown_flush_pending (self);
		self->mode = GS_MARKDOWN_MODE_H1;
		ret = gs_markdown_add_pending_header (self, &line[2]);
		goto out;
	}

	/* header2 */
	ret = gs_markdown_to_text_line_is_header2 (line);
	if (ret) {
		gs_markdown_flush_pending (self);
		self->mode = GS_MARKDOWN_MODE_H2;
		ret = gs_markdown_add_pending_header (self, &line[3]);
		goto out;
	}

	/* paragraph */
	if (self->mode == GS_MARKDOWN_MODE_BLANK ||
	    self->mode == GS_MARKDOWN_MODE_UNKNOWN) {
		gs_markdown_flush_pending (self);
		self->mode = GS_MARKDOWN_MODE_PARA;
	}

	/* add to pending */
	ret = gs_markdown_add_pending (self, line);
out:
	/* if we failed to add, we don't know the mode */
	if (!ret)
		self->mode = GS_MARKDOWN_MODE_UNKNOWN;
	return ret;
}

static void
gs_markdown_set_output_kind (GsMarkdown *self, GsMarkdownOutputKind output)
{
	g_return_if_fail (GS_IS_MARKDOWN (self));

	self->output = output;
	switch (output) {
	case GS_MARKDOWN_OUTPUT_PANGO:
		/* PangoMarkup */
		self->tags.em_start = "<i>";
		self->tags.em_end = "</i>";
		self->tags.strong_start = "<b>";
		self->tags.strong_end = "</b>";
		self->tags.code_start = "<tt>";
		self->tags.code_end = "</tt>";
		self->tags.h1_start = "<big>";
		self->tags.h1_end = "</big>";
		self->tags.h2_start = "<b>";
		self->tags.h2_end = "</b>";
		self->tags.bullet_start = "• ";
		self->tags.bullet_end = "";
		self->tags.rule = "⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n";
		self->escape = TRUE;
		self->autolinkify = TRUE;
		break;
	case GS_MARKDOWN_OUTPUT_HTML:
		/* XHTML */
		self->tags.em_start = "<em>";
		self->tags.em_end = "<em>";
		self->tags.strong_start = "<strong>";
		self->tags.strong_end = "</strong>";
		self->tags.code_start = "<code>";
		self->tags.code_end = "</code>";
		self->tags.h1_start = "<h1>";
		self->tags.h1_end = "</h1>";
		self->tags.h2_start = "<h2>";
		self->tags.h2_end = "</h2>";
		self->tags.bullet_start = "<li>";
		self->tags.bullet_end = "</li>";
		self->tags.rule = "<hr>";
		self->escape = TRUE;
		self->autolinkify = TRUE;
		break;
	case GS_MARKDOWN_OUTPUT_TEXT:
		/* plain text */
		self->tags.em_start = "";
		self->tags.em_end = "";
		self->tags.strong_start = "";
		self->tags.strong_end = "";
		self->tags.code_start = "";
		self->tags.code_end = "";
		self->tags.h1_start = "[";
		self->tags.h1_end = "]";
		self->tags.h2_start = "-";
		self->tags.h2_end = "-";
		self->tags.bullet_start = "* ";
		self->tags.bullet_end = "";
		self->tags.rule = " ----- \n";
		self->escape = FALSE;
		self->autolinkify = FALSE;
		break;
  case GS_MARKDOWN_OUTPUT_LAST:
	default:
		g_warning ("unknown output enum");
		break;
	}
}

void
gs_markdown_set_max_lines (GsMarkdown *self, gint max_lines)
{
	g_return_if_fail (GS_IS_MARKDOWN (self));
	self->max_lines = max_lines;
}

void
gs_markdown_set_smart_quoting (GsMarkdown *self, gboolean smart_quoting)
{
	g_return_if_fail (GS_IS_MARKDOWN (self));
	self->smart_quoting = smart_quoting;
}

void
gs_markdown_set_escape (GsMarkdown *self, gboolean escape)
{
	g_return_if_fail (GS_IS_MARKDOWN (self));
	self->escape = escape;
}

void
gs_markdown_set_autocode (GsMarkdown *self, gboolean autocode)
{
	g_return_if_fail (GS_IS_MARKDOWN (self));
	self->autocode = autocode;
}

void
gs_markdown_set_autolinkify (GsMarkdown *self, gboolean autolinkify)
{
	g_return_if_fail (GS_IS_MARKDOWN (self));
	self->autolinkify = autolinkify;
}

gchar *
gs_markdown_parse (GsMarkdown *self, const gchar *markdown)
{
	gboolean ret;
	gchar *temp;
	guint i;
	guint len;
	g_auto(GStrv) lines = NULL;

	g_return_val_if_fail (GS_IS_MARKDOWN (self), NULL);

	/* process */
	self->mode = GS_MARKDOWN_MODE_UNKNOWN;
	self->line_count = 0;
	g_string_truncate (self->pending, 0);
	g_string_truncate (self->processed, 0);
	lines = g_strsplit (markdown, "\n", -1);
	len = g_strv_length (lines);

	/* process each line */
	for (i = 0; i < len; i++) {
		ret = gs_markdown_to_text_line_process (self, lines[i]);
		if (!ret)
			break;
	}
	gs_markdown_flush_pending (self);

	/* remove trailing \n */
	while (g_str_has_suffix (self->processed->str, "\n"))
		g_string_set_size (self->processed, self->processed->len - 1);

	/* get a copy */
	temp = g_strdup (self->processed->str);
	g_string_truncate (self->pending, 0);
	g_string_truncate (self->processed, 0);
	return temp;
}

static void
gs_markdown_finalize (GObject *object)
{
	GsMarkdown *self;

	g_return_if_fail (GS_IS_MARKDOWN (object));

	self = GS_MARKDOWN (object);

	g_string_free (self->pending, TRUE);
	g_string_free (self->processed, TRUE);

	G_OBJECT_CLASS (gs_markdown_parent_class)->finalize (object);
}

static void
gs_markdown_class_init (GsMarkdownClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gs_markdown_finalize;
}

static void
gs_markdown_init (GsMarkdown *self)
{
	self->mode = GS_MARKDOWN_MODE_UNKNOWN;
	self->pending = g_string_new ("");
	self->processed = g_string_new ("");
	self->max_lines = -1;
	self->smart_quoting = FALSE;
	self->escape = FALSE;
	self->autocode = FALSE;
}

GsMarkdown *
gs_markdown_new (GsMarkdownOutputKind output)
{
	GsMarkdown *self;
	self = g_object_new (GS_TYPE_MARKDOWN, NULL);
	gs_markdown_set_output_kind (self, output);
	return GS_MARKDOWN (self);
}
