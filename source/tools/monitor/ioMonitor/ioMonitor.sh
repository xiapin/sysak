#!/bin/sh
#****************************************************************#
# ScriptName: ioMonitor.sh
# Author: $SHTERM_REAL_USER@alibaba-inc.com
# Create Date: 2021-06-06 16:53
# Modify Author: $SHTERM_REAL_USER@alibaba-inc.com
# Modify Date: 2021-06-06 16:53
# Function:
#***************************************************************#
TOOLS_ROOT="$SYSAK_WORK_PATH/tools"
python $TOOLS_ROOT/ioMon/ioMonitorMain.py $*
