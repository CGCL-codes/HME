#	Copyright (C) 2015-2016 Yizhou Shan <shanyizhou@ict.ac.cn>
#
#	This program is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 of the License, or
#	(at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License along
#	with this program; if not, write to the Free Software Foundation, Inc.,
#	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

obj-m    := uncore.o
obj-m    += core.o

# composite core pmu
core-y   := core_pmu.o
core-y   += core_proc.o

# composite uncore pmu
uncore-y := uncore_pmu.o
uncore-y += uncore_imc.o
uncore-y += uncore_proc.o
uncore-y += uncore_hswep.o
uncore-y += emulate_nvm.o
uncore-y += emulate_nvm_proc.o

KERNEL_VERSION = /lib/modules/$(shell uname -r)/build/

all:
	make -C $(KERNEL_VERSION) M=$(PWD) modules
clean:
	make -C $(KERNEL_VERSION) M=$(PWD) clean
help:
	make -C $(KERNEL_VERSION) M=$(PWD) help
