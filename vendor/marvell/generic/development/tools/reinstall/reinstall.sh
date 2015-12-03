#!/bin/bash
#
# Utility script to install a new system image.

error_exit() {
  local message=$1
  echo "Error: $message"
  exit 1
}

ADB=out/host/linux-x86/bin/adb
if [ ! -x "$ADB" ]; then
  ADB=$(which adb)
fi

if [ -z "$ADB" ]; then
  error_exit "Cannot find the 'adb' tool, please put it on your PATH"
fi

CLEAN_INSTALL=0
CACHE_PATH="/cache"
CHECK_OWNERSHIP=1

### Main script

usage() {
  echo "Usage: $0 [-c|--clean] [-m|--method <method>] [-f|--force] otapackage [<host>]"
  echo "       clean: wipe /data before rebooting"
  echo "       force: skip checking the ownership of the target device"
  echo "        host: remote target (default=${TARGET_HOST})"
  echo "  otapackage: package to install"
  echo ""
}

parse_args() {
  while true; do
    case "$1" in
      -c|--clean)
        CLEAN_INSTALL=1
        shift
        ;;

      -f|--force)
        CHECK_OWNERSHIP=0
        shift
        ;;

      *)
        break
        ;;
    esac
  done

  UPDATE=$1
  if [ ! -r "$UPDATE" ]; then
    usage
    error_exit "Cannot read update '$UPDATE'"
  fi

  if [ $# -gt 1 ]; then
    TARGET_HOST=$2
  fi

  if [ -z "$TARGET_HOST" ]; then
    usage
    error_exit "Target host name is required."
  fi

  if [[ $TARGET_HOST != *:* ]]; then
    # Add default port if only an IP was specified.
    TARGET_HOST="$TARGET_HOST:5555"
  fi
}

maybe_clean() {
  if [ $CLEAN_INSTALL -eq 1 ]; then
    echo "Wiping /data partition ..."
    $ADB -s $TARGET_HOST shell 'stop; for i in /data/*; do if [ "$i" != \
         "/data/lost+found" ]; then rm -r "$i"; fi; done' || return 1
  fi
}

do_shell() {
  echo "Launching command on remote host: $cmd"
  $ADB -s $TARGET_HOST shell $1 >&2
}

# adb closes connection after reboot, but some kernel does not return from
# reboot.  Work around by explicitly killing adb client after 1 second.
do_reboot() {
  mode=$1
  echo "Rebooting the device (mode=$mode)"
  $ADB -s $TARGET_HOST reboot $mode &
  local pid=$!
  sleep 1
  kill $pid 2> /dev/null
}

# Copies an update to the host, based on PUSH_METHOD configuration.
do_push_update() {
  local update=$1
  local update_dest=${CACHE_PATH}/recovery/update.zip

  # NOTE: exit code from adb is always 0 (success).
  $ADB connect $TARGET_HOST
  # keith: adb connects as root by default on Arira board (no need to do the following)
  #$ADB -s $TARGET_HOST root
  maybe_clean &&
  $ADB -s $TARGET_HOST push $update $update_dest &&
  write_command
}

write_command() {
  local recovery_command="--update_package=${CACHE_PATH}/recovery/update.zip"
  local success="OK"

  # send all commands at once for the following reasons:
  # - to catch any errors for any possible commands (adb does not forward
  # the exit codes, so catching them in the spawned shell on the remote
  # machine). This way, we can stop early if any of the commands fail.
  # - It is much faster for ssh to do everything in one shot.
  local output=$(do_shell "mkdir -p /cache/recovery &&
            echo $recovery_command > /cache/recovery/command &&
            sync && echo $success" 3>&2 2>&1 1>&3 3>&-)

  # $output ends with \r (= carriage return)
  output=${output%$'\r'}
  if [ "$output" != "$success" ]; then
      error_exit "Failed to write command"
  fi
  do_reboot recovery
}

check_ownership() {
  if [ $CHECK_OWNERSHIP -eq 0 ]; then
    echo "Skipping checking ownership"
    return
  fi

  $ADB connect $TARGET_HOST
  target_owner=`$ADB -s $TARGET_HOST shell getprop ro.build.user | tr -d '\r'`
  if [ "${target_owner}" != "${USER}" ] ; then
    error_exit "$USER isn't the owner of $TARGET_HOST (owned by $target_owner).
     If you want to update it forcefully without ownership check, use -f option."
  fi
}

do_update() {
  check_ownership
  echo "Pushing $UPDATE to $TARGET_HOST..."
  do_push_update $UPDATE
}

parse_args $@

echo "Use cache: ${CACHE_PATH}"

do_update && echo "SUCCESS"
