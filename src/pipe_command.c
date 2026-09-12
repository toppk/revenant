#include "pipe_command.h"

#include "diagnostics.h"

#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct XtpPipeCommand
{
        XtpPipeCommand *next;
        XtAppContext context;
        pid_t child;
        int fd;
        XtInputId input;
        XtIntervalId timer;
        unsigned long reap_interval_ms;
        char *text;
        size_t length;
        size_t written;
        XtpPipeCommandDoneFn done;
        void *closure;
        /* The helper is its own process group; otherwise only the direct child can be signalled. */
        bool own_group;
        pid_t group;
};

static void
StopWriting(XtpPipeCommand *job)
{
        if (job->input != (XtInputId)0) {
                XtRemoveInput(job->input);
                job->input = (XtInputId)0;
        }
        if (job->fd >= 0) {
                close(job->fd);
                job->fd = -1;
        }
        free(job->text);
        job->text = NULL;
}

static void
Finish(XtpPipeCommand *job)
{
        StopWriting(job);
        if (job->timer != (XtIntervalId)0)
                XtRemoveTimeOut(job->timer);
        if (job->done != NULL)
                job->done(job, job->closure);
        free(job);
}

static void Reap(XtPointer closure, XtIntervalId *id);

/* The group id stays reserved while any member lives, so this probe is safe to repeat. */
static bool
GroupAlive(const XtpPipeCommand *job)
{
        return job->own_group && job->group > 0 && kill(-job->group, 0) == 0;
}

static void
ScheduleReap(XtpPipeCommand *job)
{
        job->timer = XtAppAddTimeOut(job->context, job->reap_interval_ms, Reap, job);
        if (job->reap_interval_ms < 1000UL)
                job->reap_interval_ms *= 2UL;
}

/* The child is polled rather than caught with SIGCHLD so the PTY child's handling is untouched. */
static void
Reap(XtPointer closure, XtIntervalId *id)
{
        XtpPipeCommand *job = closure;
        int status = 0;
        pid_t result;

        (void)id;
        job->timer = (XtIntervalId)0;
        if (job->child < 0) {
                if (GroupAlive(job)) {
                        ScheduleReap(job);
                        return;
                }
                XtpLog(XTP_LOG_INFO, "pipe", "helper group finished group=%ld", (long)job->group);
                Finish(job);
                return;
        }
        result = waitpid(job->child, &status, WNOHANG);
        if (result == 0) {
                ScheduleReap(job);
                return;
        }
        if (result < 0 && errno == EINTR) {
                ScheduleReap(job);
                return;
        }
        if (result < 0)
                XtpLog(XTP_LOG_WARNING, "pipe", "pid=%ld lost: %s", (long)job->child,
                       strerror(errno));
        else if (WIFEXITED(status))
                XtpLog(XTP_LOG_INFO, "pipe", "command exited pid=%ld status=%d", (long)job->child,
                       WEXITSTATUS(status));
        else
                XtpLog(XTP_LOG_INFO, "pipe", "command ended pid=%ld signal=%d", (long)job->child,
                       WIFSIGNALED(status) ? WTERMSIG(status) : 0);
        job->child = (pid_t)-1;
        /* Nothing will read what is left once the helper is gone; Xt may not report the
         * full pipe as writable again, so the poll ends the write here. */
        if (job->fd >= 0 && job->written < job->length)
                XtpLog(XTP_LOG_WARNING, "pipe",
                       "output pipe closed early written=%zu of %zu: helper exited", job->written,
                       job->length);
        StopWriting(job);
        /* Background descendants keep the group alive; the job stays until it empties so
         * teardown can still reach them. */
        if (GroupAlive(job)) {
                ScheduleReap(job);
                return;
        }
        Finish(job);
}

static void
Writable(XtPointer closure, int *fd, XtInputId *id)
{
        XtpPipeCommand *job = closure;
        ssize_t count;

        (void)fd;
        (void)id;
        while (job->written < job->length) {
                count = write(job->fd, job->text + job->written, job->length - job->written);
                if (count >= 0) {
                        job->written += (size_t)count;
                        continue;
                }
                if (errno == EINTR)
                        continue;
                if (errno == EAGAIN)
                        return;
                XtpLog(XTP_LOG_WARNING, "pipe", "output pipe closed early written=%zu of %zu: %s",
                       job->written, job->length, strerror(errno));
                break;
        }
        if (job->written == job->length)
                XtpLog(XTP_LOG_INFO, "pipe", "output written bytes=%zu", job->written);
        StopWriting(job);
        if (job->child < 0)
                Finish(job);
}

XtpPipeCommand *
XtpPipeCommandStart(XtAppContext context, const char *command, const char *directory, char *text,
                    size_t length, XtpPipeCommandDoneFn done, void *closure)
{
        XtpPipeCommand *job;
        int fds[2];
        int flags;

        if (command == NULL || *command == '\0' || text == NULL) {
                free(text);
                return NULL;
        }
        job = calloc(1, sizeof(*job));
        if (job == NULL || pipe(fds) != 0) {
                XtpLog(XTP_LOG_WARNING, "pipe", "cannot create the output pipe: %s",
                       strerror(errno));
                free(job);
                free(text);
                return NULL;
        }
        /* The event loop writes this end; it must never block, so a failure to arrange that
         * abandons the job before anything is started. */
        flags = fcntl(fds[1], F_GETFL);
        if (flags < 0 || fcntl(fds[1], F_SETFL, flags | O_NONBLOCK) != 0 ||
            fcntl(fds[1], F_SETFD, FD_CLOEXEC) != 0) {
                XtpLog(XTP_LOG_WARNING, "pipe", "cannot make the output pipe non-blocking: %s",
                       strerror(errno));
                close(fds[0]);
                close(fds[1]);
                free(job);
                free(text);
                return NULL;
        }
        job->context = context;
        job->done = done;
        job->closure = closure;
        job->fd = fds[1];
        job->text = text;
        job->length = length;
        job->reap_interval_ms = 50UL;
        job->child = fork();
        if (job->child < 0) {
                XtpLog(XTP_LOG_WARNING, "pipe", "cannot fork: %s", strerror(errno));
                close(fds[0]);
                close(fds[1]);
                free(text);
                free(job);
                return NULL;
        }
        if (job->child == 0) {
                /* Its own process group lets teardown terminate the helper and its children. */
                (void)setpgid(0, 0);
                (void)signal(SIGPIPE, SIG_DFL);
                if (dup2(fds[0], STDIN_FILENO) < 0)
                        _exit(126);
                close(fds[0]);
                if (directory != NULL && *directory != '\0' && chdir(directory) != 0)
                        _exit(126);
                execl("/bin/sh", "sh", "-c", command, (char *)NULL);
                _exit(127);
        }
        close(fds[0]);
        job->own_group = setpgid(job->child, job->child) == 0 || getpgid(job->child) == job->child;
        job->group = job->own_group ? job->child : (pid_t)-1;
        if (!job->own_group)
                XtpLog(XTP_LOG_WARNING, "pipe",
                       "pid=%ld has no process group of its own; only it can be stopped at exit",
                       (long)job->child);
        XtpLog(XTP_LOG_INFO, "pipe", "spawned pid=%ld bytes=%zu directory=%s", (long)job->child,
               length, directory != NULL && *directory != '\0' ? directory : "(inherited)");
        job->input =
            XtAppAddInput(context, job->fd, (XtPointer)(uintptr_t)XtInputWriteMask, Writable, job);
        ScheduleReap(job);
        return job;
}

static pid_t
WaitNoHang(pid_t child, int *status)
{
        pid_t result;

        do
                result = waitpid(child, status, WNOHANG);
        while (result < 0 && errno == EINTR);
        return result;
}

/* Teardown terminates whatever the helper left in its process group: SIGTERM, a short
 * grace period, then SIGKILL, each sent only while the group still has members (a reaped
 * direct child without a group is never signalled again, since its id may be reused). */
static void
TerminateChild(XtpPipeCommand *job)
{
        const struct timespec step = {0, 10000000L};
        int status = 0;
        bool leader_reaped = job->child < 0 || WaitNoHang(job->child, &status) != 0;
        bool graceful = false;
        int attempt;

        if (job->own_group) {
                if (GroupAlive(job))
                        (void)kill(-job->group, SIGTERM);
                for (attempt = 0; attempt < 25 && GroupAlive(job); ++attempt) {
                        (void)nanosleep(&step, NULL);
                        if (!leader_reaped)
                                leader_reaped = WaitNoHang(job->child, &status) != 0;
                }
                graceful = !GroupAlive(job);
                if (!graceful)
                        (void)kill(-job->group, SIGKILL);
        } else if (!leader_reaped) {
                (void)kill(job->child, SIGTERM);
                for (attempt = 0; attempt < 25 && !leader_reaped; ++attempt) {
                        (void)nanosleep(&step, NULL);
                        leader_reaped = WaitNoHang(job->child, &status) != 0;
                }
                graceful = leader_reaped;
                if (!leader_reaped)
                        (void)kill(job->child, SIGKILL);
        }
        if (!leader_reaped) {
                while (waitpid(job->child, &status, 0) < 0 && errno == EINTR)
                        ;
        }
        if (job->own_group && !graceful)
                XtpLog(XTP_LOG_INFO, "pipe", "command killed group=%ld", (long)job->group);
        else if (job->child > 0 || job->own_group)
                XtpLog(XTP_LOG_INFO, "pipe", "command terminated group=%ld", (long)job->group);
        job->child = (pid_t)-1;
}

void
XtpPipeCommandAbandon(XtpPipeCommand *job)
{
        if (job == NULL)
                return;
        if (job->written < job->length)
                XtpLog(XTP_LOG_WARNING, "pipe", "abandoning output written=%zu of %zu",
                       job->written, job->length);
        StopWriting(job);
        if (job->timer != (XtIntervalId)0) {
                XtRemoveTimeOut(job->timer);
                job->timer = (XtIntervalId)0;
        }
        if (job->child > 0 || GroupAlive(job))
                TerminateChild(job);
        free(job);
}

XtpPipeCommand **
XtpPipeCommandLink(XtpPipeCommand *job)
{
        return &job->next;
}
