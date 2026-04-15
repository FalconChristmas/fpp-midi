The MIDI Plugin can be use to respond to MIDI events by invoking FPP Commands.
<p>
At the top of the page, each of the detected MIDI devices/ports is displayed.  Each can be individually enabled for listening.   For each device, you can also turn on the reporting of Systed Extend (SysEX) events, Time Code events, and Sense events.   If you don't need those events, it is recommended to leave them off to reduce processing overhead.
<p>
For each Event added, the following fields need to be configured:
<p>
<ol>
<li>Description - this is a short description of what the event does.  This is ignored by FPP, but can be used to help you organized the events.</li>
<li>Conditions - these are conditions to filter in/out events based on the bytes in the message sent from the MIDI device.  For example, you could apply a condition to only respond to button down states insead of up and down.</li>
<li>Command - the FPP Command to execute.
<p>
If the parameter starts with a single equal sign, it will be evaluated as a simple mathematical formula.  For example, you can create a red color that is scaled from the velocity of the key press (usually byte 3, values 0-127) by using a formula like "=rgb(b3*2,0,0)".  The following variables are available in expressions:
<ul>
    <li>b1, b2, b3, b4, b5 - first five bytes of the MIDI message (0 if not present)</li>
    <li>note - same as b2</li>
    <li>velocity - same as b3</li>
    <li>channel - lower 4 bits of b1 (0-15)</li>
    <li>pitch - combined from b3 and b2, range -8192 to 8191</li>
    <li>control - same as b2</li>
    <li>note_var - alias of note</li>
    <li>channel_var - alias of channel</li>
    <li>velocity_var - alias of velocity</li>
</ul>
For example, the formula above can be written as "=rgb(velocity*2,0,0)".
<p>
If the parameter does not start with a single =, it is treated as a string, but parameters can be substituted in by using %%name%% in the string (for example %%b1%%, %%note%%, or %%velocity%%).  For example: "Matrix-%%b1%%".
<p>


    The plugin uses the TinyExpr library from https://github.com/codeplea/tinyexpr for implementing the expression processing.   We have added three useful functions:<br>
    <ul>
        <li>rgb(r, g, b) - will take the r/g/b values (0-255) and create an integer to represent the color </li>
        <li>hsv(h, s, v) - will take the hue/saturation/value values (0-1) and create an integer to represent the color </li>
        <li>if(cond, tExp, fExp) - will evaluate the condition and if not 0, will return tExp, otherwise fExp</li>
    </ul>
</li>
</ol>
<p>
The "Last Messages" section in the upper right displays the last 25 messages that FPPD has received.  Clicking Refresh will refresh the list.  These can be used to help identify which parameters are being used to help define conditions.
