/* gstyle-colorlexer.c
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "gstyle-colorlexer"

#include <glib.h>

#include "gstyle-color.h"
#include "gstyle-color-item.h"
#include "gstyle-types.h"
#include "gstyle-utils.h"

#include "gstyle-colorlexer.h"

#define PARSE_ARRAY_INITIAL_SIZE  32

#define YYCTYPE     guchar
#define YYCURSOR    cursor
#define YYMARKER    s->ptr
#define YYCTXMARKER s->ptr_ctx

static void
gstyle_colorlexer_init (GstyleColorScanner *s,
                        const gchar        *data)
{
  s->start = data;
  s->cursor = data;
}

static gint
gstyle_colorlexer_scan (GstyleColorScanner *s)
{
  const gchar *cursor = s->cursor;

standard:
  s->start = cursor;

/*!re2c
  re2c:define:YYCTYPE = guchar;
  re2c:yyfill:enable = 0;
  re2c:indent:top = 2;

  params = [0-9., \t%];
  hex = [0-9a-fA-F];
  named = "aliceblue" | "antiquewhite" | "aqua" | "aquamarine" | "azure" | "beige" | "bisque" | "black"| "blanchedalmond" |
          "blue" | "blueviolet" | "brown" | "burlywood" | "cadetblue" | "chartreuse" | "chocolate" | "coral" |
          "cornflowerblue" | "cornsilk" | "crimson" | "cyan" | "darkblue" | "darkcyan" | "darkgoldenrod" | "darkgray" |
          "darkgreen" | "darkgrey" | "darkkhaki" | "darkmagenta" | "darkolivegreen" | "darkorange" | "darkorchid" |
          "darkred" | "darksalmon" | "darkseagreen" | "darkslateblue" | "darkslategray" | "darkslategrey" | "darkturquoise" |
          "darkviolet" | "deeppink" | "deepskyblue" | "dimgray" | "dimgrey" | "dodgerblue" | "firebrick" | "floralwhite" |
          "forestgreen" | "fuchsia" | "gainsboro" | "ghostwhite" | "gold" | "goldenrod" | "gray" | "green" | "greenyellow" | "grey" |
          "honeydew" | "hotpink" | "indianred" | "indigo" | "ivory" | "khaki" | "lavender" | "lavenderblush" | "lawngreen" |
          "lemonchiffon" | "lightblue" | "lightcoral" | "lightcyan" | "lightgoldenrodyellow" | "lightgray" | "lightgreen" | "lightgrey" |
          "lightpink" | "lightsalmon" | "lightseagreen" | "lightskyblue" | "lightslategray" | "lightslategrey" | "lightsteelblue" |
          "lightyellow" | "lime" | "limegreen" | "linen" | "magenta" | "maroon" | "mediumaquamarine" | "mediumblue" | "mediumorchid" |
          "mediumpurple" | "mediumseagreen" | "mediumslateblue" | "mediumspringgreen" | "mediumturquoise" | "mediumvioletred" |
          "midnightblue" | "mintcream" | "mistyrose" | "moccasin" | "navajowhite" | "navy" | "oldlace" | "olive" | "olivedrab" |
          "orange" | "orangered" | "orchid" | "palegoldenrod" | "palegreen" | "paleturquoise" | "palevioletred" | "papayawhip" |
          "peachpuff" | "peru" | "pink" | "plum" | "powderblue" | "purple" | "red" | "rosybrown" | "royalblue" | "saddlebrown" |
          "salmon" | "sandybrown" | "seagreen" | "seashell" | "sienna" | "silver" | "skyblue" | "slateblue" | "slategray" |
          "slategrey" | "snow" | "springgreen" | "steelblue" | "tan" | "teal" | "thistle" | "tomato" | "turquoise" | "violet" |
          "wheat" | "white" | "whitesmoke" | "yellow" | "yellowgreen";

  "/*"                              { goto comment; }
  [ \t"',:(] [ \t]*                 { s->start = cursor;goto color; }

  "\000"                            {s->cursor = cursor; return GSTYLE_COLOR_TOKEN_EOF;}
  [^]                               { goto standard; }
*/

color:
/*!re2c

  "#" hex{3}                        {s->cursor = cursor; return GSTYLE_COLOR_TOKEN_HEX;}
  "#" hex{6}                        {s->cursor = cursor; return GSTYLE_COLOR_TOKEN_HEX;}
  "#"? "rgb" "a"? "(" params+ ")"   {s->cursor = cursor; return GSTYLE_COLOR_TOKEN_RGB;}
  "#"? "hsl" "a"? "(" params+ ")"   {s->cursor = cursor; return GSTYLE_COLOR_TOKEN_HSL;}
  "#"? named / ["' \t,;)]           {s->cursor = cursor; return GSTYLE_COLOR_TOKEN_NAMED;}

  "\000"                            {s->cursor = cursor; return GSTYLE_COLOR_TOKEN_EOF;}
  [^]                               { goto standard; }
*/

comment:
/*!re2c

  "*/"                              { goto standard; }
  "\000"                            {s->cursor = cursor; return GSTYLE_COLOR_TOKEN_EOF;}
  [^]                               { goto comment; }
*/
}

/* TODO: check for " ' \t space around named */
/**
 * gstyle_colorlexer_parse:
 * @data: a constant string.
 *
 *  Parse a string and return an array of #GstyleColorItem.
 *
 * Returns: (element-type GstyleColorItem) (transfer full): a #GPtrArray of #GstyleColorItem.
 *
 */
GPtrArray *
gstyle_colorlexer_parse (const gchar *data)
{
  GPtrArray *ar;
  GstyleColorItem *item;
  GstyleColorScanner s;
  gint token;

  if (gstyle_str_empty0 (data))
    return NULL;

  ar = g_ptr_array_new_full (PARSE_ARRAY_INITIAL_SIZE, (GDestroyNotify)gstyle_color_item_unref);
  gstyle_colorlexer_init (&s, data);

  while((token = gstyle_colorlexer_scan (&s)) != GSTYLE_COLOR_TOKEN_EOF)
    {
      if (token != GSTYLE_COLOR_TOKEN_HEX && *s.start == '#')
        ++s.start;

      item = gstyle_color_item_new (NULL, s.start - data, s.cursor - s.start);
      g_ptr_array_add (ar, item);
    }

  return ar;
}
