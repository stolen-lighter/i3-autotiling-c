# Functionality
The way this autotiling script works is by checking the height and the width of the focused window and splitting
the bigger dimension. By default i3 splits to the right and to the bottom. So thats the behavior of this script too.

It is made to be running in the background, it uses a maximum of `36Kb` RAM not taking into account initialization.\

Suggestions to improve documentation in both readme and contibuting files are always welcome.

---
# Logic

The functions used are these:
- `const char *simple_getenv(const char *name)`
- `int simple_atoi(const char *str)`
- `const char *get_i3_socket_path(void)`
- `void flush_reply(int fd, uint32_t size)`
- `int connect_to_i3(const char *socket_path)`
- `int window_events_subscribe(int i3_fd_event)`
- `int read_single_window_event(int i3_fd_event, int *out_width, int *out_height)`
- `int send_i3_split_command(int i3_cmd_fd, const char *split_x)`
- `void handle_signal(int sig)`

*Note: Extra details are inside the autotiling.c file in the form of comments, if you want to contribute and have questions feel free to message me.*
*Extra note: if there exists no explaination of the implementation is because it is though of as intuitive or easy to recognize, even so the above
message still holds up, DO NOT HESITATE TO ASK ANYTHING IF YOU INTEND TO CONTRIBUTE !!!*

First to communicate with the i3 ipc, we need to find its socket path using `sys/socket`.
We try two methods:
1. Checking the `I3SOCK` environment variable.
3. Falling back to `/tmp/i3-ipc.sock` (mimicking `i3-msg`).

We now create two separate connections to the socket.
First `i3_fd_event` for subscribing to and reading continuous window events
and `i3_fd_cmd` for sending our split commands without interrupting our event listener.

## How the event listener works
First we read the header of fixxed length.
Then we test to see if we got the entire header, if not the connection might have been corrupted.
The next check is important. We first check to see if the event is of correct type, if not we flush the payload
to clean our socket and return the EVENT_IGNORE flag.
    
The next part is a decision, there may be better implementations, but this is the one chosen for now.
Our payload buffer is 4Kb. If the incoming payload is bigger we will overflow.
So we chose to ignore it even if it is of the correct type. The chances of the payload being a 4Kb Json formated
string are very slim, so it is a calculated risk. The logic is that we must read the window payload a maximum of one times,
in order to be fast.

In the next part we start to read the payload, we check if the socket closes at any point, or if we encounter an error
thus the check in the while loop: `if (n <= 0)`.
After we move the payload to our buffer named `json_payload`, we append the string terminator at the end.

We search for the two sub-strings of interest and store the "Boolean" value of the search result in the respective variables,
(for clarity we are referring to the `is_focus` and the `is_new` variables)
if both of them are NULL, them are null we return the EVENT_IGNORE flag.

Else it means it is an Event of interest and we search the new values of width and height to update our variables.
Thats why the flag EVENT_FOCUS is for, to inform that the height and width variables have been changed.

*The next part is where i expect the changes for different implementations to be made.*
A simple if-else block checks for the bigger dimension (if dimensions are equal we still fallback to the else block), and then
split on that dimension for the new window.

---
*Final Note: The source code itself but the make file too is subject to change as i gain better understanding of the concepts.
nothing is perfect but if this scripts finds the users it is intended for i am sure it will involve into something much better.*
