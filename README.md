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
* RapidJSON

Optionally, if you are building xmppconn to be used with the AllJoyn Gateway Agent you need to obtain and install the following:

* AllJoyn Gateway Agent

### Building

First set up the environment. Make sure to change the CPU value to the correct value for your system (e.g. x86\_64, arm, etc.). Also set the OS to the correct value if necessary. If you would like to build the debug version make sure you change the VARIANT value from release to debug.

    export CPU=x86
    export OS=linux
    export VARIANT=release
    export ROOTPATH=~/src
    mkdir -p $ROOTPATH

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

If you don't want to install libstrophe you may skip the _sudo make install_ step above and set the LIBSTROPHE_INCDIR and LIBSTROPHE_LIBDIR environment variables to the location of your include and library folder paths.

#### RapidJSON

It is necessary to download the RapidJSON source code (building is not necessary since the library is header-only). Source code can be downloaded from https://github.com/miloyip/rapidjson.git. After downloading, copy the rapidjson include folder to your /usr/include folder (assuming the RapidJSON source code folder is RAPIDJSON\_ROOT):

    sudo cp -r $RAPIDJSON_ROOT/include/rapidjson /usr/include/

If you don't want to install the RapidJSON include files in your /usr/include/ folder you may instead set the RAPIDJSON_INCDIR environment variable to be the path to the folder that contains the rapidjson include folder.
    
#### AllJoyn Core and Base Services

Pull the source code for AllJoyn Core and Base Services as follows: 

    export ALLJOYN_ROOT=$ROOTPATH/alljoyn_src
    mkdir -p $ALLJOYN_ROOT
    cd $ALLJOYN_ROOT
    mkdir -p core services gateway
    cd $ALLJOYN_ROOT/core
    git clone https://git.allseenalliance.org/gerrit/core/alljoyn
    cd alljoyn
    git checkout RB15.04
    cd $ALLJOYN_ROOT/services
    git clone https://git.allseenalliance.org/gerrit/services/base
    cd base
    git checkout RB15.04

#### AllJoyn Gateway Agent (optional)

Pull the source code for the AllJoyn Gateway Agent as follows: 

    cd $ALLJOYN_ROOT/gateway
    git clone https://git.allseenalliance.org/gerrit/gateway/gwagent
    cd gwagent
    git checkout RB15.04

#### xmppconn

Pull the source code from the repository into the xmppconn folder under $ALLJOYN_ROOT/gateway, and run scons:

    cd $ALLJOYN_ROOT/gateway
    git clone https://bitbucket.org/affinegy/xmppconn.git
    cd xmppconn
    scons BINDINGS=cpp OS=$OS CPU=$CPU VARIANT=$VARIANT WS=off POLICYDB=on USE_GATEWAY_AGENT=off
    mkdir -p $ALLJOYN_ROOT/build/lib
    mkdir -p $ALLJOYN_ROOT/build/bin
    find $ALLJOYN_ROOT/gateway/xmppconn/build -name "*\.so" -exec cp {} $ALLJOYN_ROOT/build/lib/ \;
    find $ALLJOYN_ROOT/gateway/xmppconn/build -type f -name "xmppconn" -exec cp {} $ALLJOYN_ROOT/build/bin/ \;
    export LD_LIBRARY_PATH=$ALLJOYN_ROOT/build/lib

**NOTE:** If you plan to build with the Gateway Agent then your scons command should use `USE_GATEWAY_AGENT=on`.

**NOTE:** If the scons command fails then refer to http://wiki.allseenalliance.org/gateway/getting\_started for more information.

## Installing and Running xmppconn

An AllJoyn routing node must be running on the same local system for this program to work. The next section explains how to make sure this is happening.

Prior to running xmppconn it is necessary to set up a configuration file. That is explained in a section below.

Once the configuration file has been set up it is possible to run xmppconn on your system in two ways:

1. As a normal AllJoyn application
2. As an AllJoyn Gateway Connector application

### Installing and Running the AllJoyn Daemon

An AllJoyn routing node must be running on the same local system for this program to work. The easiest way to do this on a Debian system is to use the AllJoyn core Debian package from the downloads.chariot.global APT repository.

First append the line `deb http://downloads.chariot.global trusty main` to the `/etc/apt/sources.list` file. For example:
    
    echo 'deb http://downloads.chariot.global trusty main' | sudo tee -a /etc/apt/sources.list

Then add the certificate for the downloads.chariot.global apt server to your system:

    sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 96AAA3BF

Now you can update your apt cache and install AllJoyn Core:

    sudo apt-get update
    sudo apt-get install alljoyn-core

If no package exists for your system then you will have to build and install the AllJoyn daemon yourself. Refer to http://wiki.allseenalliance.org/gateway/getting\_started to learn how to do this.

### Setting up xmppconn_factory.conf

Prior to running xmppconn it is necessary to set up the `xmppconn_factory.conf` file. The `xmppconn_factory.conf` file is used to specify the xmppconn configuration of a device at its factory default settings. When xmppconn starts it first looks for the `xmppconn.conf` file, and if it can't find it then it will look for the `xmppconn_factory.conf` file, then copy it to `xmppconn.conf`.

First make a copy of the `xmppconn_factory.conf` file that is found in the conf folder under the the xmppconn source repository:

    mkdir -p $ALLJOYN_ROOT/gateway/xmppconn/build/conf
    cp $ALLJOYN_ROOT/gateway/xmppconn/conf/xmppconn_factory.conf $ALLJOYN_ROOT/gateway/xmppconn/build/conf/

Now open the `$ALLJOYN_ROOT/gateway/xmppconn/build/conf/xmppconn\_factory.conf` file in your favorite editor.

The file looks like the following:

    {
     "ProductID": "Your Product ID",
     "SerialNumber": "Your Serial Number",
     "DeviceName" : "My Device Name",
     "AppName" : "AllJoyn XMPP Connector",
     "Manufacturer" : "My Manufacturer Name",
     "ModelNumber" : "My Model Number",
     "Description" : "Description of my device",
     "DateOfManufacture" : "1970-01-01",
     "SoftwareVersion" : "0.0.1",
     "HardwareVersion" : "0.0.1",
     "SupportUrl" : "http://www.example.org",
     "Verbosity":"2",
     "Compress":"1"
    }

#### About fields

The following fields are announced in the XMPP Connector's About Announcement. These match the AllJoyn About fields with the same names:
- DeviceName
- AppName
- Manufacturer
- ModelNumber
- Description
- DateOfManufacture
- SoftwareVersion
- HardwareVersion
- SupportUrl

#### Optional fields

The following arguments can be optionally modified as needed:

- Verbosity - level of debug output verbosity. Can be 0, 1, or 2, with 2 being the most verbose.
- Compress - whether or not to compress the body of each message. This is recommended.

#### CHARIOT-specific fields

The `ProductId` and `SerialNumber` fields are used to help connect to the CHARIOT service. These fields are used during the pairing sequence with the CHARIOT Join app. It can also be used by other applications and services for the same purpose.

If you are using the CHARIOT service then you should add these fields to your `xmppconn_factory.conf` file with the proper information as follows: 

##### ProductId

The `ProductId` is used during configuration. If you're using the CHARIOT service you will need to get the ProductID from the Developer Portal, located at https://dev.chariot.global. If you don't have an account you can sign up for one and log in. To get a product ID you can create a new product in the Developer Portal.

Paste the ProductID into the ProductID field in the xmppconn\_factory.conf file that you just opened.

##### SerialNumber

To use the CHARIOT service you also need a `SerialNumber` for your device. You can type any alphanumeric string in that field. But be aware that it must be a unique serial number. When you try to register more than one device with the same serial number the CHARIOT Join app will refuse to pair with it.

Save and close the file. You are now ready to connect the xmppconn service with the mobile app.

#### XMPP Connection fields

Several fields are not provided in the default `xmppconn_factory.conf` file but are fundamental to how xmppconn works. These fields specify how xmppconn connects to an XMPP server. When using the CHARIOT service these are usually configured and written to the `xmppconn.conf` file during the pairing procedure by using the AllJoyn Config API. However, they can be specified manually if you have a JID and password for an XMPP server.

*NOTE: If you are using the CHARIOT service you most likely don't want to set these fields.*

##### UserJID

This is the full Jabber ID (JID) that xmppconn uses to connect to the XMPP server. An example of this is `myxmppuser@xmpp.chariot.global/myresource`

##### UserPassword

This is the password used to authenticate to the XMPP server.

##### Server

This is the XMPP server. For example: `xmpp.chariot.global`

##### Roster

The Roster field is used to specify the JIDs of the other XMPP Connector devices with which this device communicates. It is a list of JIDs that this device is willing to accept communication from. When xmppconn receives data from the XMPP server it ignores anything that is not from a JID specified in this list.

    [
      "myotherdevice@muc.xmpp.chariot.global/someresource",
      "yetanotherdevice@muc.xmpp.chariot.global/someotherresource"
    ],

##### RoomJID

This is the Full JID of the XMPP chat room to which this device sends all XMPP communications. For example:
 
    roomname@muc.xmpp.chariot.global/roomresource

### Running as a normal AllJoyn application

When running as a normal AllJoyn application without the Gateaway Agent it is unnecessary for the Gateway Agent to be running. If you would like to run xmppconn as a Gateway Agent Connector app then skip this section. 

First make sure the alljoyn-daemon is running. If the instructions were followed according to the above section the daemon should already exist. It can be started as follows:

    sudo service alljoyn start

Copy the `xmppconn_factory.conf` file to the  `/etc/xmppconn/` folder:

    sudo mkdir -p /etc/xmppconn
    sudo cp $ALLJOYN_ROOT/gateway/xmppconn/build/conf/xmppconn_factory.conf /etc/xmppconn/

Then the xmppconn application can run directly if desired:

    cd $ALLJOYN_ROOT/build/bin
    sudo ./xmppconn

If it is desired that xmppconn be installed do the following:

    $ALLJOYN_ROOT/gateway/xmppconn
    sudo cp build/bin/xmppconn /usr/bin/
    sudo cp conf/xmppconn.init /etc/init.d/xmppconn
    cd /etc/rc3.d
    sudo ln -s ../init.d/xmppconn S95xmppconn

Then you can start the XMPP connector by typing `sudo service xmppconn start`

Check that it is still running by typing `sudo service xmppconn status`. If it is not, then the conf file is possibly invalid. Check the `/var/log/xmppconn.log` file for more information.

### Running as an AllJoyn Gateway Connector application

The previous section described how to run xmppconn as a service via the Linux command line. If you would rather run it as a Gateway Connector application then you can follow these instructions. Otherwise, skip this section.

#### Installation as a Gateway Agent Connector

You need to create a directory structure for the xmppconn app:

    sudo mkdir -p /opt/alljoyn/apps/xmppconn/acls
    sudo mkdir -p /opt/alljoyn/apps/xmppconn/bin
    sudo mkdir -p /opt/alljoyn/apps/xmppconn/lib
    sudo mkdir -p /opt/alljoyn/apps/xmppconn/store
    sudo mkdir -p /opt/alljoyn/apps/xmppconn/etc

Under Gateway Connector, the xmppconn process will be run as "xmppconn" user. It needs to be able to write to the "etc" subdirectory. Since we created the directory structure above as root (sudo), change the owner and the group of that directory:

     sudo chown -R xmppconn /opt/alljoyn/apps/xmppconn
     sudo chgrp -R xmppconn /opt/alljoyn/apps/xmppconn

Note that in the *Building from source* section, we ran the `scons` command to build xmppconn and its dependencies. It is necessary to build with the `USE_GATEWAY_AGENT=on` flag in order to run as a Gateway Connector. If you did not do that then you need to go back to that section and start again from the `scons` command.

Now copy the Manifest file to the top-level xmppconn app directory:

    sudo cp $ALLJOYN_ROOT/gateway/xmppconn/Manifest.xml /opt/alljoyn/apps/xmppconn

The Manifest.xml file has to be modified to allow the xmppconn process to be run as *xmppconn* user. Add the following line after the `<env_variables>` line:

    <variable name="HOME">/home/xmppconn</variable>

Just like the standalone xmppconn, the Gateway Connector app needs a configuration file. Its format is the same as previously described, but you will need to place it in a different location. Copy your xmppconn_factory.conf file from the previous section to:

    /opt/alljoyn/apps/xmppconn/etc/xmppconn_factory.conf

Note that the "store" and "acls" subdirectories will remain empty for now. You are now ready to execute xmppconn as a Gateway Connector app.

#### Running the Gateway Connector

Start the Gateway Agent:

    sudo service alljoyn-gwagent start

Verify that it is running:

    sudo service alljoyn-gwagent status

The instructions for downloading and running the Gateway Connector app are on the AllSeen Alliance website at ["Installing the Gateway Controller Sample Android App"](https://wiki.allseenalliance.org/gateway/getting\_started#installing\_the\_gateway\_controller\_sample\_android\_app). After installing the app, open it and click on AllJoyn Gateway Configuration Manager. You should see "Affinegy XMPP Connector" (a button that says Affin...) in the Gateway Connector Applications list. At this point, the state of the app should show "Stopped". This is because we haven't created any Access Control Lists (ACL's) yet.

#### Creating an ACL

Click on the "Affin..." button to open the XMPP Connector app. Using the context menu on your Android device, click on "Create ACL". This will open up a window where you choose a name for your ACL, and choose which services will be allowed to pass through xmppconn. For now, select the "Expose all services" checkbox, since we want to ensure that the xmpconn app works just as the command-line xmppconn. Click on "Create".

Go back to the previous window (the XMPP Connector app). You will see that it still shows up as "Stopped". First, you need to make sure that the newly created ACL is in the "Active" state. Then, from the Linux command line, restart the Gateway Agent:

    sudo service alljoyn-gwagent restart
    
Verify that it is running:

    sudo service alljoyn-gwagent status
    
*NOTE:* It is possible that the Gateway Agent is not running at this point (you might see the message "The process appears to be dead but pidfile still exists"). If this happens, you will need to restart the AllJoyn service, and then restart the Gateway Agent:

    sudo service alljoyn restart
    sudo service alljoyn-gwagent restart
    
The Gateway Agent should now be running.


#### Verifying the XMPP connector

In the Gateway Connector app, you should now see the XMPP connector status as "Running". On your Linux system, you should also be able to see it from the process list:

    $ ps -ef | grep xmppconn
    xmppconn 26639 26502 16 17:03 ? 00:00:12 [xmppconn]
    alljoyn 29748 9804 0 17:04 pts/0 00:00:00 grep --color=auto xmppconn

You should now be able to use the xmppconn app to make devices communicate across networks, just as with the command-line version. For example, you can use CHARIOT Join to see controller-connector communication on separate networks.


## License

Copyright (c) 2015, Affinegy, Inc.

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
