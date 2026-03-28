---
title: The Distributed Systems Lie: You Don't Need Microservices
date: 2026-03-19
slug: microservice-lies
tags: architecture, microservices, systems, devops
excerpt: Stack Overflow serves 1.3 billion page views a month with 9 web servers. You have 500 users and three Kubernetes clusters.
---

Let me tell you about a system that handles 1.3 billion page views every month.

It runs on **9 web servers**. Nine. Physical machines. Two data centers. The database is one primary, two replicas. The deployment is a PowerShell script. The architecture, in the technical sense, is deeply, aggressively, almost confrontationally boring.

It's Stack Overflow. You've probably used it today. You've definitely used it this week. It answered the question that unblocked your Kubernetes migration.

Now tell me about your startup's infrastructure. Tell me about the Kubernetes cluster. The service mesh. The API gateway. The three message queues (one for each time someone said "we should decouple this"). The distributed tracing system you installed to figure out why things are slow — which itself made things slower. The two engineers whose job titles are "platform" because the infrastructure became its own product.

How many users do you have?

I'll wait.

---

## A Function Call vs. a Network Call

When your Order Service calls your Inventory Service over the network, here is what happens at the hardware level:

1. Serialize the request object to JSON. (Or Protobuf, if you went to a conference about it.)
2. Form a TCP packet, push it onto the wire.
3. Inventory Service receives it, deserializes JSON back into an object.
4. Inventory Service processes it, serializes the response to JSON.
5. Response travels back over the wire.
6. Order Service receives it, deserializes it back into an object.

That's six steps, involving DNS lookup, TCP handshake, TLS negotiation, a load balancer in the middle, serialization libraries on both ends, and network latency that depends on the current mood of your cloud provider's infrastructure. Best case: 10–50ms. Worst case: timeout, retry, exponential backoff, circuit breaker, fallback logic, and a log message that says "Inventory Service unavailable" while the user stares at a spinning wheel.

Now here's the monolith version of the same operation: move an instruction pointer. Call a function. Nanoseconds.

Not milliseconds. *Nanoseconds*. The difference is six orders of magnitude. You wouldn't accept a 1,000,000x regression in any other context. But wrap it in a Kubernetes pod and a Helm chart and suddenly it's "architecture."

## The Distributed Transactions Problem Is Not Solved

In a monolith with a shared database, you update a user's profile and their billing status in one transaction. It's atomic. Either both succeed or neither does. `BEGIN TRANSACTION`. `COMMIT`. Done. This has been solved since 1983. It works so well that most engineers have never thought about it, which is the highest compliment you can pay a technology.

In microservices, User Service and Billing Service have separate databases — they have to, that's the whole point. Updating both requires a "distributed transaction." Your options:

1. **Two-Phase Commit** — theoretically correct, practically a distributed deadlock that's just warming up.
2. **The Saga Pattern** — a choreographed sequence of compensating transactions that simulates atomicity by *undoing things if later steps fail*. It's like a transaction, except when it fails, instead of rolling back cleanly, it runs *more code* to try to reverse what already happened. What could go wrong?
3. **Eventual Consistency** — give up on correctness entirely and hope the data sorts itself out. Send the user a "please try again" email when it doesn't.

The Saga Pattern, in case you haven't had the pleasure, is essentially building a workflow engine that re-implements `BEGIN TRANSACTION` — except distributed, eventually consistent, much harder to test, and entirely your problem to build, debug, and maintain forever.

You traded one line of SQL for a state machine. Was it worth it? Was it ever worth it?

## The Organizational Argument (and Why It Doesn't Apply to You)

There is a real reason microservices exist, and it's not a technical one. It's **Conway's Law**.

Organizations ship software that mirrors their communication structure. If you have 500 engineers across 40 teams, you will naturally produce 40 loosely-coupled services — because the alternative is 500 people committing to a shared codebase, stepping on each other's toes, and resolving merge conflicts until the heat death of the universe. Microservices solve a *people problem*, not a performance problem.

Netflix has microservices because they have thousands of engineers who need to deploy independently without sending a Slack message to thirty other teams. Amazon has them for the same reason. Google has them because Google has more engineers than some countries have people. These architectures exist because the alternative — thousands of developers in a monorepo, coordinating releases — is genuinely untenable at that scale.

You have eight engineers.

Eight. You all sit in the same room. You can literally tap someone on the shoulder and say "hey, I'm changing the user model." You do not have a Conway's Law problem. You have a product to build. And microservices are the most expensive possible way to build a product with eight people, because you'll spend most of your time managing infrastructure instead of building the thing your users actually want.

Your competition is a team of four with a Rails monolith who shipped six features this quarter while you were writing Helm charts.

## The Operational Reality

Running microservices in production means running all of this, at minimum:

- **Kubernetes** — because you need to orchestrate 20 containers. Docker Compose was apparently not enterprise enough. You needed a system that requires its own certification program.
- **A service mesh** (Istio/Linkerd) — because Kubernetes by itself was not sufficiently complex. You also needed mTLS between services, traffic policies, a control plane, and a YAML file the length of a novella.
- **Distributed tracing** (Jaeger/Zipkin) — because when a user clicks a button and it takes 4 seconds, the latency is spread across 7 services and you have no idea which one is the bottleneck. You need a PhD in flamegraph interpretation to debug a slow API call.
- **Centralized logging** — because your logs are scattered across 40 containers that Kubernetes kills and respawns on a whim. `kubectl logs` shows you the last 30 seconds of a container that may or may not still exist.
- **A secrets manager** — because 20 services each have their own database credentials, API keys, and certificates. Rotating one secret is now a cross-team project.
- **Circuit breakers** — because when one service slows down, the services that call it also slow down, and the services that call *those* also slow down, in a cascading failure that spreads through your cluster like gossip through a high school.

That's not a product. That's an infrastructure company that ships a product as a side project. You have become your own cloud provider, except worse at it than AWS.

## WhatsApp: 450 Million Users, 32 Engineers

WhatsApp reached 450 million active users with a team of 32 engineers.

Not 32 platform engineers. Not 32 backend engineers. Thirty-two engineers *total*. The entire company.

The architecture was not microservices. It was a single Erlang application — effectively a well-optimized monolith — running on a fleet of servers. The performance came from writing efficient code, not from distributing the inefficiency across more processes.

When Facebook acquired WhatsApp in 2014, the ratio was roughly 14 million active users per engineer. Facebook, with its microservice architecture and thousands of engineers, was at about 1 million users per engineer.

Efficient code in a monolith beat distributed complexity by a factor of fourteen. Not 14%. Fourteen *times*. One architecture gave each engineer responsibility for a mid-sized country's population of users. The other gave each engineer responsibility for a city.

The Erlang monolith was not a compromise. It was the reason WhatsApp won.

## The Monolith Is Only Slow If Your Code Is Slow

The real dirty secret of microservice adoption at companies that don't need it: splitting services is easier than fixing slow code.

Your user profile endpoint takes 400ms. Nobody wants to dig in and find out why. Profiling is hard. Reading someone else's database queries is tedious. But someone has an idea: extract it into its own service. "Then we can scale it independently and deploy faster." The room nods. It sounds like progress.

After the extraction, the endpoint takes 450ms — the original 400ms plus 50ms of network overhead for the new service call. But it has its own deployment pipeline, its own repository, its own Slack channel, and its own PagerDuty rotation. It *feels* like progress. It has all the artifacts of progress. There was a Jira epic.

The 400ms is still there. You just gave it a new address. The bug moved to a nicer apartment.

A well-written monolith is faster in every measurable way than a well-written microservice doing the same work. No serialization overhead. No network round-trips. No service discovery latency. Shared in-process memory instead of JSON payloads flying across a network. The monolith *is* the performance optimization, if the code inside it is good.

## The Test Before You Split

Before you decompose a service, answer these questions honestly:

- **What specific problem are you solving?** Not "what might we need someday." What problem do you have *right now* that a monolith cannot solve? Write it down. In ink.
- **Have you measured the alternative?** A single large server with 64 cores costs $500/month. A 20-node Kubernetes cluster costs $5,000/month plus two engineers' salaries to maintain it. Have you tried vertical scaling? Have you *tried*?
- **Can module boundaries solve the team problem?** Clear ownership, good interfaces, and a monorepo solve most "people problems" without a single network call. Code review exists. CODEOWNERS files exist. You don't need microservices to have boundaries.

If the answer is "we might need to scale it independently someday" — you don't have a problem. You have anxiety.

Build the monolith. Make the code fast. Profile before you split. And when someone suggests microservices at the architecture meeting, ask them: "How many users do we have?" Then ask them how many Stack Overflow has. Then sit in the silence.

---

*Part 5 of the "Rebuilding the Web" series.*
**Next:** [Complexity Is a Choice: Why Your Stack Is 100x Heavier Than It Needs To Be](/post/complexity-is-a-choice)
