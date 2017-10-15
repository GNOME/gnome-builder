#####################
Searching for Symbols
#####################

Builder supports searching for a symbol by name.

First, focus the search entry with ``Control+.``.
Then start typing the name of the symbol.
Fuzzy searching is supported.
For example, searching for ``GcWin`` might match the symbol ``GcalWindow``.

Press ``Up`` or ``Down`` arrows to move between the results.
Press ``Enter`` to activate the search result and jump to the symbol.
You can press ``Escape`` to return to the editor.

.. note:: This feature requires that the ``code-index`` plugin is enabled.

Filtering searches
------------------

You can narrow search results by prefixing searches with the following keys:

- ``f`` for functions
- ``v`` for variables
- ``s`` for structs
- ``u`` for unions
- ``e`` for enums
- ``c`` for class
- ``a`` for constants
- ``m`` for macros
