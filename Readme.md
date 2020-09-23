A small example of a [JMVTI](https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html) agent that logs method calls. This could have been done (possibly more easily) using [JDWP](https://docs.oracle.com/javase/7/docs/technotes/guides/jpda/jdwp-spec.html).

## Compiling

Compile the native agent like this. Fix the `-I` paths to point to the JVM include dir on your platform:

```
gcc -shared -I/usr/local/java-runtime/impl/8/include -I/usr/local/java-runtime/impl/8/include/darwin mlogagent.c config.c -o libmlogagent.dylib
```

Compile the example java code:
```
mkdir -p classes && javac -g -d classes Test.java
```
The `-g` may be required to avoid trivial methods being inlined.

## Running

In the project root, run java with the agent:

```
java -agentpath:/Users/bduff/Documents/Development/mlogagent/libmlogagent.dylib=config=test.conf -cp classes frodo.Test
```

You should see logging whenever the `someMethod` or `anotherMethod` methods are called.

You can also get it to write to a file using the `=file=/tmp/out.txt` option at the end of the `-agentpath`.

```
java -agentpath:/Users/bduff/Documents/Development/mlogagent/libmlogagent.dylib=config=test.conf,file=/tmp/out.txt -cp classes frodo.Test
```

## Configuring

The agent is configured by a config file. See `test.conf` and `intellij.conf` for some examples. The config file looks a lot like a yaml file, but it has an extremely barebones parser (see `config.c`), so be careful about your syntax. For example, the indent level *must* be 4 spaces exactly. It's best to copy an existing config file.

Here's what the test.conf file looks like. It attaches to two methods in one class, using default behavior for one of them, and custom behavior for the other.

```yaml
# The name of a class to attach to, in JVM (slash separated) format.
- frodo/Test
    # The name and JVM type signature of a method to attach to.
    # When this method is called, we'll log and print the toString() method of its first parameter.
    - someMethod(Lfrodo/Test$VirtualFile;)Ljava/lang/String;

    # Another method, demonstrating custom options
    - anotherMethod(ILfrodo/Test$VirtualFile;)Ljava/lang/String;
        # By default, when we hit the method breakpoint, we'll log the first method parameter (index 1)
        # You can override this with the param property
        - param: 2
        # By default, when we hit the method breakpoint, we'll call toString() on one of the method
        # parameters. You can override this to call a different method with the displayMethod
        # property. The method must take no arguments and return a java.lang.String.
        - displayMethod: getPath
        # If you want to, you can display a miniature stack trace when the breakpoint is hit using
        # the showTrace property. By default, we don't do this.
        - showTrace: true
    
    - aThirdMethod(I[Lfrodo/Test$VirtualFile;)Ljava/lang/String;
        - param: 2
        # Sometimes the method parameter you care about doesn't have a convenient method you can
        # call to convert it into a string. In these cases, it can be useful to call a static
        # method instead, passing in the parameter, and have it return a String to display instead.
        # Arrays are a really good example of this. Here's how to do this using the `displayMethodStatic`
        # property. The static method must take the parameter you've selected as its only argument,
        # and must return a String.
        - displayMethodStatic: java/util/Arrays.toString([Ljava/lang/Object;)Ljava/lang/String;
        - showTrace: true
```

Here's what the output looks like:

```
someMethod: Hello
Hello Hello
someMethod: World
Hello World
someMethod: 0
anotherMethod:  1
  trace: <- anotherMethod<- run<- main
aThirdMethod: [ 2,  2,  2]
  trace: <- aThirdMethod<- run<- main
someMethod: 3
anotherMethod:  4
  trace: <- anotherMethod<- run<- main
aThirdMethod: [ 5,  5,  5]
  trace: <- aThirdMethod<- run<- main
someMethod: 6
anotherMethod:  7
  trace: <- anotherMethod<- run<- main
aThirdMethod: [ 8,  8,  8]
  trace: <- aThirdMethod<- run<- main
someMethod: 9
```

## IntelliJ Example

[intellij.conf](https://github.com/brianduff/mlogagent/blob/master/intellij.conf) is a config file I wrote to monitor various local filesystem things that Android Studio does. To turn it on, compile the code as shown above (or grab a [precompiled release](https://github.com/brianduff/mlogagent/releases)), then edit the [studio.vmoptions](https://developer.android.com/studio/intro/studio-config#customize_vm) file to add a line like this (mac - you'd use .so instead of .dylib for other platforms):

```
-agentpath:/Users/bduff/Documents/Development/mlogagent/libmlogagent.dylib=file=/tmp/out.txt,config=/Users/bduff/Documents/Development/mlogagent/intellij.conf
```

You'll need to update those absolute paths to make them correct for your computer. It'll log output to `/tmp/out.txt'. Note that because of the way the agent works, you won't be able to attach a debugger to the process while this agent is running, so you'll need to disable the agentlib if you want to use a remote debugger.

## TODO

rewrite this whole thing in rust, because it's pretty disgusting with its yucky hand rolled yaml parser 'n all.

Investigate what the performance of this is like if it uses jpda / jdwp instead of jvmti. 

Turn this into a general purpose logging debugger. Like `System.out.println()` debugging? You'll love this :)
