#!/usr/bin/python3

import requests
import os
import sys

def main():
    gateway_endpoint: str = None
    gateway_password: str = None
    gateway_expiration: str = None

    for i in range(len(sys.argv)):
        if i == 0:
            continue

        try:
            if sys.argv[i][0] == '-':
                if sys.argv[i] in ["-e", "--endpoint"]:
                    gateway_endpoint = sys.argv[i + 1]

                elif sys.argv[i] in ["-p", "--password"]:
                    gateway_password = sys.argv[i + 1]

                elif sys.argv[i] in ["-ex", "--expiration"]:
                    gateway_expiration = sys.argv[i + 1]

                else:
                    sys.stderr.write("Argument not recognized.\n")

        except IndexError:
            sys.stderr.write(f"{sys.argv[i]} requires an argument.")
            sys.exit(-1)

    
    if gateway_endpoint == None:
        sys.stderr.write("No endpoint provided... cannot continue")
        sys.exit(-1)

    if gateway_password == None:
        sys.stderr.write("No password provided... cannot continue")
        sys.exit(-2)

    if gateway_expiration == None:
        sys.stderr.write("Warning, no expiration provided...")
        gateway_expiration = 0

    
    response = requests.post(
        gateway_endpoint,
        data={
            "password": gateway_password,
            "expiration": gateway_expiration
        },
        headers={
            "User-Agent": "gateway-cli"
        }
    )
    



if __name__ == "__main__":
    main()