package frodo;

public class Test {
  public static void main(String[] args) {
    new Test().run();
  }

  public void run() {
    System.out.println(someMethod(new RealFile("Hello")));
    System.out.println(someMethod(new RealFile("World")));

    for (int i = 0; i < 1000; i++) {
      if (i % 2 == 0) {
        someMethod(new RealFile("" + i));
      } else {
        anotherMethod(new RealFile(" " + i));
      }
    }
  }

  public String someMethod(VirtualFile file) {
    return "Hello " + file.getPath();
  }

  public String anotherMethod(VirtualFile file) {
    return "Goodbye " + file.getPath();
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
}