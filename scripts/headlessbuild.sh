#!/bin/bash
#source /opt/Xilinx/SDK/2017.4/settings64.sh

#  csh shell type: setenv WS `pwd`/MYWS
# bash shell type: export WS=err

WS=/home/chris/workspace/CItest/tmp

echo "*** warning: deleting current workspace $WS ***"
rm -rf $WS

echo "*** creating app **"
git clone git@github.com:christopherbate/MZNTAppCPP.git $WS/logger/

xsct -eval "setws $WS; importprojects $WS/logger/"

echo "*** starting build step ***"
#xsdk -wait -eclipseargs -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -build all -data $WS -vmargs -Dorg.eclipse.cdt.core.console=org.eclipse.cdt.core.systemConsole
xsct -eval "setws $WS; projects -clean; projects -build" 
echo "*** build step: done ***"
