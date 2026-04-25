/**
 * @file src/platform/linux/vdisplay_helper.cpp
 * @brief Privileged helper for virtual display debugfs/sysfs writes.
 *
 * Installed with cap_dac_override+ep so the main sunshine process
 * does not need CAP_DAC_OVERRIDE in its capability set.
 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <regex.h>
#include <sys/stat.h>
#include <unistd.h>

static bool
  validate_path(const char *path) {
  if (strcmp(path, "/sys/kernel/debug/dri") == 0) {
    return true;
  }

  static const char *patterns[] = {
    "^/sys/kernel/debug/dri/[0-9]+/[A-Za-z]+(-[A-Za-z]+)*-[0-9]+/edid_override$",
    "^/sys/kernel/debug/dri/[0-9]+/[A-Za-z]+(-[A-Za-z]+)*-[0-9]+/trigger_hotplug$",
    "^/sys/class/drm/card[0-9]+-[A-Za-z]+(-[A-Za-z]+)*-[0-9]+/status$",
  };

  for (auto pattern : patterns) {
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0) {
      continue;
    }
    int match = regexec(&re, path, 0, nullptr, 0);
    regfree(&re);
    if (match == 0) {
      return true;
    }
  }

  return false;
}

static int
  safe_open(const char *path) {
  return open(path, O_WRONLY | O_NOFOLLOW);
}

static int
  cmd_write_edid(const char *path) {
  uint8_t buf[256];
  ssize_t total = 0;
  while (total < static_cast<ssize_t>(sizeof(buf))) {
    ssize_t n = read(STDIN_FILENO, buf + total, sizeof(buf) - total);
    if (n <= 0) {
      break;
    }
    total += n;
  }
  if (total != 128 && total != 256) {
    fprintf(stderr, "Invalid EDID size: %zd\n", total);
    return 3;
  }
  int fd = safe_open(path);
  if (fd < 0) {
    perror("open");
    return 4;
  }
  ssize_t written = write(fd, buf, total);
  close(fd);
  if (written != total) {
    perror("write");
    return 5;
  }
  return 0;
}

static int
  cmd_clear_edid(const char *path) {
  int fd = safe_open(path);
  if (fd < 0) {
    perror("open");
    return 4;
  }
  ssize_t n = write(fd, "reset", 5);
  if (n < 0) {
    lseek(fd, 0, SEEK_SET);
    write(fd, "", 0);
  }
  close(fd);
  return 0;
}

static int
  cmd_write_string(const char *path, const char *value) {
  int fd = safe_open(path);
  if (fd < 0) {
    perror("open");
    return 4;
  }
  ssize_t n = write(fd, value, strlen(value));
  close(fd);
  if (n < 0) {
    perror("write");
    return 5;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <command> <path> [value]\n", argv[0]);
    return 1;
  }

  const char *command = argv[1];
  const char *path = argv[2];

  if (!validate_path(path)) {
    fprintf(stderr, "Path rejected: %s\n", path);
    return 2;
  }

  if (strcmp(command, "write-edid") == 0) {
    return cmd_write_edid(path);
  }
  if (strcmp(command, "clear-edid") == 0) {
    return cmd_clear_edid(path);
  }
  if (strcmp(command, "write-string") == 0) {
    if (argc < 4) {
      fprintf(stderr, "write-string requires a value argument\n");
      return 1;
    }
    const char *value = argv[3];
    static const char *allowed_values[] = {"1", "on", "off", "detect"};
    bool valid = false;
    for (auto v : allowed_values) {
      if (strcmp(value, v) == 0) {
        valid = true;
        break;
      }
    }
    if (!valid) {
      fprintf(stderr, "Value rejected: %s\n", value);
      return 2;
    }
    return cmd_write_string(path, value);
  }
  if (strcmp(command, "check-path") == 0) {
    struct stat st;
    return stat(path, &st) == 0 ? 0 : 1;
  }

  fprintf(stderr, "Unknown command: %s\n", command);
  return 1;
}
