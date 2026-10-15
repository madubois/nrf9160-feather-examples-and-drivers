#!/usr/bin/env bash
set -euo pipefail

server="${OTA_SERVER:-root@plongee.duckdns.org}"
firmware="${1:-$(cd "$(dirname "$0")/../../../../build/kraken_tracker/zephyr" && pwd)/app_update.bin}"
config="$(dirname "$firmware")/.config"

if [[ ! -f "$firmware" ]]; then
    echo "Firmware image not found: $firmware" >&2
    exit 1
fi

if [[ ! -f "$config" ]]; then
    echo "Build configuration not found: $config" >&2
    exit 1
fi

version="$(sed -n 's/^CONFIG_MCUBOOT_IMAGE_VERSION="\(.*\)"$/\1/p' "$config")"
if [[ -z "$version" ]]; then
    echo "MCUboot image version not found: $config" >&2
    exit 1
fi

scp "$firmware" "$server:/root/containers/webserver/app_update.bin.new"
ssh "$server" "
    set -e
    cd /root/containers/webserver
    cat app_update.bin.new > app_update.bin
    rm app_update.bin.new
    sha256sum app_update.bin
    printf '{\"available\": true, \"version\": \"%s\"}\\n' '$version' > ota_state.json
"

echo "OTA release $version published. Devices already on this version will skip it."