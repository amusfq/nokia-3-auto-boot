// Nokia TA-1032, 00WW_5_18D: the vendor alarm readback extends rtc_wkalrm by one word.
#include <errno.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/system_properties.h>
#include <time.h>
#include <unistd.h>

struct vendor_alarm {
    struct rtc_wkalrm alarm;
    uint32_t flags;
};
_Static_assert(sizeof(struct vendor_alarm) == 44, "Nokia RTC ABI");
#define GET_POWER_ON _IOR('a', 10, struct vendor_alarm)
#define SET_POWER_ON_WITH_LOGO _IOW('a', 8 | (7 << 4), struct timespec)

static int user_shutdown(const char *reason) {
    return strcmp(reason, "0userrequested") == 0;
}

static time_t alarm_time(const struct rtc_time *rtc) {
    struct tm date = {.tm_year = rtc->tm_year, .tm_mon = rtc->tm_mon,
        .tm_mday = rtc->tm_mday, .tm_hour = rtc->tm_hour,
        .tm_min = rtc->tm_min, .tm_sec = rtc->tm_sec};
    return timegm(&date);
}

static int arm(int fd, time_t *previous) {
    struct vendor_alarm reply = {0};
    if (ioctl(fd, GET_POWER_ON, &reply) < 0) return -1;
    if (reply.alarm.enabled && alarm_time(&reply.alarm.time) != *previous) {
        errno = EBUSY;
        return -1;
    }
    FILE *clock = fopen("/sys/class/rtc/rtc0/since_epoch", "r");
    if (!clock) return -1;
    long now;
    int valid = fscanf(clock, "%ld", &now);
    fclose(clock);
    if (valid != 1) return -1;
    struct timespec target = {.tv_sec = now + 10};
    if (ioctl(fd, SET_POWER_ON_WITH_LOGO, &target) < 0 ||
        ioctl(fd, GET_POWER_ON, &reply) < 0) return -1;
    if (reply.alarm.enabled != 3 || alarm_time(&reply.alarm.time) != target.tv_sec) {
        errno = EIO;
        return -1;
    }
    *previous = target.tv_sec;
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0)
        return !(user_shutdown("0userrequested") && !user_shutdown("1userrequested") &&
                 !user_shutdown("0thermal") && !user_shutdown("0battery") && !user_shutdown(""));
    int lock = open("watcher.lock", O_CREAT | O_RDWR, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) != 0) return 1;
    uint32_t serial = 0;
    char reason[PROP_VALUE_MAX];
    for (;;) {
        const prop_info *prop = __system_property_find("sys.shutdown.requested");
        __system_property_get("sys.shutdown.requested", reason);
        if (user_shutdown(reason)) break;
        if (*reason) return 0;
        __system_property_wait(prop, serial, &serial, NULL);
    }
    if (access("disable", F_OK) == 0 || access("remove", F_OK) == 0) return 0;
    int fd = open("/dev/alarm", O_RDWR);
    time_t previous = 0;
    int armed = fd >= 0 && arm(fd, &previous) == 0;
    int error = errno;
    int log = open("last-shutdown", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (log >= 0) {
        if (armed) dprintf(log, "Verified power-on alarm at RTC epoch %ld (+10 seconds)\n", (long)previous);
        else dprintf(log, "Could not arm power-on alarm: %s\n", strerror(error));
        fsync(log);
        close(log);
    }
    if (!armed) return 1;
    // Android may spend longer than ten seconds closing apps and flushing storage.
    while (sleep(2) == 0 && arm(fd, &previous) == 0) {}
    return 0;
}
