#!/usr/bin/env python3
"""
SIT210 5.2C — GUI for room LEDs with intensity sliders.

Wiring (BCM pin numbers; connect each LED cathode through ~330 Ω to GND):
  Living room  -> GPIO 17 (physical pin 11)
  Bathroom     -> GPIO 27 (physical pin 13)
  Closet       -> GPIO 22 (physical pin 15)

Run on Raspberry Pi: python3 Task5.2GUI.py
"""

import sys
import tkinter as tk
from tkinter import ttk


def _make_leds():
    """Return LED-like objects; PWM for all three rooms."""
    try:
        from gpiozero import LED, PWMLED

        return (
            PWMLED(17, active_high=False),
            PWMLED(27, active_high=False),
            PWMLED(22, active_high=False),
        )
    except Exception as e:
        print("GPIO not available — simulation mode:", e, file=sys.stderr)

        class _Stub:
            value = 0.0

            def on(self):
                self.value = 1.0

            def off(self):
                self.value = 0.0

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
    root.geometry("420x360")
    root.minsize(420, 360)

    frame = ttk.Frame(root, padding=16)
    frame.pack(fill=tk.BOTH, expand=True)

    ttk.Label(frame, text="Control room lights:").pack(anchor=tk.W)

    living_state = tk.StringVar(value="on")
    bathroom_state = tk.StringVar(value="on")
    closet_state = tk.StringVar(value="on")
    living_brightness = tk.DoubleVar(value=100.0)
    bathroom_brightness = tk.DoubleVar(value=100.0)
    closet_brightness = tk.DoubleVar(value=100.0)
    syncing_state = {"active": False}

    def apply_lights(*_):
        if syncing_state["active"]:
            return
        syncing_state["active"] = True

        # Tkinter Scale sends values as strings; clamp explicitly for safety.
        def _clamp_pct(var: tk.DoubleVar) -> float:
            return max(0.0, min(100.0, float(var.get())))

        try:
            living_pct = _clamp_pct(living_brightness)
            bathroom_pct = _clamp_pct(bathroom_brightness)
            closet_pct = _clamp_pct(closet_brightness)

            if living_pct <= 0.0 and living_state.get() != "off":
                living_state.set("off")
            elif living_pct > 0.0 and living_state.get() != "on":
                living_state.set("on")

            if bathroom_pct <= 0.0 and bathroom_state.get() != "off":
                bathroom_state.set("off")
            elif bathroom_pct > 0.0 and bathroom_state.get() != "on":
                bathroom_state.set("on")

            if closet_pct <= 0.0 and closet_state.get() != "off":
                closet_state.set("off")
            elif closet_pct > 0.0 and closet_state.get() != "on":
                closet_state.set("on")

            living_led.value = living_pct / 100.0 if living_state.get() == "on" else 0.0
            bathroom_led.value = bathroom_pct / 100.0 if bathroom_state.get() == "on" else 0.0
            closet_led.value = closet_pct / 100.0 if closet_state.get() == "on" else 0.0
        finally:
            syncing_state["active"] = False

    living_state.trace_add("write", apply_lights)
    bathroom_state.trace_add("write", apply_lights)
    closet_state.trace_add("write", apply_lights)
    living_brightness.trace_add("write", apply_lights)
    bathroom_brightness.trace_add("write", apply_lights)
    closet_brightness.trace_add("write", apply_lights)
    apply_lights()

    def add_room_controls(label: str, var: tk.StringVar):
        row = ttk.Frame(frame)
        row.pack(fill=tk.X, pady=(10, 0), anchor=tk.W)
        ttk.Label(row, text=label, width=12).pack(side=tk.LEFT)
        ttk.Radiobutton(row, text="On", variable=var, value="on").pack(side=tk.LEFT, padx=(8, 0))
        ttk.Radiobutton(row, text="Off", variable=var, value="off").pack(side=tk.LEFT, padx=(8, 0))

    add_room_controls("Living room", living_state)

    living_row = ttk.Frame(frame)
    living_row.pack(fill=tk.X, pady=(6, 0), anchor=tk.W)
    ttk.Label(living_row, text="Intensity", width=12).pack(side=tk.LEFT)
    tk.Scale(
        living_row,
        from_=0,
        to=100,
        orient=tk.HORIZONTAL,
        variable=living_brightness,
        length=170,
        resolution=1,
    ).pack(side=tk.LEFT, padx=(8, 0))

    add_room_controls("Bathroom", bathroom_state)

    bathroom_row = ttk.Frame(frame)
    bathroom_row.pack(fill=tk.X, pady=(6, 0), anchor=tk.W)
    ttk.Label(bathroom_row, text="Intensity", width=12).pack(side=tk.LEFT)
    tk.Scale(
        bathroom_row,
        from_=0,
        to=100,
        orient=tk.HORIZONTAL,
        variable=bathroom_brightness,
        length=170,
        resolution=1,
    ).pack(side=tk.LEFT, padx=(8, 0))

    add_room_controls("Closet", closet_state)

    closet_row = ttk.Frame(frame)
    closet_row.pack(fill=tk.X, pady=(6, 0), anchor=tk.W)
    ttk.Label(closet_row, text="Intensity", width=12).pack(side=tk.LEFT)
    tk.Scale(
        closet_row,
        from_=0,
        to=100,
        orient=tk.HORIZONTAL,
        variable=closet_brightness,
        length=170,
        resolution=1,
    ).pack(side=tk.LEFT, padx=(8, 0))

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
