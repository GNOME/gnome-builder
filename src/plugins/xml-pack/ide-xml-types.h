/* ide-xml-types.h
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  IDE_XML_POSITION_KIND_UNKNOW,
  IDE_XML_POSITION_KIND_IN_START_TAG,
  IDE_XML_POSITION_KIND_IN_END_TAG,
  IDE_XML_POSITION_KIND_IN_CONTENT
} IdeXmlPositionKind;

typedef enum
{
  IDE_XML_COMPLETION_TYPE_NONE,
  IDE_XML_COMPLETION_TYPE_ELEMENT,
  IDE_XML_COMPLETION_TYPE_ATTRIBUTE,
  IDE_XML_COMPLETION_TYPE_VALUE,
  IDE_XML_COMPLETION_TYPE_UI_PROPERTY,
  IDE_XML_COMPLETION_TYPE_UI_SIGNAL,
  IDE_XML_COMPLETION_TYPE_UI_PACKAGE,
  IDE_XML_COMPLETION_TYPE_UI_GTYPE
} IdeXmlCompletionType;

G_END_DECLS
