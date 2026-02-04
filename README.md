# OnTopTimer
This application shows the digital clock or stopwatch as an "on top" window
You can change some behavior of it in the options' window (click RMB on the clock/stopwatch window).

# Compilation
If you want to build this program from the source you can use the following method:

First install needed packages:

Debian/Ubuntu:
sudo apt install libgtk-3-0
sudo apt install libgtk-3-dev
sudo apt install pkg-config

Fedora/Red Hat (I don't checked it):
sudo yum install gtk3
sudo yum install gtk3-devel
sudo yum install pkgconfig

Windows
Install msys2
https://www.msys2.org/
Run the enviroment UCRT64 and update the base
pacman -Syu
Next install
pacman -S mingw-w64-ucrt-x86_64-toolchain
pacman -S mingw-w64-ucrt-x86_64-gtk3
Finally type exit to close the enviroment


I used Code::Blocks to compile it:
For Windows you must go to Settings->Compiler and the tab Toolchain executables and set the folder for the compiler
C:\msys64\ucrt64\bin
