#!/bin/sh
set -eu

package_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$package_directory"
exec "$package_directory/LuastgRetro" "$@"
