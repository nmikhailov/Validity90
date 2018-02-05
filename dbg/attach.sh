#!/bin/bash

TARGET=Test@192.168.56.101
CDB_EXE="/cygdrive/c/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe"
DEFAULT_NAME=log-$(date --iso-8601=seconds).txt

if [[ "$1" == "step2" ]]; then
	cd "$2"

	pid=`ps -W | grep WUDFHost | tail -n 1 | awk '{ print $1 }'`
	echo Attaching to $pid

	si2cce.exe "${CDB_EXE}" -p $pid -c '$$>a< tracer.txt init;g'
	#si2cce.exe cdb.exe -p $pid
	#si2cce.exe "${CDB_EXE}" -c '.logopen log.txt' notepad
else
	TARGET_DIR=$(ssh $TARGET mktemp -d)
	#scp -r attach.sh tracer.txt $TARGET:$TARGET_DIR
	rsync -avP attach.sh tracer.txt $TARGET:$TARGET_DIR

	ssh -t $TARGET $TARGET_DIR/attach.sh step2 "$TARGET_DIR"

	LOG=$(ssh $TARGET cat "$TARGET_DIR/log.txt")

	echo Cleanup
	ssh $TARGET rm -rf $TARGET_DIR

	read -e -p 'Save to: ' -i "$DEFAULT_NAME" FILENAME
	echo $LOG > "../logs/$FILENAME"
fi
