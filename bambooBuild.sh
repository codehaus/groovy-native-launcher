#! /bin/sh

groovyVersion=1.6.0
groovyZipName=groovy-binary-$groovyVersion.zip

wget  http://dist.codehaus.org/groovy/distributions/$groovyZipName
unzip $groovyZipName

export GROOVY_HOME=groovy-$groovyVersion

scons test
