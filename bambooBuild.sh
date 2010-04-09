#! /bin/sh

#  Specify the version of Groovy and Gant to use for the tests.  These will be downloaded.

groovyVersion=1.7.2
gantVersion=1.9.2

#  By default build for 32-bit systems.  If the parameter is 64 then build for 64-bit systems.

if [ $# -eq 1 ]
then
    if [ $1 -eq 64 ]
    then
        widthParameter="width=64"
    fi
fi

#  Having problems with PATH so find out what it is.
/bin/echo $PATH

#  Bamboo fiddles with the standard path so have to force things by using absolute paths.
 
groovyZipName=groovy-binary-$groovyVersion.zip
groovyInstallPath=groovy-$groovyVersion
/bin/rm -rf $groovyZipName $groovyInstallPath
/usr/bin/wget http://dist.codehaus.org/groovy/distributions/$groovyZipName
/usr/bin/unzip $groovyZipName
export GROOVY_HOME=$groovyInstallPath

gantZipName=gant-$gantVersion.zip
gantInstallPath=gant-$gantVersion
/bin/rm -rf $gantZipName $gantZipName
/usr/bin/wget http://dist.codehaus.org/gant/distributions/$gantZipName
/usr/bin/unzip $gantZipName
export GANT_HOME=$gantInstallPath

#  Remove any inherited values in JAVA_OPTS since they shouldn't be there.  Is the return code from scons
#  the return code of the script?

#__JLAUNCHER_DEBUG=true JAVA_OPTS=  scons $widthParameter test
JAVA_OPTS=  scons $widthParameter xmlOutputRequired=True test
