---
title: The TCP State Machine Nobody Teaches You
date: 2024-04-18
slug: tcp-state-machine
tags: networking, linux, systems
excerpt: TCP has 11 states. Most developers know 3. The other 8 are where all the production bugs hide.
---

Everyone knows the TCP three-way handshake: SYN, SYN-ACK, ACK. But TCP has 11 states, and the transitions between them explain most networking bugs you'll ever hit in production.

## The Full State Diagram

```
CLOSED → SYN_SENT → ESTABLISHED → FIN_WAIT_1 → FIN_WAIT_2 → TIME_WAIT → CLOSED
CLOSED → LISTEN → SYN_RCVD → ESTABLISHED → CLOSE_WAIT → LAST_ACK → CLOSED
```

The happy path is simple. The edge cases are where things get interesting.

## TIME_WAIT: The 2MSL Problem

After closing a connection, the socket enters TIME_WAIT for 2 × Maximum Segment Lifetime (typically 60 seconds on Linux). During this time, the socket's address:port tuple can't be reused.

If you're running a high-connection-rate service, you'll see thousands of TIME_WAIT sockets:

```
$ ss -s
TCP:   23847 (estab 1204, closed 21893, orphaned 0, timewait 21890)
```

21,890 sockets doing nothing but waiting. Solutions:

- `SO_REUSEADDR`: allows binding to a TIME_WAIT socket's port
- `tcp_tw_reuse = 1`: allows reusing TIME_WAIT sockets for outbound connections
- Connection pooling: reuse ESTABLISHED connections instead of creating new ones

## CLOSE_WAIT: The Connection Leak

CLOSE_WAIT means the remote side closed, but your application hasn't called `close()` yet. If you see CLOSE_WAIT sockets accumulating, you have a resource leak in your code.

```
$ ss -tanp | grep CLOSE-WAIT | wc -l
4728
```

4,728 connections where the remote hung up and your code didn't notice. Common causes:

- Not reading from the socket (so you never see the EOF)
- Exception handlers that skip `close()`
- Event loops that don't handle disconnect events

This is almost always a bug in your code, not a network issue.

## SYN_RCVD and SYN Floods

When a server receives a SYN, it sends SYN-ACK and enters SYN_RCVD. It stays there until the client completes the handshake with ACK. A SYN flood fills the SYN backlog with half-open connections:

```
$ ss -tn state syn-recv | wc -l
128
```

If this equals your `tcp_max_syn_backlog`, legitimate connections are being dropped. SYN cookies (`net.ipv4.tcp_syncookies = 1`) solve this by encoding connection state in the SYN-ACK sequence number — no backlog entry needed.

## RST: The Angry Close

A RST packet immediately terminates a connection without the graceful FIN handshake. You'll see RSTs when:

- Connecting to a port with no listener
- Sending data to a connection the remote already closed
- Firewall resets (connection tracking timeout)
- Application crashes without closing sockets

The nastiest case: a firewall between client and server has a shorter timeout than either endpoint. The endpoints think the connection is alive; the firewall sends RSTs to new packets on that 5-tuple. Everything works locally, breaks in production.

## Nagle's Algorithm and Delayed ACK

Nagle's algorithm buffers small writes until the previous segment is ACKed. Delayed ACK waits up to 40ms before sending an ACK, hoping to piggyback it on a data response. Together, they can add 40ms latency to small writes:

1. Client sends small request (Nagle buffers)
2. Actually Nagle sends it (only one outstanding segment)
3. Server reads, processes, sends response
4. Server's TCP delays the ACK for up to 40ms
5. Client's Nagle waits for the ACK before sending more

`TCP_NODELAY` disables Nagle. Set it on any latency-sensitive connection — HTTP servers, databases, RPC. The throughput cost is negligible on modern networks.

## Debugging TCP State Issues

The most useful commands:

```bash
# Connection states summary
ss -s

# All connections with process info
ss -tanp

# Connections in specific state
ss -tn state time-wait
ss -tn state close-wait

# Socket options and buffer sizes
ss -tmi

# Per-connection retransmit stats
ss -ti
```

If `Retrans` is non-zero in `ss -ti` output, packets are being lost. If `rto` is large, the connection is backing off. If `cwnd` is 1, congestion control has throttled the connection to minimum.

TCP state issues are the #1 category of networking bugs in production. Learning the state machine saves hours of debugging.
