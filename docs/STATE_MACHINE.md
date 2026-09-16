# Access Control State Machine

## Design constraints

1. Outer and inner doors must never be open at the same time.
2. The inner door is the security boundary protecting food access.
3. Any ambiguity in identity or occupancy keeps the inner door closed.
4. Any obstruction or actuator fault prioritizes pet safety over access completion.
5. Physical door state comes from limit switches, never from commanded actuator position alone.

## Top-level states

```text
BOOT
  -> HOMING
  -> IDLE
  -> AUTH_PENDING
  -> OUTER_OPENING
  -> ENTRY_WAIT
  -> OUTER_CLOSING
  -> CHAMBER_VERIFY
  -> INNER_OPENING
  -> FEED_ENTRY_WAIT
  -> INNER_CLOSING
  -> FEEDING
  -> EXIT_SEQUENCE
  -> IDLE

Any state -> SAFE_FAULT
```

## Key state behavior

### BOOT

- initialize GPIO and buses
- load persistent configuration
- restore feeding ledger
- initialize RTC and sensor drivers

### HOMING

- establish physical positions for outer and inner doors using limit switches
- verify both doors can reach a known safe state
- if homing cannot complete, enter SAFE_FAULT

### IDLE

- both doors confirmed closed
- wait for approach/presence signal
- poll/read RFID

### AUTH_PENDING

Accept only a known pet identity that currently has feeding permission.

Reject if:

- no RFID
- unknown tag
- pet not allowed at this time
- stale/duplicate authorization token

### OUTER_OPENING

Preconditions:

- inner door confirmed closed
- outer threshold is safe

Open outer door and verify open limit.

### ENTRY_WAIT

Wait for the authorized pet to enter the authentication chamber.

Do not close the outer door while the outer threshold is obstructed.

Timeout behavior:

- cancel authorization
- return to a safe open/closed recovery sequence

### OUTER_CLOSING

Close only after the threshold has been continuously clear for a debounce interval.

If obstruction is detected while moving:

- stop
- reverse/open
- invalidate the current passage attempt

### CHAMBER_VERIFY

Required conditions before inner door may open:

```text
authorized RFID identity valid
AND outer door confirmed closed
AND inner door confirmed closed
AND outer threshold clear
AND inner threshold clear
AND chamber occupancy estimate == one pet
AND chamber mass is plausible for authorized pet
AND no sensor fault
```

Recommended V1 fusion rule is fail-closed rather than majority voting: any critical disagreement aborts the attempt.

### INNER_OPENING

Open only after CHAMBER_VERIFY passes.

### FEED_ENTRY_WAIT

Verify pet transitions from authentication chamber to feeding chamber.

### INNER_CLOSING

Close only when the inner threshold is clear.

Obstruction handling is the same as for the outer door.

### FEEDING

- food is available only for the authenticated pet/session
- track bowl weight before/during/after feeding
- enforce meal/session limits

### EXIT_SEQUENCE

Reverse passage logic while preserving the same two-door interlock.

Food access must be blocked again before the external side becomes available.

## SAFE_FAULT

Typical triggers:

- both door-open sensors active simultaneously
- failed homing
- door movement timeout
- contradictory limit switches
- load-cell failure when required for verification
- ToF unavailable when required for occupancy verification
- unrecoverable actuator fault

Safe response depends on failure location, but priorities are:

1. avoid trapping/crushing a pet
2. keep food inaccessible to unauthorized pets
3. emit local/remote diagnostics

## Anti-tailgating cases

### Case A: authorized pet enters alone

Expected: proceed to inner door.

### Case B: second pet follows into chamber

Signals may include:

- ToF occupancy inconsistent with one animal
- chamber mass outside authorized pet range
- multiple RFID tags
- threshold remains occupied

Expected: inner door remains closed; abort/recover.

### Case C: second pet leaves head/body in doorway

Expected: outer door must not close; timeout and cancel attempt.

### Case D: second pet enters but sensor evidence is ambiguous

Expected: fail closed; do not open inner door.

## Persistence requirements

Persist enough state to prevent duplicate feeding after reboot:

- meal/session ID
- pet ID
- scheduled meal window
- amount already dispensed
- session completion state

Never persist a transient door command as if it were a confirmed physical state.
