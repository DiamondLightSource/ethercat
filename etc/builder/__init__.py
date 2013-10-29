from iocbuilder.support import ExportModules
from . import ethercat

ethercat.initialise()

__all__ = ExportModules(globals(), 'devices')

