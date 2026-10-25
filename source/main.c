#include "hv_defeat_0304.h"
#include "hv_defeat_0506.h"
#include "hv_defeat_0607.h"
#include "hv_defeat_1160.h"
#include "config.h"
#include "hv_defeat_13xx.h"
#include "loader.h"
#include "prepare_resume.h"
#include "utils.h"
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

// 11.60 keeps page tables from this process linked into PID 1 once the Linux
// files are staged, so it must not exit after that.
static int fail_after_staging(void) {
  if (PS5_FW_IS_11XX(fw)) {
    notify("Linux files are still staged. Cold reboot before trying again.\n");
    while (1)
      sleep(30);
  }
  return -1;
}


/* ---- stdout tee: every printf/notify line also lands in /data ---------- */
struct tee_args { int rd, out, log; };
static struct tee_args g_tee;

static void *tee_main(void *arg) {
  struct tee_args *a = (struct tee_args *)arg;
  char buf[4096];
  ssize_t n;
  unsigned long long total = 0;
  while ((n = read(a->rd, buf, sizeof(buf))) > 0) {
    if (write(a->out, buf, (size_t)n) < 0) { /* keep draining */ }
    if (a->log >= 0) {
      if (write(a->log, buf, (size_t)n) > 0) {
        total += (unsigned long long)n;
        if (total >= 0x1000) {
          fsync(a->log);
          total = 0;
        }
      }
    }
  }
  return NULL;
}

static void log_tee_start(void) {
  int log, saved, p[2];
  mkdir("/data/linuxdebug", 0777);
  log = open("/data/linuxdebug/loader.log",
             O_WRONLY | O_CREAT | O_TRUNC, 0666);
  saved = dup(1);
  if (log < 0 || saved < 0 || pipe(p)) {
    if (log >= 0) close(log);
    if (saved >= 0) close(saved);
    return;
  }
  if (dup2(p[1], 1) < 0) {
    close(p[0]); close(p[1]); close(log); close(saved);
    return;
  }
  close(p[1]);
  setvbuf(stdout, NULL, _IONBF, 0);
  g_tee.rd = p[0];
  g_tee.out = saved;
  g_tee.log = log;
  pthread_t t;
  if (pthread_create(&t, NULL, tee_main, &g_tee)) {
    dup2(saved, 1);
    close(p[0]); close(log); close(saved);
  }
}

int main(void) {
  log_tee_start();
  if (setup_env()) {
    notify("Something went wrong while initiating.\nPlease make sure your fw "
           "is supported.");
    return -1;
  }

  if (fetch_linux(&linux_i)) {
    notify("Something went wrong while installing linux files.\n");
    return -1;
  }

  void *shellcode_kernel;
  size_t shellcode_kernel_len;
  if (prepare_resume(&shellcode_kernel, &shellcode_kernel_len)) {
    notify("Something went wrong while preparing resume.\n");
    return fail_after_staging();
  }

  if ((0x0300 <= fw) && (fw < 0x0500)) {
    if (hv_defeat_0304(shellcode_kernel, shellcode_kernel_len))
      goto err;
  } else if ((0x0500 <= fw) && (fw < 0x0650)) {
    if (hv_defeat_0506(shellcode_kernel, shellcode_kernel_len))
      goto err;
  } else if ((0x0650 <= fw) && (fw < 0x0800)) {
    if (hv_defeat_0607(shellcode_kernel, shellcode_kernel_len))
      goto err;
  } else if (PS5_FW_IS_12XX(fw) || fw == 0x1300 || fw == 0x1320 ||
             fw == 0x1340 || fw == 0x1342 || fw == 0x1360) {
    if (hv_defeat_13xx(shellcode_kernel, shellcode_kernel_len))
      goto err;
  } else if (PS5_FW_IS_11XX(fw)) {
    if (hv_defeat_1160(shellcode_kernel, shellcode_kernel_len))
      goto err;
  } else {
    goto err;
  }

  notify("Finished preparation. Going to rest mode in 5 seconds.\nPlease wait "
         "for the orange light to stop "
         "blinking and then wakeup to Linux :)\n");

  sleep(5);
  enter_rest_mode();

  while (1) {
    sleep(30);
  }

  return 0;

err:
  notify("Something went wrong while defeating Hypervisor.\nPlease make sure "
         "your fw is supported.");
  return fail_after_staging();
}
