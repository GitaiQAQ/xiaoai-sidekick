#!/bin/sh
: "${LX06_SSH_PASSWORD:?Set LX06_SSH_PASSWORD or configure LX06_ASKPASS}"
printf '%s\n' "$LX06_SSH_PASSWORD"
