# SEP4 IoT — Greenhouse Sensor Node

Firmware for a greenhouse monitoring node built on an Arduino Mega 2560. It samples
temperature, humidity and light, sends readings to a cloud backend every eleven seconds,
and applies actuator commands the server sends back in reply.

This repository is the embedded half of a 4th-semester project. I was tech lead and product owner of the seven-person team and wrote this firmware; 
the backend and web dashboard live in separate repositories.

## What the node does

On boot it initialises the WiFi module, the DHT11, the light sensor and the LEDs,
starts the RTOS, and sets the clock from NTP so its timestamps are meaningful.

Then, in a loop: read the sensors, pack them into a 16-byte frame, checksum it, encrypt
it, and send it to the backend. The reply is decrypted, validated, and if it carries a
command, applied to the LED and the servo.

## The wire protocol

Each message is a fixed 16-byte frame:

| Bytes | Contents |
| --- | --- |
| 0–1 | Board ID and direction — `00 01` from the node, `80 01` from the server |
| 2–5 | Timestamp, big-endian |
| 6–9 | Humidity and temperature, integer and decimal parts |
| 10–11 | Light reading |
| 12–13 | Payload, used for commands in the server's direction |
| 14–15 | CRC16 over the preceding 14 bytes |

The CRC is computed first, then the whole frame is encrypted with AES-128, hex-encoded,
and sent as an HTTP GET. Replies arrive the same way and are unwrapped in reverse.

A reply is only acted on if it passes four checks: the CRC matches, the direction byte
marks it as server-to-node, the board ID is ours, and the server's timestamp is no more
than ten seconds behind our own. That last check is what stops a captured frame from
being replayed later — without it, AES alone would happily decrypt a recorded message
and the node would obey it.

## Structure

```
src/main.c              Boot, sampling loop, framing, command handling
lib/drivers/            dht11, light, wifi, uart, leds, servo
lib/components/convert  Hex encoding and CRC16
test/                   Unity tests: sensors, wifi, conversion
platformio.ini          Board, RTOS configuration, dependencies
```

The RTOS is [avrtos](https://github.com/lucasdietrich/AVRTOS), configured for
cooperative scheduling with a 1 ms slice, kernel timers and the time API enabled — the
build flags in `platformio.ini` are the whole configuration.

## Tests

Unit tests run under Unity, covering the sensor driver, the WiFi driver and the hex and
CRC conversion helpers.

```bash
pio test
```

They are also wired into GitHub Actions, but not in the usual way. The workflow uses
PlatformIO Remote to run the suite **on the physical board** over a remote agent, rather
than on a simulator or the host:

```yaml
run: pio remote --agent hock test -r -v
```

This only works while that agent is online, so the workflow is a record of how the tests
were run during development rather than something that passes on demand today.

## Known limitations

The AES key is a compile-time constant in `src/main.c` and is therefore in this
repository's history. For a coursework project against a demo backend that was an
accepted trade-off, but it is not how a key should be handled — it belongs in a
provisioning step, and shipping the same key to every board means one extracted device
compromises all of them.

AES-128 here is single-block ECB with no per-message nonce, so identical payloads
produce identical ciphertext. The timestamp in every frame makes that mostly moot in
practice, but it is a weakness worth naming rather than glossing over.

## Status

Completed in 2024 as coursework. The Azure backend it talked to is no longer running.
