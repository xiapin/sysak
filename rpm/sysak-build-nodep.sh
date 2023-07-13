#!/bin/bash

build_rpm()
{
	rm -rf ${RPMBUILD_DIR}/BUILD
	rm -rf ${RPMBUILD_DIR}/RPMS
	rm -rf ${RPMBUILD_DIR}/SOURCES
	rm -rf ${RPMBUILD_DIR}/SPECS
	rm -rf ${RPMBUILD_DIR}/SRPMS
	rm -rf ${RPMBUILD_DIR}/BUILDROOT
	local RPMBUILD_DIR="`realpath $BASE/../rpm`"
	local BUILD_DIR=`realpath $BASE/../out`
	local SOURCE_DIR=`realpath $BASE/../`
	mkdir -p "${RPMBUILD_DIR}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
echo "cat"
cat > $RPMBUILD_DIR/sysak.spec <<EOF
%global __filter_GLIBC_PRIVATE 1
%define __requires_exclude libc.so.6(GLIBC_PRIVATE)
Name: sysak
Summary: system analyse kit
Version: ${RPM_VERSION}
Release: ${RELEASE}
License: MulanPSL2

%description
system analyse kit
commit: $COMMIT_ID

%build
echo source_dir=%{source_dir}
if [ %{source_dir} ]; then
	echo linux_version=%{linux_version}
	cd %{source_dir} && make dist_clean
	for version in %{linux_version}; do
		cd %{source_dir} && ./configure %{target} --kernel=\$version
		make clean_middle
		make
	done
fi

%install
mkdir -p \$RPM_BUILD_ROOT/usr/bin
mkdir -p \$RPM_BUILD_ROOT/etc/sysak
mkdir -p \$RPM_BUILD_ROOT/usr/local/sysak/log
mkdir -p \$RPM_BUILD_ROOT/usr/lib/systemd/system/
/bin/cp -rf $BUILD_DIR/.sysak_components \$RPM_BUILD_ROOT/usr/local/sysak/.sysak_components
/bin/cp -rf $BUILD_DIR/sysak \$RPM_BUILD_ROOT/usr/bin/
/bin/cp -f $BUILD_DIR/.sysak_components/tools/monitor/sysakmon.conf \$RPM_BUILD_ROOT/usr/local/sysak
/bin/cp -f $BUILD_DIR/.sysak_components/tools/monitor/monctl.conf \$RPM_BUILD_ROOT/usr/local/sysak
/bin/cp -f $BUILD_DIR/.sysak_components/tools/dist/app/etc/* \$RPM_BUILD_ROOT/etc/sysak/
/bin/cp $SOURCE_DIR/rpm/sysak.service \$RPM_BUILD_ROOT/usr/lib/systemd/system/
/bin/cp $SOURCE_DIR/rpm/sysak_server.conf \$RPM_BUILD_ROOT/usr/local/sysak/

%preun
systemctl stop sysak
/sbin/lsmod | grep sysak > /dev/null
if [ \$? -eq 0 ]; then
	/sbin/rmmod sysak
	exit 0
fi

%postun
rm -rf /usr/local/sysak

%files
/etc/sysak
/usr/local/sysak
/usr/bin/sysak
/usr/lib/systemd/system/sysak.service

%changelog
EOF

echo "rpmbuild"
echo RPMBUILD_DIR=$RPMBUILD_DIR
echo LINUX_VERSION=$LINUX_VERSION
echo SOURCE_DIR=$SOURCE_DIR
rpmbuild --define "%linux_version $LINUX_VERSION" \
	 --define "%_topdir ${RPMBUILD_DIR}"       \
	 --define "%source_dir $SOURCE_DIR" \
	 --define "%target $TARGET_LIST" \
	 --define "%global __filter_GLIBC_PRIVATE 1" \
	 --define "%define __requires_exclude libc.so.6(GLIBC_PRIVATE)" \
	 -bb $RPMBUILD_DIR/sysak.spec
}

main() {
	export BASE=`pwd`
	export RPM_VERSION=$1
	export RELEASE=$2

	export LINUX_VERSION=$(uname -r)

	TARGET_LIST="--enable-target-all"

	build_rpm
}

main $1 $2
