#include "scheduler.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "msg.h"

static const char *SCHED_NAMES[] = {"FIFO", "SJF", "RR", "MLFQ", NULL};

//ESTA VARIÁVEL RECEBE O ARGUMENTO PASSADO NO OSSIM, NAO PRECISO ANDAR A MUDÁ-LA
static sched_algo_en sched_algo = SCHED_FIFO;

int set_sched_algo(const char *name) {
    for (int i = 0; SCHED_NAMES[i] != NULL; i++) {
        if (strcasecmp(SCHED_NAMES[i], name) == 0) {
            sched_algo = (sched_algo_en) i;
            return sched_algo;
        }
    }
    return -1;
}

const char *get_sched_algo_str(void) {
    return SCHED_NAMES[sched_algo];
}

/**
 * Send DONE to the application and move the finished burst to the command queue.
 */
static void finish_burst(uint32_t current_time_ms, queue_t *cq, pcb_t *task) {
    msg_t msg = {
        .pid = task->pid,
        .request = PROCESS_REQUEST_DONE,
        .time_ms = current_time_ms
    };
    if (write(task->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
        perror("write");
    }
    enqueue_pcb(cq, task);
}

/**
 * FIFO (First-In, First-Out) — não preemptivo.
 *
 * O primeiro processo a chegar à ready queue corre até o seu burst de CPU
 * terminar; só então o CPU fica livre para o processo seguinte.
 *
 * TODO (alunos): acrescentar os algoritmos SJF, RR (time-slice = TIME_SLICE_MS)
 * e MLFQ, selecionados por `sched_algo` / `--sched`.
 */
int scheduler(uint32_t current_time_ms, queue_t *rq, queue_t *cq, pcb_t **cpu_task) {

    // Only run MLFQ-specific maintenance if MLFQ is the active scheduling algorithm
    if (sched_algo == SCHED_MLFQ) {
        // MLFQ Admit new tasks arriving from the simulator into the top queue
        admit_new_ready_tasks_mlfq(rq);

        // MLFQ Check if period S has passed to perform a priority boost
        if (last_boost_time_ms == 0) {
            last_boost_time_ms = current_time_ms;
        }
        if ((current_time_ms - last_boost_time_ms) >= BOOST_INTERVAL_MS) {
            perform_priority_boost_mlfq(current_time_ms);
            // Added print for priority boost demonstration
            printf("[MLFQ] Priority boost performed for all tasks at time %u ms\n", current_time_ms);
        }
    }

    // 3a. ACTIVE PROCESS MANAGEMENT ON THE CPU
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;
        (*cpu_task)->level_elapsed_ms += TICKS_MS;

        // If the process has completed its total burst time
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            // print for burst completion demonstration
            printf("[Scheduler] PID %d completed its burst at time %u ms\n", (*cpu_task)->pid, current_time_ms);
            finish_burst(current_time_ms, cq, *cpu_task);
            *cpu_task = NULL;
        }

        //MLFQ If the process exhausts its time allocation at its current level
        else if (sched_algo == SCHED_MLFQ) {
            int current_prio = (*cpu_task)->priority;
            if ((*cpu_task)->level_elapsed_ms >= mlfq_time_slices[current_prio]) {
                // Reduce priority (move down one queue, minimum level is 0)
                if (current_prio > 0) {
                    (*cpu_task)->priority = current_prio - 1;
                }
                (*cpu_task)->level_elapsed_ms = 0;

                // print for MLFQ demotion demonstration
                printf("[MLFQ] PID %d exhausted time slice at level %d, demoted to level %d at time %u ms\n",
                       (*cpu_task)->pid, current_prio, (*cpu_task)->priority, current_time_ms);

                // Enqueue into the tail of the corresponding lower priority queue
                enqueue_pcb(&mlfq_queues[(*cpu_task)->priority], (*cpu_task));
                *cpu_task = NULL; // Preempt the CPU
            }
        }

        // RR preemption
        else if (sched_algo == SCHED_RR) {
            if ((current_time_ms - (*cpu_task)->slice_start_ms) >= TIME_SLICE_MS) {
                // print for RR preemption demonstration
                printf("[RR] PID %d exhausted time slice (%u ms), preempted at time %u ms\n",
                       (*cpu_task)->pid, TIME_SLICE_MS, current_time_ms);

                enqueue_pcb(rq, (*cpu_task));
                *cpu_task = NULL;
            }
        }
    }

    // 3b. DISPATCH LOGIC (If the CPU is idle)
    if (*cpu_task == NULL) {
        if (sched_algo == SCHED_FIFO) {
            *cpu_task = dequeue_pcb(rq);
        }

        if (sched_algo == SCHED_SJF) {
            *cpu_task = dequeue_pcb_sjf(rq);
        }

        if (sched_algo == SCHED_RR) {
            *cpu_task = dequeue_pcb(rq);
        }

        if (sched_algo == SCHED_MLFQ) {
            // Search from the highest priority queue down to the lowest
            for (int i = MLFQ_LEVELS - 1; i >= 0; i--) {
                if (mlfq_queues[i].head != NULL) {
                    // Standard dequeue executes tasks at the same priority level in Round-Robin order
                    *cpu_task = dequeue_pcb(&mlfq_queues[i]);
                    break;
                }
            }
        }

        if (*cpu_task) {
            (*cpu_task)->slice_start_ms = current_time_ms;
            //print for dequeued process
            printf("[Scheduler] [%s] DEQUEUING PID %d (Burst: %u ms, Priority: %d) at time %u ms\n",
                   get_sched_algo_str(), (*cpu_task)->pid, (*cpu_task)->time_ms,
                   ((*cpu_task)->priority), current_time_ms);

            return 1;
        }
    }
    return 0;
}
