#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/
<<<<<<< HEAD
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./lib/
=======
>>>>>>> 4fc81cf12d7ab706db155b212008392481ed8c4b
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../tsdb/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../collector/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../beaver/
source /etc/profile

cd ../../beeQ
make
if [ $? -ne 0 ];then
	echo " make  -- Failed  : "$?
	exit 0
fi

./unity-mon
