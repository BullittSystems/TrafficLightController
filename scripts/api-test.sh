#!/usr/bin/env bash
#
# api-test.sh: exercise the REST API of a running Traffic Light Controller.
#
# Usage: scripts/api-test.sh <device-ip-or-host>
#
# Needs only bash and curl. Changes the device's mode and settings while it runs and
# restores the original settings on exit (which also clears any override). Wi-Fi
# checks are non-destructive: a throwaway network is added and forgotten only when
# the list has room, and /api/wifi/reset is never called.
#
# Exit status: 0 if every check passed, 1 otherwise.

set -u

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <device-ip-or-host>" >&2
    exit 2
fi

BASE="http://$1"
TEST_SSID="__tlc_api_test__"
PASS=0
FAIL=0
STATUS=""
BODY=""
ORIG_SETTINGS=""

# req METHOD PATH [JSON]: sets STATUS and BODY.
req() {
    local method=$1 path=$2 data=${3-}
    local args=(-s -X "$method" --max-time 10 -w '\n%{http_code}')
    if [[ -n "$data" ]]; then
        args+=(-H 'Content-Type: application/json' -d "$data")
    fi
    local out
    out=$(curl "${args[@]}" "$BASE$path") || out=$'\n000'
    STATUS=${out##*$'\n'}
    BODY=${out%$'\n'*}
}

ok() {
    PASS=$((PASS + 1))
    printf '  ok    %s\n' "$1"
}

bad() {
    FAIL=$((FAIL + 1))
    printf '  FAIL  %s\n        status=%s body=%s\n' "$1" "$STATUS" "$BODY"
}

# check NAME CODE [SUBSTRING...]: last request returned CODE and contains each SUBSTRING.
check() {
    local name=$1 code=$2
    shift 2
    if [[ "$STATUS" != "$code" ]]; then
        bad "$name (expected $code)"
        return
    fi
    local s
    for s in "$@"; do
        if [[ "$BODY" != *"$s"* ]]; then
            bad "$name (missing $s)"
            return
        fi
    done
    ok "$name"
}

# check_absent NAME SUBSTRING: last request body does not contain SUBSTRING.
check_absent() {
    if [[ "$BODY" == *"$2"* ]]; then
        bad "$1 (unexpected $2)"
    else
        ok "$1"
    fi
}

restore() {
    if [[ -n "$ORIG_SETTINGS" ]]; then
        req POST /api/settings "$ORIG_SETTINGS"
        echo
        if [[ "$STATUS" == 200 ]]; then
            echo "Restored original settings."
        else
            echo "WARNING: could not restore original settings (status $STATUS)." >&2
        fi
    fi
}

echo "Testing $BASE"

req GET /api/settings
if [[ "$STATUS" != 200 ]]; then
    echo "Device not reachable at $BASE (status $STATUS)." >&2
    exit 1
fi
ORIG_SETTINGS=$BODY
trap restore EXIT

echo "Settings"
check "GET /api/settings" 200 '"mode":' '"chase":'
req POST /api/settings '{"blink":{"color":"green","intervalMs":400}}'
check "POST params only" 200 '"blink":{"color":"green","intervalMs":400}'
req POST /api/settings '{"mode":"disco"}'
check "unknown mode rejected" 400 '"error"'
req POST /api/settings '{"mode":"api"}'
check "api is no longer a mode" 400 '"error"'
req POST /api/settings '[1,2]'
check "non-object body rejected" 400 '"error"'

echo "Mode"
req GET /api/mode/chase
check "GET /api/mode/chase" 200 '"mode":"chase"' '"active":"chase"' '"override":null'
req POST '/api/mode/solid?color=yellow'
check "POST /api/mode/solid?color=yellow" 200 '"mode":"solid"' '"lit":"yellow"'
req GET /api/settings
check "solid color persisted" 200 '"solid":{"color":"yellow"}'
req GET '/api/mode/random?variant=multi&minMs=500&maxMs=900'
check "GET /api/mode/random?variant=multi..." 200 '"mode":"random"'
req GET /api/settings
check "random params persisted" 200 '"minMs":500' '"maxMs":900' '"variant":"multi"'
req GET /api/mode/off
check "GET /api/mode/off" 200 '"mode":"off"' '"lit":"off"'
req GET '/api/mode/chase?ttlMs=5000'
check "ttlMs rejected on mode" 400 '"error"'
req GET '/api/mode/blink?greenMs=100'
check "foreign param rejected on mode" 400 '"error"'
req GET '/api/mode/solid?color=purple'
check "bad color rejected on mode" 400 '"error"'
req GET /api/mode/solid/red
check "path color form removed" 404
req GET /api/mode/api
check "/api/mode/api removed" 404

echo "Override"
req GET /api/mode/chase
req GET '/api/override/blink?color=red'
check "GET /api/override/blink?color=red" 200 '"mode":"chase"' '"active":"blink"' \
    '"pattern":"blink"' '"color":"red"' '"intervalMs":400'
check_absent "untimed override has no ttl" '"ttlMs"'
req GET /api/settings
check "override does not persist" 200 '"mode":"chase"' '"blink":{"color":"green"'
req GET '/api/override/blink?color=yellow&intervalMs=150'
check "query params" 200 '"color":"yellow"' '"intervalMs":150'
req GET '/api/override/sequence?greenMs=800&yellowMs=300&redMs=800'
check "sequence query params" 200 '"pattern":"sequence"' '"yellowMs":300'
req POST /api/override '{"pattern":"solid","color":"green"}'
check "POST /api/override JSON" 200 '"active":"solid"' '"lit":"green"'
req POST /api/override '{"pattern":"chase","intervalMs":150}'
check "POST /api/override own params" 200 '"pattern":"chase"' '"intervalMs":150'
req POST /api/override '{"pattern":"disco"}'
check "unknown pattern rejected" 400 '"error"'
req POST /api/override '{"pattern":"solid","color":"purple"}'
check "bad color rejected" 400 '"error"'
req POST /api/override '{"pattern":"blink","greenMs":100}'
check "foreign JSON param rejected" 400 '"error"'
req POST /api/override '{"pattern":"off","ttlMs":-5}'
check "negative ttlMs rejected" 400 '"error"'
req GET '/api/override/chase?ttlMs=soon'
check "bad ttlMs query rejected" 400 '"error"'
req GET '/api/override/chase?intervalMs=-5'
check "negative query duration rejected" 400 '"error"'
req GET '/api/override/solid?pattern=blink'
check "pattern in query rejected" 400 '"error"'
req GET '/api/override/solid?bogus=1'
check "unknown query param rejected" 400 '"error"'
req GET /api/override/blink/red
check "path color form removed" 404

for p in off solid blink sequence random chase; do
    req POST "/api/override/$p"
    check "POST /api/override/$p" 200 '"mode":"chase"' "\"pattern\":\"$p\""
done

req GET /api/override/clear
check "GET /api/override/clear" 200 '"active":"chase"' '"override":null'
req GET '/api/override/solid?color=red'
req DELETE /api/override
check "DELETE /api/override" 200 '"override":null'

req GET '/api/override/solid?color=red'
req GET /api/mode/sequence
check "setting mode clears override" 200 '"mode":"sequence"' '"override":null'
req GET '/api/override/solid?color=red'
req POST /api/settings '{"chase":{"intervalMs":250}}'
req GET /api/state
check "settings without mode keep override" 200 '"active":"solid"'
req POST /api/settings '{"mode":"chase"}'
req GET /api/state
check "settings with mode clear override" 200 '"mode":"chase"' '"override":null'

echo "Override expiry (waits ~4 s)"
req GET '/api/override/solid?color=green&ttlMs=1000'
check "ttl via query" 200 '"ttlMs":1000' '"remainingMs":'
sleep 2
req GET /api/state
check "query ttl expired" 200 '"active":"chase"' '"override":null'
req POST /api/override '{"pattern":"off","ttlMs":1000}'
check "ttl via JSON" 200 '"ttlMs":1000'
sleep 2
req GET /api/state
check "JSON ttl expired" 200 '"override":null'
req GET '/api/override/off?ttlMs=5'
check "ttl clamped to 1 s minimum" 200 '"ttlMs":1000'
req DELETE /api/override

echo "Removed routes"
req GET /api/mode/off # dark baseline, so a stray red would be visible
req GET /api/state/solid/red
check_absent "/api/state/solid/red no longer sets state" '"lit":"red"'
req POST /api/state '{"state":"solid","color":"red"}'
check_absent "POST /api/state no longer sets state" '"lit":"red"'

echo "State"
req GET /api/state
check "GET /api/state" 200 '"mode":' '"active":' '"lit":' '"override":'

echo "Wi-Fi"
req GET /api/wifi
check "GET /api/wifi" 200 '"networks":' '"max":5'
check_absent "no passwords in list" '"password"'
wifi_list=$BODY
req POST /api/wifi/add '{"ssid":""}'
check "empty ssid rejected" 400 '"error"'
req POST /api/wifi/add '{"ssid":5}'
check "non-string ssid rejected" 400 '"error"'
req POST /api/wifi/forget "{\"ssid\":\"$TEST_SSID-missing\"}"
check "forget unknown network" 404 '"error"'
if [[ "$wifi_list" == *'"session":null'* ]]; then
    req POST /api/wifi/keep '{}'
    check "keep without session network" 404 '"error"'
fi
saved=$(grep -o '"ssid"' <<<"$wifi_list" | wc -l | tr -d ' ')
if [[ "$saved" -lt 5 ]]; then
    req POST /api/wifi/add "{\"ssid\":\"$TEST_SSID\",\"password\":\"test-pass-123\"}"
    check "add network" 200 "\"ssid\":\"$TEST_SSID\""
    check_absent "added password not echoed" 'test-pass-123'
    req POST /api/wifi/forget "{\"ssid\":\"$TEST_SSID\"}"
    check "forget network" 200
    check_absent "network gone" "$TEST_SSID"
else
    req POST /api/wifi/add "{\"ssid\":\"$TEST_SSID\",\"password\":\"test-pass-123\"}"
    check "add to full list" 409 '"error"'
fi

echo
echo "$PASS passed, $FAIL failed"
[[ $FAIL -eq 0 ]]
