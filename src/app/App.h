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
    App(const App&) = delete;
    App&
    operator=(const App&) = delete;
};