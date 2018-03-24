#!/bin/bash

TARGET=Test@192.168.56.101
CDB_EXE="/cygdrive/c/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe"
DEFAULT_NAME=log-$(date --iso-8601=seconds).txt
SCRIPT_NAME=$(basename "$0")
REG_KEY="HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\WUDF\Services\{193a1820-d9ac-4997-8c55-be817523f6aa}"
VM_NAME=Win10

if [[ "$1" == "step2" ]]; then
	cd "$2"

	pid=`ps -W | grep WUDFHost | tail -n 1 | awk '{ print $1 }'`
	echo Attaching to $pid

	si2cce.exe "${CDB_EXE}" -p $pid -c '$$>a< tracer.txt init;g'
	#si2cce.exe cdb.exe -p $pid
	#si2cce.exe "${CDB_EXE}" -c '.logopen log.txt' notepad
elif [[ "$1" == "attach" ]]; then
	TARGET_DIR=$(ssh $TARGET mktemp -d)
	#scp -r "$SCRIPT_NAME" tracer.txt $TARGET:$TARGET_DIR
	rsync -avP "$SCRIPT_NAME" tracer.txt $TARGET:$TARGET_DIR

	if [[ "$2" == "-r" ]]; then
		ssh $TARGET "reg add \"$REG_KEY\" /v HostProcessDbgBreakOnStart /t REG_DWORD /d 5 /f"

		device=$(VBoxManage list usbhost | grep -A 7 0x138a | tail -n 1 | awk '{ print $2 }')

		echo "Detaching device..."
		VBoxManage controlvm ${VM_NAME} usbdetach "$device" 2> /dev/null

		sleep 2

		echo "Attaching device..."
		VBoxManage controlvm ${VM_NAME} usbattach "$device"

		sleep 2
		ssh $TARGET "reg add \"$REG_KEY\" /v HostProcessDbgBreakOnStart /t REG_DWORD /d 0 /f"
	fi

	ssh -t $TARGET "$TARGET_DIR/$SCRIPT_NAME" step2 "$TARGET_DIR"

	LOG=$(ssh $TARGET cat "$TARGET_DIR/log.txt")

	echo Cleanup
	ssh $TARGET rm -rf $TARGET_DIR

	read -e -p 'Save to: ' -i "$DEFAULT_NAME" FILENAME
	echo $LOG > "../logs/$FILENAME"
else
	echo "Usage: $SCRIPT_NAME attach [-r]"
fi
