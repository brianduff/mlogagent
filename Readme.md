A small example of a [JMVTI](https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html) agent that logs method calls. This could have been done (possibly more easily) using [JDWP](https://docs.oracle.com/javase/7/docs/technotes/guides/jpda/jdwp-spec.html).

Compile the native agent like this. Fix the `-I` paths to point to the JVM include dir on your platform:

```
gcc -shared -I/usr/local/java-runtime/impl/8/include -I/usr/local/java-runtime/impl/8/include/darwin mlogagent.c -o libmlogagent.dylib
```

Compile the example java code:
```
mkdir -p classes && javac -g -d classes Test.java
```
The `-g` may be required to avoid trivial methods being inlined.

Run java with the agent:

```
java -agentpath:/Users/bduff/Documents/Development/mlogagent/libmlogagent.dylib -cp classes frodo.Test
```

You should see logging whenever the `someMethod` or `anotherMethod` methods are called.

You can also get it to write to a file using the `=file=/tmp/out.txt` option at the end of the `-agentpath`.

```
java -agentpath:/Users/bduff/Documents/Development/mlogagent/libmlogagent.dylib=file=/tmp/out.txt -cp classes frodo.Test
```