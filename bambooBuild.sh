#! /bin/sh

#  Bamboo filddles somewhat with the list of execiutables on the standard path so sometimes have to force
#  things by using absolute paths.  In particular wget, curl and unzip are available but not on the stnadrd
#  path.
 
groovyVersion=1.6.0
groovyZipName=groovy-binary-$groovyVersion.zip

/usr/bin/wget http://dist.codehaus.org/groovy/distributions/$groovyZipName
/usr/bin/unzip $groovyZipName

export GROOVY_HOME=groovy-$groovyVersion

scons test
