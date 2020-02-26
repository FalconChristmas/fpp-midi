The MIDI Plugin can be use to respond to MIDI events by invoking FPP Commands.
<p>
At the top of the page, each of the detected MIDI devices/ports is displayed.  Each can be individually enabled for listening.   For each device, you can also turn on the reporting of Systed Extend (SysEX) events, Time Code events, and Sense events.   If you don't need those events, it is recommended to leave them off to reduce processing overhead.
<p>
For each Event added, the following fields need to be configured:
<p>
<ol>
<li>Description - this is a short description of what the event does.  This is ignored by FPP, but can be used to help you organized the events.</li>
<li>Conditions - these are conditions to filter in/out events based on the bytes in the message sent from the MIDI device.  For example, you could apply a condition to only respond to button down states insead of up and down.</li>
<li>Command - the FPP Command to execute.   Each parameter of the command MAY be calculated based on parameters using very simple math functions.  For example, you can create a red color that is scaled from the pressure of the key press (usually byte 3, values 0-127) by using a formula like "(b3*2)*65536".

    The plugin uses the TinyExpr library from https://github.com/codeplea/tinyexpr for implementing the expression processing.
</li>
</ol>
<p>
The "Last Messages" section in the upper right displays the last 25 messages that FPPD has received.  Clicking Refresh will refresh the list.  These can be used to help identify which parameters are being used to help define conditions.
