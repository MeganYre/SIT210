#!/usr/bin/env python3
"""
SIT210 Task 8.1HD — Voice-activated lighting over Bluetooth
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import re
import sys
import traceback
from typing import List, Optional, Tuple, TYPE_CHECKING

import speech_recognition as sr

if TYPE_CHECKING:
    from bleak import BleakClient
    from vosk import Model

SERVICE_UUID = "12345678-1234-1234-1234-123456789001"
COMMAND_CHAR_UUID = "12345678-1234-1234-1234-123456789002"
STATUS_CHAR_UUID = "12345678-1234-1234-1234-123456789003"

CMD_ALL_OFF = 0x00
CMD_BATH_ON, CMD_BATH_OFF = 0x01, 0x02
CMD_HALL_ON, CMD_HALL_OFF = 0x03, 0x04
CMD_FAN_ON, CMD_FAN_OFF = 0x05, 0x06
CMD_POLL = 0xFF

MASK_BATH = 1
MASK_HALL = 2
MASK_FAN = 4

VOSK_SAMPLE_HZ = 16000

USB_MIC_NAME_KEYWORDS = (
    "usb",
    "usb audio",
    "headset",
    "webcam",
    "snowball",
    "yeti",
    "fifine",
    "logitech",
    "headphone",
)

_DEFAULT_VOSK = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "vosk-model-small-en-us-0.15",
)


def default_vosk_model_path() -> str:
    return _DEFAULT_VOSK


def list_microphone_names() -> list:
    return sr.Microphone.list_microphone_names()


def is_likely_usb_mic(name: str) -> bool:
    if not name:
        return False
    lower = name.lower()
    return any(kw in lower for kw in USB_MIC_NAME_KEYWORDS)


def find_usb_preferred_index() -> Optional[int]:
    for i, name in enumerate(list_microphone_names()):
        if is_likely_usb_mic(name or ""):
            return i
    return None


def print_microphone_list() -> None:
    names = list_microphone_names()
    print("Input devices (index — name). [USB?] = likely USB mic.")
    for i, name in enumerate(names):
        label = name if name else "(no name)"
        tag = "  [USB?]" if is_likely_usb_mic(name or "") else ""
        print("  %d  %s%s" % (i, label, tag))


def open_microphone(
    device_index: Optional[int], prefer_usb: bool
) -> Tuple[sr.Microphone, str]:
    names = list_microphone_names()
    if device_index is not None:
        if device_index < 0 or device_index >= len(names):
            raise OSError("Invalid --device %d (only %d inputs)." % (device_index, len(names)))
        label = names[device_index] if names else ""
        # Do NOT hard-code sample_rate here.
        # For many USB mics, PyAudio can't open a specific sample_rate and will fail (ALSA errors).
        # We resample later in recognize_vosk() via audio.get_raw_data(convert_rate=VOSK_SAMPLE_HZ,...).
        return sr.Microphone(device_index=device_index), (
            "index %d — %s" % (device_index, label or "(no name)")
        )
    if prefer_usb:
        usb_idx = find_usb_preferred_index()
        if usb_idx is not None:
            label = names[usb_idx] if usb_idx < len(names) else ""
            return sr.Microphone(device_index=usb_idx), (
                "USB auto — index %d — %s" % (usb_idx, label or "(no name)")
            )
        return sr.Microphone(), (
            "system default (no USB-like name; use --list-mics --device N)"
        )
    return sr.Microphone(), "system default (--mic-system)"


def configure_recognizer(recognizer: sr.Recognizer) -> None:
    recognizer.dynamic_energy_threshold = True
    recognizer.pause_threshold = 0.95


def recognize_vosk(model: "Model", audio: sr.AudioData) -> Optional[str]:
    from vosk import KaldiRecognizer

    rec = KaldiRecognizer(model, VOSK_SAMPLE_HZ)
    raw = audio.get_raw_data(convert_rate=VOSK_SAMPLE_HZ, convert_width=2)
    step = 4000
    for i in range(0, len(raw), step):
        rec.AcceptWaveform(raw[i : i + step])
    try:
        payload = json.loads(rec.FinalResult())
    except json.JSONDecodeError:
        return None
    text = (payload.get("text") or "").strip()
    return text if text else None


def text_to_commands(text: str) -> Optional[List[int]]:
    """Map utterance to one or more BLE command bytes (order preserved)."""
    t = text.lower()
    if re.search(r"\b(all|everything)\s+(off|out)\b", t) or "all off" in t:
        return [CMD_ALL_OFF]
    if re.search(r"\blights on\b", t) or re.search(r"\bboth\s+lights\s+on\b", t):
        return [CMD_BATH_ON, CMD_HALL_ON]
    if re.search(r"\blights off\b", t) or re.search(r"\bboth\s+lights\s+off\b", t):
        return [CMD_BATH_OFF, CMD_HALL_OFF]

    if "bathroom" in t or "bath room" in t:
        if re.search(r"\b(on|one|enable|start)\b", t):
            return [CMD_BATH_ON]
        if re.search(r"\b(off|of|disable|stop|out)\b", t):
            return [CMD_BATH_OFF]

    if "hallway" in t or "hall" in t:
        if re.search(r"\b(on|one|enable|start)\b", t):
            return [CMD_HALL_ON]
        if re.search(r"\b(off|of|disable|stop|out)\b", t):
            return [CMD_HALL_OFF]

    if "fan" in t or "exhaust" in t:
        if re.search(r"\b(on|one|enable|start)\b", t):
            return [CMD_FAN_ON]
        if re.search(r"\b(off|of|disable|stop|out)\b", t):
            return [CMD_FAN_OFF]

    return None


def decode_status(mask: int) -> Tuple[bool, bool, bool]:
    return (bool(mask & MASK_BATH), bool(mask & MASK_HALL), bool(mask & MASK_FAN))


def listen_once(
    recognizer: sr.Recognizer,
    mic: sr.Microphone,
    vosk_model: "Model",
    ambient_sec: float,
) -> Optional[str]:
    print("Listening…")
    try:
        with mic as source:
            recognizer.adjust_for_ambient_noise(source, duration=ambient_sec)
            audio = recognizer.listen(source, timeout=10, phrase_time_limit=7)
        text = recognize_vosk(vosk_model, audio)
        if text:
            print("Heard:", text)
            return text
        print("Could not understand audio.")
        return None
    except Exception as e:
        # Most common causes:
        # - ALSA can't open the device
        # - USB mic disconnected
        # - permissions
        print("Audio capture error:", repr(e), file=sys.stderr)
        try:
            import traceback as _tb
            print(_tb.format_exc(), file=sys.stderr)
        except Exception:
            pass
        return None


def capture_audio_once(
    recognizer: sr.Recognizer,
    mic: sr.Microphone,
    ambient_sec: float,
    *,
    phrase_time_limit: float = 5.0,
) -> Optional[int]:
    """
    Audio-only capture test (no Vosk).

    Returns the number of bytes captured from the mic, or None on error.
    """
    print("Capturing audio…")
    try:
        with mic as source:
            recognizer.adjust_for_ambient_noise(source, duration=ambient_sec)
            # Use fixed-duration recording (no speech-detection timeout),
            # so this diagnostic always finishes.
            audio = recognizer.record(source, duration=phrase_time_limit)
        # SpeechRecognition AudioData stores raw PCM16 bytes in frame_data.
        nbytes = len(audio.frame_data)
        print("Captured bytes:", nbytes)
        return nbytes
    except Exception as e:
        print("Audio capture error:", repr(e), file=sys.stderr)
        try:
            import traceback as _tb
            print(_tb.format_exc(), file=sys.stderr)
        except Exception:
            pass
        return None


async def scan_ble_devices(timeout: float) -> None:
    from bleak import BleakScanner

    print("Scanning BLE devices for %.1fs…" % timeout)
    devices = await BleakScanner.discover(timeout=timeout)
    if not devices:
        print("No devices found.")
        return
    for d in devices:
        print("  %s  %s" % (d.address, d.name or "(no name)"))


async def resolve_ble_address(name_substr: str, scan_timeout: float) -> Optional[str]:
    from bleak import BleakScanner

    devices = await BleakScanner.discover(timeout=scan_timeout)
    needle = name_substr.lower()
    for d in devices:
        if d.name and needle in d.name.lower():
            return str(d.address)
    return None


async def read_status_bytes(client: BleakClient) -> Optional[Tuple[int, int]]:
    try:
        data = await client.read_gatt_char(STATUS_CHAR_UUID)
    except Exception as e:
        print("Could not read status:", e)
        return None
    if len(data) < 2:
        print("Short status read:", data.hex())
        return None
    return data[0], data[1]


async def read_status(client: BleakClient) -> None:
    pair = await read_status_bytes(client)
    if pair is None:
        return
    mask, light = pair
    b, h, f = decode_status(mask)
    print(
        "Arduino: bathroom=%s hallway=%s fan=%s  light=%d"
        % ("ON" if b else "OFF", "ON" if h else "OFF", "ON" if f else "OFF", light)
    )


def light_allows_on(light_level: int, light_on_max: int) -> bool:
    return light_level <= light_on_max


async def send_command(client: BleakClient, cmd: int) -> None:
    # Use "write with response" to ensure ArduinoBLE sets commandChar.written().
    await client.write_gatt_char(
        COMMAND_CHAR_UUID, bytes([cmd & 0xFF]), response=True
    )


async def run_voice_loop(
    address: str,
    recognizer: sr.Recognizer,
    mic: sr.Microphone,
    vosk_model: "Model",
    ambient_sec: float,
    light_on_max: int,
    skip_light_gate: bool,
) -> None:
    from bleak import BleakClient

    print("Connecting to BLE", address, "…")
    try:
        async with BleakClient(address) as client:
            if not client.is_connected:
                print("Failed to connect.", file=sys.stderr)
                return
            print("Connected. Service UUID:", SERVICE_UUID)
            await read_status(client)

            # Keep the audio stream open while listening to reduce ALSA underrun/xrun noise.
            with mic as source:
                while True:
                    print("Listening…")
                    # Re-calibrate before each short capture window to avoid
                    # energy-threshold drift (helps when recognition works in mic-test
                    # but returns empty during continuous loop).
                    recognizer.adjust_for_ambient_noise(
                        source, duration=ambient_sec
                    )
                    audio = await asyncio.to_thread(
                        recognizer.listen, source, 10, 7
                    )
                    text = recognize_vosk(vosk_model, audio)
                    if not text:
                        print("No text recognized.")
                        continue

                    cmds = text_to_commands(text)
                    if not cmds:
                        print("No matching command.")
                        continue

                    try:
                        for cmd in cmds:
                            if (
                                not skip_light_gate
                                and cmd in (CMD_BATH_ON, CMD_HALL_ON)
                            ):
                                st = await read_status_bytes(client)
                                if st is None:
                                    break
                                _, light = st
                                if not light_allows_on(light, light_on_max):
                                    print(
                                        "Too bright to turn lights on (light=%d, max=%d)."
                                        % (light, light_on_max)
                                    )
                                    break
                            await send_command(client, cmd)
                            print("Sent:", hex(cmd))
                        await read_status(client)
                    except Exception as e:
                        print("BLE error:", repr(e), file=sys.stderr)
                        return
    except Exception as e:
        # Include traceback for the BLE connect failure path
        print("BLE connect error:", repr(e), file=sys.stderr)
        print(traceback.format_exc(), file=sys.stderr)
        return


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="SIT210 8.1HD: Pi voice (Vosk) → BLE → Nano 33 IoT lights + fan"
    )
    p.add_argument("--address", "-a", default=None, metavar="MAC")
    p.add_argument(
        "--name",
        "-n",
        default="Task8.1AudioProcessing",
        metavar="SUBSTR",
        help="Match advertised BLE name (default: Task8.1AudioProcessing)",
    )
    p.add_argument("--scan", action="store_true", help="List BLE devices and exit")
    p.add_argument("--scan-time", type=float, default=8.0)
    p.add_argument("--vosk-model", default=default_vosk_model_path(), metavar="DIR")
    p.add_argument("--device", type=int, default=None, metavar="N")
    p.add_argument("--mic-system", action="store_true")
    p.add_argument("--list-mics", action="store_true")
    p.add_argument("--ambient", type=float, default=0.55)
    p.add_argument(
        "--light-on-max",
        type=int,
        default=255,
        metavar="0-255",
        help="Bathroom/hallway ON only if Arduino light reading <= this (default 255)",
    )
    p.add_argument(
        "--no-light-gate",
        action="store_true",
        help="Ignore brightness check (testing)",
    )
    p.add_argument(
        "--mic-test",
        action="store_true",
        help="Record once from the selected mic and run Vosk once; prints recognized text or 'No text'.",
    )
    p.add_argument(
        "--mic-capture-once",
        action="store_true",
        help="Capture audio once from the selected mic (no Vosk). Prints captured byte count.",
    )
    p.add_argument(
        "--capture-sec",
        type=float,
        default=5.0,
        help="Seconds to capture for --mic-capture-once (default 5.0).",
    )
    p.add_argument(
        "--ble-status-once",
        action="store_true",
        help="Connect once (by --address or --name), read Arduino status, then exit.",
    )
    return p


def main() -> None:
    args = build_parser().parse_args()

    if args.list_mics:
        print_microphone_list()
        return

    try:
        import bleak  # noqa: F401
    except ImportError:
        print("Install bleak: pip install bleak (in your venv)", file=sys.stderr)
        sys.exit(1)

    if args.scan:
        asyncio.run(scan_ble_devices(args.scan_time))
        return

    if args.light_on_max < 0 or args.light_on_max > 255:
        print("--light-on-max must be 0..255", file=sys.stderr)
        sys.exit(1)

    # Diagnostic mode: BLE status read-once (NO Vosk, NO mic)
    if args.ble_status_once:
        address = args.address
        if not address:
            print("Resolving device name %r…" % args.name)
            address = asyncio.run(resolve_ble_address(args.name, args.scan_time))
            if not address:
                print(
                    "No device found. Use --scan and --address MAC.",
                    file=sys.stderr,
                )
                sys.exit(1)
            print("Using address:", address)

        async def _ble_once() -> None:
            from bleak import BleakClient

            print("BLE status-once: connecting to", address, "…")
            async with BleakClient(address) as client:
                if not client.is_connected:
                    print("BLE status-once: failed to connect.", file=sys.stderr)
                    return
                raw = await read_status_bytes(client)
                if raw is None:
                    print(
                        "BLE status-once: status read failed.",
                        file=sys.stderr,
                    )
                    return
                mask, light = raw
                b, h, f = decode_status(mask)
                print(
                    "BLE status-once:",
                    "bathroom=%s hallway=%s fan=%s light=%d"
                    % ("ON" if b else "OFF", "ON" if h else "OFF", "ON" if f else "OFF", light),
                )

        asyncio.run(_ble_once())
        return

    # Diagnostic mode: mic capture-only (NO Vosk, NO BLE)
    if args.mic_capture_once:
        prefer_usb = not args.mic_system
        try:
            mic, mic_desc = open_microphone(args.device, prefer_usb=prefer_usb)
        except OSError as e:
            print(e, file=sys.stderr)
            sys.exit(1)
        print("Microphone:", mic_desc)

        recognizer = sr.Recognizer()
        configure_recognizer(recognizer)
        capture_audio_once(
            recognizer,
            mic,
            args.ambient,
            phrase_time_limit=args.capture_sec,
        )
        return

    model_dir = os.path.abspath(os.path.expanduser(args.vosk_model))
    if not os.path.isdir(model_dir):
        print("Missing Vosk model directory:\n  %s" % model_dir, file=sys.stderr)
        sys.exit(1)

    try:
        from vosk import Model
    except ImportError:
        print("Install vosk: pip install vosk (in your venv)", file=sys.stderr)
        sys.exit(1)

    vosk_model = Model(model_dir)
    prefer_usb = not args.mic_system
    try:
        mic, mic_desc = open_microphone(args.device, prefer_usb=prefer_usb)
    except OSError as e:
        print(e, file=sys.stderr)
        sys.exit(1)
    print("Microphone:", mic_desc)

    recognizer = sr.Recognizer()
    configure_recognizer(recognizer)

    print("Say: bathroom/hallway/fan + on|off; 'lights on' / 'lights off'; 'all off'. Ctrl+C exits.")

    # Diagnostic mode: mic-only test (no BLE needed)
    if args.mic_test:
        text = listen_once(recognizer, mic, vosk_model, args.ambient)
        if text:
            print("MIC TEST result: Heard text =", repr(text))
        else:
            print("MIC TEST result: No text recognized.")
        return

    # Resolve BLE address only if we need BLE
    address = args.address
    if not address:
        print("Resolving device name %r…" % args.name)
        address = asyncio.run(resolve_ble_address(args.name, args.scan_time))
        if not address:
            print("No device found. Use --scan and --address MAC.", file=sys.stderr)
            sys.exit(1)
        print("Using address:", address)

    # Diagnostic mode: BLE status read-once
    if args.ble_status_once:
        async def _ble_once() -> None:
            from bleak import BleakClient

            print("BLE status-once: connecting to", address, "…")
            async with BleakClient(address) as client:
                if not client.is_connected:
                    print("BLE status-once: failed to connect.", file=sys.stderr)
                    return
                raw = await read_status_bytes(client)
                if raw is None:
                    print("BLE status-once: status read failed.", file=sys.stderr)
                    return
                mask, light = raw
                b, h, f = decode_status(mask)
                print(
                    "BLE status-once:",
                    "bathroom=%s hallway=%s fan=%s light=%d"
                    % ("ON" if b else "OFF", "ON" if h else "OFF", "ON" if f else "OFF", light),
                )

        asyncio.run(_ble_once())
        return

    try:
        asyncio.run(
            run_voice_loop(
                address,
                recognizer,
                mic,
                vosk_model,
                args.ambient,
                args.light_on_max,
                args.no_light_gate,
            )
        )
    except KeyboardInterrupt:
        print("\nExiting.")
    except Exception as e:
        print("Fatal error:", repr(e), file=sys.stderr)
        print(traceback.format_exc(), file=sys.stderr)


if __name__ == "__main__":
    main()
