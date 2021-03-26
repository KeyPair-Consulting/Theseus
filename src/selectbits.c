/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>  // for getopt, optarg, optind
#include <libgen.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "binio.h"
#include "binutil.h"
#include "enttypes.h"
#include "globals-inst.h"
#include "globals.h"
#include "precision.h"
#include "translate.h"

static uint32_t *data;
static size_t datalen;
static pthread_mutex_t threadsWaitingMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_barrier_t joinBarrier;
static bool configConservative = false;

static uint32_t threadsWaiting;

enum workerState { INITSTATE, WORKSTATE, LISTENSTATE, SENDSTATE, WAITSTATE, DONESTATE, RESULTSTATE };
struct threadInfoType {
  pthread_t threadID;
  uint32_t localThreadID;
  int assignmentWriteFD;  // this is only written by the master process
  int assignmentReadFD;  // This is only read by the processing threads
  int resultsWriteFD;  // This is only written by the processing threads
  int resultsReadFD;  // this is only read by the master process
  enum workerState state;
  size_t jobsAssigned;
};

enum workerCommand { EXITCMD, ASSIGNMENTCMD, JOINCMD };
struct assignmentType {
  enum workerCommand command;
  uint32_t mask;
};

enum workerResponse { READYRSP, ASSESSMENTRSP };
struct assessmentType {
  enum workerResponse response;
  uint32_t mask;
  double assessment;
};

struct bestMaskData {
  double entropy;
  uint32_t mask;
};

static uint32_t getThreadsWaiting(void) {
  uint32_t localThreadsWaiting;

  if (pthread_mutex_lock(&threadsWaitingMutex) != 0) {
    perror("Can't get mutex");
    pthread_exit(NULL);
  }

  localThreadsWaiting = threadsWaiting;

  if (pthread_mutex_unlock(&threadsWaitingMutex) != 0) {
    perror("Can't get mutex");
    pthread_exit(NULL);
  }

  return localThreadsWaiting;
}

static void incThreadsWaiting(void) {
  if (pthread_mutex_lock(&threadsWaitingMutex) != 0) {
    perror("Can't get mutex");
    pthread_exit(NULL);
  }

  assert(threadsWaiting < UINT32_MAX);
  threadsWaiting++;

  if (pthread_mutex_unlock(&threadsWaitingMutex) != 0) {
    perror("Can't get mutex");
    pthread_exit(NULL);
  }
}

static void decThreadsWaiting(void) {
  if (pthread_mutex_lock(&threadsWaitingMutex) != 0) {
    perror("Can't get mutex");
    pthread_exit(NULL);
  }

  assert(threadsWaiting > 0);
  threadsWaiting--;

  if (pthread_mutex_unlock(&threadsWaitingMutex) != 0) {
    perror("Can't get mutex");
    pthread_exit(NULL);
  }
}

static void readMessage(int fd, void *buffer, size_t len) {
  ssize_t result = 0;
  size_t readin = 0;

  assert(errno == 0);

  while (readin < len) {
    result = read(fd, buffer, len - readin);

    if (result < 0) {
      if ((errno == EAGAIN) || (errno == EINTR)) {
        perror("Error reading pipe; continuing");
        errno = 0;
      } else {
        perror("Read error in computing thread");
        pthread_exit(NULL);
      }
    } else {
      readin += (size_t)result;
    }
  }
}

static void writeMessage(int fd, void *buffer, size_t len) {
  ssize_t result = 0;
  size_t wroteOut = 0;

  assert(errno == 0);

  while (wroteOut < len) {
    result = write(fd, buffer, len - wroteOut);

    if (result < 0) {
      if ((errno == EAGAIN) || (errno == EINTR)) {
        perror("Error writing to pipe; continuing");
        errno = 0;
      } else {
        perror("Write error in computing thread");
        pthread_exit(NULL);
      }
    } else {
      wroteOut += (size_t)result;
    }
  }
}

static enum workerCommand getAssignment(int fd, uint32_t *mask) {
  struct assignmentType localAssignment;

  assert(mask != NULL);

  readMessage(fd, &localAssignment, sizeof(struct assignmentType));
  *mask = localAssignment.mask;

  return localAssignment.command;
}

static enum workerResponse getAssessment(int fd, uint32_t *mask, double *assessment) {
  struct assessmentType localAssessment;

  assert(mask != NULL);
  assert(assessment != NULL);

  readMessage(fd, &localAssessment, sizeof(struct assessmentType));
  *assessment = localAssessment.assessment;
  *mask = localAssessment.mask;

  return localAssessment.response;
}

static void sendAssignment(int fd, uint32_t mask, enum workerCommand command) {
  struct assignmentType localAssignment;

  localAssignment.mask = mask;
  localAssignment.command = command;

  writeMessage(fd, &localAssignment, sizeof(struct assignmentType));
}

static void sendAssessment(int fd, double assessment, uint32_t mask, enum workerResponse response) {
  struct assessmentType localAssessment;

  localAssessment.assessment = assessment;
  localAssessment.mask = mask;
  localAssessment.response = response;

  writeMessage(fd, &localAssessment, sizeof(struct assessmentType));
}

static double processAndAssess(uint32_t currentMask, const uint32_t *indata, statData_t *rewrittendata, size_t indatalen) {
  size_t k = 1U << ((size_t)__builtin_popcount(currentMask));

  extractbitsArray(indata, rewrittendata, indatalen, currentMask);
  return NSAMarkovEstimate(rewrittendata, indatalen, k, "Literal", configConservative, 0.0);
}

static void *doAssessmentThread(void *opaqueDataIn) {
  struct threadInfoType *threadInfo;
  statData_t *rewrittendata;
  uint32_t currentMask;
  bool working = true;
  enum workerCommand command;
  double assessedEnt;
  int res;

  threadInfo = (struct threadInfoType *)opaqueDataIn;
  if (configVerbose > 1) {
    fprintf(stderr, "Thread %u in INIT State\n", threadInfo->localThreadID);
  }

  if ((rewrittendata = malloc(sizeof(statData_t) * datalen)) == NULL) {
    perror("Memory allocation error in computing thread");
    pthread_exit(NULL);
  }

  while (working) {
    // Request an assignment
    threadInfo->state = SENDSTATE;
    if (configVerbose > 1) {
      fprintf(stderr, "Thread %u in SEND State\n", threadInfo->localThreadID);
    }
    sendAssessment(threadInfo->resultsWriteFD, -DBL_INFINITY, 0, READYRSP);

    // Receive a command
    threadInfo->state = LISTENSTATE;
    if (configVerbose > 1) {
      fprintf(stderr, "Thread %u in LISTEN State\n", threadInfo->localThreadID);
    }
    command = getAssignment(threadInfo->assignmentReadFD, &currentMask);

    if (command == ASSIGNMENTCMD) {
      // Assignment was received
      // perform the assessment
      threadInfo->state = WORKSTATE;
      if (configVerbose > 1) {
        fprintf(stderr, "Thread %u in WORK State; assignment is 0x%08x\n", threadInfo->localThreadID, currentMask);
      }
      assessedEnt = processAndAssess(currentMask, data, rewrittendata, datalen);

      // send the results
      threadInfo->state = RESULTSTATE;
      if (configVerbose > 1) {
        fprintf(stderr, "Thread %u in RESULT State\n", threadInfo->localThreadID);
      }
      sendAssessment(threadInfo->resultsWriteFD, assessedEnt, currentMask, ASSESSMENTRSP);
    } else if (command == JOINCMD) {
      threadInfo->state = WAITSTATE;
      if (configVerbose > 1) {
        fprintf(stderr, "Thread %u in WAIT State\n", threadInfo->localThreadID);
      }
      incThreadsWaiting();

      // Join the barrier
      res = pthread_barrier_wait(&joinBarrier);
      if ((res != PTHREAD_BARRIER_SERIAL_THREAD) && (res != 0)) {
        perror("Can't synchronize threads");
        pthread_exit(NULL);
      }

      // All done; everyone arrived
      decThreadsWaiting();
    } else if (command == EXITCMD) {
      threadInfo->state = DONESTATE;
      if (configVerbose > 1) {
        fprintf(stderr, "Thread %u in DONE State\n", threadInfo->localThreadID);
      }
      working = false;
    }
  }

  if (configVerbose > 1) {
    fprintf(stderr, "Thread %u exiting.\n", threadInfo->localThreadID);
  }

  // We have been told that no more assignments are available
  assert(rewrittendata != NULL);
  free(rewrittendata);
  rewrittendata = NULL;
  pthread_exit(NULL);
}

static uint32_t incToHammingWeight(uint32_t in, unsigned weight) {
  assert(weight <= 32);

  while (((uint32_t)__builtin_popcount(in)) < weight) {
    in |= (in + 1);  // Set the lowest unset bit
  }

  return (in);
}

static uint32_t nextFixedHammingWeight(unsigned in) {
  uint32_t out = in;
  out += (out & -out);  // Add in the lowest set bit
  out = incToHammingWeight(out, (uint32_t)__builtin_popcount(in));

  if (out < in) {
    return 0;
  } else {
    return (out);
  }
}

static void setupThreads(uint32_t threadCount, struct threadInfoType *threadInfo, struct pollfd *pfds) {
  uint32_t curThread;
  int localFDs[2];

  // first, the barrier for join steps
  if (pthread_barrier_init(&joinBarrier, NULL, threadCount + 1) < 0) {
    perror("Can't init barrier");
    exit(EX_OSERR);
  }

  // Now setup pipe pairs to communicate with all the threads
  for (curThread = 0; curThread < threadCount; curThread++) {
    // Make the command and results pipe pairs
    if (pipe(localFDs) != 0) {
      perror("Can't make pipes");
      exit(EX_OSERR);
    }
    threadInfo[curThread].assignmentReadFD = localFDs[0];
    threadInfo[curThread].assignmentWriteFD = localFDs[1];

    if (pipe(localFDs) != 0) {
      perror("Can't make pipes");
      exit(EX_OSERR);
    }
    threadInfo[curThread].resultsReadFD = localFDs[0];
    pfds[curThread].fd = localFDs[0];
    pfds[curThread].events = POLLIN;
    pfds[curThread].revents = 0;
    threadInfo[curThread].resultsWriteFD = localFDs[1];
    threadInfo[curThread].state = INITSTATE;
    threadInfo[curThread].jobsAssigned = 0;

    threadInfo[curThread].localThreadID = (uint32_t)curThread;
    // Start up threads here
    if (pthread_create(&(threadInfo[curThread].threadID), NULL, doAssessmentThread, (void *)&(threadInfo[curThread])) != 0) {
      perror("Can't create a thread");
      exit(EX_OSERR);
    }
  }
}

static uint32_t processorCount(void) {
  long int numOfProc;
  // Make a guess on the optimal number of threads based on active processors
  if ((numOfProc = sysconf(_SC_NPROCESSORS_ONLN)) <= 0) {
    perror("Can't get processor count");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Processor Count: %ld\n", numOfProc);

  return (uint32_t)numOfProc;
}

// Ask all the threads to exit after calculation results are all collected
static void killThreads(uint32_t threadCount, struct threadInfoType *threadInfo) {
  for (uint32_t curThread = 0; curThread < threadCount; curThread++) {
    uint32_t inMask;
    double assessedEnt;
    enum workerResponse response;

    if (configVerbose > 1) {
      fprintf(stderr, "Requesting that thread %u exits.\n", (uint32_t)curThread);
    }
    response = getAssessment(threadInfo[curThread].resultsReadFD, &inMask, &assessedEnt);
    assert(response == READYRSP);

    sendAssignment(threadInfo[curThread].assignmentWriteFD, 0, EXITCMD);
    if (pthread_join(threadInfo[curThread].threadID, NULL) < 0) {
      perror("Can't wait for thread to end.");
      exit(EX_OSERR);
    }
  }
}

// Computation pattern:
//    Send all the assignments
//    Wait for the last few to finish; ask all finished compute threads to join the barrier
//    Once all the threads have joined the barrier, have the base thread (here) join the barrier (releasing all the compute threads for the next computation)
static bool findBestSymbol(FILE *statfile, uint32_t curHammingWeight, uint32_t activeBits, size_t *usedBits, struct bestMaskData *bestMasks, double bitAssessments[8][16], uint32_t threadCount, struct threadInfoType *threadInfo, struct pollfd *pfds) {
  double assessedEnt, bestHammingEnt;
  uint32_t curMask;
  uint32_t expandedCurrentMask;
  uint32_t localNominalBits;
  int res;
  double bestEnt;
  uint32_t bestEntMask;

  bestEnt = -1.0;
  bestEntMask = 0;

  if (curHammingWeight != 1) {
    for (uint32_t j = 0; j < curHammingWeight - 1; j++) {
      if ((bestMasks[j].mask != 0) && (bestMasks[j].entropy > bestEnt)) {
        bestEnt = bestMasks[j].entropy;
        bestEntMask = bestMasks[j].mask;
      }
    }
  }

  // The numerically largest symbol of this hamming weight
  localNominalBits = ((1U << curHammingWeight) - 1U) << ((uint32_t)__builtin_popcount(activeBits) - curHammingWeight);

  // Get the first curMask for this hamming weight
  bestHammingEnt = 0.0;
  curMask = incToHammingWeight(0, curHammingWeight);
  expandedCurrentMask = expandBits(curMask, activeBits);
  assert(__builtin_popcount(curMask) == __builtin_popcount(expandedCurrentMask));

  fprintf(stderr, "Doing Waiting for message from thread.\n");

  // send all the assignments
  while ((curMask > 0) && (curMask <= localNominalBits)) {
    if (configVerbose > 1) {
      fprintf(stderr, "Waiting for message from thread.\n");
    }
    if ((res = poll(pfds, threadCount, -1)) < 0) {
      perror("Can't poll computing threads");
      exit(EX_OSERR);
    }
    if (configVerbose > 1) {
      fprintf(stderr, "Got responses from %d thread.\n", res);
    }

    // Find the first thread waiting for work
    for (uint32_t curThread = 0; curThread < threadCount; curThread++) {
      if ((pfds[curThread].revents & POLLIN) != 0) {
        enum workerResponse response;
        uint32_t inMask;
        response = getAssessment(threadInfo[curThread].resultsReadFD, &inMask, &assessedEnt);

        if ((response == READYRSP) && (curMask <= localNominalBits) && (curMask > 0)) {
          // This thread wants an assignment. Provide it with one.
          bool lookForMask = true;
          assert((uint32_t)__builtin_popcount(expandedCurrentMask) == curHammingWeight);

          if (configVerbose > 1) {
            fprintf(stderr, "Sending current bitmask: 0x%08X (weight: %u)\n", expandedCurrentMask, curHammingWeight);
          }
          threadInfo[curThread].jobsAssigned++;
          sendAssignment(threadInfo[curThread].assignmentWriteFD, expandedCurrentMask, ASSIGNMENTCMD);

          // Now look for the next assignment
          while (lookForMask && (curMask <= localNominalBits) && (curMask > 0)) {
            double curEntBound = 0.0;

            curMask = nextFixedHammingWeight(curMask);

            if ((curMask > localNominalBits) || (curMask == 0)) {
              break;
            }

            expandedCurrentMask = expandBits(curMask, activeBits);
            assert(__builtin_popcount(curMask) == __builtin_popcount(expandedCurrentMask));
            lookForMask = false;

            // Could this symbol possibly be better than the current best symbol?
            // Use a per-nibble assessment to (over-)estimate the possible entropy.
            // If this over-estimate is too small, we can just skip this symbol.
            // Note, this is in packed form!
            for (uint32_t i = 0; i < 8; i++) {
              curEntBound += bitAssessments[i][(curMask >> (i << 2)) & 0xF];
            }

            if ((curEntBound < bestEnt) && (curMask <= localNominalBits) && (curMask > 0)) {
              lookForMask = true;
              fprintf(stderr, "Upper entropy bound (%.17g) less than current best entropy (%.17g). Skipping mask 0x%08X (weight: %u).\n", curEntBound, bestEnt, expandedCurrentMask, curHammingWeight);
            }
          }
        } else if ((response == READYRSP) && ((curMask > localNominalBits) || (curMask == 0))) {
          // This thread wants an assignment, but we don't have any more. Ask the thread to wait.
          if (configVerbose > 1) {
            fprintf(stderr, "Sending JOIN command.\n");
          }
          sendAssignment(threadInfo[curThread].assignmentWriteFD, 0, JOINCMD);
        } else if (response == ASSESSMENTRSP) {
          uint32_t responseHammingWeight;
          // This thread is reporting in with an assessment (presumably for the current hamming weight).
          if (configVerbose > 1) {
            fprintf(stderr, "Received assessment for bitmask: 0x%08X (%.17g)\n", inMask, assessedEnt);
          }

          assert(threadInfo[curThread].jobsAssigned > 0);

          responseHammingWeight = (uint32_t)__builtin_popcount(inMask);
          assert(responseHammingWeight == curHammingWeight);

          assert(assessedEnt <= (double)curHammingWeight);
          assert(assessedEnt >= 0.0);

          // Is this the best we've seen for this hamming weight?
          if (assessedEnt > bestHammingEnt) {
            bestHammingEnt = assessedEnt;
          }

          // Is this the best we've seen over all?
          if (assessedEnt > bestEnt) {
            bestEnt = assessedEnt;
            bestEntMask = inMask;
            bestMasks[curHammingWeight - 1].mask = inMask;
            bestMasks[curHammingWeight - 1].entropy = assessedEnt;
            fprintf(stderr, "New best entropy: %.17g (mask: 0x%08X, weight: %u)\n", bestEnt, bestEntMask, curHammingWeight);

            // Note the bits present in the current best symbol.
            for (uint32_t i = 0; i < 32; i++) {
              if (bestEntMask & (1U << (uint32_t)i)) {
                usedBits[i]++;
              }
            }
          } else {
            // This is not better than the best one we've seen thus far
            fprintf(stderr, "Encountered sub-optimal entropy: %.17g (mask 0x%08X, weight: %u). Best entropy is still %.17g (mask: 0x%08X, weight: %u)\n", assessedEnt, inMask, curHammingWeight, bestEnt, bestEntMask,
                    (uint32_t)__builtin_popcount((uint32_t)bestEntMask));
          }  // end if assessedEnt > bestEnt
        }  // end if workerResponse == ASSESSMENTRSP
      }  // endif POLLIN flag is set
    }  // For loop, iterating through the threads waiting for a response.
  }  // while we haven't reached the maximum for this hamming weight.

  // Wait for the last few to finish; ask all finished compute threads to join the barrier
  // Now, we've run out of symbols to assign at this hamming weight, but need to wait for any stragglers
  // Do the join.
  while (getThreadsWaiting() < threadCount) {
    if (poll(pfds, threadCount, 1) < 0) {
      perror("Can't poll computing threads");
      exit(EX_OSERR);
    }

    for (uint32_t curThread = 0; curThread < threadCount; curThread++) {
      if ((pfds[curThread].revents & POLLIN) != 0) {
        enum workerResponse response;
        uint32_t inMask;
        response = getAssessment(threadInfo[curThread].resultsReadFD, &inMask, &assessedEnt);

        if (response == READYRSP) {
          // This thread wants an assignment, but we don't have any more. Ask the thread to wait.
          sendAssignment(threadInfo[curThread].assignmentWriteFD, 0, JOINCMD);
        } else if (response == ASSESSMENTRSP) {
          uint32_t responseHammingWeight;
          // This thread is reporting in with an assessment for the current hamming weight.
          if (configVerbose > 1) {
            fprintf(stderr, "Received assessment for bitmask: 0x%08X (%.17g)\n", inMask, assessedEnt);
          }

          assert(threadInfo[curThread].jobsAssigned > 0);

          responseHammingWeight = (uint32_t)__builtin_popcount(inMask);
          assert(responseHammingWeight == curHammingWeight);

          assert(assessedEnt <= (double)curHammingWeight);
          assert(assessedEnt >= 0.0);

          // Is this the best we've seen for this hamming weight?
          if (assessedEnt > bestHammingEnt) {
            bestHammingEnt = assessedEnt;
          }

          // Is this the best we've seen over all?
          if (assessedEnt > bestEnt) {
            bestEnt = assessedEnt;
            bestEntMask = inMask;
            bestMasks[curHammingWeight - 1].mask = inMask;
            bestMasks[curHammingWeight - 1].entropy = assessedEnt;
            fprintf(stderr, "New best entropy: %.17g (mask: 0x%08X, weight: %u)\n", bestEnt, bestEntMask, curHammingWeight);

            // Note the bits present in the current best symbol.
            for (uint32_t i = 0; i < 32; i++) {
              if (bestEntMask & (1U << (uint32_t)i)) {
                usedBits[i]++;
              }
            }
          } else {
            // This is not better than the best one we've seen thus far
            fprintf(stderr, "Encountered sub-optimal entropy: %.17g (mask 0x%08X, weight: %u). Best entropy is still %.17g (mask: 0x%08X, weight: %u).\n", assessedEnt, inMask, curHammingWeight, bestEnt, bestEntMask,
                    (uint32_t)__builtin_popcount((uint32_t)bestEntMask));
          }  // end if assessedEnt > bestEnt
        }  // end if workerResponse == ASSESSMENTRSP
      }  // endif POLLIN flag is set
    }  // for loop iterating through the threads
  }  // end while loop getThreadsWaiting() < threadCount

  // We've now waited for all the stragglers
  assert(getThreadsWaiting() == threadCount);

  // Report out on the progress thus far.
  fprintf(stderr, "Best entropy up to weight %u: %.17g (mask: 0x%08X). Best this weight: %.17g\n", curHammingWeight, bestEnt, bestEntMask, bestHammingEnt);
  fprintf(statfile, "Best entropy up to weight %u: %.17g (mask: 0x%08X)\n", curHammingWeight, bestEnt, bestEntMask);
  fflush(statfile);
  fprintf(stderr, "Current Best Counts:\n");
  for (uint32_t i = 0; i < 32; i++) {
    if (usedBits[i] != 0) {
      fprintf(stderr, "usedBits[%u]=%zu\n", i, usedBits[i]);
    }
  }

  // Once all the threads have joined the barrier, have the base thread (here) join the barrier (releasing all the compute threads for the next computation)
  // Finish the barrier
  // This should release all the processing threads
  res = pthread_barrier_wait(&joinBarrier);
  if ((res != PTHREAD_BARRIER_SERIAL_THREAD) && (res != 0)) {
    perror("Can't synchronize threads");
    exit(EX_OSERR);
  }

  // Did the current hamming weight's highest-entropy symbol entropy decrease substantially?
  if (bestHammingEnt < bestEnt * .99) {
    fprintf(stderr, "Last round's hamming entropy decreased. Stopping.\n");
    return false;
  } else {
    return true;
  }
}

static void doNibbleAssessments(double bitAssessments[8][16], uint32_t activeBits, uint32_t threadCount, struct threadInfoType *threadInfo, struct pollfd *pfds) {
  uint32_t curMask;
  int res;
  uint32_t nominalBits;
  uint32_t expandedCurrentMask;
  uint32_t nibbleNum = 0;
  uint32_t nibbleVal = 1;

  nominalBits = extractbits(activeBits, activeBits);
  assert(__builtin_popcount(nominalBits) > 0);

  // send all the assignments
  bitAssessments[0][0] = 0;
  curMask = 1;
  expandedCurrentMask = expandBits(curMask, activeBits);

  while ((curMask > 0)) {
    if (configVerbose > 1) {
      fprintf(stderr, "Waiting for message from thread.\n");
    }
    if ((res = poll(pfds, threadCount, -1)) < 0) {
      perror("Can't poll computing threads");
      exit(EX_OSERR);
    }
    if (configVerbose > 1) {
      fprintf(stderr, "Got responses from %d thread.\n", res);
    }

    // Find the first thread waiting for work
    for (uint32_t curThread = 0; curThread < threadCount; curThread++) {
      if ((pfds[curThread].revents & POLLIN) != 0) {
        enum workerResponse response;
        uint32_t inMask;
        double assessedEnt;

        response = getAssessment(threadInfo[curThread].resultsReadFD, &inMask, &assessedEnt);

        if ((response == READYRSP) && (curMask > 0)) {
          // This thread wants an assignment. Provide it with one.
          if (configVerbose > 1) {
            fprintf(stderr, "Sending current bitmask: 0x%08X\n", expandedCurrentMask);
          }
          threadInfo[curThread].jobsAssigned++;
          sendAssignment(threadInfo[curThread].assignmentWriteFD, expandedCurrentMask, ASSIGNMENTCMD);

          // Now look for the next assignment
          while (curMask > 0) {
            nibbleVal++;

            if (nibbleVal >= 16) {
              nibbleNum++;
              if (nibbleNum >= 8) {
                curMask = 0;
                expandedCurrentMask = 0;
                break;
              } else {
                bitAssessments[nibbleNum][0] = 0.0;
                nibbleVal = 1;
              }
            }

            curMask = nibbleVal << (nibbleNum << 2);
            if ((curMask & nominalBits) == curMask) {
              // This mask will work. Populate the expanded mask
              expandedCurrentMask = expandBits(curMask, activeBits);
              assert(__builtin_popcount(curMask) == __builtin_popcount(expandedCurrentMask));
              break;
            } else {
              bitAssessments[nibbleNum][nibbleVal] = 0.0;
            }
          }
        } else if ((response == READYRSP) && (curMask == 0)) {
          // This thread wants an assignment, but we don't have any more. Ask the thread to wait.
          if (configVerbose > 1) {
            fprintf(stderr, "Sending JOIN command.\n");
          }
          sendAssignment(threadInfo[curThread].assignmentWriteFD, 0, JOINCMD);
        } else if (response == ASSESSMENTRSP) {
          uint32_t nibblePos;
          uint32_t inMaskWt;
          uint32_t compressedInMask;
          // This thread is reporting in with an assessment (presumably for some nibble).
          fprintf(stderr, "Received assessment for bitmask: 0x%08X (%.17g)\n", inMask, assessedEnt);

          inMaskWt = (uint32_t)__builtin_popcount(inMask);
          assert((inMaskWt <= 4) && (inMaskWt > 0));
          assert(threadInfo[curThread].jobsAssigned > 0);
          compressedInMask = extractbits(inMask, activeBits);

          for (nibblePos = 0; nibblePos < 8; nibblePos++) {
            uint32_t curNibblePattern = 0xFU << (nibblePos << 2U);
            if ((curNibblePattern & compressedInMask) != 0) {
              assert((compressedInMask & (~curNibblePattern)) == 0);
              bitAssessments[nibblePos][compressedInMask >> (nibblePos << 2U)] = assessedEnt;
              break;
            }
          }
        }  // end if workerResponse == ASSESSMENTRSP
      }  // endif POLLIN flag is set
    }  // For loop, iterating through the threads waiting for a response.
  }  // while we haven't gone through all the nibbles

  // Wait for the last few to finish; ask all finished compute threads to join the barrier
  // Now, we've run out of symbols to assign at this hamming weight, but need to wait for any stragglers
  // Do the join.
  while (getThreadsWaiting() < threadCount) {
    if (poll(pfds, threadCount, 1) < 0) {
      perror("Can't poll computing threads");
      exit(EX_OSERR);
    }

    for (uint32_t curThread = 0; curThread < threadCount; curThread++) {
      if ((pfds[curThread].revents & POLLIN) != 0) {
        enum workerResponse response;
        uint32_t inMask;
        double assessedEnt;
        response = getAssessment(threadInfo[curThread].resultsReadFD, &inMask, &assessedEnt);

        if (response == READYRSP) {
          // This thread wants an assignment, but we don't have any more. Ask the thread to wait.
          sendAssignment(threadInfo[curThread].assignmentWriteFD, 0, JOINCMD);
        } else if (response == ASSESSMENTRSP) {
          uint32_t nibblePos;
          uint32_t inMaskWt;
          uint32_t compressedInMask;
          // This thread is reporting in with an assessment (presumably for some nibble).
          fprintf(stderr, "Received assessment for bitmask: 0x%08X (%.17g)\n", inMask, assessedEnt);

          inMaskWt = (uint32_t)__builtin_popcount(inMask);
          assert((inMaskWt <= 4) && (inMaskWt > 0));
          assert(threadInfo[curThread].jobsAssigned > 0);
          compressedInMask = extractbits(inMask, activeBits);

          for (nibblePos = 0; nibblePos < 8; nibblePos++) {
            uint32_t curNibblePattern = 0xFU << (nibblePos << 2U);
            if ((curNibblePattern & compressedInMask) != 0) {
              assert((compressedInMask & (~curNibblePattern)) == 0);
              bitAssessments[nibblePos][compressedInMask >> (nibblePos << 2U)] = assessedEnt;
              break;
            }
          }
        }  // end if workerResponse == ASSESSMENTRSP
      }  // endif POLLIN flag is set
    }  // for loop iterating through the threads
  }  // end while loop getThreadsWaiting() < threadCount

  // We've now waited for all the stragglers
  assert(getThreadsWaiting() == threadCount);

  // Once all the threads have joined the barrier, have the base thread (here) join the barrier (releasing all the compute threads for the next computation)
  // Finish the barrier
  // This should release all the processing threads
  res = pthread_barrier_wait(&joinBarrier);
  if ((res != PTHREAD_BARRIER_SERIAL_THREAD) && (res != 0)) {
    perror("Can't synchronize threads");
    exit(EX_OSERR);
  }
}

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "selectbits [-l logging dir] [-v] [-t <n>] [-c] inputfile outputBits\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  fprintf(stderr, "-t <n> \t uses <n> computing threads. (default: ceiling(number of cores * 1.3))\n");
  fprintf(stderr, "-l <dir> \t uses <dir> to contain the log (default: current working directory)\n");
  fprintf(stderr, "-v \t verbose mode.\n");
  fprintf(stderr, "-c \t Conservative mode (use confidence intervals with the Markov estimation).\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  uint32_t activeBits;
  uint32_t outputBits;
  uint32_t nominalBits;
  long int inint;
  uint32_t curHammingWeight;
  size_t usedBits[32] = {0};
  struct bestMaskData bestMasks[32];
  char statusfilename[8192];
  FILE *statfile;
  struct pollfd *pfds;
  int opt;
  long inparam;
  uint32_t threadCount;
  struct threadInfoType *threadInfo;
  uint32_t activeBitsHammingWeight;
  char logdir[4096];
  bool notDone;
  double bitAssessments[8][16];  // nibble index (least to most significant nibbles of curMask; LSN is index 0, MSN is nibble 7) followed by the nibble value

  // Initialize
  /*Nothing has happened yet!*/
  if (errno != 0) {
    perror("Mysterious startup error");
    errno = 0;
    fprintf(stderr, "Error cleared...\n");
  }

  for (int j = 0; j < 32; j++) {
    bestMasks[j].entropy = -1.0;
    bestMasks[j].mask = 0;
  }

  threadCount = 0;
  datalen = 0;
  configVerbose = 0;
  strncpy(logdir, ".", sizeof(logdir));

  assert(PRECISION(UINT_MAX) >= 32);

  // Process the command line
  while ((opt = getopt(argc, argv, "l:vt:c")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'c':
        configConservative = true;
        break;
      case 't':
        inparam = strtol(optarg, NULL, 10);
        if ((inparam <= 0) || (inparam > 10000)) {
          useageExit();
        }
        threadCount = (uint32_t)inparam;
        break;
      case 'l':
        strncpy(logdir, optarg, sizeof(logdir));
        logdir[sizeof(logdir) - 1] = 0;
        break;
      default: /* '?' */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[0], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  snprintf(statusfilename, sizeof(statusfilename), "%s/%s.select", logdir, basename(argv[0]));
  statusfilename[sizeof(statusfilename) - 1] = '\0';
  fprintf(stderr, "Logging to %s\n", statusfilename);

  if ((statfile = fopen(statusfilename, "wb")) == NULL) {
    perror("Can't open status file for write");
    exit(EX_NOINPUT);
  }

  inint = atoi(argv[1]);
  if ((inint < 0) || inint > STATDATA_BITS) {
    useageExit();
  }

  outputBits = (uint32_t)inint;

  if ((outputBits == 0) || (outputBits >= 32)) {
    useageExit();
  }

  // Read in the data
  datalen = readuint32file(infp, &data);
  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  assert(data != NULL);
  assert(datalen > 0);

  // Determine which bits we will be selecting from (the selection mask) and the hamming weight of the selection mask
  activeBits = getActiveBits(data, datalen);
  activeBitsHammingWeight = (uint32_t)__builtin_popcount(activeBits);

  fprintf(stderr, "Active bit mask: 0x%08X\n", activeBits);

  // If the number of output bits is higher than the actual number of observed active bits, adjust it down.
  outputBits = (outputBits > activeBitsHammingWeight) ? activeBitsHammingWeight : outputBits;

  // nominalBits is the maximum "nominal" (that is, packed) bitmask
  nominalBits = (uint32_t)(((uintmax_t)1 << activeBitsHammingWeight) - 1U);
  assert(nominalBits == extractbits(activeBits, activeBits));

  fprintf(stderr, "Shifted mask: 0x%08X\n", nominalBits);

  // Try to figure out how many threads to use
  if (threadCount == 0) {
    threadCount = (uint32_t)floor(1.3 * (double)processorCount());
  }

  assert(threadCount >= 1);

  fprintf(stderr, "Using %u threads\n", threadCount);

  // Setup the threads
  if ((threadInfo = malloc(sizeof(struct threadInfoType) * threadCount)) == NULL) {
    perror("Can't get memory for thread structures (1)");
    exit(EX_OSERR);
  }

  if ((pfds = malloc(sizeof(struct pollfd) * threadCount)) == NULL) {
    perror("Can't get memory for thread structures (2)");
    exit(EX_OSERR);
  }

  setupThreads(threadCount, threadInfo, pfds);

  // Populate the per-nibble patterns used for bounding the min entropy.
  fprintf(stderr, "Pre-calculating nibble entropy for estimation\n");

  // Calculate our guess for the entropy associated with each nibble
  doNibbleAssessments(bitAssessments, activeBits, threadCount, threadInfo, pfds);

  // Now process the various bitmasks, explored by hamming weight
  fprintf(stderr, "Starting main assessments.\n");

  notDone = true;

  for (curHammingWeight = 1; notDone && (curHammingWeight <= outputBits); curHammingWeight++) {
    notDone = findBestSymbol(statfile, curHammingWeight, activeBits, usedBits, bestMasks, bitAssessments, threadCount, threadInfo, pfds);
  }

  // Kill the threads, and don't move on until they are all gone.
  killThreads(threadCount, threadInfo);

  // All the worker threads are done, the poor dears.
  fprintf(statfile, "Final Best Masks: \n");
  for (uint32_t i = 0; i < outputBits; i++) {
    if (bestMasks[i].mask != 0) {
      fprintf(statfile, "0x%08X (weight %u): %.17g\n", bestMasks[i].mask, i + 1, bestMasks[i].entropy);
    }
  }

  fprintf(statfile, "test: ");
  for (uint32_t i = 0; i < outputBits; i++) {
    if (bestMasks[i].mask != 0) {
      fprintf(statfile, "0x%08X ", bestMasks[i].mask);
    }
  }
  fprintf(statfile, "\n");
  fflush(statfile);

  fclose(statfile);
  free(data);
  free(threadInfo);
  free(pfds);

  return EX_OK;
}
