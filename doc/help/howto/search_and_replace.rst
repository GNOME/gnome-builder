##################
Search and Replace
##################

Search and replace can be used to replace all instances of a keyword with another form of text.
To bring up the “Search and Replace” tool, use ``Ctrl+h`` while focused in the editor.

.. note:: If you are using an alternate keyboard shortcut theme, your shortcut might be different.

Enter the search text in the first text entry.
Enter the replacement text in the second text entry.

.. tip:: The replacement text may contain “regex backreferences” such as ``\1`` and others.
         See the g_regex_replace_ documentation for more information.

Select “Replace” to replace the next match or “Replace All” to replace all matches.

.. _g_regex_replace: https://developer.gnome.org/glib/stable/glib-Perl-compatible-regular-expressions.html#g-regex-replace

Using Special Characters
========================

If you want to insert or replace a tab character using "search and replace", you need to use the escaped form of tab (``\t``).
You can also do this for new lines with ``\n``.

