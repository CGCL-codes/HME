#!/bin/bash
#################################################################
#Copyright remains with CGCL & SCTS Lab of Huazhong University of Science and Technology.
#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation; either version 2 of the License, or (at
#your option) any later version. This program is distributed in the
#hope that it will be useful, but WITHOUT ANY WARRANTY; without even
#the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#PURPOSE. See the GNU General Public License for more details. You
#should have received a copy of the GNU General Public License along
#with this program; if not, write to the Free Software Foundation,
#Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#################################################################

PAPI_MAJOR=5
PAPI_MINOR=1
PAPI_RELEASE=1

CMAKE_MAJOR=2
CMAKE_MINOR=8

install_deps_rpm() {
    yum install -q -y numactl-devel libconfig libconfig-devel kernel-devel-`uname -r` msr-tools numactl
    if [ $? -ne 0 ]; then
        echo "Dependencies installation failed"
        exit -1
    fi
}

install_deps_deb() {
    apt-get install -y libnuma-dev libconfig-dev msr-tools numactl
    apt-get install fakeroot build-essential crash kexec-tools makedumpfile kernel-wedge
    apt-get build-dep linux
    apt-get install git-core libncurses5 libncurses5-dev libelf-dev asciidoc binutils-dev 
    apt-get update
    apt-get install linux-headers-$(uname -r)

    if [ $? -ne 0 ]; then
        echo "Dependencies installation failed"
        exit -1
    fi
}

#################### MAIN ####################

if [ $(id -u) -ne 0 ]; then
   echo "You mut be root to execute this script"
   exit -1
fi

if [ -f /etc/redhat-release ]; then
    install_deps_rpm
elif [ -f /etc/centos-release ]; then
    install_deps_rpm
elif [ -f /etc/debian_version -o -f /etc/debian-release ]; then
    install_deps_deb
else
    echo "Linux distribution not supported"
    exit -1
fi

