#! /bin/sh

#  By default build for 32-bit systems.  If the parameter is 64 then build for 64-bit systems.

if [ $# -eq 1 ]
then
    if [ $1 -eq 64 ]
    then
        widthParameter="width=64"
    fi
fi

#  Bamboo filddles somewhat with the list of execiutables on the standard path so sometimes have to force
#  things by using absolute paths.  In particular wget, curl and unzip are available but not on the stnadrd
#  path.
 
groovyVersion=1.6.2
groovyZipName=groovy-binary-$groovyVersion.zip
groovyInstallPath=groovy-$groovyVersion

/usr/bin/wget http://dist.codehaus.org/groovy/distributions/$groovyZipName
/usr/bin/unzip $groovyZipName

export GROOVY_HOME=$groovyInstallPath

#  Remove any inherited values in JAVA_OPTS since they shouldn't be there.  Is the return code from scons
#  the return code of the script?

#__JLAUNCHER_DEBUG=true JAVA_OPTS=  scons $widthParameter test
JAVA_OPTS=  scons $widthParameter test
