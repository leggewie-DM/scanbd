#!/bin/bash
# $Id: check_usb_autosuspend.sh 163 2013-01-10 15:51:48Z wimalopaan $
#
#  scanbd - KMUX scanner button daemon
#
#  Copyright (C) 2008 - 2013  Wilhelm Meier (wilhelm.meier@fh-kl.de)
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

# check to see if autosuspend is "off", that is power is "on"

for d in /sys/bus/usb/devices/*; do 
    if [ -e "$d/power/control" -a -e "$d/product" ]; then
	cat "$d/product" 
	cat "$d/power/control"
    fi
done
