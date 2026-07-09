/*
 * Style follows the Linux Kernel standards as found here:
 * https://docs.kernel.org/process/coding-style.html
 * One exception is the typedef struct, which is left there for convenience,
 * also, the extern variable.
 */
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>




/*
 * Error Message Definitions
 */
#define ERR_SOCKET_PATH_NOT_FOUND "Failed to find socket path, are you sure i3 is running?\n"
#define LEN_SOCKET_PATH_NOT_FOUND sizeof(ERR_SOCKET_PATH_NOT_FOUND) - 1

#define ERR_FAILED_TO_CREATE_SOCKET "Failed to Create Unix Socket.\n"
#define LEN_FAILED_TO_CREATE_SOCKET sizeof(ERR_FAILED_TO_CREATE_SOCKET) - 1

#define ERR_FAILED_TO_CONNECT_TO_SOCKET "Could not establish a connection to the Unix socket.\n"
#define LEN_FAILED_TO_CONNECT_TO_SOCKET sizeof(ERR_FAILED_TO_CONNECT_TO_SOCKET) - 1

#define ERR_SOCKET_ERROR_ABORT "Exiting due to socket Error ..."
#define LEN_SOCKET_ERROR_ABORT sizeof(ERR_SOCKET_ERROR_ABORT) - 1

#define EXIT_MESSAGE "\nClosed sockets. Exiting ...\n"
#define LEN_EXIT_MESSAGE sizeof(EXIT_MESSAGE) - 1

/*
 * I3 communication Definitions
 */
#define I3_IPC_MAGIC      "i3-ipc"
#define I3_IPC_HEADER_LEN 6

#define I3_IPC_MESSAGE_TYPE_COMMAND     0
#define I3_IPC_MESSAGE_TYPE_SUBSCRIBE   2

#define I3_IPC_EVENT_MASK                  (1U << 31)
#define I3_IPC_EVENT_WINDOW                (I3_IPC_EVENT_MASK | 3)
#define SUBSCRIBE_WINDOW_EVENT_PAYLOAD     "[\"window\"]"
#define SUBSCRIBE_WINDOW_EVENT_PAYLOAD_LEN 10

/*
 * Ipc Listener Return Values Definitions
 */
#define EVENT_IGNORE  0
#define EVENT_FOCUS   1
#define EVENT_ERROR   -1




typedef struct {
        char magic[I3_IPC_HEADER_LEN] __attribute__((nonstring));
        uint32_t size;
        uint32_t type;
} __attribute__((packed)) i3_ipc_header_t;




/*
 * This function is made to look for the "I3SOCK" environment variable.
 * We use the exterally defined environ variable that stores a list of strings
 * in this format "<Key>=<Value>" Then we search for the Name in these Keys, if
 * found we return a Pointer to the first character of <Value>.
 * Else we return NULL
 */
extern char **environ;
const char *simple_getenv(const char *name)
{
        char *env_value = NULL;
        size_t len = strlen(name);

        for (char **env = environ; *env != NULL; env++) {
                int name_cmp = strncmp(*env, name, len);
                if (name_cmp == 0 && (*env)[len] == '=')
                        env_value = (*env + len + 1);
        }
        return env_value;
}

/*
 * Simple ASCII To Integer (atoi) implementation
 */
int simple_atoi(const char *str)
{
        int res = 0;
        for (int i = 0; str[i] >= '0' && str[i] <= '9'; ++i)
                res = res * 10 + (str[i] - '0');

        return res;
}

/*
 * This function tries with 2 different ways to find the socket path.
 * First : with the I3SOCK environment variable
 * Second : in the fallback /tmp/i3-ipc.sock, mimicking i3-msg
 * If i3 socket is not found then we return NULL
 * !! It is up to the caller to handle the error.
 */
const char *get_i3_socket_path(void)
{
        const char *path = simple_getenv("I3SOCK");
        if (path)
                return path;

        const char *fallback = "/tmp/i3-ipc.sock";
        if (access(fallback, F_OK) == 0)
                return fallback;

        return NULL;
}

/*
 * This function Is used when the reply from the i3 ipc is useless to us
 * and we want to empty the socket buffer of the unneeded payload.
 * Else we lose the headers.
 *
 * Important Detail:
 * We don't use malloc for the reading buffer cause we don't use the heap in
 * this program
 */
void flush_reply(int fd, uint32_t size)
{
        char buf[512] = {0};
        uint32_t remaining = size;
        while (remaining > 0) {
                uint32_t to_read = remaining;

                if (remaining > sizeof(buf))
                        to_read = sizeof(buf);

                ssize_t n = read(fd, buf, to_read);

                if (n <= 0)
                        break;

                remaining -= n;
        }
}

/*
 * Create a UNIX socket and connect it with i3,
 * then return the i3 file descriptor.
 * (The convention for this script is to name it: i3_fd_<purpose>)
 * If Something fails return -1.
 */
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

/*
 * This functions job is to take a socket and make it transmit only window event messages
 * to our script.
 * This happens by sending i3 the appropriate message through the socket.
 */
int window_events_subscribe(int i3_fd_event)
{
        i3_ipc_header_t reply_header = {0};
        i3_ipc_header_t header = {
                .magic = I3_IPC_MAGIC,
                .size  = SUBSCRIBE_WINDOW_EVENT_PAYLOAD_LEN,
                .type  = I3_IPC_MESSAGE_TYPE_SUBSCRIBE
        };

        write(i3_fd_event, &header, sizeof(i3_ipc_header_t));
        write(i3_fd_event, SUBSCRIBE_WINDOW_EVENT_PAYLOAD, SUBSCRIBE_WINDOW_EVENT_PAYLOAD_LEN);

        if (read(i3_fd_event, &reply_header, sizeof(i3_ipc_header_t)) <= 0)
                return -1;

        flush_reply(i3_fd_event, reply_header.size);

        return 0;
}

/*
 * When it finds a window event, through the socket,
 * it searches to find if it was a focus event.
 * If it was then it returns the width and height of the focused window,
 * else it ignores in both cases if conditions are not met and return EVENT_IGNORE
 */
int read_single_window_event(int i3_fd_event, int *out_width, int *out_height)
{
        int result = EVENT_IGNORE;

        char *is_focus = NULL;
        char *is_new   = NULL;
        char *rect_ptr = NULL;

        i3_ipc_header_t event_header = {0};
        char json_payload[4096] = {0};

        ssize_t n = read(i3_fd_event, &event_header, sizeof(i3_ipc_header_t));
        size_t total_read = 0;

        if (n != sizeof(i3_ipc_header_t))
                goto out;

        if (event_header.type != I3_IPC_EVENT_WINDOW) {
                flush_reply(i3_fd_event, event_header.size);
                goto out;
        }

        if (event_header.size >= sizeof(json_payload)) {
                flush_reply(i3_fd_event, event_header.size);
                goto out;
        }

        while (total_read < event_header.size) {
                n = read(i3_fd_event, json_payload + total_read, event_header.size - total_read);

                if (n <= 0)
                        goto out;

                total_read += n;
        }

        json_payload[event_header.size] = '\0';

        is_focus = strstr(json_payload, "\"change\":\"focus\"");
        is_new   = strstr(json_payload, "\"change\":\"new\"");

        if (!is_focus && !is_new)
                goto out;

        result = EVENT_FOCUS;
        rect_ptr = strstr(json_payload, "\"rect\":{");
        if (rect_ptr != NULL) {
                char *width_ptr = strstr(rect_ptr, "\"width\":");
                char *height_ptr = strstr(rect_ptr, "\"height\":");

                if (width_ptr && height_ptr) {
                        *out_width = simple_atoi(width_ptr + 8);
                        *out_height = simple_atoi(height_ptr + 9);
                }
        }

out:
    return result;
}

/*

*/
int send_i3_split_command(int i3_cmd_fd, const char *split_x)
{
        i3_ipc_header_t header = {
                .magic = I3_IPC_MAGIC,
                .size  = strlen(split_x),
                .type  = I3_IPC_MESSAGE_TYPE_COMMAND
        };
        i3_ipc_header_t reply_header;

        write(i3_cmd_fd, &header, sizeof(i3_ipc_header_t));
        write(i3_cmd_fd, split_x, header.size);

        if (read(i3_cmd_fd, &reply_header, sizeof(i3_ipc_header_t)) <= 0)
                return -1;

        flush_reply(i3_cmd_fd, reply_header.size);

        return 0;
}

volatile sig_atomic_t keep_running = 1;
void handle_signal(int sig)
{
        (void)sig;
        keep_running = 0;
}




/*
 * The combination of all of the above,
 * We get two file descriptors, one for reading window events one for sending
 * split commands to i3. Then what we do is wait for a window event and extract
 * the dimensions of the window. The simple if block inside our loop decides which
 * split command we will send for the next split.
 */
int main(void)
{
        struct sigaction sa;
        const char *socket_path = get_i3_socket_path();
        int i3_fd_event = connect_to_i3(socket_path);
        int i3_cmd_fd = connect_to_i3(socket_path);
        int width  = 0;
        int height = 0;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handle_signal;
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        if (i3_fd_event == -1 || i3_cmd_fd == -1) {
                write(STDOUT_FILENO, ERR_SOCKET_ERROR_ABORT, LEN_SOCKET_ERROR_ABORT);
                return 1;
        }

        window_events_subscribe(i3_fd_event);

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

        close(i3_fd_event);
        close(i3_cmd_fd);
        write(STDOUT_FILENO, EXIT_MESSAGE, LEN_EXIT_MESSAGE);
        return 0;
}
