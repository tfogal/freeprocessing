#define _POSIX_C_SOURCE 200801L
#define _BSD_SOURCE 1
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "launcher.h"
#include "debug.h"

DECLARE_CHANNEL(consumer);

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

/* Since we are ignoring our children, they could run off the cliff.  This
 * function catches them.
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
      fprintf(stderr, "[%d] bailing due to child failure.\n", (int)getpid());
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

/** assumes argv is big enough to hold all of the given tokens. */
static void
tokenize(char* s, char* argv[]) {
  char* saveptr;
  size_t i = 0;
  argv[i++] = strtok_r(s, " ", &saveptr);
  while(argv[i-1]) {
    argv[i++] = strtok_r(NULL, " ", &saveptr);
  }
}
typedef int (charfunc)(int);
static size_t
count(const char* s, charfunc predicate) {
  size_t n = 0;
  while(*s) {
    if(predicate(*s)) { ++n; }
    ++s;
  }
  return n;
}

void
launch(const char* cmdline)
{
  /* We fix our environment in the child, but at the very least we want to stop
   * tracing opens/closes for the consumer process */
  unsetenv("LD_PRELOAD");
  /* ignore children exiting, but not if we're synchronous---then we need to
   * track children so we can grab e.g. their exit status, later. */
  if(getenv("LIBSITU_SYNC") == NULL) {
    ignore_children(); /* attempt to ignore children, to prevent zombies. */
  }

  /* get our command line into a form we can exec easily. */
  const size_t ntokens = count(cmdline, isspace);
  char** tokens = malloc(sizeof(char*) * (ntokens+1));
  tokenize((char*)cmdline, tokens);

  pid_t pid = fork();
  /* Restrictions in OMPI mean that, after a fork, we can't assume any pages
   * are writable!  This basically means we must immediately exec.  Hence why
   * we fixed our environment up front. */
  if(pid == -1) {
    ERR(consumer, "Could not fork vis process: %d\n", errno);
    return;
  }
  if(pid == 0) {
    simplify_env();
    execv(tokens[0], tokens);
    ERR(consumer, "could not exec pvbatch: %d\n", errno);
    return;
  }
  free(tokens);
  /* This is joyful.  As sigaction(2) says:
   *   "the only completely portable method of ensuring that terminated
   *    children do not become zombies is to catch the SIGCHLD signal and
   *    perform a wait(2) or similar."
   * If the zombie horde attacks, maybe this will trim off the fast
   * zombies.
   * Though, if we are synchronous we need to search the rye for children
   * anyway. */
  rye(pid);
}
