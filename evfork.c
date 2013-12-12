#define _POSIX_C_SOURCE 200801L
#define _BSD_SOURCE 1
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "evfork.h"

static void
ignore_children()
{
  struct sigaction nocld;
  memset(&nocld, 0, sizeof(struct sigaction));
  nocld.sa_flags = SA_NOCLDWAIT;
  sigaction(SIGCLD, &nocld, NULL);
}

/* get rid of all the environment variables we don't *need* to run. */
static void
simplify_env()
{
  unsetenv("LD_PRELOAD");
  const char* home = getenv("HOME");
  const char* path = getenv("PATH");
  const char* display = getenv("DISPLAY");
  if(clearenv() != 0) {
    fprintf(stderr, "Could not clear environment..");
  }
  if(setenv("PATH", path, 1) != 0) {
    fprintf(stderr, "setenv PATH failed.\n");
  }
  if(setenv("HOME", home, 1) != 0) {
    fprintf(stderr, "setenv HOME failed.\n");
  }
  if(display && setenv("DISPLAY", display, 1) != 0) {
    fprintf(stderr, "setenv DISPLAY failed.\n");
  }
}

static void reap();
static void reap_sync(pid_t);
typedef void (ryecatch)(pid_t);
static ryecatch* catcher;

/* Since we are ignoring our children, they could run off the cliff.  In this
 * function is the catcher.
 * On Linux, we actually don't need a catcher in the rye, because we ignore our
 * children.  But, like good parents, we're here wait(2)ing if they come
 * crawling back. */
static void
rye(pid_t pid)
{
  catcher = reap;
  if(getenv("LIBSITU_SYNC") != NULL) {
    catcher = reap_sync;
  }
  catcher(pid);
}

static void
waitpid_err(const pid_t pid, const int status)
{
  if(pid == -1 && errno == ECHILD) {
    /* Linux is just (properly) ignoring our children, as we asked it to. */
    return;
  } else if(pid == -1) {
    const int err = errno; /* getpid() might change errno */
    fprintf(stderr, "[%d] waitpid error: (code: %d).\n", (int)getpid(), err);
  }
  if(WIFEXITED(status)) {
    fprintf(stderr, "[%d] child (%d) exited normally (code: %d).\n",
            (int)getpid(), (int)pid, WEXITSTATUS(status));
    if(WEXITSTATUS(status) != 0) {
      fprintf(stderr, "[%d] bailing due to error.\n", (int)getpid());
      exit(1);
    }
  }
  if(pid != 0 && WIFSIGNALED(status)) {
    fprintf(stderr, "[%d] child (%d) terminated by signal (%d)\n",
            (int)getpid(), (int)pid, WTERMSIG(status));
  }
  if(pid != 0 && WIFSTOPPED(status)) {
    fprintf(stderr, "[%d] child (%d) was stopped (sig: %d)\n", (int)getpid(),
            (int)pid, WSTOPSIG(status));
  }
  if(pid != 0 && WIFCONTINUED(status)) {
    fprintf(stderr, "[%d] child (%d) continued.\n", (int)getpid(), (int)pid);
  }
}

static void
reap_sync(pid_t pid)
{
  int status;
  const pid_t cpid = waitpid(pid, &status, 0);
  waitpid_err(cpid, status);
}

static void
reap(pid_t pid)
{
  (void)pid; /* ignored. */
  int status;
  /* we use -1 to (potentially) catch a child fired off in a previous call. */
  const pid_t cpid = waitpid(-1, &status, WNOHANG);
  waitpid_err(cpid, status);
}

void
launch(char* argv[])
{
  if(getenv("LIBSITU_SYNC") == NULL) {
    ignore_children(); /* prevent zombies. */
  }
  pid_t pid = fork();
  if(pid == -1) {
    abort();
  }
  if(pid == 0) {
    simplify_env();
    if(setenv("LD_PRELOAD", "./libsitu.so", 1) != 0) {
      abort();
    }
    execvp(argv[0], argv);
    abort();
  }
  rye(pid);
}
