#allow remote set quota by defined rpcsetquota to 1(set to 0 to disabled it)
%{!?rpcsetquota:%define rpcsetquota 1}
%define quotaversion 3.17

Name: python-quota
Summary: Python support for quota
Version: 0.1
Release: 1%{?dist}
License: BSD and GPLv2+
URL: http://sourceforge.net/projects/linuxquota/
Group: System Environment/Base
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: initscripts >= 6.38 tcp_wrappers e2fsprogs
Conflicts: kernel < 2.4
BuildRequires: e2fsprogs-devel gettext tcp_wrappers-devel nss-devel
BuildRequires: openldap-devel openssl-devel dbus-devel libnl-devel
BuildRequires: python-devel python-setuptools
Source0: http://downloads.sourceforge.net/linuxquota/quota-%{quotaversion}.tar.gz
Patch0:	quota-3.06-warnquota.patch
Patch1: quota-3.06-no-stripping.patch
Patch2: quota-3.06-man-page.patch
Patch3: quota-3.06-pie.patch
Patch4: quota-3.13-wrong-ports.patch
Patch5: quota-3.16-helpoption.patch
Patch6: quota-3.16-quotaoffhelp.patch
Patch7: quota-3.17-quotactlmanpage.patch
Patch8: quota-3.17-goodclient.patch
# Bug #589478, included in upstream 4.00_pre2
Patch9: quota-3.17-quotactl_null_corruption.patch
Patch1000: quotaops.patch
Source1: quotamodule.c
Source2: setup.py

%description
python-quota uses the c-api to create a quota object
--
The quota package contains system administration tools for monitoring
and limiting user and or group disk usage per filesystem.

%prep
%setup -q -n quota-tools
%patch0 -p1
%patch1 -p1
%patch2 -p1
%ifnarch ppc ppc64
%patch3 -p1
%endif
%patch4 -p1
%patch5 -p1
%patch6 -p1 -b .usage
%patch7 -p1 -b .quotactlman
%patch8 -p1 -b .goodclient
%patch9 -p1 -b .quota_null_corruption

# python-quota patch
%patch1000 -p1 -b .cp

#fix typos/mistakes in localized documentation
for pofile in $(find ./po/*.p*)
do
   sed -i 's/editting/editing/' "$pofile"
done
cp %{SOURCE1} .
cp %{SOURCE2} .

%build
%configure \
	--enable-ldapmail=try \
%if %{rpcsetquota}
	--enable-rpcsetquota=yes \
%endif
	--enable-rootsbin \
	--enable-netlink=yes

%if %{rpcsetquota}
make rquota.h
make rquota_clnt.c
make rquota_xdr.c
%endif 

python setup.py build_ext \
%if %{rpcsetquota}
	--define RPC \
%endif
	--define BSD_BEHAVIOUR \
	--define QUOTA_VERSION=3.17 \
	--define COMPILE_OPTS \
build

%install
rm -fr %{buildroot}
mkdir -p %{buildroot}/%{python_sitelib}
cp build/lib*/quota.so %{buildroot}/%{python_sitelib}

%clean
rm -rf %{buildroot}

%files 
%defattr(-,root,root,-)
%attr(0755,root,root) %{python_sitelib}/quota.so

%changelog
* Fri Apr 1 2011 Thomas Uphill <uphill@ias.edu> 0.1-1
- initial build
