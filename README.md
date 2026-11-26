# Nokia 3 automatic power-on

Select **Power off** in the Android power menu and the Nokia 3 schedules its own return using the MediaTek real-time clock (RTC). Keep the phone powered throughout shutdown. The alarm is set ten seconds ahead and refreshed every two seconds while Android is shutting down. Allow additional time for Android and your apps to start.

Tested on a Nokia 3 **TA-1032** running Android 9, kernel from build **00WW_5_18D**, and Magisk **27.0**, with a continuous external supply at the battery terminals. In the power-menu test, the module verified the alarm, the phone powered back on, Magisk relaunched the watcher, and the separately installed CCTV app resumed streaming.

## Install with ADB

Open a terminal in this repository with the rooted Nokia connected and authorized for ADB. If several devices are connected, set `ANDROID_SERIAL` to the Nokia’s serial first. The repository includes the tested arm64 executable.

```sh
adb shell mkdir -p /data/local/tmp/nokia_poweron
adb push module.prop service.sh skip_mount poweron /data/local/tmp/nokia_poweron/
adb shell "su -c 'set -e; cd /data/local/tmp/nokia_poweron; chmod 755 poweron; ./poweron --self-test; mkdir /data/adb/modules/nokia_poweron; cp module.prop service.sh skip_mount poweron /data/adb/modules/nokia_poweron/; cd /data/adb/modules/nokia_poweron; chmod 755 service.sh poweron; chmod 644 module.prop skip_mount'"
adb reboot
```

These are first-install commands. If the module directory already exists, check **Magisk → Modules → Nokia 3 automatic power-on** before reinstalling. After Android boots, verify the watcher is running. The command prints its process ID.

```sh
adb shell pidof poweron
```

## Test a shutdown

1. Leave the external supply connected and select **Power off** from Android’s power menu.
2. Leave the Power button untouched. After the phone starts and ADB reconnects, read the last shutdown result.

```sh
adb shell "su -c 'cat /data/adb/modules/nokia_poweron/last-shutdown'"
```

A successful entry reads `Verified power-on alarm at RTC epoch … (+10 seconds)`. If another power-on alarm is already armed, the watcher preserves it and records an error. The module listens for Android’s `0userrequested` shutdown signal. A crash, thermal shutdown, abrupt loss of supply, or low-level shutdown command can leave the phone off.

## Keep the phone off or remove the module

To keep the phone off, **disable the module in Magisk before selecting Power off**. The running watcher checks the disable marker when shutdown begins. To uninstall, select **Remove** in Magisk and reboot.

## Rebuild the executable

The source uses the TA-1032 kernel’s vendor-specific 44-byte alarm readback structure. Other models and kernels are untested. To build with Android NDK r27c, set `ANDROID_NDK_HOME` to the NDK directory and select the toolchain directory for your host. For macOS:

```sh
"$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android28-clang" \
  -O2 -Wall -Wextra -Werror poweron.c -o poweron
```

SHA-256 of the included executable, verified against the installed watcher:

```text
b6175abaf54088cff2e8450673a2643b86ad9242175d0bae73ecfb243b7e7aca
```
