---
title: "Zero to Blog Engine: HTTP from Scratch"
date: 2024-06-01
slug: http-from-scratch
tags: cpp, networking, http, loom
excerpt: "Step one of building a blog engine: parsing HTTP requests with nothing but socket syscalls."
---

Most web frameworks hide the HTTP protocol behind layers of abstraction. Today we're going the other way — building an HTTP server from raw TCP sockets.

## The Socket Dance

Every HTTP server starts the same way:

```cpp
int fd = socket(AF_INET, SOCK_STREAM, 0);

sockaddr_in addr{};
addr.sin_family = AF_INET;
addr.sin_port = htons(8080);
addr.sin_addr.s_addr = INADDR_ANY;

bind(fd, (sockaddr*)&addr, sizeof(addr));
listen(fd, SOMAXCONN);
```

Four syscalls and you have a listening socket. The operating system handles the TCP handshake, buffering, retransmission, and flow control. All you need to do is read bytes and write bytes.

## Parsing HTTP

An HTTP request is just text:

```
GET /post/hello-world HTTP/1.1\r\n
Host: localhost:8080\r\n
Accept: text/html\r\n
\r\n
```

The parser needs to extract three things:

1. **Method and path** from the request line
2. **Headers** as key-value pairs
3. **Body** (everything after the blank line)

```cpp
HttpRequest parse(const std::string& raw)
{
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;

    // Request line: "GET /path HTTP/1.1"
    std::getline(stream, line);
    auto sp1 = line.find(' ');
    auto sp2 = line.find(' ', sp1 + 1);
    req.method = line.substr(0, sp1);
    req.path = line.substr(sp1 + 1, sp2 - sp1 - 1);

    // Headers
    while (std::getline(stream, line) && line != "\r")
    {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        auto key = line.substr(0, colon);
        auto val = line.substr(colon + 2);
        req.headers[key] = val;
    }

    return req;
}
```

This is naive — it doesn't handle chunked encoding, multi-line headers, or malformed requests. But it works for a blog. The first version doesn't need to be perfect. It needs to be correct enough to serve HTML.

## Responding

Sending a response is the same pattern in reverse:

```cpp
std::string response;
response += "HTTP/1.1 200 OK\r\n";
response += "Content-Type: text/html\r\n";
response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
response += "\r\n";
response += body;

send(client_fd, response.c_str(), response.size(), 0);
```

That's it. You now have a working HTTP server. It handles one connection at a time, blocks on I/O, and falls over under any real load. We'll fix all of that in the next post.

## What We Have

About 80 lines of code and a server that can:

- Accept TCP connections
- Parse GET requests
- Send HTML responses
- Crash gracefully when anything unexpected happens

Next: making it handle more than one connection at a time with epoll.
