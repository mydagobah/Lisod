This file describes the design of Lisod - a concurrent server implemented in
the course of 'Computer Networking (15441/641) Fall2012' at CMU, by Wenjun
Zhang (wenjunzh@andrew.cmu.edu).

***** Check point 1 - concurrent echo server *****

The core design of this part is to use 'select' function to handle multiple connections
concurrently. The main data structure used is a 'pool' to record a 'read_set'
and 'ready_set'. Whenever a new connection or a new request arrives, the server
will be get notified by 'select' and pass the information through 'pool' and get
these new events handled respectively.

***** Check point 2 - HTTP 1.1 HEAD GET POST *****

The part the server adds support for HTTP methods including HEAD, GET and
POST. The server also supports persistent connection handling. The server is
tolerant to any requests and keeps running. It returns proper error messages
to client whenever request is not following RFC 2616. 
Add daemonize the server.
For POST, a proper parse of the request body needs to be done.

***** Check point 3 - HTTPS via TLS *****

To be done!

***** Check point 4 - CGI *****

To be done!
