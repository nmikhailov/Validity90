#!/bin/sh

pid=`ps -W | grep WUDFHost | tail -n 1 | awk '{ print $1 }'`
echo Attaching to $pid

si2cce.exe cdb.exe -p $pid -c '$$>a< tracer2.txt init;g'
#si2cce.exe cdb.exe -p $pid