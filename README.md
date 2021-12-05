# Caps2Useful
Make the Caps lock key for Windows useful again.


## Why?

Once upon a time in the last millennium, the caps lock key was useful: 
In the old time of mechanical typewriters WRITING EVERYTHING IN UPPERCASE was the only way of emphasizing text.
There was only one font in one font size, one color (black on white), and no such features as writing bold or italic.
Wring an uppercase letter required mechanically lifting of an iron typebars relative to the ink ribbon.
The caps lock allowed to fix the heavy iron typebar in the lifted position without continuous effort.

Does this sound like a requirement you had recently ... in the last 30 years or so?

Since that time, the Caps lock key remains annoying lots of PC users, in particular, me, 
by completely destroying the flow of typing whenever it accidentally gets touched.
I used to unhinge that key from the keyboard or fix it with a bent paper clip, 
if this was possible with the keyboard mechanics that I was using.

... until I finally had an idea how to turn the existence of this key from annoying to useful again.


## How?

Caps2Useful turns the Caps lock key into user programmable shortcuts ("hotkeys").
While may programs have predefined shortcuts like "Ctrl+A", "Alt+F4", you can use your own shortcuts based on the "Caps" key:
You want to have "Caps+E" to type your email address or "Caps+Tab" to open your favorite Windows program.
You can even use longer sequences like "Caps + Esc-G-M-A-I-L" to type in your lengthy, complex and impossible to remember GMail password.
All accidental presses of the Caps lock key, without pressing another key, are ignored.


## What?

The software "Caps2Useful" registers itself as a low level Windows keyboard hook and filters all keystrokes between pushing and releasing the Caps lock key.
This key sequence between "Caps lock key push" and "Caps lock key release" is interpreted as a command code. 
The action taken for each command code can be configured by the user: Either a user defined key sequence is sent, or a user defines Windows program is started.


## Where to get it?

You can download it from GitHub release, or you can build it on your own using Microsoft Visual Studio.
Some anti-virus software may complain about Caps2Useful, since it uses keylogging, a technique that can also be used for stealing passwords.
The software does not need any installation. Just copy the executable into some folder and start it.

## License

Any person obtaining a copy of this software and associated documentation files (the "Software"), 
may use their copies of the Software free of charge for personal use, in a private as well as in a commercial setting.
You are allowed to use the software on your private PC as well as on a PC provided to you by your employer or educational institution.

The software is provided "as is", without warranty of any kind. In no event shall the authors or copyright holders be liable for any claim.

