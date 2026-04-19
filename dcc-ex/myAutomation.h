// myAutomation.h — EXRAIL definitions for Switching Controller
//
// Copy this file to your EX-CommandStation source folder.
//
// Object definitions (PIN_TURNOUT, ROSTER) must be OUTSIDE any sequence.
// Executable commands (SET_TRACK) must be INSIDE an AUTOSTART/DONE block.

// --- Turnout definitions (visible in Engine Driver, wiThrottle, JMRI) ---
PIN_TURNOUT(1, 800, "T01")
PIN_TURNOUT(2, 801, "T02")
PIN_TURNOUT(3, 802, "T03")
PIN_TURNOUT(4, 803, "T04")
PIN_TURNOUT(5, 804, "T05")
PIN_TURNOUT(6, 805, "T06")
PIN_TURNOUT(7, 806, "T07")
PIN_TURNOUT(8, 807, "T08")
PIN_TURNOUT(9, 808, "T09")
PIN_TURNOUT(10, 809, "T10")
PIN_TURNOUT(11, 810, "T11")
PIN_TURNOUT(12, 811, "T12")

// --- Roster ---
ROSTER(152, "EMD NW2", "Front & Rear Light/Bell/*Horn/Coupler Sounds/Dynamic Brake/*Rev Engine Up/*Rev Engine Down/Ditch Lights/Volume Mute/Startup & Shutdown/Raidiator Cooling Fan/Air Filling & Release/Brake Set & Release/Grade Crossing Horn/Passenger Annocements/Freight Announcements/Maintenance Sounds/Radio Sounds/City Background Sounds/Farm Background Sounds/Industrial Sounds/Lumber Yard Sounds/Switch to second horn/Track Sounds/Aux Light Control/*Long Horn/Play Macro/Record Macro Start & Stop/Brake Squeal")

// --- Startup sequence ---
AUTOSTART
  SET_TRACK(A, MAIN)
  SET_TRACK(B, PROG)
DONE
