---
title: "The Distributed Systems Lie: You Don't Need Microservices"
date: 2026-03-27T01:04:00
slug: microservice-lies
tags: architecture, microservices, systems, devops
excerpt: Stack Overflow serves 1.3 billion page views a month with 9 web servers. You have 500 users and three Kubernetes clusters.
---

Let me tell you about a system that handles 1.3 billion page views every month.

It runs on **9 web servers**. Physical machines. Two data centers. The database is one primary, two replicas. The deployment is a PowerShell script. The architecture is, in the technical sense, deeply boring.

It's Stack Overflow. You've probably used it today.

Now tell me about your startup's infrastructure. Tell me about the Kubernetes cluster, the service mesh, the API gateway, the three message queues, the distributed tracing system you installed to figure out why things are slow, and the two engineers whose job titles are "platform" because the infrastructure became its own product.

How many users do you have?

---

## A Function Call vs. a Network Call

When your Order Service calls your Inventory Service, here is what happens at the hardware level:

1. Serialize the request object to JSON (or Protobuf, if you're fancy).
2. Form a TCP packet and send it over the network.
3. Inventory Service receives it, deserializes it back into an object.
4. Inventory Service processes it, serializes the response.
5. Response travels back.
6. Order Service deserializes it.

A function call inside a monolith: move an instruction pointer. Nanoseconds.

A service call over a network: six steps, one round-trip, DNS lookup if unlucky, TLS handshake overhead, a load balancer in the middle, 10–50ms on a good day, infinite milliseconds if Inventory Service is down.

You don't get this for free. You pay for it in every request, forever. Build a chain of five services that each call the next, and your p99 latency is no longer the slowest service — it's the *sum* of all of them, on their worst day, simultaneously.

## The Distributed Transactions Problem Is Not Solved

In a monolith with a shared database: you update a user's profile and their billing status in one transaction. It's atomic. Either both succeed or neither does. This has been solved since 1983.

In microservices: User Service and Billing Service have separate databases (they have to — that's the whole point). Updating both requires a "distributed transaction." Your options are:

1. **Two-Phase Commit** — theoretically correct, practically a distributed deadlock waiting to happen.
2. **The Saga Pattern** — a choreographed sequence of compensating transactions that simulates atomicity by undoing things if later steps fail.
3. **Eventual Consistency** — give up on correctness and hope the state resolves itself. Send a "please try again" email if it doesn't.

The Saga Pattern, in case you haven't encountered it, is a workflow engine that re-implements what `BEGIN TRANSACTION` does — except distributed, eventually consistent, and your problem to build, test, and maintain.

You traded `BEGIN TRANSACTION` for a compensating transaction coordinator. Was it worth it?

## The Organizational Argument (and Why It Doesn't Apply to You)

There's a real reason microservices exist, and it's not a technical one. It's **Conway's Law**.

Organizations ship software that mirrors their communication structure. If you have 500 engineers across 40 teams, you will naturally produce 40 loosely-coupled services — because otherwise teams would constantly collide in a shared codebase. Microservices are a solution to a *people problem*, not a performance problem.

Netflix has microservices because they have thousands of engineers who need to deploy independently without coordinating with each other. Amazon has them for the same reason. These architectures exist because the alternative — thousands of people working in a monorepo, coordinating releases — is genuinely untenable at that scale.

You have eight engineers.

You don't have a people problem. You have a product to build. And microservices are the most expensive way to build a product with eight engineers, because you spend most of your time managing infrastructure rather than building the thing.

## The Operational Reality

Running microservices means running all of this:

- **Kubernetes** — because you need to orchestrate 20 containers, and apparently Docker Compose isn't serious enough
- **A service mesh** (Istio/Linkerd) — because Kubernetes alone is not complex enough; you also need mTLS, traffic policies, and a control plane
- **Distributed tracing** (Jaeger/Zipkin) — because a single user request now touches 7 services and you have no idea which one is slow
- **Centralized logging** — because your logs are in 40 different containers that get killed and respawned on a schedule
- **A secrets manager** — because 20 services each have their own credentials
- **Circuit breakers** — because when one service slows down, you need the others to not also slow down

That's not a product. That's an infrastructure company that ships a product as a side project. Your real competition is a team of four people with a Rails monolith who spent the year adding features while you were writing Helm charts.

## WhatsApp: 450 Million Users, 32 Engineers

WhatsApp reached 450 million users with a team of 32 engineers.

The architecture was not microservices. It was a single Erlang application — effectively a well-optimized monolith — running on a fleet of servers. The performance came from writing efficient code, not from distributing the inefficiency across more processes.

When Facebook acquired WhatsApp in 2014, the ratio was roughly 14 million active users per engineer. Facebook, with its microservice architecture and thousands of engineers, was at about 1 million users per engineer.

Efficient code in a monolith beat distributed complexity by a factor of fourteen.

## The Monolith Is Only Slow If Your Code Is Slow

The real driver of microservice adoption at companies that don't need them is that splitting services is easier than fixing slow code.

Your user profile endpoint takes 400ms. Nobody wants to dig in and find out why. Someone proposes extracting it into its own service — "then we can scale it independently and deploy faster." After the extraction, the endpoint takes 450ms (the original 400ms plus network overhead), but it has its own deployment pipeline and Slack channel, so it *feels* like progress.

The 400ms is still there. You just gave it a new address.

A well-written monolith is faster in every measurable way than a well-written microservice doing the same work: no serialization overhead, no network round-trips, no service discovery latency, shared in-process memory instead of JSON payloads over a network. The monolith *is* the performance optimization, if the code inside it is good.

## The Test Before You Split

Before you decompose a service, answer these questions:

- **What specific problem are you solving?** Traffic one machine can't handle? Teams colliding? Independent deploy cadences? Write it down.
- **Have you measured the alternative?** A single large server with 64 cores costs less than a 20-node cluster. Have you tried vertical scaling first?
- **Can module boundaries solve the team problem?** Clear ownership, good interfaces, and a shared repo solve most "people problems" without network calls.

If the answer is "we might need to scale it independently someday" — you don't have a problem yet. Don't build a solution for a hypothetical.

Build the monolith. Make the code fast. Split when you have a specific, measured, documented reason to split — not because the microservice talk at the conference made it sound like the professional thing to do.

---

*Part 5 of the "Rebuilding the Web" series.*
**Next:** [Complexity Is a Choice: Why Your Stack Is 100x Heavier Than It Needs To Be](/post/complexity-is-a-choice)
