#pragma once
#include <semaphore.h>
#include <time.h>

class PosixSemaphore
{
public:
  PosixSemaphore(bool interprocess, int initialValue);
  ~PosixSemaphore();
  void Wait();
  void Post();
  bool TryWait();
  bool TimedWait(time_t sec, long nsec);

private:
  sem_t *m_semaphore;
};