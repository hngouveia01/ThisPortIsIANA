# ThisPortIsIANA
A service that receives a port/protocol and tells if it is registered at IANA and, if so, returns its service name.

## Description
All communication is done by using UDP UNIX socket.

Clients may send requests in this format: "PORT/PROTOCOL" i.e.: "22/UDP" or "8080/TCP" or "214/414"

The response would be: "SSH", "HTTP", "UNKNOWN", respectivally.

The answer will be UNKNOWN if the port/protocol could not be identified or is not registered at IANA.

If you preceed your request with a '-', the server will send response in this format:

"1/SERVICE_NAME" if port is registered at IANA.

"\0" if port is not registered at IANA.

That is:

"-80/TCP" => "1/HTTP"

"-22/UDP => "1/TELNET"

"21/TCP => "FTP"

"452/AAA" => "UNKNOWN"

"24A/355" => "\0"

