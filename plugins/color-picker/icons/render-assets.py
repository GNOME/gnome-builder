#!/usr/bin/python3

import os
import subprocess
import shutil

INKSCAPE = "/usr/bin/inkscape"
SVG_SRC = "assets.svg"
TXT_SRC = "assets.txt"

CWD = os.getcwd()

class Exporter(object):
    def __init__(self):
        self.ids = []
        with open(TXT_SRC) as input_file:
            for line in input_file:
                self.ids.append(line.strip())

    def start(self):
        for id in self.ids:
            self.export (id)
            print("id:'{}' exported".format(id))

    def export(self, id):
        src = "".join([CWD, "/", SVG_SRC])
        dst = "".join([CWD, "/", id, ".svg"])
        shutil.copyfile (src, dst)
        self.call_inkscape([dst,
                            "--select={}".format(id),
                            "--verb=FitCanvasToSelection",
                            "--verb=EditInvertInAllLayers",
                            "--verb=EditDelete",
                            "--verb=EditSelectAll",
                            "--verb=SelectionUnGroup",
                            "--verb=SelectionUnGroup",
                            "--verb=SelectionUnGroup",
                            "--verb=StrokeToPath",
                            "--verb=FileVacuum",
                            "--verb=FileSave",
                            "--verb=FileClose",
                            "--verb=FileQuit"])

    def call_inkscape(self, cmd):
        cmd.insert(0, INKSCAPE)
        subprocess.run(cmd, stdout=subprocess.PIPE)

if __name__ == "__main__":
    app = Exporter()
    app.start()
