#pragma once

#include <poll.h>

#include <functional>
#include <vector>

class Event {
 public:
  Event();
  ~Event();
  void loop();
  void exitLoop();
  void *addPollFd(int fd, short events, std::function<void (void)> cb);
  void removePollFd(int fd, short events = 0);
  void removePollFd(void *handle);
  void *addTimeout(unsigned tmo, std::function<void(void)>);
  void removeTimeout(void *handle);
  void runLater(std::function<void(void)>);

 private:
  struct PollFd {
    PollFd(int fd, short events, std::function<void (void)> cb)
      : fd(fd), events(events), cb(cb) { }
    int   fd;
    short events;
    std::function<void (void)> cb;
  };

  struct Timeout {
    Timeout(unsigned tmo, std::function<void (void)> cb)
      : tmo(tmo), cb(cb) { }
    unsigned tmo;
    std::function<void (void)> cb;
  };

  void handleTimeouts(unsigned now);
  void recomputeTimeoutsAndFds();

  std::vector<PollFd *> pollFds, *newFds = NULL;
  std::vector<Timeout *> timeouts, *newTimeouts = NULL;
  std::vector<std::function<void (void)> > later;
  struct ::pollfd *fds = NULL;
  bool done = false;
};
