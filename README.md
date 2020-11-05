# ethercat
EPICS support to read/write to ethercat based hardware

Prerequisites: [IgH EtherCAT Master for Linux](http://etherlab.org/en/ethercat/index.php)

This EPICS module builds with a patched version of etherlab, described [here](http://controls.diamond.ac.uk/downloads/support/ethercat/4-7/documentation/doxygen/building.html)

The documentation builds using doxygen based on files in the folder
etc/makeDocumentation.

[Doxygen documentation for old releases (up to 4-7) is available in "controls.diamond.ac.uk"](http://controls.diamond.ac.uk/downloads/support/ethercat/)

Release notes for version 4-7: [link in "controls.diamond.ac.uk"](http://controls.diamond.ac.uk/downloads/support/ethercat/4-7/documentation/doxygen/release_notes.html)

Release notes in
etc/makeDocumentation/release_notes.src

Please email with issues as the maintainer has been know to ignore
github notifications for months.

Maintainer notes

Before making a release:

1. Check/update the version in ethercatApp/scannerSrc/version.h
2. Write release notes in etc/makeDocumentation/release_notes.src
