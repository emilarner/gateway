#!/usr/bin/env python3

import socket
import sys
import os
import json
import time
import subprocess
import threading
import multiprocessing

import gunicorn
import gunicorn.app
import gunicorn.app.base

from cstruct2.cstruct2 import cstruct2
from cstruct2.cstruct2_utils import SocketWrapper
from flask import Flask, request, Response
from enum import IntEnum

dir_path: str = os.path.dirname(os.path.realpath(__file__))

app = Flask(__name__)
gateway = None

PAGE_LOCATION = os.getenv("PAGE_LOCATION", default="/var/gateway-page.html")
PAGE_OK_LOCATION = os.getenv("PAGE_OK_LOCATION", default="/var/gateway-page-ok.html")
PAGE_ERR_LOCATION = os.getenv("PAGE_ERR_LOCATION", default="/var/gateway-page-err.html")

CONFIG_LOCATION = os.getenv("CONFIG_LOCATION", default="/var/gateway-config")
LAUNCHER_CONFIG_LOCATION = os.getenv("LAUNCHER_CONFIG_LOCATION", default="/var/gateway-launcher-config.json")

with open(PAGE_LOCATION, "r") as fp:
    PAGE_CONTENTS = fp.read()

with open(PAGE_OK_LOCATION, "r") as fp:
    PAGE_OK_CONTENTS = fp.read()

with open(PAGE_ERR_LOCATION, "r") as fp:
    PAGE_ERR_CONTENTS = fp.read()

with open(LAUNCHER_CONFIG_LOCATION, "r") as fp:
    LAUNCHER_CONFIG = json.load(fp)

ip_offenses = {}

class OffenderStatuses(IntEnum):
    TimedOut = 0
    RecentlyFreed = 1
    NotTimedOut = 2
    NotTimedOutButFreed = 3

class IPOffender:
    def __init__(self):
        self.failed_attempts = 1
        self.last_attempt = int(time.time())
    
    def add_attempt(self):
        self.failed_attempts += 1
        self.last_attempt = int(time.time())

    def in_timeout(self) -> OffenderStatuses:
        if self.failed_attempts == LAUNCHER_CONFIG["attempts"]:
            if int(time.time()) - self.last_attempt >= LAUNCHER_CONFIG["timeout"]:
                return OffenderStatuses.RecentlyFreed

            return OffenderStatuses.TimedOut
        
        if int(time.time()) - self.last_attempt >= LAUNCHER_CONFIG["timeout"]:
            return OffenderStatuses.NotTimedOutButFreed

        return OffenderStatuses.NotTimedOut



@cstruct2
class AuthenticateCommand:
    command: int = 1
    ip: int = 4
    expiration: int = 4

class GatewayCommunication:
    def __init__(self, port: int, password: str, host = "0.0.0.0"):
        self.host = host
        self.port = port

        self.password = password
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        self.sock_wrapper = SocketWrapper(self.socket)

    
    def connect(self):
        while True:
            try:
                self.socket.connect((self.host, self.port))
                print("Connected to the Gateway Master server socket.")
                break
            except Exception as error:
                sys.stderr.write(f"Error connecting to Gateway socket: {str(error)}\n")
                sys.stderr.flush()
                #os.abort()
                
                time.sleep(.500)
                continue

    def authenticate(self, ip: str, given_password: str, expiration: int) -> bool:
        if given_password != self.password:
            return False

        AuthenticateCommand.to_stream({
            "command": 0,
            "ip": socket.inet_aton(ip),
            "expiration": expiration
        }, self.sock_wrapper)

        return True



@app.route("/authenticate/", methods=["POST"])
def authenticate():
    if "password" not in request.form or "expiration" not in request.form:
        return Response("'password' and/or 'expiration' is not present.", status=500, mimetype="text/html")

    password: str = request.form["password"]
    expiration: str = request.form["expiration"]

    if expiration == "" or expiration == "0":
        expiration = -1

    else:
        try:
            expiration = int(expiration)
        except:
            return Response("wrong expiration", status=500)

    # Is the IP address flagged for wrongful credentials?
    if request.remote_addr in ip_offenses:
        status: OffenderStatuses = ip_offenses[request.remote_addr].in_timeout()
        if status in [OffenderStatuses.NotTimedOutButFreed, OffenderStatuses.RecentlyFreed]:
            del ip_offenses[request.remote_addr]

        elif status == OffenderStatuses.TimedOut:
            offender: IPOffender = ip_offenses[request.remote_addr]

            return Response(
                PAGE_ERR_CONTENTS.format(
                    remaining=LAUNCHER_CONFIG["attempts"] - offender.failed_attempts,
                    total=LAUNCHER_CONFIG["attempts"],
                    timeout=(
                        "You are timed out, your time out is: " + 
                        str(LAUNCHER_CONFIG["timeout"]) + " seconds from the last attempt."
                    )
                ), 
                status=429, 
                mimetype="text/html"
            )
        

    # Did it work?
    if gateway.authenticate(request.remote_addr, password, expiration):
        return Response(PAGE_OK_CONTENTS, status=200, mimetype="text/html")
    else:
        # Mark it against the IP. 
        if request.remote_addr not in ip_offenses:
            ip_offenses[request.remote_addr] = IPOffender()
        else:
            ip_offenses[request.remote_addr].add_attempt()

        offender: IPOffender = ip_offenses[request.remote_addr]

        return Response(
            PAGE_ERR_CONTENTS.format(
                remaining=LAUNCHER_CONFIG["attempts"] - offender.failed_attempts,
                total=LAUNCHER_CONFIG["attempts"],
                timeout=""
            ), 
            status=403, 
            mimetype="text/html"
        )

@app.route("/", methods=["GET"])
def main_page():
    return Response(PAGE_CONTENTS, status=200, mimetype="text/html")


def gateway_launcher(config: str):
    process = subprocess.Popen([dir_path + "/gateway-master", config])
    process.wait()
    
    # If it ever returns, that's bad!
    sys.stderr.write("The gateway master server terminated, that's bad! Aborting\n")
    os.abort()


# to integrate gunicorn 


def number_of_workers():
    return (multiprocessing.cpu_count() * 2) + 1

class StandaloneApplication(gunicorn.app.base.BaseApplication):

    def __init__(self, app, options=None):
        self.options = options or {}
        self.application = app
        super().__init__()

    def load_config(self):
        config = {key: value for key, value in self.options.items()
                  if key in self.cfg.settings and value is not None}
        for key, value in config.items():
            self.cfg.set(key.lower(), value)

    def load(self):
        return self.application


if __name__ == "__main__":
    port = LAUNCHER_CONFIG["port"]
    password = LAUNCHER_CONFIG["password"]
    remote = None

    if "--remote" in sys.argv:
        remote = sys.argv[sys.argv.index("--remote") + 1]

    gateway_launcher_thr = threading.Thread(target=gateway_launcher, args=(CONFIG_LOCATION,))
    gateway_launcher_thr.start()

    gateway = GatewayCommunication(60102, password)
    gateway.connect()

    options = {
        'bind': '%s:%s' % ('0.0.0.0', port),
        'workers': number_of_workers(),
    }

    StandaloneApplication(app, options).run()

    #app.run(host="0.0.0.0", port=port)