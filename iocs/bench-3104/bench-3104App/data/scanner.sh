#!/bin/sh
cd "$(dirname $0)"
/home/rjq35657/R3.14.12.3/support/ethercat/bin/linux-x86_64/scanner -q ./expanded.xml /tmp/socket
