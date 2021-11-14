#!/bin/bash

# Command line args to the install_vertica script
VERTICA=/opt/vertica
DBA_USER=verticadba
DBA_GROUP=verticadba
DBA_USER_HOME=/uusoc/env/verticadba_home

# IMPORTANT!!
# These are the a few configurations my local environment needed for vertica
# installation. They are all disabled by default to not accidentally change the
# OS configuration we don't want to change. Set them to ``t'' to enable the
# change.
SET_CPUFREQ_SCALING_POLICY=f
SET_VM_SWAPPINESS=f
CREATE_NEW_SWAPFILE=f
# $SWAPFILE is also used as a regex in grep, so don't use special characters.
SWAPFILE=/newswapfile 
INSTALL_AND_START_NTP=f
SET_IO_SCHEDULER_TO_DEADLINE=f
SET_DISK_READAHEAD=f

if [ "`whoami`" != root ]; then
	echo "setup_vertica.sh must be run as root"
	exit 1
fi

if [ ! -x "$VERTICA/sbin/install_vertica" ]; then
	echo "vertica package is not installed yet. Run dpkg -i <path-to-deb-package> first."
	exit 1
fi

echo "The setup_vertica.sh script is only tested on Ubuntu 18.04."
echo "The script does not change your OS configuration by default."
echo "However, if install_vertica scripts complains about your configuration,"
echo "we provided a few switches to apply the changes we did to our system."
echo "See the line 9 for details."
echo "It is not tested on other Ubuntu or debian systems so use caution."
echo "Some of the changes are unlikely to work on non-debian systems."
echo ""

# The installer would complain without cpu frequency scaling policy set to
# performance.
# https://www.vertica.com/docs/10.0.x/HTML/index.htm#cshid=S0140
if [ $SET_CPUFREQ_SCALING_POLICY = t ] ; then
	echo "Setting CPU frequency scaling policy"
	for i in `ls -l /sys/devices/system/cpu/ | awk '{ print $9; }' |
			  grep 'cpu[0-9][0-9]*'`; do
		echo "performance" > /sys/devices/system/cpu/$i/cpufreq/scaling_governor
	done
fi

# The installer warns about vm.swappiness above 1.
# https://www.vertica.com/docs/10.0.x/HTML/index.htm#cshid=S0112
if [ $SET_VM_SWAPPINESS = t ]; then
	echo "Setting vm.swappiness"
	echo 1 > /proc/sys/vm/swappiness
fi

# The installer requires at least 2 GB swap space. This is commented out by
# default so that you don't create multiple swap files.
# https://www.vertica.com/docs/10.0.x/HTML/index.htm#cshid=S0180
if [ $CREATE_NEW_SWAPFILE = t ]; then
	if [ `swapon --show | grep "$SWAPFILE" | wc -l` -ne 0 ]; then
		echo "$SWAPFILE already exists as a swap file"
		echo "call 'swapoff $SWAPFILE' and remove it first if you'd like to recreate it"
	else
		if [ -e $SWAPFILE ]; then
			echo "$SWAPFILE already exists"
			echo "remove it first if you'd like to recreate it"
		else	
			echo "creating new swap file"
			fallocate -l 2G $SWAPFILE
			chmod 600 $SWAPFILE
			mkswap $SWAPFILE
			swapon $SWAPFILE
		fi
	fi
fi

# The installer requires either of ntp/ntpd/chrony to be running.
# For ubuntu, we just need to install the ntp package and start it.
# https://www.vertica.com/docs/10.0.x/HTML/index.htm#cshid=S0030
if [ $INSTALL_AND_START_NTP = t ]; then
	if dpkg -l ntp &> /dev/null ; then
		echo "ntp already installed"
	else
		echo "installing ntp"
		apt install ntp
	fi	
	echo "starting ntp"
	service ntp start
fi

# The installer requires the I/O scheduler to be either noop or deadline.
# https://www.vertica.com/docs/10.0.x/HTML/index.htm#cshid=S0150
if [ $SET_IO_SCHEDULER_TO_DEADLINE = t ]; then
	if [ ! -d "$DBA_USER_HOME" ]; then
		echo "The dba user home does not exist yet."
		echo "Set SET_IO_SCHEDULER_TO_DEADLINE to f before that is created by install_vertica script."
	else
		echo "Setting I/O scheduler to deadline"
		DEVNAME=$(lsblk -no pkname `df "$DBA_USER_HOME" | awk 'END{print $1;}'`)
		echo "Device name = $DEVNAME"
		echo "deadline" > /sys/block/$DEVNAME/queue/scheduler
	fi
fi

# The installer requires the disk readahead to be at least 2048
# https://www.vertica.com/docs/10.0.x/HTML/index.htm#cshid=S0150
if [ $SET_DISK_READAHEAD = t ]; then
	if [ ! -d "$DBA_USER_HOME" ]; then
		echo "The dba user home does not exist yet."
		echo "Set SET_DISK_READAHEAD to f before that is created by install_vertica script."
	else
		echo "Setting disk readahead to 2048"
		DEVNAME=$(lsblk -no pkname `df "$DBA_USER_HOME" | awk 'END{print $1;}'`)
		echo "Device name = $DEVNAME"
		/sbin/blockdev --setra 2048 /dev/$DEVNAME
	fi
fi

"$VERTICA/sbin/install_vertica" --dba-user $DBA_USER --dba-group $DBA_GROUP \
	--dba-user-home $DBA_USER_HOME --hosts localhost
