import threading
import subprocess

class EthercatScanner():
    def __init__(self, name, scanner, xml, socket="/tmp/socket", ui=None):
        self._child = None
        self.name = name
        if ui:
            self._ui = ui.declareSimulation(self)
        args = [scanner, '-s', xml, socket]
        self._child = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if self._child:
            self.output = threading.Thread(target=self._stdoutThread)
            self.output.start()
            self.errors = threading.Thread(target=self._stderrThread)
            self.errors.start()

    def __del__(self):
        if self._child:
            self._child.terminate()
            self._child.wait()

    def _stdoutThread(self):
        while True:
            text = self._child.stdout.readline()
            self._writeOutputOrError(text)

    def _stderrThread(self):
        while True:
            text = self._child.stderr.readline()
            text = self._writeOutputOrError(text)

    def _writeOutputOrError(self, text):
        if self._ui:
            self._ui.output(text)
        else:
            print repr(text)
