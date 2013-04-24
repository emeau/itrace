itrace
======

hook objc_msgSend to trace Objective-C method callz

based on the work of Stefan Esser on his dumpdecrypted.dylib stuff 
and libsubjc from kennytm that i couldn't build from source nor use properly (maybe my bad dude)
all credz to them srsly, they've done teh work. i've just added some bullshit.

usage(until fronted functionnal): 
root# DYLD_INSERT_LIBRARIES=itrace.dylib /Applications/Calculator.app/Calculator
oh and be sure to have itrace.plist in the current directory
this dylib just hook objc_msgSend thanks to MobileSubstrate/interpos (just uncomment some 
commented line to switch method) & print out the class and methods called, white/blacklisting

last rev: 2013/02/17
work done on free time (when not drinking) and @ Sogeti ESEC, thanks to them for the research time o/

licence
=======
           DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
                   Version 2, December 2004
 
Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
 
Everyone is permitted to copy and distribute verbatim or modified
copies of this license document, and changing it is allowed as long
as the name is changed.
 
           DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
  TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 
 0. You just DO WHAT THE FUCK YOU WANT TO.

