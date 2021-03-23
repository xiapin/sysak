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

cat > $RPMBUILD_DIR/sysak.spec <<EOF
Name: sysak
Summary: system analyse kit
Version: ${RPM_VERSION}
Release: ${RELEASE}%{?dist}
License: GPLv3+

%description
system analyse kit
commit: $COMMIT_ID

%build
echo source_dir=%{source_dir}
if [ %{source_dir} ]; then
	echo linux_version=%{linux_version}
	for version in %{linux_version}; do
		make -C %{source_dir} KERNEL_VERSION=\$version clean_middle
		make -C %{source_dir} KERNEL_VERSION=\$version
	done
fi

%install
mkdir -p \$RPM_BUILD_ROOT/usr/local/sbin
/bin/cp -rf $BUILD_DIR/.sysak_compoents \$RPM_BUILD_ROOT/usr/local/sbin/.sysak_compoents
/bin/cp -rf $BUILD_DIR/sysak \$RPM_BUILD_ROOT/usr/local/sbin/

%preun

/sbin/lsmod | grep sysak > /dev/null
if [ $? -eq 0 ]; then
	/sbin/rmmod sysak
	exit 0
fi

%files
/usr/local/sbin/.sysak_compoents
/usr/local/sbin/sysak

%changelog
EOF

echo RPMBUILD_DIR=$RPMBUILD_DIR
echo LINUX_VERSION=$LINUX_VERSION
echo SOURCE_DIR=$SOURCE_DIR
rpmbuild --define "%linux_version $LINUX_VERSION" \
	 --define "%_topdir ${RPMBUILD_DIR}"       \
	 --define "%source_dir $SOURCE_DIR" \
	 -bb $RPMBUILD_DIR/sysak.spec
}

ALL_SYS_VERSIONS="4.19.91-008.ali4000.alios7.x86_64 \
		 3.10.0-327.ali2016.alios7.x86_64 \
		 3.10.0-327.ali2014.alios7.x86_64 \
		 3.10.0-1160.11.1.el7.x86_64"

main() {
	export BASE=`pwd`
	export RPM_VERSION=$1
	export RELEASE=$2

	if [ -z $SYSTEM_SUPPORT ]; then
		export LINUX_VERSION=$ALL_SYS_VERSIONS
	else
		export LINUX_VERSION=$SYSTEM_SUPPORT
	fi

	build_rpm
}

main $3 $4