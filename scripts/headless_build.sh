# Source the settings
source /opt/Xilinx/SDK/2017.4/settings64.sh

echo "Building MZNTCpp"

WS='~/workspace/xilinx'

echo "*** starting build step ***"
xsdk -wait -eclipseargs -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -cleanBuild all -data $WS -vmargs -Dorg.eclipse.cdt.core.console=org.eclipse.cdt.core.systemConsol
xsdk -wait -eclipseargs -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -build all -data $WS -vmargs -Dorg.eclipse.cdt.core.console=org.eclipse.cdt.core.systemConsole
echo "*** build step: done ***"



