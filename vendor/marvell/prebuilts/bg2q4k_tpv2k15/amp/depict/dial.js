//
// This is a Node.js implementation of the DIscovery And Launch (DIAL) server for
// the Depict Frame.  In addition, it provides a proxy server for passing JSON
// "control" messages between the client device and the Depict Frame app.
//
// For details on the DIAL server specification, see:
//     http://www.dial-multiscreen.org/dial-protocol-specification
//
// Author:  Keith McConnell (komcconnell@gmail.com)
// Date:    01/01/2015

var MCAST_ADDR = '239.255.255.250';
var MCAST_PORT = 1900;

var UUID = "5606d81a-8d3d-11e4-b116-123b93f75cba";
var DIAL_VERSION = "1.7";
var DIAL_ST = "urn:dial-multiscreen-org:service:dial:1";
var SSDP_ALL = "ssdp:all";
var MODEL_NAME = "Depict Frame";
var MANUFACTURER = "Arira";

// State machine
var STATE_INITIAL           = 1;
var STATE_DISCONNECTED_WIFI = 2;
var STATE_DISCONNECTED_WAN  = 3;
var STATE_CONNECTED_WIFI    = 4;
var STATE_CONNECTED_WAN     = 5;

var EVENT_CONNECT_WIFI      = 1;
var EVENT_CONNECT_WAN       = 2;
var EVENT_DISCONNECT_WIFI   = 3;
var EVENT_DISCONNECT_WAN    = 4;
var EVENT_STOP              = 5;

var currentState = STATE_INITIAL;

var currentFriendlyName = null;

// Generate a random "friendly" name for this frame
var generateFriendlyName = function() {
    if (currentFriendlyName === null) {
        var uniqueNum = Math.floor(Math.random() * 65535);
        currentFriendlyName = "#Frame " + uniqueNum.toString(16).toUpperCase();
    }
    return currentFriendlyName;
};

// Get firmware version string
var getVersionInfo = function() {
    return configSettings.build;
};

// Server configuration settings
var ConfigSettings = function() {
    this.content_server = "";
    this.build = "0.0";
    // Not currently used by dial.js (used by the upgrade manager)
    this.upgrade_server = "";
    this.interval = "180000";
};

// Auth credentials
var ServiceAuthCredentials = function() {
    this.username = "";
    this.password = "";
};

// Private settings
var PrivateSettings = function() {
    this.doRestart = false;
    this.dial_port = -1;
    this.ssdp_port = -1;
    this.hotspot_port = -1;

    // Pending friendly_name change.  Will only be changed after a successful
    // Wi-Fi connection.
    this.pending_friendly_name = null;

    // Server for the Frame 'receiver' web app
    this.frame_receiver_server = null;

    // Secure server info for reporting each running Frame, etc.
    this.depict_registration_server = null;
    this.depict_registration_server_auth = null;
};

// User-settable
var Settings = function() {
    this.friendly_name = generateFriendlyName();
    this.orientation = 'landscape';
    this.network = {ssid : "", password : "", valid : false};
    this.version = getVersionInfo();
    this.public_casting = false;
    this.frame_owner = {username : "", user_token : "", ipad_token: "", iphone_token: ""};
};

// Global settings
var GlobalSettings = function() {
    this.power = "up";
    this.brightness = 100;
    this.contrast = 50;
};

var privSettings = null;

// Will be initialized properly later
var settings = null;

var global_settings = null;

// We need to periodically re-register with the Depict proxy server
var REGISTRATION_REFRESH_INTERVAL_MSEC = 4 * 3600 * 1000;
var RANDOM_INTERVAL_MAX_MSEC = 10 * 60 * 1000;

var depictFrame = {
    state : "stopped",
    proc : null,
    allowStop : "true",
    additionalData : ""
}

// Client Device Types
var IPHONE = 1;
var IPAD = 2;

// For testing only
var youTube = {
    state : "stopped",
    proc : null,
    allowStop : "true",
    additionalData : ""
}

var dgram = require("dgram");
var udpMcastServer = null;

var express = require('express'),
    app = express(),
    discovery = express(),
    hotspot = express(),
    https = require('https'),
    http = require('http'),
    querystring = require('qs'),
    body_parser = require('body-parser'),
    path = require('path'),
    dns = require('dns'),
    fs = require('fs');

var os = require('os');

var server = http.createServer(app);
var dialServer = http.createServer(discovery);
var hotspotServer = http.createServer(hotspot);

var appServerStarted = false;
var dialServerStarted = false;
var hotspotServerStarted = false;
var io = null;

var startNewPlayer = false;

var player_status = '{}';

// serve receiver locally
var loadReceiver = function(req, res) {
  var options = {
    hostname: configSettings.content_server,
    port: 443,
    path: "/frame_receiver",
    method: "GET"
  };

  // var options = {
  //   host: "10.0.1.11",
  //   port: 3000,
  //   path: "/frame_receiver",
  //   method: "GET"
  // };

  LogInfo('options in load receiver: ' + JSON.stringify(options));

  var content = "";
  var request = https.request(options, function(response) {

    response.setEncoding("utf8");
    response.on("data", function (chunk) {
        content += chunk;
    });
    response.on("end", function () {
        res.writeHeader(200, {"Content-Type": "text/html"});
        res.write(content);
        res.end();
    });
  });

  request.on("error", function(e) {
    LogErr("Unable to load receiver app: " + e.message);
    res.writeHeader(502);
    res.end();
  });

  request.end();
}

app.get('/receiver', loadReceiver);


const
    child_process = require('child_process');


//=============================================================================
//
// Utility functions
//
//=============================================================================

var isAndroid = process.platform === 'android' ? true : false;

var frameSettingsFile  = isAndroid ? "/sdcard/frame_server/settings" : "/tmp/frame_settings";
var globalSettingsFile  = isAndroid ? "/sdcard/frame_server/global_settings" : "/tmp/global_settings";
var networksFile  = isAndroid ? "/sdcard/frame_server/networks.json" : "/tmp/networks.json";

var credentialsFile = isAndroid ? "/data/misc/depict/credentials.json" : "/tmp/credentials.json";
var configFile = isAndroid ? "/data/misc/depict/config.json" : "/tmp/config.json";

var serviceAuthCredentials;
var configSettings;

var localIpAddress = function() {
    var address = "0.0.0.0";
    var ifaces = os.networkInterfaces();
    Object.keys(ifaces).forEach(function (ifname) {
        ifaces[ifname].forEach(function (iface) {
            if (iface.family == 'IPv4' && iface.internal === false) {
                // Return the first non-internal (i.e. 127.0.0.1) non-ipv4
                // addresses
                address = iface.address;
                return;
            }
        });
    });
    return address;
};

// For detecting changes in IP address
var prevIpAddress = localIpAddress();

var localMacAddress = function() {
    // NOTE: On Android, it is possible that this server is started before
    // networking is completely up!  Therefore, we must detect that case and
    // check again later assuming that networking eventually comes up.

    var macAddr = "00:00:00:00:00:00";
    var ifaces = os.networkInterfaces();
    Object.keys(ifaces).forEach(function (ifname) {
        ifaces[ifname].forEach(function (iface) {
            if (iface.family == 'IPv4' && iface.internal === false && iface.mac !== undefined) {
                // Return the first non-internal (i.e. 127.0.0.1) non-ipv4
                // addresses
                macAddr = iface.mac;
                return;
            }
        });
    });
    return macAddr;
};

var getTime = function() {
    return new Date().toISOString();
}

// ERROR, WARN and INFO messages
var logLevel = 3;

var LogErr = function(msg) {
    console.log("[DIAL] [ERROR] " + getTime() + ": " + msg);
}

var LogWarn = function(msg) {
    if (logLevel >= 2) {
        console.log("[DIAL] [WARN ] " + getTime() + ": " + msg);
    }
}

var LogAjax = function(command, request) {
    if (logLevel >= 2) {
        console.log("[DIAL] [AJAX] " + getTime() + ": Got command " + command);
        console.log(request.body);
        console.log('stringified sent to receiver: ');
        console.log(JSON.stringify(request.body));
    }
}

var LogInfo = function(msg) {
    if (logLevel >= 3) {
        console.log("[DIAL] [INFO ] " + getTime() + ": " + msg);
    }
}

var LogDebug = function(msg) {
    if (logLevel >= 4) {
        console.log("[DIAL] [DEBUG] " + getTime() + ": " + msg);
    }
}


var getReceiverUrl = function() {
    var url = 'http://' + localIpAddress() + ':' + privSettings.dial_port + '/receiver';
    //var url = 'http://54.187.228.150:3000/receiver';
    return url;
}

var readServerCredentials = function(settings) {
    // Read server credentials
    try {
        var credentials_data = fs.readFileSync(credentialsFile, "utf-8");
        var json = JSON.parse(credentials_data);
        try {
            settings.username = json.username;
            settings.password = json.password;
         } catch(e) {
            LogErr('\n\nError reading credentials from file ' + credentialsFile +'!\n\n');
        }
    } catch(e) {
        LogErr('\n\nERROR reading credentials from file ' + credentialsFile +'!\n\n');
        settings = {username : "", password : ""};
    }
}

// Read server configuration settings / build number
var readConfigSettings = function(settings) {
    try {
        var data = fs.readFileSync(configFile);
        var json = JSON.parse(data);
        try {
            settings.content_server = json.content_server;
            settings.build = json.build;
            // NOTE: Ignoring other config settings intended for upgrade manager
        } catch (e) {
            LogErr("Error reading file: " + configFile + " Bad Data!");
        }
    } catch (e) {
        LogErr("Error reading file: " + configFile + " Bad Data!");
    }
}

// Read user settings from settings file.  If the file doesn't currently
// exist, create a new one with default settings.
var readUserSettings = function(settings) {
    try {
        var data = fs.readFileSync(frameSettingsFile);
        var json = JSON.parse(data);

        try {
            if (json.friendly_name) {
                settings.friendly_name = json.friendly_name;
            }
            if (json.network) {
                settings.network = json.network;
            }
            if (json.orientation) {
                settings.orientation = json.orientation;
            }
            if (json.public_casting != undefined)  {
                settings.public_casting = json.public_casting;
            }
            if (json.frame_owner) {

                LogDebug('init json', json);

                settings.frame_owner = json.frame_owner;
            }
        } catch(e) {
            LogErr("Error reading file: " + frameSettingsFile + " Bad Data!");
        }
    }
    catch (e)
    {
        LogWarn("User settings file doesn't exist: " + frameSettingsFile +
               " Creating new one.");
        settings.friendly_name = generateFriendlyName();
        var fd = fs.openSync(frameSettingsFile, 'w+', 0660);
        if (fd < 0) {
            LogErr("Unable to open file " + frameSettingsFile);
            return;
        }
        fs.writeSync(fd, JSON.stringify(settings));
        fs.closeSync(fd);
    }
};

// Write user settings to the settings file.
var writeUserSettings = function(settings) {
    // TODO: Should encrypt file (at the very least obfuscate with base64)
    try
    {
        var fd = fs.openSync(frameSettingsFile, 'w+', 0660);
        if (fd < 0) {
            LogErr("Unable to open file " + frameSettingsFile);
            return;
        }
        fs.writeSync(fd, JSON.stringify(settings));
        fs.closeSync(fd);
        LogInfo("New settings saved to file: " + frameSettingsFile);
    }
    catch (e)
    {
        LogErr("Error writing settings to file " + frameSettingsFile);
    }
};

// Read global settings from settings file.  If the file doesn't currently
// exist, create a new one with default settings.
var readGlobalSettings = function(settings)
{
    try {
        var data = fs.readFileSync(globalSettingsFile);
        var json = JSON.parse(data);

        try {
            if (json.brightness !== undefined) {
                settings.brightness = json.brightness;
            }
            if (json.power !== undefined) {
                settings.power = json.power;
            }
        } catch(e) {
            LogErr("Error reading file: " + globalSettingsFile + " Bad Data!");
        }
    }
    catch (e)
    {
        LogWarn("Global settings file doesn't exist: " + globalSettingsFile +
               " Creating new one.");
        settings.brightness = 100;
        settings.power = "up";
        var fd = fs.openSync(globalSettingsFile, 'w+', 0660);
        if (fd < 0) {
            LogErr("Unable to open file " + globalSettingsFile);
            return;
        }
        fs.writeSync(fd, JSON.stringify(settings));
        fs.closeSync(fd);
    }
};

// Write global settings to the settings file.
var writeGlobalSettings = function(settings)
{
    try
    {
        var fd = fs.openSync(globalSettingsFile, 'w+', 0660);
        if (fd < 0) {
            LogErr("Unable to open file " + globalSettingsFile);
            return;
        }
        fs.writeSync(fd, JSON.stringify(settings));
        fs.closeSync(fd);
        LogInfo("New settings saved to file: " + globalSettingsFile);
    }
    catch (e)
    {
        LogErr("Error writing settings to file " + globalSettingsFile);
    }
};

//initialize Settings
(function() {
    configSettings = new ConfigSettings();
    readConfigSettings(configSettings);

    serviceAuthCredentials = new ServiceAuthCredentials();
    readServerCredentials(serviceAuthCredentials);

    privSettings = new PrivateSettings();
    global_settings = new GlobalSettings();
    settings = new Settings();

    // FIXME: Private settings should be stored encrypted in persistent storage!

    var rusername = serviceAuthCredentials.username;
    var rpassword = serviceAuthCredentials.password;
    var rauth = rusername + ":" + rpassword;

    // PRIVATE settings
    privSettings.dial_port = 56789;  // FIXME! For security, this should be randomized!
    privSettings.ssdp_port = privSettings.dial_port + 1;
    privSettings.hotspot_port = 3002;
    privSettings.depict_registration_server = configSettings.content_server;
    privSettings.depict_registration_server_auth = rauth;

    privSettings.frame_receiver_server = getReceiverUrl();

    readGlobalSettings(global_settings);
    global_settings.power = "up";
    writeGlobalSettings(global_settings);

    readUserSettings(settings);

}());

// Verify whether or not we currently have internet connection
// Note, we need to distinguish between WiFi and WAN connection
// In the case of WiFi only connection, we can setup a WiFi Direct
// (hotspot) connection and continue communication with a local
// client.
var checkInternet = function(cb)
{
    var ipaddr = localIpAddress();

    if (ipaddr != prevIpAddress || ipaddr == '0.0.0.0')
    {
        LogDebug("Restarting network connection: ipaddr=" + ipaddr + ", prev=" + prevIpAddress);
        prevIpAddress = ipaddr;
        cb(EVENT_STOP);
        return;
    }

    // Using Google's public DNS servers
    dns.setServers(['8.8.8.8', '8.8.4.4']);

    dns.resolve('google.com', function(err, addresses) {
        var e;
        var len = (addresses == undefined) ? 0 : addresses.length;
        LogDebug("checkInternet: Resolve returned: err=" + err + ", addresses=" + addresses);
        if (len == 0 || err)
        {
            if (len == 0 && ipaddr.indexOf("192.168.43.") != -1)
            {
                // Local hotspot (can't access any outside WAN addresses)
                LogDebug("Detected Hotspot!");
                e = EVENT_CONNECT_WIFI;
            }
            else
            {
                LogInfo("Restarting network connections! error code: " + err.code);
                LogInfo("--- len = " + len + ", ipaddr = " + ipaddr);
                e = EVENT_STOP;
            }
        }
        else
        {
            e = EVENT_CONNECT_WAN;
            for (var i = 0;  i < len;  i++) {
                // Make sure the local router isn't redirecting the request
                if (addresses[i].indexOf("10.") == 0 ||
                    addresses[i].indexOf("192.168.") == 0 ||
                    addresses[i].indexOf("172.16.") == 0)
                {
                    LogDebug("checkInternet: google.com redirected to local router or hotspot!");
                    e = EVENT_DISCONNECT_WAN;
                    break;
                }
            }
        }
        cb(e);

        return;
    });
};

// Query the server to get Frame ownership tokens.  The Frame owner is basically
// the administrator and has full control over changing settings.  Others
// will only have limited access to changing the Frame's settings or casting
// to it.
var sendUserTokens = function() {

    LogInfo('Getting tokens for frame_owner: "' + settings.frame_owner.username + '"');

    var token = {};

    if (settings.frame_owner.username === '' ||
        (!!settings.frame_owner.iphone_token && !!settings.frame_owner.ipad_token)) {
        // Frame not set up yet, no useable tokens
        LogInfo('Frame not yet setup.  No usable tokens');
        return;
    }

    if (!(settings.frame_owner.ipad_token === '' ||
        settings.frame_owner.user_token === '' ||
        settings.frame_owner.ipad_token === undefined ||
        settings.frame_owner.user_token === undefined)) {
        // tokens already set, so don't query for them
        LogInfo('Tokens already setup.  Nothing to do.');
        return;
    }

    if (!!settings.frame_owner.iphone_token) {
        token.iphone = settings.frame_owner.iphone_token;
    } else {
        token.ipad = settings.frame_owner.ipad_token;
   }

    var username = serviceAuthCredentials.username;
    var password = serviceAuthCredentials.password;
    var service_auth = username + ":" + password;

    var postData = querystring.stringify({
        "token": token
    });

    LogInfo('POST: retriving /user_tokens from server. data: ' + postData);

    var options = {
        hostname: privSettings.depict_registration_server,
        port: 443,
        auth: service_auth,
        path: '/user_tokens',
        method: 'POST',
        headers: {
            'Content-type': 'application/x-www-form-urlencoded',
            'Content-Length': postData.length
        }
    };

    var req = https.request(options, function(res) {
        var response_data = "";

        res.setEncoding('utf8');
        res.on('data', function(chunk) {
            response_data = response_data + chunk;
        });

        res.on('end', function() {
            LogDebug('response end');

            LogInfo("Frame owner tokens: " + response_data);

            try
            {
                response_data = JSON.parse(response_data);

                if (response_data.success === true) {
                    // Update settings file with new tokens
                    settings.frame_owner.iphone_token = response_data.tokens.iphone_session_token;
                    settings.frame_owner.ipad_token = response_data.tokens.ipad_session_token;
                    settings.frame_owner.user_token = response_data.tokens.user_token;
                    writeUserSettings(settings);
                    LogInfo('New user tokens: ' + settings);
                } else {
                    LogErr("Unable to get user tokens:");
                    for (var i = 0;  i < response_data.errors.length; i++) {
                        LogErr("  " + response_data.errors[i]);
                    }
                }
            }
            catch (e)
            {
                LogErr("Bad return from /user_tokens POST: " + response_data);
            }
        });
    });

    req.on('error', function(e) {
        LogErr('problem with request: ' + e.message);
    });

    req.write(postData);
    req.end();
};

// Since JavaScript clients can't do M-SEARCH to discover DIAL servers
// (because JavaScript doesn't allow UDP multicasting), we need a way
// for such clients to discover our DIAL server without M-SEARCH.
// For Depict, we do this by registering this server with the Depict
// backend server (basically, a proxy server).  This function must
// be called each time our IP address changes, otherwise the proxy
// server will have stale info about our server.

var RegisterFrameServer = function() {
  var token = {};
  LogDebug('registering Frame TRYME_1.  settings are:\n' + JSON.stringify(settings));
  LogDebug('settings TRYME_1 frame owner:\n' + JSON.stringify(settings.frame_owner));
  LogDebug('settings TRYME_1 user token:\n' + JSON.stringify(settings.frame_owner.user_token));
  if (settings.frame_owner.user_token) token.user = settings.frame_owner.user_token;
  if (settings.frame_owner.iphone_token) token.iphone = settings.frame_owner.iphone_token;
  if (settings.frame_owner.ipad_token) token.ipad = settings.frame_owner.ipad_token;
  LogDebug('TRYME_1 sending token:\n' + JSON.stringify(token));

    var postData = querystring.stringify({
        'local_ip' : localIpAddress(),
        'local_port' : privSettings.ssdp_port.toString(),
        'xml_file_name' : 'dd.xml',
        'mac_address' : localMacAddress(),
        'token' : token });

    var options = {
        hostname: privSettings.depict_registration_server,
        port: 443,
        auth: privSettings.depict_registration_server_auth,
        path: '/api/v1/frame',
        method: 'POST',
        headers: {
            'Content-type': 'application/x-www-form-urlencoded',
            'Content-Length': postData.length
        }
    };

    var req = https.request(options, function(res) {
        LogDebug('headers:\n' + JSON.stringify(res.headers));
        res.setEncoding('utf8');
        res.on('data', function(chunk) {
            LogInfo('body:\n' + chunk);
        });
        sendUserTokens();    // get user session tokens
    });

    req.on('error', function(e) {
        LogErr('problem with request: ' + e.message);
    });

    req.write(postData);
    req.end();
}

// Unregister ourself from the Depict proxy server (assuming we've previously
// been registered via RegisterFrameServer).  If a callback was passed in
// call it after we've been unregistered.

var UnregisterFrameServer = function(cb)
{
    var options = {
        hostname: privSettings.depict_registration_server,
        port: 443,
        auth: privSettings.depict_registration_server_auth,
        path: '/api/v1/frame?mac_address=' + localMacAddress(),
        method: 'DELETE',
        headers: {
            'Content-type': 'application/x-www-form-urlencoded',
        }
    };

    var req = https.request(options, function(res) {
        LogInfo('headers:\n' + JSON.stringify(res.headers));
        res.setEncoding('utf8');
        res.on('data', function(chunk) {
            LogInfo('body:\n' + chunk);
        });
        if (cb) cb();
    });

    req.on('error', function(e) {
        LogErr('problem with request: ' + e.message);
        if (cb) cb();
    });

    req.end();
}

LogInfo("LOCAL IPADDR: " + localIpAddress() + " MAC: " + localMacAddress());

// for parsing ajax requests
app.use(body_parser.json());
app.use(body_parser.urlencoded({
    extended: true
}));

app.use(function(req, res, next)
{
    LogDebug("HOST: " + req.hostname);
    LogDebug("URL: " + req.method + req.url + req.path);

    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'GET,PUT,POST,DELETE,OPTIONS');
    res.header('Access-Control-Allow-Headers', 'Content-Type, Authorization, Content-Length, X-Requested-With');

    // Intercept OPTIONS
    if ('OPTIONS' == req.method) {
        res.send(200);
    } else {
        next();
    }
});

// The "hotspot" server is used to change settings.  One important function is to
// allow a client to set the initial WiFi name/password using a "wi-fi direct" (what Android calls
// "portable hotspot") connection.

var startHotspotServer = function()
{
    var ipaddr = localIpAddress();

    if (hotspotServerStarted == true) {
        LogInfo("Hotspot server already started!  Ignoring startHotspotServer()");
        return;
    }

    if (ipaddr == '0.0.0.0') {
        LogInfo("Networking not yet started!  Can't yet start Hotspot server!");
        return;
    }

    hotspotServer.on("close", function()
    {
        LogErr("hotspotServer " + ipaddr + ":" + privSettings.hotspot_port + " closed!");
    });

    hotspotServer.listen(privSettings.hotspot_port, ipaddr, function() {
        LogInfo("Hotspot listener bound to: " + ipaddr + ":" + privSettings.hotspot_port);
    });

    hotspotServer.on("error", function(err)
    {
        LogErr("hotspotServer error listening on port " +privSettings.hotspot_port +
            ":\n" + err.stack);
    });

    hotspotServerStarted = true;
}

var stopHotspotServer = function()
{
    LogInfo("stopHotspotServer");
    hotspotServer.close();
    hotspotServerStarted = false;
}


var startUdpMcastServer = function()
{
    udpMcastServer = dgram.createSocket("udp4");

    //=============================================================================
    //
    // Received an M-SEARCH request message on the DIAL multicast UDP port.  Reply
    // with a Search Target (ST) header and LOCATION header giving the HTTP URL
    // of the UPnP device description.
    //
    // The Search Target (ST) for DIAL will be of the form:
    //
    //      urn:dial-multiscreen-org:service:dial:1
    //
    // The LOCATION URL can be queried by the client to get more specific details
    // about the device (i.e. Depict Frame) and will be of the following form:
    //
    //      http://<DepictFrameIpAddr>:<port>/dd.xml

    udpMcastServer.on("message", function(msg, rinfo)
    {
        var portStr = privSettings.ssdp_port.toString();

        var SSDP_REPLY =
	"HTTP/1.1 200 OK\r\n" +
	"LOCATION: http://" + localIpAddress() + ":" + portStr + "/dd.xml\r\n" +
	"CACHE-CONTROL: max-age=1800\r\n" +
	"EXT:\r\n" +
	"BOOTID.UPNP.ORG: 1\r\n" +
	"SERVER: Linux/3.13 UPnP/1.0 quick_ssdp/1.0\r\n" +
	"ST: urn:dial-multiscreen-org:service:dial:1\r\n" +
	"USN: uuid:" + UUID + "-" + localMacAddress() + "::" +
	"urn:dial-multiscreen-org:service:dial:1\r\n\r\n";

        var reply = new Buffer(SSDP_REPLY.length);

        LogDebug("============ SERVER Received message from " +
	    rinfo.address + ":" + rinfo.port + "\n" +
            msg + "\n===============================");

        if (msg.toString().toLowerCase().indexOf(DIAL_ST) != -1 ||
            msg.toString().toLowerCase().indexOf(SSDP_ALL) != -1)
        {
            // Got a valid M-SEARCH SSDP request for available DIAL servers
            // or possibly for all UPnP devices.

            reply.write(SSDP_REPLY);
            LogDebug("=========== SERVER Sending reply:\n" + reply);

            udpMcastServer.send(reply, 0, reply.length, rinfo.port, rinfo.address);
        }
        else
        {
            LogDebug("***** Unrecognized DIAL SSDP query.  Ignore! (" +
                   msg.toString() + ") *****\n");
        }
    });


    //
    // Listen for M-SEARCH request messages being sent over the DIAL multicast
    // UDP port.
    //
    udpMcastServer.on("listening", function()
    {
        var address = udpMcastServer.address();
        LogInfo("SSDP server listening " + address.address + ":" + address.port);
    });

    udpMcastServer.bind(MCAST_PORT, null, function()
    {
        udpMcastServer.addMembership(MCAST_ADDR);
    });

    //
    // Report any errors
    //
    udpMcastServer.on("error", function(err)
    {
        LogErr("SSDP server error:\n" + err.stack);
        udpMcastServer.close();
    });
}

var stopUdpMcastServer = function()
{
    LogInfo("Closing M-SEARCH UDP server...");
    try {
        udpMcastServer.close();
    } catch (e) {
        LogErr("Error closing udpMcastServer!");
    }
}


// On response of the LOCATION URL in the M-SEARCH response, the client has
// issued an HTTP GET using that URL to get the UPnP device description.
// This function returns this desription which also includes an additional
// field, the Application-URL, which identifies the DIAL REST service for
// all supported apps.
//
// The general format for the Application-URL is:
//
//     Application-URL = "Application-URL" ":" absoluteURI
//
// Clients can control DIAL applications by issuing HTTP requests against
// That URL.  For example, to get current details about a supported DIAL app
// (e.g. YouTube), a client could issue a command like:
//
//     % curl http://<DialDeviceIpAddr>:<DialPortNum>/apps/YouTube
//
// This will cause the DIAL server to return an XML response giving runtime
// info on that app.  For details see Section 6.1 of the DIAL Specification.
//
discovery.get('/dd.xml', function(req, res)
{
    var APP_URL = "http://" + localIpAddress() + ":" +
        privSettings.dial_port.toString() + "/apps/";

    var DDXML =
	"<?xml version=\"1.0\"?>" +
	"<root" +
	"  xmlns=\"urn:schemas-upnp-org:device-1-0\"" +
	"  xmlns:r=\"urn:restful-tv-org:schemas:upnp-dd\">" +
	"  <specVersion>" +
	"    <major>1</major>" +
	"    <minor>0</minor>" +
	"  </specVersion>" +
	"  <device>" +
	"    <deviceType>urn:schemas-upnp-org:device:tvdevice:1</deviceType>" +
	"    <friendlyName>" + settings.friendly_name + "</friendlyName>" +
	"    <manufacturer>" + MANUFACTURER + "</manufacturer>" +
	"    <modelName>" + MODEL_NAME + "</modelName>" +
	"    <UDN>uuid:" + UUID + "-" + localMacAddress() + "</UDN>" +
	"  </device>" +
	"</root>\r\n";

    LogDebug("GET /dd.xml");
    LogDebug("Sending reply:\n" + DDXML);

    res.writeHead(200,
	{"Content-Type" : "application/xml",
	 "Content-Length" : DDXML.length,
         "Access-Control-Allow-Origin" : "*",
         "Access-Control-Allow-Credentials" : true,
         "Access-Control-Expose-Headers" : "Application-URL",
	 "Application-URL" : APP_URL});

    res.write(DDXML);
    res.end();
});


//
// Return runtime application information for a given application X in response
// to an HTTP GET request.  For details, see Section 6.1 of the DIAL
// Specification.
//

app.get('/apps/*', function(req, res)
{
    var APP_URL = "http://" + localIpAddress() + ":" +
        privSettings.dial_port.toString() + "/apps/";

    var appName = req.params[0];
    var appUrlRun = APP_URL + appName + "/run";
    var appState = "stopped";
    var allowStop = "false";
    var linkElem = " ";
    var additionalData = "";

    switch (appName)
    {
        case 'DepictFramePlayer':
            appState = depictFrame.state;
            linkElem = (depictFrame.state == "running") ?
                       "  <link rel=\"run\" href=\"" + appUrlRun + "\"/>\r\n" :
                       "";
            allowStop = depictFrame.allowStop;
            additionalData = depictFrame.additionalData +
                             "controlAddress=\"" + localIpAddress() + ":" +
                             privSettings.dial_port.toString() + "\"";
            break;

/*
        // For testing
        case 'YouTube':
            appState = youTube.state;
            linkElem = (youTube.state == "running") ?
                       "  <link rel=\"run\" href=\"" + appUrlRun + "\"/>\r\n" :
                       "";
            allowStop = youTube.allowStop;
            additionalData = youTube.additionalData;
            break;
*/

        default:
            // Invalid/unsupported app name
            res.writeHeader(404);
            res.write("Error 404: Not Found - App status\n");
            res.end();
            return;
    }

    var appXml =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n" +
	"<service xmlns=\"urn:dial-multiscreen-org:schemas:dial\" dialVer=\"" +
	DIAL_VERSION + "\">\r\n" +
	"  <name>" + appName + "</name>\r\n" +
	"  <options allowStop=\"" + allowStop + "\"/>\r\n" +
	"  <state>" + appState + "</state>\r\n" +
        linkElem +
	"  <additionalData>\n" +
	"    " + additionalData + "\n" +
	"  </additionalData>\r\n" +
	"</service>\r\n";

    res.writeHeader(200,
	{"Content-Type" : "application/xml",
	 "Content-Length" : appXml.length,
	 "Access-Control-Allow-Origin" : "*"});
    res.write(appXml);
    res.end();
});


var userIsPermitted = function(request, response, next) {
    LogDebug('\n\nchecking if permitted!\n\n');
    if (frameIsPublic()) {
        LogInfo('\n\nFrame is public!\n\n');
        return next();
    } else if (userIsOwner(request)) {
        LogInfo('\n\nUser is owner!\n\n');
        return next();
    } else {
        LogErr('\n\nDenying request!\n\n');
        return denyRequest(response);
    }
}

var frameIsPublic = function() {
    LogDebug('\n\npublic: ', !!settings.public_casting);

    return !!settings.public_casting;
}

// Verify whether a client request to change settings is received by an owner with
// a token which matches the device-specific token currently stored on the Frame.
var userIsOwner = function(request) {
    try {
        LogInfo('body: ' + JSON.stringify(request.body));
        LogInfo('settings: ' + JSON.stringify(settings));
    } catch (e) {};

    if (request.body === undefined ||
        request.body.frame_owner === undefined ||
        request.body.frame_owner.user_token === undefined ||
        request.body.frame_owner.user_token === "" ||
        settings === undefined ||
        settings.frame_owner === undefined)
    {
        LogWarn("Owner can't be determined!");
        LogWarn("request frame_owner: " + JSON.stringify(request.body.frame_owner));
        return false;
    }

    device_type = parseInt(request.body.frame_owner.device_type);

    var token = (device_type === IPHONE) ? settings.frame_owner.iphone_token :
                (device_type === IPAD) ? settings.frame_owner.ipad_token :
                settings.frame_owner.user_token;

    var retval = (token === undefined || token === "") ? false :
                 (request.body.frame_owner.user_token === token);

    LogInfo("userIsOwner = " + retval);

    return retval;
}

var userIsOwnerGet = function(request) {
    LogDebug('request: ' + request);
    LogDebug('query: ' + request.query);
    LogDebug('user_token: ' + request.query.user_token);
    LogDebug('settings: ' + settings);
    LogDebug('frame_owner: ' + settings.frame_owner);
    LogDebug('settings token: ' + settings.frame_owner.user_token);

    console.log(request.query);
    console.log(request.query.frame_owner);
    console.log(settings.frame_owner);

    if (request.query === undefined ||
        request.query.user_token === undefined ||
        request.query.user_token === "" ||
        settings === undefined ||
        settings.frame_owner === undefined)
    {
        LogInfo("Owner can't be determined!");
        return false;
    }

    device_type = parseInt(request.query.device_type);
    var token = (device_type === IPHONE) ? settings.frame_owner.iphone_token :
                (device_type === IPAD) ? settings.frame_owner.ipad_token :
                settings.frame_owner.user_token;

    var retval = (token === undefined || token === "") ? false :
                 (request.query.user_token === token);

    LogInfo("device_type = " + device_type +
            ", token = " + token +
            ", frame_owner = " + JSON.stringify(settings.frame_owner));

    LogInfo("userIsOwnerGet = " + retval)

    return retval;
}

var denyRequest = function(response) {
    response.writeHeader(403);
    response.write(JSON.stringify({
        error: "User must be Frame owner."
    }));
    return response.end();
}

app.post('/apps/DepictFramePlayer/load_media', userIsPermitted, function(request, response) {
    LogAjax('load_media', request);
    response.writeHeader(200);
    response.end();
    // LogAjax("IN LOAD MEDIA\n\n\n\n\n");
    // LogAjax("command request: ", request);
    io.sockets.emit('control_message', JSON.stringify(request.body));
});

app.post('/apps/DepictFramePlayer/play', userIsPermitted, function(request, response) {
    LogAjax('play', request);
    response.writeHeader(200);
    response.end();
    io.sockets.emit('control_message', JSON.stringify(request.body));
});

app.post('/apps/DepictFramePlayer/pause', userIsPermitted, function(request, response) {
    LogAjax('pause', request);
    response.writeHeader(200);
    response.end();
    io.sockets.emit('control_message', JSON.stringify(request.body));
});

app.post('/apps/DepictFramePlayer/volume', function(request, response) {
    LogAjax('volume', request);
    response.writeHeader(200);
    response.end();
    io.sockets.emit('control_message', JSON.stringify(request.body));
});

app.post('/apps/DepictFramePlayer/status', function(request, response) {
    LogAjax('status', request);
    response.write(player_status);
    response.end();
});


hotspot.get('/movies/*', function(req, res) {
	LogDebug("GET /" + req.originalUrl);
	res.sendfile(__dirname + '/public' + req.originalUrl);
});

//
// Backdoor set/get server settings
//
app.get('/settings', function(request, response) {
    // Get current settings
    res.write("Hello!  Here is your settings!");
    res.end();
});

app.post('/settings/frame_receiver_server/*', function(request, response) {
    var url = request.params[0];
    LogInfo('frame_receiver_server: ' + request);
    LogInfo('url: ' + url);
    privSettings.frame_receiver_server = url;
    stopApplication('DepictFramePlayer');
    response.writeHeader(200);
    response.end();
});


//
// Launch specified app X.  This is done by the client issuing an HTTP POST
// request to <Application-URL>/X (where X is the desired application).
// For details, see Section 6.2 of the DIAL Protocol Specification.
//
app.post('/apps/*', function(req, res) {
    var APP_URL = "http://" + localIpAddress() + ":" +
        privSettings.dial_port.toString() + "/apps/";

    var appName = req.params[0];
    var appUrlRun = APP_URL + appName + "/run";
    var appState = "stopped";
    var additionalData = "";
    var statusCode;

    LogInfo("POST /apps/" + appName);
    //LogInfo("POST body: " + req.body);

    var headerInfo =
            {"Content-Type" : "text/plain",
	     "Content-Length" : 0,
	     "Location" : appUrlRun,
	     "Access-Control-Allow-Origin" : "*"};

    statusCode = startApplication(appName);

    if (statusCode == 200 || statusCode == 201)
    {
        res.writeHeader(statusCode, headerInfo);
    }
    else
    {
        res.writeHeader(statusCode);
    }
    res.end();

});

//
// Stop the specified app.  This is done by the client issuing an HTTP DELETE
// request to <Application-URL>/X (where X is the desired application).
// For details, see Section 6.4 of the DIAL Protocol Specification.
//
app.delete('/apps/*', function(req, res) {
    var statusCode = 404;
    var url = req.params[0];
    var appName = (url.indexOf("YouTube") != -1) ? "YouTube" :
                  (url.indexOf("DepictFramePlayer") != -1) ? "DepictFramePlayer" :
                  "undefined";

    LogInfo("KILL /apps/" + appName);
    statusCode = stopApplication(appName);
    res.writeHeader(statusCode);
    res.end();
});

// for parsing ajax requests
hotspot.use(body_parser.json());
hotspot.use(body_parser.urlencoded({
    extended: true
}));

hotspot.use(function(req, res, next) {
    LogDebug("HOST: " + req.host);
    LogDebug("URL: " + req.method + req.url + req.path);

    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'GET,PUT,POST,DELETE,OPTIONS');
    res.header('Access-Control-Allow-Headers', 'Content-Type, Authorization, Content-Length, X-Requested-With');

    // Intercept OPTIONS
    if ('OPTIONS' == req.method) {
        res.send(200);
    } else {
        next();
    }
});

hotspot.get('/', function(req, res) {
    LogInfo("Got Hotspot request!");
    res.writeHeader(200);
    res.write("Hi from Hotspot server!");
    res.end();
});

// Returns a "howdy" message which can be used in initial handshaking
// If encryption is being used, will return the per/session public key
hotspot.get('/howdy', function(req, res) {
    // FIXME: Should generate {'public_key' : key}.  For now, a client
    // should assume no encryption if no public key returned
    var key = {};
    var howdyMsg = {"howdy" : key};

    res.writeHeader(200);
    res.write(JSON.stringify(howdyMsg));
    res.end();
});

// Returns list of Wi-Fi network names detected by the Frame
hotspot.get('/networks', function(req, res) {
    var networks = {};

    try {
        networks = fs.readFileSync(networksFile);
        LogInfo("Networks = " + networks);
    } catch(e) {
        LogErr("Failed reading networks file " + networksFile);
        networks = "{networks : []}";
    }
    res.writeHeader(200);
    res.write(networks);
    res.end();
});

// Returns current global user settings
hotspot.get('/settings', function(req, res) {
    // FIXME: Need to query for power setting when power management
    // is properly implemented.  Hack for now!
    var pwr;

    readGlobalSettings(global_settings);
    pwr = global_settings.power;

    var s = {
        friendly_name : settings.friendly_name,
        orientation : settings.orientation,
        network : { ssid : settings.network.ssid, valid : settings.network.valid },
        version : settings.version,
        public_casting : settings.public_casting,
        power : pwr,
        brightness : global_settings.brightness,
        contrast : global_settings.contrast,
        frame_owner : { username : settings.frame_owner.username,
                        current_user_is_owner: userIsOwnerGet(req) },
    };

    res.writeHeader(200);
    res.write(JSON.stringify(s));
    res.end();
});

var notifyOrientationChanged = function(orientation) {
    try {
        io.sockets.emit("update_orientation", JSON.stringify({ orientation: orientation }));
    } catch (e) {
        LogErr("Error: could not emit orientation change notification.");
    }
}

// Change global user settings.  The settings which are allowed
// to change depends on whether the client is the owner (sends the
// correct owner token with the POST message) or is simply a general
// user who is allowed to change certain settings when the Frame
// is in 'public' mode.
hotspot.post('/settings', function(req, res) {
    var statusCode = 200;
    var isFactoryReset = true;
    var settingsChanged = false;

    var json = req.body.settings;
    LogInfo("REQUEST BODY");
    LogInfo(JSON.stringify(req.body));
    LogInfo("New settings request: " + JSON.stringify(json));

    try {
        // Check out current settings and see if these have ever been
        // modified and we've successfully connected to the internet.
        if (settings.network.valid === false ||
            settings.frame_owner.username === undefined ||
            settings.frame_owner.username === '') {
            isFactoryReset = true;
        } else {
            isFactoryReset = false;
        }
    }
    catch (e) {
        LogErr("Internal error. Invalid settings detected!");
        isFactoryReset = true;
    }

    if (isFactoryReset) {
        // Need to get the client's device type to determine what
        // tokens to examine for further processing of settings.
        var owner;

        readUserSettings(settings);

        // Only during factory reset can the following be changed:
        //    - frame owner
        //    - wi-fi network

        try {
            LogInfo("frame_owner: " + JSON.stringify(json.frame_owner));
            var client_device_type = parseInt(json.frame_owner.device_type);
            LogInfo("Factory reset: Client device type: " + client_device_type);

            if (json.frame_owner !== undefined) {
                settings.frame_owner.username = json.frame_owner.username;

                // We currently use different tokens for different client devices
                switch (client_device_type) {
                    case IPHONE:
                        settings.frame_owner.iphone_token = json.frame_owner.user_token;
                        break;

                    case IPAD:
                        settings.frame_owner.ipad_token = json.frame_owner.user_token;
                        break;

                    default:
                        settings.frame_owner.user_token = json.frame_owner.user_token;
                        break;
                }
                settingsChanged = true;
            }
            if (json.network !== undefined ) {
                settings.network.ssid = json.network.ssid;
                //settings.network.password = json.network.password;
                if (json.network.password) {
                  // password is base64 encoded. decode here before saving
                  var decoded = new Buffer(json.network.password, 'base64').toString('utf-8')
                  settings.network.password = decoded;
                }
                settingsChanged = true;
            }
        }
        catch(e)
        {
            //LogErr("Error: Bad settings JSON message: " + req.body);
            LogErr("Error: Bad settings JSON message: " + json);
            res.writeHeader(400);
            res.end();
            return;
        }
    }

    var isOwner = userIsOwner(req) || isFactoryReset;
    var isPublic = frameIsPublic();
    var orientationChanged = false;

    if (!isPublic && !isOwner) {
        // Someone trying to change Frame settings without having proper access!
        LogErr("Invalid access!  Attempt by non-owner to access private Frame settings!");
        denyRequest(res);
        return;
    }

    if (isOwner) {
        // Settings which only the owner can change
        LogInfo('Changing Frame settings as owner');

        if (json.friendly_name !== undefined) {
            if (settings.network.valid === true) {
                settings.friendly_name = json.friendly_name;
            } else {
                // Delay setting new friendly_name until after we have a good
                // Wi-Fi network connection.
                privSettings.pending_friendly_name = json.friendly_name;
            }
            settingsChanged = true;
        }

        if (json.public_casting !== undefined)  {
            settings.public_casting = json.public_casting;
            // No need to restart the player
            //settingsChanged = true;
        }
    }

    if (isPublic || isOwner) {
        // Change settings which are only allowed when either owner or public
        LogInfo('Changing public Frame settings');

        if (json.orientation !== undefined) {
            if (settings.orientation !== json.orientation ) {
                settings.orientation = json.orientation;
                orientationChanged = true;
                settingsChanged = true;
            }
        }
    }

    res.writeHeader(statusCode);
    res.end();

    // do this after sending the response, otherwise the hotspot may
    // turn off before the response is sent, so the settings will be
    // correctly updated, but the response may not be received by
    // the client, making it seem there was an error to the client
    writeUserSettings(settings);

    if (settingsChanged === true)
    {
        if (orientationChanged && appServerStarted)
        {
            notifyOrientationChanged(settings.orientation);
        }

        // Restart application with new settings (if running)
        if (depictFrame.state === "running")
        {
            startApplication("DepictFramePlayer");
        }
    }
});

// Issue commands to the Frame
hotspot.post('/command/*', function(req, res) {
    var cmd = req.params[0];
    var statusCode = 200;

    var isOwner = userIsOwner(req);
    var isPublic = frameIsPublic();

    LogInfo("Received Frame command: " + cmd);

    switch (cmd) {
        // Reboot system (note, this is asynchronous, so may return)
        case 'reboot':
            if (isPublic || isOwner) {
                rebootSystem();
            } else {
                statusCode = 403;
            }
            break;

        // Check for updates
        case 'update':
            LogWarn("Updates not currently supported!");
            if (isOwner) {
                // updateFirmware()
            } else {
                statusCode = 403;
            }
            statusCode = 501;
            break;

        // Power system down (standby) or up
        case 'power':
            if (isPublic || isOwner) {
                var pwr = req.query.pwr;
                setStandby(pwr);
            } else {
                statusCode = 403;
            }

            break;

        // Set display brightness
        case 'brightness':

            if (isPublic || isOwner) {
                var brightness = req.query.level;
                LogInfo("Setting display brightness to: " + brightness);
                global_settings.brightness = brightness;
                writeGlobalSettings(global_settings);
            } else {
                statusCode = 403;
            }
            break;

        // Set display contrast
        case 'contrast':
            LogWarn("Setting display contrast not currently supported!");
            statusCode = 501;
            break;

        // Perform factory reset of all user settings
        case 'factory_reset':
            if (isOwner) {
                LogInfo("Performing factory reset of user settings");

                res.writeHeader(statusCode);
                res.end();

                factoryReset();

            } else {
                denyRequest(res);
            }

            return;
        default:
            LogErr("Invalid command: " + cmd);
            statusCode = 400;
            break;
    }

    res.writeHeader(statusCode);
    res.end();
});

var startApplication = function(appName) {
    var statusCode = 404;

    // On Android, we use the activity manager (am) to start the app.  For
    // normal Linux (test environment), we simply spawn the executable by name.

    switch (appName) {
        case "DepictFramePlayer":

            var depictAppName;
            var depictArgs;

            if (isAndroid) {
                var startArgs = '';
                if (startNewPlayer) {
                    startArgs = '--activity-single-top';
                    startNewPlayer = false;
                } else {
                    //startArgs = '--activity-brought-to-front';
                    startArgs = '--activity-single-top';
                }

                depictAppName = 'am';
                depictArgs = [
	        'start',
                '--user',
                '0',
                '-W',
	        '-a',
	        'android.intent.action.MAIN',
                startArgs,
	        '-n',
                'com.depict.frame.DepictFramePlayerAndroid/com.depict.frame.DepictFramePlayerAndroid.WebActivity',
	        '-e',
	        'START_URI'
                ];
            } else {
                depictAppName = 'DepictFramePlayer';
                depictArgs = [];
            }

            var depictQueryArgs = '?controlAddr=' +
                localIpAddress() + ':' + privSettings.dial_port.toString() +
                '&friendly_name=' + escape(settings.friendly_name) +
                '&orientation=' + settings.orientation;

            LogInfo("** appName: " + appName + " state: " + depictFrame.state);

            if (privSettings.doRestart == true) {
                stopApplication(appName);
                privSettings.doRestart = false;
            }

            if (!isAndroid && (depictFrame.state == "running")) {
                    // App is already running.  If parameters have changed, modify
                    // the current parameters, otherwise, this request has no effect.

                    // FIXME: Check passed-in parameters!

                    LogInfo("** appName: " + appName + "already running!  Ignore launch!!");
                    statusCode = 200;
                    return statusCode;
            }

            {
                // App isn't currently running.  Launch!

                // NOTE: The frame_receiver_server must be running prior to this call!
                // Be aware, on Linux, the DepictFramePlayer executable child process,
                // however on Android, we execute the Activity Manager ('am') which
                // indirectly starts DepictFramePlayerAndroid via an INTENT (so our
                // child process is 'am' which returns/exits immediately after issuing
                // the intent).  On Android, we always send an intent to launch even
                // if it is currently running (in that case it has no effect).

                privSettings.frame_receiver_server = getReceiverUrl();

                LogInfo("** Spawning process: " + depictAppName + ": args = " +
                    depictArgs.concat([ privSettings.frame_receiver_server + depictQueryArgs ]));

                depictFrame.proc = child_process.spawn(depictAppName,
	            depictArgs.concat([ privSettings.frame_receiver_server + depictQueryArgs ]));

                if (depictFrame.proc) {
                    depictFrame.proc.stdout.on('data', function(data) {
	                     LogDebug('[' + appName + '] stdout: ' + data);
                    });

                    depictFrame.proc.stderr.on('data', function(data) {
	                      LogDebug('[' + appName + '] stderr: ' + data);
                    });

                    depictFrame.proc.on('close', function(code) {
	                      LogDebug('[' + appName + '] exited with code ' + code);
                        if (!isAndroid || code != 0) {
                            depictFrame.state = "stopped";
                        }
                    });

                    depictFrame.proc.on('exit', function(msg) {
	                      LogDebug('[' + appName + '] Exited: ' + msg);
                        if (!isAndroid) {
                            depictFrame.state = "stopped";
                        }
                    });

                    depictFrame.proc.on('error', function(msg) {
	                      LogDebug('[' + appName + '] Error: ' + msg);
                        depictFrame.state = "stopped";
                    });

                    depictFrame.state = "running";
                    LogInfo("Spawned depictFrame.proc.pid = " +
                            depictFrame.proc.pid + ': ' + depictQueryArgs);

                    statusCode = 201;
                } else {
                    LogWarn("Failed to launch DepictFramePlayer!");
                    statusCode = 503;
                }
            }
            break;

        default:
            LogErr("Invalid app: " + appName);
            statusCode = 404;
            break;
    }

    return statusCode;
}


var stopApplication = function(appName)
{
    var statusCode = 404;

    LogInfo("Stopping application: " + appName);

    switch (appName) {
        case 'DepictFramePlayer':
            {
                if (isAndroid)
                {
                    var args = [
                      'start',
                      '--user',
                      '0',
                      '-W',
                      '-a',
                      'com.depict.action.STOP',
                      '--activity-single-top',
                      '-n',
                      'com.depict.frame.DepictFramePlayerAndroid/.WebActivity'
                    ];

                    child_process.spawn('am', args);

                } else {
                    // FIXME: Should do 'killall' to make sure no stale processes
                    // are still hanging around!
                    try {
                        depictFrame.proc.kill('SIGHUP');
                    }
                    catch(e) {}
                }
                depictFrame.state = "stopped";
                statusCode = 200;
            }
            break;

        default:
            LogErr("Invalid app: " + appName);
            statusCode = 404;
            break;
    }
    return statusCode;
}


var appStatus = function(appName)
{
    if (!isAndroid) {
        return;
    }

    var args = [
        'stack',
        'list'
    ];

    var proc = child_process.spawn('am', args);
    if (proc) {
        proc.stdout.on('data', function(data) {
            try {
                var buffer = data.toString();
                LogDebug("buffer: " + buffer);
                if (buffer.indexOf("taskId") != -1) {
                    if (buffer.indexOf(appName) != -1) {
                        if (depictFrame.state != "running") {
	                    LogInfo('[' + appName + '] RUNNING!');
                        } else {
	                    LogDebug('[' + appName + '] RUNNING!');
                        }
                        depictFrame.state = "running";
                    } else {
                        if (depictFrame.state != "stopped") {
	                    LogWarn('[' + appName + '] STOPPED! Did the app crash/die? Restarting...');
                            startApplication("DepictFramePlayer");
                        } else {
	                    LogDebug('[' + appName + '] STOPPED!');
                            depictFrame.state = "stopped";
                        }
                    }
                }
            } catch (e) {
                LogErr("Unable to get status for " + appName);
            }
        });
    }
}


var powerManager = function(command) {
    var statusCode = 404;

    // On Android, we use the activity manager (am) to start the power manager

    if (!isAndroid) {
        LogWarn("powerManager unimplemented on Linux!");
        return statusCode;
    }

    var activityManager = 'am';
    var args;
    var action;

    switch (command) {
        case "reboot": action = 'REBOOT'; break;
        case "standby_on":  action = 'STANDBY_ON'; break;
        case "standby_off": action = 'STANDBY_OFF'; break;
        case "factory_reset": action = 'FACTORY_RESET'; break;

        default:
            LogErr("Invalid power manager command: " + command);
            return statusCode;
    }

    args = [
        'start',
        '--user',
        '0',
        '-W',
        '-a',
        'com.depict.action.' + action,
        '--activity-single-top',
        '-n',
        'com.depict.frame.DepictFrameService/.PowerActivity'
    ];

    LogInfo("** PowerManager action: " + action);
    LogInfo("** Spawning process: " + activityManager + ": args = " + args);

    var proc = child_process.spawn(activityManager, args);

    if (proc) {
        proc.stdout.on('data', function(data) {
	         LogDebug('[' + activityManager + '] stdout: ' + data);
        });

        proc.stderr.on('data', function(data) {
	         LogDebug('[' + activityManager + '] stderr: ' + data);
        });

        proc.on('close', function(code) {
	         LogDebug('[' + activityManager + '] exited with code ' + code);
        });

        proc.on('exit', function(msg) {
	         LogDebug('[' + activityManager + '] Exited: ' + msg);
        });

        proc.on('error', function(msg) {
	         LogDebug('[' + activityManager + '] Error: ' + msg);
        });

        statusCode = 201;
    }

    return statusCode;
}


// Reboot platform
var rebootSystem = function() {
    powerManager('reboot');
}


// Standby on/off
var setStandby = function(pwr) {
    LogInfo("Power set to: " + pwr);
    global_settings.power = pwr;
    writeGlobalSettings(global_settings);

    if (pwr === 'down') {
        powerManager('standby_on');
    } else {
        powerManager('standby_off');
    }
}

// Reset user settings and reboot
var factoryReset = function() {
    powerManager('factory_reset');
}


// The Depict proxy server keeps track of all Frames which have been registered
// within the last 7 days.  In order not to be removed from this list, we need
// to periodically refresh our registration prior to that time.  Also, we need
// to detect when the IP address changes and in this case force a refresh.

var doRefresh = true;
var refreshStart = Date.now();

var refreshFrame = function() {
    var ipaddr = localIpAddress();
    var macaddr = localMacAddress();
    var now = Date.now();
    var elapsedRefreshMsec = now - refreshStart;
    var randomMsec = Math.floor((Math.random() * RANDOM_INTERVAL_MAX_MSEC) + 1);

    if (ipaddr == "0.0.0.0") {
        return;
    }

    //LogInfo("==== ipaddr=" + ipaddr + ", macaddr=" + macaddr + " ====");

    if (elapsedRefreshMsec > (REGISTRATION_REFRESH_INTERVAL_MSEC + randomMsec)
            || doRefresh) {
        // (Re-)register our server.  We add a bit of randomness to the
        // registration interval so that a crapload of Frames don't swamp the
        // server at the same time (e.g. immediately after a major power outage)
        LogInfo("*** REGISTER  IPADDR=" + ipaddr + "! ***");
        UnregisterFrameServer(function() {
            RegisterFrameServer();
            refreshStart = now;
            doRefresh = false;
        });
    }
}


var TIMER_TICK_INTERVAL_MSEC = 2000;
var connectionTimerId = null;


// Periodic timer for checking status of stuff like internet connection
// and running status of an application.
var heartbeatTick = function() {
    // Check running status of our app (is it alive?)
    appStatus('DepictFramePlayer');
    // See if we are currently connected to the internet
    checkInternet(function(evt) {
        stateMachine(evt);
        setTimeout(heartbeatTick, TIMER_TICK_INTERVAL_MSEC);
    });
}

//
// Kill this server.  First we do some orderly tear down (like unregistering
// this server).
//
process.on('SIGINT', function() {
    LogWarn("Caught interrupt signal");

    stopServer();

    // Since the server is exiting, tell the Depict proxy server so clients
    // won't time out unnecessarily trying to access us when we don't exist.
    UnregisterFrameServer(function() {
	     try {
            io.sockets.emit('disconnect');
        } catch (e) { }
        LogWarn("Calling process.exit!");
        process.exit();
    });
});

process.on('SIGSEGV', function() {
    LogErr("*** WTF!  SIGSEGV!! ***");
    process.exit();
});

//=============================================================================
//
// Start listening!
//
//=============================================================================

var sockets = {}, nextSocketId = 0;
var dialSockets = {}, dialNextSocketId = 0;

var startDialServer = function() {
    var ipaddr = localIpAddress();

    if (dialServerStarted == true) {
        LogInfo("DIAL server already started!  Ignoring startDialServer()");
        return dialServerStarted;
    }

    if (ipaddr == '0.0.0.0') {
        LogInfo("Networking not yet started!  Can't yet start DIAL server!");
        return dialServerStarted;
    }

    dialServer.on("connection", function(socket) {
        LogDebug("dialServer connection");
        var socketId = dialNextSocketId++;
        dialSockets[socketId] = socket;
        LogDebug("dial socket " + socketId + " opened");

        socket.on("close", function() {
            LogDebug("dial socket " + socketId + " closed");
            delete dialSockets[socketId];
        });
    });

    dialServer.on("close", function() {
        LogErr("DIAL server " + ipaddr + ":" + privSettings.ssdp_port + " closed!");
    });

    dialServer.on("error", function(err) {
        LogErr("DIAL server error listening on port " + privSettings.ssdp_port +
             ":\n" + err.stack);
        //dialServer.close();
    });

    dialServer.timeout = 10000;

    dialServer.listen(privSettings.ssdp_port, ipaddr, function() {
        LogInfo("DIAL listener bound to: " + ipaddr + ":" + privSettings.ssdp_port);
    });
    dialServerStarted = true;
    return dialServerStarted;
}


var startAppServer = function() {
    var ipaddr = localIpAddress();

    if (appServerStarted == true) {
        LogInfo("App server already started!  Ignoring startAppServer()");
        return appServerStarted;
    }

    if (ipaddr == '0.0.0.0') {
        LogInfo("Networking not yet started!  Can't yet start App server!");
        return appServerStarted;
    }

    server.on("connection", function(socket) {
        var socketId = nextSocketId++;
        sockets[socketId] = socket;
        LogDebug("socket " + socketId + " opened");

        socket.on("close", function() {
            LogDebug("socket " + socketId + " closed");
            delete sockets[socketId];
        });
    });

    server.on("close", function() {
        LogInfo("App server " + ipaddr + ":" + privSettings.dial_port + " closed!");
    });

    server.on("error", function(err) {
        LogErr("App server error listening on port " + privSettings.dial_port +
            ":\n" + err.stack);
        //server.close();
    });


    // Timeout after 10 seconds of not connecting
    //server.timeout = 10000;

    server.listen(privSettings.dial_port, ipaddr, function() {
        LogInfo("App listener bound to: " + ipaddr + ":" + privSettings.dial_port);
    });

    io = require('socket.io').listen(server);


    //=============================================================================
    //
    // Proxy for listening for JSON messages to be sent back/forth between
    // client app and server webApp
    //
    //=============================================================================

    io.sockets.on('connection', function(socket) {
        LogDebug('*** SOCKET CONNECTION! ***');

        socket.on('send_message', function(data) {
            // Forward control message from client app to Depict Frame Player
            LogDebug("Got send_message: " + data);
            io.sockets.emit('control_message', data);
        });

        socket.on('status_message', function(data) {
            // Forward status messages from Depict Frame Player back to client app(s)
            LogDebug("Got status_message: " + data);
            //socket.broadcast.emit('status_message', data);
            io.sockets.emit('status_message', data);

            // Update cached player status for ajax querying
            player_status = data;
        });

        socket.on('disconnect', function() {
            LogDebug("*** SOCKET DISCONNECT!! ***");
        });

        socket.on('close', function() {
            LogDebug("*** CLOSING SOCKET! ***");
        });

    });

    LogInfo("DIAL App Server: listening on port: " + privSettings.dial_port);
    appServerStarted = true;
    doRefresh = true;

    return appServerStarted;
}

var stopDialServer = function(cb) {
    LogInfo("Closing DIAL server...");

    // Stop accepting new connections
    dialServer.close( function() {
        LogInfo("DIAL server closed.");
        dialServerStarted = false;
        if (cb) cb();
    });

    // Now close existing connections
    for (var socketId in dialSockets) {
        LogDebug("Socket " + socketId + " closed");
        dialSockets[socketId].destroy();
    }

    dialServer.getConnections(function(err, count) {
        if (count > 0 || err) {
            LogInfo("DIAL server connections: " + count + ", err: " + err);
        }
    });
}

var stopAppServer = function(cb) {
    try {
        //io.sockets.emit('disconnect');
        io.httpServer.close();
    }
    catch (e) { }

    LogInfo("Closing App server...");

    // Stop accepting new connections
    server.close( function() {
        LogInfo("App server closed.");
        appServerStarted = false;
        if (cb) cb();
    });

    // Now close existing connections
    for (var socketId in sockets) {
        LogDebug("Socket " + socketId + " closed");
        sockets[socketId].destroy();
    }

    server.getConnections(function(err, count) {
        if (count > 0 || err) {
            LogInfo("App server connections: " + count + ", err: " + err);
        }
    });
}


var startServer = function() {
    var statusCode;
    var ipaddr = localIpAddress();

    if (ipaddr == '0.0.0.0') {
        LogInfo("Networking not yet started!  Can't yet start DIAL server!");
        return;
    }

    // If we're able to connect to WAN, that means that we also have a valid
    // Wi-Fi connection.  Let's record that we've at least once been able to
    // connect using the current Wi-Fi user/password.  This will cause the
    // DepictSettingsManager try more aggressively to use this network in
    // the future should Wi-Fi go down.  Otherwise, it will go into "Setup"
    // mode and enable the hotspot access point.
    readUserSettings(settings);
    try {
        // On development boards, we might have wired ethernet, so
        // explicitly checking that ssid is something valid first.  Not
        // strictly required but, what the heck!  Makes it easier.
        if (settings.network.ssid != "") {
            settings.network.valid = true;
            if (privSettings.pending_friendly_name != null) {
                settings.friendly_name = privSettings.pending_friendly_name;
                privSettings.pending_friendly_name = null;
            }
            writeUserSettings(settings);
        }
    }
    catch(e) {
        LogErr("Unable to set settings.network.valid=true");
    }

    while (!dialServerStarted && !appServerStarted) {
        startUdpMcastServer();
        startDialServer();
        startAppServer();
    }

    // Let's just go ahead and automatically start our main player app
    statusCode = startApplication("DepictFramePlayer");
    if  (statusCode != 200 && statusCode != 201) {
        LogErr("Unable to automatically start DepictFramePlayer: status = " +
            statusCode);
    }
}

var stopServersCallback = function() {
    if (appServerStarted == false && dialServerStarted == false) {
        LogInfo("*** DONE CLOSING SERVERS! ***");
    }
}

var stopServer = function() {
    LogInfo("Closing DIAL servers...");

    //stopApplication('DepictFramePlayer');
    stopUdpMcastServer();
    stopAppServer(stopServersCallback);
    stopDialServer(stopServersCallback);
}

var Quit = function() {
    LogInfo("Exiting!");
    LogInfo("stop DepictFramePlayer");
    process.exit();
}

process.on('exit', function() {
    LogInfo("dial.js exiting!  Bye!");
});

//-------------------------------------------------------------
// Action starts here!
//-------------------------------------------------------------

var onConnectWifi = function() {
    LogInfo("onConnectWifi");
    startHotspotServer();
}

var onConnectWAN = function() {
    LogInfo("onConnectWAN");
    startServer();
    doRefresh = true;
    refreshFrame();
}

var onDisconnectWifi = function() {
    LogInfo("onDisconnectWifi");
    stopHotspotServer();
}

var onDisconnectWAN = function() {
    LogInfo("onDisconnectWAN");
    stopServer();
}


// State Machine:  Deals with transitions between various
// networking connection scenarios.  We may have full internet
// connection, or only connection to home network (WiFi) but not
// to external WAN (e.g. router is disconnected), or a new
// WiFi "hotspot" (e.g. when setting up initial WiFi connection).

var stateMachine = function(evt) {
    LogDebug("stateMachine: state=" + currentState + ", event=" + evt);

    switch(currentState) {
        case STATE_INITIAL:
            switch (evt) {
                case EVENT_CONNECT_WIFI:
                    LogInfo("stateMachine: Starting with only WiFi!");
                    onConnectWifi();
                    currentState = STATE_CONNECTED_WIFI;
                    break;

                case EVENT_CONNECT_WAN:
                    LogInfo("stateMachine: Starting with good internet!");
                    onConnectWifi();
                    onConnectWAN();
                    currentState = STATE_CONNECTED_WAN;
                    break;

                case EVENT_DISCONNECT_WIFI:
                    currentState = STATE_DISCONNECTED_WIFI;
                    break;

                case EVENT_DISCONNECT_WAN:
                    currentState = STATE_DISCONNECTED_WAN;
                    break;

                case EVENT_STOP:
                    break;

                default:
                    LogErr("stateMachine: Invalid event: " + evt);
                    break;
            }
            startNewPlayer = true;
            break;

        case STATE_CONNECTED_WIFI:
            switch (evt) {
                case EVENT_CONNECT_WIFI:
                    break;

                case EVENT_CONNECT_WAN:
                    LogInfo("stateMachine: Got WAN back!");
                    onConnectWAN();
                    currentState = STATE_CONNECTED_WAN;
                    break;

                case EVENT_DISCONNECT_WIFI:
                    LogInfo("stateMachine: Lost WiFi!");
                    onDisconnectWifi();
                    currentState = STATE_DISCONNECTED_WIFI;
                    break;

                case EVENT_DISCONNECT_WAN:
                    onDisconnectWAN();
                    break;

                case EVENT_STOP:
                    LogInfo("stateMachine: Restarting WiFi!");
                    onDisconnectWifi();
                    currentState = STATE_INITIAL;
                    break;

                default:
                    LogErr("stateMachine: Invalid event: " + evt);
                    break;
            }
            break;

        case STATE_CONNECTED_WAN:
            switch (evt) {
                case EVENT_CONNECT_WIFI:
                    break;

                case EVENT_CONNECT_WAN:
                    break;

                case EVENT_DISCONNECT_WIFI:
                    LogInfo("stateMachine: Lost both WiFi and WAN!");
                    onDisconnectWAN();
                    onDisconnectWifi();
                    currentState = STATE_DISCONNECTED_WIFI;
                    break;

                case EVENT_DISCONNECT_WAN:
                    LogInfo("stateMachine: Lost WAN!");
                    onDisconnectWAN();
                    currentState = STATE_DISCONNECTED_WAN;
                    break;

                case EVENT_STOP:
                    LogInfo("stateMachine: Restarting WiFi!");
                    onDisconnectWAN();
                    onDisconnectWifi();
                    currentState = STATE_INITIAL;
                    break;

                default:
                    LogErr("stateMachine: Invalid event: " + evt);
                    break;
            }
            break;

        case STATE_DISCONNECTED_WIFI:
            switch (evt) {
                case EVENT_CONNECT_WIFI:
                    LogInfo("stateMachine: Got WiFi back!");
                    onConnectWifi();
                    currentState = STATE_CONNECTED_WIFI;
                    break;

                case EVENT_CONNECT_WAN:
                    LogInfo("stateMachine: Got both WiFi and WAN back!");
                    onConnectWifi();
                    onConnectWAN();
                    currentState = STATE_CONNECTED_WAN;
                    break;

                case EVENT_DISCONNECT_WIFI:
                    break;

                case EVENT_DISCONNECT_WAN:
                    LogInfo("stateMachine: Lost WAN without WiFi! Wierd!");
                    // Same thing as connect WiFi?
                    onDisconnectWifi();
                    currentState = STATE_DISCONNECTED_WAN;
                    break;

                case EVENT_STOP:
                    LogInfo("stateMachine: Restarting all internet connections!");
                    currentState = STATE_INITIAL;
                    break;

                default:
                    LogErr("stateMachine: Invalid event: " + evt);
                    break;
            }
            break;

        case STATE_DISCONNECTED_WAN:
            switch (evt) {
                case EVENT_CONNECT_WIFI:
                    LogInfo("stateMachine: Wierd! Got WiFi back with no WAN!");
                    onConnectWifi();
                    currentState = STATE_CONNECTED_WIFI;
                    break;

                case EVENT_CONNECT_WAN:
                    LogInfo("stateMachine: Got WAN back!");
                    onConnectWAN();
                    currentState = STATE_CONNECTED_WAN;
                    break;

                case EVENT_DISCONNECT_WIFI:
                    LogInfo("stateMachine: Lost WAN, now losing WiFi!");
                    onDisconnectWifi();
                    currentState = STATE_DISCONNECTED_WIFI;
                    break;

                case EVENT_DISCONNECT_WAN:
                    break;

                case EVENT_STOP:
                    LogInfo("stateMachine: Stop WiFi!");
                    onDisconnectWifi();
                    currentState = STATE_INITIAL;
                    break;

                default:
                    LogErr("stateMachine: Invalid event: " + evt);
                    break;
            }
            break;

        default:
            LogErr("stateMachine: Invalid state: " + currentState);
            break;
    }
}

//====================
// Start your engines!
//====================

// Periodically check that we have an internet connection.  This
// timer tick is what moves the state machine through its transitions
var timerId = setTimeout(heartbeatTick, 0);
