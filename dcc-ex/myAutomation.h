// myAutomation.h — EXRAIL definitions for Switching Controller turnouts
//
// Copy this file to your EX-CommandStation source folder.
// These PIN_TURNOUT definitions create turnouts that are visible in
// WiThrottle apps (Engine Driver, wiThrottle) and JMRI.
//
// Each turnout maps a DCC-EX turnout ID to a VPIN on the Switching Controller.
// VPIN 800 = turnout array index 0 (T01), 801 = index 1 (T02), etc.
//
// Control from any throttle or serial monitor:
//   <T 1 T>   — Throw turnout T01
//   <T 1 C>   — Close turnout T01
//   <JT>      — List all turnout IDs

AUTOSTART
SET_TRACK(A,MAIN)
SET_TRACK(B,PROG)

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

ROSTER(152, "EMD NW2", "Front & Rear Light/Bell/*Horn/Coupler Sounds/Dynamic Brake/*Rev Engine Up/*Rev Engine Down/Ditch Lights/Volume Mute/Startup & Shutdown/Raidiator Cooling Fan/Air Filling & Release/Brake Set & Release/Grade Crossing Horn/Passenger Annocements/Freight Announcements/Maintenance Sounds/Radio Sounds/City Background Sounds/Farm Background Sounds/Industrial Sounds/Lumber Yard Sounds/Switch to second horn/Track Sounds/Aux Light Control/*Long Horn/Play Macro/Record Macro Start & Stop/Brake Squeal")
DONE