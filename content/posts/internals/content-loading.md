---
title: "Content Loading — From Filesystem and Git to Typed Site Data"
date: 2026-01-02
slug: content-loading
tags: [loom-internals, c++20, content, parsing, git]
excerpt: "How Loom loads blog content from the local filesystem or a remote git repository using the same interface, parses frontmatter and config without dependencies, extracts image dimensions from raw PNG and JPEG bytes, and derives metadata from git history."
---

A blog engine needs content. Markdown files, config, themes, images — all sitting somewhere on disk or in a repository. Most engines have one way to load this: read files from a directory. Loom has two. It can load content from the local filesystem, or it can pull content from a git repository — local or remote. Both produce the same typed data structures, and the rest of the engine does not know which one was used.

This post covers four files: `config_parser.cpp` (109 lines), `filesystem_source.cpp` (567 lines), `git_source.cpp` (453 lines), and `git.cpp` (244 lines). It connects to the [strong types](/post/strong-types) post for the domain types that these loaders produce, and to the [cache and rendering](/post/cache-and-rendering) post for what happens to the data after loading.

## The Config Parser: Two Formats, 109 Lines

Loom has two text formats to parse: `key=value` config files and YAML-style frontmatter in markdown posts. Both are hand-written, and together they are 109 lines.

The config parser is the simpler one:

```cpp
std::map<std::string, std::string> parse_config(const std::string& text)
{
    std::map<std::string, std::string> result;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);

        // Trim whitespace
        auto trim = [](std::string& s) {
            auto start = s.find_first_not_of(" \t\r\n");
            auto end = s.find_last_not_of(" \t\r\n");
            s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };

        trim(key);
        trim(val);

        if (!key.empty())
            result[key] = val;
    }
    return result;
}
```

Lines starting with `#` are comments. Everything else is `key = value`. The trim lambda appears twice more in this file and throughout the content loaders — a small, local utility that does not need to be shared.

The frontmatter parser handles the `---` delimited metadata block at the top of markdown files:

```cpp
ParsedDocument parse_frontmatter(const std::string& text)
{
    ParsedDocument doc;

    if (text.size() < 3 || text.substr(0, 3) != "---")
    {
        doc.body = text;
        return doc;
    }

    auto end = text.find("\n---", 3);
    if (end == std::string::npos)
    {
        doc.body = text;
        return doc;
    }

    // Parse frontmatter as key: value pairs
    // ...

    // Strip surrounding quotes (YAML-style)
    if (val.size() >= 2 &&
        ((val.front() == '"' && val.back() == '"') ||
         (val.front() == '\'' && val.back() == '\'')))
        val = val.substr(1, val.size() - 2);

    // Body is everything after closing ---
    doc.body = text.substr(body_start);
    return doc;
}
```

The format is:

```
---
title: "My Post"
date: 2025-12-26
tags: c++, performance
---

# Markdown body here
```

It handles YAML-style quoting (single or double quotes are stripped), comments, and graceful fallback — if there is no frontmatter delimiter, the entire text becomes the body. This is not a full YAML parser and makes no attempt to be one. It handles the subset that blog frontmatter actually uses.

## The Filesystem Source

`FileSystemSource` is the primary content loader. It reads from a directory with this layout:

```
content/
  site.conf
  theme/style.css
  posts/
    introducing-loom.md
    seo.md
    internals/           ← series: folder name becomes series name
      strong-types.md
      router.md
  pages/
    about.md
```

Loading is four steps, called in sequence:

```cpp
void FileSystemSource::load()
{
    load_config();
    load_theme();
    load_posts();
    load_pages();
}
```

### Config Loading

The config file (`site.conf`) is a flat `key=value` file. The interesting part is how structured data is encoded in flat values:

```cpp
// Parse nav: Home:/,Blog:/blog,About:/about
if (cfg.count("nav"))
{
    std::istringstream ss(cfg["nav"]);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        auto colon = item.find(':');
        if (colon == std::string::npos) continue;

        auto title = item.substr(0, colon);
        auto url = item.substr(colon + 1);
        trim(title);
        trim(url);

        config_.navigation.items.push_back({title, url});
    }
}
```

Navigation items, sidebar widgets, footer links — all encoded as comma-separated pairs in a single config line. This is deliberately flat. A blog engine does not need nested config. The alternatives — TOML, YAML, JSON — each bring a parser dependency that is larger than Loom's entire config system.

Theme variables get a special translation. Config keys prefixed with `theme_` become CSS custom properties with underscores converted to dashes:

```cpp
for (const auto& [key, value] : cfg)
{
    if (key.substr(0, 6) == "theme_")
    {
        std::string var_name = key.substr(6);
        for (auto& c : var_name)
            if (c == '_') c = '-';
        theme_.variables[var_name] = value;
    }
}
```

So `theme_accent_color = #ff6600` becomes the CSS variable `--accent-color: #ff6600`. This connects to the [theme system](/post/theme-system) where these variables are consumed by the CSS compiler.

### Post Loading and Series Detection

Posts are loaded from the filesystem in two passes — top-level files (no series) and subdirectories (each subdirectory is a series):

```cpp
void FileSystemSource::load_posts()
{
    int counter = 1;

    // Top-level posts (no series)
    auto files = list_files(content_dir_ + "/posts", ".md");
    for (const auto& path : files)
        posts_.push_back(load_post(path, "", counter, content_dir_));

    // Subdirectories = series (folder name is the series name)
    auto dirs = list_subdirs(content_dir_ + "/posts");
    for (const auto& dir : dirs)
    {
        auto series_files = list_files(content_dir_ + "/posts/" + dir, ".md");
        for (const auto& path : series_files)
            posts_.push_back(load_post(path, dir, counter, content_dir_));
    }
}
```

The series name is the directory name. A post in `posts/internals/router.md` belongs to the `internals` series. There is no config for series — the filesystem is the configuration.

### Reading Time

The reading time calculation strips HTML tags and counts words:

```cpp
int words = 0;
bool in_word = false;
bool in_html_tag = false;
for (char c : html_content)
{
    if (c == '<') { in_html_tag = true; continue; }
    if (c == '>') { in_html_tag = false; continue; }
    if (in_html_tag) continue;

    bool is_space = (c == ' ' || c == '\n' || c == '\t' || c == '\r');
    if (!is_space && !in_word) { ++words; in_word = true; }
    else if (is_space) { in_word = false; }
}
reading_time = std::max(1, (words + 100) / 200);
```

The formula — `(words + 100) / 200` — assumes 200 words per minute with rounding. The `+ 100` biases short posts upward so they show at least "1 min" instead of zero. The HTML tag stripping is a simple state machine because the input is trusted (it came from the markdown parser).

### Excerpt Generation

If a post has no `excerpt` field in its frontmatter, the loader generates one by stripping HTML and truncating at a word boundary:

```cpp
bool in_html_tag = false;
for (char c : html_content)
{
    if (c == '<') { in_html_tag = true; continue; }
    if (c == '>') { in_html_tag = false; continue; }
    if (in_html_tag) continue;
    if (c == '\n') c = ' ';
    excerpt += c;
    if (excerpt.size() >= 200) break;
}
if (excerpt.size() >= 200)
{
    auto last_space = excerpt.rfind(' ');
    if (last_space != std::string::npos && last_space > 100)
        excerpt = excerpt.substr(0, last_space) + "...";
}
```

It collects up to 200 characters of visible text, then backtracks to the last space boundary to avoid cutting mid-word. The `> 100` guard prevents backtracking all the way to the beginning if the first 200 characters happen to be one long word (unlikely, but handled).

## Image Dimension Detection

When the markdown parser produces an `<img>` tag, the loader injects `width` and `height` attributes by reading the actual image file. This prevents layout shift in browsers — the browser can reserve space before the image loads.

The detection reads the first 28 bytes of the file and checks the magic bytes:

```cpp
static ImageDims read_image_dims(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};

    unsigned char buf[28] = {};
    f.read(reinterpret_cast<char*>(buf), 28);

    // PNG: 8-byte signature, then IHDR chunk
    if (buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G')
    {
        int w = (buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19];
        int h = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23];
        return {w, h};
    }

    // JPEG: scan for SOF markers
    if (buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF)
    {
        // ... scan marker stream for SOF0-SOF15 ...
    }

    return {};
}
```

**PNG** is straightforward. The format guarantees that the first chunk after the 8-byte signature is IHDR, and IHDR always contains width and height at fixed offsets (bytes 16-19 and 20-23, big-endian). Reading 24 bytes is enough.

**JPEG** is more involved. Dimensions are not at a fixed offset — they are inside a Start of Frame (SOF) marker, which can appear anywhere in the marker stream. The code reads up to 64KB and scans through markers until it finds a SOF:

```cpp
while (pos + 3 < sz)
{
    if (data[pos] != 0xFF) break;
    uint8_t marker = data[pos + 1];

    // SOF markers (C0-CF except C4 and C8)
    if ((marker >= 0xC0 && marker <= 0xC3) ||
        (marker >= 0xC5 && marker <= 0xC7) ||
        (marker >= 0xC9 && marker <= 0xCB) ||
        (marker >= 0xCD && marker <= 0xCF))
    {
        int h = (data[pos + 5] << 8) | data[pos + 6];
        int w = (data[pos + 7] << 8) | data[pos + 8];
        return {w, h};
    }

    // Skip markers without length fields (RST, SOI, EOI, SOS)
    // ...

    // Skip segment by reading its length
    uint16_t seg_len = (data[pos + 2] << 8) | data[pos + 3];
    pos += 2 + seg_len;
}
```

This is raw binary format parsing without a library. The JPEG marker protocol is simple — every marker starts with `0xFF`, followed by a marker byte, followed by a two-byte segment length. SOF markers contain height and width at fixed offsets within the segment. The tricky part is knowing which markers do not have a length field (RST, SOI, EOI) and skipping them correctly.

The injection pass walks the rendered HTML and only processes `<img>` tags that reference root-relative local paths and do not already have dimensions:

```cpp
static void inject_image_dims(std::string& html, const std::string& content_dir)
{
    size_t pos = 0;
    while ((pos = html.find("<img ", pos)) != std::string::npos)
    {
        // Skip if dimensions already present
        auto tag_view = html.substr(pos, tag_end - pos);
        if (tag_view.find("width=") != std::string::npos) { pos = tag_end + 1; continue; }

        // Only resolve root-relative local paths
        if (url.empty() || url[0] != '/' || (url.size() > 1 && url[1] == '/'))
            { pos = tag_end + 1; continue; }

        auto dims = read_image_dims(content_dir + url);
        if (dims.width > 0 && dims.height > 0)
        {
            std::string attrs = " width=\"" + std::to_string(dims.width)
                              + "\" height=\"" + std::to_string(dims.height) + "\"";
            html.insert(tag_end, attrs);
        }
    }
}
```

External URLs and protocol-relative URLs (`//cdn.example.com/img.png`) are skipped. The check `url[1] == '/'` distinguishes `/images/foo.png` (local) from `//cdn.example.com/img.png` (external).

## The Git Source

`GitSource` implements the same interface as `FileSystemSource` but reads content from a git repository. The constructor verifies the repo and branch exist:

```cpp
GitSource::GitSource(GitSourceConfig config)
    : config_(std::move(config))
{
    git_rev_parse(config_.repo_path, config_.branch);
    load();
}
```

Instead of reading files from disk, it reads git blobs:

```cpp
std::string GitSource::read_blob(const std::string& path) const
{
    try
    {
        return git_read_blob(config_.repo_path, ref(), prefix(path));
    }
    catch (const GitError&)
    {
        return "";
    }
}
```

Under the hood, `git_read_blob` runs `git show <ref>:<path>`. Similarly, listing files in a directory uses `git ls-tree --name-only`.

### Content Prefix

The git source supports a `content_prefix` — the subdirectory within the repo where content lives:

```cpp
std::string GitSource::prefix(const std::string& path) const
{
    if (config_.content_prefix.empty())
        return path;
    return config_.content_prefix + "/" + path;
}
```

This means a repo with structure `my-blog/content/posts/...` can have its content prefix set to `content`, and the git source will look for `posts/` under that prefix. The rest of the code works with logical paths (`posts/foo.md`) and the prefix is applied at the boundary.

### Git History as Metadata

The most interesting difference from the filesystem source is how dates are derived. The filesystem source reads file modification time from `stat()`. The git source reads dates from git history:

```cpp
auto date = doc.meta.count("date")
    ? parse_date(doc.meta["date"])
    : git_first_commit_date(config_.repo_path, prefix(rel_path));

auto mtime = git_last_commit_date(config_.repo_path, prefix(rel_path));
```

If a post has an explicit `date` in its frontmatter, that is used. Otherwise, the creation date is the date of the first commit that added the file. The modification time is always the date of the last commit that touched the file.

These are implemented with git plumbing commands:

```cpp
std::chrono::system_clock::time_point git_first_commit_date(
    const std::string& repo_path, const std::string& path)
{
    auto output = git_exec(repo_path,
        "log --follow --diff-filter=A --format=%aI -- " + path);
    if (!output.empty())
        return parse_iso8601(output);
    return std::chrono::system_clock::now();
}

std::chrono::system_clock::time_point git_last_commit_date(
    const std::string& repo_path, const std::string& path)
{
    auto output = git_exec(repo_path, "log -1 --format=%aI -- " + path);
    if (!output.empty())
        return parse_iso8601(output);
    return std::chrono::system_clock::now();
}
```

The `--follow --diff-filter=A` combination follows the file through renames and finds the commit that *added* it — the original creation date. The `--format=%aI` outputs the author date in ISO 8601 format, which is then parsed with timezone handling:

```cpp
std::chrono::system_clock::time_point parse_iso8601(const std::string& date_str)
{
    // Parse: 2026-03-15T10:30:00+05:30
    std::tm tm{};
    tm.tm_year = std::stoi(date_str.substr(0, 4)) - 1900;
    // ... parse date and time ...

    auto time = timegm(&tm);

    // Parse timezone offset
    if (date_str.size() > 19)
    {
        char tz_sign = date_str[19];
        if (tz_sign == '+' || tz_sign == '-')
        {
            int tz_hours = std::stoi(date_str.substr(20, 2));
            int tz_mins = (date_str.size() >= 25)
                ? std::stoi(date_str.substr(23, 2)) : 0;

            int offset_secs = tz_hours * 3600 + tz_mins * 60;
            time = (tz_sign == '+') ? time - offset_secs : time + offset_secs;
        }
    }

    return std::chrono::system_clock::from_time_t(time);
}
```

The timezone handling converts the local time to UTC by subtracting a positive offset or adding a negative one. `timegm` interprets the `tm` struct as UTC, so after applying the offset the result is a correct UTC timestamp regardless of the author's timezone.

## The Git Layer

All git operations go through a thin wrapper in `git.cpp`. The core function runs a git command with a timeout:

```cpp
std::string git_exec(const std::string& repo_path, const std::string& args)
{
    std::string cmd = "timeout 30 git -C " + repo_path + " " + args + " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
        throw GitError("Failed to run: " + cmd);

    std::string result;
    std::array<char, 4096> buf;
    while (auto n = fread(buf.data(), 1, buf.size(), pipe))
        result.append(buf.data(), n);

    int status = pclose(pipe);
    if (status != 0)
        throw GitError("git command failed: " + args);

    return result;
}
```

Every git command gets a 30-second timeout (120 seconds for clones). This prevents hung git processes from blocking the server. The `2>/dev/null` suppresses stderr — errors are detected by exit code, not output parsing.

### Smart Clone Caching

When the git source is configured with a remote URL, Loom clones it as a bare repository:

```cpp
std::string git_clone_bare(const std::string& url, const std::string& dest)
{
    // Derive directory name: strip .git, take last path component
    auto name = url;
    if (name.size() > 4 && name.substr(name.size() - 4) == ".git")
        name = name.substr(0, name.size() - 4);
    auto slash = name.rfind('/');
    if (slash != std::string::npos)
        name = name.substr(slash + 1);

    auto local_path = dest + "/" + name + ".git";

    if (std::filesystem::exists(local_path))
    {
        // Already cloned — just fetch
        git_fetch(local_path);
        return local_path;
    }

    // ... clone and configure ...

    // Bare clones don't get a fetch refspec by default
    git_exec(local_path, "config remote.origin.fetch +refs/heads/*:refs/heads/*");

    return local_path;
}
```

Two details here. First, if the repo already exists locally, it just fetches instead of cloning — so restarting the server does not mean re-cloning. Second, bare clones do not get a default fetch refspec, which means `git fetch` would not update any branches. The code explicitly configures the refspec after cloning.

The fetch itself is non-fatal:

```cpp
void git_fetch(const std::string& repo_path)
{
    try
    {
        git_exec(repo_path, "fetch --quiet origin +refs/heads/*:refs/heads/*");
    }
    catch (const GitError&)
    {
        // Fetch failure is non-fatal — we still have the old data
    }
}
```

If the network is down, the server keeps running with stale data. This is the right trade-off for a blog — serving yesterday's content is better than serving nothing.

## The Swappable Interface

Both sources implement the same public API:

```cpp
std::vector<Post> all_posts();
std::vector<Page> all_pages();
SiteConfig site_config() const;
Theme theme() const;
void reload(const ChangeSet& changes);
```

The rest of the engine — rendering, caching, serving — works with these functions and has no knowledge of where the data came from. The `reload` method takes a `ChangeSet` that indicates which parts changed (config, theme, posts, pages), so the loader can selectively reload only what is needed.

This is not an abstract base class with virtual dispatch. It is a structural interface — both types have the same methods, and the caller is templated or uses a variant. The types produce the same `Post`, `Page`, `SiteConfig`, and `Theme` structs that the [strong types](/post/strong-types) post described.

## The Full Pipeline

From raw files to typed data:

1. **Parse config** — `site.conf` becomes `SiteConfig` with navigation, sidebar, layout, footer, and theme variables
2. **Load theme** — custom CSS is loaded if present
3. **Load posts** — for each `.md` file: parse frontmatter, convert markdown to HTML, inject image dimensions, compute reading time, generate excerpt, detect series from directory structure
4. **Load pages** — same as posts but simpler (no tags, no series, no reading time)

From the git source, the pipeline is identical except that file reads go through `git show`, directory listings go through `git ls-tree`, and dates come from git history instead of filesystem timestamps.

The output is a collection of strongly-typed domain objects — `Post` with `Slug`, `Title`, `Content`, `Tag`, `Series` — ready for the [component system](/post/dom-and-components) to render and the [cache](/post/cache-and-rendering) to pre-serialize. The content loading stage is where raw text becomes typed data. Everything after this point operates on types, not strings.
