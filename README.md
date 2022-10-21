# ethercat
EPICS support to read/write to ethercat based hardware

Prerequisites: [IgH EtherCAT Master for Linux](http://etherlab.org/en/ethercat/index.php)

This EPICS module builds with a patched version of etherlab, described in the file etc/makeDocumentation/building.src

The documentation was made when doxygen at DLS would build in the
folder etc/makeDocumentation.

The doxygen documentation is no longer building at Diamond, but the "sources" are in
the folder etc/makeDocumentation.

Release notes in
etc/makeDocumentation/release_notes.src

Please email with issues as the maintainer has been know to ignore
github notifications for months.

Maintainer notes

Before making a release:

1. Check/update the version in ethercatApp/scannerSrc/version.h
2. Write release notes in etc/makeDocumentation/release_notes.src
