# kdbml
Native connector between KDB and Matlab. This library provides access to KDB/Q database from Matlab. Existing Matlab kx connector is terrifically slow
and it's very hard to load big datasets from KDB to Matlab.
Also existing kx Matlab connector doesn't support some KDB constructions like dictionaries and list of lists etc.
This library is written in C and compiled to Matlab MEX file providing very fast data transmition speed.

## How to make

It's possible to build this library under 64 bit Linux/Windows. (Also I'm sure it's easy to build it under 32 bit OS just using appropriate c obj files).

### Linux
Clone project from git go to src, in the Makefile set path to your Matlab installation folder and run make.

### Windows
For build library you will need MS compiler installed on your system and nmake utility.
It can be downloaded from [here](http://www.microsoft.com/en-us/download/details.aspx?id=8279).
You also need to set your Matlab root folder in Makefile.win.
Run Windows SDK 7.1 Command Prompt, go to directory where this project cloned and execute

  nmake -f Makefile.win

PS: I'm not experienced Windows user so probably there are another methods to compile it.

Also there are precompiled versions of MEX files for Linux and Windows in the bin directory.

## Using
Copy compiled MEX file, qopen.m and example.m to desired folder (for example ~/kdbmltest/ ). Run kdb q in server mode (-p key for port number):

> ~/q/l32/q -p 5555

Now run Matlab and point it to directory where you copied mex and m files. More detailed example you can find in example.m file.







