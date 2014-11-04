#!/bin/bash -xe

#AJ_BASEDIR=../alljoyn-suite-14.06.00a-src
#AJ_LOCDIR=services/base/notification
AJ_BASEDIR=../allseen
AJ_LOCDIR=gateway/gwagent

cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/cpp/lib/liballjoyn.so .
cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/services_common/lib/liballjoyn_services_common.so .
#cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/about/lib/liballjoyn_about.so .
cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/cpp/lib/liballjoyn_about.so .
cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/notification/lib/liballjoyn_notification.so .
cp $AJ_BASEDIR/$AJ_LOCDIR/build/linux/x86/debug/dist/gatewayConnector/lib/liballjoyn_gwConnector.so .

