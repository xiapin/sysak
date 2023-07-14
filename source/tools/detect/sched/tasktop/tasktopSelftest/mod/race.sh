#!bash

for((i=0;i<10;i++));
do
    cat /proc/demo_mutex &
done
