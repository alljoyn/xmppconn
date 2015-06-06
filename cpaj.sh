#!/bin/bash -xe

AJ_BASEDIR=../alljoyn_src
AJ_LOCDIR=gateway/gwagent

if [ "$1" = "" ]; then
  OUTDIR=build
else
  OUTDIR=$1
fi

cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/cpp/lib/liballjoyn.so $OUTDIR/
cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/services_common/lib/liballjoyn_services_common.so $OUTDIR/
cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/cpp/lib/liballjoyn_about.so $OUTDIR/
cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/notification/lib/liballjoyn_notification.so $OUTDIR/
cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/gatewayConnector/lib/liballjoyn_gwconnector.so $OUTDIR/

