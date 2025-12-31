#pragma once

class App
{
  public:
    App();
    void
    begin();
    void
    loop();

  private:
    // no copy
    App(const App&) = delete;
    App&
    operator=(const App&) = delete;
};