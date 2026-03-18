# Remote Microphone Support

This fork adds a working host-side remote microphone path for Apollo, focused on Windows hosts and Steam Streaming Microphone integration.

## Overview

The microphone path is:

1. A compatible Moonlight/Artemis client captures local microphone audio.
2. The client sends encrypted or unencrypted microphone packets to Apollo on the dedicated microphone stream.
3. Apollo receives the packets, decrypts them when needed, and decodes the Opus frames on the host.
4. Apollo renders the decoded PCM into the Steam playback endpoint `Speakers (Steam Streaming Microphone)`.
5. Host applications consume that audio from the paired capture endpoint `Microphone (Steam Streaming Microphone)`.

This keeps the host-side application flow simple: Apollo writes into Steam Streaming Microphone, and games, chat apps, or capture tools use `Microphone (Steam Streaming Microphone)` as the microphone.

## What Changed

The working implementation in this fork includes:

- Dedicated microphone session handling in the stream path, including packet receive, optional decryption, and per-session lifecycle management.
- Windows microphone backend initialization and teardown that stays alive for the full remote microphone session.
- A Steam-backed Windows microphone path that auto-detects the Steam microphone render/capture pair, normalizes only that pair to `2ch, 32-bit, 48000 Hz` when microphone streaming starts, decodes Opus microphone frames as mono float `48 kHz`, and writes them into the Steam microphone render buffer using a `float32` shared-mode render client.
- Host-side recovery for recoverable WASAPI failures such as device invalidation or audio service restarts.
- A Remote Microphone Debug panel in the web UI that shows packet arrival, decode status, render status, signal detection, counters, and recent mic events.

## Key Files

- `src/stream.cpp`: microphone socket handling, session startup/shutdown, and packet routing.
- `src/audio.cpp`: shared microphone debug state and persistent audio context ownership for the redirect device.
- `src/platform/windows/audio.cpp`: Windows microphone backend selection and redirect device ownership.
- `src/platform/windows/apollo_vmic.cpp`: Steam Streaming Microphone backend wrapper.
- `src/platform/windows/mic_write.cpp`: device discovery, WASAPI initialization, Opus decode, and Steam Streaming Microphone rendering.
- `src_assets/common/assets/web/configs/tabs/AudioVideo.vue`: Remote Microphone Debug UI.

## Windows Requirements

- Install the Steam audio drivers on the host.
- Ensure the playback endpoint `Speakers (Steam Streaming Microphone)` exists and is enabled.
- In host applications, select `Microphone (Steam Streaming Microphone)` as the microphone/recording source.
- Enable `stream_mic` in Apollo.
- Use a client build that supports Apollo microphone redirection.

## Configuration Notes

- `stream_mic` enables the host microphone redirect path.
- `mic_backend` defaults to `steam_streaming_microphone` on Windows in this fork.
- On Windows, Apollo auto-detects the Steam Streaming Microphone pair and normalizes only those microphone endpoints to `2ch, 32-bit, 48000 Hz` automatically instead of requiring a manual device-properties change.
- `mic_device` is mainly relevant on non-Windows platforms. The Windows path currently targets Steam Streaming Microphone automatically.

## Debugging

The Audio/Video page on Windows exposes a Remote Microphone Debug panel that shows:

- whether the client is sending packets
- whether Apollo is decoding microphone frames
- whether Apollo is rendering into Steam Streaming Microphone
- whether non-silent input is being detected
- which endpoint mix format Apollo discovered
- which render and capture device formats are currently active
- which render format Apollo actually initialized
- whether the recommended Steam microphone format is active or had to be enforced
- how mono input is mapped to the host channels
- the most recent mic errors and recent mic events

This view is intended to quickly separate client capture problems from host decode/render problems.
