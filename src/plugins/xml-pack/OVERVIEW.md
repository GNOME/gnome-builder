# Xml-pack Objects

## IdeXmlAnalysis

A boxed type grouping together an #IdeXmlSymbolTree and the associated #IdeDiagnostics.

## IdeXmlDiagnosticProvider

Get diagnostics for xml/html files from the #IdeXmlService.

## IdeXmlFormatter

Implements full-file formatting for XML/HTML

## IdeXmlHighlighter

Highlight the matching tags in xml/html files.

## IdeXmlIndenter

Handle the correct indentation in xml/html files.

## IdeXmlSymbolResolver

Get xml/html files symbol trees from the #IdeXmlService.

## IdeXmlSax

A wrapper around the libxml2 sax parser.

## IdeXmlService

A service responsible of fetching and storing the xml/html symbol trees and diagnostics.

## IdeXmlStack

A stack type used by the tree builder to avoid recursive code by using a managed stack.

## IdeXmlSymbolNode

Specific node type for xml/html element.

## IdeXmlSymbolTree

A symbols tree for xml/html files.

## IdeXmlTreeBuilder

A tree builder for xml/html files.

# XML/HTML Symbols tree

## .ui and .glade files

Files with .ui or .glade extension and <interface> as the first node
are parsed with these specifics rules.

To not clutter the symbol tree, only the following tags correspond to a visible node:

`<template> <object> <child> <packing> <style>`

`<menu> <submenu> <section> <item>`

Specifics processing:

* Collected sub-nodes or attributes shown on the same line:

`<template>` tag: "class" and "parent" attributes.

`<object>` tag: "class" and "id" attributes.

`<child>` tag: "type" and "internal-child" attributes.

`<style>` tag: `<class name=" "/>` sub-nodes.

`<menu>` tag: "id" attributes.

`<submenu>` tag: "id" attribute and `<attribute name="label">` value.

`<section>` tag: "id" attribute and `<attribute name="label">` value.

`<item>` tag: "id" attribute and `<attribute name="label">` value.

## HTML and XML files

Tag content is not shown.
Comment content is shown on the same line as the comment node.
CDATA content is not shown.
Attributes are collected and shown on the same line as the tag node.
