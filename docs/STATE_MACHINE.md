# Access Control State Machine

## Design constraints

1. Outer and inner doors must never be physically open at the same time.
2. The inner door is the security boundary protecting food access.
3. Any ambiguity in identity or occupancy keeps the inner door closed.
4. Any obstruction or actuator fault prioritizes pet safety over access completion.
5. Physical door state comes from limit switches, never from commanded actuator position alone.
6. RFID authorization is latched as a short-lived passage token; continuous RFID visibility is not required after the pet has entered the chamber.

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

Abort before food-side access
  -> RECOVERY_OUTER_OPENING
  -> IDLE

Any operational state -> SAFE_FAULT
```

## Key state behavior

### BOOT

- initialize GPIO and buses;
- load persistent configuration;
- restore feeding ledger;
- initialize RTC and sensor drivers;
- request door homing.

### HOMING

- establish physical positions for outer and inner doors using limit switches;
- verify both doors can reach a known safe closed state;
- if homing cannot complete, enter `SAFE_FAULT`.

### IDLE

- both doors confirmed closed;
- wait for a fresh known RFID identity with an available feeding session.

### AUTH_PENDING

Accept only a known pet identity that currently has feeding permission.

Reject if:

- no/faded RFID before the passage begins;
- unknown tag;
- pet not allowed at this time;
- no meal/session available;
- authorization timeout.

When accepted, latch the authorized `PetId` for this passage attempt.

This token deliberately survives temporary RFID loss after the animal moves into the chamber. RFID is an identity signal, not proof that only one animal is physically present.

### OUTER_OPENING

Preconditions:

- inner door physically confirmed closed;
- outer door physically confirmed closed;
- outer threshold safe.

Command outer door open and wait for open-limit confirmation.

### ENTRY_WAIT

Wait for the authorized pet to enter the authentication chamber.

Evidence is evaluated by the presence-fusion layer.

Proceed toward closing only when:

```text
occupancy == exactly one plausible pet
AND chamber mass matches authorized pet
AND outer threshold is clear
```

Abort to outer-side recovery when:

- multiple/tailgating occupancy is suspected;
- occupancy is ambiguous;
- required presence sensors fail;
- entry times out.

### OUTER_CLOSING

Close only after the threshold is clear.

If the outer threshold becomes blocked while closing:

- invalidate the passage authorization;
- reopen toward the public/outside side;
- never continue directly to inner-door access.

Once the closed limit is confirmed, enter `CHAMBER_VERIFY`.

### CHAMBER_VERIFY

This is the security checkpoint.

Required conditions before inner door may open:

```text
authorized passage token exists
AND feeding session remains available
AND outer door confirmed closed
AND inner door confirmed closed
AND outer threshold clear
AND inner threshold clear
AND chamber occupancy estimate == exactly one pet
AND chamber mass plausible for authorized pet
AND all required occupancy sensors healthy/fresh
```

V1 is fail-closed rather than majority-vote based: any critical disagreement aborts the attempt.

### INNER_OPENING

Open only after `CHAMBER_VERIFY` passes.

The outer door must remain physically confirmed closed during this state.

### FEED_ENTRY_WAIT

Wait until the animal moves from the authentication chamber into the feeding chamber.

The first implementation treats an empty authentication chamber plus a clear inner threshold as evidence that the crossing has completed.

### INNER_CLOSING

Close only when the inner threshold is clear.

If the inner threshold becomes blocked while closing:

- reopen the inner door;
- return to `FEED_ENTRY_WAIT`;
- do not continue forcing the door closed.

Once closed-limit confirmation is received, enter `FEEDING`.

### FEEDING

Food is available only inside the closed feeding chamber for the authenticated session.

Planned responsibilities:

- trigger/allow the correct hopper channel;
- track bowl weight before/during/after feeding;
- enforce meal/session limits;
- decide when the meal is complete.

The current initial firmware core intentionally stops here; meal-completion and reverse exit logic are the next implementation stage.

### EXIT_SEQUENCE

Reserved for the reverse two-door passage sequence.

It must preserve the same interlock invariant:

```text
NOT (outer_open AND inner_open)
```

Food access must be closed off before the external side becomes available.

### RECOVERY_OUTER_OPENING

Used when a passage attempt is rejected before the pet reaches the feeding chamber.

Rules:

- inner door must be physically confirmed closed;
- authorization token is invalidated;
- outer door is opened so all animals can leave the authentication chamber;
- wait for chamber-empty evidence;
- close outer door only when threshold is clear;
- return to `IDLE` after outer closed confirmation.

This state is particularly important for two-pet/tailgating cases: the system does not try to guess which animal should remain inside.

## SAFE_FAULT

Typical triggers:

- both doors physically open simultaneously;
- failed homing;
- door movement timeout;
- contradictory open/closed limit switches;
- unrecoverable actuator fault;
- violation of a hard interlock.

Sensor failures during chamber verification normally abort the passage toward outer-side recovery where this can be done safely. Mechanical contradictions and interlock violations latch `SAFE_FAULT`.

Priorities:

1. avoid trapping/crushing a pet;
2. keep food inaccessible to unauthorized pets;
3. emit local/remote diagnostics.

Automatic recovery from `SAFE_FAULT` is intentionally not part of the first core implementation.

## Anti-tailgating cases

### Case A: authorized pet enters alone

Expected: close outer door, verify chamber, then permit inner door.

### Case B: second pet follows into chamber

Possible evidence:

- ToF detects >=2 clusters;
- chamber mass exceeds the authorized pet's plausible range;
- multiple tag evidence in a future multi-reader configuration;
- threshold remains occupied.

Expected: inner door remains closed; authorization is invalidated; outer-side recovery is used.

### Case C: second pet leaves head/body in doorway

Expected: outer door must not continue closing; reopen and cancel the attempt.

### Case D: two animals overlap into one ToF blob

Expected: weight and other evidence must still agree. If evidence cannot prove exactly one plausible authorized animal, classify as ambiguous and fail closed.

### Case E: RFID disappears after authorized pet enters

Expected: do not immediately fail solely because the tag is temporarily unreadable. Continue using the latched passage identity, but require occupancy/weight/door evidence before opening the inner door.

## Persistence requirements

Persist enough state to prevent duplicate feeding after reboot:

- meal/session ID;
- pet ID;
- scheduled meal window;
- amount already dispensed;
- session completion state.

Never persist a transient door command as if it were a confirmed physical state.
