==============================================================
Tait Key Utility
==============================================================
Disclaimer and instructions:
This software is provided as is, and voids any product warranties.
It has been built with Qt Creator on Fedora 27, and should work on any Qt
enabled system. To make it work for you, open up QtCreator IDE and open the
.pro file for the project, then away you go.

You should be able to read keys from any TM8200 or TM9100 style radio.

NOTE: The intention for this software is to allow amateurs / enthusiasts of
second hand tait radio's the chance to extend the life and usefulness of their
radios while promoting increased user knowledge.

If you use this software on a radio covered by a Tait contract, in a fleet,
as a Tait dealer, or to make a profit by selling radios modified with this tool,
you should expect an unholy shitstorm to reign down upon you.

Most information gathered to build this program has been obtained from either
open / public domain standards, or open documents from the Tait Technical
Library in or around 2012.

--------------------------------------------------------------
Steps required to make this work:
--------------------------------------------------------------
1. Obtain an old 8200 or 9100 series radio
2. Use the Tait Calibration software to save a copy of your radio's calibration
3. Make sure you did step 2. Sometimes fiddling with firmware affects your
radio's calibration making it unusable in the future if you haven't saved its
calibration. (** You have been warned! **)
4. Ensure firmware is aligned to versions 6.01.00.0051 for a TM8200 or
8.03.01.0031 for the TM9100 series. You will need the Tait CPS/RSS to do this.
5. Extract the Tait supplied firmware file (the .ttfp file is a .zip file) to the
patched_firmware directory. The file you need to extract is:
	QMA2F_std_6.01.00.0051.Srec for the TM8200, or
	QMA3F_A00_8.03.01.0031.Srec for TM9100's.

6. Use the 'patch' utility from linux, Cygwin or the linux tools for Windows 10.
The command line will be:
	patch -p0 -i QMA2F_std_6.01.00.0051.Srec.patch
	patch -p0 -i QMA3F_A00_8.03.01.0031.Srec.patch

You will see a message like:
patching file QMA2F_std_6.01.00.0051.Srec

If this fails, DO NOT continue beyond this point. Doing so will potentially send
a corrupted firmware to your device.

7. Use the Tait firmware upgrade utility to flash the newly patched .Srec firmware
file to the radio.

In order do this you will probably need to tick the box to 'Force Upgrade'

Note: doing this invalidates any warranty and no Tait agent will ever work on
a radio that has been adjusted in this way. Additionally there is always the
chance something bad may happen and you brick your radio. ** Your risk **

8. Run this tool and inspect the radio.
9. At this point the SFE master key has obtained and you could do any number
of things but as a general rule, you would enable the relevant keys at this
point (or Enable All if you're feeling lucky)
10. Repeat the firmware process in 5 to reload Tait Provided firmware
11. Reload your radio's calibration if required
12. You can use this tool on tha radio at anytime in the future as long as
there is a data file in the ../radiodb subdirectory of this application that
matches the radio

--------------------------------------------------------------
About Advanced System Keys
--------------------------------------------------------------
You do not need any special firmware for this. You do need authority from the
system administrator (otherwise it is illegal) and they will provide you with
the System ID and relevant WACN as part of your usage licence. 

In order for trunking to work you need:
- Legal access to the network in question
- You need to know the WACN and SystemID (to generate a key)
- You need to have a valid P25 ID (subscriber unit number) for the talkgroup
(TGID) you are trying to affiliate to. Usually the system administrator for
the system (for example in the case of TaitNet NZ that is usually the NZ
Police) will define which subscriber units can access a given TGID.

--------------------------------------------------------------
About AES/DES Keys
--------------------------------------------------------------
You can define any AES key you like.
The DES key standard requires that every byte have an uneven number of 1
bits... so 01 is a valid byte, 00 is not. The GUI will warn you if you use an
invalid DES key.
==============================================================

