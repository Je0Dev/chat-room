#!/bin/bash
# Test /msg same-room rule + accept/decline flow
set -u
cd "$(dirname "$0")" || exit 1

FIFO0=/tmp/chat_c0.fifo
FIFO1=/tmp/chat_c1.fifo
FIFO2=/tmp/chat_c2.fifo

pkill -f 'bin/server' 2>/dev/null
rm -f "$FIFO0" "$FIFO1" "$FIFO2" server.log client0.log client1.log client2.log
mkfifo "$FIFO0" "$FIFO1" "$FIFO2"

bin/server > server.log 2>&1 &
SERVER_PID=$!
sleep 0.5

# Start clients sequentially so their IDs match FIFO order (0, 1, 2)
bin/client < "$FIFO0" > client0.log 2>&1 &
exec 3>"$FIFO0"
sleep 0.3
bin/client < "$FIFO1" > client1.log 2>&1 &
exec 4>"$FIFO1"
sleep 0.3
bin/client < "$FIFO2" > client2.log 2>&1 &
exec 5>"$FIFO2"
sleep 0.5

# Phase 1: same room -> PM queued, target accepts
echo "/msg 1 hello secret" >&3; sleep 0.4
echo "/accept" >&4; sleep 0.4

# Phase 2: same room -> PM queued, target declines
echo "/msg 1 second try" >&3; sleep 0.4
echo "/decline" >&4; sleep 0.4

# Phase 3: /accept with nothing pending
echo "/accept" >&4; sleep 0.4

# Phase 4: client 1 moves to gaming, now cross-room /msg must be blocked
echo "/join gaming" >&4; sleep 0.4
echo "/msg 1 are you there?" >&3; sleep 0.4

# Phase 5: /msg to nonexistent client
echo "/msg 99 ghost" >&3; sleep 0.4

# Phase 6: both in gaming -> PM works again (client 2 joins too)
echo "/join gaming" >&5; sleep 0.4
echo "/msg 2 gaming pm" >&4; sleep 0.4
echo "/accept" >&5; sleep 0.4

# Phase 7: pending PM is dropped when sender disconnects (client 2 leaves)
echo "/msg 2 bye message" >&4; sleep 0.4

sleep 1
exec 3>&- 4>&- 5>&-
sleep 0.7

kill -INT "$SERVER_PID" 2>/dev/null
sleep 0.7
kill "$SERVER_PID" 2>/dev/null
wait 2>/dev/null
rm -f "$FIFO0" "$FIFO1" "$FIFO2"

echo "=== TEST DONE ==="
