# gateway

Create a gateway that only allows authenticated users to access port-forwarded services.

## Description and Rationale

Gateway, not to be confused with anything else named that or anything, is a small tool to allow services that are supposed to exposed over the open internet to clients that are only whitelisted by IP, either through a configuration file or through dynamic authentication.  It proxies requests made to certain outbound services to services that are on the LAN of whatever server is running Gateway. It is created in spite of VPNs and proxies already existing due to the seamless nature it provides for many devices, for which using VPNs and proxies on would be a hassle. Take the example rationale for using Gateway over VPNs/proxies:

I have IP cameras that I want my family and I to be able to access away from home, which would require me to port forward their management and access system. However, the access system they have has a vulnerability which allows root access to said access and management system, which is obviously not ideal. Without Gateway, I have to either create a VPN or proxy server that is port forwarded instead, so I can then access the management system which would be available on the server's LAN. However, when dealing with mobile devices and people who are not as technically inclined, this is cumbersome. A VPN typically routes the entire Internet connection of the mobile device through the server, and a proxy is something which is not seamless--the application in question needs to be able to support it for it to be usable. Thus, it would be better to have exposed services that whitelist select IPs in order to proxy the packets to the actual intended destination. Gateway is that solution. If the IP of the mobile device changes, then an authentication endpoint or webpage can dynamically allow that IP address upon providing correct credentials.

Gateway has no current support for IPv6 addresses and no plans are made to accept them, since they are not widely used. However, it would be fairly trivial to add IPv6 support. Gateway also does not support Windows machines and it doesn't plan to, but you can just use a VM or dockerize it! 

The Gateway Master server, which sets up relays that forward authenticated requests to their intended destination, is written in C++, for purposes of maximizing speed. The Gateway Launcher, on the other hand, is written in Python, to dynamically authenticate users based off of an HTTP backend and to launch the actual Gateway Master server. Since the bottleneck of this system does not exist in the initial IP authentication and Gateway Master server launching part, it is reasonable to write this in Python to reduce complexity. 

Gateway is not intended for high-volume traffic, relying off of relatively outdated and simplistic I/O multiplexing tools to achieve its goals (select and threads, to be exact). Its purpose is to provide an easy solution to the problem displayed above, while remaining relatively minimal and very secure.

## Installation

To install Gateway as a whole, run the following set of commands after you've (hopefully) skimmed through all of the source code:

    make
    sudo make install

This will build and install Gateway Master Server, Gateway Launcher, and the default configuration files. The Gateway Master Server will be installed to path as `gateway-master` and the Gateway Launcher will be installed to path as `gateway-launcher`--the main program you want to run, typically. Python dependencies in *requirements.txt* will automatically be installed. 

Now for Gateway to start up automatically, you must configure it as a service, but try not to make it run as *root*, for security precautions. You do not typically need to proxy this through a large HTTP server, such as nginx or apache, since Gateway outbound relays are not intended to be bound on *root*-only ports--and as mentioned before, Gateway isn't performance critical. Besides, Gateway's launcher and authenticator system doesn't have functionality to accept proxies, since it only looks at the direct IP address of whoever is connecting--it doesn't consider X-Forwarded-For headers sent by nginx and friends.


## The Master Server

The Gateway Master Server is written in C++ and launches relay nodes that bind to an outbound port, which are intended to be forwarded to the open internet. These relay nodes map to a certain service on the LAN of the wherever the master server is being hosted, so they map an outbound port to an IP address and port. When someone connects to a forwarded outbound port, a connection to the mapped IP address and port is made, and using I/O multiplexing, every packet that the client of the outbound port sends is sent to the newly made connection to the mapped service and vice versa, provided that the client is in the allowed IPs database. If the IP is not in the database, the connection from them is closed immediately.

The master server accepts a configuration file location from the first command-line argument it is given. From there, it can accept IPs that are to be whitelisted, along with mappings of outbound ports to the IP addresses and ports they should reroute packets to. One can also specify other settings, but it's overall very minimal and simplistic, by explicit design. Comments in the configuration file are denoted by beginning a line with a # character. Bug: new lines cause strange behavior, so don't use them (yet).

 - An IP that is to be whitelisted is simply a line in the configuration
   file.
 - A mapping between an outbound port and an inbound service is denoted by beginning a line by the outbound port, then adding a space, followed by the inbound service in ip:port format.
 - A maximum, global expiration time for dynamically whitelisted IP addresses is given by beginning a line by 'expiration', then adding a space, followed by the maximum expiration time in seconds.

Here is an example configuration file, taken from the default configuration file that is given in this repository:

    # comments denoted with hashtag are ignored
    # a mapping between outbound to inbound
    65500 192.168.1.130:8080
    # a hardcoded, no exceptions IP whitelist
    192.168.1.130
    127.0.0.1
    # a hardcoded, superceding expiration for dynamically authenticated IPs
    expiration 60

   
The master server itself exposes a port 60102 that is to be used to dynamically authenticate IPs by clients on the LAN, though it should NOT be forwarded to the wider Internet. The master server only allows one client to be connected at any given time, and it will block other clients from connecting if one is already connected. Packed binary structures using Little Endian integers will be used to communicate with the server.

The server will accept a *uint8_t* (an unsigned 8-bit integer) denoting the command that it is going to process. As of now, the only command recognized is the *Authenticate* command, which has the following structure that will be then read by the server:

    struct  __attribute__((__packed__)) AuthenticateCommand
    {
	    in_addr_t  ip;
	    uint32_t  expiration_seconds;
    };

*ip* is the binary representation of the IPv4 address to be authenticated, and *expiration_seconds* is the number of seconds that the IPv4 address is set to expire from being allowed. If the expiration is set to *-1*, there is no expiration. If there is a global maximum expiration time set, the expiration given cannot be over that set expiration time, which includes no expiration. If it goes over, then the maximum expiration time will be the time set for the expiration time; otherwise, the expiration given in the structure will be set for the IP address. IPs whitelisted in the configuration file are not affected by this and will be permanently whitelisted.

IPs that are dynamically whitelisted through the sending of this command are not only whitelisted for the duration that the master server is active, but are also whitelisted through being inserted in a small database that is loaded when the master server starts up again. However, if the IP is expired by the time the database is loaded, it won't be added to the database of allowed IPs in memory.

You really don't need to know any of the internal protocol workings, but I've explained it for those who do/want to implement their own master server client or launcher. 

## The Launcher & Authenticator 

A Python flask server is included in this repository, which hosts an HTTP server (through gunicorn) that has endpoints that take in credentials to decide whether it will send an authentication command to the Gateway Master server to allow that IP. In addition to that, the launcher launches the Gateway Master server and connects to it, so that it can validate IPs that come through the HTTP backend. The launcher, as it is called, has its own configuration file, but in JSON format, which is by default located in /var/gateway-launcher.json. The JSON file is in the following format, though I'll add some pseudo-JSON to comment which is what:

    {
	    "port": 8081, # the port that the HTTP server is bound to
	    "password": "this is a password", # the password for authentication
	    "attempts": 4, 
	    # ^ the number of attempts someone trying to authenticate has,
	    # before they are temporarily locked out and denied subsequent
	    # requests
	    "timeout": 3600,
	    # ^ the time in seconds that a person will be locked out
	    "multiplier": 1.75
	    # ^ for each time someone gets locked out after the first time
	    # the locked out time will be multiplied by this each time,
	    # rounded up to the nearest second
    }

Note that the launcher doesn't store people who fail the authentication in any permanent database. If you restart the launcher, it will give everyone a fresh and clean slate, including any timeout multipliers they have. Also note that if the timeout ever exceeds 4,294,967,296 seconds, the IP will be permanently banned for the duration that the launcher is active.

In addition to endpoints that accept credentials to try to dynamically whitelist an IP address, the flask server has a tiny login page for its main page, which is a form that will send a POST request to the authentication endpoint for you. 

Here is a comprehensive list of endpoints and pages that the flask launcher server will expose, with the methods they support. Note, if you aren't a developer that intends to use these for your advantage, this is of no use to you. Anyways:

 - **GET**: /
	 - The main index page, with the login form, as previously mentioned
 - **POST**: /authenticate/
	 - Authenticates the caller's IP address throughout the entirety of the master server, with expiration constraints.
	 - Accepts form data through a *urlencoded* format, the format that HTML uses with its form objects by default
	 - "password" is an argument that is sent to try to authenticate the user
	 - "expiration" is an argument for specifying the expiration time in seconds.
		 - If 0 is provided, there is no expiration desired
		 - Remember, the master server's global expiration time will override this, if it exists and if your expiration is bigger than it!
	 - Both arguments must be present or the endpoint returns a 500 error.



Upon invoking the /authenticate/ endpoint, several things can happen, depending on whether you're timed out or not, or whether you entered the correct password. If you entered the correct password, your IP is now authenticated, and the endpoint will return a 200 OK HTTP response code, along with some HTML stating that the authentication process was a success. 

If authentication failed, a 403 Forbidden HTTP status code will be returned and some HTML stating that you failed to authenticate, along with the number of attempts you have left. If you've been timed out because you used up all of your available attempts for this period, HTTP 429 Too Many Requests will be returned and HTML describing the number of seconds you must wait from the beginning of your timeout will be sent back. Note, the number of timeout seconds *won't* update upon subsequent requests, since it will always report back the number of seconds you were timed out for, not how many seconds left you must wait.

If the user-agent provided for accessing the /authenticate/ endpoint is *gateway-cli*, then no HTML denoting the status of the authentication will be sent, only the HTTP status codes will denote success, failure, or timeout. If the authentication failed, If timed out, the number of seconds of timeout will be sent.

The Gateway Launcher has some customization options for these pages. The Gateway Launcher will look in /var/ for: /var/gateway-page.html, which will be displayed on the main form page; /var/gateway-page-ok.html which will be displayed when you successfully are authenticated; and /var/gateway-page-err.html when authentication has failed or you have been timed out. You can change these files at will, by editing them after Gateway has been installed or editing the files as they are in the *defaults/* folder, then reissuing the installation command. 

*gateway-page-err.html* has some additional customization, namely through some Python string formatting specifiers, which sadly requires all other text that have braces to be doubled braced. The two keywords that Gateway will use for this page for formatting is: {remaining}, which is the number of attempts remaining; {total}, the total number of attempts one has; and {timeout}, a sentence describing the number of seconds you're timed out (in English), *only* when you are timed out--otherwise, it is left blank. If you are confused, look at *gateway-page-err.html* in the *defaults/* folder in this repository. It's just lazy Python string formatting to fill in the aforementioned variables.

You'd never really need to do this, but the launcher accepts certain environment variables to change the paths of HTML files and the configuration files. The Gateway Master server uses a command-line argument though, so isn't that inconsistent usage of tools? Bad, inconsistent, and not well thought out program! Using environment variables for the launcher makes more sense, since it is the *launcher*, it launches everything, the HTTP server and the Gateway master server. Thus, it is supposed to be configured as a service, which is more suited for using environment variables. But in reality, it *really* doesn't matter. Anyways, here are the environment variables that the launcher accepts and what they do:

 - PAGE_LOCATION
	 - The main path for the / HTML file, where the form and everything is.
 - PAGE_OK_LOCATION
	 - HTML path for when authentication is successful.
 - PAGE_ERR_LOCATION
	 - HTML path for when authentication fails (either due to bad creds/timeout)
 - CONFIG_LOCATION
	 - Path for where the Gateway Master server's configuration is
 - LAUNCHER_CONFIG_LOCATION
	 - Path for where the launcher's JSON-based configuration is.


