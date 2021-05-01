#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <signal.h>

#include "event.h"
#include "util.h"


Event::Event() {
}

Event::~Event() {
  recomputeTimeoutsAndFds();
  for (auto it = timeouts.begin(); it != timeouts.end(); it++) {
    delete(*it);
  }
  for (auto it = pollFds.begin(); it != pollFds.end(); it++) {
    delete(*it);
  }
  delete[] fds;
}

void Event::loop() {
  recomputeTimeoutsAndFds();
  while (!done && (!pollFds.empty() || !timeouts.empty() || !later.empty())) {
    // Find timeout that will fire next, if any
    unsigned now = Util::millis();
    unsigned tmo = later.empty() ? 0 : now + 1;
    for (auto it = timeouts.begin(); it != timeouts.end(); it++) {
      if (*it && (!tmo || tmo > (*it)->tmo)) {
        tmo = (*it)->tmo;
      }
    }
    if (tmo) {
      // If the timeout has already expired, handle it now
      if (tmo <= now || !later.empty()) {
        handleTimeouts(now);
        continue;
      } else {
        tmo -= now;
      }
    }
    // Wait for next event
    struct timespec ts = { tmo / 1000, tmo*1000000L };
    int nFds = pollFds.size();
    int rc = ppoll(fds, nFds, tmo ? &ts : NULL, NULL);
    if (!rc) {
      handleTimeouts(Util::millis());
    } else if (rc > 0) {
      int i = 0;
      for (auto it = pollFds.begin();
           rc > 0 && it != pollFds.end(); it++, i++) {
        if (fds[i].revents) {
          if (*it) {
            (*it)->cb();
          }
          fds[i].revents = 0;
          rc--;
        }
      }
      recomputeTimeoutsAndFds();
    }
    recomputeTimeoutsAndFds();
  }
}

void Event::exitLoop() {
  done = true;
}

void *Event::addPollFd(int fd, short events, std::function<void (void)> cb) {
  if (!newFds) {
    newFds = new std::vector<PollFd *>(pollFds);
  }
  PollFd *pfd = new PollFd(fd, events, cb);
  newFds->push_back(pfd);
  return pfd;
}

void Event::removePollFd(int fd, short events) {
  // Create vector with future poll information
  if (!newFds) {
    newFds = new std::vector<PollFd *>(pollFds);
  }
  // Zero out existing record. This avoids the potential for races
  for (auto it = pollFds.begin(); it != pollFds.end(); it++) {
    if (*it &&
        fd == (*it)->fd &&
        (!events || events == (*it)->events)) {
      *it = NULL;
    }
  }
  // Remove fd from future list
  for (auto it = newFds->rbegin(); it != newFds->rend();) {
    if (fd == (*it)->fd &&
        (!events || events == (*it)->events)) {
      delete *it;
      auto iter = newFds->erase(--it.base());
      it = std::reverse_iterator<typeof iter>(iter);
    } else {
      ++it;
    }
  }
}

void Event::removePollFd(void *handle) {
  // Create vector with future poll information
  if (!newFds) {
    newFds = new std::vector<PollFd *>(pollFds);
  }
  // Zero out existing record. This avoids the potential for races
  for (auto it = pollFds.begin(); it != pollFds.end(); it++) {
    if (*it && *it == handle) {
      *it = NULL;
    }
  }
  // Remove fd from future list
  for (auto it = newFds->rbegin(); it != newFds->rend();) {
    if (*it == handle) {
      delete *it;
      auto iter = newFds->erase(--it.base());
      it = std::reverse_iterator<typeof iter>(iter);
    } else {
      ++it;
    }
  }
}

void *Event::addTimeout(unsigned tmo, std::function<void(void)> cb) {
  if (!newTimeouts) {
    newTimeouts = new std::vector<Timeout *>(timeouts);
  }
  newTimeouts->push_back(new Timeout(tmo + Util::millis(), cb));
  return newTimeouts->back();
}

void Event::removeTimeout(void *handle) {
  // Create vector with future timeouts
  if (!newTimeouts) {
    newTimeouts = new std::vector<Timeout *>(timeouts);
  }
  // Zero out existing record. This avoids the potential for races
  for (auto it = timeouts.begin(); it != timeouts.end(); it++) {
    if (*it == handle) {
      *it = NULL;
    }
  }
  // Remove timeout from future list
  for (auto it = newTimeouts->rbegin(); it != newTimeouts->rend();) {
    if (handle == *it) {
      delete *it;
      auto iter = newTimeouts->erase(--it.base());
      it = std::reverse_iterator<typeof iter>(iter);
    } else {
      ++it;
    }
  }
}

void Event::handleTimeouts(unsigned now) {
  for (auto it = timeouts.begin(); it != timeouts.end(); it++) {
    if (*it && now >= (*it)->tmo) {
      auto cb = (*it)->cb;
      removeTimeout(*it);
      cb();
    }
  }
  while (!later.empty()) {
    std::vector<std::function<void (void)> > tmp;
    tmp.swap(later);
    for (auto it = tmp.begin(); it != tmp.end(); it++) {
      (*it)();
    }
  }
  recomputeTimeoutsAndFds();
}

void Event::runLater(std::function<void(void)> cb) {
  later.push_back(cb);
}

void Event::recomputeTimeoutsAndFds() {
  if (newFds) {
    delete[] fds;
    fds = new struct ::pollfd[newFds->size()];
    int i = 0;
    for (auto it = newFds->begin(); it != newFds->end(); it++, i++) {
      fds[i].fd = (*it)->fd;
      fds[i].events = (*it)->events;
      fds[i].revents = 0;
    }
    pollFds.clear();
    pollFds.swap(*newFds);
    delete newFds;
    newFds = NULL;
  }
  if (newTimeouts) {
    timeouts.clear();
    timeouts.swap(*newTimeouts);
    delete newTimeouts;
    newTimeouts = NULL;
  }
}
