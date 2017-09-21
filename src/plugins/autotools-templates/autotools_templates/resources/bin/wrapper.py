#!@PYTHON@

import sys
{{if enable_i18n}}
import locale
import gettext

localedir = '@localedir@'

locale.bindtextdomain('{{name}}', localedir)
locale.textdomain('{{name}}')
gettext.bindtextdomain('{{name}}', localedir)
gettext.textdomain('{{name}}')
{{end}}

if __name__ == "__main__":
    sys.path.insert(1, '@pythondir@')
    from {{name_}} import __main__
    __main__.main()
