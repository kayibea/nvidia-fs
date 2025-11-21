// #define _GNU_SOURCE
// #define _POSIX_C_SOURCE 200809
#define FUSE_USE_VERSION 31

#include <errno.h>
#include <fcntl.h>
#include <fuse3/fuse.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char vram_val[16] = "0\n";
static char temp_val[16] = "0\n";
static char util_val[16] = "0\n";
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Thread to run nvidia-smi in loop and parse output
void *collector_thread(void *arg) {
  (void)arg;

  FILE *fp = popen(
      "nvidia-smi --query-gpu=memory.used,utilization.gpu,temperature.gpu "
      "--format=csv,noheader,nounits --loop-ms=3000",
      "r");
  // FILE *fp = popen(
  //     "nvidia-smi --query-gpu=memory.used,utilization.gpu,temperature.gpu "
  //     "--format=csv,noheader,nounits -l 3",
  //     "r");
  if (!fp) {
    perror("popen");
    return NULL;
  }

  char line[128];
  while (fgets(line, sizeof(line), fp)) {
    printf("LINE: %s\n", line);
    int vram, util, temp;
    if (sscanf(line, "%d, %d, %d", &vram, &util, &temp) == 3) {
      pthread_mutex_lock(&lock);
      snprintf(vram_val, sizeof(vram_val), "%d\n", vram);
      snprintf(util_val, sizeof(util_val), "%d\n", util);
      snprintf(temp_val, sizeof(temp_val), "%d\n", temp);
      pthread_mutex_unlock(&lock);
      printf("%d %d %d\n", vram, util, temp);
    }
  }

  pclose(fp);
  return NULL;
}

static int my_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi) {
  (void)fi;
  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (strcmp(path, "/vram") == 0 || strcmp(path, "/temp") == 0 ||
             strcmp(path, "/util") == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 16; // placeholder; real size is dynamic
  } else {
    return -ENOENT;
  }
  return 0;
}

static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
  (void)offset;
  (void)flags;
  (void)fi;
  if (strcmp(path, "/") != 0)
    return -ENOENT;
  filler(buf, ".", NULL, 0, 0);
  filler(buf, "..", NULL, 0, 0);
  filler(buf, "vram", NULL, 0, 0);
  filler(buf, "temp", NULL, 0, 0);
  filler(buf, "util", NULL, 0, 0);
  return 0;
}

static int my_open(const char *path, struct fuse_file_info *fi) {
  (void)fi;
  if (strcmp(path, "/vram") != 0 && strcmp(path, "/temp") != 0 &&
      strcmp(path, "/util") != 0)

    return -ENOENT;
  return 0;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  (void)fi;
  const char *data = NULL;
  pthread_mutex_lock(&lock);
  if (strcmp(path, "/vram") == 0)
    data = vram_val;
  else if (strcmp(path, "/temp") == 0)
    data = temp_val;
  else if (strcmp(path, "/util") == 0)
    data = util_val;
  pthread_mutex_unlock(&lock);

  if (!data)
    return -ENOENT;
  size_t len = strlen(data);
  if ((size_t)offset >= len)
    return 0;
  if (offset + size > len)
    size = len - offset;
  memcpy(buf, data + offset, size);
  return size;
}

static struct fuse_operations my_ops = {
    .getattr = my_getattr,
    .readdir = my_readdir,
    .open = my_open,
    .read = my_read,
};

int main(int argc, char *argv[]) {
  argv[argc++] = "-f";
  pthread_t tid;
  pthread_create(&tid, NULL, collector_thread, NULL);
  return fuse_main(argc, argv, &my_ops, NULL);
}
