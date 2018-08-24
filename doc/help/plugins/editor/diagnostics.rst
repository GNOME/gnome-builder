#######################
Diagnostics and Fix-Its
#######################

In order to show diagnostics in the editor, you need to implemenet an 
``Ide.DiagnosticProvider`` and override the ``do_diagnose_async`` and 
``do_diagnose_finish`` methods.

The ``do_diagnose_async`` is an asynchronous method that will be called with a 
callback as the fifth parameter. The callback can be passed to a ``Gio.Task`` 
for easy handling. When the task is done, ``do_diagnose_finish`` will be called 
with the ``Gio.Task`` object and is expected to return an ``Ide.Diagnostics`` 
object.


.. code-block:: python3

    class MyDiagnosticProvider(Ide.Object, Ide.DiagnosticProvider):
        def do_diagnose_async(self, file: Ide.File, buffer: Ide.Buffer, cancellable, callback, user_data):
            task = Gio.Task.new(self, cancellable, callback)
            task.diagnostics_list = []
            
            try:
                start = Ide.SourceLocation.new(file, 0, 0, 0)
                severity = Ide.DiagnosticSeverity.WARNING
                error_message = 'Diagnostic example'
            
                diagnostic = Ide.Diagnostic.new(severity, error_message, start
                task.diagnostics_list.append(diagnostic)
            except GLib.Error as err:
                task.return_error(err)
            else:
                task.return_boolean(True)
    
            task.return_boolean(True)
    
        def do_diagnose_finish(self, result: Gio.Task) -> Ide.Diagnostics:
            if result.propagate_boolean():
                return Ide.Diagnostics.new(result.diagnostics_list)