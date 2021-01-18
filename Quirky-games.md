= GAMES OF INTEREST TO SANITY-CHECK WHENEVER BEHAVIOR IS CHANGED =

* Defcon: Has two windows, one of them with a parent; does modeswitches by directly sending X protocol; hooking Xilb functions won't work
* Duke3D Megaton: Same
* Long Live the Queen: Same but toplevel window isn't override-redirect
* The Bard's Tale: Its only window is override redirect.
* Duke3D Megaton - classic: Doesn't animate in the setup if keyboard isn't pressed; two windows.
* Aquaria: tries to do modeswitches through Xrandr, is real fullscreen instead of windowed
* Wizardry 7: two windows at startup because of dosbox splashscreen; no animation during startup sequence
* Trine 2: has launcher
* Hotline Miami: has launcher
* Awesomenauts: can have launcher
* Wargame: European Escalation: pointer grabs that bypass X server path
* Planetary Annihilation has plenty of mapped windows, and a big cursor hotspot offset
* XCOM can have trouble focusing after launcher as it doesn't want to start drawing until it gets focused