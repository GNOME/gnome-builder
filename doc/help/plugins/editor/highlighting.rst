###################
Syntax Highlighting
###################

Regex-based Highlighting
========================

Syntax highlighting in Builder is performed by the GtkSourceView project.
By providing an XML description of the syntax, GtkSourceView can automatically highlight the language of your choice.
Thankfully, GtkSourceView already supports a large number of languages so the chances you need to add a new language is low.
However, if you do, we suggest that you work with GtkSourceView to ensure that all applications, such as Gedit, benefit from your work.

Chances are you can find existing language syntax files on your system in ``/usr/share/gtksourceview-4/language-specs/``.
These language-spec files serve as a great example of how to make your own.
If it is not there, chances are there is already a ``.lang`` file created but it has not yet been merged upstream.

Bundling Language Specs
-----------------------

Should you need to bundle your own language-spec, consider using ``GResources`` to embed the language-spec within your plugin.
Then append the directory path of your language-specs to the ``GtkSource.LanguageManager`` so it knows where to locate them.

.. code-block:: py

   from gi.repository import GtkSource

   manager = GtkSource.LanguageManager.get_default()
   paths = manager.get_search_path()
   paths.append('resources:///plugins/my-plugin/language-specs/')
   manager.set_search_path(paths)


Symantic Highlighting
=====================

If the language you are using provides an AST you may want to highlight additional information not easily decernable by a regex-based highlighter.
To simplify this, Builder provides the ``Ide.HighlightEngine`` and ``Ide.Highlighter`` abstractions.

The ``Ide.HighlightEngine`` provides background updating of the document so that your ``Ide.Highlighter`` implementation can focus on highlighting without dealing with performance impacts.

Out of simplicity, most ``Ide.Highlighter`` implementations in Builder today use a simple word index and highlight based on the word.
However, this is not required if you prefer to do something more technical such as matching ranges to the AST.


