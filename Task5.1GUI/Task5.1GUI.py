#!/usr/bin/env python3
"""
SIT210 5.1P — GUI for three room LEDs (living room, bathroom, closet).

Wiring (BCM pin numbers; connect each LED cathode through ~330 Ω to GND):
  Living room  -> GPIO 17 (physical pin 11)
  Bathroom     -> GPIO 27 (physical pin 13)
  Closet       -> GPIO 22 (physical pin 15)

Run on Raspberry Pi: python3 led_gui.py
"""

import sys
import tkinter as tk
from tkinter import ttk


def _make_leds():
    """Return three LED-like objects; use gpiozero on Pi, stubs elsewhere."""
    try:
        from gpiozero import LED

        return (
            LED(17, active_high=False),
            LED(27, active_high=False),
            LED(22, active_high=False),
        )
    except Exception as e:
        print("GPIO not available — simulation mode:", e, file=sys.stderr)

        class _Stub:
            def on(self):
                pass

            def off(self):
                pass

            def close(self):
                pass

        return _Stub(), _Stub(), _Stub()


def main():
    living_led, bathroom_led, closet_led = _make_leds()
    leds_by_room = {
        "living": living_led,
        "bathroom": bathroom_led,
        "closet": closet_led,
    }

    root = tk.Tk()
    root.title("Room lights")
    root.geometry("320x200")
    root.minsize(320, 220)

    frame = ttk.Frame(root, padding=16)
    frame.pack(fill=tk.BOTH, expand=True)

    ttk.Label(frame, text="Turn each room light on/off:").pack(anchor=tk.W)

    living_state = tk.StringVar(value="on")
    bathroom_state = tk.StringVar(value="on")
    closet_state = tk.StringVar(value="on")

    def apply_states(*_):
        if living_state.get() == "on":
            living_led.on()
        else:
            living_led.off()

        if bathroom_state.get() == "on":
            bathroom_led.on()
        else:
            bathroom_led.off()

        if closet_state.get() == "on":
            closet_led.on()
        else:
            closet_led.off()

    living_state.trace_add("write", apply_states)
    bathroom_state.trace_add("write", apply_states)
    closet_state.trace_add("write", apply_states)
    apply_states()

    def add_room_controls(label: str, var: tk.StringVar):
        row = ttk.Frame(frame)
        row.pack(fill=tk.X, pady=(10, 0), anchor=tk.W)
        ttk.Label(row, text=label, width=12).pack(side=tk.LEFT)
        ttk.Radiobutton(row, text="On", variable=var, value="on").pack(side=tk.LEFT, padx=(8, 0))
        ttk.Radiobutton(row, text="Off", variable=var, value="off").pack(side=tk.LEFT, padx=(8, 0))

    add_room_controls("Living room", living_state)
    add_room_controls("Bathroom", bathroom_state)
    add_room_controls("Closet", closet_state)

    ttk.Button(frame, text="Exit", command=root.destroy).pack(pady=(20, 0))

    try:
        root.mainloop()
    finally:
        for led in leds_by_room.values():
            try:
                led.off()
            except Exception:
                pass
        try:
            living_led.close()
            bathroom_led.close()
            closet_led.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()
