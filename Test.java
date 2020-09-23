package frodo;

public class Test {
  public static void main(String[] args) {
    new Test().run();
  }

  public void run() {
    System.out.println(someMethod(new RealFile("Hello")));
    System.out.println(someMethod(new RealFile("World")));

    for (int i = 0; i < 10; i++) {
      if (i % 3 == 0) {
        someMethod(new RealFile("" + i));
      } else if (i % 3 == 1) {
        anotherMethod(0, new RealFile(" " + i));
      } else {
        aThirdMethod(0, new RealFile(" " + i));
      }
    }
  }

  public String someMethod(VirtualFile file) {
    return "Hello " + file.getPath();
  }

  public String anotherMethod(int random, VirtualFile file) {
    return "Goodbye " + file.getPath();
  }

  public String aThirdMethod(int random, VirtualFile file) {
    return "The third method " + file.getPath();
  }

  private abstract static class VirtualFile {
    public abstract String getPath();
  }

  private static class RealFile extends VirtualFile {
    private String path;

    RealFile(String path) {
      this.path = path;
    }

    @Override
    public String getPath() {
      return path;
    }

    @Override
    public String toString() {
      return getPath();
    }

  }

  public static String printThingy(VirtualFile f) {
    return "A thingy was called";
  }
}