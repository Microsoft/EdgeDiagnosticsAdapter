REM Node-gyp currently does not support vs2017 build tools. We set the C++ build tools v140(vs2015).
REM If you have installed vs2017 enterprise edition:
REM set VCTargetsPath=C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\IDE\VC\VCTargets
set VCTargetsPath=C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\v140
set GYP_MSVS_VERSION=2015
