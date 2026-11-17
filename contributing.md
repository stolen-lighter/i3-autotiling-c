## Table of Contents
* [Definitions](#definitions)
    * [1. Error messages](#1-error-messages)
    * [2. i3 ipc return values](#2-i3-ipc-return-values)
    * [3. Payloads & JSON Keys](#3-payloads--json-keys)
* [Functions](#functions)
    * [1. Function readn](#1-function-readn)
        * [Source code](#source-code)
        * [Explanation](#explanation)
    * [2. Function simple_atoi()](#2-function-simple_atoi)
        * [Source code](#source-code-1)
        * [Explanation](#explanation-1)
    * [3. Function get_i3_socket_path()](#3-function-get_i3_socket_path)
        * [Source](#source)
        * [Explanation](#explanation-2)
    * [4. Function flush_reply()](#4-function-flush_reply)
        * [Source code](#source-code-2)
        * [Explanation](#explanation-3)
    * [5. Function connect_to_i3()](#5-function-connect_to_i3)
        * [Source code](#source-code-3)
        * [Explanation](#explanation-4)
    * [6. Function window_events_subscribe()](#6-function-window_events_subscribe)
        * [Source code](#source-code-4)
        * [Explanation](#explanation-5)
    * [7. Function read_single_window_event() and Helpers](#7-function-read_single_window_event-and-helpers)
    * [8. Function send_i3_split_command()](#8-function-send_i3_split_command)
        * [Source code](#source-code-5)
        * [Explanation](#explanation-6)
    * [9. Function handle_signal()](#9-function-handle_signal)
        * [Source code](#source-code-6)
        * [Explanation](#explanation-7)
    * [10. Function main()](#10-function-main)
        * [Source code](#source-code-7)
        * [Explanation](#explanation-8)
---
# Contributing

To contribute to this project, please follow these standard steps:
1. **Fork** the repository and clone it locally.
2. **Make your changes**, ensuring you adhere to the [Linux Kernel Coding Style](https://docs.kernel.org/process/coding-style.html).
3. **Compile and test** your code to ensure it builds cleanly without warnings.
4. **Submit a Pull Request** with a clear explanation of what you changed and why.

Before writing any code, it is highly recommended that you read the logic of the program
below to understand the architecture.

*The spirit of this project is:*
1. We try to minimize the includes
2. We try to work only statically, minimizing memory allocations

# Definitions
The code starts with important definitions that help the program
adapt to future changes.
### 1. Error messages
They follow this format
```c
#define ERR_<error description> "Message for console"
#define LEN_<error description> sizeof(ERR_<error description>) - 1
```
*Note: `<error description>` is a placeholder for the example*
Usage:
```c
write(STDOUT_FILENO, ERR_<error description>, LEN_<error description>);
```
This way is preferred cause `sizeof()` is calculated during compilation.

### 2. i3 ipc return values
Self-explanatory. Named the return values for readability later on.
The names as of now are:
```c
#define EVENT_IGNORE  0
#define EVENT_FOCUS   1
#define EVENT_ERROR   -1
```
Usage snippet inside of `read_single_window_event()`:
```c
int read_single_window_event(int i3_fd_event, int *out_width, int *out_height)
{
	int result = EVENT_ERROR;
	int tokens_read;
	int valid_change = 0;
	int found_width = -1;
	int found_height = -1;
	jsmn_parser p;
	jsmntok_t token_list[512];
	struct i3_ipc_header event_header = {0};
	char json_payload[4096] = {0};
        // Rest of the code
}
```
*In this instance the default return value of the function is set to `EVENT_ERROR`, and updated to `EVENT_IGNORE` once the IPC header is read.*
Then in `main()`:
```c
int main(void)
{
    // Code inside main
    // .
    // .
    // .
        while (keep_running) {
                int event = read_single_window_event(i3_fd_event, &width, &height);

                if (!keep_running || event == EVENT_ERROR)
                        break;

                if (event == EVENT_FOCUS) {
                        if (width > height)
                                send_i3_split_command(i3_cmd_fd, "split h");
                        else
                                send_i3_split_command(i3_cmd_fd, "split v");
                }
        }
// Closing sequence
closing:
// Code for closing sequence
}
```
Look at the `if (event == EVENT_FOCUS)` check in the loop above—it makes the logic easy to follow.

### 3. Payloads & JSON Keys
Here we define the subscription payload sent to the i3 IPC along with the JSON keys and values we look for when parsing window events:
```c
#define SUBSCRIBE_WINDOW_EVENT_PAYLOAD     "[\"window\"]"
#define SUBSCRIBE_WINDOW_EVENT_PAYLOAD_LEN 10

#define JSON_WINDOW_CHANGE_KEY     "change"
#define JSON_WINDOW_CHANGE_VALUE_1 "focus"
#define JSON_WINDOW_CHANGE_VALUE_2 "new"

#define JSON_WINDOW_RECT       "rect"
#define JSON_WINDOW_HEIGHT_KEY "height"
#define JSON_WINDOW_WIDTH_KEY  "width"
```
**Next up we check out our functions**

---

# Functions
The goal of this part is to explain the technical details and why these choices were
made. Of course nothing is set in stone, this is the first version and everything is
subject to change if I have made a mistake.

These are our functions in the order we see them in the source code:
```c
ssize_t readn(int fd, void *ptr, size_t n) {}
int simple_atoi(const char *str) {}
const char *get_i3_socket_path(void) {}
void flush_reply(int fd, uint32_t size) {}
int connect_to_i3(const char *socket_path) {}
int window_events_subscribe(int i3_fd_event) {}
// These are helper functions
static int tok_eq(const char *json, const jsmntok_t *tok, const char *s, size_t len) {}
static int is_valid_window_change(const char *json, const jsmntok_t *tokens, int val_idx, int tokens_read) {}
static void parse_rect_dimensions(const char *json, const jsmntok_t *tokens, int *iter, int tokens_read, int *out_width, int *out_height) {}
static void skip_token(const jsmntok_t *tokens, int *idx, int tokens_read) {}
// -------------------------
int read_single_window_event(int i3_fd_event, int *out_width, int *out_height) {}
int send_i3_split_command(int i3_cmd_fd, const char *split_x) {}
```

### 1. Function readn
#### Source code
```c
ssize_t readn(int fd, void *ptr, size_t n)
{
        size_t  nleft;
        ssize_t nread;

        nleft = n;
        while (nleft > 0)
        {
                if ((nread = read(fd, ptr, nleft)) < 0)
                {
                        if (nleft == n)
                                return -1;
                        else break;
                } else if (nread == 0) {
                        break;
                }
                nleft -= nread;
                ptr   += nread;
        }
        return (n - nleft);
}
```
#### Explanation
This is the easiest to explain, it is copied from the book:
[Advanced Programming in the Unix Environment 3rd Edition](https://raw.githubusercontent.com/zwan074/technical-books/master/Advanced.Programming.in.the.UNIX.Environment.3rd.Edition.0321637739.pdf)
Basically a wrapper for the `read()` function.
It is chosen as a very safe and minimal way to ensure that we read everything that we
specify, this is because if we miss anything our future reads will not be synchronized,
and the program ceases to function.
For more details click on the link and read from the book.

---
### 2. Function simple_atoi()
#### Source code
```c
int simple_atoi(const char *str)
{
        int res = 0;
        int i = 0;

        for (; str[i] >= '0' && str[i] <= '9'; ++i)
                res = res * 10 + (str[i] - '0');

        return res;
}
```
#### Explanation
A minimal ASCII-to-integer converter inspired by:
K&R's *The C Programming Language (2nd Edition)*.
Because `jsmn` tokens point directly into the non-null-terminated
slice of `json_payload`, stopping at the first non-digit character
(`','` or `'}'`) allows direct parsing in place without copying substrings.
The only changes are the name of the function, the signature change:
`char[]` -> `const char *str`,
and defining `res` instead of `n`.

---
### 3. Function get_i3_socket_path()
#### Source
```c
const char *get_i3_socket_path(void)
{
        char *path = getenv("I3SOCK");
        if (path)
                return path;

        const char *fallback = "/tmp/i3-ipc.sock";
        if (access(fallback, F_OK) == 0)
                return fallback;

        return NULL;
}
```
#### Explanation
Almost fully transparent by the comments in `autotiling.c`.
2 checks are happening.
First we try to retrieve the environment variable `I3SOCK`. It should not
fail if i3 is running in the system. In the case the environment variable
is not set we also check the file `/tmp/i3-ipc.sock`.
If neither is found, it returns `NULL`.

In our program we connect twice using this path: once to listen for window events,
and a second time to have a dedicated channel for sending the split commands.

---
### 4. Function flush_reply()
#### Source code
```c
void flush_reply(int fd, uint32_t size)
{
        char buf[512] = {0};
        uint32_t remaining = size;
        uint32_t to_read = 0;

        while (remaining > 0)
        {
                to_read = remaining;

                if (remaining > sizeof(buf))
                        to_read = sizeof(buf);

                ssize_t n = readn(fd, buf, to_read);

                if (n <= 0)
                         break;

                remaining -= n;
        }
}
```
#### Explanation
Many times we get a reply in the buffer that we want to ignore, it is important
that we get rid of these messages, that's why this function exists.
It uses a static buffer of sufficient size to read it and discard it.
It goes without saying that it is a helper function.

---
### 5. Function connect_to_i3()
#### Source code
```c
int connect_to_i3(const char *socket_path)
{
        int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr = { .sun_family = AF_UNIX };

        if (!socket_path) {
                write(STDOUT_FILENO, ERR_SOCKET_PATH_NOT_FOUND, LEN_SOCKET_PATH_NOT_FOUND);
                goto out;
        }

        if (socket_fd == -1) {
                write(STDOUT_FILENO, ERR_FAILED_TO_CREATE_SOCKET, LEN_FAILED_TO_CREATE_SOCKET);
                goto out;
        }

        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
        addr.sun_path[ sizeof(addr.sun_path) - 1 ] = '\0';

        if (connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
                write(STDOUT_FILENO, ERR_FAILED_TO_CONNECT_TO_SOCKET, LEN_FAILED_TO_CONNECT_TO_SOCKET);
                close(socket_fd);
                goto out;
        }

        return socket_fd;

out:
    return -1;
}
```
#### Explanation
This function is used after we have saved the path to the socket.
Its goal is to return the file descriptor `socket_fd` so we can communicate with i3.
As we saw in [get_i3_socket_path() function](#3-function-get_i3_socket_path),
if we don't get the path we have `NULL` stored. If this function sees `NULL` in
`socket_path` it prints an error. Then we check the return of the `socket()`
call.

**Buffer Safety:** Because we are avoiding dynamic memory allocation (`malloc()`),
we copy the socket path into a fixed-size static struct (`addr.sun_path`).
We use `strncpy()` with `sizeof(...) - 1` and manually add the `\0` terminator.
This guarantees we will never have a buffer overflow, even if the environment
variable gives us an unexpectedly long path.

**Minimalistic Error Handling:** Notice the use of `goto out;` for error cleanup.
This is a classic C pattern (heavily used in the Linux kernel) that keeps the code
clean and prevents us from writing `return -1;` repeatedly. Also, when an error occurs,
we use `write(STDOUT_FILENO, ...)` paired with the `ERR_` macros we defined in the 
[Definitions section](#1-error-messages). This avoids the need to include `<stdio.h>`
for `printf()`, strictly following our rule to minimize includes.

If everything succeeds, the `connect()` system call links our socket to the
i3 window manager, and we return the file descriptor so the rest of the program can
start reading or writing.

---
### 6. Function window_events_subscribe()
#### Source code
```c
int window_events_subscribe(int i3_fd_event)
{
        struct i3_ipc_header reply_header = {0};
        struct i3_ipc_header header = {
                .magic = I3_IPC_MAGIC,
                .size  = SUBSCRIBE_WINDOW_EVENT_PAYLOAD_LEN,
                .type  = I3_IPC_MESSAGE_TYPE_SUBSCRIBE
        };

        write(i3_fd_event, &header, sizeof(struct i3_ipc_header));
        write(i3_fd_event, SUBSCRIBE_WINDOW_EVENT_PAYLOAD, SUBSCRIBE_WINDOW_EVENT_PAYLOAD_LEN);

        if (readn(i3_fd_event, &reply_header, sizeof(struct i3_ipc_header)) <= 0)
                return -1;

        flush_reply(i3_fd_event, reply_header.size);

        return 0;
}
```
#### Explanation
In order for the i3 ipc to transmit the correct messages for our program through the
socket we need to request them by sending the correct message. We have it defined
[in the definitions](#3-payloads--json-keys) for readability. We use the `i3_ipc_header` that we
include from `<i3/ipc.h>`. We get it ready and then send it to the ipc and we flush the
reply since it is really improbable that we can't subscribe at this point.

Despite this, if our read for the reply by the ipc fails we return `-1`, indicating that
our connection broke.
A return value of `0` means a successful subscription to the window events.

---
### 7. Function `read_single_window_event()` and Helpers
#### Helper Overview
To parse JSON with zero heap allocations, we use `jsmn` with a stack-allocated `jsmntok_t token_list[512]` array and four `static` helpers:
* **`tok_eq()`**: Checks whether a `JSMN_STRING` token matches an expected key/value string and exact byte length.
* **`is_valid_window_change()`**: Verifies that the `"change"` value is either `"focus"` or `"new"`.
* **`parse_rect_dimensions()`**: Walks the direct key-value pairs inside `"rect"` and parses `"width"` and `"height"` using `simple_atoi()`.
* **`skip_token()`**: Skips over any token and all of its nested children so we only inspect top-level keys of the root event and `"container"` object.

#### Source code
```c
int read_single_window_event(int i3_fd_event, int *out_width, int *out_height)
{
	int result = EVENT_ERROR;
	int tokens_read;
	int valid_change = 0;
	int found_width = -1;
	int found_height = -1;
	jsmn_parser p;
	jsmntok_t token_list[512];
	struct i3_ipc_header event_header = {0};
	char json_payload[4096] = {0};

	ssize_t n = readn(i3_fd_event, &event_header, sizeof(struct i3_ipc_header));
	if (n <= 0)
		goto out;

	result = EVENT_IGNORE;
	if (event_header.type != I3_IPC_EVENT_WINDOW ||
	    event_header.size >= sizeof(json_payload)) {
		flush_reply(i3_fd_event, event_header.size);
		goto out;
	}

	n = readn(i3_fd_event, json_payload, event_header.size);
	if (n <= 0)
		goto out;

	json_payload[event_header.size] = '\0';

	jsmn_init(&p);
	tokens_read = jsmn_parse(&p, json_payload, event_header.size,
				 token_list, sizeof(token_list) / sizeof(token_list[0]));
	if (tokens_read < 2 || token_list[0].type != JSMN_OBJECT)
		goto out;

	/* Walk ONLY top-level pairs of the root event object */
	int root_pairs = token_list[0].size;
	int idx = 1;

	for (int i = 0; i < root_pairs && idx + 1 < tokens_read; i++) {
		int key_idx = idx;
		int val_idx = idx + 1;

		if (tok_eq(json_payload, &token_list[key_idx], JSON_WINDOW_CHANGE_KEY, 6)) {
			valid_change = is_valid_window_change(json_payload, token_list,
							      val_idx, tokens_read);
			idx = val_idx + 1;
		} else if (tok_eq(json_payload, &token_list[key_idx], "container", 9) &&
			   token_list[val_idx].type == JSMN_OBJECT) {
			/* Walk ONLY direct pairs inside "container" */
			int cont_pairs = token_list[val_idx].size;
			int c_idx = val_idx + 1;

			for (int j = 0; j < cont_pairs && c_idx + 1 < tokens_read; j++) {
				int c_key = c_idx;
				int c_val = c_idx + 1;

				if (tok_eq(json_payload, &token_list[c_key], JSON_WINDOW_RECT, 4)) {
					int rect_iter = c_val;
					parse_rect_dimensions(json_payload, token_list,
							      &rect_iter, tokens_read,
							      &found_width, &found_height);
					break;
				}
				c_idx = c_val;
				skip_token(token_list, &c_idx, tokens_read);
			}
			idx = val_idx;
			skip_token(token_list, &idx, tokens_read);
		} else {
			idx = val_idx;
			skip_token(token_list, &idx, tokens_read);
		}
	}

	if (valid_change && found_width >= 0 && found_height >= 0) {
		*out_width = found_width;
		*out_height = found_height;
		result = EVENT_FOCUS;
	}

out:
	return result;
}
```
#### Explanation
1. **Header & Bounds Validation:** Reads `i3_ipc_header`. If the message is not `I3_IPC_EVENT_WINDOW` or its payload is greater than or equal to 4096 bytes (leaving room for a `\0` terminator), it flushes the payload and returns `EVENT_IGNORE`.
2. **Tokenization:** Parses `json_payload` into `token_list` using `jsmn_parse()`.
3. **Targeted Tree Walk:**
   * Walks top-level key-value pairs of the root JSON object.
   * Checks `"change"` to verify the event is `"focus"` or `"new"`.
   * Enters `"container"` and walks only its direct keys to locate `"rect"`, extracting `width` and `height` while skipping unrelated subtrees (`nodes`, `floating_nodes`, `window_properties`, etc.) via `skip_token()`.
4. **Validation:** Only sets `*out_width`, `*out_height`, and returns `EVENT_FOCUS` if both a valid `"change"` event and valid non-negative dimensions were found.

---
### 8. Function `send_i3_split_command()`
#### Source code
```c
int send_i3_split_command(int i3_cmd_fd, const char *split_x)
{
        struct i3_ipc_header header = {
                .magic = I3_IPC_MAGIC,
                .size  = strlen(split_x),
                .type  = I3_IPC_MESSAGE_TYPE_COMMAND
        };
        struct i3_ipc_header reply_header;

        write(i3_cmd_fd, &header, sizeof(struct i3_ipc_header));
        write(i3_cmd_fd, split_x, header.size);

        if (readn(i3_cmd_fd, &reply_header, sizeof(struct i3_ipc_header)) <= 0)
                return -1;

        flush_reply(i3_cmd_fd, reply_header.size);

        return 0;
}
```
#### Explanation
Packages either `"split h"` or `"split v"` into an `I3_IPC_MESSAGE_TYPE_COMMAND` IPC message, sends it over the dedicated `i3_cmd_fd` socket, and flushes i3's command response.

---
### 9. Function `handle_signal()`
#### Source code
```c
volatile sig_atomic_t keep_running = 1;
void handle_signal(int sig)
{
        (void)sig;
        keep_running = 0;
}
```
#### Explanation
Catches `SIGINT` and `SIGTERM` so the daemon shuts down cleanly:
* `volatile sig_atomic_t` ensures atomic reads/writes across signal interrupts and prevents the compiler from caching `keep_running` in a register inside the `while` loop.
* `(void)sig;` silences unused-parameter compiler warnings.
* Only sets the flag (strictly async-signal-safe), letting `main()` break out of the loop and close sockets cleanly.

---
### 10. Function `main()`
#### Source code
*Look inside `autotiling.c` to see how all of the functions above fit together.*
#### Explanation
`main()` orchestrates the daemon in four phases:
1. **Two-Socket Architecture:** Opens two separate IPC connections (`i3_fd_event` and `i3_cmd_fd`). This keeps the program single-threaded without `select()`/`poll()`: `i3_fd_event` blocks waiting for window events while `i3_cmd_fd` sends commands synchronously without interleaving event packets and command replies.
2. **Signal Registration:** Uses `sigaction()` (with `sa_flags = 0` so blocking `read()` calls are interrupted with `EINTR` rather than restarted automatically) to handle `SIGINT` and `SIGTERM`.
3. **Autotiling Event Loop:** Blocks on `read_single_window_event()`. Whenever a window is focused or created (`EVENT_FOCUS`), if `width > height` it sends `"split h"`; otherwise, it sends `"split v"`.
4. **Cleanup (`closing:` label):** Closes both file descriptors and prints the exit notice before returning `0`.
