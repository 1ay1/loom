---
title: The ORM Delusion: Why Your Database Abstraction Is Making You Slow
date: 2026-03-17
slug: database-lies
tags: architecture, databases, performance, series
excerpt: Your ORM is quietly sending 300 database queries to render one page. It just never mentioned that.
---

Your web app has a dashboard.

The dashboard loads in 800ms. Not great, not catastrophic. You add some caching, bring it to 600ms, ship it, move on. A year passes. You are now investigating a production incident — load is higher than usual, the database is melting, CPU pegged at 100%, and someone in the incident channel asks you to log the queries hitting the server.

The dashboard is firing 247 separate database queries.

To render one page.

Two hundred and forty-seven. Not a typo. You count them twice. You count them a third time. Two hundred and forty-seven.

The ORM did this. The ORM thought it was helping. The ORM smiled warmly while loading your entire database into memory one row at a time, like a golden retriever fetching every stick in the park when you only threw one.

---

## The N+1 Problem Is Not a Bug. It's the Feature.

ORMs sell "lazy loading" as a feature. Only fetch data when you need it. Efficient. Smart. The word "lazy" is right there in the name, and somehow nobody took that as a warning.

Here's what it looks like in practice:

```javascript
const users = await User.findAll(); // 1 query: SELECT * FROM users

for (const user of users) {
    console.log(user.posts.length); // 1 query per user: SELECT * FROM posts WHERE user_id = ?
}
```

Looks reasonable. You're fetching users, then looking at their posts. Two concepts. Simple logic. A junior developer could read this and understand it immediately.

The database sees something different. The database sees: one query, then 100 more. One for each user. Fired in sequence. Each one establishing a new execution context, running through the query planner, scanning an index, returning a result set, and serializing it back over the wire. A hundred times. For what is, conceptually, one question: "how many posts does each user have?"

Your code has a single loop. Your database has a firing squad. The ORM helpfully abstracted away the part where it was going to machine-gun your database with queries — you had no idea until you looked at the query log and saw a number that made you say a word your manager was not expecting to hear in the incident call.

This is the N+1 query problem. It is the most common reason real web applications are slow. It doesn't happen because engineers are bad. It happens because the ORM makes it structurally invisible. A hundred queries look exactly like one line of code. The abstraction didn't simplify anything. It just hid the receipt.

## `SELECT *`: Paying for Data You Never Touch

When you fetch a model through an ORM, you get the entire model. All columns. Whether you need them or not. Like ordering a pizza and receiving the entire cow.

```javascript
const user = await User.findById(id);
return { name: user.name }; // using 1 of 22 columns
```

What actually hit the database:

```sql
SELECT id, name, email, password_hash, created_at, updated_at,
       last_login, profile_picture_url, bio, phone_number,
       address_line_1, address_line_2, city, state, postal_code,
       country, timezone, notification_preferences, api_key,
       subscription_tier, payment_method_id, stripe_customer_id
FROM users WHERE id = ?
```

You wanted `name`. You got the whole row — including the 200-character S3 URL nobody's updated since 2024, the Stripe customer ID, the notification preferences blob, and the `bio` field that one user filled with the entire lyrics to Bohemian Rhapsody. The database read it all from disk, serialized it, sent it across the network, deserialized it into a full `User` object with 22 fields — and then you extracted one string and threw the rest away.

You ordered a glass of water and the ORM delivered it via fire truck.

Multiply this by every query, every endpoint, every request. That's the overhead you pay for the convenience of not writing `SELECT name FROM users WHERE id = ?`. The ORM made one thing easy: not thinking. And it turns out not thinking about your database has consequences.

## The ORM Makes It Too Easy to Be Bad

Here's the uncomfortable truth: ORMs don't abstract away database complexity. They abstract away *awareness* of database complexity, which is a different thing entirely. The complexity is still there. The queries still run. The database still suffers. You just don't know about it until the pager goes off.

A developer writing raw SQL thinks about the query before they write it. What am I fetching? Do I need a join? What columns do I actually need? Is there an index for this condition? These are not difficult questions. They're just questions, and the act of writing SQL forces you to answer them.

A developer using an ORM thinks about the object model. "I need a User. I'll call `findById`." The database query happens somewhere, somehow, generated by code you've never read, optimized by nobody. Ship it. Feels clean. Looks clean. The database is screaming, but you can't hear it because the abstraction is soundproofed.

This isn't a discipline problem — it's an interface design problem. The ORM's interface is optimized for not thinking about the database. The database has no such luxury. Every unthought-about query still runs, still costs real time, still holds real locks, still competes for real CPU.

At 100 users: unnoticeable. At 10,000 users: pages start timing out. At 100,000 users: the database goes down, you're in an incident call at 2am, and someone asks "why is `User.findAll()` taking 40 seconds?" and you realize the answer is "because it's doing exactly what we told it to, we just never thought about what we were telling it."

The ORM didn't protect you from the database. It just delayed the introduction.

## SQL Is Not Your Enemy

At some point the industry collectively decided that writing SQL was a sign of poor architecture. Raw queries are "dangerous." "Unportable." A code smell from a less enlightened era when developers had to understand their tools.

This is completely backwards. This is like saying "using a knife to cook is dangerous — use this automatic food processor" while the food processor randomly blends things you didn't put in it.

SQL is one of the most powerful languages in software. It was designed specifically to describe what data you want — and the query planner inside PostgreSQL or MySQL has been optimized for 40+ years to fetch it efficiently. Four decades of the smartest database engineers on Earth making your queries faster. When you write SQL, you're speaking directly to that optimizer. When you use an ORM, you're hoping its code generator produces something the optimizer can work with. You're playing telephone with your database, and the ORM is the middle person who paraphrases everything.

```sql
SELECT u.name, COUNT(p.id) AS post_count
FROM users u
LEFT JOIN posts p ON p.user_id = u.id
WHERE u.created_at > NOW() - INTERVAL '30 days'
GROUP BY u.id
ORDER BY post_count DESC
LIMIT 20
```

One query. Does exactly what it says. You can run `EXPLAIN ANALYZE` on it and see the execution plan — which indexes it uses, how many rows it scans, what it costs. Try running `EXPLAIN ANALYZE` on an ORM method chain. The ORM will look at you like you asked it to do a cartwheel.

## Fetch What You Need, Nothing Else

The alternative isn't raw SQL scattered everywhere with no structure. It's not going back to PHP with queries concatenated from user input. Relax.

It's treating data access as a deliberate act: decide what you need, then ask for exactly that.

```cpp
struct DashboardUser {
    std::string name;
    int post_count;
    std::string last_post_title;
};

// One query. Three columns. Zero hidden joins. Zero surprises.
auto users = db.query<DashboardUser>(R"(
    SELECT u.name, COUNT(p.id), MAX(p.title)
    FROM users u
    LEFT JOIN posts p ON p.user_id = u.id
    GROUP BY u.id
    LIMIT 20
)");
```

The struct defines exactly what's needed. The query fetches exactly that. The compiler verifies the types at build time. If the schema changes, this breaks at compile time — not in front of a user, not in production, not in an incident call with your manager and your manager's manager.

No lazy loading. No N+1. No `SELECT *`. No guessing what the framework generated. No mystery. Just a question and an answer, which is all a database query was ever supposed to be.

## Learn the Database

The database is not a detail to abstract away. It is the most important piece of infrastructure in your stack. It's where your data lives, and how you talk to it is one of the most consequential performance decisions your system makes.

Learning to write SQL — understanding joins, indexes, execution plans, and the difference between a sequential scan and an index scan — is not a step backward. It's acquiring fluency in the most powerful query language you will ever use. It will make you a better engineer. It will make your systems faster. It will make your 2am incidents shorter, rarer, and less existentially distressing.

Kill the ORM. Write the queries. Run `EXPLAIN ANALYZE`. Know what your system is actually doing.

The database has been trying to tell you. The ORM was just covering its ears.

---

*Part 4 of the "Rebuilding the Web" series.*
**Next:** [The Distributed Systems Lie: You Don't Need Microservices](/post/microservice-lies)
