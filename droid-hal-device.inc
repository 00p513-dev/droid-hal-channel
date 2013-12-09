# This file should be %included into a device specific spec file
# where macros are defined:
# device: should be the CM codename 
# vendor: determine the directory used for ./device/<vendor>/<device>

%define android_root .

Summary: 	Droid HAL package for %{device}
License: 	BSD-3-Clause
Name: 		droid-hal-%{device}
Version: 	0.0.1
Release: 	0
Provides:       droid-hal
Source0: 	%{name}-%{version}.tar.bz2
Source1: 	makefstab
Source2: 	usergroupgen.c
Source3:        makeudev
Source4:        apply-permissions.c
Source5:        makefile
Source6:        fixup-mountpoints
Source7:        compositor_mako-cm10.1.conf
Source8:        hybris.conf
Source9:        sailfish-oneshot-rotation
Source10:       droid-hal-device.inc
# Allow device specific sources to be defined
# Reserve Source50 onwards
%dhd_sources

Group:		System
#BuildArch:	noarch
# To provide systemd services and udev rules
Requires:       droid-system-packager
# Note that oneshot is not in mer-core (yet)
BuildRequires:  oneshot
BuildRequires:  mer-kernel-checks
BuildRequires:  systemd
%systemd_requires
%{_oneshot_requires_post}

%description
%{summary}.

%package devel
Group:	Development/Tools
Requires: %{name} = %{version}-%{release}
Provides: droid-hal-devel
Summary: Development files for droid hal

%description devel
Device specific droid headers for %{device}.
Needed by libhybris

%package sailfish-config
Provides: droid-hal-sailfish-config
Group:	System
Requires: %{name} = %{version}-%{release}
Requires: oneshot
Summary: Per device configuration for sailfish

%description sailfish-config
Configure sailfish eg naturally landscape devices like mako

%prep
%setup -q

%build
echo Verifying kernel config
mer_verify_kernel_config \
    %{android_root}/out/target/product/%{device}/obj/KERNEL_OBJ/.config

echo Building local tools
rm -rf rpmsrc
mkdir rpmsrc
cp %{SOURCE2} %{SOURCE4} rpmsrc/
ANDROID_ROOT=$(readlink -e %{android_root})
(cd rpmsrc; make ANDROID_ROOT=$ANDROID_ROOT -f %{SOURCE5})

echo Building uid scripts
rpmsrc/usergroupgen add > droid-user-add.sh
rpmsrc/usergroupgen remove > droid-user-remove.sh

echo Building udev rules
rm -rf udev.rules
mkdir udev.rules
# Device specific ueventd rules is the "not goldfish" one
%{SOURCE3} \
    %{android_root}/out/target/product/%{device}/root/ueventd.rc \
    $(ls %{android_root}/out/target/product/%{device}/root/ueventd.*.rc | grep -v .goldfish.rc) \
        > udev.rules/999-android-system.rules

echo Building mount units
rm -rf units
mkdir -p units
# Use the makefstab and tell it what mountpoints to skip. It will
# generate .mount units which will be part of local-fs.target
# skip /cache and /data - won't touch them (yet)
cat %{android_root}/out/target/product/%{device}/root/fstab.* | (cd units; %{SOURCE1} /cache /data  )

echo Fixing up mount points
%{SOURCE6}

%define units %(cd units;echo *)

%install
echo install %units
rm -rf $RPM_BUILD_ROOT
# Create dir structure
mkdir -p $RPM_BUILD_ROOT%{_libexecdir}/droid-hybris/lib-dev-alog/
mkdir -p $RPM_BUILD_ROOT%{_libexecdir}/droid-hybris/system
mkdir -p $RPM_BUILD_ROOT%{_libdir}/droid
mkdir -p $RPM_BUILD_ROOT%{_libdir}/droid-devel/
mkdir -p $RPM_BUILD_ROOT/etc/droid-init/
mkdir -p $RPM_BUILD_ROOT/%{_unitdir}
mkdir -p $RPM_BUILD_ROOT/lib/udev/rules.d
mkdir -p $RPM_BUILD_ROOT/%{_sharedstatedir}/environment/compositor
mkdir -p $RPM_BUILD_ROOT/%{_sharedstatedir}/environment/nemo
mkdir -p $RPM_BUILD_ROOT/%{_oneshotdir}

# Install
cp -a %{android_root}/out/target/product/%{device}/root/. $RPM_BUILD_ROOT/
cp -a %{android_root}/out/target/product/%{device}/system/. $RPM_BUILD_ROOT%{_libexecdir}/droid-hybris/system/.
cp -a %{android_root}/out/target/product/%{device}/obj/{lib,include} $RPM_BUILD_ROOT%{_libdir}/droid-devel/
cp -a %{android_root}/out/target/product/%{device}/symbols $RPM_BUILD_ROOT%{_libdir}/droid-devel/

# If this ever becomes unmanageable then
# grep -l dev/alog %{android_root}/out/target/product/%{device}/system/lib/*
# libdsyscalls.so and libc.so are blacklisted
ln -s ../system/lib/{liblog.so,libcutils.so} $RPM_BUILD_ROOT%{_libexecdir}/droid-hybris/lib-dev-alog/.

cp -a units/* $RPM_BUILD_ROOT/%{_unitdir}

# Install the udev rules and supporting script
cp -a udev.rules/* $RPM_BUILD_ROOT/lib/udev/rules.d/

# droid user support This may be better done by passing a list of
# users/groups and running 'ensure_usergroups_exist newlist oldlist'
# which would preserve oldlist in %post and delete any users no longer
# needed (unlikely!). This avoids the transient removal of uids and
# group issues
install -D droid-user-add.sh $RPM_BUILD_ROOT%{_libdir}/droid/droid-user-add.sh
install -D droid-user-remove.sh $RPM_BUILD_ROOT%{_libdir}/droid/droid-user-remove.sh

# droid permission fixer
install -D rpmsrc/apply-permissions $RPM_BUILD_ROOT%{_libdir}/droid/apply-permissions

# Remove cruft
rm $RPM_BUILD_ROOT/fstab.*
rm -rf $RPM_BUILD_ROOT/{proc,sys,dev,sepolicy $RPM_BUILD_ROOT/{file,property,seapp}_contexts
rm -rf $RPM_BUILD_ROOT/{charger,res,data}

# Relocate rc files and other things left in / where possible
# mv $RPM_BUILD_ROOT/*rc $RPM_BUILD_ROOT/etc/droid-init/
# Name this so droid-system-packager's droid-hal-startup.sh can find it
mkdir -p $RPM_BUILD_ROOT/sbin
mv $RPM_BUILD_ROOT/init $RPM_BUILD_ROOT/sbin/droid-hal-init
# Rename any symlinks to droid's /init 
find $RPM_BUILD_ROOT/sbin/ -lname ../init -execdir echo rm {} \; -execdir echo "ln -s" ./droid-hal-init {} \;
#mv $RPM_BUILD_ROOT/charger $RPM_BUILD_ROOT/sbin/droid-hal-charger

# To set the environment for Qt/hybris (note this is a bit lipstick specific)
cp %{SOURCE7} $RPM_BUILD_ROOT/%{_sharedstatedir}/environment/compositor/mako-cm10.1.conf
cp %{SOURCE8} $RPM_BUILD_ROOT/%{_sharedstatedir}/environment/nemo/99-hybris.conf

# Add the oneshot
cp %{SOURCE9} $RPM_BUILD_ROOT/%{_oneshotdir}/oneshot-rotation

# This ghost file must exist in the installroot
touch $RPM_BUILD_ROOT/%{_libdir}/droid/droid-user-remove.sh.installed

%preun
for u in %units; do
%systemd_preun $u
done
# Only run this during final cleanup
if [ $1 == 0 ]; then
    echo purging old droid users and groups
    %{_libdir}/droid/droid-user-remove.sh.installed
    true
fi

%post
for u in %units; do
%systemd_post $u
done
cd %{_libdir}/droid
# Upgrade: remove users using stored file, then add new ones
if [ $1 == 2 ]; then
    # Remove installed users (at this point droid-user-remove.sh
    # refers to the new set of UIDs)
    echo removing old droid users and groups
    ./droid-user-remove.sh.installed
fi
# Now for both install/update add the users and force-store a removal file
echo creating droid users and groups
./droid-user-add.sh
cp -f droid-user-remove.sh droid-user-remove.sh.installed

# HACK : Now ensure default user is in graphics group
groupadd-user graphics

%post sailfish-config

if [ "$1" -eq 1 ]; then
    echo Adding oneshot
    %{_bindir}/add-oneshot --user oneshot-rotation
fi

%files
%defattr(-,root,root,-)
# hybris and /dev/alog/ libraries
%{_libexecdir}/droid-hybris/system/
# just /dev/alog/ libraries (for trying to run pure android apps)
%{_libexecdir}/droid-hybris/lib-dev-alog/.
/sbin/*
# move the .rc files to %%{_sysconfdir}/droid-init if possible
%attr(644, root, root) /*.rc
# Can this move?
%attr(644, root, root) /default.prop
# This binary should probably move to /sbin/
%{_unitdir}
/lib/udev/rules.d/*
%{_libdir}/droid/droid-user-add.sh
%{_libdir}/droid/droid-user-remove.sh
%{_libdir}/droid/apply-permissions
# Created in %%post
%ghost %attr(755, root, root) %{_libdir}/droid/droid-user-remove.sh.installed
%{_sharedstatedir}/environment/compositor/mako-cm10.1.conf
%{_sharedstatedir}/environment/nemo/99-hybris.conf

%files devel
%defattr(-,root,root,-)
%{_libdir}/droid-devel/

%files sailfish-config
%defattr(-,root,root,-)
%attr(755,root,root) %{_oneshotdir}/oneshot-rotation