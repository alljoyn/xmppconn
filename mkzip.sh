#!/bin/bash -xe

# This script creates a tar.gz file that contains the source for building the XMPPConnector
# utility. This file can be served via HTTP so that, for instance, the OpenWRT build tools
# can pull and build it.

INPUT_FILES='*.h *.cpp Makefile *.conf *.init'
STAGING_FOLDER=XMPPConnector
OUTPUT_FOLDER=output
OUTPUT_FILE=XMPPConnector.tar.gz

rm -rf $STAGING_FOLDER
mkdir -p $STAGING_FOLDER
rm -rf $OUTPUT_FOLDER
mkdir -p $OUTPUT_FOLDER
cp -rf $INPUT_FILES $STAGING_FOLDER/
tar -zcvf $OUTPUT_FOLDER/$OUTPUT_FILE $STAGING_FOLDER
rm -rf $STAGING_FOLDER

