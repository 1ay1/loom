---
title: DNS Is Not Simple
date: 2024-02-14
slug: dns-is-not-simple
tags: networking, distributed-systems
excerpt: "It's just a DNS change" is the scariest sentence in infrastructure. Here's what actually happens when you resolve a domain.
---

"Just update the DNS record." Five words that have caused more outages than any deployment pipeline.

## The Resolution Chain

When your browser resolves `example.com`:

1. Check local `/etc/hosts` → miss
2. Check OS resolver cache (systemd-resolved, nscd) → miss
3. Query configured recursive resolver (e.g., 8.8.8.8)
4. Recursive resolver checks its cache → miss
5. Query root nameserver (`.`) → "ask `.com` nameservers"
6. Query `.com` TLD nameserver → "ask `ns1.example.com`"
7. Query `ns1.example.com` → `93.184.216.34`

Each step caches the result for the TTL duration. A 1-hour TTL means 13 levels of caching infrastructure will serve stale data for up to an hour after you change the record.

## TTL: The Lie Everyone Believes

You set a TTL of 300 seconds. Surely everyone sees the new record within 5 minutes? No.

- Some resolvers enforce minimum TTLs (often 30 seconds, sometimes 300)
- Some resolvers enforce maximum TTLs (capping at 1 day regardless of your setting)
- Java's default DNS cache is **infinite** unless you set `networkaddress.cache.ttl`
- Browser DNS caches have their own TTL logic
- CDN edge nodes cache DNS independently

In practice, a DNS change propagates to ~95% of clients within 2× the old TTL. The remaining 5% trickle in over hours. For critical migrations, pre-lower the TTL to 60 seconds days in advance.

## CNAME: The Chain That Breaks

A CNAME record says "this name is an alias for that name." Simple, except:

- CNAMEs can't coexist with other record types at the same name
- CNAME at zone apex (`example.com` itself) is technically illegal per RFC
- CNAME chains (A → B → C) multiply latency and failure points
- Some resolvers limit chain depth to 8

This is why Cloudflare, AWS, and others invented "ALIAS" or "ANAME" pseudo-records that resolve server-side and return A records to clients.

## DNS Over HTTPS/TLS

Traditional DNS is unencrypted UDP on port 53. Anyone on the network path can see and modify queries. DNS over HTTPS (DoH) and DNS over TLS (DoT) fix this:

- **DoT** (port 853): TLS wrapper around DNS protocol. Simple, efficient.
- **DoH** (port 443): DNS queries as HTTPS requests. Harder to block because it's indistinguishable from regular HTTPS.

Firefox and Chrome default to DoH. This means corporate DNS-based security filters stop working unless the browser is configured to use the corporate resolver.

## DNS as Load Balancer

Round-robin DNS returns multiple A records. The client picks one (usually the first). This gives basic load distribution but:

- No health checking — dead backends stay in rotation until TTL expires
- Client caching means the same client hits the same backend
- Different clients may cache different orderings
- No awareness of backend load or geographic proximity

For anything serious, use a proper load balancer. DNS round-robin is a last resort, not a strategy.

## Debugging DNS

```bash
# Full resolution trace
dig +trace example.com

# Query specific nameserver
dig @8.8.8.8 example.com

# See all record types
dig example.com ANY

# Reverse lookup
dig -x 93.184.216.34

# Check TTL and caching
dig example.com | grep -E "^example"
# example.com.  187  IN  A  93.184.216.34
# 187 = seconds remaining on cached TTL
```

The most common DNS debugging mistake: testing from your machine (which has the new record cached) and assuming everyone else sees it too. Always test from multiple vantage points.

DNS is 40 years old and runs the internet. It's also a distributed eventual-consistency system with no transactions, no rollback, and TTL-based cache invalidation. Respect it.
