---
title: "The ORM Delusion: Why Your Database Abstraction Is Making You Slow"
date: 2026-03-17
slug: database-lies
tags: architecture, databases, performance, series
excerpt: Your ORM is quietly sending 300 database queries to render one page. It just never mentioned that.
---

Your web app has a dashboard.

The dashboard loads in 800ms. Not great, not embarrassing. You add some caching, bring it to 600ms, ship it, and move on. A year later, you're investigating a production incident — load is higher than usual, the database is at 100% CPU, and someone asks you to log the queries hitting the server.

The dashboard is firing 247 separate database queries.

To render one page.

The ORM did this. The ORM thought it was helping.

---

## The N+1 Problem Is Not a Bug. It's the Feature.

ORMs sell "lazy loading" as a feature. Only fetch data when you need it. Efficient. Smart.

Here's what it looks like in practice:

```javascript
const users = await User.findAll(); // 1 query: SELECT * FROM users

for (const user of users) {
    console.log(user.posts.length); // 1 query per user: SELECT * FROM posts WHERE user_id = ?
}
```

Looks reasonable. You're fetching users, then looking at their posts. Simple logic.

The database sees: one query, then 100 more. One for each user in the list.

Your code has a single loop. Your database has an avalanche. The ORM helpfully abstracted away the part where it was going to do that — you had no idea until you looked at the query log and saw "queries: 101" for what seemed like a trivial operation.

This is the N+1 query problem, and it is the most common reason real web applications are slow. It doesn't happen because engineers are incompetent. It happens because the ORM makes it structurally invisible. The 100 queries look exactly like one line of code.

## `SELECT *`: Paying for Data You Never Touch

When you fetch a model through an ORM, you get the entire model. All columns, whether you need them or not.

```javascript
const user = await User.findById(id);
return { name: user.name }; // using 2 of 23 columns
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

You wanted `name`. You got the whole row — including the 200-character S3 URL, the Stripe customer ID, and the timezone that was set once during signup and never changed. The database read from disk and sent across the network every column that user has ever accumulated, so the ORM could construct a full `User` object you immediately dismantled to extract one string.

Multiply this by every query, every endpoint, every request hitting your server. That's the overhead you pay for the convenience of not writing `SELECT name FROM users`.

## The ORM Makes It Too Easy to Be Bad

Here's the uncomfortable truth: ORMs don't abstract away database complexity. They let you *ignore* database complexity until it becomes catastrophic.

A developer writing raw SQL thinks about the query before they write it. What am I fetching? Do I need a join? What columns do I actually need? Is there an index for this condition?

A developer using an ORM thinks about the object model. The database query happens somewhere, somehow. It probably works. Ship it.

This isn't a discipline problem — it's an interface design problem. The ORM's interface optimizes for not thinking about the database. The database has no such luxury. Every unthought-about query runs, costs real time, and holds real locks.

At 100 users: unnoticeable. At 10,000 users: pages start timing out. At 100,000 users: database goes down and you're in an incident call at 2am trying to understand why `User.findAll()` is taking 40 seconds.

The ORM didn't protect you from the database. It just delayed the meeting.

## SQL Is Not Your Enemy

At some point the industry decided that writing SQL was a sign of poor architecture. Raw queries are "dangerous." "Unportable." A code smell from a less enlightened era.

This is completely backwards.

SQL is one of the most powerful languages in software. It was designed specifically to describe what data you want — and the query planner inside PostgreSQL or MySQL has been optimized for 40 years to fetch it efficiently. When you write SQL, you're speaking directly to that optimizer. When you use an ORM, you're hoping its code generation produces something the optimizer can work with.

```sql
SELECT u.name, COUNT(p.id) AS post_count
FROM users u
LEFT JOIN posts p ON p.user_id = u.id
WHERE u.created_at > NOW() - INTERVAL '30 days'
GROUP BY u.id
ORDER BY post_count DESC
LIMIT 20
```

This is one query. It does exactly what it says. You can run `EXPLAIN ANALYZE` on the exact string and see the execution plan. You know what indexes it uses. You know what it costs. No ORM writes this for you — and when they try, you get something worse.

## Fetch What You Need, Nothing Else

The alternative isn't raw SQL scattered everywhere with no structure. It's treating data access as a deliberate act: decide what you need, then ask for exactly that.

```cpp
struct DashboardUser {
    std::string name;
    int post_count;
    std::string last_post_title;
};

// One query. Three columns. Zero hidden joins.
auto users = db.query<DashboardUser>(R"(
    SELECT u.name, COUNT(p.id), MAX(p.title)
    FROM users u
    LEFT JOIN posts p ON p.user_id = u.id
    GROUP BY u.id
    LIMIT 20
)");
```

The struct defines exactly what's needed. The query fetches exactly that. The compiler verifies the mapping at build time. If the schema changes, this breaks at compile time — not in front of a user, not in production, not at 2am.

No lazy loading. No N+1. No mystery SELECT *. No guessing what the framework generated.

## Learn the Database

The database is not a detail to abstract away. It's where your data lives, and how you access it is one of the most consequential performance decisions your system makes.

Learning to write SQL well — understanding joins, indexes, and execution plans — is not a step backward. It's acquiring fluency in the most powerful query language in your stack.

Kill the ORM. Write the queries. Run `EXPLAIN ANALYZE`. Know what your system is actually doing.

---

*Part 4 of the "Rebuilding the Web" series.*
**Next:** [The Distributed Systems Lie: You Don't Need Microservices](/post/microservice-lies)
