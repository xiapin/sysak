#!bash

for((i=0;i<10000;i++));
do
    stress -c 16 -i 16 -t 1
done
