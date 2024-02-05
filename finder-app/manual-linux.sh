#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    #Deep clean the kernel build tree, removing .config file

    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper

    #Configure for our virt arm dev board(simuate in QMEU)
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig

    #Build a kernel image for booting with QEMU
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all

    #Build any kernel modules
    #make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules

    #Build the devicetree
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd ${OUTDIR}
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
#Creating a folder tree
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd "${OUTDIR}/rootfs"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs

#copy required files from sysroot to the lib directory
interpreter=$(find $SYSROOT -name "ld-linux-aarch64.so.1")
#cd /home/akka2103/downloads/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib
cp "$interpreter" "${OUTDIR}/rootfs/lib64"
sharedlib1=$(find $SYSROOT -name "libm.so.6")
cp "$sharedlib1" "${OUTDIR}/rootfs/lib64"
sharedlib2=$(find $SYSROOT -name "libresolv.so.2")
cp "$sharedlib2" "${OUTDIR}/rootfs/lib64"
sharedlib3=$(find $SYSROOT -name "libc.so.6")
cp "$sharedlib3" "${OUTDIR}/rootfs/lib64"
#cd /home/akka2103/downloads/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64
echo "yoo"

cd "${OUTDIR}/rootfs"

# TODO: Make device nodes
#create a dev null device
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3

#create dev console fevice
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/tty c 5 1

# TODO: Clean and build the writer utility
# Change to the directory where fibder/writer is located
cd ${FINDER_APP_DIR}

# Clean the project
make clean

# Build the project
make CROSS_COMPILE=${CROSS_COMPILE}

# Create the output directory if it doesn't exist
mkdir -p ${OUTDIR}/rootfs/home

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cd ${FINDER_APP_DIR}
cp finder* writer* "${OUTDIR}/rootfs/home/"

cd ${FINDER_APP_DIR}/conf/
cp username.txt assignment.txt "${OUTDIR}/rootfs/home/"

# Copy the autorun-qemu.sh script into the outdir/rootfs/home directory
cd ${FINDER_APP_DIR}
cp autorun-qemu.sh "${OUTDIR}/rootfs/home/"

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs/
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
echo "here"
cd ${OUTDIR}
gzip -f initramfs.cpio
echo "run successful"
