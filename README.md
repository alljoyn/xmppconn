## Synopsis

xmppconn is an application that bridges two AllJoyn networks over XMPP. See http://allseenalliance.org for more information about AllJoyn.

## Motivation

Normally an AllJoyn device can only communicate with other AllJoyn devices from within a local network. This application sends AllJoyn traffic over an XMPP connection to another node and effectively bridges the local AllJoyn bus the AllJoyn bus of the remote node. This allows, for instance, a mobile phone to continue to communicate with the local AllJoyn network even when not connected to the local AllJoyn network via WiFi.

This application can be used to connect to any other node that does the same function. For instance, it is possible to use two instances of the program, one on each side, to bridge two AllJoyn networks. It is also possible to use this program on one side, and on the other side another program written in Java for Android that does the same thing.

## Building from Source

Building the source code requires first setting up dependencies and then using normal gnu make to produce the binaries.

### Dependencies

The following dependencies must be obtained and installed:

* libxml2
* libstrophe
* AllJoyn Gateway Agent

First set up the environment. Make sure to change the CPU value to the correct value for your system (e.g. x86\_64, arm, etc.). Also set the OS to the correct value if necessary.

    export CPU=x86
    export OS=linux
    export VARIANT=release
    cd ~
    mkdir -p xmppconn_src
    cd xmppconn_src
    export ROOTPATH=`pwd`

#### libxml2

It is necessary to install libxml2. If you're on Ubuntu you can issue the following commands to install it:

    sudo apt-get update
    sudo apt-get install libxml2-dev

#### libstrophe

It is necessary to install libstrophe. Issue the following commands to install it:

    cd $ROOTPATH
    git clone https://github.com/strophe/libstrophe.git
    cd libstrophe
    ./bootstrap.sh
    ./configure --with-libxml2 --prefix=/usr
    make
    sudo make install

#### AllJoyn Gateway Agent

Pull the source code and build the AllJoyn Gateway Agent as follows. 

    cd $ROOTPATH
    mkdir -p alljoyn_src/core alljoyn_src/services alljoyn_src/gateway
    cd alljoyn_src/core
    git clone https://git.allseenalliance.org/gerrit/core/alljoyn
    cd alljoyn
    git checkout RB14.12b
    cd $ROOTPATH/alljoyn_src/services
    git clone https://git.allseenalliance.org/gerrit/services/base
    cd base
    git checkout RB14.12
    cd $ROOTPATH/alljoyn_src/gateway
    git clone https://git.allseenalliance.org/gerrit/gateway/gwagent
    cd gwagent
    git checkout RB14.12
    export GWAGENT_SRC_DIR=`pwd`
    unset ALLJOYN_DISTDIR
    export VARIANT=debug
    scons V=1 OS=$OS CPU=$CPU BINDINGS=cpp VARIANT=$VARIANT WS=off POLICYDB=on
    export ALLJOYN_DISTDIR=$GWAGENT_SRC_DIR/build/linux/$CPU/$VARIANT/dist
    mkdir -p $ROOTPATH/build/lib
    find $ALLJOYN_DISTDIR -name "*\.so" -exec cp {} $ROOTPATH/build/lib/ \;
    export LD_LIBRARY_PATH=$ROOTPATH/build/lib

**NOTE:** If the scons command fails then refer to http://wiki.allseenalliance.org/gateway/getting\_started for more information.

#### xmppconn

Pull the source code from the repositor into the xmppconn folder under $ROOTPATH.

    cd $ROOTPATH
    git clone https://bitbucket.org/affinegy/xmppconn.git
    cd xmppconn
    make

## Installation

An AllJoyn daemon must be running on the same local system for this program to work. Refer to http://wiki.allseenalliance.org/gateway/getting\_started to learn how to set up your system with the proper AllJoyn dependencies.

Once that is completed it is possible to install this application in two ways:

1. As a normal AllJoyn application
2. As an AllJoyn Gateway Connector application

When running as a normal AllJoyn application without the Gateaway Agent it is unnecessary for the gateway agent to be running. In this case just make sure the alljoyn-daemon is running. If the instructions were followed according to the above Wiki article the daemon should already exist. It can be started as follows:

    sudo service alljoyn start

Then the xmppconn application can run directly if desired:

    cd $ROOTPATH/xmppconn
    build/xmppconn

If it is desired that xmppconn be installed do the following:

    cd $ROOTPATH/xmppconn
    sudo cp build/xmppconn /usr/bin/
    sudo cp conf/xmppconn.init /etc/init.d/xmppconn
    cd /etc/rc3.d
    sudo ln -s ../init.d/xmppconn S95xmppconn

Next set up the configuration file:

    sudo mkdir /etc/xmppconn
    sudo cp conf/xmppconn.conf /etc/xmppconn/xmppconn.conf

The configuration file has a number of arguments. Some must be modified in order to connect to the server properly:

    SERVER - the XMPP server (e.g. xmpp.chariot.global)
    CHATROOM - name of the chat room (if blank, multi user chatrooms are not used)
    USER - username to log in to the server
    PASSWORD - password for the specified user
    RESOURCE - the Jabber resource (the part that comes after the slash after the base JID)
    ROSTER - the Full JabberID of the device that this is paired with
    VERBOSITY - level of debug output verbosity. Can be 0, 1, or 2, with 2 being the most verbose
    COMPRESS - whether or not to compress the body of each message. This is recommended, and must match what the paired device is doing.

After properly setting up the config file xmppconn can be started:

    sudo service xmppconn start


TODO: Install as an AllJoyn Gateway Connector application

## Tests

TODO: Describe and show how to run the tests with code examples.

## Contributors

TODO: Let people know how they can dive into the project.

## License

Copyright (c) 2015, Affinegy, Inc.

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.