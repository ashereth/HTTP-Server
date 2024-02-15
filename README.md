# HTTP Server

This directory contains source code and other files for Assignment 2.

# Description
httpserver.c is a simple implementation of an http server that can process get and put requests sent \
by a client. httpserver.c makes use of socket functionality within the asgn2_helper_funcs.a file \
these functions are described in the asgn2_helper_funcs.h file. Both of these files were provided by \Professor Kerry Veenstra and written by his TA Andrew Quinn while in the class CSE130 at UC Santa Cruz.


# Compiling
To compile the needed executables run $ make. \
To remove all executables run $ make clean.

# Running
This server is meant to be run within Linux/Unix environment. To start the server run $ ./httpserver \(port). Where port is the port that you want the server to connect to.

# Usage
You can send get or put requests to this server via a client using netcat. For example on another terminal on the same computer run:\
$ printf "GET /file.txt HTTP/1.1\r\n\r\n" | nc -N localhost 1234 \
this will run a get command provided that the server is running on port 1234.

# Command Format
Commands are formatted as: (command) /(uri) HTTP/1.1\r\n(optional header fields)\r\n\r\n(optional message body)

## GET commands.
A get command simply reads the file from the provided uri and prints its contents in the response message. A valid GET request (given the file file.txt exists) can look like : \
GET /file.txt HTTP/1.1\r\n\r\n

## Put Commands
A PUT command will either open or create a file with the given URI and write the (message body) to that file up to the supplied number of bytes through the Content-Length: (bytes) header field. \
A valid PUT request may look like: \
PUT /file.txt HTTP/1.1\r\nContent-Length: 12\r\n\r\nHello World!