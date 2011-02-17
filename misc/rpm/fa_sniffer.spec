Summary: PCI express FA sniffer driver
Name: fa_sniffer
Version: 1.0
Release: 1dkms
License: GPL
Group: System Environment/Kernel
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildArch: noarch
Requires: dkms
Requires: udev
Packager: Michael Abbott <michael.abbott@diamond.ac.uk>

# The two target directories
%define dkmsdir /usr/src/%{name}-%{version}
%define udevdir /etc/udev/rules.d

%description
Installs FA sniffer driver kernel module using dkms.  This provides
a continuous streaming interface to Libera Fast Acquisition data and
relies on a dedicated FPGA card.

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{dkmsdir} %{buildroot}%{udevdir}
install -m 0644 %{_sourcedir}/fa_sniffer.c        %{buildroot}%{dkmsdir}
install -m 0644 %{_sourcedir}/Makefile            %{buildroot}%{dkmsdir}
install -m 0644 %{_sourcedir}/Kbuild              %{buildroot}%{dkmsdir}
install -m 0644 %{_sourcedir}/dkms.conf           %{buildroot}%{dkmsdir}
install -m 0644 %{_sourcedir}/11-fa_sniffer.rules %{buildroot}%{udevdir}

%files
%{dkmsdir}/fa_sniffer.c
%{dkmsdir}/Makefile
%{dkmsdir}/Kbuild
%{dkmsdir}/dkms.conf
%{udevdir}/11-fa_sniffer.rules

%post
dkms add     -m %{name} -v %{version} --rpm_safe_upgrade
dkms build   -m %{name} -v %{version}
dkms install -m %{name} -v %{version}
modprobe fa_sniffer

%preun
modprobe -r fa_sniffer
dkms remove --all -m %{name} -v %{version} --rpm_safe_upgrade

%postun
rmdir /usr/src/%{name}-%{version}
