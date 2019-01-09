/* gstyle-color.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "gstyle-color"

#include <math.h>

#include <dazzle.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "gstyle-private.h"
#include "gstyle-colorlexer.h"
#include "gstyle-color-convert.h"
#include "gstyle-color-item.h"
#include "gstyle-color-predefined.h"

#include "gstyle-color.h"

/**
 * SECTION: GstyleColor
 * @title: Color container.
 * @short_description: A color container.
 *
 * The purpose of GstyleColor is having a container for
 * color strings representation.
 */

/* TODO: add operations on the color: darker lighter mix...*/

#define GET_COMP(ar, i) (g_array_index(ar, GstyleColorComponent, i))
#define FMOD_360(val)   ({ double a; return ((a = x / 360.0)-(int)a)*y; })

#define GSTYLE_COLOR_FUZZY_SEARCH_MAX_LEN 20

typedef struct
{
  gdouble         value;
  GstyleColorUnit unit;
} GstyleColorComponent;

struct _GstyleColor
{
  GObject           parent_instance;

  GstyleColorKind   kind;
  gchar            *name;
  gint              name_index;
  GdkRGBA           rgba;
};

typedef enum
{
  RANGE_PERCENT,
  RANGE_PERCENT_OR_1MAX,
  RANGE_PERCENT_OR_255MAX
} ComponentRange;

G_DEFINE_TYPE (GstyleColor, gstyle_color, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_KIND,
  PROP_RGBA,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gchar TRUNCATE_BUF[6];

/**
 * gstyle_color_to_hsla:
 * @self: a #GstyleColor
 * @hue: (out): The hue component of a hsla color in range [0.0-360.0[
 * @saturation: (out): The saturation component of a hsla color in range [0.0-100.0]
 * @lightness: (out): The lightness component of a hsla color in range [0.0-100.0]
 * @alpha: (out) (nullable): The alpha component of a hsla color in range [0.0-100.0]
 *
 * Get the hsla components from a #GstyleColor.
 *
 */
void
gstyle_color_to_hsla (GstyleColor *self,
                      gdouble     *hue,
                      gdouble     *saturation,
                      gdouble     *lightness,
                      gdouble     *alpha)
{
  g_return_if_fail (GSTYLE_IS_COLOR (self));
  g_return_if_fail (hue != NULL);
  g_return_if_fail (saturation != NULL);
  g_return_if_fail (lightness != NULL);

  gstyle_color_convert_rgb_to_hsl (&self->rgba, hue, saturation, lightness);
  if (alpha != NULL)
    *alpha = self->rgba.alpha;
}

static gchar *
truncate_trailing_zeros (gdouble number)
{
  guint i = (guint)number;
  guint f = (number - i) * 100;
  gint c = g_snprintf (TRUNCATE_BUF, 5, "%d.%d", i, f);

  /* Format the number string, e.g. "0.50" => "0.5"; "1.0" => "1" */
  --c;
  while (TRUNCATE_BUF[c] == '0')
    c--;

  if (TRUNCATE_BUF[c] == '.')
    c--;

  TRUNCATE_BUF[c + 1] = '\0';
  return TRUNCATE_BUF;
}

/**
 * gstyle_color_to_string:
 * @self: a #GstyleColor
 * @kind: The kind of representation as a #GstyleColorKind
 *
 * Get the string representation of a #GstyleColor.
 *
 * Notice that:
 *  - asking for an HEX3 format take only the 4 left bits of each components into account.
 *  - asking for a predefined named color format return the closest color according to
 *    CIE2000 deltaE calculation, unless the original kind is already a named color.
 *
 * Returns: A null-terminated string.
 *
 */
gchar *
gstyle_color_to_string (GstyleColor     *self,
                        GstyleColorKind  kind)
{
  gchar *string = NULL;
  gchar *alpha_str = NULL;
  gdouble hue = 0.0;
  gdouble saturation = 0.0;
  gdouble lightness = 0.0;
  guint red = 0;
  guint green = 0;
  guint blue = 0;

  g_return_val_if_fail (GSTYLE_IS_COLOR (self), NULL);

  if (kind == GSTYLE_COLOR_KIND_ORIGINAL)
    kind = self->kind;

  switch (kind)
    {
    case GSTYLE_COLOR_KIND_RGB_HEX3:
    case GSTYLE_COLOR_KIND_RGB_HEX6:
    case GSTYLE_COLOR_KIND_RGB:
    case GSTYLE_COLOR_KIND_RGBA:
      red = (guint)(0.5 + CLAMP (self->rgba.red, 0.0, 1.0) * 255.0);
      green = (guint)(0.5 + CLAMP (self->rgba.green, 0.0, 1.0) * 255.0);
      blue = (guint)(0.5 + CLAMP (self->rgba.blue, 0.0, 1.0) * 255.0);
      break;

    case GSTYLE_COLOR_KIND_RGB_PERCENT:
    case GSTYLE_COLOR_KIND_RGBA_PERCENT:
      red = (guint)(0.5 + CLAMP (self->rgba.red, 0.0, 1.0) * 100.0);
      green = (guint)(0.5 + CLAMP (self->rgba.green, 0.0, 1.0) * 100.0);
      blue = (guint)(0.5 + CLAMP (self->rgba.blue, 0.0, 1.0) * 100.0);
      break;

    case GSTYLE_COLOR_KIND_HSL:
    case GSTYLE_COLOR_KIND_HSLA:
      gstyle_color_convert_rgb_to_hsl (&self->rgba, &hue, &saturation, &lightness);
      break;

    case GSTYLE_COLOR_KIND_PREDEFINED:
      /* TODO: get closest color using deltaE formula unless the original kind is already a named color */
      break;

    case GSTYLE_COLOR_KIND_UNKNOW:
      g_warning ("UNKNOW #GstyleColorKind is not meant to be used for color string output");
      return NULL;
      break;

    case GSTYLE_COLOR_KIND_ORIGINAL:
    default:
      g_assert_not_reached ();
    }

  /* alpha_str is a pointer to a static buffer, DO NOT free it */
  alpha_str = truncate_trailing_zeros (self->rgba.alpha);

    switch (kind)
    {
    case GSTYLE_COLOR_KIND_RGB_HEX3:
      string = g_strdup_printf ("#%01X%01X%01X", red / 16, green / 16, blue / 16);
      break;

    case GSTYLE_COLOR_KIND_RGB_HEX6:
      string = g_strdup_printf ("#%02X%02X%02X", red, green, blue);
      break;

    case GSTYLE_COLOR_KIND_RGB:
      string = g_strdup_printf ("rgb(%i, %i, %i)", red, green, blue);
      break;

    case GSTYLE_COLOR_KIND_RGBA:
      string = g_strdup_printf ("rgba(%i, %i, %i, %s)", red, green, blue, alpha_str);
      break;

    case GSTYLE_COLOR_KIND_RGB_PERCENT:
      string = g_strdup_printf ("rgb(%i%%, %i%%, %i%%)", red, green, blue);
      break;

    case GSTYLE_COLOR_KIND_RGBA_PERCENT:
      string = g_strdup_printf ("rgba(%i%%, %i%%, %i%%, %s)", red, green, blue, alpha_str);
      break;

    case GSTYLE_COLOR_KIND_HSL:
      string = g_strdup_printf ("hsl(%i, %i%%, %i%%)", (gint)hue, (gint)(saturation + 0.5), (gint)(lightness + 0.5));
      break;

    case GSTYLE_COLOR_KIND_HSLA:
      string = g_strdup_printf ("hsla(%i, %i%%, %i%%, %s)", (gint)hue, (gint)(saturation + 0.5), (gint)(lightness + 0.5), alpha_str);
      break;

    case GSTYLE_COLOR_KIND_PREDEFINED:
      if (self->name_index != -1)
        string = g_strdup (predefined_colors_table [self->name_index].name);
      else
        {
          /* TODO: search for a corresponding name in the predefined_colors_table
           * or fallback to rgba syntax
           */
          red = (guint)(0.5 + CLAMP (self->rgba.red, 0.0, 1.0) * 255.0);
          green = (guint)(0.5 + CLAMP (self->rgba.green, 0.0, 1.0) * 255.0);
          blue = (guint)(0.5 + CLAMP (self->rgba.blue, 0.0, 1.0) * 255.0);

          string = g_strdup_printf ("rgba(%i, %i, %i, %s)", red, green, blue, alpha_str);
        }
      break;

    case GSTYLE_COLOR_KIND_ORIGINAL:
    case GSTYLE_COLOR_KIND_UNKNOW:
    default:
      g_assert_not_reached ();
    }

  return string;
}

/**
 * gstyle_color_get_rgba:
 * @self: a #GstyleColor
 *
 * Get a #GdkRGBA object from a #GstyleColor.
 *
 * Returns: a #GdkRGBA.
 *
 */
GdkRGBA *
gstyle_color_get_rgba (GstyleColor *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR (self), NULL);

  return gdk_rgba_copy (&self->rgba);
}

/**
 * gstyle_color_fill_rgba:
 * @self: a #GstyleColor
 * @rgba: (out): the #GdkRGBA to fill in
 *
 * Fill a #GdkRGBA object from a #GstyleColor.
 *
 */
void
gstyle_color_fill_rgba (GstyleColor *self,
                        GdkRGBA     *rgba)
{
  g_return_if_fail (GSTYLE_IS_COLOR (self));
  g_return_if_fail (rgba != NULL);

  rgba->red = self->rgba.red;
  rgba->green = self->rgba.green;
  rgba->blue = self->rgba.blue;
  rgba->alpha = self->rgba.alpha;
}

/**
 * gstyle_color_parse:
 * @string: A null terminated string to parse.
 *
 * Get an pointer array of #GstyleColorItem indicating the color and position.
 * of color strings in the parsed string.
 *
 * Returns: (nullable) (element-type GstyleColorItem) (transfer full): A newly allocated
 *   #GPtrArray containing #GstyleColorItem elements. This should be freed when
 *   the caller is done with it using g_ptr_array_free().
 *
 */
GPtrArray *
gstyle_color_parse (const gchar *string)
{
  GPtrArray *ar;
  gint n = 0;

  g_return_val_if_fail (!gstyle_str_empty0 (string), NULL);

  /* TODO: try doing full parsing in the parser */

  /* First pass to get color strings */
  ar = gstyle_colorlexer_parse (string);
  while (n < ar->len)
    {
      g_autofree gchar *str = NULL;
      GstyleColorItem *item;
      GstyleColor *color;

      item = g_ptr_array_index (ar, n);
      str = g_strndup (string + gstyle_color_item_get_start (item),
                       gstyle_color_item_get_len (item));

      /* Second pass to get rgba and kind */
      color = gstyle_color_new_from_string (NULL, str);
      if (color != NULL)
        {
          gstyle_color_item_set_color (item, color);
          g_object_unref (color);
          ++n;
        }
      else
        g_ptr_array_remove_index (ar, n);
    }

  return ar;
}

/* TODO: support for a minus in front of values */
/* format is [0-9]*.*[0-9]* */
static gboolean
str_to_float (GstyleColorScanner *s,
              gdouble            *number)
{
  const gchar *cursor;
  guint left = 0;
  guint right = 0;
  guint precision = 0;

  cursor = s->start = s->cursor;

  if (g_ascii_isdigit (*cursor) || *cursor == '.')
    {
      while (g_ascii_isdigit (*cursor))
        {
          left = left * 10 + (*cursor - '0');
          ++cursor;
        }

      if (*cursor != '.')
        goto finish;
      else
        ++cursor;

      while (g_ascii_isdigit (*cursor))
        {
          right = right * 10 + (*cursor - '0');
          ++precision;
          ++cursor;
        }
    }
  else
    {
      s->cursor = cursor;
      return FALSE;
    }

finish:
  s->cursor = cursor;

  if ((cursor - s->start) == 1 && *(s->start) == '.')
    return FALSE;

  *number = left + (right / pow (10, precision));
  return TRUE;
}

/* parse hex_len hexadecimal digits into value */
static gboolean
get_hex_digit (const gchar *str,
               guint        hex_len,
               guint       *value)
{
  gint xdigit;
  gint num = 0;

  for (; hex_len > 0; --hex_len)
    {
      num <<= 4;

      xdigit = g_ascii_xdigit_value (*str);
      if (xdigit == -1)
        return FALSE;

      num += xdigit;
      ++str;
    }

  *value = num;
  return TRUE;
}

/* String need at least to start with '#'
 * format #[0-9A-Fa-f]{3} or #[0-9A-Fa-f]{6}
 */
static gboolean
_parse_hex_string (const gchar     *str,
                   GdkRGBA         *rgba,
                   GstyleColorKind *kind)
{
  gint len;
  guint r, g, b;

  g_return_val_if_fail (*str == '#', FALSE);

  rgba->alpha = 1.0;

  len = strlen (str);
  if (len == 4)
    {
      if (get_hex_digit (str + 1, 1, &r) &&
          get_hex_digit (str + 2, 1, &g) &&
          get_hex_digit (str + 3, 1, &b))
        {
          rgba->red = (r | r << 4) / 255.0;
          rgba->green = (g | g << 4) / 255.0;
          rgba->blue = (b | b << 4) / 255.0;

          *kind = GSTYLE_COLOR_KIND_RGB_HEX3;
          return TRUE;
        }
    }
  else if (len == 7)
    {
      if (get_hex_digit (str + 1, 2, &r) &&
          get_hex_digit (str + 3, 2, &g) &&
          get_hex_digit (str + 5, 2, &b))
        {
          rgba->red = r / 255.0;
          rgba->green = g / 255.0;
          rgba->blue = b / 255.0;

          *kind = GSTYLE_COLOR_KIND_RGB_HEX6;
          return TRUE;
        }
    }

  return FALSE;
}

static inline gboolean
check_char (GstyleColorScanner *s,
            gchar               ch)
{
  if (*(s->cursor) == ch)
    {
      ++(s->cursor);
      return TRUE;
    }

  return FALSE;
}

static inline gboolean
skip_spaces (GstyleColorScanner *s)
{
  const gchar *cursor = s->cursor;
  gunichar c;

  while (g_unichar_isspace (c = g_utf8_get_char (cursor)))
    cursor = g_utf8_next_char (cursor);

  s->cursor = cursor;
  return (c != '\0');
}

static GArray *
parse_components (GstyleColorScanner *s)
{
  GstyleColorComponent comp;
  gdouble value;
  gboolean need_more = FALSE;
  GArray *ar = g_array_sized_new (FALSE, FALSE, sizeof (GstyleColorComponent), 4);

  skip_spaces (s);
  while (str_to_float (s, &value))
    {
      need_more = FALSE;
      comp.unit = GSTYLE_COLOR_UNIT_NONE;

      if (check_char (s, '%'))
        comp.unit = GSTYLE_COLOR_UNIT_PERCENT;

      comp.value = value;
      g_array_append_val (ar, comp);

      skip_spaces (s);
      if (check_char (s, ','))
        {
          skip_spaces (s);
          need_more = TRUE;
        }
      else if (!(*s->cursor == ')'))
        {
          g_array_free (ar, TRUE);
          return NULL;
        }
    }

  if (need_more == TRUE)
    {
      g_array_free (ar, TRUE);
      return NULL;
    }
  else
    return ar;
}

static gboolean
convert_component (GstyleColorComponent  comp,
                   ComponentRange        range,
                   gdouble              *number)
{
  gdouble n = comp.value;

  if (comp.unit == GSTYLE_COLOR_UNIT_PERCENT)
    {
      *number = CLAMP (n, 0.0, 100.0) / 100.0;
    }
  else
    {
      if (range == RANGE_PERCENT)
       return FALSE;

      if (range == RANGE_PERCENT_OR_1MAX)
        *number = CLAMP (n, 0.0, 1.0);
      else if (range == RANGE_PERCENT_OR_255MAX)
        *number = CLAMP (n, 0.0, 255.0) / 255.0;
    }

  return TRUE;
}

static gboolean
convert_hue_component (GstyleColorComponent  comp,
                       gdouble              *hue)
{
  gdouble num = comp.value;

  if (comp.unit == GSTYLE_COLOR_UNIT_PERCENT)
    return FALSE;

  if (num == 360.0)
    num = 0.0;
  else if (num > 360.0)
    num = fmod (num, 360.0);
  else if (num < 0.0)
    num = fmod (num, 360.0) + 360.0;

  *hue = num;
  return TRUE;
}

/* String need at least to start with 'rgb' */
static gboolean
_parse_rgba_string (const gchar      *string,
                    GdkRGBA          *rgba,
                    GstyleColorKind  *kind)
{
  GstyleColorScanner s;
  g_autoptr (GArray) ar = NULL;
  gboolean has_alpha;
  gboolean is_percent;
  gboolean ret;

  g_return_val_if_fail (*string == 'r' && *(string + 1) == 'g' && *(string + 2) == 'b', FALSE);

  s.start = string;
  s.cursor = string + 3;

  has_alpha = check_char (&s, 'a');
  if (check_char (&s, '(') && (ar = parse_components (&s)))
    {
      is_percent = (GET_COMP (ar, 0).unit == GSTYLE_COLOR_UNIT_PERCENT);
      if ((ar->len == 3 && !has_alpha) || (ar->len == 4 && has_alpha))
        {
          ret = convert_component (GET_COMP (ar, 0), RANGE_PERCENT_OR_255MAX, &rgba->red) &&
                convert_component (GET_COMP (ar, 1), RANGE_PERCENT_OR_255MAX, &rgba->green) &&
                convert_component (GET_COMP (ar, 2), RANGE_PERCENT_OR_255MAX, &rgba->blue);

          if (has_alpha)
            {
              ret = ret && convert_component (GET_COMP (ar, 3), RANGE_PERCENT_OR_1MAX, &rgba->alpha);
              *kind = is_percent ? GSTYLE_COLOR_KIND_RGBA_PERCENT : GSTYLE_COLOR_KIND_RGBA;
            }
          else
            {
              rgba->alpha = 1.0;
              *kind = is_percent ? GSTYLE_COLOR_KIND_RGB_PERCENT : GSTYLE_COLOR_KIND_RGB;
            }

          if (ret && check_char (&s, ')'))
            return TRUE;
        }
    }

  *kind = GSTYLE_COLOR_KIND_UNKNOW;

  return FALSE;
}

/* String need at least to start with 'hsl' */
static gboolean
_parse_hsla_string (const gchar      *string,
                    GdkRGBA          *rgba,
                    GstyleColorKind  *kind)
{
  GstyleColorScanner s;
  g_autoptr (GArray) ar = NULL;
  gdouble hue;
  gdouble saturation;
  gdouble lightness;
  gboolean has_alpha = FALSE;
  gboolean ret;

  g_return_val_if_fail (*string == 'h' && *(string + 1) == 's' && *(string + 2) == 'l', FALSE);

  s.start = string;
  s.cursor = string + 3;

  has_alpha = check_char (&s, 'a');
  if (check_char (&s, '(') && (ar = parse_components (&s)))
    {
      if ((ar->len == 3 && !has_alpha) || (ar->len == 4 && has_alpha))
        {
          ret = convert_hue_component (GET_COMP (ar, 0), &hue) &&
                convert_component (GET_COMP (ar, 1), RANGE_PERCENT, &saturation) &&
                convert_component (GET_COMP (ar, 2), RANGE_PERCENT, &lightness);

          if (has_alpha)
            {
              ret = ret && convert_component (GET_COMP (ar, 3), RANGE_PERCENT_OR_1MAX, &rgba->alpha);
              *kind = GSTYLE_COLOR_KIND_HSLA;
            }
          else
            {
              rgba->alpha = 1.0;
              *kind = GSTYLE_COLOR_KIND_HSL;
            }

          if (ret && check_char (&s, ')'))
            {
              gstyle_color_convert_hsl_to_rgb (hue, saturation, lightness, rgba);
              return TRUE;
            }
        }
    }

  *kind = GSTYLE_COLOR_KIND_UNKNOW;

  return FALSE;
}

/* TODO: add a public func to init so we can control the initial starting time ? */
static DzlFuzzyMutableIndex *
_init_predefined_table (void)
{
  static DzlFuzzyMutableIndex *predefined_table;
  NamedColor *item;

  if (predefined_table == NULL)
    {
      predefined_table = dzl_fuzzy_mutable_index_new (TRUE);

      dzl_fuzzy_mutable_index_begin_bulk_insert (predefined_table);
      for (guint i = 0; i < G_N_ELEMENTS (predefined_colors_table); ++i)
        {
          item = &predefined_colors_table [i];
          item->index = i;
          dzl_fuzzy_mutable_index_insert (predefined_table, item->name, (gpointer)item);
        }

      dzl_fuzzy_mutable_index_end_bulk_insert (predefined_table);
    }

  return predefined_table;
}

static gboolean
_parse_predefined_color (const gchar  *color_string,
                         GdkRGBA      *rgba,
                         gint         *name_index)
{
  g_autoptr (GArray) results = NULL;
  NamedColor *item = NULL;
  gint len;
  DzlFuzzyMutableIndex *predefined_table = _init_predefined_table ();

  results = dzl_fuzzy_mutable_index_match (predefined_table, color_string, 10);
  len = results->len;
  for (gint i = 0; i < len; ++i)
    {
      const DzlFuzzyMutableIndexMatch *match = &g_array_index (results, DzlFuzzyMutableIndexMatch, i);

      if (g_strcmp0 (color_string, match->key) == 0)
        {
          item = match->value;
          *name_index = (gint)(item - predefined_colors_table);
          rgba->alpha = 1.0;
          rgba->red = item->red /255.0;
          rgba->green = item->green / 255.0;
          rgba->blue = item->blue / 255.0;

          return TRUE;
        }
    }

  *name_index = -1;
  return FALSE;
}

/**
 * gstyle_color_fuzzy_parse_color_string:
 * @color_string: color name to search for
 *
 * Returns: (transfer full) (element-type GstyleColor): a #GPtrArray of #GstyleColor for a fuzzy search.
 */
GPtrArray *
gstyle_color_fuzzy_parse_color_string (const gchar *color_string)
{
  g_autoptr (GArray) fuzzy_results = NULL;
  GPtrArray *results;
  NamedColor *item;
  GstyleColor *color;
  GdkRGBA rgba;
  gint len;

  DzlFuzzyMutableIndex *predefined_table = _init_predefined_table ();

  results = g_ptr_array_new_with_free_func (g_object_unref);
  fuzzy_results = dzl_fuzzy_mutable_index_match (predefined_table, color_string, GSTYLE_COLOR_FUZZY_SEARCH_MAX_LEN);
  len = MIN (GSTYLE_COLOR_FUZZY_SEARCH_MAX_LEN, fuzzy_results->len);
  for (gint i = 0; i < len; ++i)
    {
      const DzlFuzzyMutableIndexMatch *match = &g_array_index (fuzzy_results, DzlFuzzyMutableIndexMatch, i);

      item = match->value;
      rgba.red = item->red / 255.0;
      rgba.green = item->green / 255.0;
      rgba.blue = item->blue / 255.0;
      rgba.alpha = 1.0;

      color = gstyle_color_new_from_rgba (g_strdup (match->key), GSTYLE_COLOR_KIND_PREDEFINED, &rgba);
      color->name_index = item->index;

      g_ptr_array_add (results, color);
    }

  return results;
}

/* TODO: add hsv/hwb colorspace */
static gboolean
_parse_color_string (const gchar      *color_string,
                     GdkRGBA          *rgba,
                     GstyleColorKind  *kind,
                     gint             *name_index)
{
  gboolean ret;

  g_assert (!gstyle_str_empty0 (color_string));
  g_assert (rgba != NULL);

  *name_index = -1;

  if (color_string[0] == '#')
    ret = _parse_hex_string (color_string, rgba, kind);
  else if (g_str_has_prefix (color_string, "rgb"))
    ret = _parse_rgba_string (color_string, rgba, kind);
  else if (g_str_has_prefix (color_string, "hsl"))
    ret = _parse_hsla_string (color_string, rgba, kind);
  else
    {
      if ((ret = _parse_predefined_color (color_string, rgba, name_index)))
        *kind =GSTYLE_COLOR_KIND_PREDEFINED;
      else
        *kind =GSTYLE_COLOR_KIND_UNKNOW;
    }

  return ret;
}

/**
 * gstyle_color_parse_color_string:
 * @color_string: A null-terminated color string to parse
 * @rgba: (out): The #GdkRGBA to fill in
 * @kind: (out): The kind of representation as a #GstyleColorKind
 *
 * Get a #GdkRGBA and a #GstyleColorKind from a color string.
 * If you want to keep the args unit format,
 * use gstyle_color_new_from_string instead.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred.
 *
 */
gboolean
gstyle_color_parse_color_string (const gchar     *color_string,
                                 GdkRGBA         *rgba,
                                 GstyleColorKind *kind)
{
  G_GNUC_UNUSED gint name_index;
  gboolean ret;

  g_return_val_if_fail (!gstyle_str_empty0 (color_string), FALSE);
  g_return_val_if_fail (rgba != NULL, FALSE);

  ret = _parse_color_string (color_string, rgba, kind, &name_index);
  if (*kind == GSTYLE_COLOR_KIND_UNKNOW)
    *kind = GSTYLE_COLOR_KIND_RGB_HEX6;

  return ret;
}

/**
 * gstyle_color_fill:
 * @src_color: Source #GstyleColor
 * @dst_color: Destination #GstyleColor to fill with @src_color data
 *
 * Fill @dst_color with the rgba, name and kind of @src_color.
 *
 */
void
gstyle_color_fill (GstyleColor *src_color,
                   GstyleColor *dst_color)
{
  GdkRGBA rgba;

  g_assert (GSTYLE_IS_COLOR (src_color));
  g_assert (GSTYLE_IS_COLOR (dst_color));

  gstyle_color_fill_rgba (src_color, &rgba);
  gstyle_color_set_rgba (dst_color, &rgba);
  gstyle_color_set_name (dst_color, gstyle_color_get_name (src_color));
  gstyle_color_set_kind (dst_color, gstyle_color_get_kind (src_color));
  dst_color->name_index = src_color->name_index;
}

/**
 * gstyle_color_copy:
 * @self: a #GstyleColor
 *
 * A full copy of a #GstyleColor.
 *
 * Returns: (transfer full): a #GstyleColor.
 *
 */
GstyleColor *
gstyle_color_copy (GstyleColor *self)
{
  GstyleColor *color;
  GdkRGBA rgba;

  g_return_val_if_fail (GSTYLE_IS_COLOR (self), NULL);

  gstyle_color_fill_rgba (self, &rgba);
  color = g_object_new (GSTYLE_TYPE_COLOR,
                        "name", gstyle_color_get_name (self),
                        "kind", gstyle_color_get_kind (self),
                        "rgba", &rgba,
                        NULL);

  color->name_index = self->name_index;

  return color;
}

/**
 * gstyle_color_new:
 * @name: (nullable): The name of the color. Can be %NULL
 * @kind: The kind of representation as a #GstyleColorKind
 * @red: The red component in a [0-255] range
 * @green: The green component in a [0-255] range
 * @blue: The blue component in a [0-255] range
 * @alpha: The alpha component in a [0-100] range
 *
 * A #GstyleColor object from #GstyleColorKind and rgba components.
 *
 * Returns: a #GstyleColor.
 *
 */
GstyleColor *
gstyle_color_new (const gchar     *name,
                  GstyleColorKind  kind,
                  guint            red,
                  guint            green,
                  guint            blue,
                  guint            alpha)
{
  GdkRGBA rgba;

  rgba.red = red / 255.0;
  rgba.green = green / 255.0;
  rgba.blue = blue / 255.0;
  rgba.alpha = alpha / 100.0;

  return g_object_new (GSTYLE_TYPE_COLOR,
                       "name", name,
                       "kind", kind,
                       "rgba", &rgba,
                       NULL);
}

/**
 * gstyle_color_new_from_rgba:
 * @name: (nullable): The name of the color. Can be %NULL
 * @kind: The kind of representation as a #GstyleColorKind
 * @rgba: a #GdkRGBA
 *
 * A #GstyleColor object from a #GstyleColorKind and a #GdkRGBA object.
 *
 * Returns: a #GstyleColor.
 *
 */
GstyleColor *
gstyle_color_new_from_rgba (const gchar     *name,
                            GstyleColorKind  kind,
                            GdkRGBA         *rgba)
{
  return g_object_new (GSTYLE_TYPE_COLOR,
                       "name", name,
                       "kind", kind,
                       "rgba", rgba,
                       NULL);
}

/**
 * gstyle_color_from_hsla:
 * @name: (nullable): The name of the color. Can be %NULL
 * @kind: The kind of representation as a #GstyleColorKind
 * @hue: The hue component of a hsla color in range [0.0-360.0[
 * @saturation: The saturation component in range [0.0-1.0]
 * @lightness: The lightness component in range [0.0-1.0]
 * @alpha: The alpha component in range [0.0-1.0]
 *
 * A #GstyleColor from #GstyleColorKind and rgba components.
 *
 * Returns: a #GstyleColor.
 *
 */
GstyleColor *
gstyle_color_new_from_hsla (const gchar     *name,
                            GstyleColorKind  kind,
                            gdouble          hue,
                            gdouble          saturation,
                            gdouble          lightness,
                            gdouble          alpha)
{
  GdkRGBA rgba;

  gstyle_color_convert_hsl_to_rgb (hue, saturation, lightness, &rgba);
  rgba.alpha = alpha;

  return g_object_new (GSTYLE_TYPE_COLOR,
                       "name", name,
                       "kind", kind,
                       "rgba", &rgba,
                       NULL);
}

/**
 * gstyle_color_new_from_string:
 * @name: (nullable): The name of the color. Can be NULL
 * @color_string: A null terminated string to parse
 *
 * A #GstyleColor object from a color string.
 *
 * Returns: a #GstyleColor or %NULL if the string can't be parsed.
 *
 */
GstyleColor *
gstyle_color_new_from_string (const gchar *name,
                              const gchar *color_string)
{
  GstyleColor *self;
  gint name_index;
  GdkRGBA rgba;
  GstyleColorKind kind;

  g_return_val_if_fail (!gstyle_str_empty0 (color_string), NULL);

  if (!_parse_color_string (color_string, &rgba, &kind, &name_index))
    return NULL;

  if (gstyle_str_empty0 (name))
    name = NULL;

  self = g_object_new (GSTYLE_TYPE_COLOR,
                       "name", name,
                       "kind", kind,
                       "rgba", &rgba,
                       NULL);

  if (kind == GSTYLE_COLOR_KIND_PREDEFINED)
    self->name_index = name_index;

  return self;
}

/**
 * gstyle_color_get_name:
 * @self: a #GstyleColor
 *
 * Get the name of a #GstyleColor.
 *
 * Returns: A string own by the callee.
 *
 */
const gchar *
gstyle_color_get_name (GstyleColor *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR (self), NULL);

  return self->name;
}

/**
 * gstyle_color_set_name:
 * @self: a #GstyleColor.
 * @name: (nullable): A string
 *
 * Set the name of a #GstyleColor.
 *
 */
void
gstyle_color_set_name (GstyleColor *self,
                       const gchar *name)
{
  g_return_if_fail (GSTYLE_IS_COLOR (self));

  if (g_strcmp0 (name, self->name) != 0)
    {
      g_free (self->name);
      if (!gstyle_str_empty0 (name))
        self->name = g_strdup (name);
      else
        self->name = NULL;

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
    }
}

/**
 * gstyle_color_get_kind:
 * @self: a #GstyleColor
 *
 * Get the #GstyleColorKind of a #GstyleColor.
 *
 * Returns: a #GstyleColorKind.
 *
 */
GstyleColorKind
gstyle_color_get_kind (GstyleColor *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR (self), 0);

  return self->kind;
}

/**
 * gstyle_color_set_kind:
 * @self: a #GstyleColor
 * @kind: a #GstyleColorKind
 *
 * Set the #GstyleColorKind of a #GstyleColor.
 *
 */
void
gstyle_color_set_kind (GstyleColor     *self,
                       GstyleColorKind  kind)
{
  g_return_if_fail (GSTYLE_IS_COLOR (self));

  if (self->kind != kind)
    {
      self->kind = kind;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KIND]);
    }
}

/**
 * gstyle_color_set_rgba:
 * @self: a #GstyleColor
 * @rgba: a #GdkRGBA
 *
 * Set #GstyleColor color from a #GdkRGBA.
 *
 */
void
gstyle_color_set_rgba (GstyleColor *self,
                       GdkRGBA     *rgba)
{
  g_return_if_fail (GSTYLE_IS_COLOR (self));
  g_return_if_fail (rgba != NULL);

  if (!gdk_rgba_equal (&self->rgba, &rgba))
    {
      self->rgba = *rgba;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RGBA]);
    }
}

/**
 * gstyle_color_set_alpha:
 * @self: a #GstyleColor
 * @alpha: the new alpha value in [0,1] range
 *
 * Set the alpha value the the #GstyleColor.
 *
 */
void
gstyle_color_set_alpha (GstyleColor *self,
                        gdouble      alpha)
{
  g_return_if_fail (GSTYLE_IS_COLOR (self));

  if (self->rgba.alpha != alpha)
    {
      self->rgba.alpha = alpha;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RGBA]);
    }
}

static void
gstyle_color_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GstyleColor *self = GSTYLE_COLOR (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, self->kind);
      break;

    case PROP_RGBA:
      g_value_set_boxed (value, &self->rgba);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GdkRGBA *rgba;
  GstyleColor *self = GSTYLE_COLOR (object);

  switch (prop_id)
    {
    case PROP_KIND:
      gstyle_color_set_kind (self, g_value_get_enum (value));
      break;

    case PROP_RGBA:
      rgba = g_value_get_boxed (value);
      gstyle_color_set_rgba (self, rgba);
      break;

    case PROP_NAME:
      gstyle_color_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_finalize (GObject *object)
{
  GstyleColor *self = GSTYLE_COLOR (object);

  gstyle_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gstyle_color_parent_class)->finalize (object);
}

static void
gstyle_color_class_init (GstyleColorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gstyle_color_finalize;
  object_class->get_property = gstyle_color_get_property;
  object_class->set_property = gstyle_color_set_property;

  properties [PROP_KIND] =
    g_param_spec_enum ("kind",
                       "Kind",
                       "The kind of color representation",
                       GSTYLE_TYPE_COLOR_KIND,
                       GSTYLE_COLOR_KIND_RGBA,
                       (G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RGBA] =
    g_param_spec_boxed ("rgba",
                        "rgba",
                        "Adress of an GdkRGBA color struct",
                        GDK_TYPE_RGBA,
                        (G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Color name",
                         "The name of the color.",
                         NULL,
                         (G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gstyle_color_init (GstyleColor *self)
{
  self->name_index = -1;
}

GType
gstyle_color_kind_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GSTYLE_COLOR_KIND_UNKNOW, "GSTYLE_COLOR_KIND_UNKNOW", "unknow" },
    { GSTYLE_COLOR_KIND_ORIGINAL, "GSTYLE_COLOR_KIND_ORIGINAL", "original" },
    { GSTYLE_COLOR_KIND_RGB_HEX6, "GSTYLE_COLOR_KIND_RGB_HEX6", "rgbhex6" },
    { GSTYLE_COLOR_KIND_RGB_HEX3, "GSTYLE_COLOR_KIND_RGB_HEX3", "rgbhex3" },
    { GSTYLE_COLOR_KIND_RGB, "GSTYLE_COLOR_KIND_RGB", "rgb" },
    { GSTYLE_COLOR_KIND_RGB_PERCENT, "GSTYLE_COLOR_KIND_RGB_PERCENT", "rgbpercent" },
    { GSTYLE_COLOR_KIND_RGBA, "GSTYLE_COLOR_KIND_RGBA", "rgba" },
    { GSTYLE_COLOR_KIND_RGBA_PERCENT, "GSTYLE_COLOR_KIND_RGBA_PERCENT", "rgbapercent" },
    { GSTYLE_COLOR_KIND_HSL, "GSTYLE_COLOR_KIND_HSL", "hsl" },
    { GSTYLE_COLOR_KIND_HSLA, "GSTYLE_COLOR_KIND_HSLA", "hsla" },
    { GSTYLE_COLOR_KIND_PREDEFINED, "GSTYLE_COLOR_KIND_PREDEFINED", "predefined" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstyleColorKind", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GType
gstyle_color_unit_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GSTYLE_COLOR_UNIT_NONE, "GSTYLE_COLOR_UNIT_NONE", "none" },
    { GSTYLE_COLOR_UNIT_PERCENT, "GSTYLE_COLOR_UNIT_PERCENT", "percent" },
    { GSTYLE_COLOR_UNIT_VALUE, "GSTYLE_COLOR_UNIT_VALUE", "value" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstyleColorUnit", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
