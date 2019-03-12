#######################
Diagnostics and Fix-Its
#######################

In order to show diagnostics in the editor, you need to implemenet an ``Ide.DiagnosticProvider`` and override two methods: ``do_diagnose_async`` and ``do_diagnose_finish``.

The ``do_diagnose_async`` is an asynchronous method that will be called with a callback as the fifth parameter. The callback can be passed to a ``Gio.Task`` for easy handling. When the task is done, ``do_diagnose_finish`` will be called with the ``Gio.Task`` object and is expected to return an ``Ide.Diagnostics`` object.


.. code-block:: python3

    # my_plugin.py
    
    import gi

    from gi.repository import GLib, Gio, Ide
    
    class MyDiagnosticProvider(Ide.Object, Ide.DiagnosticProvider):
        def do_diagnose_async(self,
                              file: Ide.File,
                              contents: bytes,
                              lang_id: str,
                              cancellable: Gio.Cancellable,
                              callback: Gio.AsyncReadyCallback,
                              user_data):
            task = Gio.Task.new(self, cancellable, callback)
            task.diagnostics_list = []
    
            start = Ide.SourceLocation.new(file, 0, 0, 0)
            severity = Ide.DiagnosticSeverity.WARNING
            error_message = 'Diagnostic example'
    
            diagnostic = Ide.Diagnostic.new(severity, error_message, start)
            task.diagnostics_list.append(diagnostic)
    
            task.return_boolean(True)
    
        def do_diagnose_finish(self, result: Gio.Task) -> Ide.Diagnostics:
            if result.propagate_boolean():
                return Ide.Diagnostics.new(result.diagnostics_list)
                

You also need to register the plugin as a diagnostic provider in the ``.plugin`` file. There is a ``X-Diagnostic-Provider-Languages`` field which specify the supported languages and ``X-Diagnostic-Provider-Languages-Priority`` which specify the diagnostic priority.

For example, a C diagnostic plugin will have a plugin file that look similar to this:

.. code-block:: none

    # my_plugin.plugin
    
    [Plugin]
    Module=my_plugin
    Name=my_plugin
    Loader=python3
    Description=Provides C diagnostics
    Authors=Author Name <authorname@mailprovider.com>
    Copyright=Copyright © 2017 Author Name <authorname@mailprovider.com>
    X-Diagnostic-Provider-Languages=c
    X-Diagnostic-Provider-Languages-Priority=100
    X-Builder-ABI=3.32
