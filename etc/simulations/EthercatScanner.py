import select
import threading
import subprocess

class StoppableThread(threading.Thread):
    """Thread class with a stop() method. The thread itself has to check regurlary
    for the stopped() condition. Found on Stackoverflow 2014-10-02 and adapted:
    http://stackoverflow.com/questions/323972/is-there-any-way-to-kill-a-thread-in-python"""
    def __init__(self, *args, **kwargs):
        super(StoppableThread, self).__init__(*args, **kwargs)
        self._stop = threading.Event()

    def stop(self):
        self._stop.set()

    def stopped(self):
        return self._stop.isSet()

class EthercatScanner():
    def __init__(self, name, scanner, xml, socket="/tmp/socket", ui=None):
        self._ui = None
        self._child = None
        self.output = None
        self.errors = None
        self.name = name

        if ui:
            self._ui = ui.declareSimulation(self)

        args = [scanner, '-s', xml, socket]
        self._child = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        if self._child:
            self.output = StoppableThread(name='output', target=self._stdoutThread)
            self.output.start()
            self.errors = StoppableThread(name='errors', target=self._stderrThread)
            self.errors.start()

    def dispose(self):
        if self._child:
            self._child.terminate()
            self._child.wait()
        if self.output:
            self.output.stop()
            self.output.join()
        if self.errors:
            self.errors.stop()
            self.errors.join()

    def _stdoutThread(self):
        while not self.output.stopped():
            out,_,_ = select.select([self._child.stdout], [], [], 1.0)
            if out:
                text = self._child.stdout.readline()
                self._writeOutputOrError(text)

    def _stderrThread(self):
        while not self.errors.stopped():
            err,_,_ = select.select([self._child.stderr], [], [], 1.0)
            if err:
                text = self._child.stderr.readline()
                self._writeOutputOrError(text)

    def _writeOutputOrError(self, text):
        if self._ui:
            self._ui.output(text)
        else:
            print repr(text)
