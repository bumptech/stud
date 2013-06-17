# The GIT_VERSION token below is automatically changed to the value of
# `git describe` by the top-level Makefile's "rpm" build target
%define git_version GIT_VERSION
#
# The git version is <tag>-#-sha1, where the <tag> is the most recent
# annotated git tag, # is the number of commits since the tag, and the
# sha1 is 'g' plus the first 7 chars of the most recent commit sha1-id
# on the branch being compiled
#
# This will split the version information back into the <tag> (version)
# and the #-sha1 (post_tag) parts
#
%define version %(echo %{git_version} | cut -d- -f1)
%define post_tag %(echo %{git_version} | cut -s -d- -f2- | sed -e s/-/_/g)
%if x%{post_tag} == "x"
    %undefine post_tag
%endif

Name:		stud
Version:	%{version}
Release:	%{?post_tag}%{!?post_tag:1}%{?dist}
Summary:	The Scalable TLS Unwrapping Daemon

Group:		Productivity/Networking/Web/Proxy
License:	BSD
URL:		https://github.com/bumptech/stud
Source0:	stud-source.tar

BuildRequires: libev-devel
Requires:	libev openssl

%description
stud is a network proxy that terminates TLS/SSL connections and forwards the
unencrypted traffic to some backend. It's designed to handle 10s of
thousands of connections efficiently on multicore machines.

%prep
%setup -q


%build
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot} PREFIX=/usr
install -d %{buildroot}/etc/init.d
install init.stud %{buildroot}/etc/init.d/stud

%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
/usr/bin/stud
/etc/init.d/stud
%doc
/usr/share/man/man8


%preun
if [ "$1" = "0" ]; then
    service stud stop 2>/dev/null
    true
fi

%postun
if [ "$1" = "0" ]; then
    # remove rundir
    rm -rf /var/run/stud >/dev/null 2>&1

    # remove config dir
    rm -rf /etc/stud >/dev/null 2>&1
fi

%post
mkdir -p /var/run/stud
mkdir -p /etc/stud


%changelog

