#!/bin/sh
TOOLS_ROOT="$SYSAK_WORK_PATH/tools"
PYTHON_PATH="/usr/bin/python"

if [ -f "/usr/bin/python" ];then
    PYTHON_PATH="/usr/bin/python"
elif [ -f "/usr/bin/python3" ];then
    PYTHON_PATH="/usr/bin/python3"
elif [ -f "/usr/bin/python2" ];then
    PYTHON_PATH="/usr/bin/python2"
fi

$PYTHON_PATH  $TOOLS_ROOT/ossre_set/ossre.py $*
