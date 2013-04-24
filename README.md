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
"THE BEER-WARE LICENSE" (Revision 42):
<emeaudroid@gmail.com> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy me a beer in return emeau/sqwall
