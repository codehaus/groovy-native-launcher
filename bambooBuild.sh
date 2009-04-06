#! /bin/sh

groovyVersion=1.6.0
groovyZipName=groovy-binary-$groovyVersion.zip

curl  http://dist.codehaus.org/groovy/distributions/$groovyZipName > $groovyZipName
unzip $groovyZipName

export GROOVY_HOME=groovy-$groovyVersion

scons test
