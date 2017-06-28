#########################
Extending the Editor View
#########################

You might want to add additional widgets or perform various operations on the view as the user interacts with the code-editor.
One way to do this is to create an ``Ide.EditorViewAddin`` in your plugin.
Implementing this interface allows you to be notified whenever a new view is created as well as when the current language changes.

Additionally, if you only want your plugin to be enabled when certain languages are active, you can set the ``X-Editor-View-Languages=c,cplusplus`` keyword in your ``.plugin`` file.
In the example provided, the plugin interface would be enabled for ``C`` and ``C++`` files.
