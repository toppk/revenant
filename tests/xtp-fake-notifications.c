#include <gio/gio.h>
#include <glib-unix.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A stand-in org.freedesktop.Notifications that records each request as hex fields. */
static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.freedesktop.Notifications'>"
    "    <method name='GetCapabilities'><arg type='as' direction='out'/></method>"
    "    <method name='GetServerInformation'>"
    "      <arg type='s' direction='out'/><arg type='s' direction='out'/>"
    "      <arg type='s' direction='out'/><arg type='s' direction='out'/>"
    "    </method>"
    "    <method name='Notify'>"
    "      <arg type='s' direction='in'/><arg type='u' direction='in'/>"
    "      <arg type='s' direction='in'/><arg type='s' direction='in'/>"
    "      <arg type='s' direction='in'/><arg type='as' direction='in'/>"
    "      <arg type='a{sv}' direction='in'/><arg type='i' direction='in'/>"
    "      <arg type='u' direction='out'/>"
    "    </method>"
    "    <method name='CloseNotification'><arg type='u' direction='in'/></method>"
    "    <signal name='NotificationClosed'><arg type='u'/><arg type='u'/></signal>"
    "  </interface>"
    "</node>";

typedef struct
{
        GDBusMethodInvocation *invocation;
        GVariant *reply;
} HeldCall;

static FILE *record;
static const char *ready_path;
static gboolean reject;
/* Held calls wait unanswered until SIGUSR1, which answers them and stops holding. */
static gboolean hold_notify;
static gboolean hold_all;
static GPtrArray *held;
static guint32 next_id = 1;
static GMainLoop *loop;

static void
WriteHex(const gchar *text)
{
        const unsigned char *bytes = (const unsigned char *)text;

        if (*bytes == '\0') {
                fputc('-', record);
                return;
        }
        for (; *bytes != '\0'; ++bytes)
                fprintf(record, "%02x", *bytes);
}

/* A NULL reply answers with the rejection error. */
static void
Answer(GDBusMethodInvocation *invocation, GVariant *reply)
{
        if (reply == NULL)
                g_dbus_method_invocation_return_dbus_error(
                    invocation, "org.freedesktop.DBus.Error.Failed", "rejected by test");
        else
                g_dbus_method_invocation_return_value(invocation, reply);
}

static void
MethodCall(GDBusConnection *connection, const gchar *sender, const gchar *path,
           const gchar *interface, const gchar *method, GVariant *parameters,
           GDBusMethodInvocation *invocation, gpointer data)
{
        GVariant *reply;

        (void)connection;
        (void)sender;
        (void)path;
        (void)interface;
        (void)data;
        if (g_strcmp0(method, "GetCapabilities") == 0) {
                const gchar *capabilities[] = {"body", "body-markup", NULL};

                reply = g_variant_new("(^as)", capabilities);
        } else if (g_strcmp0(method, "GetServerInformation") == 0) {
                reply = g_variant_new("(ssss)", "xtp-fake", "xtp", "1", "1.2");
        } else if (g_strcmp0(method, "Notify") == 0) {
                const gchar *application;
                const gchar *icon;
                const gchar *summary;
                const gchar *body;
                const gchar *image = "";
                guint32 replaces;
                gint32 timeout;
                GVariant *actions;
                GVariant *hints;

                g_variant_get(parameters, "(&su&s&s&s@as@a{sv}i)", &application, &replaces, &icon,
                              &summary, &body, &actions, &hints, &timeout);
                /* libnotify may send a named icon here or as an image hint. */
                if (!g_variant_lookup(hints, "image-path", "&s", &image))
                        (void)g_variant_lookup(hints, "image_path", "&s", &image);
                fprintf(record, "notify at=%" G_GINT64_FORMAT " app=%s icon=%s image=%s summary=",
                        g_get_monotonic_time() / 1000, application, *icon != '\0' ? icon : "-",
                        *image != '\0' ? image : "-");
                WriteHex(summary);
                fputs(" body=", record);
                WriteHex(body);
                fprintf(record, " actions=%zu result=%s\n", g_variant_n_children(actions),
                        reject ? "rejected" : "accepted");
                fflush(record);
                g_variant_unref(actions);
                g_variant_unref(hints);
                reply = reject ? NULL : g_variant_new("(u)", next_id++);
        } else {
                g_dbus_method_invocation_return_value(invocation, NULL);
                return;
        }
        /* libnotify asks for server information before Notify, so --hold lets it answer. */
        if (hold_all || (hold_notify && g_strcmp0(method, "Notify") == 0)) {
                HeldCall *call = g_new(HeldCall, 1);

                call->invocation = invocation;
                call->reply = reply != NULL ? g_variant_ref_sink(reply) : NULL;
                g_ptr_array_add(held, call);
                return;
        }
        Answer(invocation, reply);
}

static const GDBusInterfaceVTable vtable = {MethodCall, NULL, NULL, {0}};

static void
BusAcquired(GDBusConnection *connection, const gchar *name, gpointer data)
{
        GDBusNodeInfo *node = data;

        (void)name;
        if (g_dbus_connection_register_object(connection, "/org/freedesktop/Notifications",
                                              node->interfaces[0], &vtable, NULL, NULL,
                                              NULL) == 0) {
                fprintf(stderr, "cannot register the notification object\n");
                exit(EXIT_FAILURE);
        }
}

static void
NameAcquired(GDBusConnection *connection, const gchar *name, gpointer data)
{
        FILE *ready;

        (void)connection;
        (void)name;
        (void)data;
        ready = fopen(ready_path, "w");
        if (ready != NULL)
                fclose(ready);
}

static void
NameLost(GDBusConnection *connection, const gchar *name, gpointer data)
{
        (void)connection;
        (void)data;
        fprintf(stderr, "lost or could not own %s\n", name);
        exit(EXIT_FAILURE);
}

static gboolean
Release(gpointer data)
{
        guint i;

        (void)data;
        hold_notify = FALSE;
        hold_all = FALSE;
        for (i = 0; i < held->len; ++i) {
                HeldCall *call = g_ptr_array_index(held, i);

                Answer(call->invocation, call->reply);
                if (call->reply != NULL)
                        g_variant_unref(call->reply);
                g_free(call);
        }
        g_ptr_array_set_size(held, 0);
        return G_SOURCE_CONTINUE;
}

static gboolean
Quit(gpointer data)
{
        (void)data;
        g_main_loop_quit(loop);
        return G_SOURCE_REMOVE;
}

int
main(int argc, char **argv)
{
        GDBusNodeInfo *node;
        guint owner;
        int i;

        if (argc < 3) {
                fprintf(stderr, "usage: %s RECORD-FILE READY-FILE [--reject] [--hold|--hold-all]\n",
                        argv[0]);
                return EXIT_FAILURE;
        }
        for (i = 3; i < argc; ++i) {
                if (strcmp(argv[i], "--reject") == 0) {
                        reject = TRUE;
                } else if (strcmp(argv[i], "--hold") == 0) {
                        hold_notify = TRUE;
                } else if (strcmp(argv[i], "--hold-all") == 0) {
                        hold_all = TRUE;
                } else {
                        fprintf(stderr, "unknown option %s\n", argv[i]);
                        return EXIT_FAILURE;
                }
        }
        record = fopen(argv[1], "a");
        if (record == NULL) {
                perror(argv[1]);
                return EXIT_FAILURE;
        }
        ready_path = argv[2];
        held = g_ptr_array_new();
        node = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
        if (node == NULL)
                return EXIT_FAILURE;
        loop = g_main_loop_new(NULL, FALSE);
        g_unix_signal_add(SIGTERM, Quit, NULL);
        g_unix_signal_add(SIGUSR1, Release, NULL);
        owner = g_bus_own_name(G_BUS_TYPE_SESSION, "org.freedesktop.Notifications",
                               G_BUS_NAME_OWNER_FLAGS_NONE, BusAcquired, NameAcquired, NameLost,
                               node, NULL);
        g_main_loop_run(loop);
        g_bus_unown_name(owner);
        g_dbus_node_info_unref(node);
        g_main_loop_unref(loop);
        g_ptr_array_unref(held);
        fclose(record);
        return EXIT_SUCCESS;
}
