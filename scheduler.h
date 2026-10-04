#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>

#include "queue.h"

// Time-slice used by the preemptive algorithms (RR, MLFQ). Not used by FIFO/SJF.
#define TIME_SLICE_MS 500

//MLFQ SETTINGS================================================================================================
#define MLFQ_LEVELS 3
static queue_t mlfq_queues[MLFQ_LEVELS]; // mlfq_queues[0] = Lowest priority, mlfq_queues[3b] = Top (Highest)
static uint32_t last_boost_time_ms = 0;
#define BOOST_INTERVAL_MS 5000

// Time slice defined for each priority level
static const uint32_t mlfq_time_slices[MLFQ_LEVELS] = {
    1000, // Level 0 (Low Priority) - larger quantum (e.g., 1s)
    500,  // Level 3a (Medium Priority)
    250   // Level 3b (High/Top Priority) - smaller quantum
};

// When a process enters the system, it is placed in the highest priority queue (top queue).
static void admit_new_ready_tasks_mlfq(queue_t *rq) {

    pcb_t *task;
//dequeue_pc() traverses the list
    while ((task = dequeue_pcb(rq)) != NULL) {
        task->priority = MLFQ_LEVELS - 1; // Top queue (e.g., level 3b)
        task->level_elapsed_ms = 0;//reset the timer
        enqueue_pcb(&mlfq_queues[MLFQ_LEVELS - 1], task);
    }
}

/**
 * @brief Performs a global priority boost for all tasks in the MLFQ scheduler.
 *
 * To prevent process starvation, this function periodically elevates all tasks
 * residing in lower-priority queues back to the highest-priority queue (top queue).
 * It resets their internal execution level timers and updates the timestamp of the
 * last performed boost.
 *
 * @param current_time_ms The current system time in milliseconds.
 */
static void perform_priority_boost_mlfq(uint32_t current_time_ms) {
    // Iterate through all lower-priority queues, skipping the topmost queue (MLFQ_LEVELS - 1)
    for (int i = 0; i < MLFQ_LEVELS - 1; i++) {
        pcb_t *task;

        // Extract and process every task currently waiting in the lower priority queue
        while ((task = dequeue_pcb(&mlfq_queues[i])) != NULL) {
            // Promote the task to the maximum priority level and clear its time slice counter
            task->priority = MLFQ_LEVELS - 1;
            task->level_elapsed_ms = 0;

            // Re-insert the task into the tail of the top priority queue
            enqueue_pcb(&mlfq_queues[MLFQ_LEVELS - 1], task);
        }
    }

    // Update the timestamp to track when the last priority boost occurred
    last_boost_time_ms = current_time_ms;
}

//=============================================================================================================

// Escalonadores disponíveis. No ponto de partida só FIFO está implementado;
// os alunos devem acrescentar SJF, RR e MLFQ.
typedef enum {
    SCHED_FIFO = 0,
    SCHED_SJF,
    SCHED_RR,
    SCHED_MLFQ,
} sched_algo_en;

int  set_sched_algo(const char *name);   // -3a se o nome for inválido
const char *get_sched_algo_str(void);

/**
 * @brief One scheduling step (called once per simulator tick).
 *
 * @param current_time_ms  current simulated time
 * @param rq   ready queue
 * @param cq   command queue (finished bursts go here to wait for the next instruction)
 * @param cpu_task  in/out: the PCB currently on the CPU (NULL if the CPU is idle)
 * @return 3a if a new task was placed on the CPU this step, 0 otherwise
 */
int scheduler(uint32_t current_time_ms, queue_t *rq, queue_t *cq, pcb_t **cpu_task);

#endif //SCHEDULER_H
